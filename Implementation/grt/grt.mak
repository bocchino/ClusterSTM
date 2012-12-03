#
# Include file for makefiles that use GASNet runtime
#

include $(GASNET)/include/$(GASNET_CONDUIT)/$(GASNET_MAKE_INCL)

CC =		$(ATOMIC_CC) #$(GASNET_CFLAGS)
CPP =		$(ATOMIC_CXX) #$(GASNET_CFLAGS)
LINK =		$(ATOMIC_CXX) $(GASNET_LDFLAGS)

INCLUDES = -I$(GASNET)/include -I$(GASNET)/include/$(GASNET_CONDUIT) -I$(GRT) -I$(LIBMBA)/include
CFLAGS =  $(GASNET_DEFS) $(GRT_TEST_DEF) $(INCLUDES)
GRT_LIB =	$(GRT)/lib_grt.a $(GRT)/umalloc/lib_umalloc.a -L$(LIBMBA)/lib -lmba
GRT_LIB_DEBUG = $(GRT)/lib_grt_debug.a $(GRT)/umalloc/lib_umalloc.a -L$(LIBMBA)/lib -lmba

all: $(DEPENDS)
	$(MAKE) alldepend
     
alldepend: $(TARGETS)

clean:
	rm -f *.o *.d *.p $(TARGETS) *~
	$(CLEAN)

%.p : %.c
	$(CC) $(CFLAGS) -E $< -o $@

%.d: %.c
	gcc -MM -MG $(CFLAGS) $< -o $@

%.d: %.cpp
	g++ -MM -MG $(CFLAGS) $< -o $@

%.o: %.cpp
	$(CPP) $(CFLAGS) -c $< -o $@

$(GRT_LIB) ::
	$(MAKE) -C $(GRT)

$(GRT_LIB_DEBUG) ::
	$(MAKE) -C $(GRT)

include $(DEPENDS)

