TARGETS=        ssca2 graph.o error.o cut_clusters.o debug.o input.o
GRAPHS_DIR= $(CLUSTER_STM)/Benchmarks/ssca2/graphs
GRAPH= $(GRAPHS_DIR)/graph.in
K4_OUT= $(GRAPHS_DIR)/k4.out
GRAPH_SIZE= 16-8-40
CLEAN += rm -f k4.out

include $(ATOMIC_MAK)
include $(STM)/stm.mak

ssca2 : ssca2.o cut_clusters.o graph.o error.o debug.o input.o
	$(LINK) $+ -o $@ $(LDFLAGS) $(GASNET_LIBS)

run: ssca2
	./ssca2 $(GRAPH).$(GRAPH_SIZE)

test: ssca2
	./ssca2 $(GRAPH).$(GRAPH_SIZE)
	diff k4.out $(K4_OUT).$(GRAPH_SIZE)

