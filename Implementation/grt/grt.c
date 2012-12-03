#include "assert.h"
#include "grt.h"
#include <unistd.h>
#include <mba/linkedlist.h>
#include <sched.h>

#if 0
int spawncount=0;
#endif
gasnet_node_t grt_id;
unsigned grt_num_procs;
char grt_proc_name[MAX_PROCESSOR_NAME];
gasnet_seginfo_t* grt_seginfo;
void *grt_last_alloc;
umalloc_heap_t *grt_heap;
unsigned grt_heap_size = GASNET_HEAP_SIZE;
void *grt_heap_base;

static gasnet_hsl_t mem_lock = GASNET_HSL_INITIALIZER;
static gasnet_hsl_t alloc_lock = GASNET_HSL_INITIALIZER;
static gasnet_hsl_t lock_lock = GASNET_HSL_INITIALIZER;

/* Thread list for grt_on */

static gasnet_hsl_t thread_list_hsl = GASNET_HSL_INITIALIZER;
struct linkedlist thread_list;

/* AM Handler Table */

static gasnet_handlerentry_t grt_entry_table[GRT_TABLE_SIZE] = {
  GRT_TABLE
};
gasnet_handlerentry_t* entry_table = 0;
unsigned table_size = 0;

/* poll function for service pthread */

static void *poll(void* arg) {
  while(GRT_TRUE) {
    GASNET_Safe(gasnet_AMPoll());
    sched_yield();
  }
}

/* grt_init implementation */

void grt_init(int argc, char **argv) {

  pthread_t thread;

  /* Set up handler table */
  if (!entry_table) {
    entry_table = grt_entry_table;
    table_size = GRT_TABLE_SIZE;
  }

  /* call startup */
  GASNET_Safe(gasnet_init(&argc, &argv));		

  /* get SPMD info */
  grt_id = gasnet_mynode();
  grt_num_procs = gasnet_nodes();
  gethostname(grt_proc_name, MAX_PROCESSOR_NAME);

  /* Attach to network */
  GASNET_Safe(gasnet_attach(entry_table, table_size,
			    GASNET_HEAP_SIZE, MINHEAPOFFSET));
  if (grt_id == 0) {
    printf("%s\n", argv[0]);
#ifdef GRT_WORD_32
      printf("We are on a 32-bit machine.\n");
#else
      printf("We are on a 64-bit machine.\n");
#endif
      printf("gasnet_AMMaxMedium()=%lu\n", gasnet_AMMaxMedium());
      printf("gasnet_AMMaxLongRequest()=%lu\n", gasnet_AMMaxLongRequest());
      printf("gasnet_AMMaxLongReply()=%lu\n", gasnet_AMMaxLongReply());
  }
  fflush(stdout);
  BARRIER();
  fflush(stdout);
  BARRIER();

  /* Get segment info */
  grt_seginfo = (gasnet_seginfo_t*) malloc(sizeof(gasnet_seginfo_t) *
					   grt_num_procs);
  GASNET_Safe(gasnet_getSegmentInfo(grt_seginfo, grt_num_procs));

  /* Initialize the heap for memory allocation */
  grt_heap_base = grt_addr(grt_id, 0);
  grt_heap = umalloc_makeheap(grt_heap_base,
			      grt_heap_size, UMALLOC_HEAP_GROWS_UP);  

  /* Spawn off a thread to handle remote handler requests */
  pthread_create(&thread, NULL, poll, NULL);

  /* Set up thread list */
  linkedlist_init(&thread_list, 0, 0);

  BARRIER();
}

/* grt_read implementation */

void read_handler(gasnet_token_t token,
		  GRT_H_PARAM(statep),
		  GRT_H_PARAM(ptr),
		  GRT_H_PARAM(resultp)) {
  grt_word_t result = *GRT_H_PARAM_TO_WORD(grt_word_t*, ptr);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(result),
				    GRT_H_ARG(resultp)));
}

grt_word_t grt_read(gasnet_node_t proc, void *ptr) {
  grt_word_t result;
  if (proc == grt_id)
    result = *((grt_word_t*) ptr);
  else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, READ_HANDLER_ID,
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(ptr),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result;
}

void grt_read_nb(gasnet_node_t proc, grt_word_t *dest, 
		 grt_word_t *src, grt_handler_state_t *state) {
  if (proc == grt_id) {
    *dest = *src;
    gasnett_local_mb();
    *state = DONE;
  } else {
    *state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, READ_HANDLER_ID,
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(state),
					GRT_WORD_TO_H_ARG(src),
					GRT_WORD_TO_H_ARG(dest)));
  }
}

/* grt_write implementation */

void write_handler(gasnet_token_t token,
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(value),
		   GRT_H_PARAM(ptr)) {
  gasnet_hsl_lock(&mem_lock);
  *GRT_H_PARAM_TO_WORD(grt_word_t*, ptr) = 
    GRT_H_PARAM_TO_WORD(grt_word_t, value);
  gasnet_hsl_unlock(&mem_lock);
  GASNET_Safe(gasnetc_AMReplyShortM(token, VOID_REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(1),
				    GRT_H_ARG(statep)));
}

void grt_write(gasnet_node_t proc, grt_word_t val, grt_word_t *ptr) {
  if (proc == grt_id) {
    gasnet_hsl_lock(&mem_lock);
    *((grt_word_t*) ptr) = val;
    gasnet_hsl_unlock(&mem_lock);
  } else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, WRITE_HANDLER_ID, 
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(ptr)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
}

void grt_write_nb(gasnet_node_t proc, grt_word_t val,
		  grt_word_t *addr, grt_handler_state_t *state) {
  if (proc == grt_id) {
    gasnet_hsl_lock(&mem_lock);
    *((grt_word_t*) addr) = val;
    gasnet_hsl_unlock(&mem_lock);
    gasnett_local_mb();
    *state = DONE;
  } else {
    *state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, WRITE_HANDLER_ID, 
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(state),
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(addr)));
  }
}

/* grt_cas implementation */

static grt_word_t cas_local(grt_word_t *addr, grt_word_t old,
			    grt_word_t new) {
  grt_word_t result;
  gasnet_hsl_lock(&mem_lock);
  result = *addr;
  if (result == old) {
    *addr = new;
  }
  gasnet_hsl_unlock(&mem_lock);
  return result;
}

void cas_handler(gasnet_token_t token, 
		 GRT_H_PARAM(statep),
		 GRT_H_PARAM(addr),
		 GRT_H_PARAM(old_word),
		 GRT_H_PARAM(new_word),
		 GRT_H_PARAM(resultp)) {
  grt_word_t old_word, new_word;
  grt_word_t *addr;
  grt_word_t result;
  old_word = GRT_H_PARAM_TO_WORD(grt_word_t, old_word);
  new_word = GRT_H_PARAM_TO_WORD(grt_word_t, new_word);
  addr = GRT_H_PARAM_TO_WORD(grt_word_t*, addr);
  result = cas_local(addr, old_word, new_word);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(result),
				    GRT_H_ARG(resultp)));
}

grt_word_t grt_cas(unsigned processor, grt_word_t *addr,
		   grt_word_t old, grt_word_t new) {
  grt_handler_state_t state = PENDING;
  grt_word_t result = 0;
  if (processor == grt_id) {
    result = cas_local(addr, old, new);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(processor, CAS_HANDLER_ID, 
					GRT_NUM_H_ARGS(5),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(addr),
					GRT_WORD_TO_H_ARG(old),
					GRT_WORD_TO_H_ARG(new),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result;
}

/* grt_alloc implementation */

void *grt_memalign_local(size_t nbytes, size_t align) {
  gasnet_hsl_lock(&alloc_lock);
  grt_last_alloc = umemalign(grt_heap, align, nbytes);
  memset(grt_last_alloc, 0, nbytes);
  gasnet_hsl_unlock(&alloc_lock);
  return grt_last_alloc;
}

void *grt_alloc_local(size_t nbytes) {
  gasnet_hsl_lock(&alloc_lock);
  grt_last_alloc = ucalloc(grt_heap, nbytes, 1);
  gasnet_hsl_unlock(&alloc_lock);
  return grt_last_alloc;
}

void alloc_handler(gasnet_token_t token,
		   gasnet_handlerarg_t nbytes,
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(resultp)) {
  gasnet_hsl_lock(&alloc_lock);
  grt_last_alloc = ucalloc(grt_heap, nbytes, 1);
  gasnet_hsl_unlock(&alloc_lock);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(grt_last_alloc),
				    GRT_H_ARG(resultp)));
}

void *grt_alloc(gasnet_node_t proc, unsigned nbytes) {
  grt_handler_state_t state = PENDING;
  void *result = 0;
  if (proc == grt_id) {
    gasnet_hsl_lock(&alloc_lock);
    grt_last_alloc = ucalloc(grt_heap, nbytes, 1);
    result = grt_last_alloc;
    gasnet_hsl_unlock(&alloc_lock);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, ALLOC_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+1,
					nbytes,
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result;
}

/* grt_all_alloc implementation */

void get_alloc_handler(gasnet_token_t token,
		       GRT_H_PARAM(statep),
		       GRT_H_PARAM(resultp)) {
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(grt_last_alloc),
				    GRT_H_ARG(resultp)));
}

void *grt_get_last_allocation(gasnet_node_t proc) {
  void *result;
  grt_handler_state_t state = PENDING;
  if (proc == grt_id) {
    result = grt_last_alloc;
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, GET_ALLOC_HANDLER_ID, 
					GRT_NUM_H_ARGS(2),
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&result)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result;
}

void *grt_all_alloc(unsigned proc, unsigned nbytes) {
  void *result;
  if (proc == grt_id) {
    gasnet_hsl_lock(&alloc_lock);
    grt_last_alloc = ucalloc(grt_heap, nbytes, 1);
    gasnet_hsl_unlock(&alloc_lock);
  }
  BARRIER();
  result = grt_get_last_allocation(proc);
  BARRIER();
  return result;
}

/* grt_lock_alloc implementation */

grt_new_lock_t *new_lock_alloc_local(size_t n) {
  grt_new_lock_t *locks;
  unsigned i;
  locks = 
    (grt_new_lock_t*) grt_alloc(grt_id, n*sizeof(grt_new_lock_t));
  for (i = 0; i < n; ++i) {
    locks[i].num_readers = 0;
    gasnet_hsl_init(&locks[i].read_hsl);
    gasnet_hsl_init(&locks[i].write_hsl);
  }
  return locks;
}

grt_lock_t *lock_alloc_local(unsigned n) {
  grt_lock_t *locks;
  unsigned i;
  gasnet_hsl_lock(&alloc_lock);
  locks =  
    (grt_lock_t*) ucalloc(grt_heap, sizeof(grt_lock_t), n);
  gasnet_hsl_unlock(&alloc_lock);
  for (i = 0; i < n; ++i) {
    locks[i].num_holders = 0;
    locks[i].waiters = grt_list_create();
  }
  return locks;
}

void new_lock_alloc_handler(gasnet_token_t token,
			    GRT_H_PARAM(statep),
			    GRT_H_PARAM(resultp),
			    size_t n) {
  grt_new_lock_t *locks = new_lock_alloc_local(n);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(locks),
				    GRT_H_ARG(resultp)));
}

void lock_alloc_handler(gasnet_token_t token,
			GRT_H_PARAM(statep),
			GRT_H_PARAM(resultp),
			unsigned n) {
  grt_lock_t *locks = lock_alloc_local(n);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(statep),
				    GRT_WORD_TO_H_ARG(locks),
				    GRT_H_ARG(resultp)));
}

grt_new_lock_t *grt_new_lock_alloc(gasnet_node_t proc,
				   size_t n) {
  grt_new_lock_t *locks;
  if (proc == grt_id) {
    locks = new_lock_alloc_local(n);
  } else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, NEW_LOCK_ALLOC_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+1,
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&locks),
					(gasnet_handlerarg_t) n));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return locks;
}

grt_lock_t *grt_lock_alloc(gasnet_node_t proc,
			   unsigned n) {
  grt_lock_t *locks;
  if (proc == grt_id) {
    locks = lock_alloc_local(n);
  } else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, LOCK_ALLOC_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+1,
					GRT_WORD_TO_H_ARG(&state),
					GRT_WORD_TO_H_ARG(&locks),
					(gasnet_handlerarg_t) n));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return locks;
}

/* grt_all_lock_alloc implementation */

grt_new_lock_t *grt_all_new_lock_alloc(gasnet_node_t proc, 
				       size_t n) {
  grt_new_lock_t *locks = grt_all_alloc(proc, sizeof(grt_new_lock_t) * n);
  if (proc == grt_id) {
    unsigned i;
    for (i = 0; i < n; ++i) {
      gasnet_hsl_init(&locks[i].read_hsl);
      gasnet_hsl_init(&locks[i].write_hsl);
    }
  }
  BARRIER();
  return locks;
}

grt_lock_t *grt_all_lock_alloc(gasnet_node_t proc,
			       unsigned n) {
  grt_lock_t *locks = grt_all_alloc(proc, sizeof(grt_lock_t) * n);
  if (proc == grt_id) {
    unsigned i;
    for (i = 0; i < n; ++i) {
      locks[i].num_holders = 0;
      locks[i].waiters = grt_list_create();
    }
  }
  BARRIER();
  return locks;
}

/* grt_free implementation */

void free_handler(gasnet_token_t token,
		  GRT_H_PARAM(ptr)) {
  gasnet_hsl_lock(&alloc_lock);
  ufree(grt_heap, GRT_H_PARAM_TO_WORD(void*, ptr));
  gasnet_hsl_unlock(&alloc_lock);
}

void grt_free(gasnet_node_t proc, void* ptr) {
  if (proc == grt_id) {
    gasnet_hsl_lock(&alloc_lock);
    ufree(grt_heap, ptr);
    gasnet_hsl_unlock(&alloc_lock);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, FREE_HANDLER_ID, 
					GRT_NUM_H_ARGS(1),
					GRT_WORD_TO_H_ARG(ptr)));
  }
}

/* grt_lock_free implementation */

void new_lock_free_local(grt_new_lock_t *locks, size_t n) {
  unsigned i;
  for (i = 0; i < n; ++i) {
    gasnet_hsl_destroy(&locks[i].read_hsl);
    gasnet_hsl_destroy(&locks[i].write_hsl);
  }
  grt_free(grt_id, locks);
}

void lock_free_local(grt_lock_t *addr, unsigned n) {
  unsigned i;
  for (i = 0; i < n; ++i)
    grt_list_destroy(addr[i].waiters);
  gasnet_hsl_lock(&alloc_lock);
  ufree(grt_heap, addr);
  gasnet_hsl_unlock(&alloc_lock);
}

void new_lock_free_handler(gasnet_token_t token,
			   GRT_H_PARAM(locks),
			   size_t n) {
  grt_new_lock_t *locks = 
    GRT_H_PARAM_TO_WORD(grt_new_lock_t*, locks);
  new_lock_free_local(locks, n);
}

void lock_free_handler(gasnet_token_t token,
		       GRT_H_PARAM(addr),
		       unsigned n) {
  grt_lock_t *addr = GRT_H_PARAM_TO_WORD(grt_lock_t*, addr);
  lock_free_local(addr, n);
}

void grt_new_lock_free(gasnet_node_t proc, grt_new_lock_t *locks,
		       size_t n) {
  if (proc == grt_id) {
    new_lock_free_local(locks, n);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, NEW_LOCK_FREE_HANDLER_ID,
					GRT_NUM_H_ARGS(1)+1,
					GRT_WORD_TO_H_ARG(locks), n));
  }
}

void grt_lock_free(gasnet_node_t proc, grt_lock_t *addr,
		   unsigned n) {
  if (proc == grt_id) {
    lock_free_local(addr, n);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, LOCK_FREE_HANDLER_ID,
					GRT_NUM_H_ARGS(1)+1,
					GRT_WORD_TO_H_ARG(addr), n));
  }
}

/* grt_lock implementation */

grt_status_t new_trylock_local(gasnet_node_t src_proc,
			       grt_new_lock_t *lock, 
			       grt_lock_type_t type) {
  //return FAIL;
  if (type == READ) {
    if (gasnet_hsl_trylock(&lock->read_hsl) != GASNET_OK)
      return FAIL;
    if (lock->num_readers == 0) {
      if (gasnet_hsl_trylock(&lock->write_hsl) != GASNET_OK) {
	gasnet_hsl_unlock(&lock->read_hsl);
	return FAIL;
      }
    }
    ++lock->num_readers;
    lock->type = READ;
    GRT_DEBUG_PRINT("acquired read lock: lock=%x, src_proc=%u\n", lock, src_proc);
    gasnet_hsl_unlock(&lock->read_hsl);
  } else {
    if (gasnet_hsl_trylock(&lock->write_hsl) != GASNET_OK)
      return FAIL;
    lock->type = WRITE;
    GRT_DEBUG_PRINT("acquired write lock: lock=%x, src_proc=%u\n", lock, src_proc);
  }
  return SUCCEED;
}

void new_lock_local(gasnet_node_t src_proc,
		    grt_new_lock_t *lock, 
		    grt_lock_type_t type) {
  if (type == READ) {
    gasnet_hsl_lock(&lock->read_hsl);
    if (lock->num_readers == 0) {
      gasnet_hsl_lock(&lock->write_hsl);
    }
    ++lock->num_readers;
    lock->type = READ;
    GRT_DEBUG_PRINT("acquired read lock: lock=%x, src_proc=%u\n", lock, src_proc);
    gasnet_hsl_unlock(&lock->read_hsl);
  } else {
    gasnet_hsl_lock(&lock->write_hsl);
    lock->type = WRITE;
    GRT_DEBUG_PRINT("acquired write lock: lock=%x, src_proc=%u\n", lock, src_proc);
  }
}

static grt_status_t lock_local(gasnet_node_t proc, grt_lock_t *lock, 
			       grt_lock_type_t type, grt_lock_state_t *state) {
  grt_status_t status;
  gasnet_hsl_lock(&lock_lock);
  if (lock->num_holders == 0) {
    GRT_DEBUG_PRINT("lock local serving %u: no holders; taking (%u, %x), type=%u\n",
		    proc, grt_id, lock, type);
    lock->type = type;
    lock->num_holders = 1;
    status = SUCCEED;
  } else if (lock->type == READ && type == READ) {
    GRT_DEBUG_PRINT("lock local serving %u: held for reading, adding reader to (%u, %x)\n",
		    proc, grt_id, lock);
    ++lock->num_holders;
    status = SUCCEED;
  } else {
    grt_lock_waiter_t* waiter = malloc(sizeof(grt_lock_waiter_t));
    GRT_DEBUG_PRINT("lock local serving %u: adding state=%x, type=%u, to wait list for (%u, %x)\n",
		    proc, state, type, grt_id, lock);
    waiter->proc = proc;
    waiter->state = state;
    waiter->type = type;
    grt_list_append(lock->waiters, (grt_word_t) waiter);
    status = FAIL;
  }
  gasnet_hsl_unlock(&lock_lock);
  return status;
}

static size_t new_lock_local_wrapper(void *arg_buf, 
				     size_t arg_nbytes,
				     void *result_buf) {
  GRT_DEBUG_PRINT("new_lock_local_wrapper\n");
  grt_new_lock_arg_t *arg = (grt_new_lock_arg_t*) arg_buf;
  new_lock_local(arg->src_proc, arg->lock, arg->type);
  return 0;
}

void new_lock_handler(gasnet_token_t token,
		      gasnet_handlerarg_t src_proc,
		      gasnet_handlerarg_t type,
		      GRT_H_PARAM(lock),
		      GRT_H_PARAM(result_size_ptr),
		      GRT_H_PARAM(state)) {
  GRT_DEBUG_PRINT("new_lock_handler: src_proc=%u, type=%u, lock=%x, state=%x\n",
		  src_proc, type, GRT_H_PARAM_TO_WORD(grt_new_lock_t*, lock), 
		  GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state));
  grt_status_t status;
  /* Try to get the lock in the handler first */
  status = new_trylock_local(src_proc, 
			     GRT_H_PARAM_TO_WORD(grt_new_lock_t*, lock),
			     (grt_lock_type_t) type);
  if (status == SUCCEED) {
    GASNET_Safe(gasnetc_AMReplyShortM(token, VOID_REPLY_HANDLER_ID, 
				      GRT_NUM_H_ARGS(1),
				      GRT_H_ARG(state)));
  } else {
    /* If that fails, use a pthread to do it */
    grt_new_lock_arg_t arg;
    arg.src_proc = src_proc;
    arg.lock = GRT_H_PARAM_TO_WORD(grt_new_lock_t*, lock);
    arg.type = (grt_lock_type_t) type;
    grt_on_handler_work_pthread(token, &arg, sizeof(grt_new_lock_arg_t),
				src_proc, 0, 0, 
				GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr),
				GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state),
				new_lock_local_wrapper);
  }
}

void lock_handler(gasnet_token_t token,
		  gasnet_handlerarg_t proc,
		  gasnet_handlerarg_t type,
		  GRT_H_PARAM(lockp),
		  GRT_H_PARAM(statep)) {
  grt_lock_t *lock = GRT_H_PARAM_TO_WORD(grt_lock_t*, lockp);
  grt_lock_state_t *state = GRT_H_PARAM_TO_WORD(grt_lock_state_t*, statep);
  if (lock_local(proc, lock, (grt_lock_type_t) type, state) == SUCCEED)
    GASNET_Safe(gasnetc_AMReplyShortM(token, LOCK_REPLY_HANDLER_ID, 
				      GRT_NUM_H_ARGS(1),
				      GRT_H_ARG(statep)));
}

void lock_reply_handler(gasnet_token_t token,
			GRT_H_PARAM(statep)) {
  *GRT_H_PARAM_TO_WORD(grt_lock_state_t*, statep) = HELD;
}

void grt_new_lock(gasnet_node_t proc, grt_new_lock_t *lock,
		  grt_lock_type_t type) {
  GRT_DEBUG_PRINT("grt_new_lock: proc=%u, lock=%x, type=%u\n", 
		  proc, lock, type);
  if (grt_id == proc) {
    new_lock_local(proc, lock, type);
  } else {
    grt_handler_state_t state = PENDING;
    size_t result_size;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, NEW_LOCK_HANDLER_ID,
					2+GRT_NUM_H_ARGS(3),
					grt_id,
					type,
					GRT_WORD_TO_H_ARG(lock),
					GRT_WORD_TO_H_ARG(&result_size),
					GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
}


void grt_lock(gasnet_node_t proc, grt_lock_t *lock, 
	      grt_lock_type_t type, grt_lock_state_t *state) {
  GRT_DEBUG_PRINT("grt_lock: proc=%u, lock=%x, type=%u, state=%x\n",
		  proc, lock, type, state);
  *state = WAITING;
  if (proc == grt_id) {
    if (lock_local(proc, lock, type, state) == SUCCEED) {
      *state = HELD;
      return;
    }
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, LOCK_HANDLER_ID, 
					GRT_NUM_H_ARGS(2)+2,
					grt_id, type,
					GRT_WORD_TO_H_ARG(lock),
					GRT_WORD_TO_H_ARG(state)));
  }
  GASNET_BLOCKUNTIL(*state != WAITING);
}

/* grt_unlock implementation */

static void new_unlock_local(grt_new_lock_t *lock) {
  if (lock->type == READ) {
    gasnet_hsl_lock(&lock->read_hsl);
    if (--lock->num_readers == 0) {
      gasnet_hsl_unlock(&lock->write_hsl);
    }
    gasnet_hsl_unlock(&lock->read_hsl);
    GRT_DEBUG_PRINT("released read lock: lock=%x\n", lock);
  } else {
    gasnet_hsl_unlock(&lock->write_hsl);
    GRT_DEBUG_PRINT("released write lock: lock=%x\n", lock);
  }
}

static void unlock_local(grt_lock_t* lock, gasnet_node_t* receiving_proc, 
			 grt_lock_state_t **receiving_state) {
  gasnet_hsl_lock(&lock_lock);
  if (lock->waiters->size > 0) {
    grt_lock_waiter_t *waiter;
    grt_list_start(lock->waiters);
    waiter = 
      (grt_lock_waiter_t*) grt_list_get_next(lock->waiters)->contents;
    if (lock->num_holders == 1 || waiter->type == READ) {
      grt_list_remove(lock->waiters);
      lock->type = waiter->type;
      *receiving_proc = waiter->proc;
      *receiving_state = waiter->state;
    }
  }
  if (!*receiving_state) {
    --lock->num_holders;
  }
  gasnet_hsl_unlock(&lock_lock);
}

void new_unlock_handler(gasnet_token_t token,
			GRT_H_PARAM(lock),
			GRT_H_PARAM(state)) {
  grt_new_lock_t *lock = GRT_H_PARAM_TO_WORD(grt_new_lock_t*, lock);
  new_unlock_local(lock);
  GASNET_Safe(gasnetc_AMReplyShortM(token, VOID_REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(1),
				    GRT_H_ARG(state)));
}

void unlock_handler(gasnet_token_t token,
		    GRT_H_PARAM(hstate_ptr),
		    GRT_H_PARAM(rproc_ptr),
		    GRT_H_PARAM(rstate_ptr),
		    GRT_H_PARAM(lock)) {
  grt_lock_t *lock = GRT_H_PARAM_TO_WORD(grt_lock_t*, lock);
  gasnet_node_t receiving_proc = 0;
  grt_lock_state_t *receiving_state = 0;
  unlock_local(lock, &receiving_proc, &receiving_state);
  GASNET_Safe(gasnetc_AMReplyShortM(token, UNLOCK_REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(4)+1,
				    GRT_H_ARG(hstate_ptr),
				    receiving_proc,
				    GRT_H_ARG(rproc_ptr),
				    GRT_WORD_TO_H_ARG(receiving_state),
				    GRT_H_ARG(rstate_ptr)));
}

void unlock_reply_handler(gasnet_token_t token,
			  GRT_H_PARAM(hstatep),
			  gasnet_handlerarg_t lproc, 
			  GRT_H_PARAM(lprocp),
			  GRT_H_PARAM(lstate),
			  GRT_H_PARAM(lstatep)) {
  *GRT_H_PARAM_TO_WORD(gasnet_node_t*, lprocp) = lproc;
  *GRT_H_PARAM_TO_WORD(grt_lock_state_t**, lstatep) =
    GRT_H_PARAM_TO_WORD(grt_lock_state_t*, lstate);
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, hstatep) = DONE;
}

void grt_new_unlock(gasnet_node_t proc, 
		    grt_new_lock_t *lock) {
  GRT_DEBUG_PRINT("grt_new_unlock: proc=%u, lock=%x\n",
		  proc, lock);
  if (proc == grt_id) {
    new_unlock_local(lock);
  } else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, NEW_UNLOCK_HANDLER_ID, 
					GRT_NUM_H_ARGS(2),
					GRT_WORD_TO_H_ARG(lock),
					GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
}

void grt_unlock(gasnet_node_t proc, grt_lock_t *lock) {
  gasnet_node_t receiving_proc = 0;
  grt_lock_state_t *receiving_state = 0;
  GRT_DEBUG_PRINT("grt_unlock: (%u, %x)\n", proc, lock);
  if (proc == grt_id) {
    unlock_local(lock, &receiving_proc, &receiving_state);
  } else {
    grt_handler_state_t handler_state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, UNLOCK_HANDLER_ID, 
					GRT_NUM_H_ARGS(4),
					GRT_WORD_TO_H_ARG(&handler_state),
					GRT_WORD_TO_H_ARG(&receiving_proc),
					GRT_WORD_TO_H_ARG(&receiving_state),
					GRT_WORD_TO_H_ARG(lock)));
    GASNET_BLOCKUNTIL(handler_state == DONE);
  }
  if (receiving_state) {
    GRT_DEBUG_PRINT("granting (%u, %x), to processor=%u, state=%x, writing %x\n",
		    proc, lock, receiving_proc, receiving_state, HELD);
    grt_write(receiving_proc, HELD, (grt_word_t*) receiving_state);
  }
}

/* grt_on implementation */

void grt_on_reply_handler(gasnet_token_t token, void *buf, 
			  size_t nbytes,
			  GRT_H_PARAM(result),
			  GRT_H_PARAM(result_size_ptr),
			  GRT_H_PARAM(state_ptr)) {
  void *result = GRT_H_PARAM_TO_WORD(void*, result);
  if (nbytes) {
    memcpy(result, buf, nbytes);
  }
  *GRT_H_PARAM_TO_WORD(size_t*, result_size_ptr) = nbytes;
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, state_ptr) = DONE;
}

static void *grt_on_thread(void *void_arg) {
  grt_on_thread_arg_t *arg = (grt_on_thread_arg_t*) void_arg;
  void *my_result_buf = malloc(arg->result_nbytes);
  size_t result_size = (arg->local_fn)(arg->arg_buf, arg->arg_nbytes,
				       my_result_buf);
  GASNET_Safe(gasnetc_AMRequestMediumM(arg->src_proc, 
				       GRT_ON_REPLY_HANDLER_ID,
				       GASNET_SAFE_NULL(void*, my_result_buf), 
				       result_size,
				       GRT_NUM_H_ARGS(3),
				       GRT_WORD_TO_H_ARG(GASNET_SAFE_NULL(void*,
									  arg->result_buf)),
				       GRT_WORD_TO_H_ARG(arg->result_size_ptr),
				       GRT_WORD_TO_H_ARG(arg->state)));
  free(arg->arg_buf);
  free(my_result_buf);
  free(arg);
  return 0;
}

void *thread_work(void *arg) {
  grt_thread_t *thread = (grt_thread_t*) arg;
  //pthread_mutex_lock(&thread->mutex);
  while (1) {
    if (thread->proceed == GRT_TRUE) {
      thread->proceed = GRT_FALSE;
      grt_on_thread(thread->arg);
      gasnet_hsl_lock(&thread_list_hsl);
      //GRT_DEBUG_PRINT("thread_work: adding thread to list\n");
      linkedlist_add(&thread_list, thread);
      gasnet_hsl_unlock(&thread_list_hsl);
    } else {
      //GRT_DEBUG_PRINT("thread_work: cond_wait\n");
      pthread_cond_wait(&thread->cond, &thread->mutex);
    }
  }
  return 0;
}

void grt_on_handler_work_pthread(gasnet_token_t token, 
				 void *buf, size_t nbytes, 
				 gasnet_node_t src_proc, 
				 void *result_buf,
				 size_t result_nbytes,
				 size_t *result_size_ptr,
				 void *state_ptr,
				 grt_on_local_fn_t local_fn) {
  GRT_DEBUG_PRINT("grt_on_handler_work_pthread\n");
  grt_on_thread_arg_t *arg = 
    (grt_on_thread_arg_t*) malloc(sizeof(grt_on_thread_arg_t));
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
  arg->result_nbytes = result_nbytes;
  arg->result_size_ptr = result_size_ptr;
  arg->state = state_ptr;
  gasnet_hsl_lock(&thread_list_hsl);
  grt_thread_t *thread = (grt_thread_t*) linkedlist_remove(&thread_list, 0);
  gasnet_hsl_unlock(&thread_list_hsl);
  if (!thread) {
    GRT_DEBUG_PRINT("Creating new thread\n");
    pthread_t pthread;
    thread = (grt_thread_t*) malloc(sizeof(grt_thread_t));
    thread->arg = arg;
    thread->proceed = GRT_TRUE;
    pthread_cond_init(&thread->cond, NULL);
    pthread_mutex_init(&thread->mutex, NULL);
#if 0
    gasnet_hsl_lock(&thread_list_hsl);
    spawncount++;
    gasnet_hsl_unlock(&thread_list_hsl);
#endif
    int retry_count = 0;
    do {
      errno = pthread_create(&pthread, NULL, thread_work, thread);
      if (errno == 0) {
	break;
      }
      sched_yield();
      ++retry_count;
    } while (retry_count < 10);
    if (retry_count == 10) {
#if 0
      printf("spawncount=%d\n", spawncount);
#endif
      perror(0);
      fflush(stderr);
      fflush(stdout);
      gasnet_exit(errno);
    }
  } else {
    GRT_DEBUG_PRINT("Waking up thread\n");
    thread->arg = arg;
    thread->proceed = GRT_TRUE;
#if GRT_DEBUG
    size_t signal_count = 1;
#endif
    do {
      pthread_cond_signal(&thread->cond);
      sched_yield();
#if GRT_DEBUG
      ++signal_count;
#endif
    } while (thread->proceed == GRT_TRUE);
#if 0
    GRT_DEBUG_PRINT("signal_count=%u\n", signal_count);
#endif
  }
}

void grt_on_handler_work(gasnet_token_t token, void *buf, size_t nbytes, 
			 gasnet_node_t src_proc,
			 void *result_buf, size_t result_nbytes, 
			 size_t *result_size_ptr,
			 void *state_ptr, grt_on_local_fn_t local_fn) {
  void *my_result_buf = 0;
  size_t result_size;
  if (result_nbytes) {
    my_result_buf = malloc(result_nbytes);
  }
  result_size = (local_fn)(buf, nbytes, my_result_buf);
  GASNET_Safe(gasnetc_AMReplyMediumM(token,
				     GRT_ON_REPLY_HANDLER_ID,
				     GASNET_SAFE_NULL(void*, my_result_buf), 
				     result_size,
				     GRT_NUM_H_ARGS(3),
				     GRT_WORD_TO_H_ARG(result_buf),
				     GRT_WORD_TO_H_ARG(result_size_ptr),
				     GRT_WORD_TO_H_ARG(state_ptr)));
  free(my_result_buf);
}

size_t grt_on(gasnet_node_t proc, unsigned handler_id, 
	      grt_on_local_fn_t local_fn, void *arg_buf, 
	      size_t arg_nbytes, void *result_buf, 
	      size_t result_buf_size) {
  size_t result_size;
  arg_buf = GASNET_SAFE_NULL(void*, arg_buf);
  result_buf = GASNET_SAFE_NULL(void*, result_buf);
  if (grt_id == proc) {
    result_size = local_fn(arg_buf, arg_nbytes, result_buf);
  } else {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestMediumM(proc, handler_id,
					 arg_buf, arg_nbytes,
					 GRT_NUM_H_ARGS(3)+2,
					 grt_id,
					 GRT_WORD_TO_H_ARG(result_buf),
					 result_buf_size,
					 GRT_WORD_TO_H_ARG(&result_size),
					 GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  return result_size;
}

void grt_on_nb(gasnet_node_t proc, unsigned handler_id, 
	       grt_on_local_fn_t local_fn, void *arg_buf, 
	       size_t arg_nbytes, void *result_buf, 
	       size_t result_buf_size, size_t *result_size,
	       grt_handler_state_t *state) {
  arg_buf = GASNET_SAFE_NULL(void*, arg_buf);
  result_buf = GASNET_SAFE_NULL(void*, result_buf);
  if (grt_id == proc) {
    *result_size = local_fn(arg_buf, arg_nbytes, result_buf);
    *state = DONE;
  } else {
    GASNET_Safe(gasnetc_AMRequestMediumM(proc, handler_id,
					 arg_buf, arg_nbytes,
					 GRT_NUM_H_ARGS(3)+2,
					 grt_id,
					 GRT_WORD_TO_H_ARG(result_buf),
					 result_buf_size,
					 GRT_WORD_TO_H_ARG(result_size),
					 GRT_WORD_TO_H_ARG(state)));
  }
}

/* Generic AM Reply Handlers */

/* copy result to its destination; set state flag */
void reply_handler(gasnet_token_t token, 
		   GRT_H_PARAM(statep),
		   GRT_H_PARAM(result),
		   GRT_H_PARAM(resultp)) {
  *GRT_H_PARAM_TO_WORD(grt_word_t*, resultp) =
    GRT_H_PARAM_TO_WORD(grt_word_t, result);
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, statep) = DONE;
}

/* Set state flag only */
void void_reply_handler(gasnet_token_t token, 
			GRT_H_PARAM(statep)) {
  gasnett_local_mb();
  *GRT_H_PARAM_TO_WORD(grt_handler_state_t*, statep) = DONE;
}

