#include "assert.h"
#include "stm-distrib.h"
#include "grt_debug.h"
#include "hashtable.h"

node_t** table;
unsigned table_size;
unsigned local_size;

/* AM Handler Table */

#define FIRST_HT_ID (LAST_STM_ID + 1)
#define HT_ID(x) (FIRST_HT_ID + x)
#define INSERT_HANDLER_ID             HT_ID(0)
#define FIND_HANDLER_ID               HT_ID(1)
#define LAST_HT_ID FIND_HANDLER_ID
#define HT_TABLE_SIZE (STM_TABLE_SIZE + (LAST_HT_ID - FIRST_HT_ID) + 1)

#define HT_SHARE_SIZE 4

#define HT_TABLE_SEGMENT						\
  { INSERT_HANDLER_ID, insert_handler },				\
  { FIND_HANDLER_ID, find_handler }

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr),
			   gasnet_handlerarg_t proc);
static void find_handler(gasnet_token_t token,
			 gasnet_handlerarg_t src_proc,
			 GRT_H_PARAM(value),
			 GRT_H_PARAM(state_ptr),
			 GRT_H_PARAM(result_ptr));

static gasnet_handlerentry_t ht_entry_table[HT_TABLE_SIZE] = {
  GRT_TABLE,
  STM_TABLE_SEGMENT,
  HT_TABLE_SEGMENT
};

void hashtable_init(int argc, char **argv) {
  entry_table = ht_entry_table;
  table_size = HT_TABLE_SIZE;
  stm_init(argc, argv, HT_SHARE_SIZE);
}

void hashtable_create(unsigned size) {
  unsigned i, j;
  if (grt_id == 0) {
    if (size < grt_num_procs) {
      fprintf(stderr, "Table size must be >= grt_num_procs!\n");
      abort();
   }
  }
  table_size = size;
  local_size = size / grt_num_procs;
  table = (node_t**) stm_alloc(grt_id, grt_id, local_size * sizeof(node_t*));
}

hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  hash.proc = val % grt_num_procs;
  hash.offset = (val / grt_num_procs) % local_size;
  return hash;
}

static node_t* find_node(gasnet_node_t src_proc, grt_word_t val, 
			 node_t* p_arg) {
  node_t *p = p_arg;
  while(p) {
    grt_word_t* loc = (grt_word_t*) &p->val;
    grt_word_t tmp;
    assert((grt_word_t) loc >= (grt_word_t) grt_heap_base);
    stm_get(src_proc, &tmp, grt_id, loc, sizeof(grt_word_t));
    if (val == tmp) {
      return p;
    }
    loc = (grt_word_t*) &p->next;
    assert((grt_word_t) loc >= (grt_word_t) grt_heap_base);
    stm_get(src_proc, &p, grt_id, loc, sizeof(void*));
  }
  return 0;
}

/* hashtable_insert implementation */

static grt_bool_t insert_local(gasnet_node_t src_proc,
			       grt_word_t value) {
  grt_bool_t result;
  node_t **head, *p, *q;
  hash_t hash = compute_hash(value);
  grt_word_t my_value = value;
  stm_start(src_proc);
  head = &table[hash.offset];
  stm_get(src_proc, &p, grt_id, head, sizeof(void*));
  if (p && ((grt_word_t) p < (grt_word_t) grt_heap_base)) {
    grt_debug_print("read %x from %x\n", p, head);
    exit(0);
  }
  if (find_node(src_proc, value, p)) {
    result = GRT_TRUE;
  } else {
    q = (node_t*) stm_alloc(src_proc, grt_id, sizeof(node_t));
    stm_put(src_proc, grt_id, &q->next, &p, sizeof(void*));
    stm_put(src_proc, grt_id, &q->val, &my_value, sizeof(grt_word_t));
    stm_put(src_proc, grt_id, head, &q, sizeof(node_t*));
    result = GRT_FALSE;
  }
  stm_commit(src_proc);
  return result;
}

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr),
			   gasnet_handlerarg_t src_proc) {
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  grt_word_t result = insert_local(src_proc, value);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(state_ptr),
				    GRT_WORD_TO_H_ARG(result),
				    GRT_H_ARG(result_ptr)));
}

grt_bool_t hashtable_insert(grt_word_t val) {
  grt_handler_state_t state = PENDING;
  grt_word_t result;
  hash_t hash = compute_hash(val);
  if (hash.proc == grt_id) {
   result = insert_local(grt_id, val);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(hash.proc, INSERT_HANDLER_ID,
					GRT_NUM_H_ARGS(3)+1,
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result),
					grt_id));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return (grt_bool_t) result;
}

/* hashtable_find implementation */

static grt_bool_t find_local(gasnet_node_t src_proc,
			     grt_word_t value) {
  hash_t hash = compute_hash(value);
  node_t *p;
  grt_word_t result;
  stm_start(src_proc);
  stm_get(src_proc, &p, grt_id, &table[hash.offset], sizeof(void*));
  result = 
    find_node(src_proc, value, p) == 0 ? GRT_FALSE : GRT_TRUE;
  stm_commit(src_proc);
  return result;
}

static void find_handler(gasnet_token_t token,
			 gasnet_handlerarg_t src_proc,
			 GRT_H_PARAM(value),
			 GRT_H_PARAM(state_ptr),
			 GRT_H_PARAM(result_ptr)) {
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  grt_word_t result = find_local(src_proc, value);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(state_ptr),
				    GRT_WORD_TO_H_ARG(result),
				    GRT_H_ARG(result_ptr)));
}

grt_bool_t hashtable_find(grt_word_t val) {
  grt_handler_state_t state = PENDING;
  grt_word_t result;
  hash_t hash = compute_hash(val);
  if (hash.proc == grt_id) {
    return find_local(grt_id, val);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(hash.proc, FIND_HANDLER_ID,
					GRT_NUM_H_ARGS(3)+1,
					grt_id,
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return (grt_bool_t) result;
}

void hashtable_destroy() {
  stm_free(grt_id, table);
}

int __hashtable_exit(int ret_val) {
  return grt_exit(ret_val);
}

