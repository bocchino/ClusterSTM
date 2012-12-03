#include "grt_map.h"

static unsigned compute_hash(grt_word_t word) {
  return word % GRT_MAP_TABLE_SIZE;
}

grt_map_t *grt_map_create() {
  grt_map_t *map = (grt_map_t*) malloc(sizeof(grt_map_t));
  map->table = (grt_list_t**) malloc(GRT_MAP_TABLE_SIZE *
				     sizeof(grt_list_t*));
  memset(map->table, 0, GRT_MAP_TABLE_SIZE * sizeof(grt_list_t*));
  return map;
}

void grt_map_destroy(grt_map_t *map) {
  unsigned i;
  for (i = 0; i < GRT_MAP_TABLE_SIZE; ++i) {
    grt_list_t *list = map->table[i];
    if (list) {
      grt_pair_t *pair;
      grt_list_foreach(grt_pair_t*, list, pair)
	free(pair);
      grt_list_destroy(list);
    }
  }
  free(map->table);
  free(map);
}

static grt_pair_t*
find_pair(grt_word_t key, grt_list_t *list) {
  grt_pair_t *pair = 0;
  grt_list_foreach(grt_pair_t*, list, pair) {
    if (pair->key == key)
      return pair;
  }
  return 0;
}

int grt_map_insert(grt_map_t *map, grt_word_t key,
		   grt_word_t value) {
  unsigned bucket = compute_hash(key);
  grt_list_t *list = map->table[bucket];
  grt_pair_t *pair;
  if (!list) {
    list = grt_list_create();
    map->table[bucket] = list;
  }
  pair = find_pair(key, list);
  if (pair) {
    pair->value = value;
    return 1;
  }
  pair = (grt_pair_t*) malloc(sizeof(grt_pair_t));
  pair->key = key;
  pair->value = value;
  grt_list_append(list, (grt_word_t) pair);
  return 0;
}

grt_bool_t grt_map_find(grt_map_t *map, grt_word_t key, 
			grt_word_t *value) {
  unsigned bucket = compute_hash(key);
  grt_list_t *list = map->table[bucket];
  grt_pair_t *pair;
  if (!list) return GRT_FALSE;
  pair = find_pair(key, list);
  if (pair && value)
    *value = pair->value;
  return (pair != 0) ? GRT_TRUE : GRT_FALSE;
}


