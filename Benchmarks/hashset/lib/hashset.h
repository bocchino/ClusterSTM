#ifndef HASHSET_H
#define HASHSET_H

#include "grt_types.h"

typedef struct node_t {
  struct node_t *next;
  grt_word_t val;
} node_t;

typedef struct hash_t {
  unsigned proc;
  unsigned offset;
} hash_t;

hash_t compute_hash(grt_word_t val);
/* Initialize the application */
void hashset_init(int argc, char **argv, 
		  unsigned short share_power);
/* Make a new distributed hash table */
void hashset_create(unsigned size, grt_bool_t pthread);
/* Insert */
grt_bool_t hashset_insert(grt_word_t val);
/* Find */
grt_bool_t hashset_find(grt_word_t val);
/* Destroy the hashset */
void hashset_destroy();
/* Exit the application */
int __hashset_exit(int ret_val);
#define hashset_exit(ret_val) return __hashset_exit(ret_val)

#endif
