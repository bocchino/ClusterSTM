#ifndef HASHTABLE_H
#define HASHTABLE_H

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
void hashtable_init(int argc, char **argv);
/* Make a new distributed hash table */
void hashtable_create(unsigned size);
/* Insert */
grt_bool_t hashtable_insert(grt_word_t val);
/* Find */
grt_bool_t hashtable_find(grt_word_t val);
/* Destroy the hashtable */
void hashtable_destroy();
/* Exit the application */
int __hashtable_exit(int ret_val);
#define hashtable_exit(ret_val) return __hashtable_exit(ret_val)

#endif
