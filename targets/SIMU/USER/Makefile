CC = gcc

CPUFLAGS = -mmmx -msse -msse2 -m32  #-msse4
CPUFLAGS += $(shell if grep --silent ssse3 /proc/cpuinfo ; then echo "-mssse3" ; else echo ""; fi)
#CPUFLAGS += $(shell if grep --silent sse4 /proc/cpuinfo ; then echo "-msse4" ; else echo ""; fi)
linux = $(shell if [ `uname` = "Linux" ] ; then echo "1" ; else echo "0" ; fi)

TOP_DIR      = $(OPENAIR1_DIR)
OPENAIR1_TOP = $(OPENAIR1_DIR)
OPENAIR2_TOP = $(OPENAIR2_DIR)
OPENAIR3_TOP = $(OPENAIR3_DIR)
OPENAIR3     = $(OPENAIR3_DIR)

CFLAGS = -Wall -fno-strict-aliasing -DPHYSIM -DUSER_MODE -DPC_TARGET -DPC_DSP -DNB_ANTENNAS_RX=2 -DNB_ANTENNAS_TXRX=2 -DNB_ANTENNAS_TX=2 -g -ggdb $(CPUFLAGS) -I/usr/include/X11 #-Wno-packed-bitfield-compat

ASN1_MSG_INC = $(OPENAIR2_DIR)/RRC/LITE/MESSAGES

CFLAGS += -DOPENAIR_LTE -DPUCCH  #-DIFFT_FPGA -DIFFT_FPGA_UE #-DOFDMA_ULSCH

ifndef OPENAIR2
OPENAIR2=1
endif

ifndef OPENAIR_EMU
OPENAIR_EMU=1
endif

# activate OCG and libxml only under linux
ifeq ($(linux),1) 
CFLAGS += -I/usr/include/libxml2 -L/usr/lib -lxml2 -lforms -L/usr/local/lib -I/usr/include/atlas -lforms -lX11 -L/usr/X11R6/lib -lXpm -lrt
OCG_FLAG = 1
cygwin=0
else
cygwin=1
OCG_FLAG = 0
endif

ifdef XFORMS
CFLAGS += -DXFORMS
endif

ifndef rrc_cellular
rrc_cellular = 0
rrc_cellular_eNB = 0
rrc_cellular_UE = 0
else
  ifeq ($(eNB_flag),1)
    rrc_cellular_eNB=1
  endif
  ifeq ($(UE_flag),1)
    rrc_cellular_UE=1
  endif
endif

ifdef DEBUG_PHY
CFLAGS += -DDEBUG_PHY
endif

ifdef NAS_NETLINK
CFLAGS += -DNAS_NETLINK -DLINUX
NAS_FLAG=1
CFLAGS += -DDEBUG_OMG
else
NAS_FLAG=0
endif
# after the execution, a profiling output "gmon.out" will be created, 
#then we need to run "gprof ./oaisim > gmon.txt" in the same directory as "gmon.out"
# you could also use the script "grpof2dot.py gmon.txt" for visual diagrams 
ifdef GPROF
CFLAGS += -pg
endif

#require kernel patch for oai driver to enable address autoconfiguration (IPv6 only)
ifdef ADDCONF
CFLAGS+=-DADDRCONF
endif

CFLAGS += -DPHY_ABSTRACTION  #-DEMIT_ASN_DEBUG=1
CFLAGS += $(shell if [ `uname -o` = "Cygwin" ] ; then echo "-DCYGWIN" ;fi)

include $(OPENAIR1_DIR)/PHY/Makefile.inc
include $(OPENAIR1_DIR)/SCHED/Makefile.inc
include $(OPENAIR2_DIR)/LAYER2/Makefile.inc
include $(OPENAIR1_DIR)/SIMULATION/ETH_TRANSPORT/Makefile.inc
include $(OPENAIR2_DIR)/RRC/LITE/MESSAGES/Makefile.inc
include $(OPENAIR2_DIR)/RRC/NAS/Makefile.inc

SIMULATION_OBJS  = $(TOP_DIR)/SIMULATION/TOOLS/gauss.o  
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/TOOLS/random_channel.o  
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/TOOLS/rangen_double.o  
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/TOOLS/taus.o  
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/TOOLS/multipath_channel.o
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/TOOLS/abstraction.o
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/RF/rf.o
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/RF/adc.o
SIMULATION_OBJS += $(TOP_DIR)/SIMULATION/RF/dac.o

ASN1_MSG_OBJS1=$(addprefix $(OPENAIR2_DIR)/RRC/LITE/MESSAGES/, $(ASN1_MSG_OBJS))
#L2_OBJS =
#EXTRA_CFLAGS =

OAISIM_OBJS = channel_sim.o 
OAISIM_OBJS += init_lte.o 
OAISIM_OBJS += oaisim_config.o
OAISIM_OBJS += sinr_sim.o 
OAISIM_OBJS += cor_SF_sim.o
OAISIM_OBJS += phy_procedures_sim_form.o

INT_OBJS =  $(OPENAIR_TARGETS)/SIMU/PROC/interface/Iface_Process.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/gm_init_if.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/gm_update.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/gm_pc_if.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/message_modules.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/message_transport.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/openair_inet_sockets.o
INT_OBJS += $(OPENAIR_TARGETS)/SIMU/PROC/interface/openair_io_sync.o
INT_OBJS +=$(OPENAIR_TARGETS)/SIMU/PROC/channel_sim_proc.o
INT_OBJS +=$(OPENAIR_TARGETS)/SIMU/PROC/buffer_mmap.o
INT_OBJS +=$(OPENAIR_TARGETS)/SIMU/PROC/Process.o

OBJ = $(PHY_OBJS) $(SIMULATION_OBJS) $(ETHERNET_TRANSPORT_OBJS) $(TOOLS_OBJS) $(SCHED_OBJS) $(STATS_OBJS) $(ASN1_MSG_OBJS1) $(OAISIM_OBJS) $(NAS_OBJS)  $(INT_OBJS)
#oaisim_config.o 

all: oaisim

printvars:
	@echo LINUX variable is $(linux)
	@echo CYGWIN variable is $(cygwin)
	@echo rrc_cellular variable is $(rrc_cellular)
	@echo EXTRA_CFLAGS = $(EXTRA_CFLAGS)
	@echo OAI Config generator is $(OCG_FLAG)	
	@echo OAI NETLINK FLAG is $(NAS_FLAG)
	@echo SSE3 proc is $(SSE3PROC)

$(OBJ): %.o : %.c
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -DPHY_CONTEXT=1 -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o $@ $<

$(L2_OBJS): %.o : %.c
	$(CC) -c $(CFLAGS) $(EXTRA_CFLAGS) -DMAC_CONTEXT=1 -DPHY_CONTEXT=1 -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o $@ $<

oaisim : $(OBJ) $(L2_OBJS) oaisim.c  
	echo CYGWIN variable is $(cygwin)
	$(CC)  oaisim.c -I$(TOP_DIR) $(L2_incl) -I$(ASN1_MSG_INC) -o oaisim $(CFLAGS) $(EXTRA_CFLAGS) $(OBJ) $(L2_OBJS) -lm -lblas -lpthread -llapack_atlas

ifeq ($(rrc_cellular_eNB),1)
	mv oaisim oaisim_eNB
endif
ifeq ($(rrc_cellular_UE),1)
	mv oaisim oaisim_UE
endif

nasmesh_fix:
	(cd $(OPENAIR2_DIR) && make nasmesh_netlink_address_fix.ko)
	(sudo insmod $(OPENAIR2_DIR)/NAS/DRIVER/MESH/nasmesh.ko)
nasmesh_nl:
	(cd $(OPENAIR2_DIR) && make nasmesh_netlink.ko)	
	(sudo insmod $(OPENAIR2_DIR)/NAS/DRIVER/MESH/nasmesh.ko)
rb_tool:
	(cd $(OPENAIR2_DIR)/NAS/DRIVER/MESH/RB_TOOL && make)

nasmesh_install:
	(sudo rmmod nasmesh)
	(sudo insmod $(OPENAIR2_DIR)/NAS/DRIVER/MESH/nasmesh.ko)
clean:
	rm -f oaisim
	rm -f $(OBJ)
	rm -f $(L2_OBJS)
	rm -f *.o
	rm -f *.exe*

cleanl1:
	rm -f oaisim
	rm -f $(OBJ)
	rm -f *.o
	rm -f *.exe

cleanl2:
	rm -f $(L2_OBJS)

cleanasn1:
	rm -f $(ASN1_MSG_OBJS1)
	rm -f $(OPENAIR2_DIR)/RRC/LITE/MESSAGES/Makefile.inc.generated

print:
	@echo $(cygwin)
	@echo $(OCG_FLAG)
	@echo $(SSE3PROC)
