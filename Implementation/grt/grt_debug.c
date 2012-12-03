#include "grt_debug.h"
#include "grt.h"
#include <stdarg.h>
#include <stdio.h>

gasnet_hsl_t print_lock = GASNET_HSL_INITIALIZER;

void grt_debug_print(const char* fmt, ...) {
  va_list args;

  va_start(args, fmt);
  gasnet_hsl_lock(&print_lock);
  printf("processor %u: ", grt_id);
  vprintf(fmt, args);
  fflush(stdout);
  gasnet_hsl_unlock(&print_lock);
  va_end(args);
}
