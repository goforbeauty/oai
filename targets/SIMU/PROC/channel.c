#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cblas.h>


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#define SHMSZ     27

 char cr;
 int shmid;
 key_t key;
 char *shm, *s;

#include "SIMULATION/RF/defs.h"
#include "PHY/types.h"
#include "PHY/defs.h"
#include "PHY/vars.h"
#include "MAC_INTERFACE/vars.h"

//#ifdef OPENAIR2
#include "LAYER2/MAC/defs.h"
#include "LAYER2/MAC/vars.h"
#include "RRC/LITE/vars.h"
#include "PHY_INTERFACE/vars.h"
//#endif

#include "ARCH/CBMIMO1/DEVICE_DRIVER/vars.h"

#ifdef IFFT_FPGA
//#include "PHY/LTE_REFSIG/mod_table.h"
#endif //IFFT_FPGA

#include "SCHED/defs.h"
#include "SCHED/vars.h"

#ifdef XFORMS
#include "forms.h"
#include "SIMULATION/LTE_PHY_L2/phy_procedures_sim_form.h"
#endif //XFORMS

#include "../USER/oaisim.h"
#include "../USER/oaisim_config.h"
#include "UTIL/OCG/OCG_extern.h"
#include "../USER/cor_SF_sim.h"
#include "interface.h"
#include "channel_sim_proc.h"
#include "interface/definitions.h"
#include "Tsync.h"


#define RF

#define DEBUG_SIM

#define MCS_COUNT 23//added for PHY abstraction
#define N_TRIALS 1

/*
  DCI0_5MHz_TDD0_t          UL_alloc_pdu;
  DCI1A_5MHz_TDD_1_6_t      CCCH_alloc_pdu;
  DCI2_5MHz_2A_L10PRB_TDD_t DLSCH_alloc_pdu1;
  DCI2_5MHz_2A_M10PRB_TDD_t DLSCH_alloc_pdu2;
 */

#define UL_RB_ALLOC computeRIV(lte_frame_parms->N_RB_UL,0,24)
#define CCCH_RB_ALLOC computeRIV(lte_frame_parms->N_RB_UL,0,3)
#define RA_RB_ALLOC computeRIV(lte_frame_parms->N_RB_UL,0,3)
#define DLSCH_RB_ALLOC 0x1fff

#define DECOR_DIST 100
#define SF_VAR 10

//constant for OAISIM soft realtime calibration
#define SF_DEVIATION_OFFSET_NS 100000 //= 0.1ms : should be as a number of UE
#define SLEEP_STEP_US       100	//  = 0.01ms could be adaptive, should be as a number of UE
#define K 2                  // averaging coefficient 
#define TARGET_SF_TIME_NS 1000000	// 1ms = 1000000 ns

#ifdef OPENAIR2
u16 NODE_ID[1];
u8 NB_INST = 2;
#endif //OPENAIR2
char stats_buffer[16384];
channel_desc_t *eNB2UE[NUMBER_OF_eNB_MAX][NUMBER_OF_UE_MAX];
channel_desc_t *UE2eNB[NUMBER_OF_UE_MAX][NUMBER_OF_eNB_MAX];
//Added for PHY abstraction
node_desc_t *enb_data[NUMBER_OF_eNB_MAX]; 
node_desc_t *ue_data[NUMBER_OF_UE_MAX];
double sinr_bler_map[MCS_COUNT][2][9];

//OAI_Emulation * emulation_scen;
mapping small_scale_names[] =
{
    {"custom", 0},
    {"SCM_A", 1},
    {"SCM_B", 2},
    {"SCM_C", 3},
    {"SCM_D", 4},
    {"EPA", 5},
    {"EVA", 6},
    {"ETU", 7},
    {"Rayleigh8", 8},
    {"Rayleigh1", 9},
    {"Rice8", 10},
    {"Rice1", 11},
    {NULL, -1}
};


#ifdef LINUX
void
init_bypass (){

  msg ("[PHYSIM] INIT BYPASS\n");
  pthread_mutex_init (&Tx_mutex, NULL);
  pthread_cond_init (&Tx_cond, NULL);
  Tx_mutex_var = 1;
  pthread_mutex_init (&emul_low_mutex, NULL);
  pthread_cond_init (&emul_low_cond, NULL);
  emul_low_mutex_var = 1;
  bypass_init (emul_tx_handler, emul_rx_handler);
}
#endif //LINUX


#define NINST 10
void 
help (void) {
  printf
    ("Usage: oaisim -h -a -F -C tdd_config -R N_RB_DL -e -x transmission_mode -m target_dl_mcs -r(ate_adaptation) -n n_frames -s snr_dB -k ricean_factor -t max_delay -f forgetting factor -z cooperation_flag -u nb_local_ue -U omg_model_ue -b nb_local_enb -B omg_model_enb -M ethernet_flag -p nb_master -g multicast_group -l log_level -c ocg_enable \n");
  printf ("-h provides this help message!\n");
  printf ("-a Activates PHY abstraction mode\n");
  printf ("-F Activates FDD transmission (TDD is default)\n");
  printf ("-C [0-6] Sets TDD configuration\n");
  printf ("-R [6,15,25,50,75,100] Sets N_RB_DL\n");
  printf ("-e Activates extended prefix mode\n");
  printf ("-m Gives a fixed DL mcs\n");
  printf ("-r Activates rate adaptation (DL for now)\n");
  printf ("-n Set the number of frames for the simulation\n");
  printf ("-s snr_dB set a fixed (average) SNR, this deactivate the openair channel model generator (OCM)\n");
  printf ("-S snir_dB set a fixed (average) SNIR, this deactivate the openair channel model generator (OCM)\n");
  printf ("-k Set the Ricean factor (linear)\n");
  printf ("-t Set the delay spread (microseconds)\n");
  printf ("-f Set the forgetting factor for time-variation\n");
  printf ("-b Set the number of local eNB\n");
  printf ("-u Set the number of local UE\n");
  printf ("-M Set the machine ID for Ethernet-based emulation\n");
  printf ("-p Set the total number of machine in emulation - valid if M is set\n");
  printf ("-g Set multicast group ID (0,1,2,3) - valid if M is set\n");
  printf ("-l Set the log level (trace, debug, info, warn, err) only valid for MAC layer\n");
  printf
    ("-c [1,2,3,4] Activate the config generator (OCG) to process the scenario descriptor, or give the scenario manually: -c template_1.xml \n");
  printf ("-x Set the transmission mode (1,2,6 supported for now)\n");
  printf ("-z Set the cooperation flag (0 for no cooperation, 1 for delay diversity and 2 for distributed alamouti\n");
  printf ("-B Set the mobility model for eNB: 0 for static, 1 for RWP, and 2 for RWalk, 3 for mixed\n");
  printf ("-U Set the mobility model for UE : 0 for static, 1 for RWP, and 2 for RWalk, 3 for mixed\n");
  printf ("-E Random number generator seed\n");
}

#ifdef XFORMS
void
do_forms (FD_phy_procedures_sim * form,
	  LTE_UE_DLSCH ** lte_ue_dlsch_vars, LTE_eNB_ULSCH ** lte_eNB_ulsch_vars, struct complex **ch, u32 ch_len)
{

  s32 j, s, i;
  float I[3600], Q[3600], I2[3600], Q2[3600], I3[300], Q3[300];

  j = 0;
  //  printf("rxdataF_comp %p, lte_ue_dlsch_vars[0] %p\n",lte_ue_dlsch_vars[0]->rxdataF_comp[0],lte_ue_dlsch_vars[0]);
  for (s = 4; s < 12; s++) {
    for (i = 0; i < 12 * 12; i++) {
      I[j] = (float) ((short *)
		      lte_ue_dlsch_vars[0]->rxdataF_comp[0])[(2 * 25 * 12 * s) + 2 * i];
      Q[j] = (float) ((short *)
		      lte_ue_dlsch_vars[0]->rxdataF_comp[0])[(2 * 25 * 12 * s) + 2 * i + 1];
      //      printf("%d (%d): %f,%f : %d,%d\n",j,(25*12*s)+i,I[j],Q[j],lte_ue_dlsch_vars[0]->rxdataF_comp[0][(2*25*12*s)+2*i],lte_ue_dlsch_vars[0]->rxdataF_comp[0][(2*25*12*s)+2*i+1]);
      j++;
    }
    if (s == 5)
      s = 6;
    else if (s == 8)
      s = 9;
  }
  if (j > 0)
    fl_set_xyplot_data (form->pdsch_constellation, I, Q, j, "", "", "");


  //fl_set_xyplot_xbounds(form->pdsch_constellation,-800,800);
  //fl_set_xyplot_ybounds(form->pdsch_constellation,-800,800);


  j = 0;
  //  printf("rxdataF_comp %p, lte_ue_dlsch_vars[0] %p\n",lte_ue_dlsch_vars[0]->rxdataF_comp[0],lte_ue_dlsch_vars[0]);
  for (s = 0; s < 12; s++) {
    for (i = 0; i < 6 * 12; i++) {
      I2[j] = (float) ((short *)
		       lte_eNB_ulsch_vars[0]->rxdataF_comp[0][0])[(2 * 25 * 12 * s) + 2 * i];
      Q2[j] = (float) ((short *)
		       lte_eNB_ulsch_vars[0]->rxdataF_comp[0][0])[(2 * 25 * 12 * s) + 2 * i + 1];
      //      printf("%d (%d): %f,%f : %d,%d\n",j,(25*12*s)+i,I[j],Q[j],lte_ue_dlsch_vars[0]->rxdataF_comp[0][(2*25*12*s)+2*i],lte_ue_dlsch_vars[0]->rxdataF_comp[0][(2*25*12*s)+2*i+1]);
      j++;
    }
    if (s == 1)
      s = 2;
    else if (s == 7)
      s = 8;
  }
  if (j > 0)
    fl_set_xyplot_data (form->pusch_constellation, I2, Q2, j, "", "", "");

  fl_set_xyplot_xbounds(form->pusch_constellation,-800,800);
  fl_set_xyplot_ybounds(form->pusch_constellation,-800,800);

  for (j = 0; j < ch_len; j++) {

    I3[j] = j;
    Q3[j] = 10 * log10 (ch[0][j].x * ch[0][j].x + ch[0][j].y * ch[0][j].y);
  }

  fl_set_xyplot_data (form->ch00, I3, Q3, ch_len, "", "", "");
  //fl_set_xyplot_ybounds(form->ch00,-20,20);
}
#endif //XFORMS


int
main (int argc, char **argv)
{

	 Interface_init();
	 s_t* st2=(s_t*)(Instance[1].gm->mem_ref.pointer);
	 st2->port=38800;

  char c;
  s32 i, j;
  int new_omg_model; // goto ocg in oai_emulation.info.
  // pointers signal buffers (s = transmit, r,r0 = receive)
  double **s_re[NINST], **s_im[NINST], **r_re[NINST], **r_im[NINST], **r_re0, **r_im0;
  double **r_re0_d[8][3], **r_im0_d[8][3], **r_re0_u[3][8],**r_im0_u[3][8];

 // double **s_re, **s_im, **r_re, **r_im, **r_re0, **r_im0;

  double forgetting_factor=0;
  int map1,map2;
  double **ShaF= NULL;

  // Framing variables
  s32 slot, last_slot, next_slot;

  // variables/flags which are set by user on command-line
  double snr_dB, sinr_dB;
  u8 set_snr=0,set_sinr=0;

  u8 cooperation_flag;		// for cooperative communication
  u8 target_dl_mcs = 4;
  u8 target_ul_mcs = 2;
  u8 rate_adaptation_flag;

  u8 abstraction_flag = 0, ethernet_flag = 0;

  u16 Nid_cell = 0;
  s32 UE_id, eNB_id, ret;

  // time calibration for soft realtime mode  
  struct timespec time_spec;
  unsigned long time_last, time_now;
  int td, td_avg, sleep_time_us;

  char *g_log_level = "trace";	// by default global log level is set to trace
  lte_subframe_t direction;

 #ifdef XFORMS
  FD_phy_procedures_sim *form[NUMBER_OF_eNB_MAX][NUMBER_OF_UE_MAX];
  char title[255];
#endif
  LTE_DL_FRAME_PARMS *frame_parms;

  FILE *UE_stats[NUMBER_OF_UE_MAX], *eNB_stats;
  char UE_stats_filename[255];
  int len;
#ifdef ICIC
  remove ("dci.txt");
#endif

  //time_t t0,t1;
  clock_t start, stop;

  // Added for PHY abstraction
  Node_list ue_node_list = NULL;
  Node_list enb_node_list = NULL;
 
  //default parameters
  target_dl_mcs = 0;
  rate_adaptation_flag = 0;
  oai_emulation.info.n_frames = 0xffff;//1024;		//100;
  oai_emulation.info.n_frames_flag = 0;//fixme
  snr_dB = 30;
  cooperation_flag = 0;		// default value 0 for no cooperation, 1 for Delay diversity, 2 for Distributed Alamouti


   init_oai_emulation(); // to initialize everything !!!


   // get command-line options
  while ((c = getopt (argc, argv, "haePToFt:C:N:k:x:m:rn:s:S:f:z:u:b:c:M:p:g:l:d:U:B:R:E:"))
	 != -1) {

    switch (c) {
    case 'F':			// set FDD
      oai_emulation.info.frame_type = 0;
      break;
    case 'C':
      oai_emulation.info.tdd_config = atoi (optarg);
      if (oai_emulation.info.tdd_config > 6) {
	msg ("Illegal tdd_config %d (should be 0-6)\n", oai_emulation.info.tdd_config);
	exit (-1);
      }
      break;
    case 'R':
      oai_emulation.info.N_RB_DL = atoi (optarg);
      if ((oai_emulation.info.N_RB_DL != 6) && (oai_emulation.info.N_RB_DL != 15) && (oai_emulation.info.N_RB_DL != 25)
	  && (oai_emulation.info.N_RB_DL != 50) && (oai_emulation.info.N_RB_DL != 75) && (oai_emulation.info.N_RB_DL != 100)) {
	msg ("Illegal N_RB_DL %d (should be one of 6,15,25,50,75,100)\n", oai_emulation.info.N_RB_DL);
	exit (-1);
      }
    case 'N':
      Nid_cell = atoi (optarg);
      if (Nid_cell > 503) {
	msg ("Illegal Nid_cell %d (should be 0 ... 503)\n", Nid_cell);
	exit(-1);
      }
      break;
    case 'h':
      help ();
      exit (1);
    case 'x':
      oai_emulation.info.transmission_mode = atoi (optarg);
      if ((oai_emulation.info.transmission_mode != 1) &&  (oai_emulation.info.transmission_mode != 2) && (oai_emulation.info.transmission_mode != 5) && (oai_emulation.info.transmission_mode != 6)) {
	msg("Unsupported transmission mode %d\n",oai_emulation.info.transmission_mode);
	exit(-1);
      }
      break;
    case 'm':
      target_dl_mcs = atoi (optarg);
      break;
    case 'r':
      rate_adaptation_flag = 1;
      break;
    case 'n':
      oai_emulation.info.n_frames = atoi (optarg);
      //n_frames = (n_frames >1024) ? 1024: n_frames; // adjust the n_frames if higher that 1024
      oai_emulation.info.n_frames_flag = 1;
      break;
    case 's':
      snr_dB = atoi (optarg);
      set_snr = 1;
      oai_emulation.info.ocm_enabled=0;
      break;
    case 'S':
      sinr_dB = atoi (optarg);
      set_sinr = 1;
      oai_emulation.info.ocm_enabled=0;
      break;
    case 'k':
      //ricean_factor = atof (optarg);
      printf("[SIM] Option k is no longer supported on the command line. Please specify your channel model in the xml template\n"); 
      exit(-1);
      break;
    case 't':
      //Td = atof (optarg);
      printf("[SIM] Option t is no longer supported on the command line. Please specify your channel model in the xml template\n"); 
      exit(-1);
      break;
    case 'f':
      forgetting_factor = atof (optarg);
      break;
    case 'z':
      cooperation_flag = atoi (optarg);
      break;
    case 'u':
      oai_emulation.info.nb_ue_local = atoi (optarg);
      break;
    case 'b':
      oai_emulation.info.nb_enb_local = atoi (optarg);
      break;
    case 'a':
      abstraction_flag = 1;
      break;
    case 'p':
      oai_emulation.info.nb_master = atoi (optarg);
      break;
    case 'M':
      abstraction_flag = 1;
      ethernet_flag = 1;
      oai_emulation.info.ethernet_id = atoi (optarg);
      oai_emulation.info.master_id = oai_emulation.info.ethernet_id;
      oai_emulation.info.ethernet_flag = 1;
      break;
    case 'e':
      oai_emulation.info.extended_prefix_flag = 1;
      break;
    case 'l':
      g_log_level = optarg;
      break;
    case 'c':
      strcpy(oai_emulation.info.local_server, optarg);
      oai_emulation.info.ocg_enabled=1;
      break;
    case 'g':
      oai_emulation.info.multicast_group = atoi (optarg);
      break;
    case 'B':
      oai_emulation.info.omg_model_enb = atoi (optarg);
      break;
    case 'U':
      oai_emulation.info.omg_model_ue = atoi (optarg);
      break;
    case 'T':
      oai_emulation.info.otg_enabled = 1;
      break;
    case 'P':
      oai_emulation.info.opt_enabled = 1;
      break;
    case 'E':
      oai_emulation.info.seed = atoi (optarg);
      break;
    default:
      help ();
      exit (-1);
      break;
    }
  }

  // configure oaisim with OCG
  oaisim_config(g_log_level); // config OMG and OCG, OPT, OTG, OLG

  if (oai_emulation.info.nb_ue_local > NUMBER_OF_UE_MAX ) {
    printf ("Enter fewer than %d UEs for the moment or change the NUMBER_OF_UE_MAX\n", NUMBER_OF_UE_MAX);
    exit (-1);
  }
  if (oai_emulation.info.nb_enb_local > NUMBER_OF_eNB_MAX) {
    printf ("Enter fewer than %d eNBs for the moment or change the NUMBER_OF_UE_MAX\n", NUMBER_OF_eNB_MAX);
    exit (-1);
  }
	
  // fix ethernet and abstraction with RRC_CELLULAR Flag
#ifdef RRC_CELLULAR
  abstraction_flag = 1;
  ethernet_flag = 0;
#endif

  if (set_sinr == 0)
    sinr_dB = snr_dB - 20;

  // setup ntedevice interface (netlink socket)
#ifndef CYGWIN
  ret = netlink_init ();
#endif

  if (ethernet_flag == 1) {
    oai_emulation.info.master[oai_emulation.info.master_id].nb_ue = oai_emulation.info.nb_ue_local;
    oai_emulation.info.master[oai_emulation.info.master_id].nb_enb = oai_emulation.info.nb_enb_local;

    if (!oai_emulation.info.master_id)
      oai_emulation.info.is_primary_master = 1;
    j = 1;
    for (i = 0; i < oai_emulation.info.nb_master; i++) {
      if (i != oai_emulation.info.master_id)
	oai_emulation.info.master_list = oai_emulation.info.master_list + j;
      LOG_I (EMU, "Index of master id i=%d  MASTER_LIST %d\n", i, oai_emulation.info.master_list);
      j *= 2;
    }
    LOG_I (EMU, " Total number of master %d my master id %d\n", oai_emulation.info.nb_master, oai_emulation.info.master_id);
#ifdef LINUX
    init_bypass ();
#endif

    while (emu_tx_status != SYNCED_TRANSPORT) {
      LOG_I (EMU, " Waiting for EMU Transport to be synced\n");
      emu_transport_sync ();	//emulation_tx_rx();
    }
  }				// ethernet flag
  NB_UE_INST = oai_emulation.info.nb_ue_local + oai_emulation.info.nb_ue_remote;
  NB_eNB_INST = oai_emulation.info.nb_enb_local + oai_emulation.info.nb_enb_remote;
#ifndef NAS_NETLINK
  for (UE_id=0;UE_id<NB_UE_INST;UE_id++) {
    sprintf(UE_stats_filename,"UE_stats%d.txt",UE_id);
    UE_stats[UE_id] = fopen (UE_stats_filename, "w");
  }
  eNB_stats = fopen ("eNB_stats.txt", "w");
  printf ("UE_stats=%p, eNB_stats=%p\n", UE_stats, eNB_stats);
#endif
      
  LOG_I(EMU, "total number of UE %d (local %d, remote %d) \n", NB_UE_INST,oai_emulation.info.nb_ue_local,oai_emulation.info.nb_ue_remote);
  LOG_I(EMU, "Total number of eNB %d (local %d, remote %d) \n", NB_eNB_INST,oai_emulation.info.nb_enb_local,oai_emulation.info.nb_enb_remote);
  printf("Running with frame_type %d, Nid_cell %d, N_RB_DL %d, EP %d, mode %d, target dl_mcs %d, rate adaptation %d, nframes %d, abstraction %d\n",
  	 1+oai_emulation.info.frame_type, Nid_cell, oai_emulation.info.N_RB_DL, oai_emulation.info.extended_prefix_flag, oai_emulation.info.transmission_mode,target_dl_mcs,rate_adaptation_flag,oai_emulation.info.n_frames,abstraction_flag);
  

 // init_lte_vars (&frame_parms, oai_emulation.info.frame_type, oai_emulation.info.tdd_config, oai_emulation.info.extended_prefix_flag,oai_emulation.info.N_RB_DL, Nid_cell, cooperation_flag, oai_emulation.info.transmission_mode, abstraction_flag);
  init_frame_params (&frame_parms, oai_emulation.info.frame_type, oai_emulation.info.tdd_config, oai_emulation.info.extended_prefix_flag,oai_emulation.info.N_RB_DL, Nid_cell, cooperation_flag, oai_emulation.info.transmission_mode, abstraction_flag);

  printf ("Nid_cell %d\n", frame_parms->Nid_cell);

  /* Added for PHY abstraction */
  if (abstraction_flag) 
    get_beta_map();

  for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
    enb_data[eNB_id] = (node_desc_t *)malloc(sizeof(node_desc_t)); 
    init_enb(enb_data[eNB_id],oai_emulation.environment_system_config.antenna.eNB_antenna);
  }
  
  for (UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
    ue_data[UE_id] = (node_desc_t *)malloc(sizeof(node_desc_t));
    init_ue(ue_data[UE_id],oai_emulation.environment_system_config.antenna.UE_antenna);
  } 

  // init SF map here!!!
  map1 =(int)oai_emulation.topology_config.area.x_km;
  map2 =(int)oai_emulation.topology_config.area.y_km;
  //ShaF = createMat(map1,map2); -> memory is allocated within init_SF
  ShaF = init_SF(map1,map2,DECOR_DIST,SF_VAR);

  // size of area to generate shadow fading map
  printf("Simulation area x=%f, y=%f\n",
	 oai_emulation.topology_config.area.x_km,
	 oai_emulation.topology_config.area.y_km);
 



  if (abstraction_flag == 0){

	  int ci;
	  int ji=0;
	  for(ci=0;ci<NB_eNB_INST;ci++)
	  {
		  init_channel_mmap_channel(10+ji,frame_parms, &(s_re[ci]), &(s_im[ci]), &(r_re[ci]), &(r_im[ci]), &(r_re0), &(r_im0));
		ji++;
		//printf("ci %d\n",ci);
	  }
	  ji=0;

	  for(ci=NB_eNB_INST;ci<(NB_eNB_INST+NB_UE_INST);ci++)
		  {
		  init_channel_mmap_channel(20+ji,frame_parms, &(s_re[ci]), &(s_im[ci]), &(r_re[ci]), &(r_im[ci]), &(r_re0), &(r_im0));
			ji++;
			//printf("ci %d\n",ci);
		  }


  }

  for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
      for (UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
  init_rre(frame_parms,&(r_re0_u[eNB_id][UE_id]),&(r_im0_u[eNB_id][UE_id]));
  init_rre(frame_parms,&(r_re0_d[UE_id][eNB_id]),&(r_im0_d[UE_id][eNB_id]));
      }
  }

//      printf("r_re0 %lf , r_im0 %lf\n",r_re0_u[0][0][0][0],r_im0_u[0][0][0][0]);
//
//      r_im0_u[0][0][0][0]=100;
//
//      printf("r_re0 %lf , r_im0 %lf\n",r_re0_u[0][0][0][0],r_im0_u[0][0][0][0]);
//
//
//	  clean_param((r_re0_u[0][0]),(r_im0_u[0][0]),frame_parms);
//
//	  printf("r_re0 %lf , r_im0 %lf\n",r_re0_u[0][0][0][0],r_im0_u[0][0][0][0]);



  for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
    for (UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
#ifdef DEBUG_SIM
      printf ("[SIM] Initializing channel from eNB %d to UE %d\n", eNB_id, UE_id);
#endif

     eNB2UE[eNB_id][UE_id] = new_channel_desc_scm(2,
						   2,
						   map_str_to_int(small_scale_names, oai_emulation.environment_system_config.fading.small_scale.selected_option),
						   oai_emulation.environment_system_config.system_bandwidth_MB,
						   forgetting_factor,
						   0,
						   0);
      
      UE2eNB[UE_id][eNB_id] = new_channel_desc_scm(2,
						   2,
						   map_str_to_int(small_scale_names, oai_emulation.environment_system_config.fading.small_scale.selected_option),
						   oai_emulation.environment_system_config.system_bandwidth_MB,
						   forgetting_factor,
						   0,
						   0);
      
    }
  }

  randominit (0);
  set_taus_seed (0);

  number_of_cards = 1;

  openair_daq_vars.rx_rf_mode = 1;
  openair_daq_vars.tdd = 1;
  openair_daq_vars.rx_gain_mode = DAQ_AGC_ON;
  openair_daq_vars.dlsch_transmission_mode = oai_emulation.info.transmission_mode;
  openair_daq_vars.target_ue_dl_mcs = target_dl_mcs;
  openair_daq_vars.target_ue_ul_mcs = target_ul_mcs;
  openair_daq_vars.dlsch_rate_adaptation = rate_adaptation_flag;
  openair_daq_vars.ue_ul_nb_rb = 2;


  
#ifdef XFORMS
  fl_initialize (&argc, argv, NULL, 0, 0);
  for (UE_id = 0; UE_id < NB_UE_INST; UE_id++)
    for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
      form[eNB_id][UE_id] = create_form_phy_procedures_sim ();
      sprintf (title, "LTE SIM UE %d eNB %d", UE_id, eNB_id);
      fl_show_form (form[eNB_id][UE_id]->phy_procedures_sim, FL_PLACE_HOTSPOT, FL_FULLBORDER, title);
    }
#endif


  // time calibration for OAI 
  clock_gettime (CLOCK_REALTIME, &time_spec);
  time_now = (unsigned long) time_spec.tv_nsec;
  td_avg = 0;
  sleep_time_us = SLEEP_STEP_US;
  td_avg = TARGET_SF_TIME_NS;


   // s_t* st2=(s_t*)(Instance[1].gm->mem_ref.pointer);
  //  st2->Exec_FLAG=0;
    int count;


    IntInitAll();
    Soc_t* this  = (Soc_t*)(obj_inst[0].ptr->mem_ref.pointer);
//    fd_set read_ibits;
//    fd_set write_ibits;
//    int n;
//    struct timeval tvp ;
//    tvp.tv_sec=10;
//    tvp.tv_usec=0;
//
//    FD_ZERO(&read_ibits);
//	FD_SET(this->m_io_sync.io_sync_entries->fd_read, &read_ibits);

//	 ch_thread *e2u_t[NB_eNB_INST][NB_UE_INST];
//	 ch_thread *u2e_t[NB_UE_INST][NB_eNB_INST];

	/*
	 for(eNB_id=0;eNB_id<NB_eNB_INST;eNB_id++){
	 for(UE_id=0;UE_id<NB_UE_INST;UE_id++){
	 e2u_t[eNB_id][UE_id]=(ch_thread*)calloc(1,sizeof(ch_thread));
	 }}
	 for(UE_id=0;UE_id<NB_UE_INST;UE_id++){
	 for(eNB_id=0;eNB_id<NB_eNB_INST;eNB_id++){
	 u2e_t[UE_id][eNB_id]=(ch_thread*)calloc(1,sizeof(ch_thread));
	 }}
	 */

	 //pthread_t thread,thread2;
//	 pthread_t cthr_u[NB_eNB_INST][NB_UE_INST];
//	 pthread_t cthr_d[NB_UE_INST][NB_eNB_INST];

/*

	    pthread_cond_init (&downlink_cond, NULL);
	    pthread_cond_init (&uplink_cond, NULL);
	    pthread_cond_init (&downlink_cond_channel, NULL);
	    pthread_cond_init (&uplink_cond_channel, NULL);

	     NUM_THREAD_DOWNLINK=0;
	     NUM_THREAD_UPLINK=0;
	    for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){

	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

	  		u2e_t[UE_id][eNB_id]->eNB_id=eNB_id;
	  		u2e_t[UE_id][eNB_id]->UE_id=UE_id;
	  		u2e_t[UE_id][eNB_id]->r_re0=r_re0_d[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->r_im0=r_im0_d[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->r_re=r_re[NB_eNB_INST+UE_id];
	  		u2e_t[UE_id][eNB_id]->r_im=r_im[NB_eNB_INST+UE_id];
	  		u2e_t[UE_id][eNB_id]->s_im=s_im[eNB_id];
	  		u2e_t[UE_id][eNB_id]->s_re=s_re[eNB_id];
	  		u2e_t[UE_id][eNB_id]->eNB2UE=eNB2UE[eNB_id][UE_id];
	  		u2e_t[UE_id][eNB_id]->UE2eNB=UE2eNB[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->enb_data=enb_data[eNB_id];
	  		u2e_t[UE_id][eNB_id]->ue_data=ue_data[UE_id];
	  		u2e_t[UE_id][eNB_id]->next_slot=&next_slot;
	  		u2e_t[UE_id][eNB_id]->abstraction_flag=&abstraction_flag;
	  		u2e_t[UE_id][eNB_id]->frame_parms=frame_parms;

	  	pthread_create (&cthr_d[UE_id][eNB_id], NULL, do_DL_sig_channel_T,(void*)(u2e_t[UE_id][eNB_id]));
	  	NUM_THREAD_DOWNLINK++;

	    }
	    } */
/*
     	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

        for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){

				e2u_t[eNB_id][UE_id]->eNB_id=eNB_id;
      		e2u_t[eNB_id][UE_id]->UE_id=UE_id;
      		e2u_t[eNB_id][UE_id]->r_re=r_re[eNB_id];
      		e2u_t[eNB_id][UE_id]->r_im=r_im[eNB_id];
      		e2u_t[eNB_id][UE_id]->r_re0=r_re0_u[eNB_id][UE_id];
      		e2u_t[eNB_id][UE_id]->r_im0=r_im0_u[eNB_id][UE_id];
      		e2u_t[eNB_id][UE_id]->s_im=s_im[NB_eNB_INST+UE_id];
      		e2u_t[eNB_id][UE_id]->s_re=s_re[NB_eNB_INST+UE_id];
      		e2u_t[eNB_id][UE_id]->eNB2UE=eNB2UE[eNB_id][UE_id];
      		e2u_t[eNB_id][UE_id]->UE2eNB=UE2eNB[UE_id][eNB_id];
      		e2u_t[eNB_id][UE_id]->enb_data=enb_data[eNB_id];
      		e2u_t[eNB_id][UE_id]->ue_data=ue_data[UE_id];
      		e2u_t[eNB_id][UE_id]->next_slot=&next_slot;
      		e2u_t[eNB_id][UE_id]->abstraction_flag=&abstraction_flag;
      		e2u_t[eNB_id][UE_id]->frame_parms=frame_parms;

	        	 pthread_create (&cthr_u[eNB_id][UE_id], NULL, do_UL_sig_channel_T,(void*)e2u_t[eNB_id][UE_id]);
	        	 NUM_THREAD_UPLINK++;
      	 //  pthread_create (&thread[eNB_id][UE_id], NULL, do_UL_sig_channel_T,(void*)cthread);
      	//   do_UL_sig_channel(eNB_id,UE_id,r_re0,r_im0,r_re[eNB_id],r_im[eNB_id],s_re[NB_eNB_INST+UE_id],s_im[NB_eNB_INST+UE_id],UE2eNB,next_slot,abstraction_flag,frame_parms);

// do_UL_sig_channel(eNB_id,UE_id,r_re0_u[eNB_id][UE_id],r_im0_u[eNB_id][UE_id],r_re[eNB_id],r_im[eNB_id],s_re[NB_eNB_INST+UE_id],s_im[NB_eNB_INST+UE_id],UE2eNB,next_slot,abstraction_flag,frame_parms);

        }

           }
*/

	 Channel_Inst(s_re,s_im,r_re,r_im,r_re0,r_im0,r_re0_d,r_im0_d,r_re0_u,r_im0_u,eNB2UE,UE2eNB,enb_data,ue_data,abstraction_flag,frame_parms);

	 for (mac_xface->frame=0; mac_xface->frame<oai_emulation.info.n_frames; mac_xface->frame++) {
		  int dir_flag=0;

	      printf("=============== Frame Number %d ============= \n ",mac_xface->frame);


	    /*
	    // Handling the cooperation Flag
	    if (cooperation_flag == 2)
	      {
		if ((PHY_vars_eNB_g[0]->eNB_UE_stats[0].mode == PUSCH) && (PHY_vars_eNB_g[0]->eNB_UE_stats[1].mode == PUSCH))
		  PHY_vars_eNB_g[0]->cooperation_flag = 2;
	      }
	    */
	    // for dubugging the frame counter

	    update_nodes(oai_emulation.info.time);

	    enb_node_list = get_current_positions(oai_emulation.info.omg_model_enb, eNB, oai_emulation.info.time);
	    ue_node_list = get_current_positions(oai_emulation.info.omg_model_ue, UE, oai_emulation.info.time);

	    // update the position of all the nodes (eNB/CH, and UE/MR) every frame
	    if (((int)oai_emulation.info.time % 10) == 0 ) {
	      display_node_list(enb_node_list);
	      display_node_list(ue_node_list);
	      if (oai_emulation.info.omg_model_ue >= MAX_NUM_MOB_TYPES){ // mix mobility model
		for(UE_id=oai_emulation.info.first_ue_local; UE_id<(oai_emulation.info.first_ue_local+oai_emulation.info.nb_ue_local);UE_id++){
		  new_omg_model = randomGen(STATIC, MAX_NUM_MOB_TYPES);
		  LOG_D(OMG, "[UE] Node of ID %d is changing mobility generator ->%d \n", UE_id, new_omg_model);
		  // reset the mobility model for a specific node
		  set_new_mob_type (UE_id, UE, new_omg_model, oai_emulation.info.time);
		}
	      }

	      if (oai_emulation.info.omg_model_enb >= MAX_NUM_MOB_TYPES) {	// mix mobility model
		for (eNB_id = oai_emulation.info.first_enb_local; eNB_id < (oai_emulation.info.first_enb_local + oai_emulation.info.nb_enb_local); eNB_id++) {
		  new_omg_model = randomGen (STATIC, MAX_NUM_MOB_TYPES);
		  LOG_D (OMG, "[eNB] Node of ID %d is changing mobility generator ->%d \n", UE_id, new_omg_model);
		  // reset the mobility model for a specific node
		  set_new_mob_type (eNB_id, eNB, new_omg_model, oai_emulation.info.time);
		}
	      }
	    }

	#ifdef DEBUG_OMG
	    if ((((int) oai_emulation.info.time) % 100) == 0) {
	      for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++) {
		get_node_position (UE, UE_id);
	      }
	    }
	#endif

	    if (oai_emulation.info.n_frames_flag == 0){ // if n_frames not set by the user then let the emulation run to infinity
	      mac_xface->frame %=(oai_emulation.info.n_frames-1);
	      // set the emulation time based on 1ms subframe number
	      oai_emulation.info.time += 0.01; // emu time in s
	    }
	    else { // user set the number of frames for the emulation
	      // let the time go faster to see the effect of mobility
	      oai_emulation.info.time += 0.1;
	    }

	    /* check if the openair channel model is activated used for PHY abstraction */
	    /*    if ((oai_emulation.info.ocm_enabled == 1)&& (ethernet_flag == 0 )) {
	          extract_position(enb_node_list, enb_data, NB_eNB_INST);
	          extract_position(ue_node_list, ue_data, NB_UE_INST);

	          for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
	    	for (UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
	    	  calc_path_loss (enb_data[eNB_id], ue_data[UE_id], eNB2UE[eNB_id][UE_id], oai_emulation.environment_system_config,ShaF[(int)ue_data[UE_id]->x][(int)ue_data[UE_id]->y]);
	    	  UE2eNB[UE_id][eNB_id]->path_loss_dB = eNB2UE[eNB_id][UE_id]->path_loss_dB;
	    	  printf("[CHANNEL_SIM] Pathloss bw enB %d at (%f,%f) and UE%d at (%f,%f) is %f (ShaF %f)\n",
	    		 eNB_id,enb_data[eNB_id]->x,enb_data[eNB_id]->y,UE_id,ue_data[UE_id]->x,ue_data[UE_id]->y,
	    		 eNB2UE[eNB_id][UE_id]->path_loss_dB,
	    		 ShaF[(int)ue_data[UE_id]->x][(int)ue_data[UE_id]->y]);
	    	}
	          }
	        } */

	    //    else {
	          for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
	    	for (UE_id = 0; UE_id < NB_UE_INST; UE_id++) {
	    	  eNB2UE[eNB_id][UE_id]->path_loss_dB = -105 + snr_dB;
	    	  //UE2eNB[UE_id][eNB_id]->path_loss_dB = -105 + snr_dB;
	    	  if (eNB_id == (UE_id % NB_eNB_INST))
	    	    UE2eNB[UE_id][eNB_id]->path_loss_dB = -105 + snr_dB - 10;
	    	  else
	    	    UE2eNB[UE_id][eNB_id]->path_loss_dB = -105 + sinr_dB - 10;
	    #ifdef DEBUG_SIM
	    	  printf("[SIM] Path loss from eNB %d to UE %d => %f dB\n",eNB_id,UE_id,eNB2UE[eNB_id][UE_id]->path_loss_dB);
	    	  printf("[SIM] Path loss from UE %d to eNB %d => %f dB\n",UE_id,eNB_id,UE2eNB[UE_id][eNB_id]->path_loss_dB);
	    #endif
	    	}
	          }
	       // } else



	     st2->EResp_FLAG=0;

	     int Channel_Flag=1;
	    for (slot=0 ; slot<20 ; slot++) {
	        printf("=============== Frame Number %d , Slot %d ============= \n ",mac_xface->frame,slot);


	        if(Channel_Flag==1){
	        	Channel_Func(s_re,s_im,r_re,r_im,r_re0,r_im0,r_re0_d,r_im0_d,r_re0_u,r_im0_u,eNB2UE,UE2eNB,enb_data,ue_data,abstraction_flag,frame_parms,slot);
	        }

	      if(Channel_Flag==0){
	      last_slot = (slot - 1)%20;
	      if (last_slot <0)
		last_slot+=20;
	      next_slot = (slot + 1)%20;

	      direction = subframe_select(frame_parms,next_slot>>1);

	      if (direction  == SF_DL) {
	    	  dir_flag=1;
	      }

	      else if (direction  == SF_UL) {
	    	  dir_flag=2;
	      }
	      else {//it must be a special subframe
		if (next_slot%2==0) {//DL part
			dir_flag=1;
		}
		else {// UL part
			dir_flag=2;
		}
	      }
	int count=0;
	if(dir_flag==1)
	{
	Channel_DL(s_re,s_im,r_re,r_im,r_re0,r_im0,r_re0_d,r_im0_d,r_re0_u,r_im0_u,eNB2UE,UE2eNB,enb_data,ue_data,abstraction_flag,frame_parms,slot);
	}

	if(dir_flag==2)
	{
	Channel_UL(s_re,s_im,r_re,r_im,r_re0,r_im0,r_re0_d,r_im0_d,r_re0_u,r_im0_u,eNB2UE,UE2eNB,enb_data,ue_data,abstraction_flag,frame_parms,slot);
	}

	if(dir_flag==10)
	{
//Channel_DL(s_re,s_im,r_re,r_im,r_re0,r_im0,r_re0_d,r_im0_d,r_re0_u,r_im0_u,eNB2UE,UE2eNB,enb_data,ue_data,abstraction_flag,frame_parms,slot);


		  st2->EResp_FLAG=0;
	 	  count=0;
		    for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

			  send_exec_msg(next_slot,last_slot,mac_xface->frame,mac_xface->frame,slot,0,38810+eNB_id);

			  printf("Waiting for Exec Msg Complete \n");

			//	n = openairIoSyncCreateThread_2(&this->m_io_sync);

		    } // for loop

			while(count<NB_eNB_INST){
			n=trig_wait((void*)&this->m_io_sync);
			count++;
			}

//		    for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
//		   	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {
//		   	  //trigger message
//			     send_exec_msg(0,0,0,0,0,0,(35000+(10+eNB_id)+(20+UE_id)));
//  		   	  	  }
//		    }

		//    sleep(1);


		   // while(st2->EResp_FLAG<NB_eNB_INST)
			   //   {

/*
	    for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){

	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

	  		u2e_t[UE_id][eNB_id]->eNB_id=eNB_id;
	  		u2e_t[UE_id][eNB_id]->UE_id=UE_id;
	  		u2e_t[UE_id][eNB_id]->r_re0=r_re0_d[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->r_im0=r_im0_d[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->r_re=r_re[NB_eNB_INST+UE_id];
	  		u2e_t[UE_id][eNB_id]->r_im=r_im[NB_eNB_INST+UE_id];
	  		u2e_t[UE_id][eNB_id]->s_im=s_im[eNB_id];
	  		u2e_t[UE_id][eNB_id]->s_re=s_re[eNB_id];
	  		u2e_t[UE_id][eNB_id]->eNB2UE=eNB2UE[eNB_id][UE_id];
	  		u2e_t[UE_id][eNB_id]->UE2eNB=UE2eNB[UE_id][eNB_id];
	  		u2e_t[UE_id][eNB_id]->enb_data=enb_data[eNB_id];
	  		u2e_t[UE_id][eNB_id]->ue_data=ue_data[UE_id];
	  		u2e_t[UE_id][eNB_id]->next_slot=&next_slot;
	  		u2e_t[UE_id][eNB_id]->abstraction_flag=&abstraction_flag;
	  		u2e_t[UE_id][eNB_id]->frame_parms=frame_parms;

	  	 //  pthread_create (&thread[eNB_id][UE_id], NULL, do_DL_sig_channel_T,(void*)cthread);
	  	  pthread_cond_init (&count_threshold_cv, NULL);
	  	pthread_create (&cthr_d[UE_id][eNB_id], NULL, do_DL_sig_channel_T,(void*)(u2e_t[UE_id][eNB_id]));
	 // 	pthread_join(cthr_d[UE_id][eNB_id], NULL);

	  //	pthread_join(cthr_d[UE_id][eNB_id], NULL);
	  //	   pthread_join(thread[eNB_id][UE_id], NULL);
	  //  do_DL_sig_channel(eNB_id,UE_id,r_re0,r_im0,r_re[NB_eNB_INST+UE_id],r_im[NB_eNB_INST+UE_id],s_re[eNB_id],s_im[eNB_id],eNB2UE,enb_data, ue_data,next_slot,abstraction_flag,frame_parms);
	  	 //   do_DL_sig_channel(eNB_id,UE_id,r_re0_d[UE_id][eNB_id],r_im0_d[UE_id][eNB_id],r_re[NB_eNB_INST+UE_id],r_im[NB_eNB_INST+UE_id],s_re[eNB_id],s_im[eNB_id],eNB2UE,enb_data, ue_data,next_slot,abstraction_flag,frame_parms);

	    }
	    }
	    */


	    		    for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
	    		   	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {
	    		   	  //trigger message
	    			 //    send_exec_msg(0,0,0,0,0,0,(35000+(10+eNB_id)+(20+UE_id)));
	    		   	  		pthread_cond_signal(&downlink_cond[eNB_id][UE_id]);

	    		   	  	  }
	    		    }

	    		  //  sleep(1);

	//

COT=0;
//pthread_cond_broadcast(&downlink_cond);

printf("Waiting For Conditional Wait\n");


//for(count=0;count<NUM_THREAD_DOWNLINK;count++)
//{
	pthread_cond_wait(&downlink_cond_channel, &downlink_mutex_channel);

//}

//pthread_cond_destroy(&count_threshold_cv);

//for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
//for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {
//pthread_join(cthr_d[UE_id][eNB_id], NULL);
//}
//}



	  	for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
		  	  clean_param(r_re[NB_eNB_INST+UE_id],r_im[NB_eNB_INST+UE_id],frame_parms);

	  	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

	  	 channel_add(r_re[NB_eNB_INST+UE_id],r_im[NB_eNB_INST+UE_id],r_re0_d[UE_id][eNB_id],r_im0_d[UE_id][eNB_id],frame_parms);

	  	  	  }
	  	}


	//  	if(UE_id==NB_UE_INST)
	//  	pthread_join(thread, NULL);
	    //}

	  	 for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){

	  	  	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

	  	  	  	// pthread_join(thread, NULL);
	  	  		  //pthread_join(thread[eNB_id][UE_id], NULL);
	  	  	  }
	  	 }
	  	  	 // }

	    st2->EResp_FLAG=0;
		  count=0;

	    for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++)
	    	if (mac_xface->frame >= (UE_id * 10)) {	// activate UE only after 10*UE_id frames so that different UEs turn on separately

	    	    send_exec_msg(next_slot,last_slot,mac_xface->frame,mac_xface->frame,slot,0,38820+UE_id);

	    	    	printf("Waiting for Exec Msg Complete \n");
	    			//n = openairIoSyncCreateThread_2(&this->m_io_sync);
	    	  	//    n=trig_wait((void*)&this->m_io_sync);

	    }
	    	else{
	    		st2->EResp_FLAG=st2->EResp_FLAG+1;
	    		count++;
	    	}

			while(count<NB_UE_INST){
	    		n=trig_wait((void*)&this->m_io_sync);
	    		count++;
	    		}

	    //while(st2->EResp_FLAG<NB_UE_INST)
	        	// {
	//	    n = select(this->m_io_sync.hfd, &read_ibits, NULL, NULL,NULL);
	//		FD_ISSET(this->m_io_sync.io_sync_entries->fd_read, &read_ibits);

	        	// printf("..");
	        //	 }
	}
	else if(dir_flag==20)
	{
		  st2->EResp_FLAG=0;
	 	  count=0;

		for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++)
			if (mac_xface->frame >= (UE_id * 10)) {	// activate UE only after 10*UE_id frames so that different UEs turn on separately


		    	      	    send_exec_msg(next_slot,last_slot,mac_xface->frame,mac_xface->frame,slot,0,38820+UE_id);

		    	//      	    	printf("Waiting for Exec Msg Complete \n");
		    		//		n = openairIoSyncCreateThread_2(&this->m_io_sync);
		    		  	 //   n=trig_wait((void*)&this->m_io_sync);


		    	        }
			else{
			    		st2->EResp_FLAG=st2->EResp_FLAG+1;
			    		count++;
			}
		while(count<NB_UE_INST){
		    		n=trig_wait((void*)&this->m_io_sync);
		    		count++;
		    		}

		//while(st2->EResp_FLAG<NB_UE_INST)
			    	 //    	    	 {
	//	    n = select(this->m_io_sync.hfd, &read_ibits, NULL, NULL,NULL);
	//		FD_ISSET(this->m_io_sync.io_sync_entries->fd_read, &read_ibits);

			    	      	    	// printf("..");
			    	     // 	    	 }

			 	 // do_UL_sig_channel2(1,0,r_re0,r_im0,r_re,r_im,s_re,s_im,UE2eNB,next_slot,abstraction_flag,frame_parms);
			 	//  do_UL_sig_channel(r_re0,r_im0,r_re,r_im,s_re,s_im,UE2eNB,next_slot,abstraction_flag,frame_parms);


/*
		       	  for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {

		          for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){

						e2u_t[eNB_id][UE_id]->eNB_id=eNB_id;
		        		e2u_t[eNB_id][UE_id]->UE_id=UE_id;
		        		e2u_t[eNB_id][UE_id]->r_re=r_re[eNB_id];
		        		e2u_t[eNB_id][UE_id]->r_im=r_im[eNB_id];
		        		e2u_t[eNB_id][UE_id]->r_re0=r_re0_u[eNB_id][UE_id];
		        		e2u_t[eNB_id][UE_id]->r_im0=r_im0_u[eNB_id][UE_id];
		        		e2u_t[eNB_id][UE_id]->s_im=s_im[NB_eNB_INST+UE_id];
		        		e2u_t[eNB_id][UE_id]->s_re=s_re[NB_eNB_INST+UE_id];
		        		e2u_t[eNB_id][UE_id]->eNB2UE=eNB2UE[eNB_id][UE_id];
		        		e2u_t[eNB_id][UE_id]->UE2eNB=UE2eNB[UE_id][eNB_id];
		        		e2u_t[eNB_id][UE_id]->enb_data=enb_data[eNB_id];
		        		e2u_t[eNB_id][UE_id]->ue_data=ue_data[UE_id];
		        		e2u_t[eNB_id][UE_id]->next_slot=&next_slot;
		        		e2u_t[eNB_id][UE_id]->abstraction_flag=&abstraction_flag;
		        		e2u_t[eNB_id][UE_id]->frame_parms=frame_parms;

			        	 pthread_create (&cthr_u[eNB_id][UE_id], NULL, do_UL_sig_channel_T,(void*)e2u_t[eNB_id][UE_id]);
		        	 //  pthread_create (&thread[eNB_id][UE_id], NULL, do_UL_sig_channel_T,(void*)cthread);
		        	//   do_UL_sig_channel(eNB_id,UE_id,r_re0,r_im0,r_re[eNB_id],r_im[eNB_id],s_re[NB_eNB_INST+UE_id],s_im[NB_eNB_INST+UE_id],UE2eNB,next_slot,abstraction_flag,frame_parms);

	// do_UL_sig_channel(eNB_id,UE_id,r_re0_u[eNB_id][UE_id],r_im0_u[eNB_id][UE_id],r_re[eNB_id],r_im[eNB_id],s_re[NB_eNB_INST+UE_id],s_im[NB_eNB_INST+UE_id],UE2eNB,next_slot,abstraction_flag,frame_parms);

		          }

		             }
*/



		       	for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {
		       	for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
		 //      	pthread_join(thread[eNB_id][UE_id], NULL);
	      //     	     pthread_join(cthr_u[eNB_id][UE_id], NULL);
		       		pthread_cond_signal(&uplink_cond[UE_id][eNB_id]);

		       	}
		       	}


			COT_U=0;
			//pthread_cond_broadcast(&uplink_cond);

			printf("Waiting For Conditional Wait\n");

			//for(count=0;count<NUM_THREAD_UPLINK;count++)
			//{
				pthread_cond_wait(&uplink_cond_channel, &uplink_mutex_channel);

//			}

		      	for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {
		           	  clean_param(r_re[eNB_id],r_im[eNB_id],frame_parms);
		      		for (UE_id = oai_emulation.info.first_ue_local; UE_id < (oai_emulation.info.first_ue_local + oai_emulation.info.nb_ue_local); UE_id++){
		    				    channel_add(r_re[eNB_id],r_im[eNB_id],r_re0_u[eNB_id][UE_id],r_im0_u[eNB_id][UE_id],frame_parms);

		    		       	}
		    		       	}




		       		     //     }

	 	    	  st2->EResp_FLAG=0;
	 	    	  count=0;
			 	 for (eNB_id=oai_emulation.info.first_enb_local;eNB_id<(oai_emulation.info.first_enb_local+oai_emulation.info.nb_enb_local);eNB_id++) {


			 	    	  send_exec_msg(next_slot,last_slot,mac_xface->frame,mac_xface->frame,slot,0,38810+eNB_id);

			 		//		n = openairIoSyncCreateThread_2(&this->m_io_sync);
			 		  	//    n=trig_wait((void*)&this->m_io_sync);

			 	//    	  printf("Waiting for Exec Msg Complete \n");

			 	          } // for loop

			 	while(count<NB_eNB_INST){
			 	    		n=trig_wait((void*)&this->m_io_sync);
			 	    		count++;
			 	    		}

			 //	 while(st2->EResp_FLAG<NB_eNB_INST)
			 			 	    //	      {
	//				    n = select(this->m_io_sync.hfd, &read_ibits, NULL, NULL,NULL);
	//					FD_ISSET(this->m_io_sync.io_sync_entries->fd_read, &read_ibits);

			 			 	    //	    	 // printf("..");
			 		//	 	    	      }

	}


	      if ((last_slot == 1) && (mac_xface->frame == 0)
		  && (abstraction_flag == 0) && (oai_emulation.info.n_frames == 1)) {

	//	write_output ("dlchan0.m", "dlch0",
	//		      &(PHY_vars_UE_g[0]->lte_ue_common_vars.dl_ch_estimates[0][0][0]),
	//		      (6 * (PHY_vars_UE_g[0]->lte_frame_parms.ofdm_symbol_size)), 1, 1);
	//	write_output ("dlchan1.m", "dlch1",
	//		      &(PHY_vars_UE_g[0]->lte_ue_common_vars.dl_ch_estimates[1][0][0]),
	//		      (6 * (PHY_vars_UE_g[0]->lte_frame_parms.ofdm_symbol_size)), 1, 1);
	//	write_output ("dlchan2.m", "dlch2",
	//		      &(PHY_vars_UE_g[0]->lte_ue_common_vars.dl_ch_estimates[2][0][0]),
	//		      (6 * (PHY_vars_UE_g[0]->lte_frame_parms.ofdm_symbol_size)), 1, 1);
	//	write_output ("pbch_rxF_comp0.m", "pbch_comp0",
	//		      PHY_vars_UE_g[0]->lte_ue_pbch_vars[0]->rxdataF_comp[0], 6 * 12 * 4, 1, 1);
	//	write_output ("pbch_rxF_llr.m", "pbch_llr",
	//		      PHY_vars_UE_g[0]->lte_ue_pbch_vars[0]->llr, (frame_parms->Ncp == 0) ? 1920 : 1728, 1, 4);
	      }
	      /*
	         if ((last_slot==1) && (mac_xface->frame==1)) {
	         write_output("dlsch_rxF_comp0.m","dlsch0_rxF_comp0",PHY_vars_UE->lte_ue_dlsch_vars[eNB_id]->rxdataF_comp[0],300*(-(PHY_vars_UE->lte_frame_parms.Ncp*2)+14),1,1);
	         write_output("pdcch_rxF_comp0.m","pdcch0_rxF_comp0",PHY_vars_UE->lte_ue_pdcch_vars[eNB_id]->rxdataF_comp[0],4*300,1,1);
	         }
	       */
	      if (next_slot %2 == 0){
		clock_gettime (CLOCK_REALTIME, &time_spec);
		time_last = time_now;
		time_now = (unsigned long) time_spec.tv_nsec;
		td = (int) (time_now - time_last);
		if (td>0) {
		  td_avg = (int)(((K*(long)td) + (((1<<3)-K)*((long)td_avg)))>>3); // in us
		  LOG_I(EMU,"sleep frame %d, average time difference %ldns, CURRENT TIME DIFF %dus, avgerage difference from the target %dus\n",
			mac_xface->frame, td_avg, td/1000,(td_avg-TARGET_SF_TIME_NS)/1000);
		}
		if (td_avg<(TARGET_SF_TIME_NS - SF_DEVIATION_OFFSET_NS)){
		  sleep_time_us += SLEEP_STEP_US;
		}
		else if (td_avg > (TARGET_SF_TIME_NS + SF_DEVIATION_OFFSET_NS)) {
		  sleep_time_us-= SLEEP_STEP_US;
		}
	      }// end if next_slot%2

	    } //if channel_flag
	    }				//end of slot

	    //if ((mac_xface->frame==1)&&(abstraction_flag==0)) {
	    if ((abstraction_flag==0)) {
	//      write_output("UEtxsig0.m","txs0", PHY_vars_UE_g[0]->lte_ue_common_vars.txdata[0],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
	//      write_output("eNBtxsig0.m","txs0", PHY_vars_eNB_g[0]->lte_eNB_common_vars.txdata[0][0],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
	//      write_output("UErxsig0.m","rxs0", PHY_vars_UE_g[0]->lte_ue_common_vars.rxdata[0],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
	//      write_output("eNBrxsig0.m","rxs0", PHY_vars_eNB_g[0]->lte_eNB_common_vars.rxdata[0][0],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
	    }

	#ifdef XFORMS
	    for (UE_id = 0; UE_id < NB_UE_INST; UE_id++)
	      for (eNB_id = 0; eNB_id < NB_eNB_INST; eNB_id++) {
		do_forms (form[eNB_id][UE_id],
			  PHY_vars_UE_g[UE_id]->lte_ue_dlsch_vars,
			  PHY_vars_eNB_g[eNB_id]->lte_eNB_ulsch_vars,
			  eNB2UE[eNB_id][UE_id]->ch, eNB2UE[eNB_id][UE_id]->channel_length);
	      }
	#endif
	    // calibrate at the end of each frame if there is some time  left
	    if((sleep_time_us > 0)&& (ethernet_flag ==0)){
	      LOG_I(EMU,"Go to sleep for %dus\n",sleep_time_us);
	      usleep(sleep_time_us);
	      sleep_time_us=0; // reset the timer, could be done per n SF
	    }
	  }	//end of frame



	  // relase all rx state
	  if (ethernet_flag == 1) {
	    emu_transport_release ();
	  }

	  if (abstraction_flag == 0) {
	    /*
	       #ifdef IFFT_FPGA
	       free(txdataF2[0]);
	       free(txdataF2[1]);
	       free(txdataF2);
	       free(txdata[0]);
	       free(txdata[1]);
	       free(txdata);
	       #endif
	     */

	    for (i = 0; i < 2; i++) {
	      free (s_re[i]);
	      free (s_im[i]);
	      free (r_re[i]);
	      free (r_im[i]);
	    }
	    free (s_re);
	    free (s_im);
	    free (r_re);
	    free (r_im);

	    lte_sync_time_free ();
	  }

	  // added for PHY abstraction
	  if (oai_emulation.info.ocm_enabled == 1) {
	    for (eNB_id = 0; eNB_id < NUMBER_OF_eNB_MAX; eNB_id++)
	      free(enb_data[eNB_id]);

	    for (UE_id = 0; UE_id < NUMBER_OF_UE_MAX; UE_id++)
	      free(ue_data[UE_id]);
	  } //End of PHY abstraction changes

	#ifndef NAS_NETLINK
	  for(UE_id=0;UE_id<NB_UE_INST;UE_id++)
	    fclose (UE_stats[UE_id]);
	  fclose (eNB_stats);
	#endif

	 destroyMat(ShaF,map1, map2);

	  return(0);
	}

	//dummy function
	Exec_Inst()
	{

	}
	// could be per mobility type : void update_node_vector(int mobility_type, double cur_time) ;
