/***************************************************************
  Simple, non thread-safe (but hopefully fast) linked list for
  storing GRT words.
****************************************************************/

#ifndef GRT_LIST_H
#define GRT_LIST_H

#include "grt_types.h"

typedef struct grt_list_node_t {
  struct grt_list_node_t *next;
  grt_word_t contents;
} grt_list_node_t;

typedef struct {
  grt_list_node_t *head;
  grt_list_node_t *tail;
  grt_list_node_t *next;
  unsigned size;
} grt_list_t;

/* Create a new grt list */

grt_list_t *grt_list_create();

/* Destroy a grt list and all its contained data */

void grt_list_destroy(grt_list_t *list);

/* Append a word to the list */

void grt_list_append(grt_list_t *list, grt_word_t contents);

/* Remove a word from the front of the list */

grt_word_t grt_list_remove(grt_list_t *list);

/* Iterator stuff */

void grt_list_start(grt_list_t*);
grt_list_node_t *grt_list_get_next(grt_list_t*);

#define grt_list_foreach_node(__l, __n) \
  for (grt_list_start(__l); __n = grt_list_get_next(__l); )

#define grt_list_foreach(__t, __l, __c)			\
  for (grt_list_start(__l); __c = \
	 (__t) grt_list_get_next(__l), __c = __c ? \
         (__t) ((grt_list_node_t*) __c)->contents : 0; )

#endif

