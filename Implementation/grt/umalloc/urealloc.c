/* Change the size of a block allocated by `umalloc'.
   Copyright 1990, 1991 Free Software Foundation
		  Written May 1989 by Mike Haertel.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#define _UMALLOC_INTERNAL
#include "umprivate.h"

/* Resize the given region to the new size, returning a pointer
   to the (possibly moved) region.  This is optimized for speed;
   some benchmarks seem to indicate that greater compactness is
   achieved by unconditionally allocating and copying to a
   new region.  This module has incestuous knowledge of the
   internals of both ufree and umalloc. */

void *
urealloc (umalloc_heap_t *md, void * ptr, uintptr_t size)
{
    struct mdesc *mdp;
    void *result;
    int type;
    uintptr_t block, blocks, oldlimit;

    if (size == 0) {
	ufree(md, ptr);
	return (umalloc(md, 0));
    } else if (ptr == NULL) {
	return (umalloc(md, size));
    }

    mdp = MD_TO_MDP(md);

    if (mdp->urealloc_hook != NULL) {
	return ((*mdp->urealloc_hook) (md, ptr, size));
    }

    block = BLOCK_MDP(mdp, ptr);

    type = mdp->heapinfo[block].busy.type;
    switch (type) 
    {
    case 0:
	/* Maybe reallocate a large block to a small fragment.  */
	if (size <= BLOCKSIZE / 2) {
	    result = umalloc(md, size);
	    if (result != NULL) {
		memcpy(result, ptr, size);
		ufree(md, ptr);
		return (result);
	    }
	}

	/* The new size is a large allocation as well;
	   see if we can hold it in place. */
	blocks = BLOCKIFY(size);
	if (blocks < mdp->heapinfo[block].busy.info.size) {
	    /* The new size is smaller; return excess memory to the free list. */
	    mdp->heapinfo[block + blocks].busy.type = 0;
	    mdp->heapinfo[block + blocks].busy.info.size
		= mdp->heapinfo[block].busy.info.size - blocks;
	    mdp->heapinfo[block].busy.info.size = blocks;
	    ufree(md, ADDRESS_MDP(mdp,block + blocks));
	    result = ptr;
	} else if (blocks == mdp->heapinfo[block].busy.info.size) {
	    /* No size change necessary.  */
	    result = ptr;
	} else {
	    /* Won't fit, so allocate a new region that will.
	       Free the old region first in case there is sufficient
	       adjacent free space to grow without moving. */
	    blocks = mdp->heapinfo[block].busy.info.size;
	    /* Prevent free from actually returning memory to the system.  */
	    oldlimit = mdp->heaplimit;
	    mdp->heaplimit = 0;
	    ufree(md, ptr);
	    mdp->heaplimit = oldlimit;
	    result = umalloc(md, size);
	    if (result == NULL) {
		umalloc(md, blocks * BLOCKSIZE);
		return (NULL);
	    }
	    if (ptr != result) {
		memmove(result, ptr, blocks * BLOCKSIZE);
	    }
	}
	break;

    default:
	/* Old size is a fragment; type is logarithm
	   to base two of the fragment size.  */
	if (size > (uintptr_t) (1 << (type - 1)) && size <= (uintptr_t) (1 << type)) {
	    /* The new size is the same kind of fragment.  */
	    result = ptr;
	} else {
	    /* The new size is different; allocate a new space,
	       and copy the lesser of the new size and the old. */
	    result = umalloc(md, size);
	    if (result == NULL) {
		return (NULL);
	    }
	    memcpy(result, ptr, MIN(size, (uintptr_t) 1 << type));
	    ufree(md, ptr);
	}
	break;
    }

    return (result);
}


