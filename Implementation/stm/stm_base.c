#include "stm.h"
#include "grt_map.h"
#include "grt_list.h"

/* Map of maps mapping (processor,addr) to last value seen (read,
   written, or both) */
grt_map_t *cache;
/* List of all entries in the cache */
grt_list_t *cache_entry_list;
/* Set of version offsets we have opened for read. */
grt_map_t *opened_version_set;
/* List of cache locations we have opened for read, modulo version
   equivalence.  If we have opened n >= 1 locations mapping to the
   same version info for read, exactly one is guaranteed to be on this
   list. */
grt_list_t *read_list;
/* List of cache entries we have written.  At commit, we run through
   this list, look up the value in the cache, and commit the write. */
grt_list_t *write_list;
/* Set of version offsets we have tried to acquire. */
grt_map_t *acquired_set;
/* List of cache locations we have tried to acquire.  If we have
   written n >= 1 locations mapping to the same version, exactly one
   is guaranteed to be on this list if commit is successful. */
grt_list_t *acquired_list;
/* List of memory allocations we made inside the transaction, so we
   can destroy them on abort */
grt_list_t *alloc_log;
/* Base address for STM versions; a version location is given by its
   offset plus this address */
grt_word_t *stm_version_base;
/* List of all maps we create, so we can destroy them on cleanup */
grt_list_t *local_map_list;
/* Lock for ensuring that check-and-acquire operation is atomic */
gasnet_hsl_t acquire_lock = GASNET_HSL_INITIALIZER;
/* For longjmp at transaction abort */
jmp_buf env;
/* Count to keep track of whether we are inside a nested transaction.
   We currently flatten nested transactions, so we treat a nested
   (start,commit) pair as a no-op. */
unsigned nesting_count = 0;

/* An entry in the software cache */
typedef struct {
  gasnet_node_t proc;
  grt_word_t *addr;
  grt_word_t value;
  grt_word_t version;
  grt_word_t new_version;
  grt_handler_state_t state;
  grt_bool_t read;
  grt_bool_t written;
  grt_status_t status;
} cache_entry_t;

/* Macros for manipulating ownership bit of the version */

#define is_owned(__word) (__word & 1LL)
#define make_owned(__word) (__word | 1LL)
#define make_unowned(__word) (__word & ~1LL)

/* Offset from the STM version base address to get the version info
   for a GRT word.  This value is valid across processors, and depends
   only on the address. */

#define version_offset(addr) ((((grt_word_t) addr) >> STM_SHARE_POWER)	\
			      >> GRT_LOG_WORD_SIZE)

/* Put the version offset together with the STM version base to get
   the address of the version info */

#define version_addr(addr) ((grt_word_t*)				\
			    ((grt_word_t) stm_version_base)		\
			    + version_offset(addr))

/* Make a new cache entry.  We use malloc for now, but we could also
   keep a list of preallocated entries. */

static cache_entry_t *new_cache_entry() {
  cache_entry_t *entry = (cache_entry_t*) malloc(sizeof(cache_entry_t));
  grt_list_append(cache_entry_list, (grt_word_t) entry);
  return entry;
}

static grt_map_t *new_local_map() {
  grt_map_t *local_map = grt_map_create();
  grt_list_append(local_map_list, (grt_word_t) local_map);
  return local_map;
}

/* stm_init implementation */

static gasnet_handlerentry_t stm_entry_table[STM_TABLE_SIZE] = {
  GRT_TABLE,
  STM_TABLE_SEGMENT
};

void stm_init(int argc, char **argv) {
  /* Reserve the top S / (S+1) of the GASNet heap for metadata, where
     S is the number of GRT words that share the same STM metadata */
  grt_heap_size = 
    ((long long) GASNET_HEAP_SIZE * (1 << STM_SHARE_POWER)) / 
    (1 + (1 << STM_SHARE_POWER));
  grt_heap_size = grt_align_down(grt_heap_size,sizeof(grt_word_t));
  /* Set up the handler table */
  if (!entry_table) {
    entry_table = stm_entry_table;
    table_size = STM_TABLE_SIZE;
  }
  /* Call grt init */
  grt_init(argc, argv);
  stm_version_base = (grt_word_t*) 
    ((grt_word_t) grt_heap_base + grt_heap_size
     - ((((grt_word_t) grt_heap_base) >> STM_SHARE_POWER) & GRT_ALIGN_MASK));
}

void __stm_start() {
  read_list = grt_list_create();
  write_list = grt_list_create();
  alloc_log = grt_list_create();
  acquired_set = grt_map_create();
  opened_version_set = grt_map_create();
  acquired_list = grt_list_create();
  cache = grt_map_create();
  cache_entry_list = grt_list_create();
  local_map_list = grt_list_create();
}

/* stm_open_for_read implementation */

void open_for_read_handler(gasnet_token_t token,
			   GRT_H_PARAM(entry),
			   GRT_H_PARAM(addr)) {
  cache_entry_t *entry = GRT_H_PARAM_TO_WORD(cache_entry_t*, entry);
  grt_word_t *addr = GRT_H_PARAM_TO_WORD(grt_word_t*, addr);
  grt_word_t version = *version_addr(addr);
  grt_word_t val = *addr;
  GASNET_Safe(gasnetc_AMReplyShortM(token, OPEN_FOR_READ_REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(entry),
				    GRT_WORD_TO_H_ARG(version),
				    GRT_WORD_TO_H_ARG(val)));
}

void open_for_read_reply_handler(gasnet_token_t token,
				 GRT_H_PARAM(entry),
				 GRT_H_PARAM(version),
				 GRT_H_PARAM(val)) {
  cache_entry_t *entry = GRT_H_PARAM_TO_WORD(cache_entry_t*, entry);
  entry->version = GRT_H_PARAM_TO_WORD(grt_word_t, version);
  entry->value = GRT_H_PARAM_TO_WORD(grt_word_t, val);
  entry->state = DONE;
}

void stm_open_for_read(gasnet_node_t proc,
		       grt_word_t *addr) {
  grt_map_t *local_map = 0;
  cache_entry_t *cache_entry = 0;

  STM_DEBUG(printf("opening (%u,%x) for read, vaddr=%x\n",
		   proc, addr, version_addr(addr)));

  /* If it's in the cache, and we have read it or written it, we don't
     need to go to memory. */
  if (grt_map_find(cache, proc, (grt_word_t*) &local_map))
    if (grt_map_find(local_map, (grt_word_t) addr, (grt_word_t*) &cache_entry))
      if (cache_entry->read || cache_entry->written)
	return;

  /* Add the cache entry if necessary */
  if (!local_map) {
    local_map = new_local_map();
    grt_map_insert(cache, (grt_word_t) proc, (grt_word_t) local_map);
  }
  if (!cache_entry) {
    cache_entry = new_cache_entry();
    cache_entry->proc = proc;
    cache_entry->addr = addr;
    cache_entry->read = GRT_TRUE;
    cache_entry->written = GRT_FALSE;
    grt_map_insert(local_map, (grt_word_t) addr, (grt_word_t) cache_entry);
  }

  /* Add the read log entry, filtering out addresses whose versions
     are already represented */
  grt_bool_t found = grt_map_find(opened_version_set, proc, 
				  (grt_word_t*) &local_map);
  if (found) {
    found = grt_map_find(local_map, (grt_word_t) version_addr(addr), 0);
  } else {
    local_map = new_local_map();
    grt_map_insert(opened_version_set, (grt_word_t) proc, 
		   (grt_word_t) local_map);
  }
  if (!found) {
    grt_map_insert(local_map, (grt_word_t) version_addr(addr), 0);
    grt_list_append(read_list, (grt_word_t) cache_entry);
  }

  /* Get version, ownership status, and value into cache */
  if (proc == grt_id) {
    cache_entry->version = *version_addr(addr);
    cache_entry->value = *addr;
    cache_entry->state = DONE;
  } else {
    cache_entry->state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, OPEN_FOR_READ_HANDLER_ID,
					GRT_NUM_H_ARGS(2),
					GRT_WORD_TO_H_ARG(cache_entry),
					GRT_WORD_TO_H_ARG(addr)));
  }
}

/* stm_read implementation */

grt_status_t __stm_read(gasnet_node_t proc, grt_word_t *addr, 
			grt_word_t *result) {
  grt_map_t *local_map;
  cache_entry_t *cache_entry;
  if (grt_map_find(cache, proc, (grt_word_t*) &local_map)) {
    if (grt_map_find(local_map, (grt_word_t) addr, 
		     (grt_word_t*) &cache_entry)) {
      /* Make sure the version and value are in the cache */
      GASNET_BLOCKUNTIL(cache_entry->state == DONE);
      /* If this is a location we pulled from memory, and the version
	 says owned, the value is possibly inconsistent, so abort */
      if (cache_entry->read == GRT_TRUE && is_owned(cache_entry->version)) {
	STM_DEBUG(printf("RAW conflict (read owned loc) at (%u,%x), vers=%x!\n", 
			 cache_entry->proc, cache_entry->addr,
			 version_addr(cache_entry->addr)));
	stm_abort();
	return FAIL;
      }
      grt_word_t value = cache_entry->value;
      STM_DEBUG(printf("reading %x from (%d,%x)\n", value, 
		       proc, addr));
      *result = value;
      return SUCCEED;
    }
  }
  fprintf(stderr, "stm_read: location is not in cache!\n");
  abort();
}

void stm_open_for_write(gasnet_node_t proc,
			grt_word_t *addr) {
  /* This is a noop in the late acquire implementation */
}

void stm_write(gasnet_node_t proc, grt_word_t val,
	       grt_word_t *addr) {
  grt_map_t *local_map = 0;
  grt_bool_t found;
  cache_entry_t *cache_entry = 0;

  /* Look in the cache */
  grt_map_find(cache, proc, (grt_word_t*) &local_map);
  if (!local_map) {
    local_map = new_local_map();
    grt_map_insert(cache, (grt_word_t) proc, 
		   (grt_word_t) local_map);
  } else {
    grt_map_find(local_map, (grt_word_t) addr, 
		 (grt_word_t*) &cache_entry);
  }

  /* If there's no entry, make one */
  if (!cache_entry) {
    cache_entry = new_cache_entry();
    cache_entry->proc = proc;
    cache_entry->addr = addr;
    cache_entry->read = GRT_FALSE;
    cache_entry->written = GRT_FALSE;
    cache_entry->state = DONE;
    grt_map_insert(local_map, (grt_word_t) addr, (grt_word_t) cache_entry);
  } else {
    /* Make sure there are no pending reads to clobber our write! */
    GASNET_BLOCKUNTIL(cache_entry->state == DONE);
  }

  /* Write the value */
  cache_entry->value = val;

  /* If this is the first write, update the write log */
  if (cache_entry->written == GRT_FALSE) {
    grt_list_append(write_list, (grt_word_t) cache_entry);
    cache_entry->written = GRT_TRUE;
  }
}

/* stm_alloc implementation */

void *stm_alloc(gasnet_node_t proc, unsigned nbytes) {
  void *result = grt_alloc(proc, nbytes);
  gasnet_memset(proc, version_addr(result), 0, nbytes >> STM_SHARE_POWER);
  /* Log this allocation, so we can undo it on abort */
  if (nesting_count > 0) {
    /* Make a new global address.  We use malloc for now, but we could
       also keep a list of preallocated entries. */
    grt_global_addr_t *global_addr = malloc(sizeof(grt_global_addr_t));
    grt_list_append(alloc_log, (grt_word_t) global_addr);
    global_addr->proc = proc;
    global_addr->addr = result;
  }
  STM_DEBUG(printf("allocated (%u,%x)\n", proc, result));
  return result;
}

void *stm_all_alloc(gasnet_node_t proc, unsigned nbytes) {
  if (nesting_count > 1) {
    fprintf(stderr, "stm_all_alloc should not be called from inside a transaction!\n");
    abort();
  }
  void *result = grt_all_alloc(proc, nbytes);
  if (proc == grt_id) {
    gasnet_memset(proc, version_addr(result), 0, nbytes);
    STM_DEBUG(printf("allocated (%u,%x) to all processors\n", 
		     proc, result));
  }
  BARRIER();
  return result;
}

/* stm_free implementation */

void stm_free(gasnet_node_t proc, void *addr) {
  STM_DEBUG(printf("freed (%u,%x)\n", proc, addr));
  grt_free(proc, addr);
}

/* stm_acquire implementation: Attempt to acquire ownership of a word
   and return status.  This implementation will deadlock if the same
   transaction tries to acquire an owned location a second
   time. Therefore, we track the locks we are holding in local data
   structures, and make sure we never do that! */

static grt_status_t acquire_local(grt_word_t *addr) {
  grt_status_t result;
  grt_word_t *v_addr = version_addr(addr);
  grt_word_t old_version = *v_addr;
  gasnet_hsl_lock(&acquire_lock);
  if (is_owned(old_version)) {
    STM_DEBUG(printf("failed to acquire (%u,%x), vaddr=%x, vers=%x\n", 
		     grt_id, addr, v_addr, old_version));
    result = FAIL;
  } else {
    STM_DEBUG(printf("acquired (%u,%x), vaddr=%x, vers=%x\n", 
		     grt_id, addr, v_addr, old_version));
    *v_addr = make_owned(old_version);
    result = SUCCEED;
  }
  gasnet_hsl_unlock(&acquire_lock);
  return result;
}

void acquire_handler(gasnet_token_t token,
		     GRT_H_PARAM(addr),
		     GRT_H_PARAM(state_ptr),
		     GRT_H_PARAM(resultp)) {
  grt_word_t *addr = GRT_H_PARAM_TO_WORD(grt_word_t*, addr);
  grt_word_t result = acquire_local(addr);
  GASNET_Safe(gasnetc_AMReplyShortM(token, REPLY_HANDLER_ID,
				    GRT_NUM_H_ARGS(3),
				    GRT_H_ARG(state_ptr),
				    GRT_WORD_TO_H_ARG(result),
				    GRT_H_ARG(resultp)));
}

static void stm_acquire(cache_entry_t *entry) {
  gasnet_node_t proc = entry->proc;
  grt_word_t* addr = entry->addr;
  STM_DEBUG(printf("trying to acquire (%u,%x), vaddr=%x\n", 
		   proc, addr, version_addr(addr)));

  grt_bool_t found = GRT_FALSE;
  grt_map_t *local_map;
  if (!grt_map_find(acquired_set, (grt_word_t) proc, 
		    (grt_word_t*) &local_map)) {
    local_map = new_local_map();
    grt_map_insert(acquired_set, (grt_word_t) proc, 
		   (grt_word_t) local_map);
  } else {
    if (grt_map_find(local_map, 
		     (grt_word_t) version_addr(addr), 0)) {
      entry->status = SUCCEED;
      return;
    }
  }
  grt_list_append(acquired_list, (grt_word_t) entry);
  grt_map_insert(local_map, (grt_word_t) version_addr(addr), 0);
  if (proc == grt_id) {
    entry->status = acquire_local(addr);
  } else {
    entry->state = PENDING;
    GASNET_Safe(gasnetc_AMRequestShortM(proc, ACQUIRE_HANDLER_ID, 
					GRT_NUM_H_ARGS(3),
					GRT_WORD_TO_H_ARG(addr),
					GRT_WORD_TO_H_ARG(&entry->state),
					GRT_WORD_TO_H_ARG(&entry->status)));
  }
}

/* stm release implementation: release ownership of a word */

void release_handler(gasnet_token_t token,
		     GRT_H_PARAM(addr)) {
  grt_word_t *v_addr = 
    version_addr(GRT_H_PARAM_TO_WORD(grt_word_t*, addr));
  *v_addr = make_unowned(*v_addr);
}

static void stm_release(gasnet_node_t proc,
			grt_word_t *addr) {
  if (proc == grt_id) {
    grt_word_t *v_addr = version_addr(addr);
    *v_addr = make_unowned(*v_addr);
  } else {
    GASNET_Safe(gasnetc_AMRequestShortM(proc, RELEASE_HANDLER_ID,
					GRT_NUM_H_ARGS(1),
					GRT_WORD_TO_H_ARG(addr)));
    STM_DEBUG(printf("released (%u,%x)\n", proc, addr));
  }
}

/* commit_write implementation */

static void commit_write_local(grt_word_t val, 
			       grt_word_t *addr) {
  grt_word_t *v_addr = version_addr(addr);
  *addr = val;
  *v_addr = make_unowned(*v_addr+2);
  STM_DEBUG(printf("writing val=%x, vaddr=%x, vers=%x to (%u,%x)\n", 
		   val, v_addr, *v_addr, grt_id, addr));
}

void commit_write_handler(gasnet_token_t token,
			  GRT_H_PARAM(val),
			  GRT_H_PARAM(addr)) {
  grt_word_t val = GRT_H_PARAM_TO_WORD(grt_word_t, val);
  grt_word_t *addr = GRT_H_PARAM_TO_WORD(grt_word_t*, addr);
  grt_word_t *v_addr = version_addr(addr);
  commit_write_local(val, addr);
}

static void commit_write(gasnet_node_t proc, grt_word_t val,
			 grt_word_t* addr) {
  if (proc == grt_id) {
    commit_write_local(val, addr);
  } else {
    STM_DEBUG(printf("asking processor %u to write val=%x to %x\n",
		     proc,val,addr));
    GASNET_Safe(gasnetc_AMRequestShortM(proc, COMMIT_WRITE_HANDLER_ID,
					GRT_NUM_H_ARGS(2),
					GRT_WORD_TO_H_ARG(val),
					GRT_WORD_TO_H_ARG(addr)));
  }
}

/* stm_commit implementation */

grt_status_t __stm_commit() {
  grt_global_addr_t *global_addr;
  cache_entry_t* cache_entry;
  grt_map_t *local_map;
  grt_word_t val;
  cache_entry_t *entry;

  /* Fire off all the acquire requests */
  grt_list_foreach(cache_entry_t*, write_list, entry) {
    stm_acquire(entry);
  }

  /* Then block for each one and check whether it succeeded */
  grt_list_foreach(cache_entry_t*, acquired_list, entry) {
    GASNET_BLOCKUNTIL(entry->state == DONE);
    if (entry->status == FAIL) {
      STM_DEBUG(printf("WAW conflict (written val owned) at (%u,%x), vaddr=%x!\n", 
		       entry->proc, entry->addr, 
		       version_addr(entry->addr)));
      stm_abort();
      return FAIL;
    }
  }

  /* Fire off all the version read requests */
  grt_list_foreach(cache_entry_t*, read_list, entry) {
    /* Make sure any pending open for reads are complete, so we get a
       valid version if a location was opened but never actually
       read */
    GASNET_BLOCKUNTIL(entry->state == DONE);
    grt_read_nb(entry->proc, &entry->new_version, 
		version_addr(entry->addr), &entry->state);
  }

  /* Then block for each one and check consistency */
  grt_list_foreach(cache_entry_t*, read_list, entry) {
    GASNET_BLOCKUNTIL(entry->state == DONE);
    grt_word_t new_version = entry->new_version;
    grt_map_t *local_map;
    grt_bool_t found = 
      grt_map_find(acquired_set, (grt_word_t) entry->proc, 
		   (grt_word_t*) &local_map);
    if (found)
      found = grt_map_find(local_map, 
			   (grt_word_t) version_addr(entry->addr), 0);
    if (!found && is_owned(new_version)) {
      STM_DEBUG(printf("RAW conflict (read val owned) at (%u,%x), vaddr=%x!\n", 
		       entry->proc, entry->addr, version_addr(entry->addr)));
      stm_abort();
      return FAIL;
    }
    if (make_unowned(new_version) != entry->version) {
      STM_DEBUG(printf("WAR conflict at (%u,%x), vaddr=%x!\n", 
		       entry->proc, entry->addr, version_addr(entry->addr)));
      stm_abort();
      return FAIL;
    }
  }

  /* If all that succeeded, commit the writes */
  grt_list_foreach(cache_entry_t*, write_list, cache_entry) {
    gasnet_node_t proc = cache_entry->proc;
    grt_word_t *addr = cache_entry->addr;
    val = cache_entry->value;
    commit_write(proc, val, addr);
  }

  stm_cleanup();
  return SUCCEED;
}

/* stm_abort implementation */

static void stm_abort() {
  grt_global_addr_t *global_addr;
  cache_entry_t *entry;
  /* Release all variables we acquired */
  grt_list_foreach(cache_entry_t*, acquired_list, entry) {
    GASNET_BLOCKUNTIL(entry->state == DONE);
    if (entry->status == SUCCEED)
      stm_release(entry->proc, entry->addr);
  }
  /* Undo all allocations we did */
  grt_list_foreach(grt_global_addr_t*, alloc_log, global_addr) {
    stm_free(global_addr->proc, global_addr->addr);
  }
  /* Normal cleanup on commit */
  // MAY WANT TO DO BACKOFF HERE
  stm_cleanup();
}

/* stm_cleanup implementation: clean up on commit or abort */

static void stm_cleanup() {
  grt_map_t *local_map;
  grt_global_addr_t *global_addr;
  cache_entry_t *cache_entry;
  grt_list_destroy(read_list);
  grt_list_destroy(write_list);
  grt_list_foreach(grt_global_addr_t*, alloc_log, global_addr) {
    free(global_addr);
  }
  grt_list_destroy(alloc_log);
  /* Note that maps inside these maps have been appended to
     local_map_list, and so are destroyed below */
  grt_map_destroy(acquired_set);
  grt_map_destroy(opened_version_set);
  grt_map_destroy(cache);
  grt_list_foreach(grt_map_t*, local_map_list, local_map) {
    grt_map_destroy(local_map);
  }
  grt_list_destroy(local_map_list);
  grt_list_foreach(cache_entry_t*, cache_entry_list, cache_entry) {
    /* Ensure any pending open for reads have completed */
    GASNET_BLOCKUNTIL(cache_entry->state == DONE);
    free(cache_entry);
  }
  grt_list_destroy(cache_entry_list);
  grt_list_destroy(acquired_list);
  nesting_count = 0;
}

