/***************************************************************
  Simple, non thread-safe (but hopefully fast) implementation
  of a map for storing and retrieving (key,value) pairs, where
  key and value are both grt words.
****************************************************************/

#ifndef GRT_MAP_H
#define GRT_MAP_H

//#include "grt.h"
#include "grt_types.h"
#include "grt_list.h"

#define GRT_MAP_TABLE_SIZE 100

typedef struct {
  grt_word_t key;
  grt_word_t value;
} grt_pair_t;

typedef struct {
  grt_list_t **table;
} grt_map_t;

/* Create a new grt map */

grt_map_t *grt_map_create();

/* Destroy a grt map and all its contained data */

void grt_map_destroy(grt_map_t *map);

/* Insert the given (key, value) pair in the map */

int grt_map_insert(grt_map_t *map, grt_word_t key, grt_word_t value);

/* Return TRUE if the key was found in the table and FALSE if it
   wasn't.  If the key was found and the value pointer is nonzero,
   copy the value corresponding value there; otherwise leave the value
   unchanged. */

grt_bool_t grt_map_find(grt_map_t *map, grt_word_t key, 
			grt_word_t *value);


#endif
