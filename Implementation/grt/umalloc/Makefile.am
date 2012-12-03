
# name(s) of final output
lib_LIBRARIES   = libumalloc.a

# source files (.c and .h, also .l, .y)
libumalloc_a_SOURCES= 	\
	attach.c	\
	keys.c		\
	ucalloc.c	\
	ufree.c		\
	umalloc.c	\
	umalloc.h	\
	umalloc_2_upcr.h	\
	umcheck.c	\
	umemalign.c	\
	umprivate.h	\
	umstats.c	\
	umtrace.c	\
	urealloc.c	\
	uvalloc.c

docs = COPYING.LIB ChangeLog TODO umalloc.texi

EXTRA_DIST = $(docs) umtrace.awk

INCLUDES        =  -I$(top_srcdir) -I$(top_srcdir)/gasnet/other

if BUILDCONFIG_DEBUG
  DEBUG_FLAGS = -DDEBUG -DUPCR_DEBUG
else
  DEBUG_FLAGS = -DNDEBUG -DUPCR_NDEBUG
endif

CFLAGS_STAMP=$(top_builddir)/umalloc/.set-cflags

$(CFLAGS_STAMP): $(srcdir)/Makefile.in $(top_builddir)/gasnet/Makefile
	cd $(top_builddir) && CONFIG_HEADERS= CONFIG_FILES=umalloc/Makefile ./config.status
	eval `$(MAKE) --no-print-directory -f $(top_builddir)/gasnet/Makefile echovar VARNAME=CFLAGS` ; \
        mv Makefile Makefile.bak &&                                                                     \
	@PERL@ -p -e 's|^\s*CFLAGS\s*=(.*)$$|CFLAGS='"$$CFLAGS"' $(DEBUG_FLAGS)|' Makefile.bak >Makefile &&  \
 	rm -f Makefile.bak
	touch $(CFLAGS_STAMP)

Makefile: $(CFLAGS_STAMP)
	touch Makefile

CLEANFILES=$(CFLAGS_STAMP)

