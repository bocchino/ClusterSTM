#ifndef SPARSE_ARRAY_H
#define SPARSE_ARRAY_H

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <algorithm>

/* Sparse 1D array that can be efficiently traversed */

class sparse_array {
 public:
  unsigned* elements;
  bool *flags;
  size_t _size;
  unsigned *nz_idxs;
  size_t _nz_size;
  size_t nz_array_size;
  unsigned pos;
  void init(size_t sz) {
    _size = sz;
    elements = (unsigned*) malloc(_size * sizeof(unsigned));
    flags = (bool*) malloc(_size * sizeof(bool));
    nz_idxs = (unsigned*) malloc(_size * sizeof(unsigned));
    clear();
  }
  void destroy() {
    free(elements);
    free(flags);
    free(nz_idxs);
  }
  size_t nz_size() {
    return _nz_size;
  }
  void clear() {
    memset(elements, 0, _size * sizeof(unsigned));
    memset(flags, 0, _size * sizeof(bool));
    nz_array_size = 0;
    _nz_size = 0;
  }
  void put(unsigned idx, unsigned val) {
    if (val != 0 && !flags[idx]) {
      flags[idx] = true;
      nz_idxs[nz_array_size++] = idx;
    }
    if (val != 0 && elements[idx] == 0)
      ++_nz_size;
    if (val == 0 && elements[idx] != 0)
      --_nz_size;
    elements[idx] = val;
  }
  unsigned get(unsigned idx) {
    return elements[idx];
  }
  void start_nz() {
    std::sort(nz_idxs, nz_idxs+nz_array_size);
    pos = 0;
    while ((pos < nz_array_size) && (elements[nz_idxs[pos]] == 0))
      ++pos;
  }
  bool has_next_nz() {
    return pos < nz_array_size;
  }
  unsigned get_next_nz() {
    if (!has_next_nz()) {
      std::cerr << "sparse array: out of bounds!\n";
      abort();
    }
    unsigned idx = nz_idxs[pos];
    do {
      ++pos;
    }  while ((pos < nz_array_size) && (elements[nz_idxs[pos]] == 0));
    return idx;
  }
};

#endif
