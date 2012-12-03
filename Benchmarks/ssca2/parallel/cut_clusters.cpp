extern "C" {
#include "stm-distrib.h"
#include "grt_debug.h"
#include "ssca2_handler.h"
}
#include "cut_clusters.h"
#include "grt_macros.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <map>
#include <set>
#include <iostream>

using namespace std;

#if 0
#define SSCA2_BLOCKUNTIL(x) \
while(!(x)) { \
  gasnet_AMPoll(); \
  sched_yield(); \
}
#else
#define SSCA2_BLOCKUNTIL(x) \
GASNET_BLOCKUNTIL(x)
#endif

/* DATA SHARED ACROSS PROCESSORS */

/* Copy of graph->adjLists, destroyed as edges are cut */
array_array_unsigned_t_t* remaining_adj_lists;

/* State of each vertex: 0 = permanently unavailable; 1 = available;
   n>2 = locked by processor n-2 (may become available). */
unsigned *vertex_states;

/* PROCESSOR LOCAL DATA */

/* Number of vertices in the graph */
unsigned global_num_v;

/* Max row length for adjacency lists */
size_t max_row_len;

/* Number of computing processors */
unsigned num_compute_procs;

/* Number of vertices assigned to this processor */
unsigned local_num_v;

/* Offset into this processor's section of the graph */
unsigned local_base;

/* Where to start the search for a cut in the vertices we've
   collected */
unsigned start_search;

/* For each vertex, the total number of adjacencies */
unsigned *adj_counts_tot;

/* Permutation indices of sorted adj_counts_tot */
unsigned *perm_idx;

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
size_t best_adj_count_length;

/* Temporary storage */
unsigned *tmp_buf;
unsigned *write_states_buf;

/* Set of vertices we've locked */
map<unsigned, array_unsigned_t> vertex_cache;

/* Set of adjacencies to the current cluster */
array_unsigned_t adj_set_to_cluster;

/* Minimum cut fanout so far */
unsigned cn_cut;

/* Temporary storage for the current best cut */
unsigned best_cut = 0;

/* The actual cut */
unsigned cut;

/* The number of clusters we've computed */
unsigned cluster_number = 0;

/* The number of cuts we have made */
#ifndef STM
unsigned cut_links = 0;
#else
unsigned *cut_links;
#endif

/* The final result of cut_clusters */
static cc_output_t result;

void write_states(unsigned vstate, unsigned *vertices,
		  size_t len) {
  unsigned i, j;
  set<gasnet_node_t> nodes;
  map<gasnet_node_t, set<unsigned> > vertex_map;
  for (i = 0; i < len; ++i) {
    unsigned v = vertices[i];
    gasnet_node_t proc = PROC(v);
    nodes.insert(proc);
    if (!vertex_map.count(proc)) {
      vertex_map[proc] = set<unsigned>();
    }
    vertex_map[proc].insert(v);
  }
  size_t size = nodes.size();
  grt_handler_state_t states[size];
  size_t result_size;
  i = 0, j = 0;
  unsigned buf_idx = 0;
  grt_stl_set_foreach(gasnet_node_t,nodes,node) {
    states[i] = PENDING;
    gasnet_node_t proc = *node;
    size_t v_size = vertex_map[proc].size();
    write_states_buf[buf_idx] = v_size;
    write_states_buf[buf_idx+1] = vstate;
    j = 2;
    grt_stl_set_foreach(unsigned,vertex_map[proc],v) {
      write_states_buf[buf_idx+j++] = *v;
    }
#ifndef STM
    grt_on_nb(proc, WRITE_VERTICES_HANDLER_ID, write_vertices_local,
	      &write_states_buf[buf_idx], (v_size+2) * sizeof(unsigned), 0, 0,
	      &result_size, &states[i]);
#ifndef NB_ON
    SSCA2_BLOCKUNTIL(states[i] == DONE);
#endif
#else
    stm_on(grt_id, proc, WRITE_VERTICES_HANDLER_ID, write_vertices_local,
	   &write_states_buf[buf_idx], (v_size+2) * sizeof(unsigned), 0, 0);
#endif
    buf_idx += j;
    ++i;
  }
#ifdef NB_ON
  for (i = 0; i < nodes.size(); ++i) {
    SSCA2_BLOCKUNTIL(states[i] == DONE);
  }
#endif
}

#ifndef STM
void unlock_all() {
  PRINT_LOCKING("unlocking all vertices: ");
#if SHOW_LOCKING
  grt_stl_foreach(vertex_cache, I) {
    printf("%u ", I->first);
  }
  printf("\n");
  fflush(stdout);
#endif
  unsigned i = 0;
  unsigned my_buf[vertex_cache.size()];
  grt_stl_foreach(vertex_cache, I) {
    my_buf[i++] = I->first;
    free(vertex_cache[I->first].data);
  }
  write_states(1, my_buf, vertex_cache.size());
  vertex_cache.clear();
}
#endif

#ifdef STM
void cache_all(unsigned *vertices, unsigned len) {
  unsigned i = 0;
  set<gasnet_node_t> nodes;
  map<gasnet_node_t, set<unsigned> > vertex_map;
   for (i = 0; i < len; ++i) {
    unsigned v = vertices[i];
    gasnet_node_t proc = PROC(v);
    nodes.insert(proc);
    if (!vertex_map.count(proc)) {
      vertex_map[proc] = set<unsigned>();
    }
    vertex_map[proc].insert(v);
  }
  size_t size = nodes.size();

  grt_stl_set_foreach(gasnet_node_t,nodes,node) {
    gasnet_node_t proc = *node;
    size_t nv = vertex_map[proc].size();
    unsigned arg_buf[nv];
    unsigned *result_buf = (unsigned*) malloc(nv*(max_row_len+1)*sizeof(unsigned));
    i = 0;
    grt_stl_set_foreach(unsigned, vertex_map[proc], v_iter) {
      arg_buf[i++] = *v_iter;
    }
    unsigned result_buf_idx = 0;
    size_t result_size = stm_on(grt_id, proc, CACHE_VERTEX_HANDLER_ID, cache_vertex_local,
				arg_buf, nv * sizeof(unsigned), result_buf,
				nv * (max_row_len+1) * sizeof(unsigned));
    assert(result_size <= nv * (max_row_len+1) * sizeof(unsigned));
    for (i = 0; i < nv; ++i) {
      unsigned v = arg_buf[i];
      array_unsigned_t array;
      array.length = result_buf[result_buf_idx];
      array.data = (unsigned*) malloc(array.length * sizeof(unsigned));
      memcpy(array.data, &result_buf[result_buf_idx+1], array.length * sizeof(unsigned));
      vertex_cache[v] = array;
      result_buf_idx += result_buf[result_buf_idx] + 1;
    }
    free(result_buf);
  }
}
#else
#if SHOW_CONTENTION
size_t locked_count = 0;
size_t claimed_count = 0;
size_t retry_count = 0;
#endif

// This code is horribly ugly because of all the marshalling and
// unmarshalling.  I'll try to refactor.  What I really need are ways
// to send entire data structures or pieces thereof across the
// network; but GASNet doesn't provide that.
//
grt_status_t lock_all(unsigned *vertices, unsigned nv) {
  unsigned i = 0;
  PRINT_LOCKING("locking all ");
#if SHOW_LOCKING
  print_all(vertices, nv);
  fflush(stdout);
#endif
  // Vertices come in as a flat array.  Partition them into sets based
  // on processor, so we can aggregate communication.  Only add
  // vertices we haven't yet locked (i.e, they aren't in the vertex
  // cache).
  set<gasnet_node_t> procs;
  map<gasnet_node_t,set<unsigned> > vertex_map;
  size_t nv_remain = nv;
  grt_status_t status = SUCCEED;
  while (nv_remain && (status == SUCCEED)) {
    procs.clear();
    vertex_map.clear();
    size_t count = 0;
    for (i = 0; i < nv_remain; ++i) {
      unsigned v = vertices[i];
      if (!vertex_cache.count(v)) {
	gasnet_node_t proc = PROC(v);
	procs.insert(proc);
	if (!vertex_map.count(proc))
	  vertex_map[proc] = set<unsigned>();
	vertex_map[proc].insert(v);
	++count;
      }
    }
    unsigned *result_buf = (unsigned*) 
      malloc(count*(max_row_len+2)*sizeof(unsigned));
    nv_remain = 0;
    unsigned arg_buf[count + 2*procs.size()];
    grt_handler_state_t hstates[procs.size()];
    size_t result_size;
    size_t outer_buf_idx = 0;
    size_t result_buf_idx = 0;
    unsigned j = 0;
    grt_stl_set_foreach(gasnet_node_t,procs,proc_iter) {
      gasnet_node_t proc = *proc_iter;
      size_t vmap_size = vertex_map[proc].size();
      arg_buf[outer_buf_idx] = grt_id;
      arg_buf[outer_buf_idx+1] = vmap_size;
      i = 2;
      grt_stl_set_foreach(unsigned,vertex_map[proc],v_iter) {
	arg_buf[outer_buf_idx+i++] = *v_iter;
      }
      hstates[j] = PENDING;
      unsigned *result = &result_buf[result_buf_idx];
      grt_on_nb(proc, LOCK_ALL_HANDLER_ID, lock_all_local, 
		&arg_buf[outer_buf_idx],
		(2+vmap_size)*sizeof(unsigned), result, 
		vmap_size * (max_row_len+2) 
		* sizeof(unsigned), &result_size, &hstates[j]);
#ifndef NB_ON
      SSCA2_BLOCKUNTIL(hstates[j] == DONE);
#else
      outer_buf_idx += vmap_size+2;
      result_buf_idx += vmap_size * (max_row_len+2);
      ++j;
    }

    outer_buf_idx = 0;
    result_buf_idx = 0;
    j = 0;
    grt_stl_set_foreach(gasnet_node_t,procs,proc_iter) {
      gasnet_node_t proc = *proc_iter;
      size_t vmap_size = vertex_map[proc].size();
      unsigned *result = &result_buf[result_buf_idx];
      SSCA2_BLOCKUNTIL(hstates[j] == DONE);
#endif
      unsigned buf_idx = 0;
      grt_stl_set_foreach(unsigned,vertex_map[proc],v_iter) {
	unsigned v = *v_iter;
	unsigned state = result[buf_idx];
	assert(state != grt_id + 2);
	if (state == 1) {
	  array_unsigned_t array;
	  size_t array_len = result[++buf_idx];
	  array.length = array_len;
	  array.data = (unsigned*) malloc(array.length * sizeof(unsigned));
	  memcpy(array.data, &result[buf_idx+1], 
		 array_len * sizeof(unsigned));
	  vertex_cache[v] = array;
	  buf_idx += array_len;
#if SHOW_ADJACENCIES
	  grt_debug_print("caching vertex %u, ", v);
	  print_array(vertex_cache[v]);
#endif
#if NO_RETRY
	} else if (state != 0) {
#else
	} else if (state < grt_id+2) {
#endif
#if SHOW_CONTENTION
	  ++locked_count;
#endif
	  unlock_all();
	  // Go to sleep for a while, waiting for higher priority
	  // processor to get its lock.
	  sched_yield();
	  status = FAIL;
	} else if (state == 0) {
	  unlock_all();
#if SHOW_CONTENTION
	  ++claimed_count;
#endif
	  status = FAIL;
	} else {
#if SHOW_CONTENTION
	  ++retry_count;
#endif
	  vertices[nv_remain++] = v;
	}
	++buf_idx;
      }
#ifndef NB_ON
      // With blocking on, we can return right here.  With nonblocking
      // on, we may have touched other processors, so we have to clean
      // them up.
      if (status == FAIL) return FAIL;
#endif
      if (nv_remain) {
	sched_yield();
      }
      outer_buf_idx += vmap_size+2;
      result_buf_idx += vmap_size * (max_row_len+2);
      ++j;
    }
    free(result_buf);
  }

  PRINT_LOCKING("locked vertices are ");
#if SHOW_LOCKING
  grt_stl_foreach(vertex_cache, I) {
    printf("%u ", I->first);
  }
  printf("\n");
  fflush(stdout);
#endif
  return SUCCEED;
}
#endif

/* Update adj_set_to_cluster to contain all vertices with positive
   adj_weight */

inline void update_adj_set_to_cluster(void) {
  unsigned *data = adj_set_to_cluster.data;
  adj_set_to_cluster.length = 0;    
  grt_stl_map_foreach(unsigned,unsigned,adj_counts_to_cluster,
		      pair) {
    if (pair->second) {
      ++adj_set_to_cluster.length;
      *data++ = pair->first;
    }
  }
}

void claim_vertices(params_t *params) {
  unsigned i,j;
  PRINT_LOCKING("claiming all ");
#if SHOW_LOCKING
  print_all(cluster, cut);
  fflush(stdout);
#endif

  /* Claim all vertices in cluster[0:cut] */

  for (i = 0; i < cut; ++i) {
    unsigned v = cluster[i];
    free(vertex_cache[v].data);
    vertex_cache.erase(v);
  }

  CHECKPOINT("4.1");

  /* Unmask the non-claimed vertices */

  for (i = cut; i < cluster_size; ++i) {
    cluster_mask.erase(cluster[i]);
  }

  /* Put all unmasked vertices into remaining_adj_lists */

  // All nodes on which we have to do anything
  set<gasnet_node_t> nodes;
  // Collect the claimed vertices
  map<gasnet_node_t, set<unsigned> > claimed_map;
  for (i = 0; i < cut; ++i) {
    unsigned v = cluster[i];
    gasnet_node_t proc = PROC(v);
    nodes.insert(proc);
    if (!claimed_map.count(proc)) {
      claimed_map[proc] = set<unsigned>();
    }
    claimed_map[proc].insert(v);
  }
  size_t nv = 0;
  // Collect the vertices in adj_set_to_cluster
  map<gasnet_node_t, set<unsigned> > acc_map;
  for (i = 0; i < adj_set_to_cluster.length; ++i) {
    unsigned v = adj_set_to_cluster.data[i];
    gasnet_node_t proc = PROC(v);
    nodes.insert(proc);
    if (!acc_map.count(proc)) {
      acc_map[proc] = set<unsigned>();
    }
    acc_map[proc].insert(v);
    ++nv;
  }

#ifdef STM
  size_t buf_sz = cut+nodes.size()+nv*(max_row_len+2);
  unsigned *arg_buf = (unsigned*) malloc((cut+nodes.size()+
					  nv * (max_row_len+2)) * sizeof(unsigned));
#else
  // Collect the rest of the locked vertices
  map<gasnet_node_t, set<unsigned> > residual_map;
  size_t nr = 0;
  grt_stl_map_foreach(unsigned, array_unsigned_t, vertex_cache, iter) {
    unsigned v = iter->first;
    gasnet_node_t proc = PROC(v);
    if (!acc_map[proc].count(v)) {
      nodes.insert(proc);
      if (!residual_map.count(proc)) {
	residual_map[proc] = set<unsigned>();
      }
      residual_map[proc].insert(v);
      ++nr;
    }
  }
  size_t buf_sz = cut + nr + 1 + 3*nodes.size() +
    nv * (max_row_len+2);
  unsigned *arg_buf = (unsigned*) malloc(buf_sz * sizeof(unsigned));
#endif
  grt_handler_state_t hstates[nodes.size()];
  size_t result_size;
  i = 0;
  // Do work on each node.  Assemble a whole bunch of junk into a
  // buffer, then send it out for processing.
  grt_stl_set_foreach(gasnet_node_t,nodes,node) {
    gasnet_node_t proc = *node;
    // Claim the claimed vertices
    size_t cl_size = claimed_map[proc].size();
    unsigned buf_idx = 0;
    arg_buf[buf_idx++] = cl_size;
    grt_stl_set_foreach(unsigned, claimed_map[proc], v_iter) {
      arg_buf[buf_idx++] = *v_iter;
    }
#ifndef STM
    // Unlock the residual vertices
    size_t rmap_size = residual_map[proc].size();
    arg_buf[buf_idx++] = rmap_size;
    grt_stl_set_foreach(unsigned, residual_map[proc], v_iter) {
      unsigned v = *v_iter;
      arg_buf[buf_idx++] = v;
      free(vertex_cache[v].data);
      vertex_cache.erase(v);
    }
#endif
    // Update the adjacency lists for the vertices in adj_set_to_cluster
    grt_stl_set_foreach(unsigned, acc_map[proc], v_iter) {
      unsigned v = *v_iter;
      unsigned idx = IDX(v);
      array_unsigned_t *rav = RAV(proc, idx);
      array_unsigned_t *cache_rav = &vertex_cache[v];
      
      CHECKPOINT("4.2");
      
      unsigned len = cache_rav->length;
      unsigned *cache_data = cache_rav->data;
      unsigned count = 0;
      arg_buf[buf_idx] = v;
      unsigned *my_data = &arg_buf[buf_idx+2];
      
      for (j = 0; j < len; ++j) {
	unsigned vertex = cache_data[j];
	if (!cluster_mask.count(vertex)) {
	  my_data[count++] = vertex;
	}
      }
#if SHOW_ADJACENCIES
      grt_debug_print("putting back vertex %u, ", v);
      print_all(my_data, count);
#endif
      arg_buf[buf_idx+1] = count;
      buf_idx += count+2;
      assert(buf_idx < buf_sz);
    }

    // The actual work, using the assembled arg buffer
#ifndef STM
    hstates[i] = PENDING;
    grt_on_nb(proc, UPDATE_ADJ_LIST_HANDLER_ID, update_adj_list_local,
   	      &arg_buf[0], buf_idx * sizeof(unsigned), 0, 0,
    	      &result_size, &hstates[i]);
#ifndef NB_ON
    SSCA2_BLOCKUNTIL(hstates[i] == DONE);
#endif
    ++i;
#else
    stm_on(grt_id, proc, UPDATE_ADJ_LIST_HANDLER_ID, update_adj_list_local,
	   &arg_buf[0], buf_idx * sizeof(unsigned), 0, 0);
#endif

    grt_stl_set_foreach(unsigned, acc_map[proc], v_iter) {
      unsigned v = *v_iter;
      free(vertex_cache[v].data);
      vertex_cache.erase(v);
    }
  }
  CHECKPOINT("4.3");
  free(arg_buf);

#ifdef NB_ON
  for (i = 0; i < nodes.size(); ++i) {
    SSCA2_BLOCKUNTIL(hstates[i] == DONE);
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

/* Allocate shared data structures */

void init_shared(graph_t *graph) {
  unsigned i,j;

  global_num_v = graph->maxVertex;
  max_row_len = graph->max_row_len;

  // This assumes that num_procs divides global_num_v
  local_num_v = global_num_v / grt_num_procs;
  remaining_adj_lists = (array_array_unsigned_t_t*) 
    malloc(grt_num_procs * sizeof(array_array_unsigned_t_t));

  for (i = 0; i < grt_num_procs; ++i) {
    remaining_adj_lists[i].length = local_num_v;
    // remaining_adj_lists[proc].data points to a shared array of 
    // grt_num_procs x array_unsigned_t
    remaining_adj_lists[i].data = 
      (array_unsigned_t*) 
      grt_all_alloc(i, local_num_v * sizeof(array_unsigned_t));
  }

  vertex_states = (unsigned*) grt_alloc(grt_id, local_num_v * sizeof(unsigned));

  for (i = 0; i < local_num_v; ++i) {
    array_unsigned_t *arr = RAV(grt_id, i);
    unsigned row = grt_id * local_num_v + i;
    unsigned len = row_length(graph->adjLists, row);
    arr->length = len;
    arr->data = (unsigned*) grt_alloc(grt_id, len * sizeof(unsigned));
    unsigned *data = graph->adjLists->data +
      graph->adjLists->row_offsets[row];
    for (j = 0; j < len; ++j)
      arr->data[j] = *data++;
    unsigned state = (len == 0) ? 0 : 1;
    vertex_states[i] = state;
  }
}

/* Free shared data structures */

void cleanup_shared() {
  unsigned i;
  for (i = 0; i < local_num_v; ++i) {
    array_unsigned_t *arr = RAV(grt_id, i);
    grt_free(grt_id, arr->data);
  }
  BARRIER();
  grt_free(grt_id, remaining_adj_lists[grt_id].data);
  BARRIER();
  free(remaining_adj_lists);
  grt_free(grt_id, vertex_states);
  BARRIER();
}

/* Allocate local data structures */

void cut_clusters_init(graph_t *graph,
		       params_t *params,
		       FILE *fp) {
  unsigned v, i, j;

  init_shared(graph);

  if (params->max_cluster_size < 1)
    error("max_cluster_size must be at least one");
  if (params->alpha < 0 || params->alpha > 1)
    error("alpha must be between 0 and 1 inclusive");

  local_base = grt_id * local_num_v;
  start_search = ceil(params->alpha * params->max_cluster_size);
  adj_counts_tot = 
    (unsigned*) malloc(local_num_v * sizeof(unsigned));
  for (i = 0; i < local_num_v; ++i) {
    unsigned len = row_length(graph->adjLists, local_base + i);
    adj_counts_tot[i] = len;
  }
  perm_idx =
    (unsigned*) malloc(local_num_v * sizeof(unsigned));

  /* Sort vertices by out-degree and store the resulting permutation
     indices */

  for (i = 0; i < local_num_v; ++i)
    perm_idx[i] = i;
  //qsort(perm_idx, local_num_v, sizeof(unsigned), ind_adj_cmp);
  for (i = 0; i < local_num_v; ++i)
    perm_idx[i] += local_base;

  cluster_set.length = 0;
  cluster_set.data = (unsigned*)
    calloc((size_t) ceil((double) global_num_v), sizeof(unsigned));
  cluster_starts = (unsigned*)
    malloc(global_num_v * sizeof(unsigned));
  // This is because of a BUG in the Matlab implementation: should be
  // bounded by max cluster size!
  cluster = (unsigned*)
    calloc(global_num_v, sizeof(unsigned));

  new_adj.length = 0;
  new_adj.data = 
    (unsigned*) malloc(global_num_v * sizeof(unsigned));

  new_adj_best.length = 0;
  new_adj_best.data = 
    (unsigned*) malloc(global_num_v * sizeof(unsigned));

  tmp_buf = 
    (unsigned*) malloc((global_num_v+2) * sizeof(unsigned));
  write_states_buf = 
    (unsigned*) malloc((global_num_v+2) * sizeof(unsigned));
  adj_set_to_cluster.length = 0;
  adj_set_to_cluster.data = 
    (unsigned*) malloc(global_num_v * sizeof(unsigned));

#ifdef STM
  cut_links = (unsigned*) stm_alloc(grt_id, grt_id, sizeof(unsigned));
#endif
  best_adj_count_pairs = (std::pair<unsigned,unsigned>*) 
    malloc(global_num_v * sizeof(std::pair<unsigned,unsigned>));
}

/* Free local data structures */

void cut_clusters_cleanup(void) {
  free(adj_counts_tot);
  free(perm_idx);
  free(cluster);
  free(new_adj.data);
  free(new_adj_best.data);
  free(tmp_buf);
  free(write_states_buf);
  free(adj_set_to_cluster.data);
  free(result.cluster_set.data);
  free(result.cluster_starts.data);
  free(best_adj_count_pairs);
  cleanup_shared();
}

unsigned find_next_v(params_t *params) {

  unsigned next_v;
  unsigned i, j;
  
  /* Max of cn similarity results */
  
  unsigned cn_max = 0;
  
  /* For each vertex v adjacent to the current cluster */

  for (i = 0; i < adj_set_to_cluster.length; ++i) {

    unsigned v = adj_set_to_cluster.data[i];
   
    /* v_adjs contains the remaining adjacent vertices of v */
    
    unsigned idx = IDX(v);
    array_unsigned_t *v_adjs = v_adjs = &vertex_cache[v];

    /* new_adj contains all the NEW remaining adjacent vertices of
       v: i.e., v_adjs \ cluster */
    
    /* all is true if adj_counts_to_cluster for each vertex in new_adj
       are nonzero, otherwise false */
    
    bool all_nonzero = true;
    unsigned *data = new_adj.data;

    new_adj.length = 0;
    unsigned len = v_adjs->length;
    unsigned *v_adjs_data = v_adjs->data;
    for (j = 0; j < len; ++j) {
      unsigned v_adj = v_adjs_data[j];
      if (!cluster_mask.count(v_adj)) {
	++new_adj.length;
	*data++ = v_adj;
	if (adj_counts_to_cluster[v_adj] == 0)
	  all_nonzero = false;
      }
    }
    
    /* If adj_counts_to_cluster[i] != 0 for all i in new_adj, then select
       v as next_v and copy new_adj into new_adj_best */
    
    if (all_nonzero) {
      next_v = v;
      new_adj_best.length = new_adj.length;
      for (j = 0; j < new_adj.length; ++j) {
	new_adj_best.data[j] = new_adj.data[j];
      }
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
	for (j = 0; j < new_adj.length; ++j) {
	  new_adj_best.data[j] = new_adj.data[j];
	}
      }
    }
  }

  return next_v;
}

void add_next_v(unsigned next_v) {

  unsigned i,j;

  /* Update cluster, cluster_mask, and adj_counts_to_cluster */

  cluster[cut++] = next_v;
  cluster_mask.insert(next_v);
  adj_counts_to_cluster.erase(next_v);

  for (i = 0; i < new_adj_best.length; ++i) {
    unsigned idx = new_adj_best.data[i];
    ++adj_counts_to_cluster[idx];
  }
  update_adj_set_to_cluster();

}

/* Compute a cluster starting with start_v */

grt_status_t compute_cluster(unsigned start_v, params_t *params) {
  unsigned i,j;

  CHECKPOINT("3.1");

  /* Initialize vars */

  best_cut = 0;
  cn_cut = UINT_MAX;
  cut = 1;

  /* Add start_v to the cluster */

  cluster[0] = start_v;

#ifdef STM
  /* Cache start_v's adjacencies */

  cache_all(&start_v, 1);
#endif

  /* Clear out the cluster mask except for start_v */

  cluster_mask.clear();
  cluster_mask.insert(start_v);
  
  /* current cluster = { start_v } */
  /* adj_set_to_cluster = {v in V : v is adjacent to start_v} */
  /* adj_counts_to_cluster[v] = 1 if v in adj_set_to_cluster, 0 otherwise */

  adj_counts_to_cluster.clear();
  adj_set_to_cluster.length = vertex_cache[start_v].length;
  memcpy(adj_set_to_cluster.data, vertex_cache[start_v].data,
	 adj_set_to_cluster.length * sizeof(unsigned));
  for (i = 0; i < adj_set_to_cluster.length; ++i) {
    adj_counts_to_cluster[adj_set_to_cluster.data[i]] = 1;
  }
#if SHOW_ADJACENCIES
  grt_debug_print("start_v=%u, adjacencies are ", start_v);
  print_array(adj_set_to_cluster);
#endif

#ifndef STM
  if (lock_all(adj_set_to_cluster.data,
	       adj_set_to_cluster.length) == FAIL) { 
    CHECKPOINT("3.2 (fail)");
    return FAIL;
  }
  CHECKPOINT("3.2 (succeed)");
#else
  cache_all(adj_set_to_cluster.data, adj_set_to_cluster.length);
#endif
  
  /* Compute best_cut */

  while (adj_set_to_cluster.length) {

    CHECKPOINT("3.3");

    /* Examine all vertices in adj_set_to_cluster; find next v to add
       to cluster and update new_adj_best */

    unsigned next_v = find_next_v(params);
#if SHOW_ADJACENCIES
    grt_debug_print("next_v=%u, adjacencies are ", next_v);
    print_array(vertex_cache[next_v]);
#endif

#ifndef STM
    if (lock_all(new_adj_best.data,
		 new_adj_best.length) == FAIL) { 
      CHECKPOINT("3.4 (fail)");
      return FAIL;
    }
    CHECKPOINT("3.4 (succeed)");
#else
    cache_all(new_adj_best.data, new_adj_best.length);
#endif

    /* Add next_v to the cluster: Use new_adj_best to update cluster,
       cluster_mask, adj_counts_to_cluster, and adj_set_to_cluster */

    add_next_v(next_v);

    /* If the cluster is above threshold size, use
       adj_counts_to_cluster to update best_cut */

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
	break;
      }
    }
  }

  /* If there was a best cut, store it back into cut and update
     cut_links.  Also update adj_counts_to_cluster and
     adj_set_to_cluster. */

  cluster_size = cut;
  if (best_cut != 0) {
    cut = best_cut;
    adj_counts_to_cluster.clear();
    for (unsigned i = 0; i < best_adj_count_length; ++i) {
      unsigned idx = best_adj_count_pairs[i].second;
      adj_counts_to_cluster[best_adj_count_pairs[i].first] = idx;
    }    
    update_adj_set_to_cluster();
#ifndef STM
    cut_links = cut_links + cn_cut;
#else
    unsigned my_cut_links;
    stm_get(grt_id, &my_cut_links, grt_id, cut_links, sizeof(unsigned));
    my_cut_links += cn_cut;
    stm_put(grt_id, grt_id, cut_links, &my_cut_links, sizeof(unsigned));
#endif
  }

  CHECKPOINT("3.5");

  return SUCCEED;
}

/* Add a new cluster to the cluster array */

void add_cluster(void) {
  unsigned i,j;
  unsigned start = cluster_set.length;
  cluster_starts[cluster_number++] = start;
  cluster_set.length += cut;
  for (i = start, j = 0; i < cluster_set.length && 
	 j < cut; ++i, ++j)
    cluster_set.data[i] = cluster[j];
}

cc_output_t cut_clusters(graph_t *graph, 
			 params_t *params,
			 FILE *fp) {
  unsigned i,j;
  unsigned index = 0;

  while (index < local_num_v) {

    CHECKPOINT("1");
    unsigned v = perm_idx[index];
#ifndef STM
    unsigned state = vertex_states[IDX(v)];
    if (!state) {
      /* vertex is taken, move on */
      ++index;
    } else {
      CHECKPOINT("2");
      if (lock_all(&v, 1) == FAIL) {
	CHECKPOINT("3 (fail)");
	++index;
	continue;
      }
      CHECKPOINT("3 (succeed)");
      /* Compute the cluster starting with vertex */
      if (compute_cluster(v, params) == FAIL) {
	CHECKPOINT("4 (fail)");
	++index;
	continue;
      }
      CHECKPOINT("4 (succeed)");
      /* Claim vertices in cluster[0:cut], return other vertices
	 to adj lists of vertices in adj_set_to_cluster */
      claim_vertices(params);
      CHECKPOINT("5");
      /* Add the cluster to the output */
      add_cluster();
      CHECKPOINT("6");
      ++index;
    }
#else
    stm_start(grt_id);
    grt_stl_foreach(vertex_cache, I) {
      unsigned v = I->first;
      free(vertex_cache[v].data);
    }
    vertex_cache.clear();
    unsigned state;
    stm_get(grt_id, &state, grt_id, &vertex_states[IDX(v)],
	    sizeof(unsigned));
    if (!state) {
      /* vertex is taken, move on */
      ++index;
      stm_commit(grt_id);
      continue;
    }
    /* Compute the cluster starting with vertex */
    compute_cluster(v, params);
    /* Claim vertices in cluster[0:cut], return other vertices
       to adj lists of vertices in adj_set_to_cluster */
    claim_vertices(params);
    stm_commit(grt_id);

    /* Add the cluster to the output */
    add_cluster();
    ++index;
#endif
  }

  result.cluster_set = cluster_set;
  result.cluster_starts.length = cluster_number;
  result.cluster_starts.data = cluster_starts;
#ifndef STM
#if SHOW_CONTENTION
  result.cut_links = cut_links;
  result.locked_count = locked_count;
  result.claimed_count = claimed_count;
  result.retry_count = retry_count;
#endif
#else
  result.cut_links = *cut_links;
#endif

  return result;
}
