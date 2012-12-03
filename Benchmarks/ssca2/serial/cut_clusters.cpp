#include "cut_clusters.h"
#include "grt_macros.h"
#include <assert.h>
#include <math.h>
#include <set>
#include <map>

#define cut_box_size params->max_cluster_size
#define INDENT_ON
#ifdef INDENT_ON
#define SET_INDENT(x)				\
  indent += x
#define INDENT print_spaces(indent)
#else		
#define SET_INDENT(x)
#define INDENT
#endif

#define READ_INDEX_ARRAY 0

using namespace std;

unsigned cluster_count = 0;
unsigned main_loop_count = 0;

/* Number of vertices assigned to this processor */
unsigned num_v;

/* Total number of vertices */
unsigned max_vertex;

/* Where to start the search? */
unsigned start_search;

/* For each vertex, the total number of adjacencies */
unsigned *adj_counts_tot;

/* Permutation indices of sorted adj_counts_tot */
unsigned *perm_idx;

/* The ID of the processor that owns each vertex? */
/* Note that there's something funny going on with vertex_states in
   the parallel case.  The sequential case is simple though: 0 if
   not taken, 1 if taken. */
unsigned *vertex_states;

/* Flat array of vertices, made into a 2D array of clusters by
   cluster_starts */
array_unsigned_t cluster_set;

/* Index array for the sparse array cluster_set */
unsigned *cluster_starts;

/* Set of vertices in the current cluster */
unsigned *cluster;
unsigned cluster_size;

/* Boolean array that says, for each vertex, whether it is in or out
   of the current cluster */
set<int> cluster_mask;

/* New adjacencies of current candidate for next vertex in cluster */
array_unsigned_t new_adj;

/* New adjacencies of current best candidate for next vertex in
   cluster */
array_unsigned_t new_adj_best;

/* Number of links between each vertex and cluster */
map<unsigned,unsigned> adj_counts_to_cluster;
std::pair<unsigned,unsigned> *best_adj_count_pairs;
unsigned best_adj_count_length;

/* Copy of graph->adjLists, destroyed as edges are cut */
array_array_unsigned_t_t remaining_adj_lists;

/* Temporary storage for uncache clusters */
unsigned *cache_buf;

/* Set of adjacencies to the current cluster */
array_unsigned_t adj_set_to_cluster;

/* Minimum cut fanout so far */

unsigned cn_cut;

/* Temporary storage for the current best cut */

unsigned best_cut = 0;

/* The actual cut */

unsigned cut;

/* Update adj_set_to_cluster to contain all vertices with positive
   adj_weight */

inline void update_adj_set_to_cluster(void) {
  unsigned *data = adj_set_to_cluster.data;
  adj_set_to_cluster.length = 0;    
  grt_stl_map_foreach(unsigned, unsigned, adj_counts_to_cluster, 
		      idx) {
    if (idx->second != 0) {
      ++adj_set_to_cluster.length;
      *data++ = idx->first;
    }
  }
}

void claim_vertices(params_t *params) {
  unsigned i, j;

  /* Claim all vertices in cluster[0:cut] */

  for (i = 0; i < cut; ++i) {
    vertex_states[cluster[i]] = false;
  }

  /* Unmask the non-claimed vertices */

  for (i = cut; i < cluster_size; ++i) {
    cluster_mask.erase(cluster[i]);
  }

  /* Put all unmasked vertices into remaining_adj_lists */

  for (i = 0; i < adj_set_to_cluster.length; ++i) {
    unsigned count = 0;
    array_unsigned_t *i_adj = 
      &remaining_adj_lists.data[adj_set_to_cluster.data[i]];
    for (j = 0; j < i_adj->length; ++j) {
      unsigned vertex = i_adj->data[j];
      if (!cluster_mask.count(vertex)) {
	cache_buf[count++] = vertex;
      }
    }
    i_adj->length = count;
    for (j = 0; j < i_adj->length; ++j)
      i_adj->data[j] = cache_buf[j];
  }
#if 0
  printf("after:\n");
  for (i = 0; i < adj_set_to_cluster.length; ++i) {
    unsigned v = adj_set_to_cluster.data[i];
    print_unsigned(v);
    print_array(remaining_adj_lists.data[v]);
  }
#endif
}
		      
/* Comparison function for generating permutation indices */

int ind_adj_cmp(const void *keyval,
		const void *datum) {
  unsigned key = adj_counts_tot[*((unsigned*) keyval)];
  unsigned dat = adj_counts_tot[*((unsigned*) datum)];
  if (key < dat)
    return -1;
  else if (key == dat)
    return 0;
  else
    return 1;
}

/* Allocate a whole bunch of data structures */

void cut_clusters_init(graph_t *graph,
		       params_t *params,
		       FILE *fp) {
  unsigned v, i, j;

  num_v = params->num_v;
  max_vertex = graph->maxVertex;

  if (params->max_cluster_size < 1)
    error("max_cluster_size must be at least one");
  if (params->alpha < 0 || params->alpha > 1)
    error("alpha must be between 0 and 1 inclusive");

  start_search = ceil(params->alpha * params->max_cluster_size);
  adj_counts_tot = 
    (unsigned*) malloc(num_v * sizeof(unsigned));
  vertex_states =
    (unsigned*) malloc(num_v * sizeof(unsigned));
  for (v = 0; v < num_v; ++v) {
    unsigned len = row_length(graph->adjLists, v);
    adj_counts_tot[v] = len;
    vertex_states[v] = (len == 0) ? 0 : 1;
  }
  perm_idx = 
    (unsigned *) malloc(num_v * sizeof(unsigned));

  /* Sort vertices by out-degree and store the resulting permutation
     indices */

#if !READ_INDEX_ARRAY
  for (v = 0; v < num_v; ++v)
    perm_idx[v] = v;
  //qsort(perm_idx, num_v, sizeof(unsigned), ind_adj_cmp);
#else
  // Read sorted array from file because C sorting doesn't produce the
  // same results as Matlab when equal values are involved
  find_string(fp, "indAdjCounts");
  for (v = 0; v < num_v; ++v)
    perm_idx[v] = get_int(fp)-1;
#endif

  cluster_set.length = 0;
  cluster_set.data = (unsigned*)
    calloc(ceil(1.2 * num_v), sizeof(unsigned));
  cluster_starts = (unsigned*) malloc(num_v * sizeof(unsigned));
  // This is because of a BUG in the Matlab implementation: should be
  // bounded by max cluster size!
  cluster = (unsigned*)
    calloc(num_v, sizeof(unsigned));

  new_adj.length = 0;
  new_adj.data = 
    (unsigned*) malloc(max_vertex * sizeof(unsigned));

  new_adj_best.length = 0;
  new_adj_best.data = 
    (unsigned*) malloc(max_vertex * sizeof(unsigned));

  remaining_adj_lists.length = graph->adjLists->num_rows;
  remaining_adj_lists.data =
    (array_unsigned_t*) malloc(remaining_adj_lists.length *
			       sizeof(array_unsigned_t));
  unsigned *data = graph->adjLists->data;
  for (i = 0; i < remaining_adj_lists.length; ++i) {
    unsigned len = row_length(graph->adjLists, i);
    remaining_adj_lists.data[i].length = len; 
    remaining_adj_lists.data[i].data =
      (unsigned*) malloc(len * sizeof(unsigned));
    for (j = 0; j < len; ++j) {
      remaining_adj_lists.data[i].data[j] = *data++;
    }
  }

  cache_buf = (unsigned*) malloc(max_vertex * sizeof(unsigned));
  adj_set_to_cluster.length = 0;
  adj_set_to_cluster.data = (unsigned*) malloc(max_vertex * sizeof(unsigned));

  best_adj_count_pairs = (std::pair<unsigned,unsigned>*) malloc(max_vertex * sizeof(std::pair<unsigned,unsigned>));
}

/* Free all the structures we allocated */

void cut_clusters_cleanup(void) {
  free(adj_counts_tot);
  free(vertex_states);
  free(perm_idx);
  free(cluster);
  free(new_adj.data);
  free(new_adj_best.data);
  unsigned i;
  for (i = 0; i < remaining_adj_lists.length; ++i) {
    free(remaining_adj_lists.data[i].data);
  }
  free(remaining_adj_lists.data);
  free(cache_buf);
  free(adj_set_to_cluster.data);
  free(best_adj_count_pairs);
}

unsigned cluster_end = 0;
unsigned cluster_number = 0;
unsigned cut_links = 0;
unsigned next_v = 0;
static cc_output_t result;

void find_next_v(params_t *params) {

#if 0
  INDENT;
  printf("find_next_v\n");
#endif
  SET_INDENT(2);
  unsigned i, j;
  
  /* Max of cn similarity results */
  
  unsigned cn_max = 0;
  
  /* For each vertex v adjacent to the current cluster */

#if 0
  INDENT;
  printf("remaining_adj_lists:\n");
  for (i = 0; i < num_v; ++i) {
    print_unsigned(i);
    print_array(remaining_adj_lists.data[i]);
  }
  print_unsigned(adj_set_to_cluster.length);
#endif

  for (i = 0; i < adj_set_to_cluster.length; ++i) {

#if 0
    INDENT;
    printf("for loop %u\n", i);
#endif
    SET_INDENT(2);
    unsigned v = adj_set_to_cluster.data[i];
#if 0
    print_unsigned(v);
#endif
   
    /* v_adj contains the remaining adjacent vertices of v */
    
    array_unsigned_t v_adj = remaining_adj_lists.data[v];
#if 0
    print_array(v_adj);
#endif
    /* new_adj contains all the NEW remaining adjacent vertices of
       v: i.e., v_adj \ cluster */
    
    /* all_nonzero is true if adj_counts_to_cluster for each vertex in
       new_adj are nonzero, otherwise false */
    
    bool all_nonzero = true;
    unsigned *data = new_adj.data;

    new_adj.length = 0;
    for (j = 0; j < v_adj.length; ++j) {
      if (!cluster_mask.count(v_adj.data[j])) {
	++new_adj.length;
	*data++ = v_adj.data[j];
	if (adj_counts_to_cluster[v_adj.data[j]] == 0) {
	  all_nonzero = false;
	}
      }
    }
    
    /* If adj_counts_to_cluster[i] = 0 for all i in new_adj, then select
       v as next_v and copy new_adj into new_adj_best */
    
    if (all_nonzero) {
      next_v = v;
      new_adj_best.length = new_adj.length;
      for (j = 0; j < new_adj.length; ++j)
	new_adj_best.data[j] = new_adj.data[j];
#if 0
      INDENT;
      printf("all nonzero, breaking out of loop\n");
#endif
      SET_INDENT(-2);
      break;
    }
    
    /* Otherwise, compute how similar this vertex is to the current
       cluster: the number of links from matching vertices to the
       cluster + linkWeight times the number of links from this
       vertex */
    
    int cn = 0;
    for (j = 0; j < new_adj.length; ++j)
      cn += adj_counts_to_cluster[new_adj.data[j]];
    cn += params->link_weight * 
      adj_counts_to_cluster[adj_set_to_cluster.data[i]];
    /* If cn exceeds cn_max, or cn == cn_max and a tiebreaking
       condition is met, select v as next_v and copy new_adj into
       new_adj_best */
    
    if (cn >= cn_max) {
      if (cn > cn_max || 
	  new_adj.length < new_adj_best.length) {
	cn_max = cn;
	next_v = v;
	new_adj_best.length = new_adj.length;
	for (j = 0; j < new_adj.length; ++j)
	  new_adj_best.data[j] = new_adj.data[j];
      }
    }
    SET_INDENT(-2);
  }
#if 0
  print_unsigned(next_v);
#endif
  SET_INDENT(-2);
}

void add_next_v(void) {
#if 0
  INDENT;
  printf("add_next_v\n");
#endif
  SET_INDENT(2);

  unsigned i,j;

  /* Update cluster, cluster_mask, and adj_counts_to_cluster */

  cluster[cut++] = next_v;
  cluster_mask.insert(next_v);
  adj_counts_to_cluster.erase(next_v);

  for (i = 0; i < new_adj_best.length; ++i) {
    unsigned idx = new_adj_best.data[i];
    //adj_counts_to_cluster[idx] = adj_counts_to_cluster[idx]+1;
    ++adj_counts_to_cluster[idx];
  }
  update_adj_set_to_cluster();

  SET_INDENT(-2);
}

/* Compute a cluster starting with start_v */

void compute_cluster(unsigned start_v, params_t *params) {
  unsigned i,j;

#if 0
  INDENT;
  printf("compute_cluster\n");
#endif
  SET_INDENT(2);

  /* Initialize vars */

  best_cut = 0;
  cn_cut = UINT_MAX;
  cut = 1;

  /* Add start_v to the cluster */

  cluster[0] = start_v;

  /* Clear out the cluster mask except for start_v */

  cluster_mask.clear();
  cluster_mask.insert(start_v);
  
  /* current cluster = { start_v } */
  /* adj_set_to_cluster = {v in V : v is adjacent to start_v} */
  /* adj_counts_to_cluster[v] = 1 if v in adj_set_to_cluster, 0 otherwise */

#if 0
  print_unsigned(start_v);
  print_array(remaining_adj_lists.data[start_v]);
#endif
  adj_counts_to_cluster.clear();
  adj_set_to_cluster.length = 
    remaining_adj_lists.data[start_v].length;
  for (i = 0; i < adj_set_to_cluster.length; ++i) {
    adj_set_to_cluster.data[i] = 
      remaining_adj_lists.data[start_v].data[i];
    adj_counts_to_cluster[adj_set_to_cluster.data[i]] = 1;
  }
#if 0
  print_array(adj_set_to_cluster);
#endif
  
  /* Compute best_cut */

  while (adj_set_to_cluster.length) {
#if 0
    INDENT;
    printf("while (adj_set_to_cluster.length)\n");
#endif
    SET_INDENT(2);
    /* Examine all vertices in adj_set_to_cluster; find next v to add
       to cluster and update new_adj_best */
    find_next_v(params);
    /* Add next_v to the cluster: Use new_adj_best to update cluster,
       cluster_mask, adj_counts_to_cluster, and adj_set_to_cluster */
    add_next_v();
    /* If the cluster is above threshold size, use
       adj_counts_to_cluster to update best_cut */
#if 0
    print_unsigned(cut);
    print_unsigned(cluster_count);
    ++cluster_count;
    print_unsigned(cut);
    print_all(cluster,cut);
#endif
    if (cut >= start_search) {
      unsigned cn_len = 0;
      grt_stl_map_foreach(unsigned,unsigned,adj_counts_to_cluster,
			  pair) {
	if (pair->first) cn_len += pair->second;
      }
      if (cn_len <= cn_cut) {       
	cn_cut = cn_len;
	best_cut = cut;
	unsigned i = 0;
	best_adj_count_length = adj_counts_to_cluster.size();
	grt_stl_map_foreach(unsigned,unsigned,adj_counts_to_cluster,
			    pair) {
	  best_adj_count_pairs[i].first = pair->first;
	  best_adj_count_pairs[i].second = pair->second;
	  ++i;
	}
      }
      if (cut >= params->max_cluster_size) {
#if 0
	INDENT;
	printf("exceeded max cluster size, breaking out of loop\n");
#endif
	SET_INDENT(-2);
	break;
      }
    }
    SET_INDENT(-2);
  }

  /* If there was a best cut, store it back into cut and update
     cut_links.  Also update adj_counts_to_cluster and
     adj_set_to_cluster. */

  cluster_size = cut;
  if (best_cut != 0) {
    cut = best_cut;
    adj_counts_to_cluster.clear();
    for (unsigned i = 0; i < best_adj_count_length; ++i) {
      adj_counts_to_cluster[best_adj_count_pairs[i].first] =
	best_adj_count_pairs[i].second;
    }    
    update_adj_set_to_cluster();
    cut_links = cut_links + cn_cut;
  }

  SET_INDENT(-2);
}

/* Add a new cluster to the cluster array */

void add_cluster(void) {
  unsigned i,j;
  unsigned start = cluster_end;

  cluster_starts[cluster_number] = start;
  cluster_number = cluster_number+1;
  cluster_end = cluster_end + cut;
  cluster_set.length = cluster_end;
  for (i = start, j = 0; i < cluster_set.length &&
	 j < cut; ++i, ++j)
    cluster_set.data[i] = cluster[j];
}

cc_output_t cut_clusters(graph_t *graph, 
			 params_t *params,
			 FILE *fp) {
  printf("cut clusters start\n");
  unsigned i,j;

  unsigned index = 0;
  /* Note that some additional logic is needed here in the parallel
     case */
  while (index < num_v) {
    INDENT;
#if 0
    printf("main loop %u\n", main_loop_count++);
#endif
    SET_INDENT(2);
    unsigned v = perm_idx[index];
#if 0
    print_unsigned(index);
    print_unsigned(v);
#endif
    if (!vertex_states[v]) {
      /* vertex is taken, move on */
      ++index;
    } else {
      /* Compute the cluster starting with vertex */
      compute_cluster(v, params);
      /* Claim vertices in cluster[0:cut], return other vertices
	 to adj_set_to_cluster */
      claim_vertices(params);
      /* Add the cluster to the output */
      add_cluster();
#if 0
      print_unsigned(cluster_number);
      print_array(cluster_set);
      print_all(cluster_starts, cluster_number);
#endif
    }
    SET_INDENT(-2);
  }

#if 0
  printf("FINAL RESULT:\n");
  print_unsigned(cluster_number);
  print_array(cluster_set);
  print_all(cluster_starts, cluster_number);
#endif

  result.cluster_set = cluster_set;
  result.cluster_starts.length = cluster_number;
  result.cluster_starts.data = cluster_starts;
  result.cut_links = cut_links;

  return result;
}
