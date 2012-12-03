#include "grt.h"
#include "hashmap.h"
#include <math.h>

#define NEW_LOCKS 0

node_t** table;
#if NEW_LOCKS
grt_new_lock_t **locks;
#else
grt_lock_t **locks;
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

void hash_map_init(int argc, char **argv,
		  unsigned short share_power) {
  entry_table = hs_entry_table;
  table_size = HS_TABLE_SIZE;
  grt_init(argc, argv);
}

void hash_map_create(unsigned size,
		     grt_bool_t pthread) {
  unsigned i, j;
  if (grt_id == 0) {
    if (size < grt_num_procs) {
      fprintf(stderr, "Table size must be >= grt_num_procs!\n");
      abort();
    }
  }
  table_size = size;
  local_size = (size_t) ceil(((float) size) / grt_num_procs);
  table = (node_t**) calloc(local_size, sizeof(node_t*));
#if NEW_LOCKS
  locks = (grt_new_lock_t**) malloc(grt_num_procs*sizeof(grt_new_lock_t*));
  for (i = 0; i < grt_num_procs; ++i)
    locks[i] = (grt_new_lock_t*) grt_new_lock_alloc(i, local_size);
#else
  locks = (grt_lock_t**) malloc(grt_num_procs*sizeof(grt_lock_t*));
  for (i = 0; i < grt_num_procs; ++i)
    locks[i] = (grt_lock_t*) grt_all_lock_alloc(i, local_size);
#endif
  if (pthread == GRT_TRUE) {
    insert_handler_id = INSERT_PTHREAD_HANDLER_ID;
    find_handler_id = FIND_PTHREAD_HANDLER_ID;
  } else {
    insert_handler_id = INSERT_HANDLER_ID;
    find_handler_id = FIND_HANDLER_ID;
  }
}

#if 0
hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  hash.proc = val % grt_num_procs;
  hash.offset = (val / grt_num_procs) % local_size;
  return hash;
}
#else
hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  unsigned global_idx = val % table_size;
  hash.proc = global_idx % grt_num_procs;
  hash.offset = global_idx / grt_num_procs;
  return hash;
}
#endif

static node_t* find_node(grt_word_t key, node_t* p) {
  while (p) {
    if (key == p->key)
      return p;
    p = (node_t*) p->next;
  }
  return 0;
}

/* hash_map_insert implementation */

void hash_map_lock(gasnet_node_t proc, unsigned offset,
		  grt_lock_type_t type, grt_lock_state_t *state) {
  grt_lock(proc, &locks[proc][offset], type, state);
}

void hash_map_unlock(gasnet_node_t proc, unsigned offset) {
  grt_unlock(proc, &locks[proc][offset]);
}

size_t insert_local(void *arg_buf, size_t arg_nbytes,
		    void *result_buf) {
  grt_word_t *my_arg_buf = (grt_word_t*) arg_buf;
  grt_word_t key = my_arg_buf[0];
  grt_word_t val = my_arg_buf[1];
  grt_bool_t result;
  hash_t hash = compute_hash(key);
  node_t **head = &table[hash.offset];
  node_t *p = *head;
  node_t *q = find_node(key, p);
  if (q) {
    q->val = val;
    result = GRT_TRUE;
  } else {
    q = (node_t*) grt_alloc(grt_id, sizeof(node_t));
    q->key = key;
    q->val = val;
    q->next = p;
    *head = q;
    result = GRT_FALSE;
  }
  *((grt_bool_t*) result_buf) = result;
  return sizeof(grt_bool_t);
}

grt_bool_t hash_map_insert(grt_word_t key, grt_word_t val) {
  grt_word_t arg_buf[2];
  arg_buf[0] = key;
  arg_buf[1] = val;
  grt_bool_t result;
  hash_t hash = compute_hash(key);
  grt_on(hash.proc, insert_handler_id, insert_local, 
	 arg_buf, 2*sizeof(grt_word_t), &result, sizeof(grt_bool_t));
  return (grt_bool_t) result;
}

/* hash_map_find implementation */

size_t find_local(void *arg_buf, size_t arg_nbytes,
		  void *result_buf) {
  find_result_t *result = (find_result_t*) result_buf;
  grt_word_t key = *((grt_word_t*) arg_buf);
  hash_t hash = compute_hash(key);
  node_t *p = table[hash.offset];
  p = find_node(key, p);
  if (p) {
    result->found = GRT_TRUE;
    result->val = p->val;
  } else {
    result->found = GRT_FALSE;
  }
  return sizeof(find_result_t);
}

grt_bool_t hash_map_find(grt_word_t key, grt_word_t *val) {
  find_result_t result;
  hash_t hash = compute_hash(key);
  grt_on(hash.proc, find_handler_id, find_local, 
	 &key, sizeof(grt_word_t), &result, sizeof(find_result_t));
  if (val && (result.found == GRT_TRUE)) *val = result.val;
  return result.found;
}

void hash_map_destroy() {
  free(table);
  grt_lock_free(grt_id, locks[grt_id], local_size);
  free(locks);
}

int __hash_map_exit(int ret_val) {
  return grt_exit(ret_val);
}

void lock(grt_word_t key1, grt_word_t key2,
	  grt_lock_state_t *state1,
	  grt_lock_state_t *state2) {
  hash_t hash1 = compute_hash(key1), 
    hash2 = compute_hash(key2);
  if (hash1.proc == hash2.proc &&
      hash1.offset == hash2.offset) {
    hash_map_lock(hash1.proc, hash1.offset, WRITE, state1);
  } else if (key1 < key2) {
    hash_map_lock(hash1.proc, hash1.offset, WRITE, state1);
    hash_map_lock(hash2.proc, hash2.offset, WRITE, state2);
  } else {
    hash_map_lock(hash2.proc, hash2.offset, WRITE, state2);
    hash_map_lock(hash1.proc, hash1.offset, WRITE, state1);
  }
}

void unlock(grt_word_t key1, grt_word_t key2) {
  hash_t hash1 = compute_hash(key1);
  hash_t hash2 = compute_hash(key2);
  if (hash1.proc == hash2.proc &&
      hash1.offset == hash2.offset) {
    hash_map_unlock(hash1.proc, hash1.offset);
  } else {
    hash_map_unlock(hash1.proc, hash1.offset);
    hash_map_unlock(hash2.proc, hash2.offset);
  }
}

