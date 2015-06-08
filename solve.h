/*
 * solve.h
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

#ifndef _SOLVE_H
#define _SOLVE_H

#include <glib.h>


#define NUL			    '\0'
#define NUMSYMBOLS  26
#define MAXNGRAMLEN 8
#define MAXVOWELS   7

extern GMutex *updateBestMutex;

extern char bestKey[];
extern double bestFit;
extern int bestTrial;
extern int bestGen;

extern int ngramLen;
extern int numTrials;
extern int maxThreads;
extern int maxGens;
extern int popSize;
extern int muteRate;
extern int freq[];

extern int numLeft;

extern gboolean isVowel[];
extern char vowels[];
extern int numVowels;

extern char *encText;
extern char *decText;
extern char *solText;
extern int textLen;


gboolean scoreInit  (const char *file);
gboolean scoreDone  (void);
double   scoreEval  (char *str, int len);

gboolean cryptoLoad (const char *file, const char *solution);
gboolean cryptoFree (void);

double  cryptoEval  (char *key);
void    cryptoSolve (void);
void	  cryptoPrint (char *key);

void	genSolve	    (gpointer trial, gpointer udata);

void  vowIdentify   (void);



#endif /* _SOLVE_H */
