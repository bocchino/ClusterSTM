#ifndef HASHMAP_H
#define HASHMAP_H

#include "grt_types.h"
#include "math.h"

typedef struct {
  grt_bool_t found;
  grt_word_t val;
} find_result_t;

typedef struct node_t {
  grt_word_t key;
  grt_word_t val;
  struct node_t *next;
} node_t;

typedef struct hash_t {
  unsigned proc;
  unsigned offset;
} hash_t;

hash_t compute_hash(grt_word_t val);
/* Initialize the application */
void hash_map_init(int argc, char **argv, 
		   unsigned short share_power);
/* Make a new hash_map */
void hash_map_create(unsigned size, grt_bool_t pthread);
#ifdef LOCKS
/* Exposed locks */
void hash_map_lock(gasnet_node_t proc, unsigned offset,
		  grt_lock_type_t type, grt_lock_state_t *state);
void hash_map_unlock(gasnet_node_t proc, unsigned offset);
void lock(grt_word_t key1, grt_word_t key2,
	  grt_lock_state_t *state1, grt_lock_state_t *state2);
void unlock(grt_word_t key1, grt_word_t key2);
#endif
/* Insert and find */
grt_bool_t hash_map_insert(grt_word_t key, grt_word_t val);
grt_bool_t hash_map_find(grt_word_t key, grt_word_t *val);
/* Destroy and exit */
void hash_map_destroy();
int __hash_map_exit(int ret_val);
#define hash_map_exit(ret_val) return __hash_map_exit(ret_val)

#endif
