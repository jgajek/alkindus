/*
 * vowel.c
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
#include <string.h>

#include "solve.h"

gboolean isVowel[NUMSYMBOLS];
char vowels[MAXVOWELS];
int  numVowels = 0;

/**
 * vowIdentify: Identify which ciphertext characters most likely represent
 *              vowels. Uses Sukhotin's algorithm.
 *
 * @Returns: Nothing
 **/
void
vowIdentify (void)
{
	int cmat[NUMSYMBOLS][NUMSYMBOLS];
	int csum[NUMSYMBOLS];

	memset(cmat, 0, NUMSYMBOLS * NUMSYMBOLS * sizeof(int));
	memset(csum, 0, NUMSYMBOLS * sizeof(int));

  for (int i = 0; i < NUMSYMBOLS; i++) {
    isVowel[i] = FALSE;
  }

  /* Initialize adjacency matrix */
	for (int i = 1; i < textLen; i++) {
		cmat[encText[i]-'a'][encText[i-1]-'a'] += 1;
		cmat[encText[i-1]-'a'][encText[i]-'a'] += 1;
	}

	for (int j = 0; j < NUMSYMBOLS; j++) {
		cmat[j][j] = 0;

		for (int k = 0; k < NUMSYMBOLS; k++) {
			csum[j] += cmat[j][k];
		}
	}

	while (1) {
		int maxSum = 0;
		int index;
		
		for (int i = 0; i < NUMSYMBOLS; i++) {
			if (isVowel[i] == FALSE && csum[i] > maxSum) {
        maxSum = csum[i];
        index = i;
      }
		}

		if (maxSum > 0 && numVowels < MAXVOWELS) {
			isVowel[index] = TRUE;
			vowels[numVowels] = 'a' + index;
			numVowels += 1;
		} else {
			break;
		}

		for (int i = 0; i < NUMSYMBOLS; i++) {
			if (isVowel[i] == FALSE) {
				csum[i] -= cmat[i][index] * 2;
			}
		}
	}

  printf("PROBABLE VOWELS: ");

  for (int i = 0; i < numVowels; i++) {
    printf("%c ", vowels[i]);
  }

  printf("\n\n");
}
