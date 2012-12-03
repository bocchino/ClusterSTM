#
# Variables used by atomic build environment
#
C_SRCS=		$(wildcard *.c)
C_DEPENDS=	$(C_SRCS:.c=.d)
CPP_SRCS=	$(wildcard *.cpp)
CPP_DEPENDS=	$(CPP_SRCS:.cpp=.d)
SRCS=		$(C_SRCS) $(CPP_SRCS)
DEPENDS=	$(C_DEPENDS) $(CPP_DEPENDS)
GRT =		$(ATOMIC_ROOT)/grt
STM =	   	$(ATOMIC_ROOT)/stm
BENCHMARKS =	$(ATOMIC_ROOT)/benchmarks
DOCS =	   	$(ATOMIC_ROOT)/docs
