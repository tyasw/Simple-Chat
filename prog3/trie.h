/*
  Definitions for the string TRIE utility.
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

/*
  General description:
  A string can be anything, including not printable characters.
    It should always end with 0, so 0 can't be part of a string.

  The TRIE is used when one has a big collection of strings and
    want to search if a string is part of that collection.
   The time complexity of the main functions is:
     trie_add(..)    - O(string_length)
     trie_del(..)    - O(string_length)
     trie_search(..) - O(string_length)
   So it doesn't matter how big is the trie.
*/
#ifndef _TRIE_H_
#define _TRIE_H_


/*
  struct TRIE_NODE: an internal structure for holding a node in the TRIE
  Programmer does not need to use this.
*/
typedef struct sTRIE_NODE 
{
  char is_final;        // 1 if the current string is in the trie, 0 otherwise
			// 1 if the node is a data storage node
			//   ==> data will contain data stored
			// 0 if node is an internal node
			//   ==> use nxt[i] to search further

  void *data;           // user data that is stored with this node
			// Only use this field if  is_final == 1

  unsigned char childs; // number of childs. useful for fast deletion
			// (If childs becomes 0, we delete this node)

  struct sTRIE_NODE *nxt[256]; // child pointers

} TRIE_NODE;

/*
  The TRIE structure. This is the main object of this library.
  This should be used only with trie_new() pointers.
    For example, `TRIE *t = trie_new();`
       NOT `TRIE t;`
*/
typedef struct 
{
  TRIE_NODE *root;
} TRIE, *pTRIE;


/* ==============================================================
	Functions
   ============================================================== */

/* 
  Allocate and initialize a TRIE structure.
  Return value: a pointer to that usable TRIE structure
*/
TRIE *trie_new();

/*
  Free the memory used by a TRIE.
  It also frees the trie pointer.
*/
void trie_delete(TRIE *trie);


/*
  Adds a string to the trie, and some user data of 
    length user_data_len associated with that string.

  That user_data will be malloc-ed and copied again internally
    for the trie.
  user_data can be 0 if no data should be linked to this string.

  If the string already exists, the user data is overwritten,

  Return value: 1 if string was added successfully
                0 if can't malloc something
*/
int trie_add(TRIE *trie, char *str, void *user_data, int user_data_len);


/*
  Deletes a string from the trie, and all remaining unused nodes.

  No return value (this function should be successful all the time)
  If the string is not in the trie, this function does nothing.
*/
void trie_del(TRIE *trie, char *str);


/*
  Search a string in the TRIE.

  Return value: 1 if string is found
                0 otherwise
    If string is found, p_user_data will be set to the user_data pointer.
    p_user_data can be 0 if the application is not interested in the
      user_data value.
*/
int trie_search(TRIE *trie, char *str, void **p_user_data);

#endif // _TRIE_H_
