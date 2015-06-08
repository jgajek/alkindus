/*
 * ngram.h
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

#ifndef NGRAM_H
#define NGRAM_H

#include <glib.h>
#include <stdio.h>

#define NUMSYMBOLS      26

struct s_trieNode {
  gchar *ngram;                   // N-gram character sequence
  glong  total;                   // Count of n-grams in corpus text
  struct s_trieNode *parent;      // Parent node
  struct s_trieNode *child[NUMSYMBOLS];
};

typedef struct s_trieNode trieNode;

typedef void (*trieTraverseFunc)(trieNode *root);

trieNode *trieNodeNew    (void);
trieNode *trieNodeInsert (trieNode *node, gint n);
trieNode *trieGetChild   (trieNode *node, gint n);

void trieTraverseAll    (trieNode *root, trieTraverseFunc func);
void trieTraverseLeaf   (trieNode *root, trieTraverseFunc func);
void trieTraverseLevel  (trieNode *root, trieTraverseFunc func, guint n);

gboolean tokenInit     (const char *file);
void     tokenEnd      (void);
gint     tokenGetBlock (gchar *buf, gint len);

void probGoodTuring (FILE *fp);


extern trieNode *trieRoot;
extern guint ngramLen;

#endif // NGRAM_H
