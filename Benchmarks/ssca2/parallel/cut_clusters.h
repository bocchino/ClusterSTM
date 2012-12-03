#include "graph.h"
#include "params.h"
#include "input.h"
#include "debug.h"
#include "error.h"

#define SHOW_CHECKPOINTS 0
#define SHOW_LOCKING 0
#define SHOW_ADJACENCIES 0
#define SHOW_CONTENTION 1
#define NO_RETRY 1

#if SHOW_LOCKING
#define PRINT_LOCKING(...) grt_debug_print(__VA_ARGS__)
#else
#define PRINT_LOCKING(...)
#endif

#if SHOW_CHECKPOINTS
#define CHECKPOINT(cp) grt_debug_print("checkpoint %s\n", cp)
#else
#define CHECKPOINT(cp)
#endif

extern unsigned local_num_v;
extern array_array_unsigned_t_t* remaining_adj_lists;
extern unsigned *vertex_states;
#if SHOW_CONTENTION
extern size_t retry_count;
#endif

#define PROC(__v) (__v / local_num_v)
#define IDX(__v) (__v % local_num_v)
#define RAV(__proc, __idx) (&remaining_adj_lists[__proc].data[__idx])
#define VERTEX_STATES vertex_states

typedef struct {
  array_unsigned_t cluster_set;
  array_unsigned_t cluster_starts;
  unsigned cut_links;
#if SHOW_CONTENTION
  size_t locked_count;
  size_t claimed_count;
  size_t retry_count;
#endif
} cc_output_t;

void init_shared(graph_t *graph);

void cleanup_shared(void);

void cut_clusters_init(graph_t *graph,
		       params_t *params,
		       FILE *fp);

void cut_clusters_cleanup(void);

cc_output_t cut_clusters(graph_t *graph, 
			 params_t *params,
			 FILE *fp);


