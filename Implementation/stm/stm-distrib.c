#include <assert.h>
#include "stm-distrib.h"

/* The STM tracks reads and writes in aligned groups of words of size
   2^stm_share_power */
unsigned short stm_share_power = 0;
/* Base address for STM metadata; a metadata location is given by its
   offset plus this address */
grt_word_t *stm_meta_base;
/* Transaction descriptors, one for each processor.  On processor x,
   tx_dor[y] holds all the metadata owing to transaction x's memory
   accesses on processor y.  Thus, the metadata for a transaction is
   distributed on all the data processors accessed by that
   transaction. */
tx_dor_t *tx_dors;
/* Processor-local lock for accessing metadata */
gasnet_hsl_t meta_lock = GASNET_HSL_INITIALIZER;

#ifdef STATS
size_t commit_count = 0, abort_count = 0;
size_t *commit_counts, *abort_counts;
#endif

/* Macros for working around the limitation that hashtable doesn't
   like 0 or 1 as a key value */

#define ENTABLE(x) ((void*) ((((unsigned long) x) + 1) << 1))
#define DETABLE(x) ((((unsigned long) x) >> 1) - 1)
			   
/* Support functions for the dataset */

typedef struct {
  void *addr;
  grt_lock_type_t type;
#ifdef READ_VERSIONING
  grt_word_t old_version;
#endif
#ifdef WRITE_BUFFERING
  void *wb_data;
#else
  void *undo_data;
#endif
#ifdef LATE_ACQUIRE
  grt_bool_t promoted;
  grt_bool_t acquired;
#endif
} dataset_entry_t;

static unsigned long dataset_hash(const void *object, void *context) {
  dataset_entry_t *entry = (dataset_entry_t*) object;
  return (unsigned long) entry->addr;
}

static int dataset_cmp(const void *object1, const void *object2,
		       void *context ) {
  dataset_entry_t *entry1 = (dataset_entry_t*) object1;
  dataset_entry_t *entry2 = (dataset_entry_t*) object2;
  return (entry1->addr != entry2->addr);
}

static int dataset_del(void *context, void *object) {
  dataset_entry_t *entry = (dataset_entry_t*) object;
#ifdef WRITE_BUFFERING
  if (entry->wb_data) free(entry->wb_data);
#else
  if (entry->undo_data) free(entry->undo_data);
#endif
  free(entry);
  return 0;
}

static dataset_entry_t *new_dataset_entry(tx_dor_t *tx_dor,
					  grt_word_t *addr) {
  dataset_entry_t *entry = 
    (dataset_entry_t*) malloc(sizeof(dataset_entry_t));
  entry->addr = addr;
#ifdef WRITE_BUFFERING
  entry->wb_data = 0;
#else
  entry->undo_data = 0;
#endif
#ifdef LATE_ACQUIRE
  entry->promoted = GRT_FALSE;
  entry->acquired = GRT_FALSE;
#endif
  hashmap_put(&tx_dor->dataset, entry, entry);
  return entry;
}

/* Support functions for transaction descriptors */

static void init_tx_dor(tx_dor_t *tx_dor, gasnet_node_t src_proc) {
  tx_dor->src_proc = src_proc;
  tx_dor->tx_depth = 0;
  hashmap_init(&tx_dor->dataset, 0, dataset_hash,
	       dataset_cmp, NULL, NULL);
  hashmap_init(&tx_dor->remote_procs, 0, 0, NULL, NULL, NULL);
  linkedlist_init(&tx_dor->allocations, 0, NULL);
  tx_dor->retry_count = 0;
}

tx_dor_t *get_tx_dor(gasnet_node_t src_proc) {
  tx_dor_t *tx_dor = &tx_dors[src_proc];
  return tx_dor;
}

/* Thread list for stm_on */

static gasnet_hsl_t stm_thread_list_hsl = GASNET_HSL_INITIALIZER;
struct linkedlist stm_thread_list;

/* Macros for manipulating meta words.  The low-order bit is the write
   bit, and the remaining bits are (1) the ID of the writer (for a
   writer lock) or (2) the number of readers (for a reader lock). */

#define test_write_bit(__m_word) (__m_word & 1LL)
#define set_write_bit(__m_word) (__m_word | 1LL)
#define clear_write_bit(__m_word) (__m_word & ~1LL)
#define num_readers(__m_word) (__m_word >> 1)
#define inc_readers(__m_word) (__m_word + 2)
#define dec_readers(__m_word) (__m_word - 2)
#define writer_id(__m_word) (__m_word >> 1)

/* Offset from the STM metadata base address to get the metadata info
   for a GRT word.  This value is valid across processors, and depends
   only on the address. */

#define meta_offset(addr) ((((unsigned long) addr) >> STM_SHARE_POWER)	\
			      >> GRT_LOG_WORD_SIZE)

/* Put the metadata offset together with the STM metadata base to get
   the address of the meta info */

#define meta_addr(addr) ((grt_word_t*)				\
			 ((unsigned long) stm_meta_base)		\
			 + meta_offset(addr))

/* stm_init implementation */

static gasnet_handlerentry_t stm_entry_table[STM_TABLE_SIZE] = {
  GRT_TABLE,
  STM_TABLE_SEGMENT
};

void stm_init(int argc, char **argv, unsigned short share_power) {
  unsigned i;
  stm_share_power = share_power;
  /* Reserve the top S / (S+1) of the GASNet heap for metadata, where
     S is the number of GRT words that share the same STM metadata */
  grt_heap_size = 
    (((long long) GASNET_HEAP_SIZE) * STM_SHARE_SIZE) / 
     (1 + STM_SHARE_SIZE);
  grt_heap_size = grt_align_down(grt_heap_size,sizeof(grt_word_t));
  /* Set up the handler table */
  if (!entry_table) {
    entry_table = stm_entry_table;
    table_size = STM_TABLE_SIZE;
  }
  /* Call grt init */
  grt_init(argc, argv);
  stm_meta_base = (grt_word_t*) 
    ((unsigned long) grt_heap_base + grt_heap_size
     - ((((unsigned long) grt_heap_base) >> STM_SHARE_POWER) & GRT_ALIGN_MASK));
  tx_dors = calloc(grt_num_procs, sizeof(tx_dor_t));
  for (i = 0; i < grt_num_procs; ++i)
    init_tx_dor(&tx_dors[i], i);

  if (grt_id == 0) {
    printf("stm_share_size: %u x grt_word (%lu bytes)\n", STM_SHARE_SIZE,
	   STM_SHARE_SIZE_BYTES);
    fflush(stdout);
  }
#ifdef BACKOFF
  /* Seed random number generator for backoff */
  srandom(grt_id);
#endif

  /* Set up STM thread list */
  linkedlist_init(&stm_thread_list, 0, 0);

#ifdef STATS
  commit_counts = (size_t*)
    grt_all_alloc(0, grt_num_procs * sizeof(size_t));
  abort_counts = (size_t*)
    grt_all_alloc(0, grt_num_procs * sizeof(size_t));
#endif

  grt_barrier();
}

static grt_global_addr_t *new_global_addr(gasnet_node_t proc,
					  void *addr) {
  grt_global_addr_t *global_addr = 
    malloc(sizeof(grt_global_addr_t));
  global_addr->proc = proc;
  global_addr->addr = addr;
  return global_addr;
}

/* acquiring read and write locks */

static grt_status_t open_read(tx_dor_t *tx_dor,
				 void *src) {
  dataset_entry_t src_entry;
  grt_status_t result;
  grt_word_t *m_addr;
  grt_word_t m_word;

  STM_DEBUG_PRINT("open_read: src_proc=%u, src=%x\n",
		  tx_dor->src_proc, src);

  /* If it's in the data set, we have it already */
  src_entry.addr = src;
  if (hashmap_get(&tx_dor->dataset, &src_entry)) {
    STM_DEBUG_PRINT("open_read: src_proc=%u, already held\n",
		    tx_dor->src_proc);
    return SUCCEED;
  }

  /* Otherwise try to acquire and return status */
  gasnet_hsl_lock(&meta_lock);
  m_addr = meta_addr(src);
  m_word = *m_addr;
  if (!test_write_bit(m_word)) {
    /* No writers; add this reader */
    dataset_entry_t *entry = new_dataset_entry(tx_dor, src);
#ifdef READ_VERSIONING
    entry->old_version = m_word;
#else
    *m_addr = inc_readers(m_word);
#endif
    entry->type = READ;
    result = SUCCEED;   
    STM_DEBUG_PRINT("open_read: SUCCEED, src_proc=%u, *m_addr=%x\n", 
		    tx_dor->src_proc, *m_addr);
  } else {
    /* Some other writer has it */
    result = FAIL;
    STM_DEBUG_PRINT("open_read: FAIL, src_proc=%u, *m_addr=%x\n",
		    tx_dor->src_proc, *m_addr);
  }
  
  gasnet_hsl_unlock(&meta_lock);
  return result;
}

static grt_status_t open_write(tx_dor_t *tx_dor,
				  void *dest) {
  dataset_entry_t *entry = 0;
  dataset_entry_t tmp_entry;
  grt_status_t result;
  grt_word_t *m_addr, m_word;

  STM_DEBUG_PRINT("open_write: src_proc=%u, dest=%x\n",
		  tx_dor->src_proc, dest);

  /* If it's in the data set for writing, we have it already */
  tmp_entry.addr = dest;
  if ((entry = hashmap_get(&tx_dor->dataset, &tmp_entry))) {
    if (entry->type == WRITE) {
      STM_DEBUG_PRINT("open_write: SUCCEED, src_proc=%u, *maddr=%x\n", 
		      tx_dor->src_proc, *meta_addr(dest));
      return SUCCEED;
    }
#ifdef LATE_ACQUIRE
    entry->promoted = GRT_TRUE;
#endif
  }

  /* Otherwise try to open and return status */
  result = FAIL;
  unsigned retry_count = 0;
  
  gasnet_hsl_lock(&meta_lock);
  m_addr = meta_addr(dest);
  m_word = *m_addr;

#ifdef LATE_ACQUIRE
  if (!test_write_bit(m_word)) result = SUCCEED;
#else
#ifdef READ_VERSIONING
  if (!test_write_bit(m_word)) result = SUCCEED;
  if ((result == SUCCEED) && entry) {
    /* Validate any reads here */
    if (entry->type == READ) {
      if (*m_addr != entry->old_version) {
	STM_DEBUG_PRINT("open_write: validation conflict detected for address %x, old_version=%x, *m_addr=%x\n", entry->addr, entry->old_version, *m_addr);
	result = FAIL;
      }
    }
  }
#else
  /* No readers or writers */
  if (m_word == 0) result = SUCCEED;
  /* One reader, us: we are promoting from reader to writer */
  if ((m_word == 2) && (entry != 0)) result = SUCCEED;
#endif
#endif
  
  if (result == SUCCEED) {
#ifndef LATE_ACQUIRE
#ifdef READ_VERSIONING
    *m_addr = set_write_bit(m_word);
#else
    *m_addr = set_write_bit(tx_dor->src_proc << 1);
#endif
#endif
    if (!entry) entry = new_dataset_entry(tx_dor, dest);
    entry->type = WRITE;
#ifdef WRITE_BUFFERING
    entry->wb_data = malloc(STM_SHARE_SIZE_BYTES);
    memcpy(entry->wb_data, dest, STM_SHARE_SIZE_BYTES);
#else
    entry->undo_data = malloc(STM_SHARE_SIZE_BYTES);
    memcpy(entry->undo_data, dest, STM_SHARE_SIZE_BYTES);
#endif
    result = SUCCEED;   
    STM_DEBUG_PRINT("open_write: SUCCEED, src_proc=%u, *m_addr=%x\n", 
		    tx_dor->src_proc, *m_addr);
  } else {
    STM_DEBUG_PRINT("open_write: FAIL, src_proc=%u, *m_addr=%x\n",
		    tx_dor->src_proc, *m_addr);
  }
  gasnet_hsl_unlock(&meta_lock);

  return result;
}

static grt_status_t open_local(gasnet_node_t src_proc,
			       void *loc, size_t nbytes,
			       grt_lock_type_t type) {
  if (type == READ)
    STM_DEBUG_PRINT("open_local: src_proc=%u, loc=%x, type=READ\n",
		    src_proc, loc);
  else
    STM_DEBUG_PRINT("open_local: src_proc=%u, loc=%x, type=WRITE\n",
		    src_proc, loc);
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  void *aligned_loc = 
    (void*) grt_align_down((unsigned long) loc, STM_SHARE_SIZE_BYTES);
  unsigned difference = (unsigned long) loc - (unsigned long) aligned_loc;
  unsigned count = 0;
  while (count < (nbytes + difference)) {
    grt_status_t status;
    if (type == READ)
      status = open_read(tx_dor, aligned_loc);
    else
      status = open_write(tx_dor, aligned_loc);
    if (status == FAIL) return FAIL;
    aligned_loc = (void*) (((char*) aligned_loc) + 
			   STM_SHARE_SIZE_BYTES);
    count += STM_SHARE_SIZE_BYTES;
  }
  return SUCCEED;
}

#ifdef WRITE_BUFFERING
/* Transfer data from private memory to write buffer or vice versa.
   tx_ptr always points to the transactional store, and prv_ptr points
   to the private store.  If type == READ, we are copying nbytes bytes
   from the transactional store to the private store.  If type ==
   WRITE, we are copying nbytes bytes the other way. */
static void memcpy_wb(gasnet_node_t src_proc, void *tx_ptr, void *prv_ptr, 
		      size_t nbytes, grt_lock_type_t type) {
  if (type == READ)
    STM_DEBUG_PRINT("memcpy_wb: tx_ptr=%x, prv_ptr=%x, nbytes=%u, type=READ\n",
		    tx_ptr, prv_ptr, nbytes);
  else
    STM_DEBUG_PRINT("memcpy_wb: tx_ptr=%x, prv_ptr=%x, nbytes=%u, type=WRITE\n",
		    tx_ptr, prv_ptr, nbytes);
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  void *aligned_tx_ptr = 
    (void*) grt_align_down((size_t) tx_ptr, STM_SHARE_SIZE_BYTES);
  size_t offset = (size_t) tx_ptr - (size_t) aligned_tx_ptr;
  size_t num_copied = 0;
  size_t num_to_copy = MIN(nbytes, STM_SHARE_SIZE_BYTES - offset);
  dataset_entry_t tmp_entry;
  dataset_entry_t *entry;
  void *buf_ptr;
  tmp_entry.addr = aligned_tx_ptr;
  entry = hashmap_get(&tx_dor->dataset, &tmp_entry);
  /* All writes must have been opened */
  assert((type == READ) || (entry && entry->wb_data));
  /* Copy the first chunk */
  buf_ptr = (entry && entry->wb_data) ? ((void*) (((size_t) entry->wb_data) + offset)) : tx_ptr;
  STM_DEBUG_PRINT("memcpy_wb: before loop: buf_ptr=%x, num_to_copy=%u\n",
		  buf_ptr, num_to_copy);
  if (type == READ) {
    memcpy(prv_ptr, buf_ptr, num_to_copy);
  } else {
    memcpy(buf_ptr, prv_ptr, num_to_copy);
  }
  num_copied += num_to_copy;
  size_t num_left;
  /* Advance by share size and copy more */
  while ((num_left = nbytes - num_copied) > 0) {
    aligned_tx_ptr = (void*) (((size_t) aligned_tx_ptr) +
			      STM_SHARE_SIZE_BYTES);
    prv_ptr = (void*) (((size_t) prv_ptr) +
		       STM_SHARE_SIZE_BYTES);
    tmp_entry.addr = aligned_tx_ptr;
    dataset_entry_t *entry = hashmap_get(&tx_dor->dataset, &tmp_entry);
    assert((type == READ) || (entry && entry->wb_data));
    void *buf_ptr = (entry && entry->wb_data) ? entry->wb_data : aligned_tx_ptr;
    num_to_copy = MIN(num_left, STM_SHARE_SIZE_BYTES);
    STM_DEBUG_PRINT("memcpy_wb: in loop: buf_ptr=%x, prv_ptr=%x, num_to_copy=%u\n",
		    buf_ptr, prv_ptr, num_to_copy);
    if (type == READ) {
      memcpy(prv_ptr, buf_ptr, num_to_copy);
    } else {
      memcpy(buf_ptr, prv_ptr, num_to_copy);
    }
    num_copied += num_to_copy;
  }
}
#endif

/* stm_get_implementation */

void stm_get_handler(gasnet_token_t token, 
		     gasnet_handlerarg_t src_proc, 
		     GRT_H_PARAM(dest), GRT_H_PARAM(src),
		     gasnet_handlerarg_t nbytes,
		     GRT_H_PARAM(status), GRT_H_PARAM(state)) {
  grt_handler_state_t *state = 
    GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state);
  void *src = GRT_H_PARAM_TO_WORD(void*, src);
  void *dest = GRT_H_PARAM_TO_WORD(void*, dest);
  if (open_local(src_proc, src, nbytes, READ) == FAIL)
    nbytes = 0;
#ifdef WRITE_BUFFERING
  void *buf = malloc(nbytes);
  memcpy_wb(src_proc, src, buf, nbytes, READ);
  unsigned i;
  GASNET_Safe(gasnetc_AMReplyMediumM(token, STM_GET_REPLY_HANDLER_ID,
  				     buf, nbytes, GRT_NUM_H_ARGS(3),
  				     GRT_H_ARG(dest), GRT_H_ARG(status),
  				     GRT_H_ARG(state)));
  free(buf);
#else
  GASNET_Safe(gasnetc_AMReplyMediumM(token, STM_GET_REPLY_HANDLER_ID,
				     src, nbytes, GRT_NUM_H_ARGS(3),
				     GRT_H_ARG(dest), GRT_H_ARG(status),
				     GRT_H_ARG(state)));
#endif
}

void stm_get_reply_handler(gasnet_token_t token, void *buf,
			   gasnet_handlerarg_t nbytes, 
			   GRT_H_PARAM(dest),
			   GRT_H_PARAM(status),
			   GRT_H_PARAM(state)) {
  grt_status_t *status = GRT_H_PARAM_TO_WORD(grt_status_t*, status);
  void *dest = GRT_H_PARAM_TO_WORD(void*, dest);
  if (nbytes > 0) {
    memcpy(dest, buf, nbytes);
    *status = SUCCEED;
  } else {
    *status = FAIL;
  }
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state) = DONE;
}

void stm_get(gasnet_node_t src_proc, void *dest, 
	     gasnet_node_t mem_proc, 
	     void *src, size_t nbytes) {
  grt_status_t status;
  STM_DEBUG_PRINT("stm_get: src_proc=%u, mem_proc = %u, dest=%x, src=%x, nbytes=%u\n",
		  src_proc, mem_proc, dest, src, nbytes);
  if (mem_proc == grt_id) {
    status = open_local(src_proc, src, nbytes, READ);
    if (status == SUCCEED) {
#ifdef WRITE_BUFFERING
      memcpy_wb(src_proc, src, dest, nbytes, READ);
      unsigned i;
#else
      memcpy(dest, src, nbytes);
#endif
    }
  } else {
    grt_handler_state_t state = PENDING;
    tx_dor_t *tx_dor = get_tx_dor(src_proc);
    hashmap_put(&tx_dor->remote_procs, ENTABLE(mem_proc), 0);
    GASNET_Safe(gasnetc_AMRequestShortM(mem_proc, STM_GET_HANDLER_ID, 
					GRT_NUM_H_ARGS(4)+2,
					src_proc, GRT_WORD_TO_H_ARG(dest),
					GRT_WORD_TO_H_ARG(src), nbytes,
					GRT_WORD_TO_H_ARG(&status),
					GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  if (status == FAIL)
    stm_finish(&tx_dors[src_proc], ABORT);				
}

/* stm_put implementation */

void stm_put_handler(gasnet_token_t token, 
		     void *src, size_t nbytes,
		     gasnet_handlerarg_t src_proc, 
		     GRT_H_PARAM(dest),
		     GRT_H_PARAM(status_ptr), GRT_H_PARAM(state)) {
  grt_handler_state_t *state = 
    GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state);
  void *dest = GRT_H_PARAM_TO_WORD(void*, dest);
  grt_status_t status = open_local(src_proc, dest, nbytes, WRITE);
  if (status == SUCCEED) {
#ifdef WRITE_BUFFERING
    memcpy_wb(src_proc, dest, src, nbytes, WRITE);
#else
    memcpy(dest, src, nbytes);
#endif

  }
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(state),
				    GRT_WORD_TO_H_ARG(status), 
				    GRT_H_ARG(status_ptr)));
}

void stm_put(gasnet_node_t src_proc, gasnet_node_t mem_proc,
	     void *dest, void *src, size_t nbytes) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  grt_status_t status;
  if (!nbytes) return;
  STM_DEBUG_PRINT("stm_put: src_proc=%u, mem_proc = %u, dest=%x, src=%x, nbytes=%u\n",
		  src_proc, mem_proc, dest, src, nbytes);
  if (mem_proc == grt_id) {
    status = open_local(src_proc, dest, nbytes, WRITE);
    if (status == SUCCEED) {
#ifdef WRITE_BUFFERING
      memcpy_wb(src_proc, dest, src, nbytes, WRITE);
#else
      memcpy(dest, src, nbytes);
#endif
    }
  } else {
    grt_handler_state_t state = PENDING;
    hashmap_put(&tx_dor->remote_procs, ENTABLE(mem_proc), 0);
    GASNET_Safe(gasnetc_AMRequestMediumM(mem_proc, STM_PUT_HANDLER_ID, 
					 src, nbytes,
					 GRT_NUM_H_ARGS(3)+1,
					 src_proc, GRT_WORD_TO_H_ARG(dest),
					 GRT_WORD_TO_H_ARG(&status),
					 GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  if (status == FAIL)
    stm_finish(tx_dor, ABORT);				
}

/* stm_alloc implementation */

void *stm_alloc_local(tx_dor_t *tx_dor, unsigned nbytes) {
  void *result = 0;
  if (tx_dor)
    STM_DEBUG_PRINT("stm_alloc_local: src_proc=%u, nbytes=%u\n",
		    tx_dor->src_proc, nbytes);
  else
    STM_DEBUG_PRINT("stm_alloc_local: all_alloc, nbytes=%u\n",
		    nbytes);
  nbytes = grt_align_up(nbytes, STM_SHARE_SIZE_BYTES);
  result = grt_memalign_local(nbytes, STM_SHARE_SIZE_BYTES);
  memset(meta_addr(result), 0, sizeof(grt_word_t) * nbytes /
	 STM_SHARE_SIZE_BYTES);
  
  if (tx_dor && tx_dor->tx_depth > 0) {
    /* Add to allocations list in case we need to undo on abort */
    linkedlist_add(&tx_dor->allocations, result);
  }
  return result;
}

void stm_alloc_handler(gasnet_token_t token, 
		       gasnet_handlerarg_t src_proc, 
		       gasnet_handlerarg_t tx_depth,
		       gasnet_handlerarg_t nbytes,
		       GRT_H_PARAM(result_ptr), GRT_H_PARAM(state)) {
  void *result;
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  tx_dor->tx_depth = tx_depth;
  result = stm_alloc_local(tx_dor, nbytes);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(state),
				    GRT_WORD_TO_H_ARG(result), 
				    GRT_H_ARG(result_ptr)));
}

void *stm_alloc(gasnet_node_t src_proc, gasnet_node_t mem_proc, 
		size_t nbytes) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  void *result = 0;
  STM_DEBUG_PRINT("stm_alloc: src_proc=%u, mem_proc=%u, nbytes=%u\n",
		  src_proc, mem_proc, nbytes);
  if (mem_proc == grt_id)
    result = stm_alloc_local(tx_dor, nbytes);
  else {
    grt_handler_state_t state = PENDING;
    if (tx_dor->tx_depth > 0) {
      hashmap_put(&tx_dor->remote_procs, ENTABLE(mem_proc), 0);
    }
    GASNET_Safe(gasnetc_AMRequestShortM(mem_proc, STM_ALLOC_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+3,
					src_proc, tx_dor->tx_depth, nbytes,
					GRT_WORD_TO_H_ARG(&result),
					GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result;
}

void *stm_all_alloc(gasnet_node_t mem_proc, unsigned nbytes) {
  void *result;
  if (mem_proc == grt_id) {
    result = stm_alloc_local(0, nbytes);
  }
  grt_barrier();
  if (mem_proc != grt_id) {
    result = grt_get_last_allocation(mem_proc);
  }
  grt_barrier();
  return result;
}

/* stm_free implementation */

void stm_free(gasnet_node_t mem_proc, void *addr) {
  grt_free(mem_proc, addr);
}

/* stm_commit implementation */

void stm_commit(gasnet_node_t src_proc) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  STM_DEBUG_PRINT("stm_commit: src_proc=%u, tx_depth=%u\n", 
		  src_proc, tx_dor->tx_depth);
  if (!--tx_dor->tx_depth) {
    stm_finish(tx_dor, COMMIT);
  }
}

/* stm_finish implementation */

static void stm_finish_local(tx_dor_t *tx_dor, 
			     stm_status_t status) {
  STM_DEBUG_PRINT("stm_finish_local: src_proc=%u\n",
		  tx_dor->src_proc);
  /* Release all locks */
  struct hashmap *dataset = &tx_dor->dataset;
  struct linkedlist *allocations = &tx_dor->allocations;
  iter_t iter;
  dataset_entry_t *entry;
  hashmap_iterate(dataset, &iter);
  while ((entry = (dataset_entry_t*) hashmap_next(dataset, &iter))) {
    grt_word_t *m_addr = meta_addr(entry->addr);
    gasnet_hsl_lock(&meta_lock);
    if (entry->type == READ) {
      /* Release read lock */
      *m_addr = dec_readers(*m_addr);
      STM_DEBUG_PRINT("releasing read lock:  src_proc=%u,addr=%x, *m_addr=%u\n", 
		      tx_dor->src_proc, entry->addr, *m_addr);
    } else {
      /* WRITE dataset entry */
#ifdef WRITE_BUFFERING
      if (status == COMMIT) {
	STM_DEBUG_PRINT("stm_finish_local: committing: src_proc=%u, addr=%x\n",
			tx_dor->src_proc, entry->addr);
	memcpy(entry->addr, entry->wb_data, STM_SHARE_SIZE_BYTES);
      }
#else
      if (status == ABORT) {
	/* Restore from the undo log */
	STM_DEBUG_PRINT("restoring from undo log\n");
	memcpy(entry->addr, entry->undo_data, STM_SHARE_SIZE_BYTES);
      }
#endif
#ifdef READ_VERSIONING
      /* Increment or restore version */
      if (status == COMMIT) {
	*m_addr = clear_write_bit(*m_addr)+2;
	STM_DEBUG_PRINT("stm_finish_local: incrementing version: src_proc=%u, addr=%x, *m_addr=%u\n", 
			tx_dor->src_proc, entry->addr, *m_addr);
      } else {
	*m_addr = clear_write_bit(*m_addr);
	STM_DEBUG_PRINT("stm_finish_local: restoring version: src_proc=%u, addr=%x, *m_addr=%u\n", 
			tx_dor->src_proc, entry->addr, *m_addr);
      }
#else
#ifdef LATE_ACQUIRE
      if (entry->acquired == GRT_TRUE) {
#endif
	/* Release write lock */
	*m_addr = 0;
	STM_DEBUG_PRINT("releasing write lock: src_proc=%u, addr=%x, *m_addr=%u\n", 
			tx_dor->src_proc, entry->addr, *m_addr);
#ifdef LATE_ACQUIRE
      } else if (entry->promoted == GRT_TRUE) {
	/* Release read lock */
	*m_addr = dec_readers(*m_addr);
	STM_DEBUG_PRINT("releasing read lock:  src_proc=%u,addr=%x, *m_addr=%u\n", 
			tx_dor->src_proc, entry->addr, *m_addr);
      }
#endif
#endif
    }
    gasnet_hsl_unlock(&meta_lock);
  }
  if (status == ABORT) {
    /* Free allocations we did */
    void *addr;
    linkedlist_iterate(allocations, &iter);
    while ((addr = linkedlist_next(allocations, &iter))) {
      STM_DEBUG_PRINT("releasing allocation: addr=%x\n", addr);
      stm_free(grt_id, addr);
    }
  }
  hashmap_clear(dataset, dataset_del, NULL, NULL);
  linkedlist_clear(allocations, NULL, NULL);
}

void stm_finish_handler(gasnet_token_t token, 
			gasnet_handlerarg_t src_proc,
			gasnet_handlerarg_t status,
			GRT_H_PARAM(state)) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  stm_finish_local(tx_dor, status);
  GASNET_Safe(gasnetc_AMReplyShortM(token, VOID_REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(1),
				    GRT_H_ARG(state)));
}

static stm_status_t stm_acquire_local(tx_dor_t *tx_dor) {
#ifdef LATE_ACQUIRE
  STM_DEBUG_PRINT("stm_acquire_local: src_proc=%u\n",
		  tx_dor->src_proc);
  stm_status_t status = COMMIT;
  struct hashmap *dataset = &tx_dor->dataset;
  iter_t iter;
  dataset_entry_t *entry;
  hashmap_iterate(dataset, &iter);
  while ((entry = (dataset_entry_t*) hashmap_next(dataset, &iter))) {
    grt_word_t *m_addr = meta_addr(entry->addr);
    gasnet_hsl_lock(&meta_lock);
    if (entry->type == WRITE) {
      if (*m_addr == 0 || 
	  (*m_addr == 2 && (entry->promoted == GRT_TRUE))) {
	*m_addr = set_write_bit(*m_addr);
	entry->acquired = GRT_TRUE;
      } else {
	STM_DEBUG_PRINT("stm_acquire_local: conflict detected for address %x, *m_addr=%x\n", 
			entry->addr, *m_addr);
	status = ABORT;
      }
    }
    gasnet_hsl_unlock(&meta_lock);
    if (status == ABORT) return ABORT;
  }
#endif
  return COMMIT;
}

void stm_acquire_handler(gasnet_token_t token,
			  gasnet_handlerarg_t src_proc,
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr)) {
  STM_DEBUG_PRINT("acquire handler, src_proc=%u\n", src_proc);
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  stm_status_t status = stm_acquire_local(tx_dor);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(2)+1,
				    GRT_H_ARG(state_ptr),
				    status,
				    GRT_H_ARG(status_ptr)));
}

static stm_status_t stm_acquire(tx_dor_t *tx_dor) {
  stm_status_t status;
  struct hashmap *remote_procs;
  iter_t iter;
  gasnet_node_t remote_proc;
  unsigned i = 0;
  unsigned size;
  grt_word_t *state_arr;
  grt_word_t *status_arr;
  STM_DEBUG_PRINT("stm_acquire: src_proc=%u\n", tx_dor->src_proc);
  /* Local acquire */
  status = stm_acquire_local(tx_dor);
  if (status == ABORT)
    return status;
  /* Acquire on remote processors */
  remote_procs = &tx_dor->remote_procs;
  hashmap_iterate(remote_procs, &iter);
  size = hashmap_size(remote_procs);
  state_arr = malloc(size * sizeof(grt_word_t));
  status_arr = malloc(size * sizeof(grt_word_t));
  while ((remote_proc = 
	  (gasnet_node_t) (grt_word_t) hashmap_next(remote_procs, &iter))) {
    grt_word_t *state_ptr = &state_arr[i];
    grt_word_t *status_ptr = &status_arr[i];
    *state_ptr = PENDING;
    STM_DEBUG_PRINT("stm_acquire: calling handler, src_proc=%u\n", tx_dor->src_proc);
    GASNET_Safe(gasnetc_AMRequestShortM(DETABLE(remote_proc), 
					STM_ACQUIRE_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+1,
					tx_dor->src_proc,
					GRT_WORD_TO_H_ARG(status_ptr),
					GRT_WORD_TO_H_ARG(state_ptr)));
    ++i;
  }
  for (i = 0; i < size; ++i) {
    GASNET_BLOCKUNTIL(state_arr[i] == DONE);
    if (status_arr[i] == ABORT)
      status = ABORT;
  }
  free(state_arr);
  free(status_arr);
  return status;
}

#ifdef READ_VERSIONING
static stm_status_t stm_validate_local(tx_dor_t *tx_dor) {
  STM_DEBUG_PRINT("stm_validate_local: src_proc=%u\n",
		  tx_dor->src_proc);
  stm_status_t status = COMMIT;
  struct hashmap *dataset = &tx_dor->dataset;
  iter_t iter;
  dataset_entry_t *entry;
  hashmap_iterate(dataset, &iter);
  while ((entry = (dataset_entry_t*) hashmap_next(dataset, &iter))) {
    grt_word_t *m_addr = meta_addr(entry->addr);
    gasnet_hsl_lock(&meta_lock);
    if (entry->type == READ)
      if (*m_addr != entry->old_version) {
	STM_DEBUG_PRINT("stm_finish_local: conflict detected for address %x, old_version=%x, *m_addr=%x\n", entry->addr, entry->old_version, *m_addr);
	status = ABORT;
      }
    gasnet_hsl_unlock(&meta_lock);
    if (status == ABORT) return ABORT;
  }
  return COMMIT;
}

void stm_validate_handler(gasnet_token_t token,
			  gasnet_handlerarg_t src_proc,
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr)) {
  STM_DEBUG_PRINT("validate handler called\n");
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  grt_word_t status = stm_validate_local(tx_dor);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(2)+1,
				    GRT_H_ARG(state_ptr),
				    status,
				    GRT_H_ARG(status_ptr)));
}

static stm_status_t stm_validate(tx_dor_t *tx_dor) {
  stm_status_t status;
  struct hashmap *remote_procs;
  iter_t iter;
  gasnet_node_t remote_proc;
  unsigned i = 0;
  unsigned size;
  grt_word_t *state_arr;
  grt_word_t *status_arr;
  STM_DEBUG_PRINT("stm_validate: src_proc=%u\n", tx_dor->src_proc);
  /* Local validate */
  status = stm_validate_local(tx_dor);
  if (status == ABORT)
    return status;
  /* Validate on remote processors */
  remote_procs = &tx_dor->remote_procs;
  hashmap_iterate(remote_procs, &iter);
  size = hashmap_size(remote_procs);
  state_arr = malloc(size * sizeof(grt_word_t));
  status_arr = malloc(size * sizeof(grt_word_t));
  while ((remote_proc = 
	  (gasnet_node_t) (grt_word_t) hashmap_next(remote_procs, &iter))) {
    grt_word_t *state_ptr = &state_arr[i];
    grt_word_t *status_ptr = &status_arr[i];
    *state_ptr = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(DETABLE(remote_proc), 
					STM_VALIDATE_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+1,
					tx_dor->src_proc,
					GRT_WORD_TO_H_ARG(status_ptr),
					GRT_WORD_TO_H_ARG(state_ptr)));
    ++i;
  }
  for (i = 0; i < size; ++i) {
    GASNET_BLOCKUNTIL(state_arr[i] == DONE);
    if (status_arr[i] == ABORT)
      status = ABORT;
  }
  free(state_arr);
  free(status_arr);
  return status;
}
#endif

#ifdef READ_VERSIONING
stm_status_t stm_finish_home(tx_dor_t *tx_dor, stm_status_t status) {
#else
#ifdef LATE_ACQUIRE
stm_status_t stm_finish_home(tx_dor_t *tx_dor, stm_status_t status) {
#else
void stm_finish_home(tx_dor_t *tx_dor, stm_status_t status) {
#endif
#endif
  struct hashmap *remote_procs;
  iter_t iter;
  gasnet_node_t remote_proc;
  unsigned i = 0;
  unsigned size;
  grt_word_t *states;
  if (status == COMMIT) {
    STM_DEBUG_PRINT("stm_finish: src_proc=%u, COMMIT\n",
		    tx_dor->src_proc);
  } else {
    STM_DEBUG_PRINT("stm_finish: src_proc=%u, ABORT\n",
		    tx_dor->src_proc);
  }
#ifdef LATE_ACQUIRE
  if (status == COMMIT)
    status = stm_acquire(tx_dor);
#endif
#ifdef READ_VERSIONING
  if (status == COMMIT)
    status = stm_validate(tx_dor);
#endif
  /* Local finish */
  stm_finish_local(tx_dor, status);
  /* Finish on all remote processors */
  remote_procs = &tx_dor->remote_procs;
  hashmap_iterate(remote_procs, &iter);
  size = hashmap_size(remote_procs);
  states = malloc(size * sizeof(grt_word_t));
  while ((remote_proc = 
	  (gasnet_node_t) (grt_word_t) hashmap_next(remote_procs, &iter))) {
    grt_word_t *state = &states[i++];
    *state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(DETABLE(remote_proc), 
					STM_FINISH_HANDLER_ID, 
					GRT_NUM_H_ARGS(1)+2,
					tx_dor->src_proc,
					status,
					GRT_WORD_TO_H_ARG(state)));
  }
  for (i = 0; i < size; ++i)
    GASNET_BLOCKUNTIL(states[i] == DONE);
  free(states);
  hashmap_clear(remote_procs, NULL, NULL, NULL);
#ifdef BACKOFF
  if (status == ABORT) {
    if (tx_dor->retry_count == 20) {
      tx_dor->retry_count = 0;
    }
    if (tx_dor->retry_count == 0) {
      tx_dor->backoff_time = ((double) random()) * 10 / RAND_MAX;
    } else {
      tx_dor->backoff_time <<= 1;
    }
    usleep(tx_dor->backoff_time);
    ++tx_dor->retry_count;
  }
#endif
#ifdef READ_VERSIONING
  return status;
#else
#ifdef LATE_ACQUIRE
  return status;
#endif
#endif
}

void stm_finish(tx_dor_t *tx_dor, stm_status_t status) {
  STM_DEBUG_PRINT("stm_finish: src_proc=%u\n", tx_dor->src_proc);
  if (tx_dor->tx_proc == grt_id) {
#ifdef READ_VERSIONING
    status = stm_finish_home(tx_dor, status);
#else
#ifdef LATE_ACQUIRE
    status = stm_finish_home(tx_dor, status);
#else
    stm_finish_home(tx_dor, status);
#endif
#endif
  }
#ifdef STATS
  if (status == COMMIT) {
    ++commit_count;
  } else {
    ++abort_count;
  }
#endif
  if (status == ABORT) {
    longjmp(tx_dor->env, 1);
  }
}

/* stm_on implementation */

void stm_on_reply_handler(gasnet_token_t token, void *buf, 
			  size_t nbytes,
			  gasnet_handlerarg_t status,
			  GRT_H_PARAM(result),
			  GRT_H_PARAM(result_size_ptr),
			  GRT_H_PARAM(status_ptr),
			  GRT_H_PARAM(state_ptr)) {
  stm_on_local_arg_t *result = 
    GRT_H_PARAM_TO_WORD(stm_on_local_arg_t*, result);
  if (nbytes) {
    memcpy(result, buf, nbytes);
  }
  *GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr) = nbytes;
  *GRT_H_PARAM_TO_WORD(grt_status_t*, status_ptr) = status;
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr) = DONE;
}

static void *stm_on_thread(void *void_arg) {
  stm_on_thread_arg_t *arg = (stm_on_thread_arg_t*) void_arg;
  grt_word_t status = SUCCEED;
  void *my_result_buf = malloc(arg->result_buf_size);
  size_t result_size = 0;
  if (setjmp(tx_dors[arg->src_proc].env) == 0) {
    STM_DEBUG_PRINT("stm_on_thread: calling local_fn for src_proc=%u\n",
		    arg->src_proc);
    /* First time through; if we finish local_fn, we've succeeded */
    result_size = (arg->local_fn)(arg->src_proc, arg->arg_buf, 
				  arg->arg_nbytes, my_result_buf);
  } else {
    /* On longjmp */
    STM_DEBUG_PRINT("stm_on_pthread: back from longjmp\n");
    status = FAIL;
  }
  GASNET_Safe(gasnetc_AMRequestMediumM(arg->src_proc, 
				       STM_ON_REPLY_HANDLER_ID,
				       GASNET_SAFE_NULL(void*, my_result_buf), 
				       result_size,
				       GRT_NUM_H_ARGS(4)+1, status,
				       GRT_WORD_TO_H_ARG(GASNET_SAFE_NULL(void*,
									  arg->result_buf)),
				       GRT_WORD_TO_H_ARG(arg->result_size_ptr),
				       GRT_WORD_TO_H_ARG(arg->status),
				       GRT_WORD_TO_H_ARG(arg->state)));
  free(arg->arg_buf);
  free(my_result_buf);
  free(arg);
  return 0;
}

void *stm_thread_work(void *arg) {
  stm_thread_t *thread = (stm_thread_t*) arg;
  while (1) {
    if (thread->proceed == GRT_TRUE) {
      thread->proceed = GRT_FALSE;
      stm_on_thread(thread->arg);
      gasnet_hsl_lock(&stm_thread_list_hsl);
      STM_DEBUG_PRINT("thread_work: adding thread to list\n");
      linkedlist_add(&stm_thread_list, thread);
      gasnet_hsl_unlock(&stm_thread_list_hsl);
    } else {
      STM_DEBUG_PRINT("thread_work: cond_wait\n");
      pthread_cond_wait(&thread->cond, &thread->mutex);
    }
  }
  return 0;
}

void stm_on_handler_work_pthread(gasnet_token_t token, 
				 void *buf, size_t nbytes, 
				 gasnet_node_t src_proc, 
				 gasnet_node_t tx_proc,
				 unsigned short tx_depth, 
				 void *result_buf,
				 size_t result_buf_size,
				 size_t *result_size_ptr,
				 void *status_ptr, void *state_ptr,
				 stm_on_local_fn_t local_fn) {
  STM_DEBUG_PRINT("stm_on_handler_work_pthread\n");
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  stm_on_thread_arg_t *arg = 
    (stm_on_thread_arg_t*) malloc(sizeof(stm_on_thread_arg_t));
  tx_dor->tx_depth = tx_depth;
  tx_dor->tx_proc = tx_proc;
  arg->src_proc = src_proc;
  arg->local_fn = local_fn;
  if (nbytes) {
    arg->arg_buf = malloc(nbytes);
    memcpy(arg->arg_buf, buf, nbytes);
  } else {
    arg->arg_buf = 0;
  }
  arg->arg_nbytes = nbytes;
  arg->result_buf = result_buf;
  arg->result_buf_size = result_buf_size;
  arg->result_size_ptr = result_size_ptr;
  arg->status = status_ptr;
  arg->state = state_ptr;
  STM_DEBUG_PRINT("taking stm_thread_list_hsl\n");
  gasnet_hsl_lock(&stm_thread_list_hsl);
  STM_DEBUG_PRINT("accessing thread_list\n");
  stm_thread_t *thread = (stm_thread_t*) linkedlist_remove(&stm_thread_list, 0);
  STM_DEBUG_PRINT("releasing stm_thread_list_hsl\n");
  gasnet_hsl_unlock(&stm_thread_list_hsl);
  if (!thread) {
    STM_DEBUG_PRINT("Creating new thread\n");
    pthread_t pthread;
    thread = (stm_thread_t*) malloc(sizeof(stm_thread_t));
    thread->arg = arg;
    thread->proceed = GRT_TRUE;
    pthread_cond_init(&thread->cond, NULL);
    pthread_mutex_init(&thread->mutex, NULL);
    GRT_Safe(pthread_create(&pthread, NULL, stm_thread_work, thread));
  } else {
    STM_DEBUG_PRINT("Waking up thread\n");
    thread->arg = arg;
    thread->proceed = GRT_TRUE;
    do {
      pthread_cond_signal(&thread->cond);
      sched_yield();
    } while (thread->proceed == GRT_TRUE);
  }
}

void stm_on_handler_work(gasnet_token_t token, void *buf, size_t nbytes, 
			 gasnet_node_t src_proc, gasnet_node_t tx_proc,
			 unsigned short tx_depth, void *result_buf,
			 size_t result_buf_size, size_t *result_size_ptr,
			 void *status_ptr, void *state_ptr,
			 stm_on_local_fn_t local_fn) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  void *my_result_buf = 0;
  grt_status_t status = SUCCEED;
  size_t result_size = 0;
  tx_dor->tx_depth = tx_depth;
  tx_dor->tx_proc = tx_proc;
  if (result_buf_size) {
    my_result_buf = malloc(result_buf_size);
  }
  if (setjmp(tx_dors[src_proc].env) == 0) {
    STM_DEBUG_PRINT("stm_on_handler: calling local_fn\n");
    /* First time through; if we finish local_fn, we've succeeded */
    result_size = (local_fn)(src_proc, buf, nbytes, my_result_buf);
  } else {
    /* On longjmp */
    STM_DEBUG_PRINT("stm_on_handler: back from longjmp\n");
    status = FAIL;
  }
  GASNET_Safe(gasnetc_AMReplyMediumM(token,
				     STM_ON_REPLY_HANDLER_ID,
				     GASNET_SAFE_NULL(void*, my_result_buf), 
				     result_size,
				     GRT_NUM_H_ARGS(4)+1, status,
				     GRT_WORD_TO_H_ARG(result_buf),
				     GRT_WORD_TO_H_ARG(result_size_ptr),
				     GRT_WORD_TO_H_ARG(status_ptr),
				     GRT_WORD_TO_H_ARG(state_ptr)));
  free(my_result_buf);
}

size_t stm_on(gasnet_node_t src_proc, gasnet_node_t dest_proc, 
	      unsigned handler_id, stm_on_local_fn_t local_fn,
	      void *arg_buf, size_t arg_nbytes, void *result_buf, 
	      size_t result_buf_size) {
  tx_dor_t *tx_dor = get_tx_dor(src_proc);
  size_t result_size;
  arg_buf = GASNET_SAFE_NULL(void*, arg_buf);
  result_buf = GASNET_SAFE_NULL(void*, result_buf);
  if (grt_id == dest_proc) {
    result_size = local_fn(grt_id, arg_buf, arg_nbytes, result_buf);
  } else {
    grt_handler_state_t state = PENDING;
    grt_status_t status;
    state = PENDING;
    if (tx_dor->tx_depth > 0)
      hashmap_put(&tx_dor->remote_procs, ENTABLE(dest_proc), 0);
    GASNET_Safe(gasnetc_AMRequestMediumM(dest_proc, handler_id,
					 arg_buf, arg_nbytes,
					 GRT_NUM_H_ARGS(4)+4,
					 src_proc, tx_dor->tx_proc,
					 tx_dor->tx_depth,
					 GRT_WORD_TO_H_ARG(result_buf),
					 result_buf_size,
					 GRT_WORD_TO_H_ARG(&result_size),
					 GRT_WORD_TO_H_ARG(&status),
					 GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
    if (status == FAIL)
      stm_finish(tx_dor, ABORT);
  }
  return result_size;
}

#ifdef STATS
 int __stm_exit(int ret_val) {
   unsigned i = 0;							
   gasnet_put(0, &commit_counts[grt_id], &commit_count, sizeof(size_t)); 
   gasnet_put(0, &abort_counts[grt_id], &abort_count, sizeof(size_t));	
   grt_barrier();							
   if (grt_id == 0) {							
     commit_count = abort_count = 0;
     for (i = 0; i < grt_num_procs; ++i) {				
       commit_count += commit_counts[i];
       abort_count += abort_counts[i];
     }
     printf("commit count: %d\n", (int) commit_count);
     printf("abort count: %d\n", (int) abort_count);
     fflush(stdout);
   }
   grt_barrier();
   return grt_exit(ret_val);
 }
#endif


