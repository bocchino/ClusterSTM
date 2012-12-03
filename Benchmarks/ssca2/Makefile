SUBDIRS= serial parallel

all :
	@(for dir in $(SUBDIRS); do make -C $$dir; done)

clean :
	rm -f *~
	@(for dir in $(SUBDIRS); do make -C $$dir clean; done)

