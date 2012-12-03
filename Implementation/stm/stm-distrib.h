/*****************************************************************
  Distributed STM Interface
******************************************************************/

#ifndef STM_DISTRIB_H
#define STM_DISTRIB_H

#include <mba/hashmap.h>
#include <mba/linkedlist.h>

#include <setjmp.h>
#include "grt.h"

#ifdef STM_DEBUG
#define STM_DEBUG_PRINT(...) grt_debug_print(__VA_ARGS__)
#else
#define STM_DEBUG_PRINT(...)
#endif

#ifdef LATE_ACQUIRE
#ifndef WRITE_BUFFERING
#error Late acquire needs write buffering!
#endif
#ifdef READ_VERSIONING
#error Read versioning does not yet work with late acquire!
#endif
#endif


/* The STM tracks reads and writes in aligned groups of words of size
   2^STM_SHARE_POWER */

#define STM_SHARE_POWER stm_share_power
#define STM_SHARE_SIZE (1 << STM_SHARE_POWER)
#define STM_SHARE_SIZE_BYTES (STM_SHARE_SIZE * sizeof(grt_word_t))

/* Define this to 1 to turn on stats */
#define STATS 1

/* AM Handler Table */

#define FIRST_STM_ID (LAST_GRT_ID + 1)
#define STM_ID(x) (FIRST_STM_ID + x)

#define STM_ON_REPLY_HANDLER_ID        STM_ID(0)
#define STM_ALLOC_HANDLER_ID           STM_ID(1)
#define STM_PUT_HANDLER_ID             STM_ID(2)
#define STM_FINISH_HANDLER_ID          STM_ID(3)
#define STM_GET_REPLY_HANDLER_ID       STM_ID(4)
#define STM_GET_HANDLER_ID             STM_ID(5)
#define STM_ACQUIRE_HANDLER_ID         STM_ID(6)
#ifdef READ_VERSIONING
#define STM_VALIDATE_HANDLER_ID        STM_ID(7)
#define LAST_STM_ID STM_VALIDATE_HANDLER_ID
#else
#define LAST_STM_ID STM_ACQUIRE_HANDLER_ID
#endif

void stm_alloc_handler(gasnet_token_t token, 
		       gasnet_handlerarg_t src_proc, 
		       gasnet_handlerarg_t tx_depth,
		       gasnet_handlerarg_t nbytes,
		       GRT_H_PARAM(result), GRT_H_PARAM(state));
void stm_get_handler(gasnet_token_t token, gasnet_handlerarg_t src_proc, 
			 GRT_H_PARAM(dest), GRT_H_PARAM(src),
			 gasnet_handlerarg_t nbytes,
			 GRT_H_PARAM(status), GRT_H_PARAM(state));
void stm_get_reply_handler(gasnet_token_t token, void *buf,
			   gasnet_handlerarg_t nbytes, GRT_H_PARAM(dest),
			   GRT_H_PARAM(status), GRT_H_PARAM(state));
void stm_put_handler(gasnet_token_t token, 
		     void *src, size_t nbytes,
		     gasnet_handlerarg_t src_proc, 
		     GRT_H_PARAM(dest),
		     GRT_H_PARAM(status_ptr), GRT_H_PARAM(state));
void stm_on_reply_handler(gasnet_token_t token, void *buf, 
			  size_t nbytes,
			  gasnet_handlerarg_t status,
			  GRT_H_PARAM(result),
			  GRT_H_PARAM(result_size_ptr),
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr));
void stm_finish_handler(gasnet_token_t token, 
			gasnet_handlerarg_t src_proc,
			gasnet_handlerarg_t status,
			GRT_H_PARAM(state));
#ifdef READ_VERSIONING
void stm_validate_handler(gasnet_token_t token,
			  gasnet_handlerarg_t src_proc,
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr));
#endif
void stm_acquire_handler(gasnet_token_t token,
			  gasnet_handlerarg_t src_proc,
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr));

#ifdef READ_VERSIONING
#define STM_TABLE_SEGMENT				\
  { STM_ON_REPLY_HANDLER_ID, stm_on_reply_handler },	\
  { STM_ALLOC_HANDLER_ID, stm_alloc_handler},		\
  { STM_PUT_HANDLER_ID, stm_put_handler},		\
  { STM_FINISH_HANDLER_ID, stm_finish_handler },	\
  { STM_GET_REPLY_HANDLER_ID, stm_get_reply_handler },	\
  { STM_GET_HANDLER_ID, stm_get_handler },		\
    { STM_ACQUIRE_HANDLER_ID, stm_acquire_handler },	\
  { STM_VALIDATE_HANDLER_ID, stm_validate_handler }
#else
#define STM_TABLE_SEGMENT				\
  { STM_ON_REPLY_HANDLER_ID, stm_on_reply_handler },	\
  { STM_ALLOC_HANDLER_ID, stm_alloc_handler},		\
  { STM_PUT_HANDLER_ID, stm_put_handler},		\
  { STM_FINISH_HANDLER_ID, stm_finish_handler },	\
  { STM_GET_REPLY_HANDLER_ID, stm_get_reply_handler },	\
  { STM_GET_HANDLER_ID, stm_get_handler },		\
    { STM_ACQUIRE_HANDLER_ID, stm_acquire_handler }
#endif
#define STM_TABLE_SIZE (GRT_TABLE_SIZE + (LAST_STM_ID - FIRST_STM_ID) + 1)

/* Transaction descriptor */
typedef struct {
  /* Source processor: the processor on behalf of which we are
     recording info in this descriptor. In the array of descriptors,
     tx_dors[x].src_proc = x.  Thus this field is redundant with the
     array index; we maintain this field only so that we can recover
     the source processor given just the descriptor. */
  gasnet_node_t src_proc;
  /* TX processor: the processor on which the currently open
     transaction (if any) was started.  If tx_depth == 0, this value
     is meaningless. */
  gasnet_node_t tx_proc;
  /* TX nesting depth */
  unsigned short tx_depth;
  /* For longjmp at transaction abort */
  jmp_buf env;
  /* Set of data touched on this processor */
  struct hashmap dataset;
  /* Set of remote processors touched */
  struct hashmap remote_procs;
  /* List of tx allocations made */
  struct linkedlist allocations;
  /* How many times we've unsuccessfully retried */
  unsigned retry_count;
  /* Amount of time to back off on abort */
  unsigned backoff_time;
} tx_dor_t;

extern tx_dor_t *tx_dors;
tx_dor_t *get_tx_dor(gasnet_node_t src_proc);

#ifdef STATS
extern size_t commit_count, abort_count;
extern size_t *commit_counts, *abort_counts;
#endif

/* Handler definition template for stm_on */

#define STM_ON_HANDLER_DECL(handler_name)				\
  void handler_name##_handler(gasnet_token_t token, void *buf,		\
			      size_t nbytes,				\
			      gasnet_handlerarg_t src_proc,		\
			      gasnet_handlerarg_t tx_proc,		\
			      gasnet_handlerarg_t tx_depth,		\
			      GRT_H_PARAM(result_buf),			\
			      gasnet_handlerarg_t result_buf_size,	\
			      GRT_H_PARAM(result_size_ptr),		\
			      GRT_H_PARAM(status_ptr),			\
			      GRT_H_PARAM(state_ptr))

#define STM_ON_HANDLER_DEF(handler_name) \
  STM_ON_HANDLER_DECL(handler_name) {	 \
    stm_on_handler_work(token, buf, nbytes, src_proc, tx_proc, tx_depth, \
			GRT_H_PARAM_TO_WORD(stm_on_local_arg_t*,	\
					    result_buf),		\
			result_buf_size,				\
			GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr),	\
			GRT_H_PARAM_TO_WORD(grt_status_t*, status_ptr),	\
			GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr), \
			handler_name##_local);				\
  }									\
  STM_ON_HANDLER_DECL(handler_name##_pthread) {	 \
    stm_on_handler_work_pthread(token, buf, nbytes, src_proc, tx_proc,	\
				tx_depth,				\
				GRT_H_PARAM_TO_WORD(stm_on_local_arg_t*, \
						    result_buf),	\
				result_buf_size,			\
				GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr), \
				GRT_H_PARAM_TO_WORD(grt_status_t*, status_ptr),	\
				GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr), \
				handler_name##_local);			\
  }
    
/* Form of handler function for stm_on.  This function calls the local
   function, either directly or after spawning off a pthread. */

typedef void (*stm_on_handler_fn_t) (gasnet_token_t token, void* buf, 
				     size_t nbytes, 
				     gasnet_handlerarg_t src_proc, 
				     gasnet_handlerarg_t tx_proc,
				     gasnet_handlerarg_t tx_depth,
				     GRT_H_PARAM(status_ptr), 
				     GRT_H_PARAM(state_ptr));

/* Form of argument to and result from local function */

typedef struct {
  size_t nbytes;
  void *buf;
} stm_on_local_arg_t;

/* Form of local function for stm_on.  This function does the actual
   work; it is called either directly or by the handler. */

typedef size_t (*stm_on_local_fn_t) (gasnet_node_t src_proc,
				     void *arg_buf, size_t arg_nbytes,
				     void *result_buf);
#define STM_ON_LOCAL_FN_DECL(name)				\
  size_t name##_local(gasnet_node_t src_proc, void *arg_buf,	\
		      size_t arg_nbytes, void *result_buf)

/* Form of argument passed to pthread created by the handler
   function. */

typedef struct {
  gasnet_node_t src_proc;
  stm_on_local_fn_t local_fn;
  void *arg_buf;
  size_t arg_nbytes;
  void *result_buf;
  size_t result_buf_size;
  size_t *result_size_ptr;
  grt_status_t *status;
  grt_handler_state_t *state;
} stm_on_thread_arg_t;

/* Entry in the thread list */
typedef struct {
  stm_on_thread_arg_t *arg;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  grt_bool_t proceed;
} stm_thread_t;

/* Transaction finish status:  COMMIT or ABORT */

typedef enum { COMMIT, ABORT } stm_status_t;

/* Finish the transaction (commit or abort) */

void stm_finish(tx_dor_t *tx_dor, stm_status_t status);

/* Initialize the STM state.  Must be called once at the start of
   main. */

void stm_init(int argc, char **argv, unsigned short share_power);

/* Start a new transaction on behalf of src_proc. */

#define stm_start(__src_proc)						\
  STM_DEBUG_PRINT("stm_start: src_proc=%u, tx_depth=%u\n",              \
    __src_proc, tx_dors[__src_proc].tx_depth);				\
  ++tx_dors[__src_proc].tx_depth;     					\
  if (tx_dors[__src_proc].tx_depth == 1) {				\
    tx_dors[__src_proc].tx_proc = grt_id;				\
    if (setjmp(tx_dors[__src_proc].env) != 0) {				\
      tx_dors[__src_proc].tx_depth = 1;					\
    }									\
  }

/* Same as grt_alloc and grt_all_alloc, but also set aside space for
   ownership info. */

void *stm_alloc(gasnet_node_t src_proc, gasnet_node_t mem_proc, 
		size_t nbytes);
void *stm_all_alloc(gasnet_node_t proc, unsigned nbytes);
void stm_free(gasnet_node_t proc, void *addr);

/* Get nbytes bytes on behalf of src_proc from (node, src) into dest */

void stm_get(gasnet_node_t src_proc, void *dest, 
	     gasnet_node_t mem_proc, void *src, size_t nbytes);

/* Write a GRT word on behalf of src_proc to (mem_proc, addr). */

void stm_put(gasnet_node_t src_proc, gasnet_node_t mem_proc,
	     void *dest, void *src, size_t nbytes);

/* Commit the current transaction on behalf of src_proc */

void stm_commit(gasnet_node_t src_proc);

/* Do a transactional operation on a remote processor */

size_t stm_on(gasnet_node_t src_proc, gasnet_node_t dest_proc, 
	      unsigned handler_id, stm_on_local_fn_t local_fn, 
	      void *buf, size_t nbytes, void *result_buf,
	      size_t result_nbytes);
void stm_on_handler_work(gasnet_token_t token, void *buf, size_t nbytes, 
			 gasnet_node_t src_proc, gasnet_node_t tx_proc,
			 unsigned short tx_depth, void *result_ptr,
			 size_t result_buf_size, size_t *result_size_ptr,
			 void *status_ptr, void *state_ptr,
			 stm_on_local_fn_t local_fn);
void stm_on_handler_work_pthread(gasnet_token_t token, void *buf, 
				 size_t nbytes, gasnet_node_t src_proc, 
				 gasnet_node_t tx_proc, 
				 unsigned short tx_depth, void *result_ptr,
				 size_t result_buf_size, size_t *result_size_ptr,
				 void *status_ptr, void *state_ptr,
				 stm_on_local_fn_t local_fn);


/* Clean up and leave.  Must be the last statement in main. */

#ifdef STATS
int __stm_exit(int);
#define stm_exit(ret_val) __stm_exit(ret_val)
#else
#define stm_exit(ret_val) grt_exit(ret_val)
#endif

#endif
