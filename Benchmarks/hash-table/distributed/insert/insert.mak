TARGETS= hashtable-stm-distrib hashtable_locks hashtable_locks_cache hashtable_locks_remote hashtable_stm_base hashtable_stm_base_remote 
LIBDIR= ../lib
include $(ATOMIC_MAK)
include $(STM)/stm.mak
${LIBDIR}/hashtable-stm-distrib.o ::
	make -C ${LIBDIR} hashtable-stm-distrib.o
${LIBDIR}/hashtable_locks.o ::
	make -C ${LIBDIR} hashtable_locks.o
${LIBDIR}/hashtable_locks_cache.o ::
	make -C ${LIBDIR} hashtable_locks_cache.o
${LIBDIR}/hashtable_locks_remote.o ::
	make -C ${LIBDIR} hashtable_locks_remote.o
${LIBDIR}/hashtable_stm_base.o ::
	make -C ${LIBDIR} hashtable_stm_base.o
${LIBDIR}/hashtable_stm_base_remote.o ::
	make -C ${LIBDIR} hashtable_stm_base_remote.o
hashtable-stm-distrib: driver.o ${LIBDIR}/hashtable-stm-distrib.o $(STM-DISTRIB-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
hashtable_locks: driver.o ${LIBDIR}/hashtable_locks.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
hashtable_locks_cache: driver.o ${LIBDIR}/hashtable_locks_cache.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
hashtable_locks_remote: driver.o ${LIBDIR}/hashtable_locks_remote.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
hashtable_stm_base: driver.o ${LIBDIR}/hashtable_stm_base.o $(STM_BASE_LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
hashtable_stm_base_remote: driver.o ${LIBDIR}/hashtable_stm_base_remote.o $(STM_BASE_LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)
