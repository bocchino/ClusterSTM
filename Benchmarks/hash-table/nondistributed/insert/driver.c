#include <stdio.h>
#include <assert.h>

#include "gasnet.h"
#include "../lib/hashtable.h"
#include "gasnet_tools.h"

#define TABLE_PROC 0
#define TABLE_SIZE 1000
#define TARGET_N 100
#define WRITERS (grt_num_procs - 1)
#define WRITER (grt_id - 1)
#define N (WRITERS * (TARGET_N / WRITERS))
#define N_PER_WRITER (N / WRITERS)
#define NUM_RUNS 5

grt_word_t numbers[TARGET_N];
double time_per_op;
double max_time;

#define ARRAY_SIZE 10000000
int array[ARRAY_SIZE];
grt_word_t *times;

void run() {
  unsigned i;
  gasnett_tick_t start, end, time;

  hashtable_create(TABLE_PROC, TABLE_SIZE);

  if (grt_id > 0) {
    start = gasnett_ticks_now();
    for (i = 0; i < N_PER_WRITER; ++i) {
      hashtable_insert(grt_id + grt_num_procs * i);
    }
    end = gasnett_ticks_now();
    unsigned time = ((unsigned) gasnett_ticks_to_us(end - start));
    printf("processor %u: insertion time=%f us\n", 
	   grt_id, (double) time);
    fflush(stdout);
    grt_write(0, time, &times[WRITER]);
  }

  BARRIER();

  /* sanity check */
  if (grt_id > 0) {
    for (i = 0; i < N_PER_WRITER; ++i) {
      grt_word_t num = grt_id + grt_num_procs * i;
      unsigned found = hashtable_find(num);
      if (!found) {
	fprintf(stderr, "processor %d: expected to find %d\n", 
		grt_id, (int) num);
      } 
    }
  }

  BARRIER();

  if (grt_id == 0) {
    time = 0, max_time = 0;
    for (i = 0; i < WRITERS; ++i) {
      gasnett_tick_t this_time = times[i];
      time += this_time;
      if (this_time >= max_time) max_time = this_time;
    }
    time_per_op = ((float) time) / N;
    printf("total CPU time=%f us\n", (double) time);
    printf("time per operation=%f us\n", time_per_op);
    printf("max time=%f us\n", (double) max_time);
  }

  /* Flush the cache */
  
  for (i = 0; i < ARRAY_SIZE; ++i)
    time += array[i];

  hashtable_destroy();

  BARRIER();

}


int main(int argc, char** argv) {
  INIT(argc, argv);

  unsigned i;

  if (grt_id == 0) {
    if (grt_num_procs < 2) {
      fprintf(stderr,"%s: number of processors must be at least 2\n",argv[0]);
      exit(1);
    }
    printf("TABLE_SIZE=%u\n", TABLE_SIZE);
    printf("N=%u\n", N);
    printf("WRITERS=%u\n", WRITERS);
    printf("N_PER_WRITER=%u\n", N_PER_WRITER);
  }

  double sum = 0;
  double min_time_per_op = -1;
  double min_of_max = -1;
  times = 
    (grt_word_t*) grt_all_alloc(0, WRITERS * sizeof(grt_word_t));

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

  EXIT(0);
}
