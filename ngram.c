/*
 * ngram.c
 * Copyright (C) Jacob Gajek 2010 <jgajek@gmail.com>
 * 
 * Alkindus is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Alkindus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ngram.h"


#define BLOCKSIZE   65536
#define NUMBINS     (sizeof(freqBin)/sizeof(freqBin[0]))

#define MAXNGRAMLEN 8       // N-grams beyond length 8 probably not feasible
                            // due to astronomical memory and training data
                            // requirements

static gboolean ngramExtract  (const char *file);
static gboolean ngramSummary  (void);
static gboolean ngramFreeData (void);

static void ngramFreeNode  (trieNode *node);
static void ngramCountNode (trieNode *node);

static void ngramInsert (trieNode *root, gchar *ngram, glong count);


static gchar *outFile = NULL;
static FILE  *outf;

guint ngramLen = 3;

static gboolean summaryOnly = FALSE;

static guint ngramsUnique = 0;
static gchar *ngramsTop10s[10] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static guint ngramsTop10f[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Frequencies of frequencies */
static guint freqBin[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 50, 100, 500, 1000, 5000, 10000, 50000, 100000
};
static guint freqSum[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Command line summary and options */
static const gchar *cmdSummary =
  "Utility for building character n-gram models from text corpora.";

static const GOptionEntry cmdOption[] = {
	{ "ngram-length", 'n', 0, G_OPTION_ARG_INT, &ngramLen,
		"n-gram length (default=3)" },
  { "output-file", 'o', 0, G_OPTION_ARG_FILENAME, &outFile,
    "Output file (default=stdout)" },
  { "summary-only", 's', 0, G_OPTION_ARG_NONE, &summaryOnly,
    "Print n-gram summary only (default=off)" },
	{ NULL }
};


int
main (int argc, char *argv[])
{
	GOptionContext *optc = g_option_context_new("<text file(s)> ...");
	g_option_context_add_main_entries(optc, cmdOption, NULL);
  g_option_context_set_summary(optc, cmdSummary);

  GError *errp = NULL;

	if (g_option_context_parse(optc, &argc, &argv, &errp) == FALSE) {		
		g_critical("%s\n", errp->message);
    g_clear_error(&errp);
    return 1;
	}

  if (ngramLen < 1 || ngramLen > MAXNGRAMLEN) {
    g_critical("n-gram length parameter out of range\n");
    return 1;
  }

	if (argc < 2) {
		gchar *usage = g_option_context_get_help(optc, TRUE, NULL);
		g_printerr("%s\n", usage);
		g_free(usage);
		return 1;
	}

  trieRoot = trieNodeNew();

  /* Process each input text file in turn */
  for (int i = 1; i < argc; i++) {
    ngramExtract(argv[i]);
  }

  if (summaryOnly == TRUE) {
    ngramSummary();
    ngramFreeData();
    g_option_context_free(optc);
    return 0;
  }

  if (outFile != NULL) {
    if ((outf = fopen(outFile, "w")) == NULL) {
      g_critical("Error opening output file '%s' for writing\n", outFile);
      return 1;
    }
  } else {
    outf = stdout;
  }
  
  probGoodTuring(outf);

  if (outf != stdout) {
    if (fclose(outf) != 0) {
      g_warning("Error closing output file '%s'\n", outFile);
    }
  }

  ngramFreeData();
  g_option_context_free(optc);  
	return 0;	
}


/**
 * ngramExtract: Extract n-grams from corpus text file
 *
 * @file: Path of corpus text file
 *
 * @Returns: FALSE if an error occurred
 **/
static gboolean
ngramExtract (const char *file)
{
	if (tokenInit(file) == FALSE) {
		return FALSE;
	}

  gchar *buf = g_malloc(BLOCKSIZE);
  gint nchars;

  while ((nchars = tokenGetBlock(buf, BLOCKSIZE)) > 0) {
    for (int i = 0; i <= nchars - ngramLen; i++) {
      ngramInsert(trieRoot, &buf[i], 1L);
    }
  }

  g_free(buf);
	tokenEnd();

  return TRUE;
}


/**
 * ngramInsert: Add n-gram to the trie data structure
 *
 * @root: Pointer to the root node of the trie
 * @ngram: Pointer to the n-gram character sequence
 * @count: Frequency count of the n-gram in the corpus
 *
 * @Returns: Nothing
 **/
static void
ngramInsert (trieNode *root,
             gchar    *ngram,
             glong     count)
{
  trieNode *pnode = root;  // Parent node
  
  for (int i = 0; i < ngramLen; i++) {
    trieNode *tnode = trieGetChild(pnode, ngram[i]-'a');

    if (tnode == NULL) {
      tnode = trieNodeInsert(pnode, ngram[i]-'a');

      if (i == ngramLen-1) {
        tnode->ngram = g_malloc(ngramLen);
        memcpy(tnode->ngram, ngram, ngramLen);
      }
    }

    tnode->total += count;
    pnode = tnode;
  }

  root->total += count;
}


/**
 * ngramFreeData: Free all resources allocated to trie data structure
 *
 * @Returns: FALSE if an error occurs
 **/
static gboolean
ngramFreeData (void)
{
  trieTraverseAll(trieRoot, ngramFreeNode);
  
  return TRUE;
}


/**
 * ngramSummary: Collect n-gram statistics summary data
 *
 * @Returns: FALSE if an error occurs
 **/
static gboolean
ngramSummary (void)
{
  guint ngramsTotal = trieRoot->total;
  guint ngramsPossible = NUMSYMBOLS;

  for (int i = 1; i < ngramLen; i++) {
    ngramsPossible *= NUMSYMBOLS;
  }

  trieTraverseLeaf(trieRoot, ngramCountNode);

  printf("\nSummary of %d-gram statistics in corpus:\n"
         "\nTotal n-grams seen:  %d"
         "\nDistinct types seen: %d of %d (%2.2f%%)",
         ngramLen, ngramsTotal, ngramsUnique, ngramsPossible,
         ((double)ngramsUnique / (double)ngramsPossible) * 100);

  printf("\n\nTop 10 types by frequency:\n");

  for (int i = 0; i < 10; i++) {
    printf("\n%s\t%d", ngramsTop10s[i], ngramsTop10f[i]);
    g_free(ngramsTop10s[i]);
  }

  printf("\n\nFrequencies of frequencies:"
         "\n---------------------------\n");

  for (int i = NUMBINS-1; i >= NUMBINS/2; i--) {
    printf("\n%7d:\t%d", freqBin[i], freqSum[i]);
    printf("\t%7d:\t%d", freqBin[i-NUMBINS/2], freqSum[i-NUMBINS/2]);
  }

  puts("\n\n");

  return TRUE;
}


/**
 * ngramCountNode: Collect statistics about n-gram nodes
 *
 * This function is called via trieTraverseLeaf() for each leaf node.
 *
 * @Returns: Nothing
 **/
static void
ngramCountNode (trieNode *node)
{
  ngramsUnique += 1;
  int i;

  for (i = 9; i >= 0; i--) {
    if (node->total <= ngramsTop10f[i]) {
      break;
    }
  }

  i += 1;

  if (i < 10) {
    g_free(ngramsTop10s[9]);
           
    for (int j = 9; j > i; j--) {
      ngramsTop10s[j] = ngramsTop10s[j-1];
      ngramsTop10f[j] = ngramsTop10f[j-1];
    }
    
    ngramsTop10f[i] = node->total;
    ngramsTop10s[i] = g_malloc(ngramLen+1);
    memcpy(ngramsTop10s[i], node->ngram, ngramLen);
    ngramsTop10s[i][ngramLen] = '\0'; 
  }

  for (int i = NUMBINS-1; i >= 0; i--) {
    if (node->total >= freqBin[i]) {
      freqSum[i] += 1;
      break;
    }
  }
}


/**
 * ngramFreeNode: Free all memory associated with a single n-gram node
 *
 * This function is called via trieTraverseAll() for each node.
 *
 * @Returns: Nothing
 **/
static void
ngramFreeNode (trieNode *node)
{
  g_free(node->ngram);   // No need to check for NULL pointer with g_free()
  g_slice_free(trieNode, node);
}
