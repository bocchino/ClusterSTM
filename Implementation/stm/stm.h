/*****************************************************************
  STM Interface
******************************************************************/

#ifndef STM_H
#define STM_H

#ifdef STM_EARLY
#include <mba/hashmap.h>
#endif

#define STM_DEBUG_ON 0

#if STM_DEBUG_ON
#define STM_DEBUG(msg) \
printf("processor %u: ", grt_id); \
msg; \
fflush(stdout);
#else
#define STM_DEBUG(msg)
#endif

#include <setjmp.h>
#include "grt.h"

/* The STM tracks reads and writes in aligned groups of words of size
   2^STM_SHARE_POWER */

#ifndef STM_SHARE_POWER
#define STM_SHARE_POWER 0
#endif

extern unsigned nesting_count;
extern jmp_buf env;

/* Initialize the STM state.  Must be called once at the start of
   main. */

void stm_init(int argc, char **argv);

/* Same as grt_alloc and grt_all_alloc, but also set aside space for
   ownership info. */

void *stm_alloc(gasnet_node_t proc, unsigned nbytes);
void *stm_all_alloc(gasnet_node_t proc, unsigned nbytes);
void stm_free(gasnet_node_t proc, void *ptr);

/* AM Handler Table */

#define FIRST_STM_ID (LAST_GRT_ID + 1)
#define STM_ID(x) (FIRST_STM_ID + x)

#ifdef STM_EARLY
#define LAST_STM_ID FIRST_STM_ID
#define STM_TABLE_SEGMENT
#define STM_TABLE_SIZE GRT_TABLE_SIZE
#else
void acquire_handler(gasnet_token_t token,
		     GRT_H_PARAM(addr),
		     GRT_H_PARAM(state_ptr),
		     GRT_H_PARAM(resultp));

void release_handler(gasnet_token_t token,
		     GRT_H_PARAM(addr));

void open_for_read_handler(gasnet_token_t token,
			   GRT_H_PARAM(entry),
			   GRT_H_PARAM(addr));

void open_for_read_reply_handler(gasnet_token_t token,
				 GRT_H_PARAM(entry),
				 GRT_H_PARAM(version),
				 GRT_H_PARAM(val));

void commit_write_handler(gasnet_token_t token,
			  GRT_H_PARAM(val),
			  GRT_H_PARAM(addr));

#define ACQUIRE_HANDLER_ID             STM_ID(0)
#define RELEASE_HANDLER_ID             STM_ID(1)
#define OPEN_FOR_READ_HANDLER_ID       STM_ID(2)
#define OPEN_FOR_READ_REPLY_HANDLER_ID STM_ID(3)
#define COMMIT_WRITE_HANDLER_ID        STM_ID(4)
#define LAST_STM_ID COMMIT_WRITE_HANDLER_ID

#define STM_TABLE_SEGMENT						\
  { ACQUIRE_HANDLER_ID, acquire_handler },				\
  { RELEASE_HANDLER_ID, release_handler },				\
  { OPEN_FOR_READ_HANDLER_ID, open_for_read_handler },			\
  { OPEN_FOR_READ_REPLY_HANDLER_ID, open_for_read_reply_handler },	\
  { COMMIT_WRITE_HANDLER_ID, commit_write_handler }
#define STM_TABLE_SIZE (GRT_TABLE_SIZE + (LAST_STM_ID - FIRST_STM_ID) + 1)
#endif

static void stm_abort();
static void stm_cleanup();

#define ENV env
#define NESTING_COUNT nesting_count

/* Start a new transaction. */

void __stm_start();
#define stm_start()	  			              \
  if (NESTING_COUNT == 0) {				      \
    setjmp(ENV);				     	      \
    NESTING_COUNT = 1;					      \
    __stm_start();					      \
  } else {					      	      \
    ++NESTING_COUNT;				       	      \
  }

/* Log that a read will occur at the designated GRT word.  Must be
   called at least once before each STM read of this word in the
   current transaction. */

void stm_open_for_read(gasnet_node_t proc, grt_word_t *ptr);

/* Read a GRT word from the designated processor at the designated
   pointer into shared memory.  Pointer must validly point into this
   processor's address space. */

grt_status_t __stm_read(gasnet_node_t proc, grt_word_t *addr, 
			grt_word_t *result);
#define stm_read(__proc, __addr, __result)		\
  if (__stm_read(__proc, __addr, __result) == FAIL) {	\
    longjmp(ENV, 1);					\
  }

/* Log that a write will occur at the designated GRT word.  Must be
   called at least once before each STM write of this word in the
   current transaction. */

void stm_open_for_write(gasnet_node_t proc,
			grt_word_t *ptr);

/* Write a GRT word to the designated processor at the designated
   pointer into shared memory.  Pointer must validly point into this
   processor's address space. */

void stm_write(gasnet_node_t proc, grt_word_t val,
	       grt_word_t *ptr);

/* Attempt to commit the current transaction and abort on failure. */

grt_status_t __stm_commit();
#define stm_commit() \
  if (!--NESTING_COUNT) {						\
    if (__stm_commit() == FAIL) {					\
      longjmp(ENV, 1);							\
    }									\
  }

/* Clean up and leave.  Must be the last statement in main. */

#define stm_exit(ret_val) grt_exit(ret_val)

#endif
