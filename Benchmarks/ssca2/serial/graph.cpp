#include "graph.h"
#include "input.h"
#include "error.h"

void get_adjLists(FILE *fp, graph_t *graph) {
  find_string(fp, "adjLists");
  unsigned num_elts = get_int(fp);
  unsigned row_offset = 0;
  unsigned i, j;
  graph->adjLists = 
    create_sparse_2D_array(graph->maxVertex, num_elts);
  unsigned *data =graph->adjLists->data;
  for (i = 0; i < graph->maxVertex; ++i) {
    find_string(fp, "adjLists{%d}", i+1);
    unsigned row_len = get_int(fp);
    if (row_offset + row_len > num_elts)
      error("too many elements in adjLists");
    graph->adjLists->row_offsets[i] = row_offset;
    row_offset += row_len;
    for (j = 0; j < row_len; ++j) {
      *data++ = get_int(fp)-1;
    }
  }  
}

void get_edgeWeights(FILE *fp, graph_t *graph) {
  unsigned i, j, k;
  graph->edgeWeights = 
    (int*) malloc(graph->maxParalEdges * 
		  graph->maxVertex * sizeof(int));
  unsigned num_elts;
  unsigned col;
  int row, weight;
  int idx = 0;
  unsigned *data;
  int *edgeWeights;
  edgeWeights = graph->edgeWeights;
  for (i = 0; i < graph->maxParalEdges; ++i) {
    find_string(fp, "edgeWeights{%d}", i+1);
    num_elts = get_int(fp);
    data = graph->adjLists->data;
    for (j = 0; j < graph->maxVertex; ++j) {
      unsigned len = row_length(graph->adjLists, j);
      weight = 0;
      for (k = 0; k < len; ++k) {
	/* If we need a new weight, then read in a new coordinate pair
	   and weight */
	skip_whitespace(fp);
	if (peek_char(fp) != '(') {
	  *edgeWeights++ = 0;
	  ++data;
	  continue;
	}
	if (weight == 0) {
	  find_string(fp, "(");
	  row = get_int(fp);
	  find_string(fp, ",");
	  col = get_int(fp);
	  find_string(fp, ")");
	  weight = get_int(fp);
	}
	/* Check for errors */
	if (idx >= graph->maxParalEdges * 
	    graph->maxVertex)
	  error("Too many edge weights");
	/* If the row and column match, store and reset the weight to
	   zero; otherwise, store a zero for the weight and keep
	   going */
	if (row != *data+1 || col != j+1) {
	  *edgeWeights = 0;
	} else {
	  *edgeWeights = weight;
	  weight = 0;
	}
	++edgeWeights;
	++data;
      }
    }
  }
}

/* This doesn't quite work because the strings can apparently be
   empty!  If I need this to work, I can put in some sort of marker
   value.  But I don't think I need it for Kernel 4. */

void get_stringArray(FILE *fp, graph_t *graph) {
  find_string(fp, "stringArray");
  graph->stringArray = 
    (string_array_t*) malloc(sizeof(string_array_t));
  unsigned num_rows = get_int(fp);
  unsigned row_length = get_int(fp);
  graph->stringArray->num_rows = num_rows;
  graph->stringArray->row_length = row_length;
  char *data = 
    (char*) malloc(num_rows * row_length * sizeof(char));
  graph->stringArray->data = data;
  unsigned i;
  for (i = 0; i < num_rows * row_length; ++i) {
    skip_whitespace(fp);
    *data++ = get_char(fp);
  }
}

graph_t *create_graph(FILE *fp, params_t *params) {
  graph_t *graph = (graph_t*) malloc(sizeof(graph_t));

  graph->maxVertex = params->num_v;
  find_string(fp, "maxParalEdges");
  graph->maxParalEdges = get_int(fp);

  get_adjLists(fp, graph);
  // Don't do this for now
  //get_edgeWeights(fp, graph);
  //get_stringArray(fp, graph);

  return graph;
}

sparse_2D_array_t *create_sparse_2D_array(unsigned num_rows,
					  unsigned num_elts) {
  sparse_2D_array_t *array =
    (sparse_2D_array_t*) malloc(sizeof(sparse_2D_array_t));
  array->num_rows = num_rows;
  array->row_offsets = 
    (unsigned*) malloc(num_rows * sizeof(unsigned));
  array->num_elts = num_elts;
  array->data = (unsigned*) malloc(num_elts * sizeof(int));
  return array;
}

unsigned row_length(sparse_2D_array_t *arr, unsigned idx) {
  unsigned row_offset_hi = 
    (idx == arr->num_rows - 1) ? 
    arr->num_elts : 
    arr->row_offsets[idx+1];
  return row_offset_hi - arr->row_offsets[idx];
}
