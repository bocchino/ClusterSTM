#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <algorithm>

/* 1D array */

typedef struct array_t {
  size_t length;
  size_t elt_size;
  void *data;
} array_t;

#define array(__type)		      \
  typedef struct array_##__type##_t { \
      size_t length;		      \
      __type * data;		      \
  } array_##__type##_t

array(int);
array(unsigned);
array(array_int_t);
array(array_unsigned_t);

/* sparse 2D array */

typedef struct sparse_2D_array_t {
  /* Row layout info */
  unsigned num_rows;
  unsigned *row_offsets;
  
  /* Actual data, stored as flat array of elements */
  unsigned num_elts;
  unsigned *data;
} sparse_2D_array_t;

/* 2D array of strings */

typedef struct string_array_t {
  unsigned num_rows;
  unsigned row_length;
  char *data;
} string_array_t;

#endif
