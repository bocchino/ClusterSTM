/*****************************************************************
  Interface for a simple "GASNet Runtime" (GRT)
******************************************************************/

#ifndef GRT_H
#define GRT_H

#include <pthread.h>
#include "grt_macros.h"
#include "grt_list.h"
#include "grt_debug.h"
#include "umalloc/umalloc.h"
#include "gasnet_tools.h"

#if GRT_DEBUG
#define GRT_DEBUG_PRINT(...) grt_debug_print(__VA_ARGS__)
#else
#define GRT_DEBUG_PRINT(...)
#endif

#define FIRST_GRT_ID 128
#define GRT_ID(x) (FIRST_GRT_ID + x)

#define CAS_HANDLER_ID          GRT_ID(0)
#define ALLOC_HANDLER_ID        GRT_ID(1)
#define LOCK_ALLOC_HANDLER_ID   GRT_ID(2)
#define NEW_LOCK_ALLOC_HANDLER_ID GRT_ID(3)
#define GET_ALLOC_HANDLER_ID    GRT_ID(4)
#define REPLY_HANDLER_ID        GRT_ID(5)
#define VOID_REPLY_HANDLER_ID   GRT_ID(6)
#define FREE_HANDLER_ID         GRT_ID(7)
#define LOCK_FREE_HANDLER_ID    GRT_ID(8)
#define NEW_LOCK_FREE_HANDLER_ID GRT_ID(9)
#define LOCK_HANDLER_ID         GRT_ID(10)
#define NEW_LOCK_HANDLER_ID     GRT_ID(11)
#define LOCK_REPLY_HANDLER_ID   GRT_ID(12)
#define UNLOCK_HANDLER_ID       GRT_ID(13)
#define NEW_UNLOCK_HANDLER_ID   GRT_ID(14)
#define UNLOCK_REPLY_HANDLER_ID GRT_ID(15)
#define READ_HANDLER_ID         GRT_ID(16)
#define WRITE_HANDLER_ID        GRT_ID(17)
#define GRT_ON_REPLY_HANDLER_ID GRT_ID(18)

#define LAST_GRT_ID GRT_ON_REPLY_HANDLER_ID

#define GRT_TABLE_SIZE (LAST_GRT_ID - FIRST_GRT_ID + 1)

#define GRT_TABLE \
  { CAS_HANDLER_ID, cas_handler },			\
  { ALLOC_HANDLER_ID, alloc_handler },			\
  { LOCK_ALLOC_HANDLER_ID, lock_alloc_handler },	\
  { NEW_LOCK_ALLOC_HANDLER_ID, new_lock_alloc_handler },	\
  { GET_ALLOC_HANDLER_ID, get_alloc_handler },		\
  { REPLY_HANDLER_ID, reply_handler },			\
  { VOID_REPLY_HANDLER_ID, void_reply_handler },	\
  { FREE_HANDLER_ID, free_handler },			\
  { LOCK_FREE_HANDLER_ID, lock_free_handler},		\
  { NEW_LOCK_FREE_HANDLER_ID, new_lock_free_handler},		\
  { LOCK_HANDLER_ID, lock_handler },			\
  { NEW_LOCK_HANDLER_ID, new_lock_handler },		\
  { LOCK_REPLY_HANDLER_ID, lock_reply_handler },	\
  { UNLOCK_HANDLER_ID, unlock_handler },		\
  { NEW_UNLOCK_HANDLER_ID, new_unlock_handler },		\
  { UNLOCK_REPLY_HANDLER_ID, unlock_reply_handler },	\
  { READ_HANDLER_ID, read_handler },			\
  { WRITE_HANDLER_ID, write_handler },                  \
  { GRT_ON_REPLY_HANDLER_ID, grt_on_reply_handler }

/* Program global, processor local data */

extern gasnet_node_t grt_id;
extern unsigned grt_num_procs;
extern char grt_proc_name[];
extern gasnet_seginfo_t* grt_seginfo;
extern unsigned grt_heap_size;
extern void *grt_heap_base;

/* Change these variables to use a different handler table */

extern gasnet_handlerentry_t* entry_table;
extern unsigned table_size;

/* AM handlers */

void cas_handler(gasnet_token_t token, 
		 GRT_H_PARAM(statep),
		 GRT_H_PARAM(addr),
		 GRT_H_PARAM(old),
		 GRT_H_PARAM(new),
		 GRT_H_PARAM(resultp));

void alloc_handler(gasnet_token_t token,
		   gasnet_handlerarg_t nbytes,
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(resultp));

void new_lock_alloc_handler(gasnet_token_t token,
			    GRT_H_PARAM(statep),
			    GRT_H_PARAM(resultp),
			    size_t n);

void lock_alloc_handler(gasnet_token_t token,
			GRT_H_PARAM(statep),
			GRT_H_PARAM(resultp),
			unsigned n);

void free_handler(gasnet_token_t token,
		  GRT_H_PARAM(ptr));

void new_lock_free_handler(gasnet_token_t token,
			   GRT_H_PARAM(locks),
			   size_t n);
void lock_free_handler(gasnet_token_t token,
		       GRT_H_PARAM(addr),
		       unsigned n);

void get_alloc_handler(gasnet_token_t token,
		       GRT_H_PARAM(statep),
		       GRT_H_PARAM(resultp));

void new_lock_handler(gasnet_token_t token,
		      gasnet_handlerarg_t src_proc,
		      gasnet_handlerarg_t type,
		      GRT_H_PARAM(lock),
		      GRT_H_PARAM(result_size_ptr),
		      GRT_H_PARAM(state));

void lock_handler(gasnet_token_t token,
		  gasnet_handlerarg_t proc,
		  gasnet_handlerarg_t type,
		  GRT_H_PARAM(lockp),
		  GRT_H_PARAM(statep));

void new_unlock_handler(gasnet_token_t token,
			GRT_H_PARAM(lock),
			GRT_H_PARAM(state));
void unlock_handler(gasnet_token_t token,
		    GRT_H_PARAM(hstatep),
		    GRT_H_PARAM(lprocp),
		    GRT_H_PARAM(lstatep),
		    GRT_H_PARAM(lockp));

/* AM Reply Handlers */

/* copy result to its destination; set state flag */

void reply_handler(gasnet_token_t token, 
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(result),
		   GRT_H_PARAM(resultp));

/* Set state flag only */

void void_reply_handler(gasnet_token_t token, 
			GRT_H_PARAM(statep));

void lock_reply_handler(gasnet_token_t token,
			GRT_H_PARAM(statep));

void unlock_reply_handler(gasnet_token_t token,
			  GRT_H_PARAM(hstatep),
			  gasnet_handlerarg_t lproc, 
			  GRT_H_PARAM(lprocp),
			  GRT_H_PARAM(lstate),
			  GRT_H_PARAM(lstatep));

void read_handler(gasnet_token_t token,
		  GRT_H_PARAM(statep),
		  GRT_H_PARAM(ptr),
		  GRT_H_PARAM(resultp));

void write_handler(gasnet_token_t token,
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(value),
		   GRT_H_PARAM(ptr));

void grt_on_reply_handler(gasnet_token_t token, void *buf, 
			  size_t nbytes, GRT_H_PARAM(result),
			  GRT_H_PARAM(result_size_ptr), 
			  GRT_H_PARAM(state_ptr));

/* Software queued lock */

typedef struct {
  unsigned num_holders;
  grt_lock_type_t type;
  grt_list_t *waiters;
} grt_lock_t;

typedef struct {
  size_t num_readers;
  grt_lock_type_t type;
  gasnet_hsl_t read_hsl;
  gasnet_hsl_t write_hsl;
} grt_new_lock_t;

typedef struct {
  gasnet_node_t src_proc;
  grt_new_lock_t *lock;
  grt_lock_type_t type;
} grt_new_lock_arg_t;

typedef struct {
  gasnet_node_t proc;
  grt_lock_state_t *state;
  grt_lock_type_t type;
} grt_lock_waiter_t;

/* Calculate the local address in processor's shared address space at
   the given offset */

#define grt_addr(processor, offset) \
  ((char*) grt_seginfo[processor].addr) + offset

/* Initialize the GRT state.  Must be called once at the start of
   main. */

void grt_init(int argc, char **argv);

/* Clean up and leave.  Must be the last statement in main. */

#define grt_exit(ret_val)			\
  fflush(stdout);				\
  BARRIER();					\
  if (grt_id == 0)				\
    gasnet_exit(ret_val);			\
  return ret_val

/* Compare and swap of old and new on processor at addr.  Addr must
   validly point into processor's shared address space.  Returns what
   was in addr before the operation.  If return value == old_word, the
   operation succeeded; otherwise it failed. */

/* NOTE THAT GRT_CAS IS ONLY GUARANTEED TO WORK IF EVERY CONFLICTING
   WRITE IS DONE WITH GRT_WRITE OR GRT_WRITE_NB!  In particular, you
   can't use any of the gasnet_put functions to conflicting locations.
   For this reason, grt_cas is deprecated, and I may get rid of it
   entirely. */

grt_word_t grt_cas(unsigned processor, grt_word_t *addr,
		   grt_word_t old_word, grt_word_t new_word);

/* Allocate and initialize a global queued lock */

grt_new_lock_t *grt_new_lock_alloc(gasnet_node_t proc,
				   size_t n);
grt_new_lock_t *grt_all_new_lock_alloc(gasnet_node_t proc,
				       size_t n);
grt_lock_t *grt_lock_alloc(gasnet_node_t proc, unsigned n);
grt_lock_t *grt_all_lock_alloc(gasnet_node_t proc, unsigned n);
void *grt_get_last_allocation(gasnet_node_t proc);

/* Lock or unlock a GRT lock. */

void grt_new_lock(gasnet_node_t proc, grt_new_lock_t *lock,
		  grt_lock_type_t type);
void grt_lock(gasnet_node_t proc, grt_lock_t *lock, 
	      grt_lock_type_t type, grt_lock_state_t *state);
void grt_unlock(gasnet_node_t proc, grt_lock_t *lock);

/* Destroy a GRT lock */

void grt_lock_free(gasnet_node_t proc, grt_lock_t *addr,
		   unsigned n);

/* Read a GRT word from the designated processor at the designated
   pointer into shared memory.  Pointer must validly point into this
   processor's address space. */

grt_word_t grt_read(gasnet_node_t proc, void *ptr);
void grt_read_nb(gasnet_node_t proc, grt_word_t *src, 
		 grt_word_t *dest, grt_handler_state_t *state);

/* Write a GRT word to the designated processor at the designated
   pointer into shared memory.  Pointer must validly point into this
   processor's address space. */

void grt_write(gasnet_node_t proc, grt_word_t val, grt_word_t *ptr);
void grt_write_nb(gasnet_node_t proc, grt_word_t val,
		  grt_word_t *addr, grt_handler_state_t *state);

/* Allocate nbytes bytes of shared space on the designated processor
   and return a pointer to the allocation.  If called by multiple
   processors, each call does a new allocation. */

void *grt_alloc(gasnet_node_t proc, unsigned nbytes);
void *grt_alloc_local(size_t nbytes);
void *grt_memalign_local(size_t nbytes, size_t align);

/* Must be called by all processors.  Allocate nbytes bytes of shared
   space on the designated processor and return the same pointer to
   the allocation to all callers. */

void *grt_all_alloc(unsigned processor, unsigned nbytes);

/* Free an allocation made by grt_alloc or grt_all_alloc.  May be
   called only once for each allocation. */

void grt_free(gasnet_node_t proc, void* ptr);

/* Do some work on a remote processor */

/* Form of local function for grt_on.  This function does the actual
   work; it is called either directly or by the handler. */
typedef size_t (*grt_on_local_fn_t) (void *arg_buf, size_t arg_nbytes,
				     void *result_buf);

/* Form of argument passed to pthread created by the handler
   function. */
typedef struct {
  gasnet_node_t src_proc;
  grt_on_local_fn_t local_fn;
  void *arg_buf;
  size_t arg_nbytes;
  void *result_buf;
  size_t *result_size_ptr;
  size_t result_nbytes;
  grt_handler_state_t *state;
} grt_on_thread_arg_t;

/* Entry in the thread list */
typedef struct {
  grt_on_thread_arg_t *arg;
  pthread_cond_t cond;
  pthread_mutex_t mutex;
  grt_bool_t proceed;
} grt_thread_t;

size_t grt_on(gasnet_node_t proc, unsigned handler_id, 
	      grt_on_local_fn_t local_fn, void *arg_buf, 
	      size_t arg_nbytes, void *result_buf, 
	      size_t result_nbytes);
void grt_on_nb(gasnet_node_t proc, unsigned handler_id, 
	       grt_on_local_fn_t local_fn, void *arg_buf, 
	       size_t arg_nbytes, void *result_buf, 
	       size_t result_buf_size, size_t *result_size,
	       grt_handler_state_t *state);

/* Handler definition template for grt_on */

#define GRT_ON_LOCAL_FN_DECL(handler_name) \
  size_t handler_name##_local(void *arg_buf, size_t arg_nbytes, \
			      void *result_buf)
#define GRT_ON_HANDLER_DECL(handler_name)				\
  void handler_name##_handler(gasnet_token_t token, void *buf,		\
			      size_t nbytes,				\
			      gasnet_handlerarg_t src_proc,		\
			      GRT_H_PARAM(result_buf),			\
			      gasnet_handlerarg_t result_buf_size,	\
			      GRT_H_PARAM(result_size_ptr),		\
			      GRT_H_PARAM(state_ptr))

#define GRT_ON_HANDLER_DEF(handler_name) \
  GRT_ON_HANDLER_DECL(handler_name) {	 \
    grt_on_handler_work(token, buf, nbytes, src_proc,                   \
			GRT_H_PARAM_TO_WORD(void*, result_buf),		\
			result_buf_size,				\
			GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr),	\
			GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr), \
			handler_name##_local);				\
  }									\
  GRT_ON_HANDLER_DECL(handler_name##_pthread) {	 \
    grt_on_handler_work_pthread(token, buf, nbytes, src_proc,           \
				GRT_H_PARAM_TO_WORD(void*, result_buf), \
				result_buf_size,			\
				GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr), \
				GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr), \
				handler_name##_local);			\
  }
    
void grt_on_handler_work_pthread(gasnet_token_t token, 
				 void *buf, size_t nbytes, 
				 gasnet_node_t src_proc, 
				 void *result_buf,
				 size_t result_nbytes,
				 size_t *result_size_ptr,
				 void *state_ptr,
				 grt_on_local_fn_t local_fn);

void grt_on_handler_work(gasnet_token_t token, void *buf, size_t nbytes, 
			 gasnet_node_t src_proc,
			 void *result_buf, size_t result_nbytes,
			 size_t *result_size_ptr,
			 void *state_ptr, grt_on_local_fn_t local_fn);
#endif
