#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#include "hashset.h"
#include "grt.h"
#include "grt_debug.h"

#define N 300
#define TABLE_SIZE 50

grt_word_t numbers[N];

int main(int argc, char** argv) {
  hashset_init(argc, argv, 4);

  struct timeval tv;
  gettimeofday(&tv, 0);
  srand(tv.tv_usec);
  unsigned i;

#ifdef PTHREAD
  hashset_create(TABLE_SIZE, GRT_TRUE);
#else
  hashset_create(TABLE_SIZE, GRT_FALSE);
#endif

  BARRIER();

  grt_debug_print("generating numbers\n");
  for (i = 0; i < N; ++i) {
    numbers[i] = rand();
  }
  grt_debug_print("inserting numbers\n");
  for (i = 0; i < N; ++i) {
    hashset_insert(numbers[i]);
  }

  BARRIER();

  grt_debug_print("finding numbers\n");
  for (i = 0; i < N; ++i) {
    grt_word_t num = numbers[i];
    unsigned found = hashset_find(num);
    if (!found) {
      fprintf(stderr, "processor %d: expected to find %llx\n", 
	      grt_id, num);
    } 
  }

  BARRIER();

  grt_debug_print("done\n");
  hashset_destroy();

  hashset_exit(0);
}
