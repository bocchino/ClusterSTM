#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#include "hashmap.h"
#include "stm-distrib.h"
#include "grt_debug.h"

#define N 3000
#define TABLE_SIZE 6000

typedef struct {
  grt_word_t first;
  grt_word_t second;
} pair_t;

grt_word_t keys[N];
grt_word_t values[N];

int main(int argc, char** argv) {
  hash_map_init(argc, argv, 4);

  struct timeval tv;
  gettimeofday(&tv, 0);
  srand(tv.tv_usec);
  unsigned i;

  hash_map_create(TABLE_SIZE, GRT_TRUE);

  BARRIER();

  grt_debug_print("generating keys and values\n");
  for (i = 0; i < N; ++i) {
    keys[i] = N*grt_id + i;
    values[i] = rand();
  }

  BARRIER();

  grt_debug_print("inserting (key, value) pairs\n");
  grt_lock_state_t state;
  for (i = 0; i < N; ++i) {
    grt_word_t key = keys[i], val = values[i];
#ifdef LOCKS
    hash_t hash = compute_hash(key);
    hash_map_lock(hash.proc, hash.offset, WRITE, &state);
#endif
    hash_map_insert(key, val);
#ifdef LOCKS
    hash_map_unlock(hash.proc, hash.offset);
#endif
  }

  BARRIER();

  grt_debug_print("swapping pairs of values\n");
#ifdef LOCKS
  grt_lock_state_t state1, state2;
#endif
  for (i = 0; i < N; ++i) {
    grt_word_t key1 = keys[i];
    unsigned second_idx = (i < N-1) ? i+1 : 0;
    grt_word_t key2 = keys[second_idx];
#ifdef LOCKS
    lock(key1, key2, &state1, &state2);

#endif
    grt_word_t val1, val2;
#ifndef LOCKS
    stm_start(grt_id);
#endif
    grt_bool_t found1 = hash_map_find(key1, &val1);
    grt_bool_t found2 = hash_map_find(key2, &val2);
    hash_map_insert(key1, val2);
    hash_map_insert(key2, val1);
#ifndef LOCKS
    stm_commit(grt_id);
#endif
    if (found1 == GRT_FALSE) {
      fprintf(stderr, "processor %d: expected to find key %llx\n", 
	      grt_id, key1);
    }
    if (found2 == GRT_FALSE) {
      fprintf(stderr, "processor %d: expected to find key %llx\n", 
	      grt_id, key2);
    }
    grt_word_t tmp = values[i];
    values[i] = values[second_idx];
    values[second_idx] = tmp;
#if LOCKS
    unlock(key1, key2);
#endif
  }

  grt_barrier();

  grt_debug_print("finding keys and values\n");
  for (i = 0; i < N; ++i) {
    grt_word_t key = keys[i];
    grt_word_t value;
    hash_t hash = compute_hash(key);
#ifdef LOCKS
    hash_map_lock(hash.proc, hash.offset, READ, &state);
#endif
    grt_bool_t found = hash_map_find(key, &value);
#ifdef LOCKS
    hash_map_unlock(hash.proc, hash.offset);
#endif
    if (found == GRT_FALSE) {
      fprintf(stderr, "processor %d: expected to find key %llx\n", 
	      grt_id, key);
    } else if (value != values[i]) {
      fprintf(stderr, "processor %d: at key %llx, expected value %llx, found %llx\n",
	      grt_id, key, values[i], value);
    }
  }

  BARRIER();

  grt_debug_print("done\n");
  hash_map_destroy();

  hash_map_exit(0);
}
