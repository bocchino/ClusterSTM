TARGETS=	stm-distrib.o stm-distrib-debug.o stm-distrib-rv.o stm-distrib-rv-debug.o stm-distrib-wb.o stm-distrib-wb-debug.o stm-distrib-la.o stm-distrib-la-debug.o

include $(ATOMIC_MAK)
include $(STM)/stm.mak

stm-distrib-debug.o : stm-distrib.c
	$(CC) $(CFLAGS) -DSTM_DEBUG -c $< -o $@

stm-distrib-rv.o : stm-distrib.c
	$(CC) $(CFLAGS) -DREAD_VERSIONING -c $< -o $@

stm-distrib-rv-debug.o : stm-distrib.c
	$(CC) $(CFLAGS) -DREAD_VERSIONING -DSTM_DEBUG -c $< -o $@

stm-distrib-wb.o : stm-distrib.c
	$(CC) $(CFLAGS) -DWRITE_BUFFERING -c $< -o $@

stm-distrib-wb-debug.o : stm-distrib.c
	$(CC) $(CFLAGS) -DWRITE_BUFFERING -DSTM_DEBUG -c $< -o $@

stm-distrib-la.o : stm-distrib.c
	$(CC) $(CFLAGS) -DLATE_ACQUIRE -DWRITE_BUFFERING -c $< -o $@

stm-distrib-la-debug.o : stm-distrib.c
	$(CC) $(CFLAGS) -DLATE_ACQUIRE -DWRITE_BUFFERING -DSTM_DEBUG -c $< -o $@



