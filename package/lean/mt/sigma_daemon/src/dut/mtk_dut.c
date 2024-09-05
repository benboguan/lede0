/****************************************************************************
*
* Copyright (c) 2016 Wi-Fi Alliance
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*
*****************************************************************************/

/*
 * File: wfa_dut.c - The main program for DUT agent.
 *       This is the top level of traffic control. It initializes a local TCP
 *       socket for command and control link and waits for a connect request
 *       from a Control Agent. Once the the connection is established, it
 *       will process the commands from the Control Agent. For details, please
 *       reference the architecture documents.
 *
 *       ======================================================================
 *
 *       This will be a main baseline code for the new ca/dut user space program
 *       key modification:
 *       1. change sockfd structure defined in both sock.c and sock.h files
 *       2. get more paramters for setting up TCP connect with UCC, mimic wfa_ca
 *       3. using select to determine which traffic is in the flow
 *
 *       Managed and maintained by MUS_CSD_CSD4_SD17 Yanfang Liu
 */

#include <stdio.h>
#include <stdlib.h>
#include "wfa_tg.h"
#include <pthread.h>
#include <time.h>
#include "wfa_debug.h"
#include "wfa_main.h"
#include "wfa_types.h"
#include "mtk_dut.h"
#include "wfa_agtctrl.h"
#include <signal.h>
#include <time.h>
#include "mtk_ap.h"
#include "mtk_parse.h"
#include "mtk_dict.h"
#include "mtk_dut.h"
#include "wfa_portall.h"
#include "wfa_debug.h"
#include "wfa_main.h"
#include "wfa_types.h"
#include "wfa_sock.h"
#include "wfa_tlv.h"
#include "wfa_miscs.h"
#include "wfa_agt.h"
#include "wfa_rsp.h"
#include "wfa_wmmps.h"
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <sys/select.h>
#include <ctype.h>
#include "wfa_cmds.h"
#include "wfa_ca.h"
#include "wfa_agtctrl.h"

#define WFA_ENV_AGENT_IPADDR "WFA_ENV_AGENT_IPADDR"

extern int xcCmdProcGetVersion(unsigned char *parms);
extern dutCommandRespFuncPtr wfaCmdRespProcFuncTbl[];
extern typeNameStr_t nameStr[];
extern char gRespStr[];
extern char Device_Ver[];
extern const char *CA_Ver;

int gSock = -1, tmsockfd, gCaSockfd = -1, xcSockfd, btSockfd;
int gtgSend, gtgRecv, gtgTransac;
//char gnetIf[32] = "any";
tgStream_t    *theStreams;
long          itimeout = 0;
volatile sig_atomic_t    isExit;
unsigned short wfa_defined_debug = WFA_DEBUG_ERR | WFA_DEBUG_WARNING | WFA_DEBUG_INFO;
//unsigned short dfd_lvl = WFA_DEBUG_DEFAULT | WFA_DEBUG_ERR | WFA_DEBUG_INFO;


//
/* Global flags for synchronizing the TG functions */
int        gtimeOut = 0;        /* timeout value for select call in usec */

#ifdef WFA_WMM_PS_EXT
extern BOOL gtgWmmPS;
extern unsigned long psTxMsg[512];
extern unsigned long psRxMsg[512];
extern wfaWmmPS_t wmmps_info;
extern tgWMM_t    wmmps_mutex_info;
extern int  psSockfd;
extern struct apts_msg *apts_msgs;

extern void BUILD_APTS_MSG(int msg, unsigned long *txbuf);
extern int wfaWmmPowerSaveProcess(int sockfd);
extern void wfaSetDUTPwrMgmt(int);
extern void wfaTGSetPrio(int, int);
#endif /* WFA_WMM_PS_EXT */

extern int adj_latency;           /* adjust sleep time due to latency */
char        gnetIf[WFA_BUFF_32];        /* specify the interface to use */
char        gIPaddr[20];

//extern uint8_t   *trafficBuf, *respBuf;
uint8_t *trafficBuf=NULL, *respBuf=NULL;
/* stream table */
extern tgStream_t gStreams[];         /* streams' buffers             */

/* the agent local Socket, Agent Control socket and baseline test socket*/
extern int btSockfd;


/* the WMM traffic streams socket fds - Socket Handler table */
extern int tgSockfds[];

//extern     xcCommandFuncPtr gWfaCmdFuncTbl[]; /* command process functions */
extern     char gCmdStr[];
extern     tgStream_t *findStreamProfile(int);
extern     int clock_drift_ps;

dutCmdResponse_t gGenericResp;

/* Debug message flags */
//unsigned short wfa_defined_debug = WFA_DEBUG_ERR | WFA_DEBUG_WARNING | WFA_DEBUG_INFO;
unsigned short dfd_lvl = WFA_DEBUG_DEFAULT | WFA_DEBUG_ERR | WFA_DEBUG_INFO;

/*
 * Thread Synchronize flags
 */
tgWMM_t wmm_thr[WFA_THREADS_NUM];

extern void *wfa_wmm_thread(void *thr_param);
extern void *wfa_wmmps_thread();

extern double gtgPktRTDelay;

int gxcSockfd = -1;
// define a new sockfd structure for select
int gxc2Sockfd = -1;

#define DEBUG 0

extern int wfa_estimate_timer_latency();
extern void wfa_dut_init(uint8_t **tBuf, uint8_t **rBuf, uint8_t **paBuf, uint8_t **cBuf, struct timeval **timerp);

/*
 * start testing handler part, define the handlers list
 * handler will map the whole structure with its handlers
 */
// 3 dictionary data structures are used for changing dat files
volatile dict_t global_interface1_dat_dict;
volatile dict_t global_interface2_dat_dict;
volatile dict_t global_interface3_dat_dict;
volatile dict_t global_key_dict;

static int get_cmd_tag (char* capi_name)
{
    int cmd_tag = 0;
    while(dut_tbl[cmd_tag].type != -1) {
        if (strcmp(dut_tbl[cmd_tag].name, capi_name) == 0) {
            //printf("cmd is found\n");
            return cmd_tag;
        }
        cmd_tag++;
    }
    return 0;
}

static int wfa_thread_init(void)
{
    tgThrData_t tdata[WFA_THREADS_NUM];
    pthread_attr_t ptAttr;
    int ptPolicy;
    struct sched_param ptSchedParam;
    int i;

    pthread_attr_init(&ptAttr);
    ptSchedParam.sched_priority = 10;
    pthread_attr_setschedparam(&ptAttr, &ptSchedParam);
    pthread_attr_getschedpolicy(&ptAttr, &ptPolicy);
    pthread_attr_setschedpolicy(&ptAttr, SCHED_RR);
    pthread_attr_getschedpolicy(&ptAttr, &ptPolicy);
    /*
     * Create multiple threads for WMM Stream processing.
     */
    printf("WFA_THREADS_NUM: %d\n", WFA_THREADS_NUM);

    for (i = 0; i< WFA_THREADS_NUM; i++) {
        tdata[i].tid = i;
        pthread_mutex_init(&wmm_thr[i].thr_flag_mutex, NULL);
        pthread_cond_init(&wmm_thr[i].thr_flag_cond, NULL);
        wmm_thr[i].thr_id = pthread_create(&wmm_thr[i].thr,
                                           &ptAttr, wfa_wmm_thread, &tdata[i]);
    }
    return 0;
}

static void sigintHandler(int sig_num, siginfo_t *siginfo, void *context) 
{ 
    /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
    isExit = 0;
    printf ("Sig_num: %d, Sending PID: %ld, UID: %ld\n",sig_num,
			(long)siginfo->si_pid, (long)siginfo->si_uid);
} 

/*
 * This daemon can support 2G/5G interface
 * Format : ./wfa_dut <interface_recv> <CA_recv_port> <UCC_recv_port> <interface1> <interface2>
 *
 *
 */

int main(int argc, char **argv)
{
    uint8_t sock_recv_buf[WFA_BUFF_1K];
    uint8_t sock_resp_buf[WFA_BUFF_512];

    int	      nfds, maxfdn1 = -1, nbytes = 0, /*isExit = 1,*/ i = 0;
    fd_set    sockSet;         /* Set of socket descriptors for select()     */
    uint8_t      *xcCmdBuf=NULL, *parmsVal=NULL;
    struct timeval *toutvalp=NULL, *tovalp; /* Timeout for select()           */
    struct sockfds fds;
    retType_t status;
 
    //DPRINT_ERR(WFA_ERR, "Build Time %s %s\n",__DATE__,__TIME__);
    if (argc < 4) {
        if (argc == 2) {
             if ((strcasecmp(argv[1], "-v") == 0) ||(strcasecmp(argv[1], "--version") == 0)) {
                device_get_ver();
                printf("CA Version: %s\n", CA_Ver);
                printf("Device Version: %s", Device_Ver);
                exit(1);
            }
        }
        //printf("Usage:  %s [-v|--version]\n", argv[0]);
        printf( "Usage:  %s <ap/sta> <UCC cmd interface> <UCC cmd port> [-p <program>]\n", argv[0]);
        printf("                [-m <mode>] [-d <delay1>] [-z <delay2>]\n", argv[0]);
        printf("Example:  %s ap br-lan 9000 -p 6G\n", argv[0]);
        printf("<program>: 6G\n");
        printf("<mode>: efuse|flash\n");
        printf("<delay1>: delay time in second between interface down/up\n");
        printf("<delay2>: delay time in second after interface up\n");
        exit(1);
    }
 
    device_get_ver();

    char interface1[8];
    char interface2[8];
    wifi_mode def_mode = WIFI_5G;
    char daemon_dut_mode[8];
    int wifi_intf_fd;
    int c, pre_delay = 5, post_delay = 0;
    char program[20];
    char mode[20];
    char log_file_name[30];
    memset(program, 0, sizeof(program));
    memset(mode, 0, sizeof(mode));
    memset(log_file_name, 0, sizeof(log_file_name));

    for (;;) {
        c = getopt(argc-3, &argv[3], "d:l:m:p:z:");
        if (c < 0)
            break;
        switch (c) {
            case 'd':
                pre_delay = atoi(optarg);
                break;
            case 'm':
                strcpy(mode, optarg);
                break;
            case 'l':
                strcpy(log_file_name, optarg);
                break;
            case 'p':
                strcpy(program, optarg);
                break;
            case 'z':
                post_delay = atoi(optarg);
                break;
            default:
                break;
        }
    }

    if (log_file_name) {
        FILE *logfile;
        int fd;
        char log_full_name[40] = "/tmp/log/";

        strcat(log_full_name, log_file_name);
        logfile = fopen(log_full_name, "w+");
        if(logfile != NULL) {
            fd = fileno(logfile);
            DPRINT_INFO(WFA_OUT,"redirecting the output to %s\n", log_full_name);
            dup2(fd,1);
            dup2(fd,2);
            fclose(logfile);
        } else {
            DPRINT_ERR(WFA_ERR, "Cant open the log file continuing without redirecting\n");
        }
    }

    if (isString(argv[2]) == WFA_FAILURE) {
        DPRINT_ERR(WFA_ERR, "Incorrect network interface\n");
        exit(1);
    }
    strncpy(gnetIf, argv[2], 31);
    if (getIPaddr(gnetIf, gIPaddr) != WFA_SUCCESS) {
        DPRINT_ERR(WFA_ERR, "Incorrect IP Addr\n");
        exit(1);
    }
    printf("%s IP Addr is %s\n", gnetIf, gIPaddr);
   
    if (isNumber(argv[3]) == WFA_FAILURE) {
        DPRINT_ERR(WFA_ERR, "incorrect port number\n");
        exit(1);
    }

    printf("\nargc: %d\n", argc);
    strcpy(daemon_dut_mode, argv[1]);
    printf("daemon is running under %s mode\n", daemon_dut_mode);
    int ucc_port = atoi(argv[3]);

    if ((wifi_intf_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        DPRINT_ERR(WFA_ERR, "create WIFI Interface socket() failed");
        return WFA_FAILURE;
    }
  
    global_key_dict = init_capi_key_dict();
    printf("CAPI cmd to profile key mapping table loaded.\n");

    mtk_ap_buf_t mtk_ap_buf;
    memset(&mtk_ap_buf, 0, sizeof(mtk_ap_buf_t));
    if (program) {
        strcpy(mtk_ap_buf.cmd_cfg.program, program);
        DPRINT_INFO(WFA_OUT, "Program is %s!\n", program);
    } else {
        strcpy(mtk_ap_buf.cmd_cfg.program, "General");
    }
    if ((strcasecmp(mode, "efuse") == 0) || (strcasecmp(mode, "flash") == 0)) {
        strcpy(mtk_ap_buf.cmd_cfg.mode, mode);
        DPRINT_INFO(WFA_OUT, "E2pAccessMode is %s!\n", mode);
    }
    mtk_ap_buf.cmd_cfg.intf_rst_delay= pre_delay;
    mtk_ap_buf.cmd_cfg.post_intf_rst_delay = post_delay;
    
    if (init_profile_name(&mtk_ap_buf) != WFA_SUCCESS) {
        dict_destroy(global_key_dict);
        return 0;
    }
    if (wifi_enum_devices(wifi_intf_fd, &fillup_intf, &mtk_ap_buf, 1)!= WFA_SUCCESS) {
        dict_destroy(global_key_dict);
        return 0;
    }
    backup_profile(&mtk_ap_buf);
    //turn_all_interface_down_up(&mtk_ap_buf);
    close(wifi_intf_fd);
    if (ap_init(&mtk_ap_buf) != WFA_SUCCESS) {
        dict_destroy(global_key_dict);
        return 0;
    }
    // init ioctl socket
    mtk_ap_buf.ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (mtk_ap_buf.ioctl_sock < 0) {
        dict_destroy(global_key_dict);
        return 0;
    }
    printf("done with ap_init\n");

    printf("open TCP server based on argv %d.\n", ucc_port);
    adj_latency = wfa_estimate_timer_latency() + 4000; /* four more mini */

    if (adj_latency > 500000) {
        printf("****************** WARNING  **********************\n");
        printf("!!!THE SLEEP TIMER LATENCY IS TOO HIGH!!!!!!!!!!!!\n");
        printf("**************************************************\n");

        /* Just set it to  500 mini seconds */
        adj_latency = 500000;
    }

    wfa_dut_init(&trafficBuf, &respBuf, &parmsVal, &xcCmdBuf, &toutvalp);
    printf("done with wfa_dut init\n");

    /* allocate the traffic stream table */
    /* 4create listening TCP socket */

    int   gagtSockfd = -1;

    gagtSockfd = wfaCreateTCPServSock(ucc_port);
    // adding ucc tcp server
    // adding more condition
    if (gagtSockfd == -1) {
        DPRINT_ERR(WFA_ERR, "Failed to open socket\n");
        exit(1);
    }
    DPRINT_INFO(WFA_OUT, "Open socket successful!\n");

    wfa_thread_init();


    for (i = 0; i < WFA_MAX_TRAFFIC_STREAMS; i++)
        tgSockfds[i] = -1;

#ifdef WFA_WMM_PS_EXT
    /* WMMPS thread   */
    ret = pthread_mutex_init(&wmmps_mutex_info.thr_flag_mutex,NULL);
    if (ret != 0) {
        DPRINT_INFO(WFA_OUT, "WMMPS pthread_mutex_init faile\n");
    }
    ret = pthread_cond_init(&wmmps_mutex_info.thr_flag_cond,NULL);
    if (ret != 0) {
        DPRINT_INFO(WFA_OUT, "WMMPS pthread_cond_init faile\n");
    }
    wmmps_mutex_info.thr_id=pthread_create(&wmmps_mutex_info.thr,NULL /*&ptAttr*/,wfa_wmmps_thread,(void*)&wmmps_mutex_info.thr_id);// calls up the wmmps-thread
#endif

    maxfdn1 = gagtSockfd + 1;
    // adding for gagt2sSockfd
    //maxfdn1 = gagt2Sockfd + 1;
    // end of pretest
   {
        struct sigaction action;

        memset(&action, '\0', sizeof(action));
        action.sa_sigaction = &sigintHandler;
        action.sa_flags = SA_SIGINFO;
        if(sigaction(SIGINT, &action, NULL) < 0) {
		printf ("sigaction\n");
		return 1;
	 }
        isExit = 1;
    }

    printf("== stating while loop == \n");
    int ucc_sock_fd = -1;
    while (isExit) {
        fds.agtfd = &gagtSockfd;
        fds.cafd = &gxcSockfd;
        // add ucc
        fds.uccfd = &ucc_sock_fd;
        fds.tgfd = &btSockfd;
        fds.wmmfds = tgSockfds;
#ifdef WFA_WMM_PS_EXT
        fds.psfd = &psSockfd;
#endif

        wfaSetSockFiDesc(&sockSet, &maxfdn1, &fds); // reset fds

        /*
         * The timer will be set for transaction traffic if no echo is back
         * The timeout from the select call force to send a new packet
         */
        tovalp = NULL;
        if (gtimeOut != 0) {
            /* timeout is set to usec */
            tovalp = wfaSetTimer(0, gtimeOut*1000, toutvalp);
        }
        nfds = 0;
        if ( (nfds = select(maxfdn1, &sockSet, NULL, NULL, tovalp)) < 0) {
            if (errno == EINTR)
                continue;           /* back to for() */
            else
                DPRINT_WARNING(WFA_WNG, "Warning: select()-%i", errno);
        }

        if(nfds == 0) {
#if 0 //def WFA_WMM_PS_EXT
            /*
             * For WMM-Power Save
             * periodically send HELLO to Console for initial setup.
             */
            if (gtgWmmPS != 0 && psSockfd != -1) {
                wfaSetDUTPwrMgmt(0);
                wfaTGSetPrio(psSockfd, 0);
                BUILD_APTS_MSG(APTS_HELLO, psTxMsg);
                wfaTrafficSendTo(psSockfd, (char *)psTxMsg, sizeof(psTxMsg), (struct sockaddr *) &wmmps_info.psToAddr);

                wmmps_info.sta_state = 0;
                wmmps_info.wait_state = WFA_WAIT_STAUT_00;
                continue;
            }
#endif /* WFA_WMM_PS_EXT */
        }
        if (FD_ISSET(gagtSockfd, &sockSet)) {
            /* Incoming connection request */
            ucc_sock_fd = wfaAcceptTCPConn(gagtSockfd);
            printf("\n\n Accepted TCP connection from UCC \n\n");
            if (ucc_sock_fd == -1) {
                DPRINT_ERR(WFA_ERR, "Failed to open control link socket\n");
                exit(1);
            }
        }
        /*
         * event from ucc starts
        */
        if (ucc_sock_fd >= 0 && FD_ISSET(ucc_sock_fd, &sockSet)) {
            printf("\nReceived from UCC sock\n");
            memset(sock_recv_buf, 0, WFA_BUFF_1K);
            nbytes = wfaCtrlRecv(ucc_sock_fd, sock_recv_buf); // receive string
            if (nbytes <= 0 ) {
                /* errors at the port, close it */
                shutdown(ucc_sock_fd, SHUT_WR);
                close(ucc_sock_fd);
                ucc_sock_fd = -1;
            } else {
                // maping  part
                char *capi_str = NULL;
                int cmd_len = WFA_BUFF_1K, resp_len = WFA_BUFF_1K;
                uint8_t capi_buf[WFA_BUFF_1K];
                //uint8_t resp_buf[WFA_BUFF_1K];
                char capi_name[WFA_BUFF_64];
                int cmd_tag = -1;
                int slen;
                char *token;

                /*
                 * The modification starts from here,
                */
                printf("\n\n***** START CAPI HANDLER *****\n");

                DPRINT_INFO(WFA_OUT, "message %s", sock_recv_buf);
                slen = (int )strlen((char *)sock_recv_buf);
                if (slen >=3) {
                    //DPRINT_INFO(WFA_OUT, "first %x last %x last-1  %x last-2 %x last-3 %x\n",sock_recv_buf[0], sock_recv_buf[slen], sock_recv_buf[slen-1], sock_recv_buf[slen-2], sock_recv_buf[slen-3]);
                } else 
                    continue;

                sock_recv_buf[slen-3] = '\0';			

                token = strtok_r((char *) sock_recv_buf, ",", &capi_str);
                if (token == NULL) {
                    	DPRINT_INFO(WFA_OUT, "not a CAPI command!\n");
                    	continue;
                } 
                //memcpy(capi_name, strtok_r((char *) sock_recv_buf, ",", &capi_str), 32);
                memcpy(capi_name, token, 32);
                //printf("CAPI Command Name: %s\n\n", capi_name);
                cmd_tag = get_cmd_tag(capi_name); // parser function to get the tag
                if (cmd_tag == 0) {
                    DPRINT_INFO(WFA_OUT, "Not a CAPI command!\n");
                    continue;
                }
                // call the function to parse the data into wanted format  only in UCC mode
                DPRINT_INFO(WFA_OUT, "===== begin of parse function =====\n");
                dut_tbl[cmd_tag].cmd_parse(capi_str, capi_buf, &cmd_len);
                DPRINT_INFO(WFA_OUT, "===== end of parse function =====\n");
                // add this for v 9.2
                memset(sock_resp_buf, 0, WFA_BUFF_512);
                sprintf((char*) sock_resp_buf, "status,RUNNING\r\n");
                wfaCtrlSend(ucc_sock_fd, sock_resp_buf, ((int) strlen((char *)sock_resp_buf)));

                status = mtk_ap_exec(&mtk_ap_buf, (capi_data_t *) capi_buf,
                            sock_resp_buf, cmd_len, &resp_len, cmd_tag);
                printf("===== end of configuration setup function =====\n");

                // start the resp function
                printf("===== begin of resp function =====\n");
                dut_tbl[cmd_tag].cmd_resp(sock_resp_buf, status);
                printf("===== end of resp function =====\n");

                if (wfaCtrlSend(ucc_sock_fd, sock_resp_buf, ((int) strlen((char *)sock_resp_buf))) == -1 ) {
                    printf("Failed to send to UCC\n");
                } else {
                    printf("RESP BUF Sent: %s\n", sock_resp_buf);
                }
                printf("***** CAPI RESP sent to UCC, END of HANDLER *****\n\n\n");

            }
        }
#if 0 // def WFA_WMM_PS_EXT
        /*
         * Check if there is from Console
         */
        if(psSockfd != -1 && FD_ISSET(psSockfd, &sockSet)) {
            wfaWmmPowerSaveProcess(psSockfd);
            continue;
        }
#endif /* WFA_WMM_PS_EXT */

    }
    
    restore_profile(&mtk_ap_buf);
    /*
     * necessarily free all mallocs for flat memory real-time systems
     */
    dict_destroy(global_interface1_dat_dict);
    dict_destroy(global_interface2_dat_dict);
    if (mtk_ap_buf.intf_6G.status) {
        dict_destroy(global_interface3_dat_dict);
    }
    dict_destroy(global_key_dict);
    wFREE(trafficBuf);
    wFREE(toutvalp);
    wFREE(respBuf);
    wFREE(xcCmdBuf);
    wFREE(parmsVal);

    /* Close sockets */
    if (mtk_ap_buf.ioctl_sock >= 0)
        close(mtk_ap_buf.ioctl_sock);
    wCLOSE(gagtSockfd);
    wCLOSE(gxcSockfd);
    wCLOSE(gxc2Sockfd);
    wCLOSE(btSockfd);
    wCLOSE(ucc_sock_fd);

    for(i= 0; i< WFA_MAX_TRAFFIC_STREAMS; i++)
    {
        if(tgSockfds[i] != -1)
        {
            wCLOSE(tgSockfds[i]);
            tgSockfds[i] = -1;
        }
    }

    return 0;
}
