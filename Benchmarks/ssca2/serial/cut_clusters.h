#include "graph.h"
#include "params.h"
#include "input.h"
#include "debug.h"
#include "error.h"

typedef struct {
  array_unsigned_t cluster_set;
  array_unsigned_t cluster_starts;
  unsigned cut_links;
} cc_output_t;

void cut_clusters_init(graph_t *graph,
		       params_t *params,
		       FILE *fp);
cc_output_t cut_clusters(graph_t *graph, 
			 params_t *params,
			 FILE *fp);
void cut_clusters_cleanup(void);
