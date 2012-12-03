CLEAN = rm -f timing.mak
ifndef NUM_PROCS
	NUM_PROCS = 512
endif
ifndef TIME
	TIME = 0:10
endif
SPAN= -R "span[ptile=1]"

include timing.mak
TARGETS += hashset-nolocks

timing.mak :: makegen.pl
	perl $< > $@

hashset-nolocks: timing.o ${LIBDIR}/hashset-nolocks.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

${LIBDIR}/hashset-nolocks.o ::
	make -C ${LIBDIR} hashset-locks.o

validate-locks: validate.o ${LIBDIR}/hashset-locks.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

timing-rv.o : timing.c
	$(CC) $(CFLAGS) -DREAD_VERSIONING -c $< -o $@

timing-wb.o : timing.c
	$(CC) $(CFLAGS) -DWRITE_BUFFERING -c $< -o $@

timing-la.o : timing.c
	$(CC) $(CFLAGS) -DWRITE_BUFFERING -DLATE_ACQUIRE -c $< -o $@

plots: runtime-stms-speedup.dat runtime-sizes-speedup.dat
	gnuplot < runtime-stms-speedup.gpt
	gnuplot < runtime-sizes-speedup.gpt
	gnuplot < runtime-stms.gpt

buildall:
	make hashset-locks
	make hashset-stm-distrib
	make hashset-stm-distrib-rv
	make hashset-stm-distrib-wb
	make hashset-stm-distrib-la


