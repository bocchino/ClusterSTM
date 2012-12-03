#include <stdio.h>
#include <math.h>

extern "C" {
#include "gasnet.h"
#include "gasnet_tools.h"
}
#include "params.h"
#include "cut_clusters.h"
#include "graph.h"
#include "input.h"
#include "error.h"
#include "debug.h"

void parse_args(int argc, char** argv);
void init_params(FILE*);
void write_output(cc_output_t result);

static FILE *infile = stdin;
static params_t params;
static cc_output_t result;

int main(int argc, char** argv) {
  prog_name = argv[0];
  parse_args(argc, argv);
  find_start(infile);
  init_params(infile);
  graph_t *graph = create_graph(infile, &params);
  gasnett_tick_t start, end;

  cut_clusters_init(graph, &params, infile);
  start = gasnett_ticks_now();
  result = cut_clusters(graph, &params, infile);
  end = gasnett_ticks_now();
  cut_clusters_cleanup();

  unsigned time = ((unsigned) gasnett_ticks_to_us(end - start));
  printf("execution time=%f s\n", ((float)time) / 1000000);

  write_output(result);
  free(result.cluster_set.data);
  free(result.cluster_starts.data);
  return 0;
}

void write_output(cc_output_t result) {
  FILE *outfile = fopen("k4.out", "w+");
  unsigned i;
  for (i = 0; i < result.cluster_set.length; ++i)
    fprintf(outfile, "%u ", result.cluster_set.data[i]);
  fprintf(outfile, "\n\n");
  for (i = 0; i < result.cluster_starts.length; ++i)
    fprintf(outfile, "%u ", result.cluster_starts.data[i]);
  fprintf(outfile, "\n\n");
  fprintf(outfile, "%u", result.cut_links);
  fprintf(outfile, "\n\n");
  fclose(outfile);
  double ratio = (double) params.max_cluster_size / 
    params.max_clique_size;
  double predicted = 1 / sqrt(ratio);
  double actual = (double) result.cut_links / 
    params.num_ic_edges;
  double error = actual / predicted - 1;
  printf("error=%f\n", error);
}

void init_params(FILE *fp) {
  find_string(fp, "numICEdges");
  params.num_ic_edges = get_int(fp);
  print_unsigned(params.num_ic_edges);
  find_string(fp, "maxVertex");
  params.num_v = get_int(fp);
  print_unsigned(params.num_v);
  find_string(fp, "maxClusterSize");
  params.max_cluster_size = get_int(fp);
  print_unsigned(params.max_cluster_size);
  find_string(fp, "maxCliqueSize");
  params.max_clique_size = get_int(fp);
  print_unsigned(params.max_clique_size);
  find_string(fp, "alpha");
  params.alpha = get_double(fp); 
  print_double(params.alpha);
  find_string(fp, "linkWeight");
  params.link_weight = get_int(fp);
  print_unsigned(params.link_weight);
}

void parse_args(int argc, char** argv) {
  if (argc > 1) {
    char *fname = argv[1];
    if (!(infile = fopen(fname, "r"))) {
      error("error opening file %s", fname);
    }
  }
}
