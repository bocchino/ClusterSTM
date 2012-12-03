GASNet Runtime (GRT)
====================
Rob Bocchino
October 2006
Revised December 2012

This is a simple runtime built on top of GASNet that provides the
following features:

 * An integer type grt_word_t that is either 32 or 64 bits, depending
   on the host machine, and macros for packing/unpacking a 64-bit
   grt_word_t into 32-bit host words.  This is important because
   GASNet supports only 32-bit words.

 * Memory allocation of global shared memory with a UPC-like
   interface.  You can give a handle to allocated memory to one
   processor (alloc) or to all processors (all_alloc).  You can also
   free allocated memory.  The internal, per-processor implementation
   is built on top of GCC umalloc.

 * Atomic compare and swap.

 * Locks, implemented as a distributed queued lock (qlock) that spins
   on a processor-local variable.

 * A simple list and map.


