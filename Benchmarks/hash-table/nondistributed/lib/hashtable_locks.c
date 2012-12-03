#include "grt.h"
#include "hashtable.h"

node_t** table;
grt_lock_t *table_locks;
gasnet_node_t table_proc;
extern unsigned alloc_offset;
grt_lock_state_t *state;
unsigned table_size;

/* Make a new hashtable on the specified processor */
void hashtable_create(gasnet_node_t proc, unsigned size) {
  unsigned i;
  table_proc = proc;
  table_size = size;
  state = grt_alloc(grt_id, sizeof(grt_lock_state_t));
  table = 
    (node_t**) grt_all_alloc(proc, table_size * sizeof(node_t*));
  table_locks = grt_all_lock_alloc(proc, table_size);
}

/* Free the hashtable created on table_proc */
void hashtable_destroy() {
  if (grt_id == table_proc) {
    unsigned i;
    for (i = 0; i < table_size; ++i) {
      if (table[i]) grt_free(table_proc, table[i]);
    }
    grt_free(table_proc, table);
    grt_lock_free(table_proc, table_locks, table_size);
  }
}

unsigned compute_hash(grt_word_t val) {
  return val % table_size;
}

static node_t* find_node(grt_word_t val, node_t* p) {
  while (p) {
    if (val == grt_read(table_proc, (void*) &p->val))
      return p;
    p = (node_t*) grt_read(table_proc, (void*) &p->next);
  }
  return 0;
}

grt_bool_t hashtable_insert(grt_word_t val) {
  node_t *p, *q;
  unsigned bucket = compute_hash(val);
  grt_lock_t *lock = &table_locks[bucket];
  grt_lock(table_proc, lock, WRITE, state);
  p = (node_t*) grt_read(table_proc, (void*) &table[bucket]);
  if (find_node(val, p)) {
    grt_unlock(table_proc, lock);
    return GRT_TRUE;
  }
  q = (node_t*) grt_alloc(table_proc, sizeof(node_t));
  grt_write(table_proc, (grt_word_t) p, (void*) &q->next);
  grt_write(table_proc, val, &q->val);
  grt_write(table_proc, (grt_word_t) q, (void*) &table[bucket]);
  grt_unlock(table_proc, lock);
  return GRT_FALSE;
}

grt_bool_t hashtable_find(grt_word_t val) {
  unsigned bucket = compute_hash(val);
  node_t *p = (node_t*) grt_read(table_proc, (void*) &table[bucket]);
  return find_node(val, p) ? GRT_TRUE : GRT_FALSE;
}

