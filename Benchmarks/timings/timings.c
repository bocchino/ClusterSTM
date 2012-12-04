#include "gasnet.h"
#include "grt_macros.h"
#include "gasnet_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define LARGE_ARRAY_SIZE 1048576
#define SMALL_ARRAY_SIZE 8

#define SHORT 100
#define LONG  100000

#define DECLS \
  gasnett_tick_t start, end; \
  gasnett_tick_t baseline, i;
#define LOOP for (i = 0; i < len; ++i)
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

#define CAS_HANDLER_ID 128
#define CAS_REPLY_HANDLER_ID 129

#if SIZEOF_VOID_P == 4
  #define PTR_LO(ptr) ptr
  #define PTR_HI(ptr) 0
  #define PTR(type, lo, hi) ((type*) lo)
#else
  #define PTR_LO(ptr) \
    ((unsigned) ((long long) ptr))
  #define PTR_HI(ptr) \
    ((unsigned) (((long long) ptr) >> 32))
  #define PTR(type, lo, hi) \
    ((type*) ((lo & 0xffffffff) | (((long long) hi) << 32)))
#endif

#define LOCAL_ADDR(type, processor, offset) \
  ((type*) (((char*) seginfo[processor].addr) + offset*sizeof(type)))

gasnet_node_t myid;
int numprocs;
unsigned numruns;
char processor_name[MAX_PROCESSOR_NAME];
gasnet_seginfo_t* seginfo;
gasnet_hsl_t lock = GASNET_HSL_INITIALIZER;

void CAS_handler(gasnet_token_t token, 
		 unsigned statep_lo, unsigned statep_hi,
		 unsigned addr_lo, unsigned addr_hi,
		 int old, int new,
		 unsigned resultp_lo, unsigned resultp_hi) {
  gasnet_hsl_lock(&lock);
  int *addr = PTR(int, addr_lo, addr_hi);
  if (*addr == old) {
    *addr = new;
  }
  gasnet_hsl_unlock(&lock);
  GASNET_Safe(gasnet_AMReplyShort5(token, CAS_REPLY_HANDLER_ID, 
				   statep_lo, statep_hi,  
				   *addr, resultp_lo, resultp_hi));
}

/* copy result to its destination; set state flag */
void CAS_reply_handler(gasnet_token_t token, 
		       unsigned statep_lo, unsigned statep_hi,
		       int result, unsigned resultp_lo,
		       unsigned resultp_hi) {
  *PTR(int, resultp_lo, resultp_hi) = result;
  *PTR(int, statep_lo, statep_hi) = 1;
}

static gasnet_handlerentry_t entry_table[] = {
  { CAS_HANDLER_ID, CAS_handler },
  { CAS_REPLY_HANDLER_ID, CAS_reply_handler }
};

int compare_and_swap(unsigned processor, unsigned offset,
		     int old, int new) {
  unsigned state = 0;
  int result = 0;
  int *local_addr = LOCAL_ADDR(int, processor, offset);
  if (processor == myid) {
    gasnet_hsl_lock(&lock);
    if (*local_addr == old) {
      *local_addr = new;
    }
    result = *local_addr;
    gasnet_hsl_unlock(&lock);
  } else {
    GASNET_Safe(gasnet_AMRequestShort8(processor, CAS_HANDLER_ID, 
				       PTR_LO(&state), PTR_HI(&state), 
				       PTR_LO(local_addr), PTR_HI(local_addr),
				       old, new, 
				       PTR_LO(&result), PTR_HI(&result)));
    GASNET_BLOCKUNTIL(state == 1);
  }
  return result;
}


int result[LARGE_ARRAY_SIZE];
int local_array[LARGE_ARRAY_SIZE];

gasnett_tick_t read_local(unsigned array_size,
			  unsigned len) {
  BASELINE(result[INDEX] = INDEX);
  TIMING(result[INDEX] = local_array[INDEX]);
}

gasnett_tick_t read_remote(unsigned array_size,
			   unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING(gasnet_get(&result, 0, LOCAL_ADDR(int, 0, INDEX), 
		    sizeof(int)));
}

gasnett_tick_t read_remote_nbi(unsigned array_size,
			       unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING_NBI(gasnet_get_nbi(&result, 0, LOCAL_ADDR(int, 0, INDEX),
			    sizeof(int)));
}

gasnett_tick_t write_local(unsigned array_size,
			   unsigned len) {
  volatile int val;
  BASELINE(val = result[INDEX]);
  TIMING(local_array[INDEX] = result[INDEX]);
}

gasnett_tick_t write_remote(unsigned array_size,
			    unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING(gasnet_put(0, LOCAL_ADDR(int, 0, INDEX),
		    &result, sizeof(int)));
}

gasnett_tick_t write_remote_nbi(unsigned array_size,
				unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING_NBI(gasnet_put_nbi(0, LOCAL_ADDR(int, 0, INDEX),
			    &result, sizeof(int)));
}

gasnett_tick_t cas_local(unsigned array_size,
			 unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING(compare_and_swap(1, INDEX, 0, 0));
}

gasnett_tick_t cas_remote(unsigned array_size,
			  unsigned len) {
  volatile int val;
  BASELINE(val = *(LOCAL_ADDR(int, 0, INDEX)));
  TIMING(compare_and_swap(0, INDEX, 0, 0));
}

void initialize(int argc, char **argv) {
  /* call startup */
  GASNET_Safe(gasnet_init(&argc, &argv));		

  /* get SPMD info */
  myid = gasnet_mynode();
  numprocs = gasnet_nodes();
  gethostname(processor_name, MAX_PROCESSOR_NAME);

  /* Attach to network */
  GASNET_Safe(gasnet_attach(entry_table, 2, 
			    2 * LARGE_ARRAY_SIZE * sizeof(int), MINHEAPOFFSET));
  printf("processor %d is %s\n", myid, processor_name);

  /* Get segment info */
  seginfo = (gasnet_seginfo_t*) malloc(sizeof(gasnet_seginfo_t) *
				       numprocs);
  GASNET_Safe(gasnet_getSegmentInfo(seginfo, numprocs));
}
 
void run(gasnett_tick_t (*kernel)(), const char* msg, 
         unsigned array_size, unsigned len) {
  gasnett_tick_t time;
  printf("%s:  ", msg);
  time = kernel(array_size, len);
  double single_op_time = ((double) time) / len;
  printf("%f us\n", single_op_time);
  printf("  total time=%.0f us, ", (double) time);
  printf("%u iterations\n", len);
  fflush(stdout);
}

int main(int argc, char **argv) {

  initialize(argc, argv);

  fflush(stdout);
  BARRIER();

  if (myid == 1) {
    run(read_local, "local read, small array", SMALL_ARRAY_SIZE, LONG);
    run(read_local, "local read, large array", LARGE_ARRAY_SIZE, LONG);
    run(read_remote, "remote read, small array", SMALL_ARRAY_SIZE, SHORT);
    run(read_remote, "remote read, large array", LARGE_ARRAY_SIZE, SHORT);
    run(read_remote_nbi, "remote read non-blocking implicit, small array",
	SMALL_ARRAY_SIZE, SHORT);
    run(read_remote_nbi, "remote read non-blocking implicit, large array",
	LARGE_ARRAY_SIZE, SHORT);
    run(write_local, "local write, small array", SMALL_ARRAY_SIZE, LONG);
    run(write_local, "local write, large array", LARGE_ARRAY_SIZE, LONG);
    run(write_remote, "remote write, small array", SMALL_ARRAY_SIZE, SHORT);
    run(write_remote, "remote write, large array", LARGE_ARRAY_SIZE, SHORT);
    run(write_remote_nbi, "remote write non-blocking implicit, small array", 
	SMALL_ARRAY_SIZE, SHORT);
    run(write_remote_nbi, "remote write non-blocking implicit, large array", 
	LARGE_ARRAY_SIZE, SHORT);
    run(cas_local, "local cas, small array", SMALL_ARRAY_SIZE, LONG);
    run(cas_local, "local cas, large array", LARGE_ARRAY_SIZE, LONG);
    run(cas_remote, "remote cas, small array", SMALL_ARRAY_SIZE, SHORT);
    run(cas_remote, "remote cas, large array", LARGE_ARRAY_SIZE, SHORT);
  }

  fflush(stdout);
  BARRIER();

  if (myid == 0)
    gasnet_exit(0);

  return result[rand() % LARGE_ARRAY_SIZE];

}

