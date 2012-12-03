#include "grt_random.h"
#include <math.h>

static const int64_t seed = 314159265;
static const int64_t arand = 1220703125.0;
static double r23, t23, r46, t46;
static int32_t internalSeed;
static double cursorVal;

static void initCursorVal() {
  cursorVal = internalSeed;
}

static double randlc_int(int64_t *x, int64_t a) {
  double t1 = r23 * a;
  const double a1 = floor(t1),
    a2 = a - t23 * a1;
  t1 = r23 * *x;
  const double x1 = floor(t1),
    x2 = *x - t23 * x1;
  t1 = a1 * x2 + a2 * x1;
  const double t2 = floor(r23 * t1),
    z  = t1 - t23 * t2,
    t3 = t23 * z + a2 * x2,
    t4 = floor(r46 * t3),
    x3 = t3 - t46 * t4;
  *x = x3;
  
  return r46 * x3;
}

static double randlc_double(double *x, int64_t a) {
  double t1 = r23 * a;
  const double a1 = floor(t1),
    a2 = a - t23 * a1;
  t1 = r23 * *x;
  const double x1 = floor(t1),
    x2 = *x - t23 * x1;
  t1 = a1 * x2 + a2 * x1;
  const double t2 = floor(r23 * t1),
    z  = t1 - t23 * t2,
    t3 = t23 * z + a2 * x2,
    t4 = floor(r46 * t3),
    x3 = t3 - t46 * t4;
  *x = x3;
  
  return r46 * x3;
}

void grt_random_init() {
  r23 = pow(0.5,23),
  t23   = pow(2.0, 23),
  r46   = pow(0.5, 46),
  t46   = pow(2.0, 46);
  internalSeed = seed;
  // ensure seed is odd
  if (internalSeed % 2 == 0) internalSeed += 1;
  initCursorVal();
  grt_random_next();
}

double grt_random_next() {
  return randlc_double(&cursorVal, arand);
}

double grt_random_nth(int64_t n) {
  int64_t t = arand;
  initCursorVal();
  int64_t retval = arand;
  while (n != 0) {
    int64_t i = n / 2;
    if (2 * i != n) 
      randlc_double(&cursorVal, t);
    retval = randlc_int(&t, t);
    n = i;
  }
  return grt_random_next();
}

