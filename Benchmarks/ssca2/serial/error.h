#ifndef ERROR_H
#define ERROR_H

#include <stdlib.h>
#include <stdarg.h>

extern const char* prog_name;

/* Print an error message and exit */

void error(const char*, ...);

#endif
