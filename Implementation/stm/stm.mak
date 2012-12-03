include $(GRT)/grt.mak

CFLAGS += 	-I$(STM) $(DEBUG)
STM_WB_LIB=	$(STM)/stm_wb.o
STM_BASE_LIB=	$(STM)/stm_base.o
STM-DISTRIB-LIB =  $(STM)/stm-distrib.o
STM-DISTRIB-DEBUG-LIB= $(STM)/stm-distrib-debug.o
STM-DISTRIB-RV-LIB = $(STM)/stm-distrib-rv.o
STM-DISTRIB-RV-DEBUG-LIB = $(STM)/stm-distrib-rv-debug.o
STM-DISTRIB-WB-LIB = $(STM)/stm-distrib-wb.o
STM-DISTRIB-WB-DEBUG-LIB = $(STM)/stm-distrib-wb-debug.o
STM-DISTRIB-LA-LIB = $(STM)/stm-distrib-la.o
STM-DISTRIB-LA-DEBUG-LIB = $(STM)/stm-distrib-la-debug.o

$(STM_BASE_LIB) ::
	$(MAKE) -C $(STM) stm_base.o

$(STM-DISTRIB-LIB) ::
	$(MAKE) -C $(STM) stm-distrib.o

$(STM-DISTRIB-DEBUG-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-debug.o

$(STM-DISTRIB-RV-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-rv.o

$(STM-DISTRIB-RV-DEBUG-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-rv-debug.o

$(STM-DISTRIB-WB-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-wb.o

$(STM-DISTRIB-WB-DEBUG-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-wb-debug.o

$(STM-DISTRIB-LA-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-la.o

$(STM-DISTRIB-LA-DEBUG-LIB) ::
	$(MAKE) -C $(STM) stm-distrib-la-debug.o
