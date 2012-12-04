#include "grt.h"
#include "grt_macros.h"
#include "gasnet_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* AM Handler Table */

#define FIRST_HANDLERS_ID (LAST_GRT_ID + 1)
#define HANDLERS_ID(x) (FIRST_HANDLERS_ID + x)

#define MEDIUM_HANDLER_ID             HANDLERS_ID(0)
#define MEDIUM_RT_HANDLER_ID          HANDLERS_ID(1)
#define LAST_HANDLERS_ID MEDIUM_RT_HANDLER_ID
#define HANDLERS_TABLE_SIZE (GRT_TABLE_SIZE + \
  (LAST_HANDLERS_ID - FIRST_HANDLERS_ID) + 1)

#define DECLS \
  gasnett_tick_t start, end; \
  unsigned i;
#define LOOP for (i = 0; i < NUM_ITERS; ++i)
#define START start = gasnett_ticks_now()
#define END end = gasnett_ticks_now()
#define ELAPSED gasnett_ticks_to_us(end - start)
#define INDEX (rand() % array_size)
#define BASELINE(loop_body) \
  DECLS; \
  START; LOOP { loop_body; } END; \
  baseline = ELAPSED;
#define TIMING(loop_body) \
  START; LOOP { loop_body; } END; \
  return ELAPSED - baseline;
#define TIMING_NBI(loop_body)			\
  START; LOOP { loop_body; }			\
  END;						\
  gasnet_wait_syncnbi_all();			\
  return ELAPSED - baseline;

gasnet_node_t remote_proc;

void medium_handler(gasnet_token_t token, 
		    void *buf, size_t nbytes) {
  volatile int x = 0;
}

void medium_rt_handler(gasnet_token_t token, 
		       void *buf, size_t nbytes,
		       GRT_H_PARAM(state)) {
  volatile int x = 0;
  GASNET_Safe(gasnetc_AMReplyShortM(token, VOID_REPLY_HANDLER_ID, 
				    GRT_NUM_H_ARGS(1),
				    GRT_H_ARG(state)));
}

#define HANDLERS_TABLE_SEGMENT				\
  { MEDIUM_HANDLER_ID, medium_handler }, \
  { MEDIUM_RT_HANDLER_ID, medium_rt_handler }

static gasnet_handlerentry_t 
handlers_entry_table[HANDLERS_TABLE_SIZE] = {
  GRT_TABLE,
  HANDLERS_TABLE_SEGMENT
};

#define NUM_ITERS 1000

grt_word_t source;

gasnett_tick_t medium() {
  DECLS;
  START;
  LOOP {
    GASNET_Safe(gasnetc_AMRequestMediumM(remote_proc,
					 MEDIUM_HANDLER_ID, 
					 (void*) &source, 0, 0));
  }
  END;
  return ELAPSED;
}

gasnett_tick_t medium_rt() {
  DECLS;
  START;
  LOOP {
    grt_handler_state_t state = PENDING;
    GASNET_Safe(gasnetc_AMRequestMediumM(remote_proc,
					 MEDIUM_RT_HANDLER_ID, (void*) &source, 0, 
					 GRT_NUM_H_ARGS(1),
					 GRT_WORD_TO_H_ARG(&state)));
    GASNET_BLOCKUNTIL(state == DONE);
  }
  END;
  return ELAPSED;
}

void run(gasnett_tick_t (*kernel)(), const char *s) {
    gasnett_tick_t time;
    grt_debug_print("starting %s\n",s);
    fflush(stdout);
    time = kernel();
    double single_op_time = ((double) time) / NUM_ITERS;
    grt_debug_print("%s: %f us\ntotal time=%0f us, %u iterations\n",
		    s, single_op_time, (double) time, NUM_ITERS);
    fflush(stdout);
}

void run_all() {
  run(medium, "medium");
  run(medium_rt, "medium round trip");
}
 
int main(int argc, char **argv) {

  entry_table = handlers_entry_table;
  table_size = HANDLERS_TABLE_SIZE;
  grt_init(argc, argv);

  remote_proc = (grt_id == 0) ? 1 : 0;

  if (grt_id == 0) {
    printf("\nONE WORK PROCESSOR:\n");
    fflush(stdout);
    run_all();
    printf("\nTWO WORK PROCESSORS:\n");
    fflush(stdout);
  }

  grt_barrier();
  
  run_all();

  grt_exit(0);

}

