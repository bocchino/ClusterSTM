TARGETS=        ssca2 ssca2-nb ssca2-seq ssca2-seq-nb ssca2-stm-rv ssca2-stm-wb ssca2-stm-la ssca2-stm ssca2-stm-seq

#include local.mak
include $(ATOMIC_MAK)
include $(STM)/stm.mak

# Linked executables

ssca2 : ssca2.o cut_clusters.o graph.o error.o debug.o input.o ssca2_handler.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-nb : ssca2-nb.o cut_clusters.o graph.o error.o debug.o input.o ssca2_handler.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-seq : ssca2-seq.o cut_clusters.o graph.o error.o debug.o input.o ssca2_handler.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-seq-nb : ssca2-seq-nb.o cut_clusters.o graph.o error.o debug.o input.o ssca2_handler.o $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm : ssca2-stm.o cut_clusters-stm.o graph.o error.o debug.o input.o ssca2_handler-stm.o  $(STM-DISTRIB-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-rv : ssca2-stm-rv.o cut_clusters-stm-rv.o graph.o error.o debug.o input.o ssca2_handler-stm-rv.o  $(STM-DISTRIB-RV-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-wb : ssca2-stm-wb.o cut_clusters-stm-wb.o graph.o error.o debug.o input.o ssca2_handler-stm-wb.o  $(STM-DISTRIB-WB-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-la : ssca2-stm-la.o cut_clusters-stm-la.o graph.o error.o debug.o input.o ssca2_handler-stm-la.o  $(STM-DISTRIB-LA-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-seq : ssca2-stm-seq.o cut_clusters-stm.o graph.o error.o debug.o input.o ssca2_handler-stm.o  $(STM-DISTRIB-LIB) $(GRT_LIB)
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-debug: ssca2-stm.o cut_clusters-stm.o graph.o error.o debug.o input.o ssca2_handler-stm.o $(STM-DISTRIB-DEBUG-LIB) $(GRT_LIB)
	g++ $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

ssca2-stm-wb-debug: ssca2-stm-wb.o cut_clusters-stm-wb.o graph.o error.o debug.o input.o ssca2_handler-stm-wb.o $(STM-DISTRIB-WB-DEBUG-LIB) $(GRT_LIB)
	g++ $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

#.o files

ssca2-nb.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DNB_ON -o $@

ssca2-seq.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSEQUENTIAL -o $@

ssca2-seq-nb.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DNB_ON -DSEQUENTIAL -o $@

ssca2-stm.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -o $@

ssca2-stm-rv.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DREAD_VERSIONING -o $@

ssca2-stm-wb.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -o $@

ssca2-stm-la.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -DLATE_ACQUIRE -o $@

ssca2-stm-seq.o : ssca2.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DSEQUENTIAL -o $@

ssca2_handler-stm.o : ssca2_handler.c
	$(CC) $(CFLAGS) $< -c -DSTM -o $@

ssca2_handler-stm-rv.o : ssca2_handler.c
	$(CC) $(CFLAGS) $< -c -DSTM -DREAD_VERSIONING -o $@

ssca2_handler-stm-wb.o : ssca2_handler.c
	$(CC) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -o $@

ssca2_handler-stm-la.o : ssca2_handler.c
	$(CC) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -DLATE_ACQUIRE -o $@

cut_clusters-stm.o : cut_clusters.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -o $@

cut_clusters-stm-rv.o : cut_clusters.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DREAD_VERSIONING -o $@

cut_clusters-stm-wb.o : cut_clusters.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -o $@

cut_clusters-stm-la.o : cut_clusters.cpp
	$(CPP) $(CFLAGS) $< -c -DSTM -DWRITE_BUFFERING -DLATE_ACQUIRE -o $@

# Runs

run: 
	./ssca2 $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-nb: ssca2-nb
	./ssca2-nb $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-seq : ssca2-seq
	./ssca2-seq $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-seq-nb : ssca2-seq-nb
	./ssca2-seq-nb $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm: 
	./ssca2-stm $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-rv: 
	./ssca2-stm-rv $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-wb: 
	./ssca2-stm-wb $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-la: 
	./ssca2-stm-la $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-seq: 
	./ssca2-stm-seq $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-debug: ssca2-stm-debug
	./ssca2-stm-debug $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

run-stm-wb-debug: ssca2-stm-wb-debug
	./ssca2-stm-wb-debug $(NUM_PROCS) $(GRAPH).$(GRAPH_SIZE)

