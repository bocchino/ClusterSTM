TARGETS=        driver_locks.c driver_stm.c hashtable_locks hashtable_stm_base

include $(ATOMIC_MAK)
include $(STM)/stm.mak

driver_locks.c : driver_locks.h driver.c
	cat $+ > $@

driver_stm.c : driver_stm.h driver.c
	cat $+ > $@

../lib/hashtable_locks.o ::
	make -C ../lib hashtable_locks.o

../lib/hashtable_stm_base.o ::
	make -C ../lib hashtable_stm_base.o

hashtable_locks : driver_locks.o ../lib/hashtable_locks.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

hashtable_stm_base : driver_stm.o ../lib/hashtable_stm_base.o $(STM_BASE_LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
