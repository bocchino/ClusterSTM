include $(ATOMIC_MAK)

O_TARGETS=	$(SRCS:.c=.o)
TARGETS= 	lib_umalloc.a

include $(GRT)/grt.mak

lib_umalloc.a : $(O_TARGETS)
	ar -rc $@ $+
	ranlib $@

