/*
  Implementation of the string TRIE utility.
  Copyright (C) 2008  Cristi Balas

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"


/* * * * * * * * * * * * * *
 *      NEW and DELETE     *
 * * * * * * * * * * * * * */
static void trie_init_node(TRIE_NODE *node)
{
  int i;

  for(i = 0; i < 256; i++)
    node->nxt[i] = 0;

  node->childs = 0;	// # children = 0
  node->is_final = 0;
  node->data = 0;
}

TRIE *trie_new() 
{
  TRIE *trie = malloc(sizeof(TRIE));
  trie->root = malloc(sizeof(TRIE_NODE));

  trie_init_node(trie->root);
  return trie;
}

static void trie_node_free(TRIE_NODE *node) 
{
  int i;
  for(i = 1; i < 256; i++)
    if(node->nxt[i])
      trie_node_free(node->nxt[i]);
  free(node->data); // free(0) is valid
  free(node);
}

void trie_delete(TRIE *trie) 
{
  trie_node_free(trie->root);
  free(trie);
}


/* * * * * * * * * * * * * *
 *          ADDING         *
 * * * * * * * * * * * * * */

static void *crt_user_data;
static int crt_user_data_len;
 
static int trie_add_nodes(TRIE_NODE *node, char *str, int pos)
{
  unsigned char ch = (unsigned char)str[pos]; // current char

  if( ch ) // ch != '\0', i.e., string not finished
  {
    if( 0 == node->nxt[ch] ) // construct next node
    {
      node->nxt[ch] = malloc(sizeof(TRIE_NODE));

      if(0 == node->nxt[ch]) // out of memory
        return 0;

      trie_init_node(node->nxt[ch]);
      node->childs++;
    }
    return trie_add_nodes(node->nxt[ch], str, pos + 1);
  }
  else
  { /* ---------------------------------------
       End of string... insert real data
       --------------------------------------- */
    if(node->is_final)
      free(node->data);		// There was some data -> de-allocate it

    node->is_final = 1;		// Mark node as data containing

    if(crt_user_data && crt_user_data_len)
    {
      node->data = malloc(crt_user_data_len);  	// Allocate space for data

      if(0 == node->data)
        return 0;		// Out of memory

      memcpy(node->data, crt_user_data, crt_user_data_len); // Copy data
    }
    else
      node->data = 0;

    return 1;
  }
}

int trie_add(TRIE *trie, char *str, void *user_data, int user_data_len)
{
  /* ============================================================
     Use global variables so we don't need to pass these stuff
     into a RECURSIVE function (too many parameters !)
     ============================================================ */
  crt_user_data = user_data;
  crt_user_data_len = user_data_len;

  return trie_add_nodes(trie->root, str, 0);  	// Call recursive function
						// to do the the job
}

/* * * * * * * * * * * * * *
 *        DELETING         *
 * * * * * * * * * * * * * */
static void trie_del_recursive(TRIE_NODE *node, char *str, int pos)
{
  unsigned char ch = (unsigned char)str[pos]; // current char
  if(ch) // string not finished
  {
    if(node->nxt[ch]) // next character is in trie
    {
      trie_del_recursive(node->nxt[ch], str, pos + 1);
      if(node->nxt[ch]->is_final == 0 && node->nxt[ch]->childs == 0)
      {
        free(node->nxt[ch]);
        node->nxt[ch] = 0;
        node->childs--;
      }
    }
  }
  else
  {
    if(node->is_final)
    {
      node->is_final = 0;
      free(node->data);
      node->data = 0;
    }
  }
}

void trie_del(TRIE *trie, char *str)
{
  trie_del_recursive(trie->root, str, 0);
}

/* * * * * * * * * * * * * *
 *        SEARCHING        *
 * * * * * * * * * * * * * */

static void *crt_search_data;

int trie_search_nodes(TRIE_NODE *node, char *str, int pos)
{
  unsigned char ch = (unsigned char)str[pos]; // current char

  if(ch) // string not finished
  {
    if(node->nxt[ch])
      return trie_search_nodes(node->nxt[ch], str, pos + 1);
    else
      return 0; // not found
  }
  else
  {
    if(node->is_final)
    {
      crt_search_data = node->data;
      return 1;
    }
    else
      return 0;
  }
}

int trie_search(TRIE *trie, char *str, void **p_user_data)
{
  crt_search_data = 0;
  if(trie_search_nodes(trie->root, str, 0))
  {
    if(p_user_data)
      *p_user_data = crt_search_data;
    return 1;
  }
  else
    return 0;
}
