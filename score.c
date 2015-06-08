/*
 * score.c
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

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "solve.h"

#define CHUNKSIZE   65536


struct s_ngramScore {
  double value;
  struct s_ngramScore *next;
};

typedef struct s_ngramScore ngramScore;


/*
 * Absolute probabilities for (n-1)-grams are stored in hash table scorePrior.
 * Conditional probabilities for n-grams are stored in hash table scoreCond.
 * Probability for unseen n-grams is stored in scoreZero.
 */
static ngramScore   *scoreList;     // Linked list for easy deallocation
static GHashTable   *scorePrior;
static GHashTable   *scoreCond;
static double        scoreZero;
static guint         countZero;

static GStringChunk *ngramChunk;


/**
 * scoreInit: Initialize n-gram score table
 *
 * @Returns: FALSE if an error occurs
 **/
gboolean
scoreInit (const char *file)
{
  GString *scoreFile;
  GString *format;
  FILE *fp;

  scoreList = NULL;

  ngramChunk = g_string_chunk_new(CHUNKSIZE);
  scorePrior = g_hash_table_new(g_str_hash, g_str_equal);
  scoreCond  = g_hash_table_new(g_str_hash, g_str_equal);
  scoreFile  = g_string_sized_new(strlen(file)+4);
  format     = g_string_sized_new(16);

  gchar *ngramBuf = g_malloc0(MAXNGRAMLEN+1);
  scoreZero = 1.0000000000;
  countZero = pow(NUMSYMBOLS, ngramLen);
  
  double value;

  /* Read in (n-1)-gram probabilities */
  g_string_printf(scoreFile, "%s.%d", file, ngramLen-1);
  g_string_printf(format, " %%%d[a-z] %%lf", ngramLen-1);

  if ((fp = fopen(scoreFile->str, "r")) == NULL) {
    g_critical("Error opening file '%s' for reading\n", scoreFile->str);
    return FALSE;
  }

  while (!feof(fp)) {
    if(fscanf(fp, format->str, ngramBuf, &value) != 2) {
      if (feof(fp)) {
        break;
      } else {
        g_critical("Error reading score data in '%s'\n", scoreFile->str);
        return FALSE;
      }
    }

    gchar *ngram = g_string_chunk_insert(ngramChunk, ngramBuf);
    ngramScore *score = g_new(ngramScore, 1);

    score->value = log(value);
    score->next  = scoreList;
    scoreList    = score;

    g_hash_table_insert(scorePrior, ngram, score);
  } 

  fclose(fp);

  /* Read in n-gram probabilities */
  g_string_printf(scoreFile, "%s.%d", file, ngramLen);
  g_string_printf(format, " %%%d[a-z] %%lf", ngramLen);
  memset(ngramBuf, NUL, MAXNGRAMLEN+1);

  if ((fp = fopen(scoreFile->str, "r")) == NULL) {
    g_critical("Error opening file '%s' for reading\n", scoreFile->str);
    return FALSE;
  }

  while (!feof(fp)) {
    if(fscanf(fp, format->str, ngramBuf, &value) != 2) {
      if (feof(fp)) {
        break;
      } else {
        g_critical("Error reading score data in '%s'\n", scoreFile->str);
        return FALSE;
      }
    }

    gchar *ngram = g_string_chunk_insert(ngramChunk, ngramBuf);
    ngramScore *score = g_new(ngramScore, 1);
    ngramBuf[ngramLen-1] = NUL;
    ngramScore *prior = g_hash_table_lookup(scorePrior, ngramBuf);

    g_assert(prior != NULL);

    score->value = log(value) - prior->value;
    score->next  = scoreList;
    scoreList    = score;

    g_hash_table_insert(scoreCond, ngram, score);

    scoreZero -= value;
    countZero -= 1;
  } 

  fclose(fp);

  scoreZero = log(scoreZero / countZero);

  g_free(ngramBuf);
    
  g_string_free(scoreFile, TRUE);
  g_string_free(format, TRUE);
  
  return TRUE;
}


/**
 * scoreDone: Clean up all allocated resources
 *
 * @Returns: FALSE if an error occurs
 **/
gboolean
scoreDone (void)
{
  ngramScore *lp = scoreList;

  while (lp != NULL) {
    ngramScore *np = lp->next;
    g_free(lp);
    lp = np;
  }

  g_hash_table_destroy(scorePrior);
  g_hash_table_destroy(scoreCond);
  g_string_chunk_free(ngramChunk);

  return TRUE;
}


/**
 * scoreEval: Evaluate probability for a text string using n-gram model
 *
 * @str: Text string to evaluate
 * @len: Length of text string
 *
 * @Returns: Probability of text string
 **/
double
scoreEval (char *str,
           int   len)
{
  g_assert(len > ngramLen);

  double score = 0.0000000000;
  char buf[ngramLen+1];
  
  memcpy(buf, str, ngramLen-1);
  buf[ngramLen-1] = NUL;

  ngramScore *pn = g_hash_table_lookup(scorePrior, buf);

  if (pn != NULL) {
    score += pn->value;
  }

  for (int i = ngramLen-1; i < len; i++) {
    memcpy(buf, &str[i], ngramLen);
    buf[ngramLen] = NUL;
    pn = g_hash_table_lookup(scoreCond, buf);

    if (pn != NULL) {
      score += pn->value;
    } else {
      score += scoreZero;
    }
  }

  return score;
}
