
/* More debugging hooks for `umalloc'.
   Copyright 1991, 1992, 1994 Free Software Foundation

   Written April 2, 1991 by John Gilmore of Cygnus Support
   Based on mcheck.c by Mike Haertel.
   Modified Mar 1992 by Fred Fish.  (fnf@cygnus.com)

This file is part of the GNU C Library.

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
Boston, MA 02111-1307, USA.  */

#define _UMALLOC_INTERNAL
#include "umprivate.h"
#include <stdio.h>

static void tr_break(void);
static void tr_freehook(void *, void *);
static void *tr_mallochook(void *, uintptr_t);
static void *tr_reallochook(void *, void *, uintptr_t);

#ifndef	__GNU_LIBRARY__
extern char *getenv();
#endif

static FILE *mallstream;

#if 0				/* FIXME:  Disabled for now. */
static char mallenv[] = "MALLOC_TRACE";
static char mallbuf[BUFSIZ];	/* Buffer for the output.  */
#endif

/* Address to breakpoint on accesses to... */
static void *mallwatch;

/* Old hook values.  */

static void (*old_ufree_hook) (void *, void *);
static void *(*old_umalloc_hook) (void *, uintptr_t);
static void *(*old_urealloc_hook) (void *, void *, uintptr_t);

/* This function is called when the block being alloc'd, realloc'd, or
   freed has an address matching the variable "mallwatch".  In a debugger,
   set "mallwatch" to the address of interest, then put a breakpoint on
   tr_break.  */

static void tr_break(void)
{
}

static void tr_freehook(void *md, void *ptr)
{
    struct mdesc *mdp;

    mdp = MD_TO_MDP(md);
    /* Be sure to print it first.  */
    fprintf(mallstream, "- %08lx\n", (unsigned long) ptr);
    if (ptr == mallwatch)
	tr_break();
    mdp->ufree_hook = old_ufree_hook;
    ufree(md, ptr);
    mdp->ufree_hook = tr_freehook;
}

static void *tr_mallochook(void *md, uintptr_t size)
{
    void *hdr;
    struct mdesc *mdp;

    mdp = MD_TO_MDP(md);
    mdp->umalloc_hook = old_umalloc_hook;
    hdr = (void *) umalloc(md, size);
    mdp->umalloc_hook = tr_mallochook;

    /* We could be printing a NULL here; that's OK.  */
    fprintf(mallstream, "+ %08lx %x\n", (unsigned long) hdr, (int)size);

    if (hdr == mallwatch)
	tr_break();

    return (hdr);
}

static void *tr_reallochook(void *md, void *ptr, uintptr_t size)
{
    void *hdr;
    struct mdesc *mdp;

    mdp = MD_TO_MDP(md);

    if (ptr == mallwatch)
	tr_break();

    mdp->ufree_hook = old_ufree_hook;
    mdp->umalloc_hook = old_umalloc_hook;
    mdp->urealloc_hook = old_urealloc_hook;
    hdr = (void *) urealloc(md, ptr, size);
    mdp->ufree_hook = tr_freehook;
    mdp->umalloc_hook = tr_mallochook;
    mdp->urealloc_hook = tr_reallochook;
    if (hdr == NULL)
	/* Failed realloc.  */
	fprintf(mallstream, "! %08lx %x\n", (unsigned long) ptr, (int)size);
    else
	fprintf(mallstream, "< %08lx\n> %08lx %x\n", (unsigned long) ptr,
		(unsigned long) hdr, (int)size);

    if (hdr == mallwatch)
	tr_break();

    return hdr;
}

/* We enable tracing if either the environment variable MALLOC_TRACE
   is set, or if the variable mallwatch has been patched to an address
   that the debugging user wants us to stop on.  When patching mallwatch,
   don't forget to set a breakpoint on tr_break!  */

int umtrace(void)
{
#if 0				/* FIXME!  This is disabled for now until we figure out how to
				   maintain a stack of hooks per heap, since we might have other
				   hooks (such as set by umcheck/umcheckf) active also. */
    char *mallfile;

    mallfile = getenv(mallenv);
    if (mallfile != NULL || mallwatch != NULL) {
	mallstream = fopen(mallfile != NULL ? mallfile : "/dev/null", "w");
	if (mallstream != NULL) {
	    /* Be sure it doesn't umalloc its buffer!  */
	    setbuf(mallstream, mallbuf);
	    fprintf(mallstream, "= Start\n");
	    old_ufree_hook = mdp->ufree_hook;
	    mdp->ufree_hook = tr_freehook;
	    old_umalloc_hook = mdp->umalloc_hook;
	    mdp->umalloc_hook = tr_mallochook;
	    old_urealloc_hook = mdp->urealloc_hook;
	    mdp->urealloc_hook = tr_reallochook;
	}
    }
#endif				/* 0 */

    return (1);
}
