#ifndef PARAMS_H
#define PARAMS_H

#include "grt_types.h"
#include <math.h>

typedef struct {
  /* Number of inter-clique edges */
  unsigned num_ic_edges;
  /* Maximum cluster size */
  unsigned max_cluster_size;
  /* Maximum clique size */
  unsigned max_clique_size;
  /* Scale factor */
  double alpha;
  /* Weight used in computing cluster */
  unsigned short link_weight;
  /* Maximum randomized vertex number */
  unsigned num_v;
  /* Number of compute nodes */
  unsigned num_compute_procs;
} params_t;

#endif
