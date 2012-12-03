#include <stdio.h>
#include "error.h"

const char *prog_name;

void error(const char* fmt, ...) {
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "%s: ", prog_name);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}
