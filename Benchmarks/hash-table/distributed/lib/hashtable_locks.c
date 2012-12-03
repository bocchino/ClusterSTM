#include "grt.h"
#include "grt_debug.h"
#include "hashtable.h"

#define NB 1

node_t*** table;
grt_lock_t **table_locks;
grt_lock_state_t *state;
unsigned table_size;
unsigned local_size;

hash_t compute_hash(grt_word_t val) {
  hash_t hash;
  hash.proc = val % grt_num_procs;
  hash.offset = (val / grt_num_procs) % local_size;
  return hash;
}

/* Make a new distributed hash table */
void hashtable_create(unsigned size) {
  unsigned i, j;
  state = grt_alloc(grt_id, sizeof(grt_lock_state_t));
  if (grt_id == 0) {
    if (size < grt_num_procs) {
      fprintf(stderr, "Table size must be >= grt_num_procs!\n");
      abort();
    }
  }
  table_size = size;
  local_size = size / grt_num_procs;
  table = (node_t***) malloc(grt_num_procs * sizeof(node_t**));
  table_locks = malloc(grt_num_procs * sizeof(grt_lock_t*));
  for (i = 0; i < grt_num_procs; ++i) {
    table[i] = 
      (node_t**) grt_all_alloc(i, local_size * sizeof(node_t*));
    table_locks[i] = grt_all_lock_alloc(i,local_size);
  }
}

/* Destroy the hashtable */
void hashtable_destroy() {
  unsigned i;
  for (i = 0; i < local_size; ++i) {
    node_t* node = table[grt_id][i];
    while (node) {
      node_t *next = node->next;
      grt_free(grt_id, node);
      node = next;
    }
  }
  grt_lock_free(grt_id, table_locks[grt_id], local_size);
  free(table);
  free(table_locks);
}

static node_t* find_node(gasnet_node_t proc, grt_word_t val, node_t* p) {
  while (p) {
    if (val == grt_read(proc, (void*) &p->val))
      return p;
    p = (node_t*) grt_read(proc, (void*) &p->next);
  }
  return 0;
}

#define READ_LOCK \
  grt_lock_t *lock = &table_locks[hash.proc][hash.offset]; \
  grt_lock(hash.proc, lock, READ, state)
#define WRITE_LOCK \
  grt_lock_t *lock = &table_locks[hash.proc][hash.offset]; \
  grt_lock(hash.proc, lock, WRITE, state)
#define UNLOCK \
  grt_unlock(hash.proc, lock);

grt_handler_state_t states[3];

grt_bool_t hashtable_insert(grt_word_t val) {
  unsigned i;
  hash_t hash = compute_hash(val);
  WRITE_LOCK;
  node_t **head = &table[hash.proc][hash.offset];
  node_t *p = (node_t*) grt_read(hash.proc, (grt_word_t*) head);
  node_t *q = find_node(hash.proc, val, p);
  if (q) {
    UNLOCK;
    return GRT_TRUE;
  }
  q = (node_t*) grt_alloc(hash.proc, sizeof(node_t));
#if NB
  gasnet_put_nbi(hash.proc, (grt_word_t*) &q->next, &p,
		 sizeof(grt_word_t));
  gasnet_put_nbi(hash.proc, &q->val, &val, sizeof(grt_word_t));
  gasnet_put_nbi(hash.proc, (grt_word_t*) head, (grt_word_t*) &q,
		 sizeof(grt_word_t));
  gasnet_wait_syncnbi_puts();
#else
  grt_write(hash.proc, (grt_word_t) p, (grt_word_t*) &q->next);
  grt_write(hash.proc, val, &q->val);
  grt_write(hash.proc, (grt_word_t) q, (grt_word_t*) head);
#endif
  UNLOCK;
  return GRT_FALSE;
}

grt_bool_t hashtable_find(grt_word_t val) {
  hash_t hash = compute_hash(val);
  READ_LOCK;
  node_t *p = (node_t*) grt_read(hash.proc, (void*) &table[hash.proc][hash.offset]);
  grt_bool_t result =  find_node(hash.proc, val, p) == 0 ? GRT_FALSE : GRT_TRUE;
  UNLOCK;
  return result;
}

void hashtable_init(int argc, char **argv) {
  grt_init(argc, argv);
}

int __hashtable_exit(int ret_val) {
  return grt_exit(ret_val);
}

