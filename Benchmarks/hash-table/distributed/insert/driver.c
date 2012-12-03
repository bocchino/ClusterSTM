#include <stdio.h>
#include <assert.h>

#include "stm.h"
#include "../lib/hashtable.h"
#include "gasnet_tools.h"

#define TABLE_SIZE 1000
#define TARGET_N 100
#define NUM_WRITES (grt_num_procs * (TARGET_N / grt_num_procs))
#define WRITES_PER_PROC (NUM_WRITES / grt_num_procs)
#define NUM_RUNS 2

grt_word_t numbers[TARGET_N];
double time_per_op;
double max_time;

grt_word_t *times;

void run() {

  unsigned i;
  gasnett_tick_t start, end;

  hashtable_create(TABLE_SIZE);

  BARRIER();

  start = gasnett_ticks_now();
  for (i = 0; i < WRITES_PER_PROC; ++i) {
    hashtable_insert(grt_id * WRITES_PER_PROC + i);
  }
  end = gasnett_ticks_now();
  unsigned time = ((unsigned) gasnett_ticks_to_us(end - start));
  printf("processor %u: insertion time=%f us\n", 
	 grt_id, (double) time);
  fflush(stdout);
  grt_write(0, time, &times[grt_id]);

  BARRIER();

  /* sanity check */
  for (i = 0; i < WRITES_PER_PROC; ++i) {
    grt_word_t num = grt_id * WRITES_PER_PROC + i;
    grt_bool_t found = hashtable_find(num);
    if (!found) {
      fprintf(stderr, "processor %d: expected to find %d\n", 
	      grt_id, (int) num);
    }
  }

  BARRIER();

  if (grt_id == 0) {
    time = 0, max_time = 0;
    for (i = 0; i < grt_num_procs; ++i) {
      gasnett_tick_t this_time = times[i];
      time += this_time;
      if (this_time >= max_time) max_time = this_time;
    }
    time_per_op = ((float) time) / NUM_WRITES;
    printf("total CPU time=%f us\n", (double) time);
    printf("time per operation=%f us\n", time_per_op);
    printf("max time=%f us\n", (double) max_time);
  }

  BARRIER();

  hashtable_destroy();

  BARRIER();

}


int main(int argc, char** argv) {

  hashtable_init(argc, argv);

  unsigned i;

  if (grt_id == 0) {
    printf("TABLE_SIZE=%u\n", TABLE_SIZE);
    printf("NUM_WRITES=%u\n", NUM_WRITES);
    printf("grt_num_procs=%u\n", grt_num_procs);
    printf("WRITES_PER_PROC=%u\n", WRITES_PER_PROC);
  }

  double sum = 0;
  double min_time_per_op = -1;
  double min_of_max = -1;
  times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));

  for (i = 0; i < NUM_RUNS; ++i) {
    if (grt_id == 0) {
      printf("\nRUN NUMBER %u\n\n", i);
      fflush(stdout);
    }
    BARRIER();
    run();
    if (grt_id == 0) {
      sum += time_per_op;
      if (min_time_per_op == -1 ||
	  time_per_op < min_time_per_op)
	min_time_per_op = time_per_op;
      if (min_of_max == -1 ||
	  max_time < min_of_max)
	min_of_max = max_time;
    }
  }
  
  BARRIER();

  if (grt_id == 0) {
    printf("\nSUMMARY RESULTS\n\n");
    printf("average time per operation=%f us\n", sum / NUM_RUNS);
    printf("min time per op=%f us\n", min_time_per_op);
    printf("min of max=%f us\n", min_of_max);
  }

  hashtable_exit(0);
}
