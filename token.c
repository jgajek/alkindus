/*
 * token.c
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
#include <string.h>

#include "ngram.h"


#define MAXTOKENLEN   255
#define NUL           '\0'


static gboolean tokenNext    (void);
static gboolean tokenProcess (gchar *tp, guint len);


static const gchar punct[] = {
  ',', '.', ':', ';', '-', '+', '/', '\\', '\'', '&', '@', '_', NUL
};

static gchar *fn;
static FILE  *fp;

static gchar *bp, *cp, *ep;
static gchar *tok;

static gint line;

// TRUE if unemitted token waiting in buffer
static gboolean tokenWaiting;


/**
 * tokenInit: Initialize the tokenizer
 *
 * @file: Path to corpus text file
 *
 * @Returns: FALSE if an error occurs
 **/
gboolean
tokenInit (const char *file)
{
	if ((fp = fopen(file, "r")) == NULL) {
    g_critical("Error opening file '%s' for reading\n", file);
    return FALSE;
	}

  tokenWaiting = FALSE;
  line = 1;

  fn = g_strdup(file);

	tok = g_malloc(MAXTOKENLEN+1);
	bp = cp = ep = NULL;

	return TRUE;
}


/**
 * tokenGetBlock - Tokenize the input file one block at a time
 *
 * @buf: Pointer to block buffer
 * @len: Length of block buffer
 *
 * @Returns: Number of characters placed in block buffer
 **/
gint
tokenGetBlock (gchar *buf,
               gint   len)
{
  g_assert(len > MAXTOKENLEN);
  
	ssize_t m;
	size_t n = 0;
	size_t a = MAXTOKENLEN;
  gint nchars = 0;

  // Emit waiting token if present
  if (tokenWaiting == TRUE) {
    nchars = strlen(tok);
    if (nchars > len) {
      g_critical("Length of token on line %d in file '%s' exceeds %d\n",
                 line, fn, len);
      return 0;
    }
    memcpy(buf, tok, nchars);
  }

  // Fill up block buffer with tokens	
	while (!feof(fp)) {
		
		if (cp == ep) {
			if ((m = getline(&bp, &n, fp)) < 1) {
				if (!feof(fp)) {
          g_critical("Error reading line %d in file '%s'\n", line, fn);
					return nchars;
				}
				break;
			}

      // Allocate bigger token buffer if line too long
			if (m > a) {
				a = MAX(m+1, n);
        tok = g_realloc(tok, a);
			}

			cp = bp;
			ep = bp + m;
		}

		if (tokenNext() == TRUE) {
			if (tokenProcess(tok, strlen(tok)) == TRUE) {
        guint toklen = strlen(tok);
        if (toklen > len - nchars) {
          // Prepend last ngramLen-1 characters from previous block
          if (a < toklen + ngramLen) {
            a += MAXTOKENLEN+1;
            tok = g_realloc(tok, a);
          }
          memmove(&tok[ngramLen-1], tok, toklen+1);
          memcpy(tok, &buf[nchars-ngramLen+1], ngramLen-1);
          tokenWaiting = TRUE;
          break;
        }
				memcpy(&buf[nchars], tok, toklen);
        nchars += toklen;
			}
		} else {
      guint toklen = strlen(tok);
      if (toklen > len - nchars) {
        // Prepend last ngramLen-1 characters from previous block
        if (a < toklen + ngramLen) {
          a += MAXTOKENLEN+1;
          tok = g_realloc(tok, a);
        }
        memmove(&tok[ngramLen-1], tok, toklen+1);
        memcpy(tok, &buf[nchars-ngramLen+1], ngramLen-1);
        tokenWaiting = TRUE;
        break;
      }
      memcpy(&buf[nchars], tok, toklen);
      nchars += toklen;
		}

    line++;
	}

	return nchars;
}


/**
 * tokenEnd - Terminate the tokenizer and clean up
 *
 * @Returns: Nothing
 **/
void
tokenEnd (void)
{
	fclose(fp);

  g_free(fn);
	g_free(bp);
	g_free(tok);
}


/**
 * tokenNext: Extract next word token from the input text
 *
 * @Returns: TRUE if post-processing of token is needed
 **/
static gboolean
tokenNext (void)
{
	gchar *tp = tok;
	gboolean a = FALSE;

	while (cp != ep) {	
		if (!isspace(*cp)) {
			break;
		}
		cp++;		
	}

	if (cp == ep) {		
		*tp = NUL;	
		return TRUE;
	}

	while (!isspace(*cp)) {
		*tp++ = tolower(*cp);
		if (!isalpha(*cp)) {
			a = TRUE;
		}
		cp++;
		if (cp == ep) {
			break;
		}
	}
	*tp = NUL;
	return a;
}


/**
 * tokenProcess: Perform post-processing of tokens
 *
 * @Returns: TRUE if an unemitted token remains in the buffer
 **/
static gboolean
tokenProcess (gchar *tp, 
              guint len)
{
	gchar *lp, *mp;
	
	if (len == 0 || (isspace(*tp) && len == 1)) {
		return FALSE;
	}

	/* Strip off opening punctuation */
	if (ispunct(*tp)) {	
		memmove(tp, tp+1, len);			
		return tokenProcess(tp, len-1);
	}

	lp = tp + len - 1;

	/* Strip off closing punctuation */
	if (ispunct(*lp)) {
		*lp = NUL;		
		return tokenProcess(tp, len-1);
	}

  /* Remove embedded punctuation */
  for (int i = 0; punct[i] != NUL; i++) {
    if ((mp = strchr(tp, punct[i])) != NULL) {
      memmove(mp, mp+1, strlen(mp+1)+1);
      return tokenProcess(tp, strlen(tp));
    }
  }
  
  // Discard token if any non-alpha characters still remaining
	while (*tp != NUL) {
		if (!isalpha(*tp)) {
			return FALSE;
		}
		tp++;
	}
		
	return TRUE;
}
