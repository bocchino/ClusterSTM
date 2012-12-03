#include "grt.h"
#include "hashset.h"
#include <math.h>
#include <assert.h>

#define NEW_LOCKS 0
/* For diagnostic purposes, take out any actual work and just run the
   handlers.  Set this to 0 if you want to use the benchmark for any
   work timing! */
#define EMPTY_WORK 0
#if EMPTY_WORK
#warning EMPTY_WORK=1:  Use this build for diagnostic purposes only!
#endif

node_t** table;
#if NEW_LOCKS
grt_new_lock_t *locks;
#else
grt_lock_t *locks;
#endif
unsigned table_size;
unsigned local_size;
unsigned insert_handler_id;
unsigned find_handler_id;

/* AM Handler Table */

#define FIRST_HS_ID (LAST_GRT_ID + 1)
#define HS_ID(x) (FIRST_HS_ID + x)
#define INSERT_HANDLER_ID  HS_ID(0)
#define INSERT_PTHREAD_HANDLER_ID HS_ID(1)
#define FIND_HANDLER_ID HS_ID(2)
#define FIND_PTHREAD_HANDLER_ID HS_ID(3)
#define LAST_HS_ID FIND_PTHREAD_HANDLER_ID
#define HS_TABLE_SIZE (GRT_TABLE_SIZE + (LAST_HS_ID - FIRST_HS_ID) + 1)

GRT_ON_LOCAL_FN_DECL(insert);
GRT_ON_HANDLER_DEF(insert);
GRT_ON_LOCAL_FN_DECL(find);
GRT_ON_HANDLER_DEF(find);

#define HS_TABLE_SEGMENT						\
  { INSERT_HANDLER_ID, insert_handler }, \
  { INSERT_PTHREAD_HANDLER_ID, insert_pthread_handler }, \
  { FIND_HANDLER_ID, find_handler }, \
  { FIND_PTHREAD_HANDLER_ID, find_pthread_handler }

static gasnet_handlerentry_t hs_entry_table[HS_TABLE_SIZE] = {
  GRT_TABLE,
  HS_TABLE_SEGMENT
};

void hashset_init(int argc, char **argv,
		  unsigned short share_power) {
  entry_table = hs_entry_table;
  table_size = HS_TABLE_SIZE;
  grt_init(argc, argv);
}

void hashset_create(unsigned size,
		    grt_bool_t pthread) {
  unsigned i, j;
  if (grt_id == 0) {
#if EMPTY_WORK
    grt_debug_print("WARNING:  EMPTY_WORK=1; no actual work being done!\n");
#endif
    if (size < grt_num_procs) {
      fprintf(stderr, "Table size must be >= grt_num_procs!\n");
      abort();
    }
  }
  table_size = size;
  local_size = (size_t) ceil(((float) size) / grt_num_procs);
  table = (node_t**) calloc(local_size, sizeof(node_t*));
#if NEW_LOCKS
  locks = (grt_new_lock_t*) grt_new_lock_alloc(grt_id, local_size);
#else
  locks = (grt_lock_t*) grt_lock_alloc(grt_id, local_size);
#endif
  if (pthread == GRT_TRUE) {
    insert_handler_id = INSERT_PTHREAD_HANDLER_ID;
    find_handler_id = FIND_PTHREAD_HANDLER_ID;
  } else {
    insert_handler_id = INSERT_HANDLER_ID;
    find_handler_id = FIND_HANDLER_ID;
  }
}

hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  unsigned global_idx = val % table_size;
  hash.proc = global_idx % grt_num_procs;
  hash.offset = global_idx / grt_num_procs;
  return hash;
}

static node_t* find_node(grt_word_t val, node_t* p) {
  while (p) {
    if (val == p->val)
      return p;
    p = (node_t*) p->next;
  }
  return 0;
}

/* hashset_insert implementation */

size_t insert_local(void *arg_buf, size_t arg_nbytes,
		    void *result_buf) {
  grt_bool_t result = GRT_FALSE;
#if !EMPTY_WORK
  grt_word_t value = *((grt_word_t*) arg_buf);
  grt_lock_state_t state;
  hash_t hash = compute_hash(value);
#ifndef NOLOCKS
#if NEW_LOCKS
  grt_new_lock(grt_id, &locks[hash.offset], WRITE);
#else
  grt_lock(grt_id, &locks[hash.offset], WRITE, &state);
#endif
#endif
  node_t **head = &table[hash.offset];
  node_t *p = *head;
  node_t *q = find_node(value, p);
  if (q) {
    result = GRT_TRUE;
  } else {
    q = (node_t*) grt_alloc(grt_id, sizeof(node_t));
    q->next = p;
    q->val = value;
    *head = q;
    result = GRT_FALSE;
  }
#ifndef NOLOCKS
#if NEW_LOCKS
  grt_new_unlock(grt_id, &locks[hash.offset]);
#else
  grt_unlock(grt_id, &locks[hash.offset]);
#endif
#endif
#endif
  *((grt_bool_t*) result_buf) = result;
  return sizeof(grt_bool_t);
}

grt_bool_t hashset_insert(grt_word_t val) {
  grt_bool_t result;
  hash_t hash = compute_hash(val);
  grt_on(hash.proc, insert_handler_id, insert_local, 
	 &val, sizeof(grt_word_t), &result, sizeof(grt_bool_t));
  return (grt_bool_t) result;
}

/* hashset_find implementation */

size_t find_local(void *arg_buf, size_t arg_nbytes,
		  void *result_buf) {
  grt_word_t result = GRT_FALSE;
#if !EMPTY_WORK
  grt_word_t value = *((grt_word_t*) arg_buf);
  grt_lock_state_t state;
  hash_t hash = compute_hash(value);
#ifndef NOLOCKS
#if NEW_LOCKS
  grt_new_lock(grt_id, &locks[hash.offset], READ);
#else
  grt_lock(grt_id, &locks[hash.offset], READ, &state);
#endif
#endif
  node_t *p = table[hash.offset];
  result = find_node(value, p) == 0 ? GRT_FALSE : GRT_TRUE;
#ifndef NOLOCKS
#if NEW_LOCKS
  grt_new_unlock(grt_id, &locks[hash.offset]);
#else
  grt_unlock(grt_id, &locks[hash.offset]);
#endif
#endif
#endif
  *((grt_bool_t*) result_buf) = result;
  return sizeof(grt_bool_t);
}

grt_bool_t hashset_find(grt_word_t val) {
  grt_bool_t result;
  hash_t hash = compute_hash(val);
  grt_on(hash.proc, find_handler_id, find_local, 
	 &val, sizeof(grt_word_t), &result, sizeof(grt_bool_t));
  return (grt_bool_t) result;
}

void hashset_destroy() {
  free(table);
#if NEW_LOCKS
  grt_new_lock_free(grt_id, locks, local_size);
#else
  grt_lock_free(grt_id, locks, local_size);
#endif
}

int __hashset_exit(int ret_val) {
  return grt_exit(ret_val);
}

