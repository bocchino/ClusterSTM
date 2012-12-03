#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#include "stm-distrib.h"
#include "../lib/hashset.h"
#include "gasnet_tools.h"
#include "math.h"
#include "grt_random.h"

#define BUF_SIZE 256
#define NUM_PARAMS 6
typedef enum { SHARE_POWER, HASHSET_SIZE, NUM_OPS, NUM_RUNS, 
	       PERCENT_PUT, ON_PTHREAD } param_id_t;
char buf[BUF_SIZE];
const char *param_names[NUM_PARAMS] = {
  "share-power",
  "hashset-size",
  "num-ops",
  "num-runs",
  "percent-put",
  "on-pthread"
};
unsigned params[NUM_PARAMS] = {
  0,
  10000,
  1000,
  1,
  0,
  GRT_FALSE
};

//#define NUM_OPS_ROUNDED (grt_num_procs * (params[NUM_OPS] / grt_num_procs))
#define MY_NUM_OPS grt_distrib_range(params[NUM_OPS])

grt_word_t *numbers;
grt_bool_t *put_flags;
double time_per_op;
double max_time;

grt_word_t *times;

void run() {

  unsigned i, time;
  gasnett_tick_t start, end;

  hashset_create(params[HASHSET_SIZE], params[ON_PTHREAD]);

  BARRIER();

  start = gasnett_ticks_now();
  for (i = 0; i < MY_NUM_OPS; ++i) {
    if (put_flags[i] == GRT_TRUE) {
      hashset_insert(numbers[i]);
    } else {
      hashset_find(numbers[i]);
    }
  }
  end = gasnett_ticks_now();
  time = ((unsigned) gasnett_ticks_to_us(end - start));
  //printf("processor %u: execution time=%f us\n", 
  //	 grt_id, (double) time);
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
    time_per_op = ((float) time) / params[NUM_OPS];
    printf("total CPU time=%f us\n", (double) time);
    printf("time per operation=%f us\n", time_per_op);
    printf("max time=%f us\n", (double) max_time);
  }

  BARRIER();

  hashset_destroy();

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

static int get_params() {
  FILE *params_file = fopen("params", "r");
  param_id_t param_id;
  if (!params_file) {
    return 0;
  }
  while (fgets(buf, BUF_SIZE, params_file)) {
    param_id_t my_param_id;
    buf[strlen(buf)-1] = 0;
    if (get_param(buf, &my_param_id) == SUCCEED)
      param_id = my_param_id;
    else if (!strncmp(buf, "yes", 3))
      params[param_id] = GRT_TRUE;
    else if (!strncmp(buf, "no", 3))
      params[param_id] = GRT_FALSE;
    else if (buf[0] != '%' && buf[0] != 0) {
      if (param_id < NUM_PARAMS)
	params[param_id] = atoi(buf);
    }
  }
  fclose(params_file);
  return 1;
}


int main(int argc, char** argv) {

  unsigned i;
  unsigned num_put = 0;
  struct timeval tv;
  double sum = 0;
  double min_time_per_op = -1;
  double min_of_max = -1;

  gettimeofday(&tv, 0);
  srandom(tv.tv_usec);
  grt_random_init();

  int got_params = get_params();
  hashset_init(argc, argv, params[SHARE_POWER]);
  if (!got_params) {
    if (grt_id == 0) {
      fprintf(stderr, "*** USING DEFAULT PARAMETERS ***\n");
    }
    grt_barrier();
  }

  numbers = malloc(MY_NUM_OPS * sizeof(grt_word_t));
  put_flags = malloc(MY_NUM_OPS * sizeof(grt_bool_t));
  int nth = 2*grt_distrib_start(params[NUM_OPS])+1;
  grt_random_nth(grt_distrib_start(nth));
  for (i = 0; i < MY_NUM_OPS; ++i) {
    unsigned flag_val;
    numbers[i] = RAND_MAX * grt_random_next();
    flag_val = ceil(100 * grt_random_next());
    put_flags[i] = (flag_val <= params[PERCENT_PUT]) ? 
      GRT_TRUE : GRT_FALSE;
    if (put_flags[i] == GRT_TRUE) ++num_put;
  }

  grt_barrier();

  if (grt_id == 0) {
    printf("User-specified parameters:\n");
    for (i = 0; i < NUM_PARAMS; ++i) {
      const char *name = param_names[i];
      unsigned param = params[i];
      printf("  %s=", name);
      if (i == ON_PTHREAD)
	printf((param == GRT_TRUE) ? "yes" : "no");
      else
	printf("%u", param);
      printf("\n");
    }
    printf("  grt_num_procs=%u\n", grt_num_procs);
  }

  grt_barrier();

  //grt_debug_print("%u/%u puts\n", num_put, MY_NUM_OPS);
  //grt_barrier();

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

  hashset_exit(0);
}
