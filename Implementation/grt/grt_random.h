#ifndef GRT_RANDOM_H
#define GRT_RANDOM_H

#include <sys/types.h>

void grt_random_init();
double grt_random_next();
double grt_random_nth(int64_t n);

#endif
