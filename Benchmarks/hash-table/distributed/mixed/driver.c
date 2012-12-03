#include <stdio.h>
#include <assert.h>

#include "stm.h"
#include "../lib/hashtable.h"
#include "gasnet_tools.h"
#include "math.h"

#define BUF_SIZE 256
#define NUM_PARAMS 4
typedef enum { HASHTABLE_SIZE, NUM_OPS, NUM_RUNS, PERCENT_PUT } param_id_t;
char buf[BUF_SIZE];
const char *param_names[NUM_PARAMS] = {
  "hashtable-size",
  "num-ops",
  "num-runs",
  "percent-put",
};
unsigned params[NUM_PARAMS] = {
  10000,
  1000,
  2,
  0
};

#define NUM_OPS_ROUNDED (grt_num_procs * (params[NUM_OPS] / grt_num_procs))
#define OPS_PER_PROC (NUM_OPS_ROUNDED / grt_num_procs)

grt_word_t *numbers;
grt_bool_t *put_flags;
double time_per_op;
double max_time;

grt_word_t *times;

void run() {

  unsigned i, time;
  gasnett_tick_t start, end;

  hashtable_create(params[HASHTABLE_SIZE]);

  BARRIER();

  start = gasnett_ticks_now();
  for (i = 0; i < OPS_PER_PROC; ++i) {
    if (put_flags[i] == TRUE) {
      hashtable_insert(numbers[i]);
    } else {
      hashtable_find(numbers[i]);
    }
  }
  end = gasnett_ticks_now();
  time = ((unsigned) gasnett_ticks_to_us(end - start));
  printf("processor %u: execution time=%f us\n", 
	 grt_id, (double) time);
  fflush(stdout);
  grt_write(0, time, &times[grt_id]);

  BARRIER();

  if (grt_id == 0) {
    time = 0, max_time = 0;
    for (i = 0; i < grt_num_procs; ++i) {
      gasnett_tick_t this_time = times[i];
      time += this_time;
      if (this_time >= max_time) max_time = this_time;
    }
    time_per_op = ((float) time) / NUM_OPS_ROUNDED;
    printf("total CPU time=%f us\n", (double) time);
    printf("time per operation=%f us\n", time_per_op);
    printf("max time=%f us\n", (double) max_time);
  }

  BARRIER();

  hashtable_destroy();

  BARRIER();

}

static grt_status_t get_param(char *buf, param_id_t *id) {
  unsigned i;
  for (i = 0; i < NUM_PARAMS; ++i) {
    if (!strncmp(param_names[i], buf, BUF_SIZE)) {
      *id = i;
      return SUCCEED;
    }
  }
  return FAIL;
}

static void get_params() {
  FILE *params_file = fopen("params", "r");
  param_id_t param_id;
  if (!params_file) {
    if (grt_id == 0)
      fprintf(stderr, "error opening params file\n");
    exit(0);
  }
  while (fgets(buf, BUF_SIZE, params_file)) {
    buf[strlen(buf)-1] = 0;
    param_id_t my_param_id;
    if (get_param(buf, &my_param_id) == SUCCEED)
      param_id = my_param_id;
    else if (buf[0] != '%' && buf[0] != 0) {
      if (param_id >= 0 && param_id < NUM_PARAMS)
	params[param_id] = atoi(buf);
    }
  }
  fclose(params_file);
}


int main(int argc, char** argv) {

  unsigned i;

  hashtable_init(argc, argv);

  get_params();
  numbers = malloc(OPS_PER_PROC * sizeof(grt_word_t));
  put_flags = malloc(OPS_PER_PROC * sizeof(grt_bool_t));
  for (i = 0; i < OPS_PER_PROC; ++i) {
    numbers[i] = random();
    unsigned flag_val = ceil(100 * ((float) random() / RAND_MAX));
    put_flags[i] = (flag_val <= params[PERCENT_PUT]) ? 
      GRT_TRUE : GRT_FALSE;
  }

  grt_barrier();

  if (grt_id == 0) {
    printf("User-specified parameters:\n");
    for (i = 0; i < NUM_PARAMS; ++i) {
      printf("  %s=%u\n", param_names[i], params[i]);
    }
    printf("  grt_num_procs=%u\n", grt_num_procs);
    printf("Derived parameters:\n");
    printf("  NUM_OPS_ROUNDED=%u\n", NUM_OPS_ROUNDED);
    printf("  OPS_PER_PROC=%u\n", OPS_PER_PROC);
  }

  double sum = 0;
  double min_time_per_op = -1;
  double min_of_max = -1;
  times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));

  for (i = 0; i < params[NUM_RUNS]; ++i) {
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
    printf("average time per operation=%f us\n", sum / params[NUM_RUNS]);
    printf("min time per op=%f us\n", min_time_per_op);
    printf("min of max=%f us\n", min_of_max);
  }

  hashtable_exit(0);
}
