include $(ATOMIC_MAK)
TARGETS= $(C_SRCS:.c=.o) hashset-stm-distrib-rv.o hashset-stm-distrib-wb.o hashset-stm-distrib-la.o hashset-nolocks.o

include $(STM)/stm.mak

hashset-nolocks.o : hashset-locks.c
	$(CC) $(CFLAGS) -c -DNOLOCKS $< -o $@

hashset-stm-distrib-rv.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DREAD_VERSIONING $< -o $@

hashset-stm-distrib-rv-debug.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DREAD_VERSIONING -DSTM_DEBUG $< -o $@

hashset-stm-distrib-wb.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DWRITE_BUFFERING $< -o $@

hashset-stm-distrib-wb-debug.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DWRITE_BUFFERING -DSTM_DEBUG $< -o $@

hashset-stm-distrib-la.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DLATE_ACQUIRE -DWRITE_BUFFERING $< -o $@

hashset-stm-distrib-la-debug.o : hashset-stm-distrib.c
	$(CC) $(CFLAGS) -c -DLATE_ACQUIRE -DWRITE_BUFFERING -DSTM_DEBUG $< -o $@
