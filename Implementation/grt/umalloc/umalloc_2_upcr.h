/*
 * Provides conversion from umalloc to upc runtime allocator interface.
 *
 * See notes on "Memory Management in the UPC Runtime" for details.
 *
 * Jason Duell <jcduell@lbl.gov>
 */

#ifndef UMALLOC_2_UPCR
#define UMALLOC_2_UPCR

#include "umalloc.h"

/* umalloc now supports global head growing downwards */
#undef UPCR_GLOBALHEAP_GROWS_UP

/* 
 * umalloc works as both local and global shared heap: just need to define
 * both sets of functions/types to same set of functions/types in unmalloc.
 */

extern void upcri_umalloc_corruption_callback(void);
extern uintptr_t upcri_umalloc_alignthreshold;
extern uintptr_t upcri_umalloc_alignsize;

/************************************************************************ 
 * Shared global heap allocator 
 ************************************************************************/

typedef umalloc_heap_t upcra_sharedglobal_heap_t;

UPCRI_INLINE(upcra_sharedglobal_init) umalloc_heap_t *
upcra_sharedglobal_init(void *heapstart, uintptr_t initbytes)
{
    umalloc_heap_t *heap;
    heap = umalloc_makeheap(heapstart, initbytes, 
	    #ifdef UPCR_GLOBALHEAP_GROWS_UP
		UMALLOC_HEAP_GROWS_UP
	    #else
		UMALLOC_HEAP_GROWS_DOWN
	    #endif
	   );
    #ifdef UPCR_DEBUG
      umcheck(heap, upcri_umalloc_corruption_callback);
    #endif
    return heap;
}

UPCRI_INLINE(upcra_sharedglobal_alloc) void *
upcra_sharedglobal_alloc(umalloc_heap_t *heap, uintptr_t size)
{
    upcri_assert(upcri_umalloc_alignthreshold && upcri_umalloc_alignsize);
    if (size >= upcri_umalloc_alignthreshold)
      return umemalign(heap, upcri_umalloc_alignsize, size);
    else
      return umalloc(heap, size);
}

UPCRI_INLINE(upcra_sharedglobal_free) void
upcra_sharedglobal_free(umalloc_heap_t *heap, void *addr)
{
    ufree(heap, addr);
}

UPCRI_INLINE(upcra_sharedglobal_provide_pages) void
upcra_sharedglobal_provide_pages(umalloc_heap_t *heap, uintptr_t numbytes)
{
    umalloc_provide_pages(heap, numbytes);
}

/* returns a lower-bound approximation on the number of free bytes in the heap */
UPCRI_INLINE(upcra_sharedglobal_query_freebytes) uintptr_t
upcra_sharedglobal_query_freebytes(umalloc_heap_t *heap)
{
   uintptr_t freebytes = umstats_bytes_free(heap) + umstats_extra_corespace(heap);
   return freebytes;
}

/************************************************************************ 
 * Shared local heap allocator 
 ************************************************************************/

typedef umalloc_heap_t upcra_sharedlocal_heap_t;

UPCRI_INLINE(upcra_sharedlocal_init) umalloc_heap_t *
upcra_sharedlocal_init(void *heapstart, uintptr_t initbytes)
{
    umalloc_heap_t *heap;
    heap = umalloc_makeheap(heapstart, initbytes, UMALLOC_HEAP_GROWS_UP);
    #ifdef UPCR_DEBUG
      umcheck(heap, upcri_umalloc_corruption_callback);
    #endif
    return heap;
}

UPCRI_INLINE(upcra_sharedlocal_alloc) void *
upcra_sharedlocal_alloc(umalloc_heap_t *heap, uintptr_t size)
{
    upcri_assert(upcri_umalloc_alignthreshold && upcri_umalloc_alignsize);
    if (size >= upcri_umalloc_alignthreshold)
      return umemalign(heap, upcri_umalloc_alignsize, size);
    else
      return umalloc(heap, size);
}

UPCRI_INLINE(upcra_sharedlocal_free) void
upcra_sharedlocal_free(umalloc_heap_t *heap, void *addr)
{
    ufree(heap, addr);
}

UPCRI_INLINE(upcra_sharedlocal_provide_pages) void
upcra_sharedlocal_provide_pages(umalloc_heap_t *heap, uintptr_t numbytes)
{
    umalloc_provide_pages(heap, numbytes);
}

/* returns a lower-bound approximation on the number of free bytes in the heap */
UPCRI_INLINE(upcra_sharedlocal_query_freebytes) uintptr_t
upcra_sharedlocal_query_freebytes(umalloc_heap_t *heap)
{
   uintptr_t freebytes = umstats_bytes_free(heap) + umstats_extra_corespace(heap);
   return freebytes;
}

#endif /* UMALLOC_2_UPCR */

