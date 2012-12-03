#include "debug.h"

unsigned indent = 0;

void print_spaces(unsigned n) {
  unsigned i;
  for (i = 0; i < n; ++i)
    printf(" ");
}

void __print_unsigned(const char *name,
		      unsigned val) {
  print_spaces(indent);
  printf("%s: %d\n", name, val);
}

void __print_double(const char *name,
		  double val) {
  print_spaces(indent);
  printf("%s: %f\n", name, val);
}

void __print_array(const char *name,
		   array_unsigned_t arr) {
  unsigned i;
  print_spaces(indent);
  printf("%s: ", name);
  for (i = 0; i < arr.length; ++i)
    printf("%u ", arr.data[i]);
  printf("\n");
}

void __print_nonzero(const char *name,
		     int *arr, size_t len) {
  unsigned i;
  print_spaces(indent);
  printf("%s: ", name);
  for (i = 0; i < len; ++i)
    if (arr[i] != 0)
      printf("%u:%d ", i, arr[i]);
  printf("\n");
}

void __print_all(const char *name,
		 int *arr, size_t len) {
  unsigned i;
  print_spaces(indent);
  printf("%s: ", name);
  for (i = 0; i < len; ++i)
    printf("%d ", arr[i]);
  printf("\n");
}

