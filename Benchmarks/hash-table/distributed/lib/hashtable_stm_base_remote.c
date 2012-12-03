#include "stm.h"
#include "grt_debug.h"
#include "hashtable.h"

node_t** table;
unsigned table_size;
unsigned local_size;

/* Because the stm_base implementation isn't thread safe, to make this
   work we have to wrap each transaction in a gasnet_hsl.  This is a
   hack and won't give good scalable performance.  The right solution
   is to make stm_base thread safe by moving the global pointer
   variables to a struct that's passed around by pointer. */

gasnet_hsl_t transaction_lock = GASNET_HSL_INITIALIZER;

/* AM Handler Table */

#define FIRST_HT_ID (LAST_STM_ID + 1)
#define HT_ID(x) (FIRST_HT_ID + x)
#define INSERT_HANDLER_ID             HT_ID(0)
#define FIND_HANDLER_ID               HT_ID(1)
#define LAST_HT_ID FIND_HANDLER_ID
#define HT_TABLE_SIZE (STM_TABLE_SIZE + (LAST_HT_ID - FIRST_HT_ID) + 1)

#define HT_TABLE_SEGMENT						\
  { INSERT_HANDLER_ID, insert_handler },				\
  { FIND_HANDLER_ID, find_handler }

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr),
			   gasnet_handlerarg_t proc);
static void find_handler(gasnet_token_t token,
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
  stm_init(argc, argv);
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
  table = (node_t**) stm_alloc(grt_id, local_size * sizeof(node_t*));
}

hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  hash.proc = val % grt_num_procs;
  hash.offset = (val / grt_num_procs) % local_size;
  return hash;
}

static node_t* find_node(gasnet_node_t proc, grt_word_t val, 
			 node_t* p) {
  while(p) {
    grt_word_t* loc = (grt_word_t*) &p->val;
    grt_word_t tmp;
    stm_open_for_read(proc, loc);
    stm_read(proc, loc, &tmp);
    if (val == tmp) {
      return p;
    }
    loc = (grt_word_t*) &p->next;
    stm_open_for_read(proc, loc);
    stm_read(proc, loc, (grt_word_t*) &p);
  }
  return 0;
}

/* hashtable_insert implementation */

static grt_bool_t insert_local(grt_word_t value,
			       gasnet_node_t proc) {
  grt_bool_t result;
  hash_t hash = compute_hash(value);
  gasnet_hsl_lock(&transaction_lock);
  stm_start();
  node_t **head = &table[hash.offset];
  stm_open_for_read(hash.proc, (grt_word_t*) head);
  node_t *p, *q;
  stm_read(hash.proc, (grt_word_t*) head, (grt_word_t*) &p);
  if (find_node(hash.proc, value, p)) {
    result = GRT_TRUE;
  } else {
    q = (node_t*) stm_alloc(hash.proc, sizeof(node_t));
    stm_open_for_write(hash.proc, (grt_word_t*) &q->next);
    stm_write(hash.proc, (grt_word_t) p, (grt_word_t*) &q->next);
    stm_open_for_write(hash.proc, (grt_word_t*) &q->val);
    stm_write(hash.proc, value, &q->val);
    stm_open_for_write(hash.proc, (grt_word_t*) head);
    stm_write(hash.proc, (grt_word_t) q, (grt_word_t*) head);
    result = GRT_FALSE;
  }
  stm_commit();
  gasnet_hsl_unlock(&transaction_lock);
  return result;
}

static gasnet_hsl_t alloc_lock = GASNET_HSL_INITIALIZER;

typedef struct {
  gasnet_node_t proc;
  grt_handler_state_t *state;
  grt_word_t value;
  grt_word_t *result_ptr;
} handler_thread_arg_t;

void *insert_handler_thread(void *arg) {
  handler_thread_arg_t *thread_arg = (handler_thread_arg_t*) arg;
  grt_word_t result = insert_local(thread_arg->value, 
				   thread_arg->proc);
  grt_write(thread_arg->proc, result, thread_arg->result_ptr);
  grt_write(thread_arg->proc, DONE, (grt_word_t*) thread_arg->state);
  gasnet_hsl_lock(&alloc_lock);
  free(arg);
  gasnet_hsl_unlock(&alloc_lock);
  return 0;
}

static void insert_handler(gasnet_token_t token,
			   GRT_H_PARAM(value),
			   GRT_H_PARAM(state_ptr),
			   GRT_H_PARAM(result_ptr),
			   gasnet_handlerarg_t proc) {
  grt_word_t result = GRT_FALSE;
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  gasnet_hsl_lock(&alloc_lock);
  handler_thread_arg_t *arg = 
    (handler_thread_arg_t*) malloc(sizeof(handler_thread_arg_t));
  gasnet_hsl_unlock(&alloc_lock);
  arg->proc = proc;
  arg->state = GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr);
  arg->value = value;
  arg->result_ptr = GRT_H_PARAM_TO_WORD(grt_word_t*, result_ptr);
  pthread_t thread;
  pthread_create(&thread, NULL, insert_handler_thread, arg);
}

grt_bool_t hashtable_insert(grt_word_t val) {
  grt_handler_state_t state = PENDING;
  grt_word_t result;
  hash_t hash = compute_hash(val);
  if (hash.proc == grt_id) {
    result = insert_local(val, hash.proc);
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

static grt_bool_t find_local(grt_word_t value) {
  hash_t hash = compute_hash(value);
  gasnet_hsl_lock(&transaction_lock);
  stm_start();
  stm_open_for_read(grt_id, (grt_word_t*) &table[hash.offset]);
  node_t *p;
  stm_read(grt_id, (grt_word_t*) &table[hash.offset], (grt_word_t*) &p);
  grt_word_t result = 
    find_node(grt_id, value, p) == 0 ? GRT_FALSE : GRT_TRUE;
  stm_commit();
  gasnet_hsl_unlock(&transaction_lock);
  return result;
}

static void find_handler(gasnet_token_t token,
			 GRT_H_PARAM(value),
			 GRT_H_PARAM(state_ptr),
			 GRT_H_PARAM(result_ptr)) {
  grt_word_t value = GRT_H_PARAM_TO_WORD(grt_word_t, value);
  grt_word_t result = find_local(value);
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
  stm_free(grt_id, table);
}

int __hashtable_exit(int ret_val) {
  return grt_exit(ret_val);
}

