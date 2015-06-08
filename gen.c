/*
 * gen.c
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
#include <stdlib.h>
#include <string.h>

#include "solve.h"


#define MAXSWAPS        100


static void genInit   (char **popKey, double *popFit);
static void genMate   (char **popKey, double *popFit);
static void genMutate (char **popKey, double *popFit);
static int  genSelect (void);

static void genCrossover (char **popKey,
                          int    x,
                          int    y,
                          char  *child);

static void genSort   (char  **popKey,
                       double *popFit);


GMutex *updateBestMutex = NULL;

char    bestKey[NUMSYMBOLS+1];
double  bestFit;
int     bestTrial;
int     bestGen;


/**
 * genSolve: Solve cryptogram using genetic algorithm
 *
 * @trial: Number of current trial
 *
 * @Returns: Nothing
 **/
void
genSolve (gpointer trial,
          gpointer udata)
{
  char   *popKey[popSize];
  double *popFit;

  for (int i = 0; i < popSize; i++) {
    popKey[i] = g_malloc(NUMSYMBOLS+1);
  }

  popFit = g_malloc(popSize * sizeof(double));

  genInit(popKey, popFit);
  genSort(popKey, popFit);

  for (int j = 1; j <= maxGens; j++) {
    genMate(popKey, popFit);
    genSort(popKey, popFit);

    g_mutex_lock(updateBestMutex);
  
    if (popFit[0] > bestFit && strcmp(bestKey, popKey[0])) {
      strcpy(bestKey, popKey[0]);
      bestFit   = popFit[0];
      bestTrial = GPOINTER_TO_INT(trial);
      bestGen   = j;
    }

    g_mutex_unlock(updateBestMutex);
   
    genMutate(popKey, popFit);
    genSort(popKey, popFit);
  }

  for (int i = 0; i < popSize; i++) {
    g_free(popKey[i]);
  }

  g_free(popFit);

  g_mutex_lock(updateBestMutex);
  numLeft -= 1;
  g_mutex_unlock(updateBestMutex);
}


/**
 * genInit: Generate initial population of random keys
 *
 * @Returns: Nothing
 **/
static void
genInit (char **popKey, double *popFit)
{  
  static char genVow[] = "aeiouyt";
  static char genKey[] = "aeiouytbcdfghjklmnpqrsvwxz";

  for (int i = 0; i < popSize; i++) {
    memset(popKey[i], NUL, NUMSYMBOLS+1);
    
    for (int j = 0; j < numVowels; j++) {
      popKey[i][vowels[j]-'a'] = genVow[j];
    }

    int c = 0;

    for (int k = 0; k < NUMSYMBOLS; k++) {
      if (popKey[i][k] == NUL) {
        popKey[i][k] = genKey[c+numVowels];
        c += 1;
      }
    }

    int numSwaps = rand() % MAXSWAPS;

    for (int j = 0; j < numSwaps; j++) {
      int x;
      int y;

      /* Mix up the consonants */
      do {
        x = rand() % NUMSYMBOLS;
      } while (isVowel[x] == TRUE);

      do {
        y = rand() % NUMSYMBOLS;
      } while (isVowel[y] == TRUE || y == x);

      char tmp = popKey[i][x];
      popKey[i][x] = popKey[i][y];
      popKey[i][y] = tmp;

      /* Mix up the vowels */
      x = rand() % numVowels;
      do {
        y = rand() % numVowels;
      } while (y == x);

      tmp = popKey[i][vowels[x]-'a'];
      popKey[i][vowels[x]-'a'] = popKey[i][vowels[y]-'a'];
      popKey[i][vowels[y]-'a'] = tmp;
    }

    popFit[i] = cryptoEval(popKey[i]);
  }
}


/**
 * genMate: Simulate mating process to generate next population of keys
 *
 * @Returns: Nothing
 **/
static void
genMate (char **popKey, double *popFit)
{
  char childKey[popSize][NUMSYMBOLS+1];
  
  for (int i = 0; i < popSize; i++) {
    /* Select two keys for mating */
    int x = i;
    int y;

    do {
      y = genSelect();
    } while (y == x);

    genCrossover(popKey, x, y, childKey[i]);
  }

  /* Replace parent population with child population */
  for (int i = 0; i < popSize; i++) {
    strcpy(popKey[i], childKey[i]);
    popFit[i] = cryptoEval(popKey[i]);
  }
}


/**
 * genMutate: Mutate child generation of keys
 *
 * @Returns: Nothing
 **/
static void
genMutate (char **popKey, double *popFit)
{
  for (int i = 0; i < popSize; i++) {
    int z = rand() % 100;

    if (z < muteRate) {
      int x;
      int y;

      do {
        x = rand() % NUMSYMBOLS;
      } while (freq[x] == 0);

      do {
        y = rand() % NUMSYMBOLS;
      } while (y == x || freq[y] == 0);
      
      char tmp = popKey[i][x];
      popKey[i][x] = popKey[i][y];
      popKey[i][y] = tmp;

      popFit[i] = cryptoEval(popKey[i]);
    }
  }
}


/**
 * genSelect: Select a key for mating. Keys with higher fitness have a
 *            greater probability of being selected.
 *
 * @Returns: Index of key selected (0 ... POPSIZE-1)
 **/
static int
genSelect (void)
{
  int k = rand() % (popSize * (popSize+1) / 2);
  int n = 0;

  for (int i = 0; i < popSize; i++) {
    n += popSize - i;
    if (k < n) {
      return i;
    }
  }
}


/**
 * genCrossover: Apply crossover operation to pass on "genetic material"
 *               from parents to child.
 *
 * @x: Index of first parent key
 * @y: Index of second parent key
 * @child1: Address where to store child key
 *
 * @Nothing
 **/
static void
genCrossover (char **popKey,
              int    x,
              int    y,
              char  *child)
{
  char testKey[NUMSYMBOLS+1];
  strcpy(testKey, popKey[x]);
  double testFit = cryptoEval(testKey);
  char tmp;
  
  for (int i = 0; i < NUMSYMBOLS; i++) {
    /* Look for a gene to pass on from parent 2 to child */
    if (popKey[x][i] != popKey[y][i]) {
      int j;
      for (j = 0; popKey[x][j] != popKey[y][i]; j++);
      tmp = testKey[i];
      testKey[i] = testKey[j];
      testKey[j] = tmp;
      if (cryptoEval(testKey) < testFit) {
        tmp = testKey[i];
        testKey[i] = testKey[j];
        testKey[j] = tmp;
      } else {
        testFit = cryptoEval(testKey);
      }
    }
  }
  strcpy(child, testKey);
}


/**
 * genSort: Sort population in order of descending fitness. Uses insertion
 *          sort algorithm.
 *
 * @Returns: Nothing
 **/
static void
genSort (char   **popKey,
         double  *popFit)
{
	for (int k = 1; k < popSize; k++) {
		int n = k-1;
		
		char  *key = popKey[k];
		double fit = popFit[k];

		while ((n >= 0) && (fit > popFit[n])) {
		  popKey[n+1] = popKey[n];
		  popFit[n+1] = popFit[n];
		  n--;
		}

	  popKey[n+1] = key;
		popFit[n+1] = fit;
  }
}
