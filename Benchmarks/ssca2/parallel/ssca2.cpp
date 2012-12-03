#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

extern "C" {
#include "stm-distrib.h"
#include "ssca2_handler.h"
#include "gasnet_tools.h"
}
#include "params.h"
#include "cut_clusters.h"
#include "graph.h"
#include "input.h"
#include "error.h"
#include "debug.h"

void parse_args(int argc, char** argv);
void init_params(FILE*,unsigned);
void write_output(cc_output_t result);

static FILE *infile = stdin;
static FILE *outfile;
static params_t params;
static cc_output_t result;
static size_t *cut_links_arr;
static unsigned *times;
#if SHOW_CONTENTION
static size_t *locked_counts;
static size_t *retry_counts;
static size_t *claimed_counts;
#endif

int main(int argc, char** argv) {
  ssca2_init(argc, argv);
  setpriority(PRIO_PROCESS, 0, 19);
  unsigned i;
  prog_name = argv[0];
  parse_args(argc, argv);
  find_start(infile);
  init_params(infile, grt_num_procs);
  
  times = 
    (unsigned*) grt_all_alloc(0, grt_num_procs * sizeof(unsigned));
#if SHOW_CONTENTION
  locked_counts = (size_t*)
    grt_all_alloc(0, grt_num_procs * sizeof(size_t));
  claimed_counts = (size_t*)
    grt_all_alloc(0, grt_num_procs * sizeof(size_t));
  retry_counts = (size_t*)
    grt_all_alloc(0, grt_num_procs * sizeof(size_t));
#endif
  if (grt_id == 0) {
    print_unsigned(params.num_v);
    print_unsigned(params.num_ic_edges);
    print_unsigned(params.max_cluster_size);
    print_unsigned(params.max_clique_size);
    print_double(params.alpha);
    print_unsigned(params.link_weight);
    if (params.num_v % grt_num_procs != 0)
      error("number of processors must divide number of graph vertices!");
    printf("%u processors; ", grt_num_procs);
#ifdef SEQUENTIAL
    printf("sequential\n");
#else
    printf("parallel\n");
#endif
#ifdef STM
    printf("STM\n");
#else
    printf("locks\n");
#endif
#ifdef NB_ON
    if (grt_id == 0) {
      printf("NONBLOCKING ON\n");
    }
#endif
    printf("FIRST_SSCA2_ID=%d\n", FIRST_SSCA2_ID);
    fflush(stdout);
  }
  BARRIER();

  cut_links_arr = (size_t*) grt_all_alloc(0, grt_num_procs * sizeof(size_t));
  
  graph_t *graph = create_graph(infile, params.num_v);
  cut_clusters_init(graph, &params, infile);

  gasnett_tick_t start, end;
  unsigned time, run_time;
#if SHOW_CONTENTION
  size_t locked_count, claimed_count, retry_count;
#endif
  BARRIER();

#ifdef SEQUENTIAL
  for (i = 0; i < grt_num_procs; ++i) {
    if (grt_id == i) {
#endif
      start = gasnett_ticks_now();
      result = cut_clusters(graph, &params, infile);
      end = gasnett_ticks_now();
      time = ((unsigned) gasnett_ticks_to_us(end - start));
      gasnet_put(0, &times[grt_id], &time, sizeof(unsigned));

#ifdef SEQUENTIAL
    }
    BARRIER();
  }
#endif
  gasnet_put(0, &cut_links_arr[grt_id], &result.cut_links, 
	     sizeof(size_t));
#if SHOW_CONTENTION
  gasnet_put(0, &locked_counts[grt_id], &result.locked_count,
	     sizeof(size_t));
  gasnet_put(0, &claimed_counts[grt_id], &result.claimed_count,
	     sizeof(size_t));
  gasnet_put(0, &retry_counts[grt_id], &result.retry_count,
	     sizeof(size_t));
#endif
  // Sequential output
  BARRIER();
  if (grt_id == 0) {
    time = 0, run_time = 0;
    locked_count = claimed_count = retry_count = 0;
    for (i = 0; i < grt_num_procs; ++i) {
      unsigned this_time = times[i];
#ifdef SEQUENTIAL
      // Sequential run time is sum of processor times
      run_time += this_time;
#else
      // Parallel run time is max of processor times
      if (this_time >= run_time) run_time = this_time;
#endif
#if SHOW_CONTENTION
      locked_count += locked_counts[i];
      retry_count += retry_counts[i];
      claimed_count += claimed_counts[i];
#endif
    }
    printf("execution time=%f s\n", (float) run_time / 1000000);
#if SHOW_CONTENTION
    print_unsigned(locked_count);
    print_unsigned(retry_count);
    print_unsigned(claimed_count);
#endif
  }

  if (grt_id == 0) {
    size_t total_cl = 0;
    for (i = 0; i < grt_num_procs; ++i)
      total_cl += cut_links_arr[i];
    print_double(total_cl);
    double ratio = (double) params.max_cluster_size / 
      params.max_clique_size;
    print_double(ratio);
    double predicted = 1 / sqrt(ratio);
    print_double(predicted);
    double actual = (double) total_cl / params.num_ic_edges;
    print_double(actual);
    double error = actual / predicted - 1;
    print_double(error);
  }
  BARRIER();
  
  cut_clusters_cleanup();
  if (grt_id == 0)
    grt_free(0, cut_links_arr);

#ifdef STM
  stm_exit(0);
#else
  grt_exit(0);
#endif
}

void write_output(cc_output_t result) {
  unsigned i;
  for (i = 0; i < result.cluster_set.length; ++i)
    fprintf(outfile, "%u ", result.cluster_set.data[i]);
  fprintf(outfile, "\n\n");
  for (i = 0; i < result.cluster_starts.length; ++i)
    fprintf(outfile, "%u ", result.cluster_starts.data[i]);
  fprintf(outfile, "\n\n");
  fprintf(outfile, "%u", result.cut_links);
  fprintf(outfile, "\n\n");
}

void init_params(FILE *fp, unsigned grt_num_procs) {
  find_string(fp, "numICEdges");
  params.num_ic_edges = get_int(fp);
  find_string(fp, "maxVertex");
  params.num_v = get_int(fp);
  find_string(fp, "maxClusterSize");
  params.max_cluster_size = get_int(fp);
  find_string(fp, "maxCliqueSize");
  params.max_clique_size = get_int(fp);
  find_string(fp, "alpha");
  params.alpha = get_double(fp); 
  find_string(fp, "linkWeight");
  params.link_weight = get_int(fp);
  params.num_compute_procs = grt_num_procs;
}

#define FILE_ARG 3

void parse_args(int argc, char** argv) {
  if (argc > FILE_ARG) {
    char *fname = argv[FILE_ARG];
    if (!(infile = fopen(fname, "r"))) {
      error("error opening file %s", fname);
    }
  }
}
