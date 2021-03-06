CC = gcc

SSE3PROC = $(shell echo `grep ssse3 /proc/cpuinfo`)
SSE4PROC = $(shell echo `grep sse4 /proc/cpuinfo`)
CPUFLAGS = -mmmx -msse -msse2 -m32  -mssse3 #-msse4
CPUFLAGS += $(shell if [ -z $$SSE3PROC]; then echo "" ; else echo "-mssse3"; fi)
CPUFLAGS += $(shell if [ -z $$SSE4PROC]; then echo "" ; else echo "-msse4"; fi)
cygwin = $(shell if [ $$OSTYPE = "cygwin" ] ; then echo "1" ; else echo "0" ; fi)

TOP_DIR      = $(OPENAIR1_DIR)
OPENAIR1_TOP = $(OPENAIR1_DIR)
OPENAIR2_TOP = $(OPENAIR2_DIR)
OPENAIR3_TOP = $(OPENAIR3_DIR)
OPENAIR3     = $(OPENAIR3_DIR)

CFLAGS = -Wall -fno-strict-aliasing -DPHYSIM -DNODE_RG -DUSER_MODE -DPC_TARGET -DPC_DSP -DNB_ANTENNAS_RX=2 -DNB_ANTENNAS_TXRX=2 -DNB_ANTENNAS_TX=2 -DMAX_MODULES=1 -g -ggdb $(CPUFLAGS) -I/usr/include/X11 #-Wno-packed-bitfield-compat

ASN1_MSG_INC = $(OPENAIR2_DIR)/RRC/LITE/MESSAGES

CFLAGS += -DOPENAIR_LTE -DOPENAIR2 #-DOFDMA_ULSCH -DIFFT_FPGA -DIFFT_FPGA_UE
CFLAGS += -I/usr/include/libxml2 -lxml2

#CFLAGS += -DXFORMS

ifdef DEBUG_PHY
CFLAGS += -DDEBUG_PHY
endif

ifdef NAS_NETLINK
CFLAGS += -DNAS_NETLINK -DLINUX -DDEBUG_CONTROL
endif

CFLAGS += -DPHY_ABSTRACTION  #-DEMIT_ASN_DEBUG=1
CFLAGS += $(shell if [ $$OSTYPE = "cygwin" ] ; then echo "-DCYGWIN" ; fi)

#include $(OPENAIR1_DIR)/PHY/Makefile.inc
#include $(OPENAIR1_DIR)/SCHED/Makefile.inc


EXTRA_CFLAGS += -DOPENAIR2
EXTRA_CFLAGS += -DASSERT
EXTRA_CFLAGS += -DNO_RRM
EXTRA_CFLAGS += -DTEST_RLC_AM

COMMON_DIR = $(OPENAIR2_TOP)/COMMON
L2_INTERFACE_DIR = $(OPENAIR2_TOP)/RRC/L2_INTERFACE
RLC_UM_DIR = $(OPENAIR2_TOP)/LAYER2/RLC/UM_v9.3.0
RLC_AM_DIR = $(OPENAIR2_TOP)/LAYER2/RLC/AM_v9.3.0
RLC_TM_DIR = $(OPENAIR2_TOP)/LAYER2/RLC/TM
RLC_DIR = $(OPENAIR2_TOP)/LAYER2/RLC
PDCP_DIR = $(OPENAIR2_TOP)/LAYER2/PDCP
MEM_DIR = $(OPENAIR2_TOP)/UTIL/MEM
LIST_DIR = $(OPENAIR2_TOP)/UTIL/LISTS
MATH_DIR = $(OPENAIR2_TOP)/UTIL/MATH
TIMER_DIR = $(OPENAIR2_TOP)/UTIL/TIMER
PHY_INTERFACE_DIR = $(OPENAIR2_TOP)/PHY_INTERFACE
RRC_DIR = $(OPENAIR2_TOP)/RRC/LITE
L2_INTERFACE_DIR = $(OPENAIR2_TOP)/RRC/L2_INTERFACE
MAC_DIR = $(OPENAIR2_TOP)/LAYER2/MAC
LOG_DIR=$(OPENAIR2_TOP)/UTIL/LOG
OCG_DIR=$(OPENAIR2_TOP)/UTIL/OCG
OPT_DIR=$(OPENAIR2_TOP)/UTIL/OPT


SOURCES_L2 =  $(LIST_DIR)/list.c
SOURCES_L2 +=  $(LIST_DIR)/list2.c

SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_init.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_timer_poll_retransmit.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_timer_reordering.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_timer_status_prohibit.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_segment.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_segments_holes.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_in_sdu.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_receiver.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_retransmit.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_windows.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_rx_list.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_reassembly.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_status_report.c
SOURCES_L2 +=  $(RLC_AM_DIR)/rlc_am_test.c

SOURCES_L2 +=  $(MEM_DIR)/mem_block.c

LOG_OBJS =  $(LOG_DIR)/log.o
OPT_OBJS =  $(OPT_DIR)/probe.o

L2_OBJS=$(addsuffix .o,$(basename $(SOURCES_L2))) $(PHY_INTERFACE_OBJS) $(MAC_OBJS) $(L2_INTERFACE_OBJS) $(RRC_OBJS) $(LOG_OBJS) $(OPT_OBJS)


L2_incl = -I$(OPENAIR2_TOP)  -I$(COMMON_DIR) -I$(RLC_DIR) -I$(RLC_AM_DIR) -I$(RLC_UM_DIR)  -I$(RLC_TM_DIR) -I$(PDCP_DIR) -I$(MEM_DIR) -I$(LIST_DIR)  -I$(LOG_DIR) -I$(MATH_DIR) -I$(TIMER_DIR) -I$(OPENAIR3)/MESH -I$(OPENAIR2_TOP)/RRC/MESH -I$(OPENAIR2_TOP)/RRC/MESH/MESSAGES -I$(OPENAIR3)/MESH/RRM -I$(OPT_DIR)






ASN1_MSG_OBJS2 := $(foreach file, $(ASN1_MSG_OBJS), $(OPENAIR2_DIR)/RRC/LITE/MESSAGES/$(file))

OBJ = $(PHY_OBJS) $(SIMULATION_OBJS)  $(SCHED_OBJS) $(STATS_OBJS) $(ASN1_MSG_OBJS2)

all: testam


$(OBJ) : %.o : %.c
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -DPHY_CONTEXT=1 -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o $@ $<

$(L2_OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -DMAC_CONTEXT=1 -DPHY_CONTEXT=1 -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o $@ $<

testam : $(OBJ) $(L2_OBJS) oaisim.c
	echo CYGWIN variable is $(cygwin)
	$(CC)  oaisim.c -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o testam $(CFLAGS) $(EXTRA_CFLAGS) $(OBJ) $(L2_OBJS) -lm -lcurses


clean:
	rm -f testam
	rm -f $(OBJ)
	rm -f $(L2_OBJS)
	rm -f *.o
	rm -f *.exe*

