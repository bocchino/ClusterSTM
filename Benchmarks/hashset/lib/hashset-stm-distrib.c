#include "assert.h"
#include "stm-distrib.h"
#include "hashset.h"
#include <math.h>

node_t** table;
unsigned table_size;
unsigned local_size;
unsigned insert_handler_id;
unsigned find_handler_id;

static size_t insert_local(gasnet_node_t src_proc,
			   void *arg_buf, size_t arg_nbytes,
			   void *result_buf);
static size_t find_local(gasnet_node_t src_proc,
			 void *arg_buf, size_t arg_nbytes,
			 void *result_buf);

/* AM Handler Table */

#define FIRST_HT_ID (LAST_STM_ID + 1)
#define HT_ID(x) (FIRST_HT_ID + x)
#define INSERT_HANDLER_ID             HT_ID(0)
#define INSERT_PTHREAD_HANDLER_ID     HT_ID(1)
#define FIND_HANDLER_ID               HT_ID(2)
#define FIND_PTHREAD_HANDLER_ID       HT_ID(3)
#define LAST_HT_ID FIND_PTHREAD_HANDLER_ID
#define HT_TABLE_SIZE (STM_TABLE_SIZE + (LAST_HT_ID - FIRST_HT_ID) + 1)

STM_ON_HANDLER_DEF(insert);
STM_ON_HANDLER_DEF(find);

#define HT_TABLE_SEGMENT						\
  { INSERT_HANDLER_ID, insert_handler },				\
  { INSERT_PTHREAD_HANDLER_ID, insert_pthread_handler },		\
  { FIND_HANDLER_ID, find_handler },					\
  { FIND_PTHREAD_HANDLER_ID, find_pthread_handler }

static gasnet_handlerentry_t ht_entry_table[HT_TABLE_SIZE] = {
  GRT_TABLE,
  STM_TABLE_SEGMENT,
  HT_TABLE_SEGMENT
};

void hashset_init(int argc, char **argv,
		  unsigned short share_power) {
  entry_table = ht_entry_table;
  table_size = HT_TABLE_SIZE;
  stm_init(argc, argv, share_power);
}

void hashset_create(unsigned size, grt_bool_t pthread) {
  unsigned i, j;
  if (grt_id == 0) {
    if (size < grt_num_procs) {
      fprintf(stderr, "Table size must be >= grt_num_procs!\n");
      abort();
   }
  }
  table_size = size;
  local_size = (size_t) ceil(((float) size) / grt_num_procs);
  table = (node_t**) stm_alloc(grt_id, grt_id, 
			       local_size * sizeof(node_t*));
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

static node_t* find_node(gasnet_node_t src_proc, grt_word_t val, 
			 node_t* p_arg) {
  node_t *p = p_arg;
  while(p) {
    grt_word_t* loc = (grt_word_t*) &p->val;
    grt_word_t tmp;
    stm_get(src_proc, &tmp, grt_id, loc, sizeof(grt_word_t));
    if (val == tmp) {
      return p;
    }
    loc = (grt_word_t*) &p->next;
    stm_get(src_proc, &p, grt_id, loc, sizeof(void*));
  }
  return 0;
}

/* hashset_insert implementation */

static size_t insert_local(gasnet_node_t src_proc,
			   void *arg_buf, size_t arg_nbytes,
			   void *result_buf) {
  grt_bool_t result;
  node_t **head, *p, *q;
  grt_word_t value = *((grt_word_t*) arg_buf);
  hash_t hash = compute_hash(value);
  stm_start(src_proc);
  head = &table[hash.offset];
  stm_get(src_proc, &p, grt_id, head, sizeof(void*));
  if (find_node(src_proc, value, p)) {
    result = GRT_TRUE;
  } else {
    q = (node_t*) stm_alloc(src_proc, grt_id, sizeof(node_t));
    stm_put(src_proc, grt_id, &q->next, &p, sizeof(void*));
    stm_put(src_proc, grt_id, &q->val, &value, sizeof(grt_word_t));
    stm_put(src_proc, grt_id, head, &q, sizeof(node_t*));
    result = GRT_FALSE;
  }
  stm_commit(src_proc);
  *((grt_bool_t*) result_buf) = result;
  return sizeof(grt_bool_t);
}

grt_bool_t hashset_insert(grt_word_t val) {
  grt_bool_t result;
  hash_t hash = compute_hash(val);
  stm_on(grt_id, hash.proc, insert_handler_id, insert_local, 
	 &val, sizeof(grt_word_t), &result, sizeof(grt_bool_t));
  return (grt_bool_t) result;
}

/* hashset_find implementation */

static size_t find_local(gasnet_node_t src_proc,
			 void *arg_buf, size_t arg_nbytes,
			 void *result_buf) {
  grt_word_t value = *((grt_word_t*) arg_buf);
  hash_t hash = compute_hash(value);
  node_t *p;
  grt_word_t result;
  stm_start(src_proc);
  stm_get(src_proc, &p, grt_id, &table[hash.offset], sizeof(void*));
  result = 
    find_node(src_proc, value, p) == 0 ? GRT_FALSE : GRT_TRUE;
  stm_commit(src_proc);
  *((grt_bool_t*) result_buf) = result;
  return sizeof(grt_bool_t);
}

grt_bool_t hashset_find(grt_word_t val) {
  grt_bool_t result;
  hash_t hash = compute_hash(val);
  stm_on(grt_id, hash.proc, find_handler_id, find_local, 
	 &val, sizeof(grt_word_t), &result, sizeof(grt_bool_t));
  return (grt_bool_t) result;
}

void free_buckets(node_t *p) {
  while(p) {
    node_t *next = p->next;
    stm_free(grt_id, p);
    p = next;
  }
}

void hashset_destroy() {
  unsigned i;
#if 0
  for (i = 0; i < local_size; ++i) {
    free_buckets(table[i]);
  }
#endif
  stm_free(grt_id, table);
}

int __hashset_exit(int ret_val) {
  return stm_exit(ret_val);
}

