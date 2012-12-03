#include "grt_types.h"

typedef struct node_t {
  struct node_t *next;
  grt_word_t val;
} node_t;

unsigned compute_hash(grt_word_t val);
grt_bool_t hashtable_insert(grt_word_t val);
grt_bool_t hashtable_find(grt_word_t val);
void hashtable_create(gasnet_node_t proc, unsigned size);
void hashtable_destroy();
