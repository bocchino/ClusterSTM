#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include "array.h"

extern unsigned indent;

void print_spaces(unsigned);

#define print_unsigned(x) __print_unsigned(#x,x);
void __print_unsigned(const char *name,
		      unsigned val);

#define print_double(x) __print_double(#x,x);
void __print_double(const char *name,
		  double val);

#define print_array(x) __print_array(#x,x)
void __print_array(const char *name,
		   array_unsigned_t arr);

#define print_nonzero(arr,len) __print_nonzero(#arr, arr, len)
void __print_nonzero(const char *name,
		     int *arr, size_t len);

#define print_all(arr,len) __print_all(#arr, (int*) arr, len)
void __print_all(const char *name,
		 int *arr, size_t len);

#endif
