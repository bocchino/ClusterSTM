TARGETS=	lib_grt.a lib_grt_debug.a umalloc/lib_umalloc.a
O_TARGETS=	$(SRCS:.c=.o)
O_TARGETS+=	$(CPP_SRCS:.cpp=.o)
DEBUG_TARGETS=  $(O_TARGETS:.o=.debug.o)
CLEAN=		$(MAKE) -C umalloc clean

include $(ATOMIC_MAK)
include $(GRT)/grt.mak

%.debug.o : %.c
	$(CC) $(CFLAGS) -DGRT_DEBUG -c $< -o $@

lib_grt.a : $(O_TARGETS)
	ar -rc $@ $+
	ranlib $@

lib_grt_debug.a : $(DEBUG_TARGETS)
	ar -rc $@ $+
	ranlib $@

umalloc/lib_umalloc.a ::
	$(MAKE) -C umalloc lib_umalloc.a

