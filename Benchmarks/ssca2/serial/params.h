#ifndef PARAMS_H
#define PARAMS_H

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
  /* ??? */
  unsigned short link_weight;
  /* ??? */
  unsigned short max_processors;
  /* Maximum randomized vertex number */
  unsigned num_v;
} params_t;

#endif
