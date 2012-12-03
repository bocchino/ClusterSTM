SUBDIRS= hash-table hashmap-swap hashset ssca2

all :
	@(for dir in $(SUBDIRS); do make -C $$dir; done)

clean :
	rm -f *~
	@(for dir in $(SUBDIRS); do make -C $$dir clean; done)

