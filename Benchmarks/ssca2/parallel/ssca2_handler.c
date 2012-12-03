#include "stm-distrib.h"
#include "cut_clusters.h"
#include "ssca2_handler.h"
#include "assert.h"

#ifndef STM
GRT_ON_HANDLER_DEF(lock_all);
GRT_ON_HANDLER_DEF(write_state);
GRT_ON_HANDLER_DEF(update_adj_list);
GRT_ON_HANDLER_DEF(write_vertices);
#endif


#ifdef STM
STM_ON_HANDLER_DEF(cache_vertex);
STM_ON_HANDLER_DEF(write_state);
STM_ON_HANDLER_DEF(update_adj_list);
STM_ON_HANDLER_DEF(write_vertices);
#endif

#ifndef STM
static gasnet_handlerentry_t ssca2_entry_table[SSCA2_TABLE_SIZE] = {
  GRT_TABLE,
  SSCA2_TABLE_SEGMENT
};

void ssca2_init(int argc, char** argv) {
  entry_table = ssca2_entry_table;
  table_size = SSCA2_TABLE_SIZE;
  grt_init(argc, argv);
}
#else
static gasnet_handlerentry_t ssca2_entry_table[SSCA2_TABLE_SIZE] = {
  GRT_TABLE,
  STM_TABLE_SEGMENT,
  SSCA2_TABLE_SEGMENT
};

void ssca2_init(int argc, char** argv) {
  entry_table = ssca2_entry_table;
  table_size = SSCA2_TABLE_SIZE;
  stm_init(argc, argv, SSCA2_SHARE_POWER);
}
#endif


#ifdef STM
size_t cache_vertex_local(gasnet_node_t src_proc, 
			  void *arg_buf_void, size_t arg_len,
			  void *result_buf_void) {
  unsigned* arg_buf = (unsigned*) arg_buf_void;
  unsigned *result_buf = (unsigned*) result_buf_void;
  unsigned i = 0;
  size_t nv = arg_len / sizeof(unsigned);
  unsigned result_buf_idx = 0;
  for (i = 0; i < nv; ++i) {
    unsigned v = arg_buf[i];
    unsigned idx = IDX(v);
    array_unsigned_t* rav = RAV(grt_id, idx);
    size_t array_len;
    stm_get(src_proc, &array_len, grt_id, &rav->length, sizeof(unsigned));
    unsigned *data = rav->data;
    result_buf[result_buf_idx] = array_len;
    stm_get(src_proc, &result_buf[result_buf_idx+1], grt_id, data, array_len * sizeof(unsigned));
    result_buf_idx += array_len+1;
  }
  return result_buf_idx * sizeof(unsigned);
}
#endif

#ifndef STM
gasnet_hsl_t state_lock = GASNET_HSL_INITIALIZER;

size_t lock_all_local(void *arg_buf_void, size_t arg_len,
		      void *result_buf_void) {
  unsigned result_idx = 0;
  unsigned *arg_buf = (unsigned*) arg_buf_void;
  unsigned *result_buf = (unsigned*) result_buf_void;
  gasnet_node_t src_proc = arg_buf[0];
  size_t size = arg_buf[1];
  unsigned i;
  for (i = 0; i < size; ++i) {
    unsigned v = arg_buf[2+i];
    unsigned idx = IDX(v);
    array_unsigned_t* rav = RAV(grt_id, idx);
    gasnet_hsl_lock(&state_lock);
    unsigned state = vertex_states[idx];
    if (state == 1) {
      vertex_states[idx] = src_proc+2;
    }
    gasnet_hsl_unlock(&state_lock);
    result_buf[result_idx++] = state;
    if (state == 1) {
      /* Get data into cache */
      size_t array_len = rav->length;
      unsigned *data = rav->data;
      result_buf[result_idx++] = array_len;
      memcpy(&result_buf[result_idx], data, 
	     array_len * sizeof(unsigned));
      result_idx += array_len;
    }
  }
  return result_idx * sizeof(unsigned);
}
#endif

#ifndef STM
GRT_ON_LOCAL_FN_DECL(write_state) {
  grt_word_t *arg_buf_uint = (grt_word_t*) arg_buf;
  unsigned v = arg_buf_uint[0];
  unsigned state = arg_buf_uint[1];
  unsigned *state_ptr = &vertex_states[IDX(v)];
  gasnet_hsl_lock(&state_lock);
  *state_ptr = state;
  gasnet_hsl_unlock(&state_lock);
  return 0;
}
#else
STM_ON_LOCAL_FN_DECL(write_state) {
  unsigned v = *((unsigned*) arg_buf);
  unsigned *state_ptr = &vertex_states[IDX(v)];
  unsigned state = 0;
  stm_put(src_proc, grt_id, state_ptr, &state, sizeof(unsigned));
  return 0;
}
#endif


#ifndef STM
GRT_ON_LOCAL_FN_DECL(update_adj_list) {
  unsigned *arg_buf_uint = (unsigned*) arg_buf;
  size_t nclaimed = arg_buf_uint[0];
  unsigned buf_idx = 1;
  unsigned i;
  for (i = 0; i < nclaimed; ++i) {
    unsigned v = arg_buf_uint[buf_idx++];
    vertex_states[IDX(v)] = 0;
  }
#if 1
  size_t nunclaimed = arg_buf_uint[buf_idx++];
  for (i = 0; i < nunclaimed; ++i) {
    unsigned v = arg_buf_uint[buf_idx++];
    vertex_states[IDX(v)] = 1;
  }
#endif
  while (buf_idx * sizeof(unsigned) < arg_nbytes) {
    unsigned v = arg_buf_uint[buf_idx];
    array_unsigned_t *rav = RAV(grt_id, IDX(v));
    rav->length = arg_buf_uint[buf_idx+1];
    memcpy(rav->data, &arg_buf_uint[buf_idx+2], rav->length * sizeof(unsigned));
    gasnet_hsl_lock(&state_lock);
    vertex_states[IDX(v)] = 1;
    gasnet_hsl_unlock(&state_lock);
    buf_idx += rav->length + 2;
  }
  return 0;
}
#else
STM_ON_LOCAL_FN_DECL(update_adj_list) {
  unsigned *arg_buf_uint = (unsigned*) arg_buf;
  size_t nclaimed = arg_buf_uint[0];
  unsigned buf_idx = 1;
  unsigned i;
  for (i = 0; i < nclaimed; ++i) {
    unsigned v = arg_buf_uint[buf_idx++];
    vertex_states[IDX(v)] = 0;
  }
  while (buf_idx * sizeof(unsigned) < arg_nbytes) {
    unsigned v = arg_buf_uint[buf_idx];
    array_unsigned_t *rav = RAV(grt_id, IDX(v));
    unsigned length = arg_buf_uint[buf_idx+1];
    stm_put(src_proc, grt_id, &rav->length,
	    &arg_buf_uint[buf_idx+1], sizeof(unsigned));
    stm_put(src_proc, grt_id, rav->data, 
	    &arg_buf_uint[buf_idx+2], length * sizeof(unsigned));
    unsigned one = 1;
    stm_put(src_proc, grt_id, &vertex_states[IDX(v)],
	    &one, sizeof(unsigned));
    buf_idx += length + 2;
  }
  return 0;
}
#endif

#ifndef STM
GRT_ON_LOCAL_FN_DECL(write_vertices) {
  unsigned *arg_buf_uint = (unsigned*) arg_buf;
  unsigned len = arg_buf_uint[0];
  unsigned val = arg_buf_uint[1];
  unsigned *vertices = &arg_buf_uint[2];
  unsigned i;
  for (i = 0; i < len; ++i) {
    unsigned v = vertices[i];
    if (PROC(v) == grt_id) {
      unsigned *state_ptr = &vertex_states[IDX(v)];
      gasnet_hsl_lock(&state_lock);
      *state_ptr = val;
      gasnet_hsl_unlock(&state_lock);
    }
  }
  return 0;
}
#else
STM_ON_LOCAL_FN_DECL(write_vertices) {
  unsigned *arg_buf_uint = (unsigned*) arg_buf;
  unsigned len = arg_buf_uint[0];
  unsigned val = arg_buf_uint[1];
  unsigned *vertices = &arg_buf_uint[2];
  unsigned i;
  for (i = 0; i < len; ++i) {
    unsigned v = vertices[i];
    if (PROC(v) == grt_id) {
      unsigned *state_ptr = &vertex_states[IDX(v)];
      stm_put(src_proc, grt_id, state_ptr, &val,
	      sizeof(unsigned));
    }
  }
  return 0;
}
#endif
