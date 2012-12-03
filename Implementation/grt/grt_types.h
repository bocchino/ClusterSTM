#ifndef GRT_TYPES_H
#define GRT_TYPES_H

#include "gasnet.h"

/* Definitions of GRT word and macros for decomposing GRT words into
   actual words.  On a 32-bit machine, a GRT word is a 32-bit unsigned
   int, its low word is the entire GRT word, and its high word is 0.
   On a 64-bit machine, a GRT word is a 64-bit unsigned int, and its
   low and high words are its low and high 32 bits. */

#if SIZEOF_VOID_P == 4
  #define GRT_WORD_32
/* Log of word size, in bytes */
  #define GRT_LOG_WORD_SIZE 2
/* Mask for stripping off unaligned bits of address */
  #define GRT_ALIGN_MASK (~0x3U)
  typedef uint32_t grt_word_t;
  #define GRT_H_PARAM(x) gasnet_handlerarg_t x##_param
  #define GRT_H_ARG(__x) __x##_param
  #define GRT_H_PARAM_TO_WORD(__type, __name) ((__type) __name##_param)
  #define GRT_WORD_TO_H_ARG(__x) ((gasnet_handlerarg_t) __x)
  #define GRT_NUM_H_ARGS(x) (x)
#elif SIZEOF_VOID_P == 8
  #define GRT_WORD_64
/* Log of word size, in bytes */
  #define GRT_LOG_WORD_SIZE 3
/* Mask for stripping off unaligned bits of address */
  #define GRT_ALIGN_MASK (~0x7UL)
  typedef uint64_t grt_word_t;
  #define GRT_WORD_LO(ptr) \
    ((uint32_t) ((uint64_t) ptr))
  #define GRT_WORD_HI(ptr) \
    ((uint32_t) (((uint64_t) ptr) >> 32))
  #define GRT_WORD(lo, hi) ((grt_word_t) ((lo & 0xffffffff) | (((uint64_t) hi) << 32)))
#define GRT_H_PARAM(x)						\
  gasnet_handlerarg_t x##_lo, gasnet_handlerarg_t x##_hi
#define GRT_H_ARG(__x)				\
  __x##_lo, __x##_hi
#define GRT_H_PARAM_TO_WORD(__type, __name) \
  ((__type) GRT_WORD(__name##_lo, __name##_hi))
#define GRT_WORD_TO_H_ARG(__x) \
  ((gasnet_handlerarg_t) GRT_WORD_LO(__x)), ((gasnet_handlerarg_t) GRT_WORD_HI(__x))
#define GRT_NUM_H_ARGS(x) (x << 1)
#else
#error Not on 32-bit or 64-bit machine!
#endif

typedef struct {
  gasnet_node_t proc;
  grt_word_t* addr;
} grt_global_addr_t;

typedef enum { SUCCEED, FAIL } grt_status_t;
typedef enum { FREE, HELD, WAITING } grt_lock_state_t;
typedef enum { READ, WRITE } grt_lock_type_t;
typedef enum { PENDING, DONE } grt_handler_state_t;
typedef enum { GRT_FALSE, GRT_TRUE } grt_bool_t;

#endif
