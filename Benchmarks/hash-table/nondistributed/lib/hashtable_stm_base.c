#include "stm.h"
#include "hashtable.h"

node_t** table;
unsigned table_size;
gasnet_node_t table_proc;

/* Make a new hashtable on the specified processor */
void hashtable_create(gasnet_node_t proc, unsigned size) {
  unsigned i;
  table_proc = proc;
  table_size = size;
  table = (node_t**) stm_all_alloc(proc, table_size * sizeof(node_t*));
}

/* Free the hashtable created on table_proc */
void hashtable_destroy() {
  if (grt_id == table_proc) {
    unsigned i;
    for (i = 0; i < table_size; ++i) {
      if (table[i]) stm_free(table_proc, table[i]);
    }
    stm_free(table_proc, table);
  }
}

unsigned compute_hash(grt_word_t val) {
  return val % table_size;
}

static node_t* find_node_stm(grt_word_t val, node_t* p) {
  while(p) {
    grt_word_t* loc = (grt_word_t*) &p->val;
    grt_word_t tmp;
    stm_open_for_read(table_proc, loc);
    stm_read(table_proc, loc, &tmp);
    if (val == tmp) {
      return p;
    }
    loc = (grt_word_t*) &p->next;
    stm_open_for_read(table_proc, loc);
    stm_read(table_proc, loc, (grt_word_t*) &p);
  }
  return 0;
}

grt_bool_t hashtable_insert(grt_word_t val) {
  unsigned bucket = compute_hash(val);
  node_t *p, *q;
  stm_start();
  grt_word_t *loc = (grt_word_t*) &table[bucket];
  stm_open_for_read(table_proc, loc);
  stm_read(table_proc, loc, (grt_word_t*) &p);
  if (find_node_stm(val, p)) {
    stm_commit();
    return GRT_TRUE;
  }
  q = (node_t*) stm_alloc(table_proc, sizeof(node_t));
  loc = (grt_word_t*) &q->next;
  stm_open_for_write(table_proc, loc);
  stm_write(table_proc, (grt_word_t) p, loc);
  loc = (grt_word_t*) &q->val;
  stm_open_for_write(table_proc, loc);
  stm_write(table_proc, val, loc);
  loc = (grt_word_t*) &table[bucket];
  stm_open_for_write(table_proc, loc);
  stm_write(table_proc, (grt_word_t) q, loc);
  stm_commit();
  return GRT_FALSE;
}

grt_bool_t hashtable_find(grt_word_t val) {
  unsigned bucket = compute_hash(val);
  grt_word_t *loc = (grt_word_t*) &table[bucket];
  node_t *p;
  stm_start();
  stm_open_for_read(table_proc, loc);
  stm_read(table_proc, loc, (grt_word_t*) &p);
  grt_bool_t result = find_node_stm(val, p) == 0 ? GRT_FALSE : GRT_TRUE;
  stm_commit();
  return result;
}


