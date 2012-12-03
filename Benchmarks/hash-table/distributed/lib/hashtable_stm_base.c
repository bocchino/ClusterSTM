#include "stm.h"
#include "hashtable.h"

node_t*** table;
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
  if (grt_id == 0) {
    if (grt_num_procs > size) {
      fprintf(stderr, "size must be >= grt_num_procs!\n");
      abort();
    }
  }
  table_size = size;
  local_size = size / grt_num_procs;
  table = (node_t***) malloc(grt_num_procs * sizeof(node_t**));
  for (i = 0; i < grt_num_procs; ++i) {
    table[i] = 
      (node_t**) stm_all_alloc(i, local_size * sizeof(node_t*));
  }
}

/* Destroy the hashtable */
void hashtable_destroy() {
  unsigned i;
  for (i = 0; i < local_size; ++i) {
    node_t* node = table[grt_id][i];
    while (node) {
      node_t *next = node->next;
      stm_free(grt_id, node);
      node = next;
    }
  }
  free(table);
}

/* Try to find a node in the current table position corresponding to
   the given value.  Must be called within a transaction. */
static node_t* find_node(gasnet_node_t proc, grt_word_t val, 
			 node_t* p) {
  while(p) {
    grt_word_t* loc = (grt_word_t*) &p->val;
    grt_word_t tmp;
    stm_open_for_read(proc, loc);
    stm_read(proc, loc, &tmp);
    if (val == tmp) {
      return p;
    }
    loc = (grt_word_t*) &p->next;
    stm_open_for_read(proc, loc);
    stm_read(proc, loc, (grt_word_t*) &p);
  }
  return 0;
}

grt_bool_t hashtable_insert(grt_word_t val) {
  hash_t hash = compute_hash(val);
  stm_start();
  grt_word_t *loc = (grt_word_t*) &table[hash.proc][hash.offset];
  stm_open_for_read(hash.proc, loc);
  node_t *p, *q;
  stm_read(hash.proc, (grt_word_t*) loc, (grt_word_t*) &p);
#if 0
  grt_debug_print("inserting %x, read (%u,%x)\n", val, hash.proc, p);
#endif
  if (find_node(hash.proc, val, p)) {
    stm_commit();
    return GRT_TRUE;
  }
  q = (node_t*) stm_alloc(hash.proc, sizeof(node_t));
  loc = (grt_word_t*) &q->next;
  stm_open_for_write(hash.proc, loc);
  stm_write(hash.proc, (grt_word_t) p, loc);
  loc = (grt_word_t*) &q->val;
  stm_open_for_write(hash.proc, loc);
  stm_write(hash.proc, val, loc);
  loc = (grt_word_t*) &table[hash.proc][hash.offset];
  stm_open_for_write(hash.proc, loc);
  stm_write(hash.proc, (grt_word_t) q, loc);
#if 0
  grt_debug_print("inserting %x, wrote (%u,%x)\n", val, hash.proc, q);
#endif
  stm_commit();
  return GRT_FALSE;
}

grt_bool_t hashtable_find(grt_word_t val) {
  hash_t hash = compute_hash(val);
  grt_word_t *loc = (grt_word_t*) &table[hash.proc][hash.offset];
  node_t *p;
  stm_start();
  stm_open_for_read(hash.proc, loc);
  stm_read(hash.proc, loc, (grt_word_t*) &p);
  grt_bool_t result = find_node(hash.proc, val, p) == 0 ? GRT_FALSE : GRT_TRUE;
  stm_commit();
  return result;
}

void hashtable_init(int argc, char **argv) {
  stm_init(argc, argv);
}

int __hashtable_exit(int ret_val) {
  return stm_exit(ret_val);
}
