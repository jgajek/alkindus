/*
 * prob.c
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ngram.h"


#define INITSIZE  16384     // Initial resizable array size


static void probCountNode (trieNode *node);
static void probEmitProb  (trieNode *node);
static void probGetCounts (guint level);
static void probBestFit   (void);

static guint ngramTotal;
static guint numCounts;

static FILE *outf;

/* Use resizable arrays r, n since we don't know their size in advance */

static GArray *r;       // Array of n-gram frequencies seen
static GArray *n;       // Count for each n-gram frequency seen

static double *Z;       // Frequency counts with averaging transform applied
static double *log_Z;   // log(Z)
static double *log_r;   // log(r)
static double a;        // Intercept of the line of best fit
static double b;        // Slope of the line of best fit
#define SMOOTH(n)       (exp(a + b * log(n)))

static double *rStar;   // Good-Turing n-gram frequency estimates
static double *p;       // Good-Turing n-gram probability estimates
static double pZero;    // Good-Turing total probability for unseen n-grams


/**
 * probGoodTuring: Calculate estimates for n-gram probabilities using
 *                 Simple Good-Turing (SGT) method (see Gale & Sampson,
 *                 "Good-Turing Frequency Estimation Without Tears", 1995)
 *
 * @fp: File pointer to output file
 *
 * @Returns: Nothing
 **/
void
probGoodTuring (FILE *fp)
{
  outf = fp;
  
  /* Compute frequency counts for observed n-grams */
  r = g_array_sized_new(FALSE, FALSE, sizeof(gint), INITSIZE);
  n = g_array_sized_new(FALSE, FALSE, sizeof(gint), INITSIZE);

  probGetCounts(ngramLen);

  /* Compute probability estimate for all unseen n-grams */
  pZero = g_array_index(n, gint, 0) / (double)ngramTotal;

  /* Apply the averaging transform to the observed frequency counts */
  Z = g_malloc_n(numCounts, sizeof(double));

  double N, R1, R2;

  N  = g_array_index(n, gint, 0);
  R1 = 0;
  R2 = g_array_index(r, gint, 1); 
  Z[0] = 2 * N / (R2 - R1);

  for (guint i = 1; i < numCounts-1; i++) {
    N  = g_array_index(n, gint, i);
    R1 = g_array_index(r, gint, i-1);
    R2 = g_array_index(r, gint, i+1);
    Z[i] = 2 * N / (R2 - R1);
  }

  N  = g_array_index(n, gint, numCounts-1);
  R1 = g_array_index(r, gint, numCounts-2);
  R2 = g_array_index(r, gint, numCounts-1);  
  Z[numCounts-1] = N / (R2 - R1);
     
  /* Smooth the observed frequency counts using SGT estimator */
  log_Z = g_malloc_n(numCounts, sizeof(double));
  log_r = g_malloc_n(numCounts, sizeof(double));
  rStar = g_malloc_n(numCounts, sizeof(double));
     
  for (guint i = 0; i < numCounts; i++) {
    log_Z[i] = log(Z[i]);
    log_r[i] = log(g_array_index(r, gint, i));
  }

  probBestFit();

  for (guint i = 0; i < numCounts; i++) {
    gint R = g_array_index(r, gint, i);
    rStar[i] = (R+1) * SMOOTH(R+1) / SMOOTH(R);
  }

  g_free(Z);
  g_free(log_Z);
  g_free(log_r);

  /* For small r, using Turing estimator directly is preferable over SGT */
  for (guint i = 0; i < numCounts; i++) {
    gint R  = g_array_index(r, gint, i);
    gint R1 = g_array_index(r, gint, i+1);

    if (R1 != R+1) {
      break;
    }

    double N  = g_array_index(n, gint, i);
    double N1 = g_array_index(n, gint, i+1);

    double x = (R+1) * N1 / N;
    double d = fabs(x - rStar[i]);

    if (d <= 1.96 * sqrt((R+1) * (R+1) * (N1 / (N * N)) * (1 + N1 / N))) {
      break;
    }

    rStar[i] = x;
  }

  /* Renormalize the estimated n-gram probabilities */
  p = g_malloc_n(numCounts, sizeof(double));
  double newTotal = 0;

  for (guint i = 0; i < numCounts; i++) {
    gint N = g_array_index(n, gint, i);
    newTotal += rStar[i] * N;
  }

  for (guint i = 0; i < numCounts; i++) {
    p[i] = (1 - pZero) * rStar[i] / newTotal;
  }
  
  g_free(rStar);

  /* Write log(p) for n-grams into probability table */
  trieTraverseLevel(trieRoot, probEmitProb, ngramLen);

  g_array_free(r, TRUE);
  g_array_free(n, TRUE);
  g_free(p);
}


/**
 * probGetCounts: Calculate frequencies of frequencies
 *
 * @level: The trie node level (i.e. n-gram length)
 *
 * @Returns: Nothing
 **/
static void
probGetCounts (guint level)
{
  ngramTotal = 0;
  
  trieTraverseLevel(trieRoot, probCountNode, level);

  numCounts = r->len;
}


/**
 * probCountNode: Count node frequency
 *
 * @node: Pointer to the trie node
 *
 * This function is invoked for nodes at a given level via trieTraverseLevel()
 *
 * @Returns: Nothing
 **/
static void
probCountNode (trieNode *node)
{
  static gint one = 1;
  ngramTotal += node->total;
  
  for (guint i = 0; i < r->len; i++) {
    if (node->total <= g_array_index(r, gint, i)) {
      if (node->total == g_array_index(r, gint, i)) {
        g_array_index(n, gint, i) += 1;
      } else {
        g_array_insert_val(r, i, node->total);
        g_array_insert_val(n, i, one);
      }
      return;
    }
  }

  g_array_append_val(r, node->total);
  g_array_append_val(n, one);
}


/**
 * probEmitProb: Write probability for n-gram to probability table
 *
 * @node: Pointer to n-gram node
 *
 * @Returns: Nothing
 **/
static void
probEmitProb (trieNode *node)
{
  int cmp_int(const void *u, const void *v)
  {
    return *(const gint *)u - *(const gint *)v;
  }
  
  for (int i = 0; i < ngramLen; i++) {
    putc(node->ngram[i], outf);
  }

  gint *e = bsearch(&(node->total), r->data, numCounts, sizeof(gint), cmp_int);
  gint index = (e - (gint *)(r->data));

  fprintf(outf, "\t%16.10e\n", p[index]);
}


/**
 * probBestFit: Calculate line of best fit for log(Z) vs log(r)
 *
 * @Returns: Nothing
 **/
static void
probBestFit (void)
{
  double XY, Xsquare, meanX, meanY;

  XY = Xsquare = meanX = meanY = 0;

  for (int i = 0; i < numCounts; i++) {
    meanX += log_r[i];
    meanY += log_Z[i];
  }

  meanX /= numCounts;
  meanY /= numCounts;

  for (int i = 0; i < numCounts; i++) {
    XY += (log_r[i] - meanX) * (log_Z[i] - meanY);
    Xsquare += (log_r[i] - meanX) * (log_r[i] - meanX);
  }

  b = XY / Xsquare;
  a = meanY - b * meanX;
}  
