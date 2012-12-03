include $(ATOMIC_MAK)
TARGETS= $(C_SRCS:.c=.o) hashmap-stm-distrib-rv.o hashmap-stm-distrib-wb.o hashmap-stm-distrib-la.o

include $(STM)/stm.mak

hashmap-locks.o : hashmap-locks.c
	$(CC) $(CFLAGS) -c -DLOCKS $< -o $@

hashmap-stm-distrib.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c $< -o $@

hashmap-stm-distrib-debug.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DSTM_DEBUG $< -o $@

hashmap-stm-distrib-rv.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DREAD_VERSIONING $< -o $@

hashmap-stm-distrib-rv-debug.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DREAD_VERSIONING -DSTM_DEBUG $< -o $@

hashmap-stm-distrib-wb.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DWRITE_BUFFERING $< -o $@

hashmap-stm-distrib-wb-debug.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DWRITE_BUFFERING -DSTM_DEBUG $< -o $@

hashmap-stm-distrib-la.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DLATE_ACQUIRE -DWRITE_BUFFERING $< -o $@

hashmap-stm-distrib-la-debug.o : hashmap-stm-distrib.c
	$(CC) $(CFLAGS) -c -DLATE_ACQUIRE -DWRITE_BUFFERING -DSTM_DEBUG $< -o $@
