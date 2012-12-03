#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#include "stm-distrib.h"
#include "../lib/hashset.h"
#include "gasnet_tools.h"
#include "math.h"
#include "grt_random.h"

#define ZERO_ONLY

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

#define MY_NUM_OPS grt_distrib_range(params[NUM_OPS])

grt_word_t *numbers;
grt_bool_t *put_flags;
double time_per_op;
double max_time;

grt_word_t *local_insert_times;
grt_word_t *local_insert_counts;
grt_word_t *local_find_times;
grt_word_t *local_find_counts;
grt_word_t *remote_insert_times;
grt_word_t *remote_insert_counts;
grt_word_t *remote_find_times;
grt_word_t *remote_find_counts;

void run() {

  unsigned i;
  unsigned local_insert_time, local_find_time;
  grt_word_t local_insert_count, local_find_count;
  unsigned remote_insert_time, remote_find_time;
  grt_word_t remote_insert_count, remote_find_count;
  gasnett_tick_t start, end;
  unsigned time;

  hashset_create(params[HASHSET_SIZE], params[ON_PTHREAD]);

  BARRIER();

  local_insert_time = 0, local_insert_count = 0;
  local_find_time = 0, local_find_count = 0;
  remote_insert_time = 0, remote_insert_count = 0;
  remote_find_time = 0, remote_find_count = 0;
#ifdef ZERO_ONLY
  if (grt_id == 0) {
#endif
  for (i = 0; i < MY_NUM_OPS; ++i) {
    if (put_flags[i] == GRT_TRUE) {
      start = gasnett_ticks_now();
      hashset_insert(numbers[i]);
      end = gasnett_ticks_now();
      time = ((unsigned) gasnett_ticks_to_us(end - start));
      if (compute_hash(numbers[i]).proc == grt_id) {
	++local_insert_count;
	local_insert_time += time;
      } else {
	++remote_insert_count;
	remote_insert_time += time;
      }
    } else {
      start = gasnett_ticks_now();
      hashset_find(numbers[i]);
      end = gasnett_ticks_now();
      time = ((unsigned) gasnett_ticks_to_us(end - start));
      if (compute_hash(numbers[i]).proc == grt_id) {
	++local_find_count;
	local_find_time += time;
      } else {
	++remote_find_count;
	remote_find_time += time;
      }
    }
  }
  assert(local_insert_count + remote_insert_count + 
	 local_find_count + remote_find_count == MY_NUM_OPS);
#ifdef ZERO_ONLY
  }
#endif
  grt_write(0, local_insert_time, &local_insert_times[grt_id]);
  grt_write(0, local_insert_count, &local_insert_counts[grt_id]);
  grt_write(0, remote_insert_time, &remote_insert_times[grt_id]);
  grt_write(0, remote_insert_count, &remote_insert_counts[grt_id]);
  grt_write(0, local_find_time, &local_find_times[grt_id]);
  grt_write(0, local_find_count, &local_find_counts[grt_id]);
  grt_write(0, remote_find_time, &remote_find_times[grt_id]);
  grt_write(0, remote_find_count, &remote_find_counts[grt_id]);
  BARRIER();

  if (grt_id == 0) {
    local_insert_time = 0, local_find_time = 0;
    local_insert_count = 0, local_find_count = 0;
    remote_insert_time = 0, remote_find_time = 0;
    remote_insert_count = 0, remote_find_count = 0;
    for (i = 0; i < grt_num_procs; ++i) {
      local_insert_time += local_insert_times[i];
      local_find_time += local_find_times[i];
      remote_insert_time += remote_insert_times[i];
      remote_find_time += remote_find_times[i];
      local_insert_count += local_insert_counts[i];
      local_find_count += local_find_counts[i];
      remote_insert_count += remote_insert_counts[i];
      remote_find_count += remote_find_counts[i];
    }
    printf("local insert: total %u us, %u ops, avg %f us\n", 
	   local_insert_time, local_insert_count, 
	   ((float) local_insert_time) / local_insert_count);
    printf("local find: total %u us, %u ops, avg %f us\n", 
	   local_find_time, local_find_count, 
	   ((float) local_find_time) / local_find_count);
    printf("remote insert: total %u us, %u ops, avg %f us\n", 
	   remote_insert_time, remote_insert_count, 
	   ((float) remote_insert_time) / remote_insert_count);
    printf("remote find: total %u us, %u ops, avg %f us\n", 
	   remote_find_time, remote_find_count, 
	   ((float) remote_find_time) / remote_find_count);

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

static void get_params() {
  FILE *params_file = fopen("params", "r");
  param_id_t param_id;
  if (!params_file) {
    if (grt_id == 0) {
      fprintf(stderr, "*** USING DEFAULT PARAMETERS ***\n");
    }
    grt_barrier();
    return;
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
      if (param_id >= 0 && param_id < NUM_PARAMS)
	params[param_id] = atoi(buf);
    }
  }
  fclose(params_file);
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

  hashset_init(argc, argv, params[SHARE_POWER]);

  get_params();

  numbers = malloc(MY_NUM_OPS * sizeof(grt_word_t));
  put_flags = malloc(MY_NUM_OPS * sizeof(grt_bool_t));
  int nth = 2*grt_distrib_start(params[NUM_OPS])+1;
  //grt_debug_print("getting random nth: %u\n", nth);
  grt_random_nth(grt_distrib_start(nth))
;
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
#ifdef ZERO_ONLY
    printf("Only processor 0 is performing inserts and finds\n");
#endif
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

  local_insert_times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  local_find_times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  remote_insert_times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  remote_find_times = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  local_insert_counts = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  local_find_counts = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  remote_insert_counts = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));
  remote_find_counts = 
    (grt_word_t*) grt_all_alloc(0, grt_num_procs * sizeof(grt_word_t));


  for (i = 0; i < params[NUM_RUNS]; ++i) {
    if (grt_id == 0) {
      printf("\nRUN NUMBER %u\n\n", i);
      fflush(stdout);
    }
    BARRIER();
    run();
  }
  
  BARRIER();

  hashset_exit(0);
}
