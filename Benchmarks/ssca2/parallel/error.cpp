#include <stdio.h>
#include "error.h"
extern "C" {
#include "grt.h"
}

const char *prog_name;

void error(const char* fmt, ...) {
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "processor %u: ", grt_id);
  fprintf(stderr, "%s: ", prog_name);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}
