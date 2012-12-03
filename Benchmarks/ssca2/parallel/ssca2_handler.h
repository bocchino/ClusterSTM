#ifndef SSCA2_HANDLER_H
#define SSCA2_HANDLER_H

#define NEW_STUFF

#ifndef STM
#define FIRST_SSCA2_ID (LAST_GRT_ID + 1)
#else
#define FIRST_SSCA2_ID (LAST_STM_ID + 1)
#endif

#define SSCA2_ID(x) (FIRST_SSCA2_ID + x)
#ifdef STM
#define WRITE_STATE_HANDLER_ID       SSCA2_ID(0)
#define UPDATE_ADJ_LIST_HANDLER_ID           SSCA2_ID(1)
#define WRITE_VERTICES_HANDLER_ID    SSCA2_ID(2)
#define CACHE_VERTEX_HANDLER_ID      SSCA2_ID(3)
#define LAST_SSCA2_ID CACHE_VERTEX_HANDLER_ID
#else
#define LOCK_ALL_HANDLER_ID          SSCA2_ID(0)
#define WRITE_STATE_HANDLER_ID       SSCA2_ID(1)
#define UPDATE_ADJ_LIST_HANDLER_ID   SSCA2_ID(2)
#define WRITE_VERTICES_HANDLER_ID    SSCA2_ID(3)
#define LAST_SSCA2_ID WRITE_VERTICES_HANDLER_ID
#endif

#ifndef STM
#define SSCA2_TABLE_SIZE						\
  (GRT_TABLE_SIZE + (LAST_SSCA2_ID - FIRST_SSCA2_ID) + 1)
#else
#define SSCA2_TABLE_SIZE						\
  (STM_TABLE_SIZE + (LAST_SSCA2_ID - FIRST_SSCA2_ID) + 1)
#endif

#ifndef STM
#define SSCA2_TABLE_SEGMENT					\
  { LOCK_ALL_HANDLER_ID, lock_all_handler },	\
  { WRITE_STATE_HANDLER_ID, write_state_handler },	\
  { UPDATE_ADJ_LIST_HANDLER_ID, update_adj_list_handler }, \
  { WRITE_VERTICES_HANDLER_ID, write_vertices_handler }
#else
#define SSCA2_TABLE_SEGMENT					\
  { WRITE_STATE_HANDLER_ID, write_state_handler },	\
  { UPDATE_ADJ_LIST_HANDLER_ID, update_adj_list_handler }, \
  { WRITE_VERTICES_HANDLER_ID, write_vertices_handler }, \
  { CACHE_VERTEX_HANDLER_ID, cache_vertex_handler }
#endif

#define SSCA2_SHARE_POWER 2

#ifndef STM
GRT_ON_HANDLER_DECL(lock_all);
GRT_ON_LOCAL_FN_DECL(lock_all);
GRT_ON_HANDLER_DECL(write_state);
GRT_ON_LOCAL_FN_DECL(write_state);
GRT_ON_HANDLER_DECL(update_adj_list);
GRT_ON_LOCAL_FN_DECL(update_adj_list);
GRT_ON_HANDLER_DECL(write_vertices);
GRT_ON_LOCAL_FN_DECL(write_vertices);
#endif


#ifdef STM
STM_ON_LOCAL_FN_DECL(cache_vertex);
STM_ON_LOCAL_FN_DECL(write_vertices);
STM_ON_LOCAL_FN_DECL(write_state);
STM_ON_LOCAL_FN_DECL(update_adj_list);
#endif

void ssca2_init(int argc, char** argv);

#endif
