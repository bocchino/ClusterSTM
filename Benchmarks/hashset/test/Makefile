CLEAN= rm -f test.mak
NUM_PROCS = 3

include test.mak

test.mak : makegen.pl
	perl $< > $@

test : $(TARGETS)
	@(for target in $(TARGETS); do ./$$target $(NUM_PROCS); done)

test-base : $(TARGETS-BASE)
	@(for target in $(TARGETS-BASE); do ./$$target $(NUM_PROCS); done)

test-rv : $(TARGETS-RV)
	@(for target in $(TARGETS-RV); do ./$$target $(NUM_PROCS); done)

test-wb : $(TARGETS-WB)
	@(for target in $(TARGETS-WB); do ./$$target $(NUM_PROCS); done)

test-la : $(TARGETS-LA)
	@(for target in $(TARGETS-LA); do ./$$target $(NUM_PROCS); done)
