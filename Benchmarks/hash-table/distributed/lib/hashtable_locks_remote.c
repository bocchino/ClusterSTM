#include "grt.h"
#include "grt_debug.h"
#include "hashtable.h"

node_t** table;
grt_lock_t *locks;
unsigned table_size;
unsigned local_size;

/* AM Handler Table */

#define FIRST_HT_ID (LAST_GRT_ID + 1)
#define HT_ID(x) (FIRST_HT_ID + x)
#define INSERT_HANDLER_ID             HT_ID(0)
#define FIND_HANDLER_ID               HT_ID(1)
#define LAST_HT_ID FIND_HANDLER_ID
#define HT_TABLE_SIZE (GRT_TABLE_SIZE + (LAST_HT_ID - FIRST_HT_ID) + 1)

#define HT_TABLE_SEGMENT						\
  { INSERT_HANDLER_ID, insert_handler },				\
  { FIND_HANDLER_ID, insert_handler }

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr));
static void find_handler(gasnet_token_t token,
			 GRT_H_PARAM(value),
			 GRT_H_PARAM(state_ptr),
			 GRT_H_PARAM(result_ptr));

static gasnet_handlerentry_t ht_entry_table[HT_TABLE_SIZE] = {
  GRT_TABLE,
  HT_TABLE_SEGMENT
};

void hashtable_init(int argc, char **argv) {
  entry_table = ht_entry_table;
  table_size = HT_TABLE_SIZE;
  grt_init(argc, argv);
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
  table = (node_t**) calloc(local_size, sizeof(node_t*));
  locks = (grt_lock_t*) grt_lock_alloc(grt_id, local_size);
}

hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  hash.proc = val % grt_num_procs;
  hash.offset = (val / grt_num_procs) % local_size;
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

/* hashtable_insert implementation */

static grt_bool_t insert_local(grt_word_t value) {
  grt_bool_t result;
  grt_lock_state_t state;
  hash_t hash = compute_hash(value);
  grt_lock(grt_id, &locks[hash.offset], WRITE, &state);
  node_t **head = &table[hash.offset];
  node_t *p = *head;
  node_t *q = find_node(value, p);
  if (q) {
    result = GRT_TRUE;
  } else {
    q = (node_t*) grt_alloc(grt_id, sizeof(node_t));//malloc(sizeof(node_t));
    q->next = p;
    q->val = value;
    *head = q;
    result = GRT_FALSE;
  }
  grt_unlock(grt_id, &locks[hash.offset]);
  return result;
}

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr)) {
  grt_word_t result = GRT_FALSE;
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  result = insert_local(value);
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
    result = insert_local(val);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(hash.proc, INSERT_HANDLER_ID,
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return (grt_bool_t) result;
}

/* hashtable_find implementation */

static grt_bool_t find_local(grt_word_t value) {
  grt_lock_state_t state;
  hash_t hash = compute_hash(value);
  grt_lock(grt_id, &locks[hash.offset], READ, &state);
  node_t *p = table[hash.offset];
  grt_word_t result = find_node(value, p) == 0 ? GRT_FALSE : GRT_TRUE;
  grt_unlock(grt_id, &locks[hash.offset]);
  return result;
}

static void find_handler(gasnet_token_t token,
			 GRT_H_PARAM(value),
			 GRT_H_PARAM(state_ptr),
			 GRT_H_PARAM(result_ptr)) {
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  grt_word_t result = find_local(value);
  grt_handler_state_t state = PENDING;
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
    return find_local(val);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(hash.proc, FIND_HANDLER_ID,
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return (grt_bool_t) result;
}

void hashtable_destroy() {
  free(table);
  grt_lock_free(grt_id, locks, local_size);
}

int __hashtable_exit(int ret_val) {
  return grt_exit(ret_val);
}

