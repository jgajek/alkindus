/*
 * main.c
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
#include <stdio.h>

#include "solve.h"


int ngramLen    = 3;
int numTrials   = 5;
int maxThreads  = 2;
int popSize     = 100;
int maxGens     = 150;
int muteRate    = 3;


/* Command line summary and options */
static const gchar *cmdSummary =
  "Simple substitution cryptogram solver.";

static const GOptionEntry cmdOption[] = {
  { "max-generations", 'g', 0, G_OPTION_ARG_INT, &maxGens,
    "Maximum number of generations (default=150)" },
  { "mutation-rate", 'm', 0, G_OPTION_ARG_INT, &muteRate,
    "Percent chance of mutation (default=3)" },
	{ "ngram-length", 'n', 0, G_OPTION_ARG_INT, &ngramLen,
		"n-gram length (default=3)" },
  { "max-threads", 'p', 0, G_OPTION_ARG_INT, &maxThreads,
    "Maximum number of concurrent threads (default=2)" },
  { "population-size", 's', 0, G_OPTION_ARG_INT, &popSize,
    "Size of population (default=100)" },
  { "num-trials", 't', 0, G_OPTION_ARG_INT, &numTrials,
    "Number of trials (default=5)" },
	{ NULL }
};


int
main (int argc, char *argv[])
{
  g_thread_init(NULL);
  
	GOptionContext *optc =
    g_option_context_new("<cryptogram file> [<solution file>]");
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

  if (maxThreads < 1) {
    g_critical("maximum threads parameter out of range\n");
    return 1;
  }

  if (numTrials < 1) {
    g_critical("number of trials parameter out of range\n");
    return 1;
  }

  if (muteRate < 0 || muteRate > 100) {
    g_critical("mutation rate parameter out of range\n");
    return 1;
  }

	if (argc < 2) {
		gchar *usage = g_option_context_get_help(optc, TRUE, NULL);
		g_printerr("%s\n", usage);
		g_free(usage);
		return 1;
	}
  
  scoreInit("ngramscores");

  solText = NULL;

  if (cryptoLoad(argv[1], argv[2]) == TRUE) {
    cryptoSolve();
    cryptoPrint(bestKey);

    char encKey[NUMSYMBOLS];

    for (int i = 0; i < NUMSYMBOLS; i++) {
      encKey[bestKey[i]-'a'] = 'a'+i;
    }
    encKey[NUMSYMBOLS] = NUL;
    
    printf("\nENCRYPTION KEY: %s", encKey);
    printf("\nDECRYPTION KEY: %s", bestKey);

    printf("\nSCORE: %f  TRIAL: %d  GENERATION: %d\n", 
           bestFit, bestTrial, bestGen);

    if (solText != NULL) {
      printf("\nSCORE OF TRUE SOLUTION: %f\n", scoreEval(solText, textLen));
    }
  }

  cryptoFree();
  scoreDone();

  g_option_context_free(optc);  
  return 0;
}
