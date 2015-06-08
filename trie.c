/*
 * trie.c
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

#include "ngram.h"


/**
 * The n-gram statistics are stored in a trie data structure, such that each
 * node at level K stores the counts for all n-grams having a particular
 * n-gram prefix of length K (e.g. the leftmost node at level 1 stores the
 * counts for all n-grams starting with 'A', the rightmost node at level 2
 * stores the counts for all n-grams starting with 'ZZ', etc.) Each node's 26
 * children store the counts for the corresponding prefixes of length K+1.
 * Thus, the leaf nodes of the trie store the counts for the fully specified
 * n-grams.  The root node (level 0) stores the total count for all n-grams.
 **/
trieNode *trieRoot = NULL;


/**
 * trieNodeNew: Allocate and initialize a new trie node
 *
 * @Returns: Pointer to the new node
 **/
trieNode *
trieNodeNew (void)
{
  trieNode *node = g_slice_new(trieNode);

  g_assert(node != NULL);

  node->ngram   = NULL;
  node->total   = 0L;
  node->parent  = NULL;

  for (int i = 0; i < NUMSYMBOLS; i++) {
    node->child[i] = NULL;
  }

  return node;
}


/**
 * trieNodeInsert: Allocate new trie node and insert it as n-th child of
 *                 parent node
 *
 * @node: Parent node
 * @n: Index of the new child
 *
 * @Returns: Pointer to the new child node or NULL
 **/
trieNode *
trieNodeInsert (trieNode *node, gint n)
{
  g_assert(n >= 0 && n < NUMSYMBOLS);
  g_assert(node->child[n] == NULL);

  trieNode *child = trieNodeNew();

  child->parent  = node;
  return (node->child[n] = child);
}

/**
 * trieGetChild: Get the n-th child of a trie node
 *
 * @n: Index of the child (0 .. NUMSYMBOLS-1)
 *
 * @Returns: Pointer to the child node or NULL
 **/
trieNode *
trieGetChild (trieNode *node, gint n)
{
  g_assert(n >= 0 && n < NUMSYMBOLS);

  return (n < NUMSYMBOLS) ? node->child[n] : NULL;
}


/**
 * trieTraverseAll: Traverse all nodes in order and invoke func() for each.
 *
 * @root: Root node of trie
 * @func: Function to be called for each node
 *
 * @Returns: Nothing
 **/
void
trieTraverseAll (trieNode        *root,
                 trieTraverseFunc func)
{
  for (int i = 0; i < NUMSYMBOLS; i++) {
    if (root->child[i] != NULL) {
      trieTraverseAll(root->child[i], func);
    }
  }

  func(root);
}


/**
 * trieTraverseLeaf: Traverse leaf nodes in order and invoke func() for each.
 *
 * @root: Root node of trie
 * @func: Function to be called for each leaf node
 *
 * @Returns: Nothing
 **/
void
trieTraverseLeaf (trieNode *root,
                  trieTraverseFunc func)
{
  if (root->ngram != NULL) {    // Only leaf nodes have ngram != NULL
    func(root);
  } else {
    for (int i = 0; i < NUMSYMBOLS; i++) {
      if (root->child[i] != NULL) {
        trieTraverseLeaf(root->child[i], func);
      }
    }
  }
}


/**
 * trieTraverseLevel: Traverse nodes at level n and invoke func() for each.
 *
 * @root: Root node of trie
 * @func: Function to be called for each node at level n
 * @n: Node level (root is level 0, children of root are level 1, etc.)
 *
 * @Returns: Nothing
 **/
void
trieTraverseLevel (trieNode *root,
                   trieTraverseFunc func,
                   guint n)
{
  if (n == 0) {
    func(root);
  } else {
    for (int i = 0; i < NUMSYMBOLS; i++) {
      if (root->child[i] != NULL) {
        trieTraverseLevel(root->child[i], func, n-1);
      }
    }
  }
}
