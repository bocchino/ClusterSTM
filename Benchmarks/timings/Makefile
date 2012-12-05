TARGETS=        timings handlers
NUM_PROCS= 2
CLEAN += rm -f timings.out

include $(ATOMIC_MAK)
include $(GRT)/grt.mak

timings: timings.o
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

handlers: handlers.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

timings.out : timings
	./$< 2 > $@

