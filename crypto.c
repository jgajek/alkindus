/*
 * crypto.c
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
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "solve.h"


#define MAXCIPHERLEN  512


int freq[NUMSYMBOLS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static FILE *fp;

char *encText;      // Ciphertext
char *decText;      // Plaintext
char *solText;      // Text of correct solution (if given)
int   textLen;      // Length of ciphertext/plaintext

int   numLeft;      // Number of trials left to perform


/**
 * cryptoLoad: Load cryptogram file
 *
 * @file: Path to cryptogram file
 * @solution:  Path to solution file
 *
 * @Return: FALSE if an error occurs
 **/
gboolean
cryptoLoad (const char *file, const char *solution)
{
  if ((fp = fopen(file, "r")) == NULL) {
    g_critical("Error opening file '%s' for reading\n", file);
    return FALSE;
  }

	gsize bufsize = MAXCIPHERLEN;
  textLen = 0;

	encText = g_malloc(bufsize);
	decText = g_malloc(bufsize);

	/* Read in ciphertext from file */
	while (!feof(fp)) {
		int c = getc(fp);

		if (textLen > bufsize) {
			bufsize += MAXCIPHERLEN;
			encText = g_realloc(encText, bufsize);
			decText = g_realloc(decText, bufsize);
		}

		if (isalpha(c)) {
			encText[textLen++] = tolower(c);
      freq[tolower(c)-'a'] += 1;
		}
	}

  fclose(fp);

  /* Read in correct solution if given */
  if (solution != NULL) {
    if ((fp = fopen(solution, "r")) == NULL) {
      g_critical("Error opening file '%s' for reading\n", solution);
      return FALSE;
    }

    solText = g_malloc(bufsize);

    int solLen = 0;

    while (!feof(fp)) {
      int c = getc(fp);

      if (solLen > bufsize) {
        bufsize += MAXCIPHERLEN;
        solText = g_realloc(solText, bufsize);
      }        

      if (isalpha(c)) {
        solText[solLen++] = tolower(c);
      }
    }

    if (solLen != textLen) {
      g_warning("Length of solution is incorrect\n");
    }

    fclose(fp);
  }

	printf("\nCryptogram file \'%s\' loaded", file);
	printf("\nLength: %d characters\n\n", textLen);

  vowIdentify();
  srand(time(0));

  return TRUE;
}


/**
 * cryptoFree: Clean up allocated resources
 *
 * @Returns: FALSE if an error occurs
 **/
gboolean
cryptoFree (void)
{
  g_free(encText);
  g_free(decText);

  return TRUE;
}


/**
 * cryptoEval: Evaluate a potential decryption key
 *
 * @key: The decryption key to evaluate
 *
 * @Returns: Numeric score between -INFINITY and 0 (closer to 0 is better)
 **/
double
cryptoEval (char *key)
{
  char tryText[textLen];
  
  for (int i = 0; i < textLen; i++) {
    tryText[i] = key[encText[i]-'a'];
  }

  return scoreEval(tryText, textLen);
}


/**
 * cryptoSolve: Solve a cryptogram
 *
 * @Returns: Best decryption key found
 **/
void
cryptoSolve (void)
{
  GTimer      *timer = g_timer_new();
  GThreadPool *tpool;
  
  bestFit   = -INFINITY;
  bestTrial = 0;
  bestGen   = 0;

  numLeft   = numTrials;

  updateBestMutex = g_mutex_new();

  tpool = g_thread_pool_new(genSolve, NULL, maxThreads, TRUE, NULL);

  for (int i = 1; i <= numTrials; i++) {
    g_thread_pool_push(tpool, GINT_TO_POINTER(i), NULL);
  }

  guint tleft;
  guint trunning;

  do {
    tleft = g_thread_pool_unprocessed(tpool);
    trunning = g_thread_pool_get_num_threads(tpool);
    guint etime = g_timer_elapsed(timer, NULL);

		printf("\rThreads Running: %d\tIn Queue: %3d\tElapsed Time: %02d:%02d:%02d",
		       trunning, tleft, etime / 3600, (etime % 3600) / 60, etime % 60);
		fflush(stdout);
		
		g_usleep(G_USEC_PER_SEC / 5);
	} while (numLeft > 0);

	g_thread_pool_free(tpool, FALSE, TRUE);
	g_timer_destroy(timer);

  g_mutex_free(updateBestMutex);
}


/**
 * cryptoPrint: Print best solution
 *
 * @key: Decryption key
 *
 * @Returns: Nothing
 **/
void
cryptoPrint (char *key)
{
  for (int i = 0; i < textLen; i++) {
    decText[i] = key[encText[i]-'a'];
  }

  printf("\n\n");
  
  int nlines = (textLen / 50) + 1;

	for (int i = 0; i < nlines; i++) {
		for (int j = i * 50; j < (i + 1) * 50; j++) {
			if (j == textLen) {
				break;
			}
			printf("%c", toupper(encText[j]));
			if (j % 5 == 4) {
				printf(" ");
			}
		}
		printf("\n");
		for (int k = i * 50; k < (i + 1) * 50; k++) {
			if (k == textLen) {
				break;
			}
			printf("%c", decText[k]);
			if (k % 5 == 4) {
				printf(" ");
			}
		}
		printf("\n\n");
	}
}
