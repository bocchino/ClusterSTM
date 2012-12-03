CLEAN = rm -f timing.mak

include timing.mak

timing.mak :: makegen.pl
	perl $< > $@

timing-nolocks.o: timing.c
	$(CC) $(CFLAGS)  -DNOLOCKS -c $< -o $@

hashmap-nolocks: timing-nolocks.o ../lib/hashmap-locks.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LD_FLAGS) $(GASNET_LIBS)

hashmap-locks-debug: timing.o ../lib/hashmap-locks.o $(GRT_LIB_DEBUG)
	$(LINK) $+ -o $@ $(LD_FLAGS) $(GASNET_LIBS)

buildall:
	make hashmap-locks
	make hashmap-stm-distrib
	make hashmap-stm-distrib-rv
	make hashmap-stm-distrib-wb
	make hashmap-stm-distrib-la

