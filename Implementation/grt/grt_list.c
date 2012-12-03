#include "grt_list.h"
#include "assert.h"

grt_list_t *grt_list_create() {
  grt_list_t *list = malloc(sizeof(grt_list_t));
  list->head = list->tail = list->next = 0;
  list->size = 0;
  return list;
}

void grt_list_destroy(grt_list_t *list) {
  grt_list_node_t *node = list->head;
  while(node) {
    grt_list_node_t *next = node->next;
    free(node);
    node = next;
  }
  free(list);
}

void grt_list_append(grt_list_t *list, grt_word_t contents) {
  grt_list_node_t *node = malloc(sizeof(grt_list_node_t));
  node->next = 0;
  node->contents = contents;
  if (!list->head)
    list->head = list->tail = node;
  else {
    list->tail->next = node;
    list->tail = node;
  }
  ++list->size;
}

grt_word_t grt_list_remove(grt_list_t *list) {
  grt_word_t result = list->head->contents;
  list->head = list->head->next;
  if (list->head == 0) list->tail = 0;
  --list->size;
  return result;
}

void grt_list_start(grt_list_t* list) {
  list->next = list->head;
}

grt_list_node_t *grt_list_get_next(grt_list_t *list) {
  grt_list_node_t *result = 0;
  if (list->next) {
    result = list->next;
    list->next = list->next->next;
  }
  return result;
}

