#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "params.h"

/* Graph */

typedef struct graph_t {
  unsigned maxVertex;
  unsigned maxParalEdges;
  sparse_2D_array_t *adjLists;
  int *edgeWeights;
  string_array_t *stringArray;
  size_t max_row_len;
} graph_t;

unsigned row_length(sparse_2D_array_t *arr, unsigned idx);
graph_t *create_graph(FILE *fp, unsigned num_v);
sparse_2D_array_t *create_sparse_2D_array(unsigned num_rows,
					  unsigned num_elts);


#endif
