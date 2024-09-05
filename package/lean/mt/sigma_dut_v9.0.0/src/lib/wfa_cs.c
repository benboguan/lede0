/****************************************************************************
(c) Copyright 2014 Wi-Fi Alliance.  All Rights Reserved

Permission to use, copy, modify, and/or distribute this software for any purpose with or
without fee is hereby granted, provided that the above copyright notice and this permission
notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************************/

/*
 *   File: wfa_cs.c -- configuration and setup
 *   This file contains all implementation for the dut setup and control
 *   functions, such as network interfaces, ip address and wireless specific
 *   setup with its supplicant.
 *
 *   The current implementation is to show how these functions
 *   should be defined in order to support the Agent Control/Test Manager
 *   control commands. To simplify the current work and avoid any GPL licenses,
 *   the functions mostly invoke shell commands by calling linux system call,
 *   system("<commands>").
 *
 *   It depends on the differnt device and platform, vendors can choice their
 *   own ways to interact its systems, supplicants and process these commands
 *   such as using the native APIs.
 *
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <poll.h>

#include "wfa_portall.h"
#include "wfa_debug.h"
#include "wfa_ver.h"
#include "wfa_main.h"
#include "wfa_types.h"
#include "wfa_ca.h"
#include "wfa_tlv.h"
#include "wfa_sock.h"
#include "wfa_tg.h"
#include "wfa_cmds.h"
#include "wfa_rsp.h"
#include "wfa_utils.h"
#ifdef WFA_WMM_PS_EXT
#include "wfa_wmmps.h"
#endif
#ifdef MTK_PRIVATE_IOCTL
#include "wfa_mtk.h"
#endif /* MTK_PRIVATE_IOCTL */

#define CERTIFICATES_PATH    "/etc/wpa_supplicant"

/* Some device may only support UDP ECHO, activate this line */
//#define WFA_PING_UDP_ECHO_ONLY 1

#define WFA_ENABLED 1

extern unsigned short wfa_defined_debug;

extern char gnetIf[];

#ifdef TGAC_DAEMON
extern char* wireless_intf;
extern char dut_mac[];
#endif /* TGAC_DAEMON */

typedef struct caHeCfgSet
{
	char ssid[WFA_SSID_NAME_LEN];
} caHeCfgSet_t;
caHeCfgSet_t gHeCfgSet_t;

int wfaExecuteCLI(char *CLI);

/* Since the two definitions are used all over the CA function */
char gCmdStr[WFA_CMD_STR_SZ];
dutCmdResponse_t gGenericResp;
int wfaTGSetPrio(int sockfd, int tgClass);
void create_apts_msg(int msg, unsigned int txbuf[],int id);

int sret = 0;

extern char e2eResults[];

FILE *e2efp = NULL;
int chk_ret_status()
{
    char *ret = getenv(WFA_RET_ENV);

    if(*ret == '1')
        return WFA_SUCCESS;
    else
        return WFA_FAILURE;
}

#if 1
int SetTxPPDUEnable(char *intf, BOOL en)
{
    DPRINT_INFO(WFA_OUT, " SuTxPPDU = %s\n", en ? "enable" : "disable");
    sprintf(gCmdStr, "iwpriv %s set disable_contention_tx=%d",
                      intf, (en == TRUE) ? 0:1);
    DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
    sret = system(gCmdStr);

    return 0;
}
#endif

#if 1
int SetManualTxopDur(char *intf, BOOL en)
{
	static int count = 0;

	if(en == TRUE && count == 1)
	{
		sprintf(gCmdStr, "iwpriv %s mac 830AB1FC=400000,830ab200=7D0", intf);
		count++;
	}
	else if(en == TRUE)
	{
		count++;
	}
	else
	{
		count = 0;
		sprintf(gCmdStr, "iwpriv %s mac 830AB1FC=0,830ab200=0", intf);
	}
    DPRINT_INFO(WFA_OUT, "scmd = %s en=%d count=%d\n", gCmdStr,en,count);
    sret = system(gCmdStr);

    return 0;
}
#endif

#if 1
int SetAggLimit(char *intf)
{
    sprintf(gCmdStr, "iwpriv %s mac 820E2048=0a0a0a0a,820F2048=0a0a0a0a,"
			"820E204C=0a0a0a0a,820F204C=0a0a0a0a,"
			"820E2050=0a0a0a0a,820F2050=0a0a0a0a,"
			"820E2054=0a0a0a0a,820F2054=0a0a0a0a,"
			"820E2058=0a0a0a0a,820F2058=0a0a0a0a", intf);

    DPRINT_INFO(WFA_OUT, "iwpriv %s mac 820E2048~820E2058=0a0a0a0a,820F2048~820F2048=0a0a0a0a\n", intf);
    sret = system(gCmdStr);

    return 0;
}
#endif


/*
 * agtCmdProcGetVersion(): response "ca_get_version" command to controller
 *  input:  cmd --- not used
 *          valLen -- not used
 *  output: parms -- a buffer to store the version info response.
 */
int agtCmdProcGetVersion(int len, BYTE *parms, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *getverResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "entering agtCmdProcGetVersion ...\n");

    getverResp->status = STATUS_COMPLETE;
    wSTRNCPY(getverResp->cmdru.version, WFA_SYSTEM_VER, WFA_VERNAM_LEN);

    wfaEncodeTLV(WFA_GET_VERSION_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)getverResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaAssociate():
 *    The function is to force the station wireless I/F to re/associate
 *    with the AP.
 */
int wfaStaAssociate(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *assoc = (dutCommand_t *)caCmdBuf;
    char *ifname = assoc->intf;
    dutCmdResponse_t *staAssocResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "entering wfaStaAssociate ...\n");
    /*
     * if bssid appears, station should associate with the specific
     * BSSID AP at its initial association.
     * If it is different to the current associating AP, it will be forced to
     * roam the new AP
     */
    if(assoc->cmdsu.assoc.bssid[0] != '\0')
    {
        /* if (the first association) */
        /* just do initial association to the BSSID */


        /* else (station already associate to an AP) */
        /* Do forced roaming */
	    if((strncmp(assoc->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON
			    ||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	      ) {
		    wfa_driver_mtk_set_cmd(assoc->intf, "ApCliEnable=0");
		    sprintf(gCmdStr, "ApCliBssid=%s", assoc->cmdsu.assoc.bssid);
		    wfa_driver_mtk_set_cmd(assoc->intf, gCmdStr);

		    wfa_driver_mtk_set_cmd(assoc->intf, "ApCliEnable=1");

	    }
    }
    else
    {
#ifdef MTK_PRIVATE_IOCTL

	if((strncmp(ifname,"apcli", 5) == 0)
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif /* TGAC_DAEMON */
		 ||(strncmp(ifname, "wlan-apcli", 10) == 0)
	)
	{
#ifdef DBDC_MODE_CERT
#ifndef WIFI_MULTI_PROFILE
		if (strncmp(ifname,"apcli1", 6) == 0)
		{
			wfa_driver_mtk_set_cmd(ifname, "ApCliWirelessMode=14");
		}
#endif /* WIFI_MULTI_PROFILE */
#endif /* DBDC_MODE_CERT */

		sprintf(gCmdStr, "ApCliSsid=%s", assoc->cmdsu.ssid);
		wfa_driver_mtk_set_cmd(ifname, gCmdStr);
		sleep(1);
		wfa_driver_mtk_set_cmd(ifname, "ApCliEnable=1");
//don't need to set ApCliAutoConnect again, this will lead connect twice!
#if 0//def DRIVER_SCAN_TRIGGER
		sleep(2);
		/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
		wfa_driver_mtk_set_cmd(ifname, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */

	} else {
		sprintf(gCmdStr, "SSID=%s", assoc->cmdsu.ssid);
		wfa_driver_mtk_set_cmd(ifname, gCmdStr);
	}
#else /* MTK_PRIVATE_IOCTL */
        /* use 'ifconfig' command to bring down the interface (linux specific) */
      sprintf(gCmdStr, "ifconfig %s down", ifname);
      sret = system(gCmdStr);

        /* use 'ifconfig' command to bring up the interface (linux specific) */
      sprintf(gCmdStr, "ifconfig %s up", ifname);
      sret = system(gCmdStr);

        /*
         *  use 'wpa_cli' command to force a 802.11 re/associate
         *  (wpa_supplicant specific)
         */
        sprintf(gCmdStr, "wpa_cli -i%s reassociate", ifname);
        sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */
    }

    /*
     * Then report back to control PC for completion.
     * This does not have failed/error status. The result only tells
     * a completion.
     */
    staAssocResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_ASSOCIATE_RESP_TLV, 4, (BYTE *)staAssocResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaReAssociate():
 *    The function is to force the station wireless I/F to re/associate
 *    with the AP.
 */
int wfaStaReAssociate(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *assoc = (dutCommand_t *)caCmdBuf;
    char *ifname = assoc->intf;
    dutCmdResponse_t *staAssocResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "entering wfaStaAssociate ...\n");
    /*
     * if bssid appears, station should associate with the specific
     * BSSID AP at its initial association.
     * If it is different to the current associating AP, it will be forced to
     * roam the new AP
     */
    if(assoc->cmdsu.assoc.bssid[0] != '\0')
    {
        /* if (the first association) */
        /* just do initial association to the BSSID */


        /* else (station already associate to an AP) */
        /* Do forced roaming */

    }
    else
    {
#ifndef MTK_PRIVATE_IOCTL
        /* use 'ifconfig' command to bring down the interface (linux specific) */
        sprintf(gCmdStr, "ifconfig %s down", ifname);
        sret = system(gCmdStr);

        /* use 'ifconfig' command to bring up the interface (linux specific) */
        sprintf(gCmdStr, "ifconfig %s up", ifname);
	sret = system(gCmdStr); /* MTK patch */

        /*
         *  use 'wpa_cli' command to force a 802.11 re/associate
         *  (wpa_supplicant specific)
         */
        sprintf(gCmdStr, "wpa_cli -i%s reassociate", ifname);
        sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */
    }

    /*
     * Then report back to control PC for completion.
     * This does not have failed/error status. The result only tells
     * a completion.
     */
    staAssocResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_ASSOCIATE_RESP_TLV, 4, (BYTE *)staAssocResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaReAssoc():
 *    The function is to force the station wireless I/F to re/associate
 *    with the AP.
 */
int wfaStaReAssoc(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *assoc = (dutCommand_t *)caCmdBuf;
    char *ifname = assoc->intf;
    dutCmdResponse_t *staAssocResp = &gGenericResp;
	int is_connected = 0;



    DPRINT_INFO(WFA_OUT, "entering wfaStaReAssoc ...\n");
    /*
     * if bssid appears, station should associate with the specific
     * BSSID AP at its initial association.
     * If it is different to the current associating AP, it will be forced to
     * roam the new AP
     */
    if(assoc->cmdsu.assoc.bssid[0] != '\0')
    {
        /* if (the first association) */
        /* just do initial association to the BSSID */
        /* else (station already associate to an AP) */
        /* Do forced roaming */
		if((strncmp(assoc->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		) {
			wfa_driver_mtk_set_cmd(assoc->intf, "ApCliEnable=0");

			sprintf(gCmdStr, "ApCliBssid=%s", assoc->cmdsu.assoc.bssid); 
			wfa_driver_mtk_set_cmd(assoc->intf, gCmdStr);

			wfa_driver_mtk_set_cmd(assoc->intf, "ApCliEnable=1");

		}

    }
    else
    {

		if((strncmp(assoc->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		) {
			wfa_driver_mtk_set_cmd(assoc->intf, "ApCliEnable=1");
		}

#ifndef MTK_PRIVATE_IOCTL
        /* use 'ifconfig' command to bring down the interface (linux specific) */
        sprintf(gCmdStr, "ifconfig %s down", ifname);
        sret = system(gCmdStr);

        /* use 'ifconfig' command to bring up the interface (linux specific) */
        sprintf(gCmdStr, "ifconfig %s up", ifname);
	sret = system(gCmdStr); /* MTK patch */

        /*
         *  use 'wpa_cli' command to force a 802.11 re/associate
         *  (wpa_supplicant specific)
         */
        sprintf(gCmdStr, "wpa_cli -i%s reassociate", ifname);
        sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */
    }

    /*
     * Then report back to control PC for completion.
     * This does not have failed/error status. The result only tells
     * a completion.
     */
    staAssocResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_REASSOC_RESP_TLV, 4, (BYTE *)staAssocResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}


/*
 * wfaStaIsConnected():
 *    The function is to check whether the station's wireless I/F has
 *    already connected to an AP.
 */
int wfaStaIsConnected(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *connStat = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *staConnectResp = &gGenericResp;
    char *ifname = connStat->intf;
    FILE *tmpfile = NULL;
    char result[32];


    DPRINT_INFO(WFA_OUT, "Entering isConnected ...\n");

#ifdef MTK_PRIVATE_IOCTL
   wfa_driver_mtk_get_oid(ifname, OID_GEN_MEDIA_CONNECT_STATUS, (unsigned char *)&staConnectResp->cmdru.connected, sizeof(int));
   DPRINT_INFO(WFA_OUT, "staConnectResp->cmdru.connected = %d\n", staConnectResp->cmdru.connected);
#else /* MTK_PRIVATE_IOCTL */
#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_chkconnect %s\n", ifname);
    sret = system(gCmdStr);

    if(chk_ret_status() == WFA_SUCCESS)
        staConnectResp->cmdru.connected = 1;
    else
        staConnectResp->cmdru.connected = 0;
#else
    /*
     * use 'wpa_cli' command to check the interface status
     * none, scanning or complete (wpa_supplicant specific)
     */
    sprintf(gCmdStr, "wpa_cli -i%s status | grep ^wpa_state= | cut -f2- -d= > /tmp/.isConnected", ifname);
    sret = system(gCmdStr);

    /*
     * the status is saved in a file.  Open the file and check it.
     */
    tmpfile = fopen("/tmp/.isConnected", "r+");
    if(tmpfile == NULL)
    {
        staConnectResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_IS_CONNECTED_RESP_TLV, 4, (BYTE *)staConnectResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    sret = fscanf(tmpfile, "%s", (char *)result);

    if(strncmp(result, "COMPLETED", 9) == 0)
        staConnectResp->cmdru.connected = 1;
    else
        staConnectResp->cmdru.connected = 0;
#endif
#endif /* !MTK_PRIVATE_IOCTL */

	/* add HE-5.64.1 workaround */
	if((strcmp(gHeCfgSet_t.ssid,"HE-5.64.1_24G") == 0)
		|| (strcmp(gHeCfgSet_t.ssid,"HE-5.64.1_5G") == 0)
		|| (strcmp(gHeCfgSet_t.ssid,"HE-5.64.1") == 0))
	{
		SetAggLimit(ifname);
	}

    /*
     * HE-5.54.1_5G workaround for Chip MT7915 (AX1800)
     * When we run cert. test case HE-5.54.1_5G, the CPU loading is full load.
     * 7621 CPU is not powerful and traffic generator take a lot of loading.
     * There is no free CPU loading to handle traffic from host to PLE in HE-5.54.1_5G.
     * We add Fixedrate workaround to limit the rate, so the BA can always aggregate more than 64.
     */
    if((strcmp(gHeCfgSet_t.ssid,"HE-5.54.1_5G") == 0))
    {
	    sprintf(gCmdStr, "iwpriv apclix0 set FixedRate=1-8-2-3-2-85-0-0-7-0");
	    DPRINT_INFO(WFA_OUT, "iwpriv apclix0 set FixedRate=1-8-2-3-2-85-0-0-7-0\n");
	    /*
	     * AX3600 workaround
	     * HW send packet faster than upper layer application (tput gen.)
	     * so we fixedrate to limit the MCS rate, in order to aggregate packet more than 64.
	     */
	    sprintf(gCmdStr, "iwpriv apclii0 set FixedRate=1-8-2-3-2-85-0-0-7-0");
	    DPRINT_INFO(WFA_OUT, "iwpriv apclii0 set FixedRate=1-8-2-3-2-85-0-0-7-0\n");
	    sret = system(gCmdStr);

    }

    /*
     * Report back the status: Complete or Failed.
     */
    staConnectResp->status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_IS_CONNECTED_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)staConnectResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetIpConfig():
 * This function is to retriev the ip info including
 *     1. dhcp enable
 *     2. ip address
 *     3. mask
 *     4. primary-dns
 *     5. secondary-dns
 *
 *     The current implementation is to use a script to find these information
 *     and store them in a file.
 */
int wfaStaGetIpConfig(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    int slen, ret, i = 0;
    dutCommand_t *getIpConf = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *ipconfigResp = &gGenericResp;
    char *ifname = getIpConf->intf;
    caStaGetIpConfigResp_t *ifinfo = &ipconfigResp->cmdru.getIfconfig;

    FILE *tmpfd;
    char string[256];
    char *str;

    /*
     * check a script file (the current implementation specific)
     */
    ret = access("/sbin/getipconfig.sh", F_OK);
    if(ret == -1)
    {
        ipconfigResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, 4, (BYTE *)ipconfigResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file not exist\n");
        return WFA_FAILURE;

    }

    strcpy(ifinfo->dns[0], "0");
    strcpy(ifinfo->dns[1], "0");

    /*
     * Run the script file "getipconfig.sh" to check the ip status
     * (current implementation  specific).
     * note: "getipconfig.sh" is only defined for the current implementation
     */
    sprintf(gCmdStr, "getipconfig.sh /tmp/ipconfig.txt %s\n", ifname);

    sret = system(gCmdStr);

    /* open the output result and scan/retrieve the info */
    tmpfd = fopen("/tmp/ipconfig.txt", "r+");

    if(tmpfd == NULL)
    {
        ipconfigResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, 4, (BYTE *)ipconfigResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    for(;;)
    {
        if(fgets(string, 256, tmpfd) == NULL)
            break;

        /* check dhcp enabled */
        if(strncmp(string, "dhcpcli", 7) ==0)
        {
            str = strtok(string, "=");
            str = strtok(NULL, "=");
            if(str != NULL)
                ifinfo->isDhcp = 1;
            else
                ifinfo->isDhcp = 0;
        }

        /* find out the ip address */
        if(strncmp(string, "ipaddr", 6) == 0)
        {
            str = strtok(string, "=");
            str = strtok(NULL, " ");
            if(str != NULL)
            {
                wSTRNCPY(ifinfo->ipaddr, str, 15);

                ifinfo->ipaddr[15]='\0';
            }
            else
                wSTRNCPY(ifinfo->ipaddr, "none", 15);
        }

        /* check the mask */
        if(strncmp(string, "mask", 4) == 0)
        {
            char ttstr[16];
            char *ttp = ttstr;

            str = strtok_r(string, "=", &ttp);
            if(*ttp != '\0')
            {
                strcpy(ifinfo->mask, ttp);
                slen = strlen(ifinfo->mask);
                ifinfo->mask[slen-1] = '\0';
            }
            else
                strcpy(ifinfo->mask, "none");
        }

        /* find out the dns server ip address */
        if(strncmp(string, "nameserv", 8) == 0)
        {
            char ttstr[16];
            char *ttp = ttstr;

            str = strtok_r(string, " ", &ttp);
            if(str != NULL && i < 2)
            {
                strcpy(ifinfo->dns[i], ttp);
                slen = strlen(ifinfo->dns[i]);
                ifinfo->dns[i][slen-1] = '\0';
            }
            else
                strcpy(ifinfo->dns[i], "none");

            i++;
        }
    }

    /*
     * Report back the results
     */
    ipconfigResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)ipconfigResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

#if 0
    DPRINT_INFO(WFA_OUT, "%i %i %s %s %s %s %i\n", ipconfigResp->status,
                ifinfo->isDhcp, ifinfo->ipaddr, ifinfo->mask,
                ifinfo->dns[0], ifinfo->dns[1], *respLen);
#endif

    fclose(tmpfd);
    return WFA_SUCCESS;
}

/*
 * wfaStaSetIpConfig():
 *   The function is to set the ip configuration to a wireless I/F.
 *   1. IP address
 *   2. Mac address
 *   3. default gateway
 *   4. dns nameserver (pri and sec).
 */
int wfaStaSetIpConfig(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *setIpConf = (dutCommand_t *)caCmdBuf;
    caStaSetIpConfig_t *ipconfig = &setIpConf->cmdsu.ipconfig;
    dutCmdResponse_t *staSetIpResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "entering wfaStaSetIpConfig ...\n");

    /*
     * Use command 'ifconfig' to configure the interface ip address, mask.
     * (Linux specific).
     */
    sprintf(gCmdStr, "/sbin/ifconfig %s %s netmask %s > /dev/null 2>&1 ", ipconfig->intf, ipconfig->ipaddr, ipconfig->mask);
    sret = system(gCmdStr);

    /* use command 'route add' to set set gatewway (linux specific) */
    if(ipconfig->defGateway[0] != '\0')
    {
        sprintf(gCmdStr, "/sbin/route add default gw %s > /dev/null 2>&1", ipconfig->defGateway);
        sret = system(gCmdStr);
    }

    /* set dns (linux specific) */
    sprintf(gCmdStr, "cp /etc/resolv.conf /tmp/resolv.conf.bk");
    sret = system(gCmdStr);
    sprintf(gCmdStr, "echo nameserv %s > /etc/resolv.conf", ipconfig->pri_dns);
    sret = system(gCmdStr);
    sprintf(gCmdStr, "echo nameserv %s >> /etc/resolv.conf", ipconfig->sec_dns);
    sret = system(gCmdStr);

    /*
     * report status
     */
    staSetIpResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_IP_CONFIG_RESP_TLV, 4, (BYTE *)staSetIpResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaVerifyIpConnection():
 * The function is to verify if the station has IP connection with an AP by
 * send ICMP/pings to the AP.
 */
int wfaStaVerifyIpConnection(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *verip = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *verifyIpResp = &gGenericResp;

#ifndef WFA_PING_UDP_ECHO_ONLY
    char strout[64], *pcnt;
    FILE *tmpfile;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaVerifyIpConnection ...\n");

    /* set timeout value in case not set */
    if(verip->cmdsu.verifyIp.timeout <= 0)
    {
        verip->cmdsu.verifyIp.timeout = 10;
    }

    /* execute the ping command  and pipe the result to a tmp file */
    sprintf(gCmdStr, "ping %s -c 3 -W %u | grep loss | cut -f3 -d, 1>& /tmp/pingout.txt", verip->cmdsu.verifyIp.dipaddr, verip->cmdsu.verifyIp.timeout);
    sret = system(gCmdStr);

    /* scan/check the output */
    tmpfile = fopen("/tmp/pingout.txt", "r+");
    if(tmpfile == NULL)
    {
        verifyIpResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, 4, (BYTE *)verifyIpResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    verifyIpResp->status = STATUS_COMPLETE;
    if(fscanf(tmpfile, "%s", strout) == EOF)
        verifyIpResp->cmdru.connected = 0;
    else
    {
        pcnt = strtok(strout, "%");

        /* if the loss rate is 100%, not able to connect */
        if(atoi(pcnt) == 100)
            verifyIpResp->cmdru.connected = 0;
        else
            verifyIpResp->cmdru.connected = 1;
    }

    fclose(tmpfile);
#else
    int btSockfd;
    struct pollfd fds[2];
    int timeout = 2000;
    char anyBuf[64];
    struct sockaddr_in toAddr;
    int done = 1, cnt = 0, ret, nbytes;

    verifyIpResp->status = STATUS_COMPLETE;
    verifyIpResp->cmdru.connected = 0;

    btSockfd = wfaCreateUDPSock("127.0.0.1", WFA_UDP_ECHO_PORT);

    if(btSockfd == -1)
    {
        verifyIpResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, 4, (BYTE *)verifyIpResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;
        return WFA_FAILURE;;
    }

    toAddr.sin_family = AF_INET;
    toAddr.sin_addr.s_addr = inet_addr(verip->cmdsu.verifyIp.dipaddr);
    toAddr.sin_port = htons(WFA_UDP_ECHO_PORT);

    while(done)
    {
        wfaTrafficSendTo(btSockfd, (char *)anyBuf, 64, (struct sockaddr *)&toAddr);
        cnt++;

        fds[0].fd = btSockfd;
        fds[0].events = POLLIN | POLLOUT;

        ret = poll(fds, 1, timeout);
        switch(ret)
        {
        case 0:
            /* it is time out, count a packet lost*/
            break;
        case -1:
        /* it is an error */
        default:
        {
            switch(fds[0].revents)
            {
            case POLLIN:
            case POLLPRI:
            case POLLOUT:
                nbytes = wfaTrafficRecv(btSockfd, (char *)anyBuf, (struct sockaddr *)&toAddr);
                if(nbytes != 0)
                    verifyIpResp->cmdru.connected = 1;
                done = 0;
                break;
            default:
                /* errors but not care */
                ;
            }
        }
        }
        if(cnt == 3)
        {
            done = 0;
        }
    }

#endif

    wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)verifyIpResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetMacAddress()
 *    This function is to retrieve the MAC address of a wireless I/F.
 */
int wfaStaGetMacAddress(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *getMac = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *getmacResp = &gGenericResp;
    char *str;
    char *ifname = getMac->intf;

    FILE *tmpfd;
    char string[257];

    DPRINT_INFO(WFA_OUT, "Entering wfaStaGetMacAddress ... ifname(%s)\n",ifname);
    /*
     * run the script "getipconfig.sh" to find out the mac
     */
    sprintf(gCmdStr, "ifconfig %s > /tmp/ipconfig.txt ", ifname);
    sret = system(gCmdStr);

    tmpfd = fopen("/tmp/ipconfig.txt", "r+");
    if(tmpfd == NULL)
    {
        getmacResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_MAC_ADDRESS_RESP_TLV, 4, (BYTE *)getmacResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;
	DPRINT_INFO(WFA_OUT, "file open failed\n");
        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    if(fgets((char *)&string[0], 256, tmpfd) == NULL)
    {
    	DPRINT_INFO(WFA_OUT, "fgets failed\n");
        getmacResp->status = STATUS_ERROR;
    }

    str = strtok(string, " ");
    while(str && ((strcmp(str,"HWaddr")) != 0))
    {
        str = strtok(NULL, " ");
    }

    /* get mac */
    if(str)
    {
        str = strtok(NULL, " ");
#ifdef TGAC_DAEMON	
	printf("dut_mac=%s\n", dut_mac);
	strcpy(getmacResp->cmdru.mac, dut_mac);//woody
#else
        strcpy(getmacResp->cmdru.mac, str);
#endif /* TGAC_DAEMON */
        getmacResp->status = STATUS_COMPLETE;
    }

    wfaEncodeTLV(WFA_STA_GET_MAC_ADDRESS_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)getmacResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    fclose(tmpfd);
    return WFA_SUCCESS;
}

/*
 * wfaStaGetStats():
 * The function is to retrieve the statistics of the I/F's layer 2 txFrames,
 * rxFrames, txMulticast, rxMulticast, fcsErrors/crc, and txRetries.
 * Currently there is not definition how to use these info.
 */
int wfaStaGetStats(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *statsResp = &gGenericResp;

    /* this is never used, you can skip this call */

    statsResp->status = STATUS_ERROR;
    wfaEncodeTLV(WFA_STA_GET_STATS_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)statsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);


    return WFA_SUCCESS;
}

/*
 * wfaSetEncryption():
 *   The function is to set the wireless interface with WEP or none.
 *
 *   Since WEP is optional test, current function is only used for
 *   resetting the Security to NONE/Plaintext (OPEN). To test WEP,
 *   this function should be replaced by the next one (wfaSetEncryption1())
 *
 *   Input parameters:
 *     1. I/F
 *     2. ssid
 *     3. encpType - wep or none
 *     Optional:
 *     4. key1
 *     5. key2
 *     6. key3
 *     7. key4
 *     8. activeKey Index
 */

int wfaSetEncryption1(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEncryption_t *setEncryp = (caStaSetEncryption_t *)caCmdBuf;
    dutCmdResponse_t *setEncrypResp = &gGenericResp;

#ifdef MTK_PRIVATE_IOCTL
   DPRINT_INFO(WFA_OUT, "entering wfaSetEncryption1 ...\n");
	if((strncmp(setEncryp->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif /* TGAC_DAEMON */
		)
	{
#ifdef DBDC_MODE_CERT
#ifndef WIFI_MULTI_PROFILE
		if (strncmp(setEncryp->intf,"apcli1", 6) == 0)
		{
			wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliWirelessMode=14");
		}
#endif /* WIFI_MULTI_PROFILE */
#endif /* DBDC_MODE_CERT */

		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEnable=0");
		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliAuthMode=OPEN");
		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEncrypType=NONE");
		sprintf(gCmdStr, "ApCliSsid=%s", setEncryp->ssid);
		wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEnable=1");

#ifdef DRIVER_SCAN_TRIGGER
		sleep(2);
		/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */	   
	} else {
		   wfa_driver_mtk_set_cmd(setEncryp->intf, "AuthMode=OPEN");
		   wfa_driver_mtk_set_cmd(setEncryp->intf, "EncrypType=NONE");
		   sprintf(gCmdStr, "SSID=%s", setEncryp->ssid);
		   wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);	
	}
	
#else /* MTK_PRIVATE_IOCTL */
    /*
     * disable the network first
     */
    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", setEncryp->intf);
    sret = system(gCmdStr);

    /*
     * set SSID
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", setEncryp->intf, setEncryp->ssid);
    sret = system(gCmdStr);

    /*
     * Tell the supplicant for infrastructure mode (1)
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 mode 0", setEncryp->intf);
    sret = system(gCmdStr);

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt NONE", setEncryp->intf);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", setEncryp->intf);
    sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */

    setEncrypResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_ENCRYPTION_RESP_TLV, 4, (BYTE *)setEncrypResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 *  Since WEP is optional, this function could be used to replace
 *  wfaSetEncryption() if necessary.
 */
int wfaSetEncryption(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEncryption_t *setEncryp = (caStaSetEncryption_t *)caCmdBuf;
    dutCmdResponse_t *setEncrypResp = &gGenericResp;
    int i;

#ifdef MTK_PRIVATE_IOCTL

	if((strncmp(setEncryp->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif /* TGAC_DAEMON */
	)
	{
		DPRINT_INFO(WFA_OUT, "entering wfaSetEncryption (apcli) ...\n");

#ifdef DBDC_MODE_CERT
#ifndef WIFI_MULTI_PROFILE
		if (strncmp(setEncryp->intf,"apcli1", 6) == 0)
		{
			wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliWirelessMode=14");
		}
#endif /* WIFI_MULTI_PROFILE */
#endif /* DBDC_MODE_CERT */

		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEnable=0");
			
	   wfa_driver_mtk_set_cmd(setEncryp->intf, "NetworkType=Infra");
	   wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliAuthMode=OPEN");

	   if(setEncryp->encpType == ENCRYPT_WEP)
	   {
		  wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEncrypType=WEP");

		  for(i=0; i<4; i++)
	      {
	         if(setEncryp->keys[i][0] != '\0')
	         {
	             sprintf(gCmdStr, "ApCliKey%i=%s",
	                   i+1, setEncryp->keys[i]);
				 wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
	         }
	      }

	      /* set active key */
	      i = setEncryp->activeKeyIdx;
	      if(setEncryp->keys[i][0] != '\0')
	      {
	          sprintf(gCmdStr, "ApCliDefaultKeyID=%i", setEncryp->activeKeyIdx);
			  wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
	      }
	   }
	   else
	   {
		  wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEncrypType=NONE");
	   }

	   sprintf(gCmdStr, "ApCliSsid=%s", setEncryp->ssid);
	   wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);

	   wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliEnable=1");

#ifdef DRIVER_SCAN_TRIGGER
		sleep(2);
		/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
		wfa_driver_mtk_set_cmd(setEncryp->intf, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */

	} else
	{
	   DPRINT_INFO(WFA_OUT, "entering wfaSetEncryption (ra0)...\n");

	   wfa_driver_mtk_set_cmd(setEncryp->intf, "NetworkType=Infra");
	   wfa_driver_mtk_set_cmd(setEncryp->intf, "AuthMode=OPEN");

	   if(setEncryp->encpType == ENCRYPT_WEP)
	   {
		  wfa_driver_mtk_set_cmd(setEncryp->intf, "EncrypType=WEP");

		  for(i=0; i<4; i++)
	      {
	         if(setEncryp->keys[i][0] != '\0')
	         {
	             sprintf(gCmdStr, "Key%i=%s",
	                   i+1, setEncryp->keys[i]);
				 wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
	         }
	      }

	      /* set active key */
	      i = setEncryp->activeKeyIdx;
	      if(setEncryp->keys[i][0] != '\0')
	      {
	          sprintf(gCmdStr, "DefaultKeyID=%i", setEncryp->activeKeyIdx);
			  wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
	      }
	   }
	   else
	   {
		  wfa_driver_mtk_set_cmd(setEncryp->intf, "EncrypType=NONE");
	   }

	   sprintf(gCmdStr, "SSID=%s", setEncryp->ssid);
	   wfa_driver_mtk_set_cmd(setEncryp->intf, gCmdStr);
	}
#else /* MTK_PRIVATE_IOCTL */
    /*
     * disable the network first
     */
    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", setEncryp->intf);
    sret = system(gCmdStr);

    /*
     * set SSID
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", setEncryp->intf, setEncryp->ssid);
    sret = system(gCmdStr);

    /*
     * Tell the supplicant for infrastructure mode (1)
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 mode 0", setEncryp->intf);
    sret = system(gCmdStr);

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt NONE", setEncryp->intf);
    sret = system(gCmdStr);

    /* set keys */
    if(setEncryp->encpType == 1)
    {
        for(i=0; i<4; i++)
        {
            if(setEncryp->keys[i][0] != '\0')
            {
                sprintf(gCmdStr, "wpa_cli -i %s set_network 0 wep_key%i %s",
                        setEncryp->intf, i, setEncryp->keys[i]);
                sret = system(gCmdStr);
            }
        }

        /* set active key */
        i = setEncryp->activeKeyIdx;
        if(setEncryp->keys[i][0] != '\0')
        {
            sprintf(gCmdStr, "wpa_cli -i %s set_network 0 wep_tx_keyidx %i",
                    setEncryp->intf, setEncryp->activeKeyIdx);
            sret = system(gCmdStr);
        }
    }
    else /* clearly remove the keys -- reported by p.schwann */
    {

        for(i = 0; i < 4; i++)
        {
            sprintf(gCmdStr, "wpa_cli -i %s set_network 0 wep_key%i \"\"", setEncryp->intf, i);
            sret = system(gCmdStr);
        }
    }

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", setEncryp->intf);
    sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */

    setEncrypResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_ENCRYPTION_RESP_TLV, 4, (BYTE *)setEncrypResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetSecurity(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    int ret = WFA_SUCCESS;
    dutCmdResponse_t *set_sec_resp = &gGenericResp;


#ifndef WFA_PC_CONSOLE
	   dutCommand_t *cmd = (dutCommand_t *)caCmdBuf;

	   caStaSetSecurity_t *set_sec = (caStaSetSecurity_t *)&cmd->cmdsu.setsec;

	   DPRINT_INFO(WFA_OUT, "%s() inf=%s type=%u keyMgmtType=%s encpType=%s\n",
	   				__FUNCTION__, cmd->intf, set_sec->type, set_sec->keyMgmtType, set_sec->encpType);
#ifdef MTK_PRIVATE_IOCTL
		if((strncmp(cmd->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)
		{
#ifdef DBDC_MODE_CERT
#ifndef WIFI_MULTI_PROFILE
			if (strncmp(cmd->intf,"apcli1", 6) == 0)
			{
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliWirelessMode=14");
			}
#endif /* WIFI_MULTI_PROFILE */
#endif /* DBDC_MODE_CERT */

		   wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEnable=0");

		   //TODO : maybe need to check encpType for detail
		   if((set_sec->type == SEC_TYPE_PSK) && (strcasecmp(set_sec->keyMgmtType, "wpa2") == 0) && (strcasecmp(set_sec->encpType, "aes-ccmp") == 0)) 
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=WPA2PSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=AES");
		   }
		   else if((set_sec->type == SEC_TYPE_PSK) && (strcasecmp(set_sec->keyMgmtType, "wpa") == 0) && (strcasecmp(set_sec->encpType, "tkip") == 0))
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=WPAPSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=TKIP");
		   }
		   else if((set_sec->type == SEC_TYPE_SAE) && (strcasecmp(set_sec->keyMgmtType, "wpa2") == 0) && (strcasecmp(set_sec->encpType, "aes-ccmp") == 0))
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=WPA3PSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=AES");
				if (set_sec->EcGroupId != 0) {
					sprintf(gCmdStr, "ApCliSAEGroup=%d", set_sec->EcGroupId);
					wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
				}

		   }
		   else if((set_sec->type == SEC_TYPE_PSKSAE) && (strcasecmp(set_sec->keyMgmtType, "wpa2") == 0) && (strcasecmp(set_sec->encpType, "aes-ccmp") == 0))
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=WPA2PSKWPA3PSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=AES");

				if (set_sec->EcGroupId != 0) {
					sprintf(gCmdStr, "ApCliSAEGroup=%d", set_sec->EcGroupId);
					wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
				}
		   }
		   else if((set_sec->type == SEC_TYPE_OWE) && (strcasecmp(set_sec->keyMgmtType, "owe") == 0) && (strcasecmp(set_sec->encpType, "aes-ccmp") == 0))
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=OWE");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=AES");
				if (set_sec->EcGroupId != 0) {
					sprintf(gCmdStr, "ApCliOWEGroup=%d", set_sec->EcGroupId);
					wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
				}

		   }
		   else if(set_sec->type == SEC_TYPE_OPEN)
		   {
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=OPEN");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=NONE");
		   }
		   else if(set_sec->type == SEC_TYPE_WEP)
		   {
			    int i;
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=OPEN");
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=WEP");
				
				for(i=0; i<4; i++)
				{
				   if(set_sec->secu.wepkey.keys[i][0] != '\0')
				   {
					   sprintf(gCmdStr, "ApCliKey%i=%s",
							 i+1, set_sec->secu.wepkey.keys[i]);
					   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
				   }
				}
				
				/* set active key */
				i = set_sec->secu.wepkey.activeKeyIdx;
				if(set_sec->secu.wepkey.keys[i][0] != '\0')
				{
					sprintf(gCmdStr, "ApCliDefaultKeyID=%i", set_sec->secu.wepkey.activeKeyIdx);
					wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
				}

		   }
		   else // wpa2
		   {
			wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAuthMode=WPA2PSK");
			wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEncrypType=AES");
			}

		}
		else
		{
			//TODO : maybe need to check encpType for detail
			if(strcasecmp(set_sec->keyMgmtType, "wpa2-wpa-psk") == 0) //for AutoMix Mode for TGn 5.2.53 Item. (This cmd need driver automode support)
			{
				wfa_driver_mtk_set_cmd(cmd->intf, "AuthMode=WPAPSKWPA2PSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "EncrypType=AES"); //actually auto mode don't need to set this
			}
			else if(strcasecmp(set_sec->keyMgmtType, "wpa") == 0)
			{
				wfa_driver_mtk_set_cmd(cmd->intf, "AuthMode=WPAPSK");
				wfa_driver_mtk_set_cmd(cmd->intf, "EncrypType=TKIP");
			}
			else //wpa2 case
			{
			wfa_driver_mtk_set_cmd(cmd->intf, "AuthMode=WPA2PSK");
			wfa_driver_mtk_set_cmd(cmd->intf, "EncrypType=AES");
			}
		}

#if 1 //add for PMF check, need to set the  the follwing cmd after the security cmds, otherwise we need to preset the profile.

	/*
		iwpriv ra0 set PMFSHA256=1 (set the AKM support SHA256)
		iwpriv ra0 set PMFMFPC=1 (enable the PMF)
		iwpriv ra0 set PMFMFPR=1 (set the the PMF to required)
	*/
	   sleep(1);

	   /* if PMF enable */
	   if(set_sec->pmf == WFA_ENABLED || set_sec->pmf == WFA_OPTIONAL)
	   {

			if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
			)
				sprintf(gCmdStr, "ApCliPMFMFPC=1");
			else
				sprintf(gCmdStr, "PMFMFPC=1");

		   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			//clear Required for when DUT run PMF 5.2 items, then the PMFR is always keep, not clear in following items.
			if(strncmp(cmd->intf,"apcli", 5) == 0)
				sprintf(gCmdStr, "ApCliPMFMFPR=0");
			else
				sprintf(gCmdStr, "PMFMFPR=0");

			wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

	   }
	   else if(set_sec->pmf == WFA_REQUIRED)
	   {

			if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
			)
			{
				sprintf(gCmdStr, "ApCliPMFMFPC=1");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "ApCliPMFMFPR=1");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}
			else
			{
				sprintf(gCmdStr, "PMFMFPC=1");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "PMFMFPR=1");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}

	   }
	   else if(set_sec->pmf == WFA_F_REQUIRED)
	   {

			if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
			)
				sprintf(gCmdStr, "ApCliPMFMFPR=1");
			else
				sprintf(gCmdStr, "PMFMFPR=1");

		   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
	   }
	   else if(set_sec->pmf == WFA_F_DISABLED)
	   {

			if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
			)
			{
				sprintf(gCmdStr, "ApCliPMFSHA256=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "ApCliPMFMFPC=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "ApCliPMFMFPR=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}
			else
			{
				sprintf(gCmdStr, "PMFSHA256=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "PMFMFPC=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "PMFMFPR=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}
	   }
	   else
	   {   /* Disable PMF */

		   if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		   ) {
			if ((set_sec->type == SEC_TYPE_SAE) || (set_sec->type == SEC_TYPE_PSKSAE) ||
				(set_sec->type == SEC_TYPE_OWE)) {

			   sprintf(gCmdStr, "ApCliPMFSHA256=1");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			   sprintf(gCmdStr, "ApCliPMFMFPC=1");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			   sprintf(gCmdStr, "ApCliPMFMFPR=1");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			} else {
			   sprintf(gCmdStr, "ApCliPMFSHA256=0");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			   sprintf(gCmdStr, "ApCliPMFMFPC=0");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			   sprintf(gCmdStr, "ApCliPMFMFPR=0");
			   wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}
		   }
		   else
		   {
				sprintf(gCmdStr, "PMFSHA256=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "PMFMFPC=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

				sprintf(gCmdStr, "PMFMFPR=0");
				wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
			}
	   }

#endif

		if(strncmp(cmd->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
			||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)
		{
			sprintf(gCmdStr, "ApCliSsid=%s", set_sec->ssid);
			wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			sleep(1);
			sprintf(gCmdStr, "ApCliWPAPSK=%s", set_sec->secu.passphrase);
			wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			wfa_driver_mtk_set_cmd(cmd->intf, "ApCliEnable=1");

#ifdef DRIVER_SCAN_TRIGGER
				sleep(2);
				/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
				wfa_driver_mtk_set_cmd(cmd->intf, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */
		}
		else
		{
			sprintf(gCmdStr, "SSID=%s", set_sec->ssid);
			wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);

			sleep(1);
			sprintf(gCmdStr, "WPAPSK=%s", set_sec->secu.passphrase);
			wfa_driver_mtk_set_cmd(cmd->intf, gCmdStr);
		}
#endif /* !MTK_PRIVATE_IOCTL */
#endif

    /*
    * set SSID
    */
    memset(gHeCfgSet_t.ssid, 0, WFA_SSID_NAME_LEN);
    memcpy(gHeCfgSet_t.ssid, set_sec->ssid, WFA_SSID_NAME_LEN);

		set_sec_resp->status = STATUS_COMPLETE;
		wfaEncodeTLV(WFA_STA_SET_SECURITY_RESP_TLV, 4, (BYTE *)set_sec_resp, respBuf);
		*respLen = WFA_TLV_HDR_LEN + 4;

    return ret;
}

int wfaStaScan(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    int ret = WFA_SUCCESS;
    dutCmdResponse_t *sta_scan_resp = &gGenericResp;
	caStaSetScan_t *staSetScan = (caStaSetScan_t *)caCmdBuf;

	memset(&sta_scan_resp->cmdru.scanResult, 0, WFA_SCAN_RESULT_LEN);

	if(strncmp(staSetScan->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	  ) {
	  	if(staSetScan->output_ssid_bssid == 1)
	  	{
			char resultBuf[WFA_SCAN_RESULT_LEN]="";
			char tmpBuf[256]="";
			NDIS_802_11_GET_SSID_BSSID scan_result = {0};
			PSSID_BSSID scan_entry = NULL;
			unsigned char i = 0;

	  		DPRINT_INFO(WFA_OUT, "scan and get ssid/bssid result\n");
			wfa_driver_mtk_set_cmd(staSetScan->intf, "ApCliAutoConnect=4");
			sleep(6); // sleep 6 sec
			DPRINT_INFO(WFA_OUT, "scan done\n");

			wfa_driver_mtk_get_oid(staSetScan->intf,
				OID_802_11_GET_SSID_BSSID,
				(unsigned char *)&scan_result,
				sizeof(NDIS_802_11_GET_SSID_BSSID));

			for (i = 0; i <	scan_result.entry_num; i++)
			{
				scan_entry = &scan_result.entry[i];
				sprintf(tmpBuf,",SSID,%.*s,BSSID,%02x:%02x:%02x:%02x:%02x:%02x",
					scan_entry->ssid_len,
					scan_entry->ssid,
					scan_entry->bssid[0],
					scan_entry->bssid[1],
					scan_entry->bssid[2],
					scan_entry->bssid[3],
					scan_entry->bssid[4],
					scan_entry->bssid[5]);
				strncat(resultBuf, tmpBuf, strlen(tmpBuf));
				memset(tmpBuf, 0, sizeof(tmpBuf));
			}

			strncpy(sta_scan_resp->cmdru.scanResult, resultBuf, WFA_SCAN_RESULT_LEN);
		}
		else
		{
			DPRINT_INFO(WFA_OUT, "scan only\n");
			wfa_driver_mtk_set_cmd(staSetScan->intf, "ApCliAutoConnect=4");
			sleep(6); // sleep 6 sec
			DPRINT_INFO(WFA_OUT, "scan done\n");
		}
	}

    sta_scan_resp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SCAN_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)sta_scan_resp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
    return ret;
}

/*
 * wfaStaSetEapTLS():
 *   This is to set
 *   1. ssid
 *   2. encrypType - tkip or aes-ccmp
 *   3. keyManagementType - wpa or wpa2
 *   4. trustedRootCA
 *   5. clientCertificate
 */
int wfaStaSetEapTLS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapTLS_t *setTLS = (caStaSetEapTLS_t *)caCmdBuf;
    char *ifname = setTLS->intf;
    dutCmdResponse_t *setEapTlsResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaSetEapTLS ...\n");

    /*
     * need to store the trustedROOTCA and clientCertificate into a file first.
     */
#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eaptls -i %s %s %s %s", ifname, setTLS->ssid, setTLS->trustedRootCA, setTLS->clientCertificate);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    /* ssid */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setTLS->ssid);
    sret = system(gCmdStr);

    /* key management */
    if(strcasecmp(setTLS->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2") == 0)
    {
        // to take all and device to pick any one supported.
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

    /* protocol WPA */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 proto WPA", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap TLS", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ca_cert '\"%s\"'", ifname, setTLS->trustedRootCA);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"wifi-user@wifilabs.local\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 private_key '\"%s/%s\"'", ifname, CERTIFICATES_PATH, setTLS->clientCertificate);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 private_key_passwd '\"wifi\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setEapTlsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPTLS_RESP_TLV, 4, (BYTE *)setEapTlsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * The function is to set
 *   1. ssid
 *   2. passPhrase
 *   3. keyMangementType - wpa/wpa2
 *   4. encrypType - tkip or aes-ccmp
 */
int wfaStaSetPSK(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    /*Incompleted function*/
    dutCmdResponse_t *setPskResp = &gGenericResp;

#ifndef WFA_PC_CONSOLE
   caStaSetPSK_t *setPSK = (caStaSetPSK_t *)caCmdBuf;
    DPRINT_INFO(WFA_OUT, "entering wfaSetPSK ...:%s\n", setPSK->keyMgmtType);
#ifdef MTK_PRIVATE_IOCTL
   DPRINT_INFO(WFA_OUT, "entering wfaSetPSK ...\n");
   DPRINT_INFO(WFA_OUT, "interface name %s\n", setPSK->intf);
  

	if((strncmp(setPSK->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	)
	{
#ifdef DBDC_MODE_CERT
#ifndef WIFI_MULTI_PROFILE
		if (strncmp(setPSK->intf,"apcli1", 6) == 0)
		{
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliWirelessMode=14");
		}
#endif /* WIFI_MULTI_PROFILE */
#endif /* DBDC_MODE_CERT */

	   //TODO : maybe need to check encpType for detail
	   if(strcasecmp(setPSK->keyMgmtType, "wpa2-wpa-psk") == 0) //for AutoMix Mode for TGn 5.2.53 Item. (This cmd need driver automode support)
	   {
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEnable=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliAuthMode=WPAPSKWPA2PSK");
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEncrypType=TKIPAES"); //actually auto mode don't need to set this
	   }
	   else if(strcasecmp(setPSK->keyMgmtType, "wpa") == 0)
	   {
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEnable=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliAuthMode=WPAPSK");
			wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEncrypType=TKIP");
	   }
	   else // wpa2
	   {
		wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEnable=0");
		wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliAuthMode=WPA2PSK");
		wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEncrypType=AES");
	}

	}
	else
	{
		//TODO : maybe need to check encpType for detail
		if(strcasecmp(setPSK->keyMgmtType, "wpa2-wpa-psk") == 0) //for AutoMix Mode for TGn 5.2.53 Item. (This cmd need driver automode support)
		{
			wfa_driver_mtk_set_cmd(setPSK->intf, "AuthMode=WPAPSKWPA2PSK");
			wfa_driver_mtk_set_cmd(setPSK->intf, "EncrypType=AES"); //actually auto mode don't need to set this
		}
		else if(strcasecmp(setPSK->keyMgmtType, "wpa") == 0)
		{
			wfa_driver_mtk_set_cmd(setPSK->intf, "AuthMode=WPAPSK");
			wfa_driver_mtk_set_cmd(setPSK->intf, "EncrypType=TKIP");
		}
		else //wpa2 case
		{
		wfa_driver_mtk_set_cmd(setPSK->intf, "AuthMode=WPA2PSK");
		wfa_driver_mtk_set_cmd(setPSK->intf, "EncrypType=AES");
	}
	}


#if 1 //add for PMF check, need to set the  the follwing cmd after the security cmds, otherwise we need to preset the profile.

/*
	iwpriv ra0 set PMFSHA256=1 (set the AKM support SHA256)
	iwpriv ra0 set PMFMFPC=1 (enable the PMF)
	iwpriv ra0 set PMFMFPR=1 (set the the PMF to required)

*/
   sleep(1);

   /* if PMF enable */
   if(setPSK->pmf == WFA_ENABLED || setPSK->pmf == WFA_OPTIONAL)
   {
  
		if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)
			sprintf(gCmdStr, "ApCliPMFMFPC=1");
		else
	   		sprintf(gCmdStr, "PMFMFPC=1");
		
	   wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
		//clear Required for when DUT run PMF 5.2 items, then the PMFR is always keep, not clear in following items.
		if(strncmp(setPSK->intf,"apcli", 5) == 0)
			sprintf(gCmdStr, "ApCliPMFMFPR=0");
		else
	   		sprintf(gCmdStr, "PMFMFPR=0");
		
	   	wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
		
   }
   else if(setPSK->pmf == WFA_REQUIRED)
   {
  
		if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)
		{
			sprintf(gCmdStr, "ApCliPMFMFPC=1");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "ApCliPMFMFPR=1");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
		}
		else
		{
			sprintf(gCmdStr, "PMFMFPC=1");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "PMFMFPR=1");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
		}  
		
   } 
   else if(setPSK->pmf == WFA_F_REQUIRED)
   {
     
		if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)
			sprintf(gCmdStr, "ApCliPMFMFPR=1");
		else
 	   		sprintf(gCmdStr, "PMFMFPR=1");
	
	   wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
   } 
   else if(setPSK->pmf == WFA_F_DISABLED)
   {
  
		if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
		)			
		{
			sprintf(gCmdStr, "ApCliPMFSHA256=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "ApCliPMFMFPC=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "ApCliPMFMFPR=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);  
		}
		else
		{
			sprintf(gCmdStr, "PMFSHA256=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "PMFMFPC=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "PMFMFPR=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);  
		}
   } 
   else
   {   /* Disable PMF */
  
	   if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	   )
	   {
		   sprintf(gCmdStr, "ApCliPMFSHA256=0");
		   wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
	   
		   sprintf(gCmdStr, "ApCliPMFMFPC=0");
		   wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
	   
		   sprintf(gCmdStr, "ApCliPMFMFPR=0");
		   wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);  
	   }
	   else
	   {
			sprintf(gCmdStr, "PMFSHA256=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "PMFMFPC=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

			sprintf(gCmdStr, "PMFMFPR=0");
			wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
	   	}
   }

#endif
  
	if(strncmp(setPSK->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	)
	{
		sprintf(gCmdStr, "ApCliSsid=%s", setPSK->ssid);
		wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

		sleep(1);
		sprintf(gCmdStr, "ApCliWPAPSK=%s", setPSK->passphrase); 
		wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
		wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliEnable=1");

#ifdef DRIVER_SCAN_TRIGGER
		sleep(2);
		/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
		wfa_driver_mtk_set_cmd(setPSK->intf, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */
	}
	else
	{
		sprintf(gCmdStr, "SSID=%s", setPSK->ssid);
		wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);

		sleep(1);
		sprintf(gCmdStr, "WPAPSK=%s", setPSK->passphrase);
		wfa_driver_mtk_set_cmd(setPSK->intf, gCmdStr);
	}
#else /* MTK_PRIVATE_IOCTL */
#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_psk %s %s %s", setPSK->intf, setPSK->ssid, setPSK->passphrase);
    sret = system(gCmdStr);
#else
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", setPSK->intf, setPSK->ssid);
    sret = system(gCmdStr);

    if(strcasecmp(setPSK->keyMgmtType, "wpa2-sha256") == 0)
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA2-SHA256", setPSK->intf);
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick it supported.
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", setPSK->intf);
    }
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2-psk") == 0)
    {
	sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-PSK", setPSK->intf);
    }
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2-ft") == 0)
    {
	sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt FT-PSK", setPSK->intf);
    }
    else if (strcasecmp(setPSK->keyMgmtType, "wpa2-wpa-psk") == 0)
    {
	sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-PSK", setPSK->intf);
    }
    else
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-PSK", setPSK->intf);

    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 psk '\"%s\"'", setPSK->intf, setPSK->passphrase);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", setPSK->intf);
    sret = system(gCmdStr);

    /* if PMF enable */
    if(setPSK->pmf == WFA_ENABLED || setPSK->pmf == WFA_OPTIONAL)
    {

    }
    else if(setPSK->pmf == WFA_REQUIRED)
    {

    }
    else if(setPSK->pmf == WFA_F_REQUIRED)
    {

    }
    else if(setPSK->pmf == WFA_F_DISABLED)
    {

    }
    else
    {
        /* Disable PMF */

    }

#endif
#endif /* !MTK_PRIVATE_IOCTL */
#endif

    setPskResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PSK_RESP_TLV, 4, (BYTE *)setPskResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaGetInfo():
 * Get vendor specific information in name/value pair by a wireless I/F.
 */
int wfaStaGetInfo(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    dutCommand_t *getInfo = (dutCommand_t *)caCmdBuf;

    /*
     * Normally this is called to retrieve the vendor information
     * from a interface, no implement yet
     */
    sprintf(infoResp.cmdru.info, "interface,%s,vendor,XXX,cardtype,802.11a/b/g", getInfo->intf);

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_INFO_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetEapTTLS():
 *   This is to set
 *   1. ssid
 *   2. username
 *   3. passwd
 *   4. encrypType - tkip or aes-ccmp
 *   5. keyManagementType - wpa or wpa2
 *   6. trustedRootCA
 */
int wfaStaSetEapTTLS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapTTLS_t *setTTLS = (caStaSetEapTTLS_t *)caCmdBuf;
    char *ifname = setTTLS->intf;
    dutCmdResponse_t *setEapTtlsResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapttls %s %s %s %s %s", ifname, setTTLS->ssid, setTTLS->username, setTTLS->passwd, setTTLS->trustedRootCA);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setTTLS->ssid);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"%s\"'", ifname, setTTLS->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 password '\"%s\"'", ifname, setTTLS->passwd);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    sret = system(gCmdStr);

    /* This may not need to set. if it is not set, default to take all */
//   sprintf(cmdStr, "wpa_cli -i %s set_network 0 pairwise '\"%s\"", ifname, setTTLS->encrptype);
    if(strcasecmp(setTTLS->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa") == 0)
    {

    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2") == 0)
    {
        // to take all and device to pick one it supported
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap TTLS", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ca_cert '\"%s/%s\"'", ifname, CERTIFICATES_PATH, setTTLS->trustedRootCA);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 proto WPA", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase2 '\"auth=MSCHAPV2\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setEapTtlsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPTTLS_RESP_TLV, 4, (BYTE *)setEapTtlsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetEapSIM():
 *   This is to set
 *   1. ssid
 *   2. user name
 *   3. passwd
 *   4. encrypType - tkip or aes-ccmp
 *   5. keyMangementType - wpa or wpa2
 */
int wfaStaSetEapSIM(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapSIM_t *setSIM = (caStaSetEapSIM_t *)caCmdBuf;
    char *ifname = setSIM->intf;
    dutCmdResponse_t *setEapSimResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapsim %s %s %s %s", ifname, setSIM->ssid, setSIM->username, setSIM->encrptype);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setSIM->ssid);
    sret = system(gCmdStr);


    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"%s\"'", ifname, setSIM->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 pairwise '\"%s\"'", ifname, setSIM->encrptype);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap SIM", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 proto WPA", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);

    if(strcasecmp(setSIM->keyMgmtType, "wpa2-sha256") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-SHA256", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2-eap") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2-ft") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-FT", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

#endif

    setEapSimResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPSIM_RESP_TLV, 4, (BYTE *)setEapSimResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetPEAP()
 *   This is to set
 *   1. ssid
 *   2. user name
 *   3. passwd
 *   4. encryType - tkip or aes-ccmp
 *   5. keyMgmtType - wpa or wpa2
 *   6. trustedRootCA
 *   7. innerEAP
 *   8. peapVersion
 */
int wfaStaSetPEAP(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapPEAP_t *setPEAP = (caStaSetEapPEAP_t *)caCmdBuf;
    char *ifname = setPEAP->intf;
    dutCmdResponse_t *setPeapResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_peap %s %s %s %s %s %s %i %s", ifname, setPEAP->ssid, setPEAP->username,
            setPEAP->passwd, setPEAP->trustedRootCA,
            setPEAP->encrptype, setPEAP->peapVersion,
            setPEAP->innerEAP);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setPEAP->ssid);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap PEAP", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 anonymous_identity '\"anonymous\"' ", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"%s\"'", ifname, setPEAP->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 password '\"%s\"'", ifname, setPEAP->passwd);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ca_cert '\"%s/%s\"'", ifname, CERTIFICATES_PATH, setPEAP->trustedRootCA);
    sret = system(gCmdStr);

    if(strcasecmp(setPEAP->keyMgmtType, "wpa2-sha256") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-SHA256", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2-eap") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2-ft") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-FT", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase1 '\"peaplabel=%i\"'", ifname, setPEAP->peapVersion);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase2 '\"auth=%s\"'", ifname, setPEAP->innerEAP);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setPeapResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PEAP_RESP_TLV, 4, (BYTE *)setPeapResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetUAPSD()
 *    This is to set
 *    1. acBE
 *    2. acBK
 *    3. acVI
 *    4. acVO
 */
int wfaStaSetUAPSD(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *setUAPSDResp = &gGenericResp;
   caStaSetUAPSD_t *setUAPSD = (caStaSetUAPSD_t *)caCmdBuf;
   BYTE acBE=1;
   BYTE acBK=1;
   BYTE acVO=1;
   BYTE acVI=1;
   BYTE APSDCapable;

   DPRINT_INFO(WFA_OUT, "Entering wfaStaSetUAPSD ...\n");
#if 0 /* used for only one specific device, need to update to reflect yours */
    caStaSetUAPSD_t *setUAPSD = (caStaSetUAPSD_t *)caCmdBuf;
    char *ifname = setUAPSD->intf;
    char tmpStr[10];
    char line[100];
    char *pathl="/etc/Wireless/RT61STA";
    BYTE acBE=1;
    BYTE acBK=1;
    BYTE acVO=1;
    BYTE acVI=1;
    BYTE APSDCapable;
    FILE *pipe;

    /*
     * A series of setting need to be done before doing WMM-PS
     * Additional steps of configuration may be needed.
     */

    /*
     * bring down the interface
     */
    sprintf(gCmdStr, "ifconfig %s down",ifname);
    sret = system(gCmdStr);
    /*
     * Unload the Driver
     */
    sprintf(gCmdStr, "rmmod rt61");
    sret = system(gCmdStr);
#ifndef WFA_WMM_AC
    if(setUAPSD->acBE != 1)
        acBE=setUAPSD->acBE = 0;
    if(setUAPSD->acBK != 1)
        acBK=setUAPSD->acBK = 0;
    if(setUAPSD->acVO != 1)
        acVO=setUAPSD->acVO = 0;
    if(setUAPSD->acVI != 1)
        acVI=setUAPSD->acVI = 0;
#else
    acBE=setUAPSD->acBE;
    acBK=setUAPSD->acBK;
    acVO=setUAPSD->acVO;
    acVI=setUAPSD->acVI;
#endif

    APSDCapable = acBE||acBK||acVO||acVI;
    /*
     * set other AC parameters
     */

    sprintf(tmpStr,"%d;%d;%d;%d",setUAPSD->acBE,setUAPSD->acBK,setUAPSD->acVI,setUAPSD->acVO);
    sprintf(gCmdStr, "sed -e \"s/APSDCapable=.*/APSDCapable=%d/g\" -e \"s/APSDAC=.*/APSDAC=%s/g\" %s/rt61sta.dat >/tmp/wfa_tmp",APSDCapable,tmpStr,pathl);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "mv /tmp/wfa_tmp %s/rt61sta.dat",pathl);
    sret = system(gCmdStr);
    pipe = popen("uname -r", "r");
    /* Read into line the output of uname*/
    fscanf(pipe,"%s",line);
    pclose(pipe);

    /*
     * load the Driver
     */
    sprintf(gCmdStr, "insmod /lib/modules/%s/extra/rt61.ko",line);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "ifconfig %s up",ifname);
    sret = system(gCmdStr);
#endif

	/* MT7615 is the 1st project to use this, other projects can leverage this
	or create a new one to meet its won requirement */
/* MT7615 */
	APSDCapable = acBE||acBK||acVO||acVI;

	/* WMMCapable */
	sprintf(gCmdStr, "WmmCapable=%d", APSDCapable);

	if(wfa_driver_mtk_set_cmd(setUAPSD->intf, gCmdStr) < 0)
	{
		DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
	}
	else
	{
		DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
	}

	/* UAPSDCapable */
	sprintf(gCmdStr, "UAPSDCapable=%d", APSDCapable);

	if(wfa_driver_mtk_set_cmd(setUAPSD->intf, gCmdStr) < 0)
	{
		DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
	}
	else
	{
		DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
	}

	/* APSDAC */
	sprintf(gCmdStr, "APSDAC=%d:%d:%d:%d", setUAPSD->acBE,setUAPSD->acBK,setUAPSD->acVI,setUAPSD->acVO);

	if(wfa_driver_mtk_set_cmd(setUAPSD->intf, gCmdStr) < 0)
	{
		DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
	}
	else
	{
		DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
	}

	/* MaxSPLength */
	sprintf(gCmdStr, "MaxSPLength=%d", setUAPSD->maxSPLength);

	if(wfa_driver_mtk_set_cmd(setUAPSD->intf, gCmdStr) < 0)
	{
		DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
	}
	else
	{
		DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
	}
/* MT7615 End*/

    setUAPSDResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_UAPSD_RESP_TLV, 4, (BYTE *)setUAPSDResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wfaDeviceGetInfo(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *dutCmd = (dutCommand_t *)caCmdBuf;
    caDevInfo_t *devInfo = &dutCmd->cmdsu.dev;
    dutCmdResponse_t *infoResp = &gGenericResp;
    /*a vendor can fill in the proper info or anything non-disclosure */
    caDeviceGetInfoResp_t dinfo = {"WFA Lab", "DemoUnit", WFA_SYSTEM_VER};

    DPRINT_INFO(WFA_OUT, "Entering wfaDeviceGetInfo ...\n");

    if(devInfo->fw == 0)
        memcpy(&infoResp->cmdru.devInfo, &dinfo, sizeof(caDeviceGetInfoResp_t));
    else
    {
        // Call internal API to pull the version ID */
        unsigned char model[32], drv_verion[32] = {"0.0.0.0"};
        unsigned int chip_id = 0;

#ifdef MTK_PRIVATE_IOCTL
        wfa_driver_mtk_get_oid(dutCmd->intf,
                               OID_MTK_CHIP_ID,
                               (unsigned char *) &chip_id,
                               sizeof(unsigned int));
        wfa_driver_mtk_get_oid(dutCmd->intf,
                               OID_MTK_DRVER_VERSION,
                               drv_verion,
                               sizeof(drv_verion));
#endif /* MTK_PRIVATE_IOCTL */
        sprintf((char *) model, "MT%04x", chip_id);
        infoResp->cmdru.devInfo.firmware[0] = '\0';
        memcpy(infoResp->cmdru.devInfo.vendor, "Mediatek", 8);
        memcpy(infoResp->cmdru.devInfo.model, model, 6);
        memcpy(infoResp->cmdru.devInfo.version, drv_verion, 8);
    }

    infoResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_DEVICE_GET_INFO_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;

}

/*
 * This funciton is to retrieve a list of interfaces and return
 * the list back to Agent control.
 * ********************************************************************
 * Note: We intend to make this WLAN interface name as a hardcode name.
 * Therefore, for a particular device, you should know and change the name
 * for that device while doing porting. The MACRO "WFA_STAUT_IF" is defined in
 * the file "inc/wfa_ca.h". If the device OS is not linux-like, this most
 * likely is hardcoded just for CAPI command responses.
 * *******************************************************************
 *
 */
int wfaDeviceListIF(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *infoResp = &gGenericResp;
    dutCommand_t *ifList = (dutCommand_t *)caCmdBuf;
    caDeviceListIFResp_t *ifListResp = &infoResp->cmdru.ifList;

    DPRINT_INFO(WFA_OUT, "Entering wfaDeviceListIF ...\n");
   DPRINT_INFO(WFA_OUT, "interface %s !!!\n", gnetIf);

    switch(ifList->cmdsu.iftype)
    {
    case IF_80211:
        infoResp->status = STATUS_COMPLETE;
        ifListResp->iftype = IF_80211;
        //strcpy(ifListResp->ifs[0], WFA_STAUT_IF);
        strcpy(ifListResp->ifs[0], gnetIf);
        strcpy(ifListResp->ifs[1], "NULL");
        strcpy(ifListResp->ifs[2], "NULL");
        break;
    case IF_ETH:
        infoResp->status = STATUS_COMPLETE;
        ifListResp->iftype = IF_ETH;
        strcpy(ifListResp->ifs[0], "eth0");
        strcpy(ifListResp->ifs[1], "NULL");
        strcpy(ifListResp->ifs[2], "NULL");
        break;
    default:
    {
        infoResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_DEVICE_LIST_IF_RESP_TLV, 4, (BYTE *)infoResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        return WFA_SUCCESS;
    }
    }

    wfaEncodeTLV(WFA_DEVICE_LIST_IF_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

int wfaStaDebugSet(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *debugResp = &gGenericResp;
    dutCommand_t *debugSet = (dutCommand_t *)caCmdBuf;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaDebugSet ...\n");

    if(debugSet->cmdsu.dbg.state == 1) /* enable */
        wfa_defined_debug |= debugSet->cmdsu.dbg.level;
    else
        wfa_defined_debug = (~debugSet->cmdsu.dbg.level & wfa_defined_debug);

    debugResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_INFO_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)debugResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);


    return WFA_SUCCESS;
}


/*
 *   wfaStaGetBSSID():
 *     This function is to retrieve BSSID of a specific wireless I/F.
 */
int wfaStaGetBSSID(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
#ifdef MTK_PRIVATE_IOCTL
   dutCommand_t *dutCmd = (dutCommand_t *)caCmdBuf;
   char *ifname = dutCmd->intf;
   unsigned char bssid[8];
#else /* MTK_PRIVATE_IOCTL */
    char string[64];
#endif /* !MTK_PRIVATE_IOCTL */
    char *str;
    FILE *tmpfd;
    dutCmdResponse_t *bssidResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaGetBSSID ...\n");

#ifdef MTK_PRIVATE_IOCTL
   memset(&bssid[0], 0, sizeof(bssid));
   wfa_driver_mtk_get_oid(ifname, OID_802_11_BSSID, &bssid[0], sizeof(bssid));
   
   sprintf(bssidResp->cmdru.bssid, "%02x:%02x:%02x:%02x:%02x:%02x", 
       bssid[0],
   	   bssid[1],
   	   bssid[2],
       bssid[3],
   	   bssid[4],
   	   bssid[5]);
   bssidResp->status = STATUS_COMPLETE;
   
   DPRINT_INFO(WFA_OUT, "wfaStaGetBSSID = %s\n", bssidResp->cmdru.bssid);
#else /* MTK_PRIVATE_IOCTL */
    /* retrieve the BSSID */
    sprintf(gCmdStr, "wpa_cli status > /tmp/bssid.txt");

    sret = system(gCmdStr);

    tmpfd = fopen("/tmp/bssid.txt", "r+");
    if(tmpfd == NULL)
    {
        bssidResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_BSSID_RESP_TLV, 4, (BYTE *)bssidResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    for(;;)
    {
        if(fscanf(tmpfd, "%s", string) == EOF)
        {
            bssidResp->status = STATUS_COMPLETE;
            strcpy(bssidResp->cmdru.bssid, "00:00:00:00:00:00");
            break;
        }

        if(strncmp(string, "bssid", 5) == 0)
        {
            str = strtok(string, "=");
            str = strtok(NULL, "=");
            if(str != NULL)
            {
                strcpy(bssidResp->cmdru.bssid, str);
                bssidResp->status = STATUS_COMPLETE;
                break;
            }
        }
    }

    fclose(tmpfd);
#endif /* !MTK_PRIVATE_IOCTL */
 
    wfaEncodeTLV(WFA_STA_GET_BSSID_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)bssidResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetIBSS()
 *    This is to set
 *    1. ssid
 *    2. channel
 *    3. encrypType - none or wep
 *    optional
 *    4. key1
 *    5. key2
 *    6. key3
 *    7. key4
 *    8. activeIndex - 1, 2, 3, or 4
 */
int wfaStaSetIBSS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetIBSS_t *setIBSS = (caStaSetIBSS_t *)caCmdBuf;
    dutCmdResponse_t *setIbssResp = &gGenericResp;
    int i;

#ifdef MTK_PRIVATE_IOCTL
   DPRINT_INFO(WFA_OUT, "entering wfaSetIBSS ...\n");
	return 0;

   wfa_driver_mtk_set_cmd(setIBSS->intf, "NetworkType=Adhoc");
   wfa_driver_mtk_set_cmd(setIBSS->intf, "AuthMode=OPEN");

   if(setIBSS->encpType == ENCRYPT_WEP)
   {
	  wfa_driver_mtk_set_cmd(setIBSS->intf, "EncrypType=WEP");

	  for(i=0; i<4; i++)
      {
         if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
         {
             sprintf(gCmdStr, "Key%i=%s", i+1, setIBSS->keys[i]);
			 wfa_driver_mtk_set_cmd(setIBSS->intf, gCmdStr);
         }
      } 

      i = setIBSS->activeKeyIdx;
      if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
      {
         sprintf(gCmdStr, "DefaultKeyID=%i", setIBSS->activeKeyIdx);
         wfa_driver_mtk_set_cmd(setIBSS->intf, gCmdStr);
      }
   }
   else
   {
	  wfa_driver_mtk_set_cmd(setIBSS->intf, "EncrypType=NONE");
   }

   sprintf(gCmdStr, "Channel=%i", setIBSS->channel);
   wfa_driver_mtk_set_cmd(setIBSS->intf, gCmdStr);

   sprintf(gCmdStr, "SSID=%s", setIBSS->ssid); 
   wfa_driver_mtk_set_cmd(setIBSS->intf, gCmdStr);
#else /* MTK_PRIVATE_IOCTL */

    /*
     * disable the network first
     */
    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", setIBSS->intf);
    sret = system(gCmdStr);

    /*
     * set SSID
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", setIBSS->intf, setIBSS->ssid);
    sret = system(gCmdStr);

    /*
     * Set channel for IBSS
     */
    sprintf(gCmdStr, "iwconfig %s channel %i", setIBSS->intf, setIBSS->channel);
    sret = system(gCmdStr);

    /*
     * Tell the supplicant for IBSS mode (1)
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 mode 1", setIBSS->intf);
    sret = system(gCmdStr);

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt NONE", setIBSS->intf);
    sret = system(gCmdStr);

    if(setIBSS->encpType == 1)
    {
        for(i=0; i<4; i++)
        {
            if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
            {
                sprintf(gCmdStr, "wpa_cli -i %s set_network 0 wep_key%i \"%s\"",
                        setIBSS->intf, i, setIBSS->keys[i]);
                sret = system(gCmdStr);
            }
        }

        i = setIBSS->activeKeyIdx;
        if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
        {
            sprintf(gCmdStr, "wpa_cli -i %s set_network 0 wep_tx_keyidx %i",
                    setIBSS->intf, setIBSS->activeKeyIdx);
            sret = system(gCmdStr);
        }
    }

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", setIBSS->intf);
    sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */

    setIbssResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_IBSS_RESP_TLV, 4, (BYTE *)setIbssResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 *  wfaSetMode():
 *  The function is to set the wireless interface with a given mode (possible
 *  adhoc)
 *  Input parameters:
 *    1. I/F
 *    2. ssid
 *    3. mode adhoc or managed
 *    4. encType
 *    5. channel
 *    6. key(s)
 *    7. active  key
 */
int wfaStaSetMode(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetMode_t *setmode = (caStaSetMode_t *)caCmdBuf;
    dutCmdResponse_t *SetModeResp = &gGenericResp;
    int i;

#ifdef MTK_PRIVATE_IOCTL
   DPRINT_INFO(WFA_OUT, "entering wfaSetMode ...\n");



if(strncmp(setmode->intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON		
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif /* TGAC_DAEMON */
)
{
   if(setmode->mode == 1)
   {
   	DPRINT_INFO(WFA_OUT, "wfaSetMode = Adhoc return\n");
      wfa_driver_mtk_set_cmd(setmode->intf, "NetworkType=Adhoc");
	  return 0;
   }
   
  wfa_driver_mtk_set_cmd(setmode->intf, "ApCliEnable=0");

   if(setmode->encpType == ENCRYPT_WEP)
   {
      for(i=0; i<4; i++)
      {
         if(setmode->keys[i][0] != '\0')
         {
             sprintf(gCmdStr, "ApCliKey%i=%s",
                   i+1, setmode->keys[i]);
			 wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
         }
      }

      /* set active key */
      i = setmode->activeKeyIdx;
      if(setmode->keys[i][0] != '\0')
      {
          sprintf(gCmdStr, "ApCliDefaultKeyID=%i", setmode->activeKeyIdx);
		  wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
      }

   }
   /*
    * Set channel for IBSS
    */
    if(setmode->channel)
    {
      sprintf(gCmdStr, "Channel=%i", setmode->channel);
      wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
    }

   /*
    * set SSID
    */
   sprintf(gCmdStr, "ApCliSsid=%s", setmode->ssid);
   wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);

   	sprintf(gCmdStr, "ApCliEnable=1");
	wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);

	wfa_driver_mtk_set_cmd(setmode->intf, "ApCliEnable=1");

#ifdef DRIVER_SCAN_TRIGGER
		sleep(2);
		/* 3 : means scan trigger by driver, 2 : means scan trigger by user */
		wfa_driver_mtk_set_cmd(setmode->intf, "ApCliAutoConnect=3");
#endif /* DRIVER_SCAN_TRIGGER */
	
} else
{
   DPRINT_INFO(WFA_OUT, "entering wfaSetMode ...\n");

   if(setmode->mode == 1)
      wfa_driver_mtk_set_cmd(setmode->intf, "NetworkType=Adhoc");
   else
      wfa_driver_mtk_set_cmd(setmode->intf, "NetworkType=Infra");

   if(setmode->encpType == ENCRYPT_WEP)
   {
      for(i=0; i<4; i++)
      {
         if(setmode->keys[i][0] != '\0')
         {
             sprintf(gCmdStr, "Key%i=%s",
                   i+1, setmode->keys[i]);
			 wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
         }
      }

      /* set active key */
      i = setmode->activeKeyIdx;
      if(setmode->keys[i][0] != '\0')
      {
          sprintf(gCmdStr, "DefaultKeyID=%i", setmode->activeKeyIdx);
		  wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
      }

   }
   /*
    * Set channel for IBSS
    */
    if(setmode->channel)
    {
      sprintf(gCmdStr, "Channel=%i", setmode->channel);
      wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
    }

   /*
    * set SSID
    */
   sprintf(gCmdStr, "SSID=%s", setmode->ssid);
   wfa_driver_mtk_set_cmd(setmode->intf, gCmdStr);
}
#else /* MTK_PRIVATE_IOCTL */
    /*
     * bring down the interface
     */
    sprintf(gCmdStr, "ifconfig %s down",setmode->intf);
    sret = system(gCmdStr);

    /*
     * distroy the interface
     */
    sprintf(gCmdStr, "wlanconfig %s destroy",setmode->intf);
    sret = system(gCmdStr);

    /*
     * re-create the interface with the given mode
     */
    if(setmode->mode == 1)
        sprintf(gCmdStr, "wlanconfig %s create wlandev wifi0 wlanmode adhoc",setmode->intf);
    else
        sprintf(gCmdStr, "wlanconfig %s create wlandev wifi0 wlanmode managed",setmode->intf);

    sret = system(gCmdStr);
    if(setmode->encpType == ENCRYPT_WEP)
    {
        int j = setmode->activeKeyIdx;
        for(i=0; i<4; i++)
        {
            if(setmode->keys[i][0] != '\0')
            {
                sprintf(gCmdStr, "iwconfig  %s key  s:%s",
                        setmode->intf, setmode->keys[i]);
                sret = system(gCmdStr);
            }
            /* set active key */
            if(setmode->keys[j][0] != '\0')
                sprintf(gCmdStr, "iwconfig  %s key  s:%s",
                        setmode->intf, setmode->keys[j]);
            sret = system(gCmdStr);
        }

    }
    /*
     * Set channel for IBSS
     */
    if(setmode->channel)
    {
        sprintf(gCmdStr, "iwconfig %s channel %i", setmode->intf, setmode->channel);
        sret = system(gCmdStr);
    }


    /*
     * set SSID
     */
    sprintf(gCmdStr, "iwconfig %s essid %s", setmode->intf, setmode->ssid);
    sret = system(gCmdStr);

    /*
     * bring up the interface
     */
    sprintf(gCmdStr, "ifconfig %s up",setmode->intf);
    sret = system(gCmdStr);
#endif /* !MTK_PRIVATE_IOCTL */

    SetModeResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_MODE_RESP_TLV, 4, (BYTE *)SetModeResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetPwrSave(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetPwrSave_t *setps = (caStaSetPwrSave_t *)caCmdBuf;
    dutCmdResponse_t *SetPSResp = &gGenericResp;

#if 0
    sprintf(gCmdStr, "iwconfig %s power %s", setps->intf, setps->mode);
    sret = system(gCmdStr);
#else
    DPRINT_INFO(WFA_OUT, "Entering wfaStaSetPwrSave ...\n");
    if (strcmp(setps->mode, "off") == 0)
    {
        sprintf(gCmdStr, "PSMode=CAM");

        if(wfa_driver_mtk_set_cmd(setps->intf, gCmdStr) < 0)
        {
            DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
        }
        else
        {
            DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
        }
    }
    else
    {
		/* add HE-5.60.1 workaround */
		if((strcmp(gHeCfgSet_t.ssid,"HE-5.60.1_24G") == 0) ||
			(strcmp(gHeCfgSet_t.ssid,"HE-5.60.1_5G") == 0) ||
			(strcmp(gHeCfgSet_t.ssid,"HE-5.60.1") == 0))
		{
			DPRINT_INFO(WFA_OUT, "TWT will apply PSM by itself\n");
		}
		else if(strstr(gHeCfgSet_t.ssid,"HE-5.72.1") != NULL)
		{
			DPRINT_INFO(WFA_OUT, "Meet 5.72 case\n");
			sprintf(gCmdStr, "PSMode=Fast_PSP");
			if(wfa_driver_mtk_set_cmd(setps->intf, gCmdStr) < 0)
			{
				DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
			}
			else
			{
				DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
			}
		}
		else
		{
			DPRINT_INFO(WFA_OUT, "Other case\n");
			sprintf(gCmdStr, "PSMode=Legacy_PSP");
			if(wfa_driver_mtk_set_cmd(setps->intf, gCmdStr) < 0)
			{
				DPRINT_ERR(WFA_ERR, "Fail to %s\n", gCmdStr);
			}
			else
			{
				DPRINT_INFO(WFA_OUT, "Ok to %s\n", gCmdStr);
			}
		}
	}
#endif

    SetPSResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PWRSAVE_RESP_TLV, 4, (BYTE *)SetPSResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaUpload(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaUpload_t *upload = &((dutCommand_t *)caCmdBuf)->cmdsu.upload;
    dutCmdResponse_t *upLoadResp = &gGenericResp;
    caStaUploadResp_t *upld = &upLoadResp->cmdru.uld;

    if(upload->type == WFA_UPLOAD_VHSO_RPT)
    {
        int rbytes;
        /*
         * if asked for the first packet, always to open the file
         */
        if(upload->next == 1)
        {
            if(e2efp != NULL)
            {
                fclose(e2efp);
                e2efp = NULL;
            }

            e2efp = fopen(e2eResults, "r");
        }

        if(e2efp == NULL)
        {
            upLoadResp->status = STATUS_ERROR;
            wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, 4, (BYTE *)upLoadResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + 4;
            return WFA_FAILURE;
        }

        rbytes = fread(upld->bytes, 1, 256, e2efp);

        if(rbytes < 256)
        {
            /*
             * this means no more bytes after this read
             */
            upld->seqnum = 0;
            fclose(e2efp);
            e2efp=NULL;
        }
        else
        {
            upld->seqnum = upload->next;
        }

        upld->nbytes = rbytes;

        upLoadResp->status = STATUS_COMPLETE;
        wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)upLoadResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
    }
    else
    {
        upLoadResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, 4, (BYTE *)upLoadResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;
    }

    return WFA_SUCCESS;
}
/*
 * wfaStaSetWMM()
 *  TO be ported on a specific plaform for the DUT
 *  This is to set the WMM related parameters at the DUT.
 *  Currently the function is used for GROUPS WMM-AC and WMM general configuration for setting RTS Threshhold, Fragmentation threshold and wmm (ON/OFF)
 *  It is expected that this function will set all the WMM related parametrs for a particular GROUP .
 */
int wfaStaSetWMM(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
#ifdef WFA_WMM_AC
    caStaSetWMM_t *setwmm = (caStaSetWMM_t *)caCmdBuf;
    char *ifname = setwmm->intf;
    dutCmdResponse_t *setwmmResp = &gGenericResp;

    switch(setwmm->group)
    {
    case GROUP_WMMAC:
        if (setwmm->send_trig)
        {
            int Sockfd;
            struct sockaddr_in psToAddr;
            unsigned int TxMsg[512];

            Sockfd = wfaCreateUDPSock(setwmm->dipaddr, 12346);
            memset(&psToAddr, 0, sizeof(psToAddr));
            psToAddr.sin_family = AF_INET;
            psToAddr.sin_addr.s_addr = inet_addr(setwmm->dipaddr);
            psToAddr.sin_port = htons(12346);


            switch (setwmm->trig_ac)
            {
            case WMMAC_AC_VO:
                wfaTGSetPrio(Sockfd, 7);
                create_apts_msg(APTS_CK_VO, TxMsg, 0);
                printf("\r\nSending AC_VO trigger packet\n");
                break;

            case WMMAC_AC_VI:
                wfaTGSetPrio(Sockfd, 5);
                create_apts_msg(APTS_CK_VI, TxMsg, 0);
                printf("\r\nSending AC_VI trigger packet\n");
                break;

            case WMMAC_AC_BK:
                wfaTGSetPrio(Sockfd, 2);
                create_apts_msg(APTS_CK_BK, TxMsg, 0);
                printf("\r\nSending AC_BK trigger packet\n");
                break;

            default:
            case WMMAC_AC_BE:
                wfaTGSetPrio(Sockfd, 0);
                create_apts_msg(APTS_CK_BE, TxMsg, 0);
                printf("\r\nSending AC_BE trigger packet\n");
                break;
            }

            sendto(Sockfd, TxMsg, 256, 0, (struct sockaddr *)&psToAddr,
                   sizeof(struct sockaddr));
            close(Sockfd);
            usleep(1000000);
        }
        else if (setwmm->action == WMMAC_ADDTS)
        {
            printf("ADDTS AC PARAMS: dialog id: %d, TID: %d, "
                   "DIRECTION: %d, PSB: %d, UP: %d, INFOACK: %d BURST SIZE DEF: %d"
                   "Fixed %d, MSDU Size: %d, Max MSDU Size %d, "
                   "MIN SERVICE INTERVAL: %d, MAX SERVICE INTERVAL: %d, "
                   "INACTIVITY: %d, SUSPENSION %d, SERVICE START TIME: %d, "
                   "MIN DATARATE: %d, MEAN DATA RATE: %d, PEAK DATA RATE: %d, "
                   "BURSTSIZE or MSDU Aggreg: %d, DELAY BOUND: %d, PHYRATE: %d, SPLUSBW: %f, "
                   "MEDIUM TIME: %d, ACCESSCAT: %d\n",
                   setwmm->actions.addts.dialog_token,
                   setwmm->actions.addts.tspec.tsinfo.TID,
                   setwmm->actions.addts.tspec.tsinfo.direction,
                   setwmm->actions.addts.tspec.tsinfo.PSB,
                   setwmm->actions.addts.tspec.tsinfo.UP,
                   setwmm->actions.addts.tspec.tsinfo.infoAck,
                   setwmm->actions.addts.tspec.tsinfo.bstSzDef,
                   setwmm->actions.addts.tspec.Fixed,
                   setwmm->actions.addts.tspec.size,
                   setwmm->actions.addts.tspec.maxsize,
                   setwmm->actions.addts.tspec.min_srvc,
                   setwmm->actions.addts.tspec.max_srvc,
                   setwmm->actions.addts.tspec.inactivity,
                   setwmm->actions.addts.tspec.suspension,
                   setwmm->actions.addts.tspec.srvc_strt_tim,
                   setwmm->actions.addts.tspec.mindatarate,
                   setwmm->actions.addts.tspec.meandatarate,
                   setwmm->actions.addts.tspec.peakdatarate,
                   setwmm->actions.addts.tspec.burstsize,
                   setwmm->actions.addts.tspec.delaybound,
                   setwmm->actions.addts.tspec.PHYrate,
                   setwmm->actions.addts.tspec.sba,
                   setwmm->actions.addts.tspec.medium_time,
                   setwmm->actions.addts.accesscat);

            //tspec should be set here.

            sret = system(gCmdStr);
        }
        else if (setwmm->action == WMMAC_DELTS)
        {
            // send del tspec
        }

        setwmmResp->status = STATUS_COMPLETE;
        break;

    case GROUP_WMMCONF:
        sprintf(gCmdStr, "iwconfig %s rts %d",
                ifname,setwmm->actions.config.rts_thr);

        sret = system(gCmdStr);
        sprintf(gCmdStr, "iwconfig %s frag %d",
                ifname,setwmm->actions.config.frag_thr);

        sret = system(gCmdStr);
        sprintf(gCmdStr, "iwpriv %s wmmcfg %d",
                ifname, setwmm->actions.config.wmm);

        sret = system(gCmdStr);
        setwmmResp->status = STATUS_COMPLETE;
        break;

    default:
        DPRINT_ERR(WFA_ERR, "The group %d is not supported\n",setwmm->group);
        setwmmResp->status = STATUS_ERROR;
        break;

    }

    wfaEncodeTLV(WFA_STA_SET_WMM_RESP_TLV, 4, (BYTE *)setwmmResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
#endif

    return WFA_SUCCESS;
}

int wfaStaSendNeigReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *sendNeigReqResp = &gGenericResp;

    /*
     *  run your device to send NEIGREQ
     */

    sendNeigReqResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SEND_NEIGREQ_RESP_TLV, 4, (BYTE *)sendNeigReqResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetEapFAST(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapFAST_t *setFAST= (caStaSetEapFAST_t *)caCmdBuf;
    char *ifname = setFAST->intf;
    dutCmdResponse_t *setEapFastResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapfast %s %s %s %s %s %s", ifname, setFAST->ssid, setFAST->username,
            setFAST->passwd, setFAST->pacFileName,
            setFAST->innerEAP);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setFAST->ssid);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"%s\"'", ifname, setFAST->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 password '\"%s\"'", ifname, setFAST->passwd);
    sret = system(gCmdStr);

    if(strcasecmp(setFAST->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap FAST", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 pac_file '\"%s/%s\"'", ifname, CERTIFICATES_PATH,     setFAST->pacFileName);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 anonymous_identity '\"anonymous\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase1 '\"fast_provisioning=1\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase2 '\"auth=%s\"'", ifname,setFAST->innerEAP);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setEapFastResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPFAST_RESP_TLV, 4, (BYTE *)setEapFastResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetEapAKA(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapAKA_t *setAKA= (caStaSetEapAKA_t *)caCmdBuf;
    char *ifname = setAKA->intf;
    dutCmdResponse_t *setEapAkaResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapaka %s %s %s %s", ifname, setAKA->ssid, setAKA->username, setAKA->passwd);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i %s disable_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 ssid '\"%s\"'", ifname, setAKA->ssid);
    sret = system(gCmdStr);

    if(strcasecmp(setAKA->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli -i %s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 proto WPA2", ifname);
    sret = system(gCmdStr);
    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 proto CCMP", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 eap AKA", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 phase1 \"result_ind=1\"", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 identity '\"%s\"'", ifname, setAKA->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s set_network 0 password '\"%s\"'", ifname, setAKA->passwd);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i %s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setEapAkaResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPAKA_RESP_TLV, 4, (BYTE *)setEapAkaResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/* EAP-AKA' */
int wfaStaSetEapAKAPrime(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapAKAPrime_t *setAKAPrime = (caStaSetEapAKAPrime_t *)caCmdBuf;
    char *ifname = setAKAPrime->intf;
    dutCmdResponse_t *setEapAkaPrimeResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapakaprime %s %s %s %s", ifname, setAKAPrime->ssid, setAKAPrime->username, setAKAPrime->passwd);
    sret = system(gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli -i%s remove_network 0", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s add_network", ifname);
    sret = system(gCmdStr);

    if(strcasecmp(setAKAPrime->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setAKAPrime->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setAKAPrime->keyMgmtType, "wpa2-ft") == 0)
    {
 
    }
    else if(strcasecmp(setAKAPrime->keyMgmtType, "wpa") == 0)
    {
       sprintf(gCmdStr, "wpa_cli -i%s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setAKAPrime->keyMgmtType, "wpa2") == 0)
    {
      // take all and device to pick one which is supported.
      sprintf(gCmdStr, "wpa_cli -i%s set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else
    {
       // ??
       setEapAkaPrimeResp->status = STATUS_INVALID;
       strcpy(setEapAkaPrimeResp->cmdru.info, "invalid key management parameter");
       wfaEncodeTLV(WFA_STA_SET_EAPAKAPRIME_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)setEapAkaPrimeResp, respBuf);
       *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
       return WFA_FAILURE;
    }
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 pairwise CCMP", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 proto WPA2", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 eap AKA\\'", ifname);
    printf("eap command=%s\n", gCmdStr);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 phase1 '\"result_ind=1\"'", ifname);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 identity '\"%s\"'", ifname, setAKAPrime->username);
    printf("user name=%s\n", setAKAPrime->username);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 password '\"%s\"'", ifname, setAKAPrime->passwd);
    printf("password=%s\n", setAKAPrime->passwd);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s set_network 0 ssid '\"%s\"'", ifname, setAKAPrime->ssid);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "wpa_cli -i%s enable_network 0", ifname);
    sret = system(gCmdStr);
#endif

    setEapAkaPrimeResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPAKAPRIME_RESP_TLV, 4, (BYTE *)setEapAkaPrimeResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}



int wfaStaSetSystime(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetSystime_t *systime = (caStaSetSystime_t *)caCmdBuf;
    dutCmdResponse_t *setSystimeResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaSetSystime ...\n");

    sprintf(gCmdStr, "date %d-%d-%d",systime->month,systime->date,systime->year);
    sret = system(gCmdStr);

    sprintf(gCmdStr, "time %d:%d:%d", systime->hours,systime->minutes,systime->seconds);
    sret = system(gCmdStr);

    setSystimeResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_SYSTIME_RESP_TLV, 4, (BYTE *)setSystimeResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

#ifdef WFA_STA_TB
int wfaStaPresetParams(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    caStaPresetParameters_t *params = (caStaPresetParameters_t *)caCmdBuf;
    char *ifname = params->intf;
    int i = 0;

    DPRINT_INFO(WFA_OUT, "%s:%d\n", __func__, __LINE__);

    // Implement the function and its sub commands
    if (params->program == PROG_TYPE_MBO) {
        if((strncmp(ifname,"apcli", 5) == 0)) {
            sprintf(gCmdStr, "mbo_ch_pref=1-%d-%d-%d-%d", params->wfdChPrefNum,
                params->wfdChPref, params->wfdChOpClass,
                params->wfdChReasonCode);
            wfa_driver_mtk_set_cmd(ifname, gCmdStr);
        }
    }
    infoResp.status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_PRESET_PARAMETERS_RESP_TLV, 4, (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSet11n(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *v11nParamsResp = &gGenericResp;

    v11nParamsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, 4, (BYTE *)v11nParamsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}
int wfaStaSetWireless(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staWirelessResp = &gGenericResp;
    caStaSetWireless_t *sw = (caStaSetWireless_t *)caCmdBuf;
    char *intf = sw->intf;
    static BOOL pre_bw_sgnl; /* the sgnl and dyn break into two cmds */
	caStaSetWirelessProgHe_t *prog_he = &sw->prog_he;

    if (sw->bw_sgnl)	
    {
        /* enable the bw signal */
        pre_bw_sgnl = TRUE;
    }
    else
    {
        /* disable the bw sgnl */
        pre_bw_sgnl = FALSE;
        wfa_driver_mtk_set_cmd(intf, "VhtBwSignal=0");
    }	

    if (pre_bw_sgnl)
    {
        if(sw->dyn_bw_sgnl)
        {
		wfa_driver_mtk_set_cmd(intf, "VhtBwSignal=2");
        }
        else
        {
		wfa_driver_mtk_set_cmd(intf, "VhtBwSignal=1");
        }
    }
    /* set 160 BW for case 5.2.65 */
    if(sw->bw == 2)
    {
	wfa_driver_mtk_set_cmd(intf, "VhtBw=2");
    }	 

	// program,HE
	if (prog_he->set_addbareq_bufsize == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=addbareq_bufsize-%d", prog_he->ADDBAReq_BufSize);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_addbaresp_bufsize == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=addbaresp_bufsize-%d", prog_he->ADDBAResp_BufSize);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_bcc == eEnable)
	{
		// 0 for iwpriv cmd to enable bcc
		sprintf(gCmdStr, "sta_wireless=bcc_ldpc-%d", prog_he->BCC ? 0 : 1);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_ldpc == eEnable)
	{
		// 0 for iwpriv cmd to enable ldpc
		sprintf(gCmdStr, "sta_wireless=bcc_ldpc-%d", prog_he->LDPC ? 1 : 0);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_amsdu == eEnable)
	{
		sprintf(gCmdStr, "HtAmsdu=%d", prog_he->amsdu);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_mcs_fixedrate == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=mcs_fixedrate-%d", prog_he->mcs_fixedrate);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_rxsp_stream == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=rxsp_stream-%d", prog_he->rxsp_stream);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_txsp_stream == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=txsp_stream-%d", prog_he->txsp_stream);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}
	if (prog_he->set_width == eEnable)
	{
		sprintf(gCmdStr, "sta_wireless=bandwidth-%d", prog_he->width);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(intf, gCmdStr);
	}

//YF ADD for TGac R2
    staWirelessResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_WIRELESS_RESP_TLV, 4, (BYTE *)staWirelessResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wfaStaSendADDBA(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSendADDBAResp = &gGenericResp;

    staSendADDBAResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_SEND_ADDBA_RESP_TLV, 4, (BYTE *)staSendADDBAResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wfaStaSetRIFS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSetRIFSResp = &gGenericResp;

    wfaEncodeTLV(WFA_STA_SET_RIFS_TEST_RESP_TLV, 4, (BYTE *)staSetRIFSResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSendCoExistMGMT(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSendMGMTResp = &gGenericResp;

    wfaEncodeTLV(WFA_STA_SEND_COEXIST_MGMT_RESP_TLV, 4, (BYTE *)staSendMGMTResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;

}

int wfaStaResetDefault(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaResetDefault_t *reset = (caStaResetDefault_t *)caCmdBuf;
    dutCmdResponse_t *ResetResp = &gGenericResp;

    // need to make your own command available for this, here is only an example
    printf("%s: program %s - intf %s\n", __func__, reset->prog, reset->intf);

    system("sed -i 's/MboSupport=0/MboSupport=1/g' /etc/wireless/sigma_test/wifi_cert_b0.dat");
    system("sed -i 's/MboSupport=0/MboSupport=1/g' /etc/wireless/sigma_test/wifi_cert_b1.dat");
    system("sed -i 's/RRMEnable=0/RRMEnable=1/g' /etc/wireless/sigma_test/wifi_cert_b0.dat");
    system("sed -i 's/RRMEnable=0/RRMEnable=1/g' /etc/wireless/sigma_test/wifi_cert_b1.dat");

    system("sed -i 's/MboSupport=0/MboSupport=1/g' /etc/wireless/mediatek/wifi_cert.1.dat");
    system("sed -i 's/MboSupport=0/MboSupport=1/g' /etc/wireless/mediatek/wifi_cert.2.dat");
    system("sed -i 's/RRMEnable=0/RRMEnable=1/g' /etc/wireless/mediatek/wifi_cert.1.dat");
    system("sed -i 's/RRMEnable=0/RRMEnable=1/g' /etc/wireless/mediatek/wifi_cert.2.dat");
    wfa_driver_mtk_reset_profile();
    sprintf(gCmdStr, "/sbin/wifi down; ifconfig %s up;", reset->intf);
    sret = system(gCmdStr);
    sprintf(gCmdStr, "wappctrl %s mbo reset_default", reset->intf);
    sret = system(gCmdStr);

	if((strncmp(reset->intf,"apcli", 5) == 0)
#ifdef TGAC_DAEMON
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif
	) {
		if(strcasecmp(reset->prog, "wpa3") == 0) {

			wfa_driver_mtk_set_cmd(reset->intf, "ApCliEnable=0");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliSsid=");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliAuthMode=");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliEncrypType=");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliPMFMFPC=0");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliPMFMFPR=0");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliPMFSHA256=0");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliWPAPSK=");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliAutoConnect=0");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliDelPMKIDList=1");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliSAEGroup=19");
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliOWEGroup=19");
#ifdef FIRST_CARD_MT7615_SECOND_CARD_MT7615
			wfa_driver_mtk_set_cmd(reset->intf, "ApCliCertEnable=1");
#endif
		}
	}

	SetManualTxopDur(reset->intf, FALSE);

    ResetResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_RESET_DEFAULT_RESP_TLV, 4, (BYTE *)ResetResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

#else

int wfaStaTestBedCmd(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staCmdResp = &gGenericResp;

    staCmdResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_DISCONNECT_RESP_TLV, 4, (BYTE *)staCmdResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}
#endif

/*
 * This is used to send a frame or action frame
 */
int wfaStaDevSendFrame(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *cmd = (dutCommand_t *)caCmdBuf;
    /* uncomment it if needed */
    // char *ifname = cmd->intf;
    dutCmdResponse_t *devSendResp = &gGenericResp;
    caStaDevSendFrame_t *sf = &cmd->cmdsu.sf;

    DPRINT_INFO(WFA_OUT, "Inside wfaStaDevSendFrame function ...\n");
    /* processing the frame */

    switch(sf->program)
    {
    case PROG_TYPE_PMF:
    {
        pmfFrame_t *pmf = &sf->frameType.pmf;
        switch(pmf->eFrameName)
        {
        case PMF_TYPE_DISASSOC:
        {
            /* use the protected to set what type of key to send */

        }
        break;
        case PMF_TYPE_DEAUTH:
        {

        }
        break;
        case PMF_TYPE_SAQUERY:
        {

        }
        break;
        case PMF_TYPE_AUTH:
        {
        }
        break;
        case PMF_TYPE_ASSOCREQ:
        {
        }
        break;
        case PMF_TYPE_REASSOCREQ:
        {
        }
        break;
        }
    }
    break;
    case PROG_TYPE_TDLS:
    {
        tdlsFrame_t *tdls = &sf->frameType.tdls;
        switch(tdls->eFrameName)
        {
        case TDLS_TYPE_DISCOVERY:
            /* use the peer mac address to send the frame */
            break;
        case TDLS_TYPE_SETUP:
            break;
        case TDLS_TYPE_TEARDOWN:
            break;
        case TDLS_TYPE_CHANNELSWITCH:
            break;
        case TDLS_TYPE_NULLFRAME:
            break;
        }
    }
    break;
    case PROG_TYPE_VENT:
    {
        ventFrame_t *vent = &sf->frameType.vent;
        switch(vent->type)
        {
        case VENT_TYPE_NEIGREQ:
            break;
        case VENT_TYPE_TRANSMGMT:
            break;
        }
    }
    break;
    case PROG_TYPE_WFD:
    {
        wfdFrame_t *wfd = &sf->frameType.wfd;
        switch(wfd->eframe)
        {
        case WFD_FRAME_PRBREQ:
        {
            /* send probe req */
        }
        break;

        case WFD_FRAME_PRBREQ_TDLS_REQ:
        {
            /* send tunneled tdls probe req  */
        }
        break;

        case WFD_FRAME_11V_TIMING_MSR_REQ:
        {
            /* send 11v timing mearurement request */
        }
        break;

        case WFD_FRAME_RTSP:
        {
            /* send WFD RTSP messages*/
            // fetch the type of RTSP message and send it.
            switch(wfd->eRtspMsgType)
            {
            case WFD_RTSP_PAUSE:
                break;
            case WFD_RTSP_PLAY:
                //send RTSP PLAY
                break;
            case WFD_RTSP_TEARDOWN:
                //send RTSP TEARDOWN
                break;
            case WFD_RTSP_TRIG_PAUSE:
                //send RTSP TRIGGER PAUSE
                break;
            case WFD_RTSP_TRIG_PLAY:
                //send RTSP TRIGGER PLAY
                break;
            case WFD_RTSP_TRIG_TEARDOWN:
                //send RTSP TRIGGER TEARDOWN
                break;
            case WFD_RTSP_SET_PARAMETER:
                //send RTSP SET PARAMETER
                if (wfd->eSetParams == WFD_CAP_UIBC_KEYBOARD)
                {
                    //send RTSP SET PARAMETER message for UIBC keyboard
                }
                if (wfd->eSetParams == WFD_CAP_UIBC_MOUSE)
                {
                    //send RTSP SET PARAMETER message for UIBC Mouse
                }
                else if (wfd->eSetParams == WFD_CAP_RE_NEGO)
                {
                    //send RTSP SET PARAMETER message Capability re-negotiation
                }
                else if (wfd->eSetParams == WFD_STANDBY)
                {
                    //send RTSP SET PARAMETER message for standby
                }
                else if (wfd->eSetParams == WFD_UIBC_SETTINGS_ENABLE)
                {
                    //send RTSP SET PARAMETER message for UIBC settings enable
                }
                else if (wfd->eSetParams == WFD_UIBC_SETTINGS_DISABLE)
                {
                    //send RTSP SET PARAMETER message for UIBC settings disable
                }
                else if (wfd->eSetParams == WFD_ROUTE_AUDIO)
                {
                    //send RTSP SET PARAMETER message for route audio
                }
                else if (wfd->eSetParams == WFD_3D_VIDEOPARAM)
                {
                    //send RTSP SET PARAMETER message for 3D video parameters
                }
                else if (wfd->eSetParams == WFD_2D_VIDEOPARAM)
                {
                    //send RTSP SET PARAMETER message for 2D video parameters
                }
                break;
            }
        }
        break;
        }
    }
    break;
    /* not need to support HS2 release 1, due to very short time period  */
    case PROG_TYPE_HS2_R2:
    {
        /* type of frames */
        hs2Frame_t *hs2 = &sf->frameType.hs2_r2;
        switch(hs2->eframe)
        {
        case HS2_FRAME_ANQPQuery:
        {

        }
        break;
        case HS2_FRAME_DLSRequest:
        {

        }
        break;
        case HS2_FRAME_GARPReq:
        {

        }
        break;
        case HS2_FRAME_GARPRes:
        {
        }
        break;
        case HS2_FRAME_NeighAdv:
        {
        }
        case HS2_FRAME_ARPProbe:
        {
        }
        case HS2_FRAME_ARPAnnounce:
        {

        }
        break;
        case HS2_FRAME_NeighSolicitReq:
        {

        }
        break;
        case HS2_FRAME_ARPReply:
        {

        }
        break;
        }

        }/*  PROG_TYPE_HS2-R2  */
    case PROG_TYPE_GEN:
    {
        /* General frames */
    }


    }
    devSendResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_DEV_SEND_FRAME_RESP_TLV, 4, (BYTE *)devSendResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * This is used to set a temporary MAC address of an interface
 */
int wfaStaSetMacAddr(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    // Uncomment it if needed
    //dutCommand_t *cmd = (dutCommand_t *)caCmdBuf;
    // char *ifname = cmd->intf;
    dutCmdResponse_t *staCmdResp = &gGenericResp;
    // Uncomment it if needed
    //char *macaddr = &cmd->cmdsu.macaddr[0];

    wfaEncodeTLV(WFA_STA_SET_MAC_ADDRESS_RESP_TLV, 4, (BYTE *)staCmdResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}


int wfaStaDisconnect(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
#ifdef MTK_PRIVATE_IOCTL
    dutCommand_t *disc = (dutCommand_t *)caCmdBuf;
    char *intf = disc->intf;
#endif /* MTK_PRIVATE_IOCTL */
    dutCmdResponse_t *staDiscResp = &gGenericResp;
    int maintain_profile = disc->cmdsu.stadisconn.maintainprofile;

    // stop the supplicant
#ifdef MTK_PRIVATE_IOCTL

   DPRINT_INFO(WFA_OUT, "entering wfaStaDisconnect ...\n");
	if(strncmp(intf,"apcli", 5) == 0
#ifdef TGAC_DAEMON
		||(strncmp(wireless_intf, "apcli", 5) == 0)
#endif /* TGAC_DAEMON */
	)
	{

	   /*
	   	Add For PMF 5.3.3.3 De-auth Frame may Not Encrypted after "iwpriv apcli0 set ApCliEnable=0"

	   	Driver Code :
	   	PMF_RobustFrameClassify() will reference the pEntry, but after "iwpriv apcli0 set ApCliEnable=0" will delete the pEntry,
	   	and let PMF_RobustFrameClassify() fall into NORMAL_FRAME case!

	   	Solution :
	   	Add New Tx De-auth pkt cmd to send De-auth first, and then move on the driver's state machine.
	   */

	if(maintain_profile == 1) {
		DPRINT_INFO(WFA_OUT, "Keep profile  ...\n");
		wfa_driver_mtk_set_cmd(intf, "ApCliTxDeAuth=1");
		sleep(1);
		wfa_driver_mtk_set_cmd(intf, "ApCliEnable=0");

	} else {
	   wfa_driver_mtk_set_cmd(intf, "ApCliTxDeAuth=1");
	   sleep(1);

	   wfa_driver_mtk_set_cmd(intf, "ApCliSsid=");
	   wfa_driver_mtk_set_cmd(intf, "ApCliEnable=0");
	}
	SetTxPPDUEnable(intf, TRUE);
}
	else {
	   wfa_driver_mtk_set_cmd(intf, "SSID=");
	}
#endif /* MTK_PRIVATE_IOCTL */

    staDiscResp->status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_DISCONNECT_RESP_TLV, 4, (BYTE *)staDiscResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/* Execute CLI, read the status from Environment variable */
int wfaExecuteCLI(char *CLI)
{
    char *retstr;

    sret = system(CLI);

    retstr = getenv("WFA_CLI_STATUS");
    printf("cli status %s\n", retstr);
    return atoi(retstr);
}

/* Supporting Functions */

void wfaSendPing(tgPingStart_t *staPing, float *interval, int streamid)
{
    int totalpkts, tos=-1;
    char cmdStr[256];
//    char *addr = staPing->dipaddr;
#ifdef WFA_PC_CONSOLE
    char addr[40];
    char bflag[] = "-b";
    char *tmpstr;
    int inum=0;
#else
    char bflag[] = "  ";
#endif

    totalpkts = (int)(staPing->duration * staPing->frameRate);

#ifdef WFA_PC_CONSOLE

    printf("\nwfa_cs.c wfaSendPing CS : The Stream ID is %d",streamid);
    
    strcpy(addr,staPing->dipaddr);
	printf("\nCS :the addr is %s ",addr);
    printf("\nCS :Inside the WFA_PC_CONSLE BLOCK");
    printf("\nCS :the addr is %s ",addr);
    if (staPing->iptype == 2)
    {
        memset(bflag, 0, strlen(bflag));
    }
    else
    {
        tmpstr = strtok(addr, ".");
        inum = atoi(tmpstr);

        printf("interval %f\n", *interval);

        if(inum >= 224 && inum <= 239) // multicast
        {
        }
        else // if not MC, check if it is BC address
        {
            printf("\nCS :Inside the BC address BLOCK");
            printf("\nCS :the inum %d",inum);
            strtok(NULL, ".");
            //strtok(NULL, ".");
            tmpstr = strtok(NULL, ".");
            printf("tmpstr %s\n", tmpstr);
            inum = atoi(tmpstr);
            printf("\nCS : The string is %s",tmpstr);
            if(inum != 255)
                memset(bflag, 0, strlen(bflag));
        }
    }
#endif
    if ( staPing->dscp >= 0)
    {
        tos= convertDscpToTos(staPing->dscp);
        if (tos < 0)
            printf("\nwfaSendPing invalid tos converted, dscp=%d",  staPing->dscp);
    }
    printf("\nwfa_cs.c wfaSendPing : The Stream ID=%d IPtype=%i\n",streamid, staPing->iptype);
    printf("IPtype : %i  tos=%d",staPing->iptype, tos);

    if (staPing->iptype == 2)
    {
#if 0
        if ( tos>0)
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s %s -i %f -c %i -Q %d -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, *interval, totalpkts, tos,  staPing->frameSize,streamid);
        else
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s %s -i %f -c %i -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, *interval, totalpkts, staPing->frameSize,streamid);
#else
		if ( tos>0)
			sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s %s -c %i -Q %d -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
					streamid,streamid,bflag, staPing->dipaddr, totalpkts, tos,  staPing->frameSize,streamid);
		else
			sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s %s -c %i -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
					streamid,streamid,bflag, staPing->dipaddr, totalpkts, staPing->frameSize,streamid);
#endif
        sret = system(cmdStr);
        printf("\nCS : The command string is %s",cmdStr);
    }
    else
    {
    		/* We don't support the option -i in ping command in our busybox. */
#if 0
        if (tos > 0)
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s %s -i %f -c %i  -Q %d -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, *interval, totalpkts, tos, staPing->frameSize,streamid);
        else
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s %s -i %f -c %i -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, *interval, totalpkts, staPing->frameSize,streamid);
#else
        if (tos > 0)
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s %s -c %i  -Q %d -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, totalpkts, tos, staPing->frameSize,streamid);
        else
            sprintf(cmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s %s -c %i -s %i -q >> /tmp/spout_%d.txt 2>/dev/null",
                    streamid,streamid,bflag, staPing->dipaddr, totalpkts, staPing->frameSize,streamid);
#endif
        sret = system(cmdStr);
        printf("\nCS : The command string is %s",cmdStr);
    }
    sprintf(cmdStr, "updatepid.sh /tmp/spout_%d.txt",streamid);
    sret = system(cmdStr);
    printf("\nCS : The command string is %s",cmdStr);

}

int wfaStopPing(dutCmdResponse_t *stpResp, int streamid)
{
    char strout[256];
    FILE *tmpfile = NULL;
    char cmdStr[128];
    printf("\nwfa_cs.c wfaStopPing:: stream id=%d\n", streamid);
    sprintf(cmdStr, "getpid.sh /tmp/spout_%d.txt /tmp/pid.txt",streamid);
    sret = system(cmdStr);

    printf("\nCS : The command string is %s",cmdStr);

    sret = system("stoping.sh /tmp/pid.txt ; sleep 2");

    sprintf(cmdStr, "getpstats.sh /tmp/spout_%d.txt",streamid);
    sret = system(cmdStr);

    printf("\nCS : The command string is %s",cmdStr);

    tmpfile = fopen("/tmp/stpsta.txt", "r+");

    if(tmpfile == NULL)
    {
        return WFA_FAILURE;
    }

    if(fscanf(tmpfile, "%s", strout) != EOF)
    {
        if(*strout == '\0')
        {
            stpResp->cmdru.pingStp.sendCnt = 0;
        }

        else
            stpResp->cmdru.pingStp.sendCnt = atoi(strout);
    }

    printf("\nwfaStopPing after scan sent count %i\n", stpResp->cmdru.pingStp.sendCnt);


    if(fscanf(tmpfile, "%s", strout) != EOF)
    {
        if(*strout == '\0')
        {
            stpResp->cmdru.pingStp.repliedCnt = 0;
        }
        else
            stpResp->cmdru.pingStp.repliedCnt = atoi(strout);
    }
    printf("wfaStopPing after scan replied count %i\n", stpResp->cmdru.pingStp.repliedCnt);

    fclose(tmpfile);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetP2pDevAddress():
 */
int wfaStaGetP2pDevAddress(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* dutCommand_t *getInfo = (dutCommand_t *)caCmdBuf; */

    printf("\n Entry wfaStaGetP2pDevAddress... ");

    // Fetch the device ID and store into infoResp->cmdru.devid
    //strcpy(infoResp->cmdru.devid, str);
    strcpy(&infoResp.cmdru.devid[0], "ABCDEFGH");

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_GET_DEV_ADDRESS_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}



/*
 * wfaStaSetP2p():
 */
int wfaStaSetP2p(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaSetP2p_t *getStaSetP2p = (caStaSetP2p_t *)caCmdBuf; uncomment and use it*/

    printf("\n Entry wfaStaSetP2p... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SETP2P_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}
/*
 * wfaStaP2pConnect():
 */
int wfaStaP2pConnect(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaP2pConnect_t *getStaP2pConnect = (caStaP2pConnect_t *)caCmdBuf; uncomment and use it */

    printf("\n Entry wfaStaP2pConnect... ");

    // Implement the function and does not return anything.


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_CONNECT_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaStartAutoGo():
 */
int wfaStaStartAutoGo(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    //caStaStartAutoGo_t *getStaStartAutoGo = (caStaStartAutoGo_t *)caCmdBuf;

    printf("\n Entry wfaStaStartAutoGo... ");

    // Fetch the group ID and store into 	infoResp->cmdru.grpid
    strcpy(&infoResp.cmdru.grpid[0], "ABCDEFGH");

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_START_AUTO_GO_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}




/*
 * wfaStaP2pStartGrpFormation():
 */
int wfaStaP2pStartGrpFormation(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    printf("\n Entry wfaStaP2pStartGrpFormation... ");

    strcpy(infoResp.cmdru.grpFormInfo.result, "CLIENT");
    strcpy(infoResp.cmdru.grpFormInfo.grpId, "AA:BB:CC:DD:EE:FF_DIRECT-SSID");


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_START_GRP_FORMATION_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


/*
 * wfaStaP2pDissolve():
 */
int wfaStaP2pDissolve(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    printf("\n Entry wfaStaP2pDissolve... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_DISSOLVE_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSendP2pInvReq():
 */
int wfaStaSendP2pInvReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaSendP2pInvReq_t *getStaP2pInvReq= (caStaSendP2pInvReq_t *)caCmdBuf; */

    printf("\n Entry wfaStaSendP2pInvReq... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SEND_INV_REQ_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


/*
 * wfaStaAcceptP2pInvReq():
 */
int wfaStaAcceptP2pInvReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* uncomment and use it
     * caStaAcceptP2pInvReq_t *getStaP2pInvReq= (caStaAcceptP2pInvReq_t *)caCmdBuf;
     */

    printf("\n Entry wfaStaAcceptP2pInvReq... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_ACCEPT_INV_REQ_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


/*
 * wfaStaSendP2pProvDisReq():
 */
int wfaStaSendP2pProvDisReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* uncomment and use it
     * caStaSendP2pProvDisReq_t *getStaP2pProvDisReq= (caStaSendP2pProvDisReq_t *)caCmdBuf;
     */

    printf("\n Entry wfaStaSendP2pProvDisReq... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SEND_PROV_DIS_REQ_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetWpsPbc():
 */
int wfaStaSetWpsPbc(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* uncomment and use it
     * caStaSetWpsPbc_t *getStaSetWpsPbc= (caStaSetWpsPbc_t *)caCmdBuf;
     */

    printf("\n Entry wfaStaSetWpsPbc... ");

    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_WPS_SETWPS_PBC_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaWpsReadPin():
 */
int wfaStaWpsReadPin(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* uncomment and use it
     * caStaWpsReadPin_t *getStaWpsReadPin= (caStaWpsReadPin_t *)caCmdBuf;
     */

    printf("\n Entry wfaStaWpsReadPin... ");

    // Fetch the device PIN and put in 	infoResp->cmdru.wpsPin
    //strcpy(infoResp->cmdru.wpsPin, "12345678");
    strcpy(&infoResp.cmdru.wpsPin[0], "1234456");


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_WPS_READ_PIN_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}



/*
 * wfaStaWpsReadLabel():
 */
int wfaStaWpsReadLabel(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    printf("\n Entry wfaStaWpsReadLabel... ");

    // Fetch the device Label and put in	infoResp->cmdru.wpsPin
    //strcpy(infoResp->cmdru.wpsPin, "12345678");
    strcpy(&infoResp.cmdru.wpsPin[0], "1234456");


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_WPS_READ_PIN_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


/*
 * wfaStaWpsEnterPin():
 */
int wfaStaWpsEnterPin(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* uncomment and use it
     * caStaWpsEnterPin_t *getStaWpsEnterPin= (caStaWpsEnterPin_t *)caCmdBuf;
     */

    printf("\n Entry wfaStaWpsEnterPin... ");

    // Implement the function and this does not return any thing back.


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_WPS_ENTER_PIN_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


/*
 * wfaStaGetPsk():
 */
int wfaStaGetPsk(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaGetPsk_t *getStaGetPsk= (caStaGetPsk_t *)caCmdBuf; uncomment and use it */

    printf("\n Entry wfaStaGetPsk... ");


    // Fetch the device PP and SSID  and put in 	infoResp->cmdru.pskInfo
    strcpy(&infoResp.cmdru.pskInfo.passPhrase[0], "1234456");
    strcpy(&infoResp.cmdru.pskInfo.ssid[0], "WIFI_DIRECT");


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_GET_PSK_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaP2pReset():
 */
int wfaStaP2pReset(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* dutCommand_t *getStaP2pReset= (dutCommand_t *)caCmdBuf; */

    printf("\n Entry wfaStaP2pReset... ");
    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_RESET_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}



/*
 * wfaStaGetP2pIpConfig():
 */
int wfaStaGetP2pIpConfig(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaGetP2pIpConfig_t *staGetP2pIpConfig= (caStaGetP2pIpConfig_t *)caCmdBuf; */

    caStaGetIpConfigResp_t *ifinfo = &(infoResp.cmdru.getIfconfig);

    printf("\n Entry wfaStaGetP2pIpConfig... ");

    ifinfo->isDhcp =0;
    strcpy(&(ifinfo->ipaddr[0]), "192.165.100.111");
    strcpy(&(ifinfo->mask[0]), "255.255.255.0");
    strcpy(&(ifinfo->dns[0][0]), "192.165.100.1");
    strcpy(&(ifinfo->mac[0]), "ba:ba:ba:ba:ba:ba");

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_GET_IP_CONFIG_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}




/*
 * wfaStaSendServiceDiscoveryReq():
 */
int wfaStaSendServiceDiscoveryReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    printf("\n Entry wfaStaSendServiceDiscoveryReq... ");
    // Implement the function and this does not return any thing back.


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SEND_SERVICE_DISCOVERY_REQ_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}



/*
 * wfaStaSendP2pPresenceReq():
 */
int wfaStaSendP2pPresenceReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SEND_PRESENCE_REQ_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetSleepReq():
 */
int wfaStaSetSleepReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaSetSleep_t *staSetSleepReq= (caStaSetSleep_t *)caCmdBuf; */

    printf("\n Entry wfaStaSetSleepReq... ");
    // Implement the function and this does not return any thing back.


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SET_SLEEP_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN +4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetOpportunisticPsReq():
 */
int wfaStaSetOpportunisticPsReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    printf("\n Entry wfaStaSetOpportunisticPsReq... ");
    // Implement the function and this does not return any thing back.


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_SET_OPPORTUNISTIC_PS_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}
#ifndef WFA_STA_TB
/*
 * wfaStaPresetParams():
 */

int wfaStaPresetParams(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;

    DPRINT_INFO(WFA_OUT, "Inside wfaStaPresetParameters function ...\n");

    // Implement the function and its sub commands
    infoResp.status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_PRESET_PARAMETERS_RESP_TLV, 4, (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}
int wfaStaSet11n(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

    dutCmdResponse_t infoResp;
    dutCmdResponse_t *v11nParamsResp = &infoResp;

#ifdef WFA_11N_SUPPORT_ONLY

    caSta11n_t * v11nParams = (caSta11n_t *)caCmdBuf;

    int st =0; // SUCCESS

    DPRINT_INFO(WFA_OUT, "Inside wfaStaSet11n function....\n");

    if(v11nParams->addba_reject != 0xFF && v11nParams->addba_reject < 2)
    {
        // implement the funciton
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_addba_reject failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->ampdu != 0xFF && v11nParams->ampdu < 2)
    {
        // implement the funciton

        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_ampdu failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->amsdu != 0xFF && v11nParams->amsdu < 2)
    {
        // implement the funciton
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_amsdu failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->greenfield != 0xFF && v11nParams->greenfield < 2)
    {
        // implement the funciton
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "_set_greenfield failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->mcs32!= 0xFF && v11nParams->mcs32 < 2 && v11nParams->mcs_fixedrate[0] != '\0')
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_mcs failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }
    else if (v11nParams->mcs32!= 0xFF && v11nParams->mcs32 < 2 && v11nParams->mcs_fixedrate[0] == '\0')
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_mcs32 failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }
    else if (v11nParams->mcs32 == 0xFF && v11nParams->mcs_fixedrate[0] != '\0')
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_mcs32 failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->rifs_test != 0xFF && v11nParams->rifs_test < 2)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_rifs_test failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->sgi20 != 0xFF && v11nParams->sgi20 < 2)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_sgi20 failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->smps != 0xFFFF)
    {
        if(v11nParams->smps == 0)
        {
            // implement the funciton
            //st = wfaExecuteCLI(gCmdStr);
        }
        else if(v11nParams->smps == 1)
        {
            // implement the funciton
            //st = wfaExecuteCLI(gCmdStr);
            ;
        }
        else if(v11nParams->smps == 2)
        {
            // implement the funciton
            //st = wfaExecuteCLI(gCmdStr);
            ;
        }
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_smps failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->stbc_rx != 0xFFFF)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_stbc_rx failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->width[0] != '\0')
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_11n_channel_width failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->_40_intolerant != 0xFF && v11nParams->_40_intolerant < 2)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_40_intolerant failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

    if(v11nParams->txsp_stream != 0 && v11nParams->txsp_stream <4)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_txsp_stream failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }

    }

    if(v11nParams->rxsp_stream != 0 && v11nParams->rxsp_stream < 4)
    {
        // implement the funciton
        //st = wfaExecuteCLI(gCmdStr);
        if(st != 0)
        {
            v11nParamsResp->status = STATUS_ERROR;
            strcpy(v11nParamsResp->cmdru.info, "set_rxsp_stream failed");
            wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)v11nParamsResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return FALSE;
        }
    }

#endif

    v11nParamsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, 4, (BYTE *)v11nParamsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}
#endif
/*
 * wfaStaAddArpTableEntry():
 */
int wfaStaAddArpTableEntry(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaAddARPTableEntry_t *staAddARPTableEntry= (caStaAddARPTableEntry_t *)caCmdBuf; uncomment and use it */

    printf("\n Entry wfastaAddARPTableEntry... ");
    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_ADD_ARP_TABLE_ENTRY_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaBlockICMPResponse():
 */
int wfaStaBlockICMPResponse(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    /* caStaBlockICMPResponse_t *staAddARPTableEntry= (caStaBlockICMPResponse_t *)caCmdBuf; uncomment and use it */

    printf("\n Entry wfaStaBlockICMPResponse... ");
    // Implement the function and this does not return any thing back.

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_P2P_BLOCK_ICMP_RESPONSE_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetRadio():
 */

int wfaStaSetRadio(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *setRadio = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *staCmdResp = &gGenericResp;
    caStaSetRadio_t *sr = &setRadio->cmdsu.sr;

    if(sr->mode == WFA_OFF)
    {
        // turn radio off
    }
    else
    {
        // always turn the radio on
    }

    staCmdResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_RADIO_RESP_TLV, 4, (BYTE *)staCmdResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetRFeature():
 */

int wfaStaSetRFeature(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *dutCmd = (dutCommand_t *)caCmdBuf;
    caStaRFeat_t *rfeat = &dutCmd->cmdsu.rfeat;
    dutCmdResponse_t *caResp = &gGenericResp;

    if(strcasecmp(rfeat->prog, "tdls") == 0)
    {


    } else if(strcasecmp(rfeat->prog, "mbo") == 0) {
        char *ifname = dutCmd->intf;
        if((strncmp(ifname,"apcli", 5) == 0)) {
            if (rfeat->wfdChPrefNum == 0)
                sprintf(gCmdStr, "mbo_ch_pref=2");
            else
                sprintf(gCmdStr, "mbo_ch_pref=3-%d-%d-%d-%d",
                    rfeat->wfdChPrefNum,
                    rfeat->wfdChPref, rfeat->wfdChOpClass,
                     rfeat->wfdChReasonCode);

            wfa_driver_mtk_set_cmd(ifname, gCmdStr);
        }
    }

    if(rfeat->txsuppdu == eEnable)
    {
        DPRINT_INFO(WFA_OUT, "%s(%d)\n", __FUNCTION__, __LINE__);
        SetTxPPDUEnable(dutCmd->intf, TRUE);
    }
    else if (rfeat->txsuppdu == eDisable)
    {
        DPRINT_INFO(WFA_OUT, "%s(%d)\n", __FUNCTION__, __LINE__);
        SetTxPPDUEnable(dutCmd->intf, FALSE);
    }

	if (strcasecmp(rfeat->twt_req_type, "teardown") == 0)
	{
		DPRINT_INFO(WFA_OUT, " TWT: teardown\n");
		sprintf(gCmdStr, "ApCliTwtParams=5-%d", rfeat->twt_param.FlowID);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(dutCmd->intf, gCmdStr);
		sleep(1);
	}
	else if (strcasecmp(rfeat->twt_req_type, "request") == 0)
	{
		DPRINT_INFO(WFA_OUT, " TWT: request\n");
		sprintf(gCmdStr, "ApCliTwtParams=4-%d-%d-%d-%d-%d-%d-%d-%d",
			rfeat->twt_param.FlowID,
			rfeat->twt_param.SetupCommand,
			rfeat->twt_param.TWT_Trigger,
			rfeat->twt_param.FlowType,
			rfeat->twt_param.WakeIntervalExp,
			rfeat->twt_param.Protection,
			rfeat->twt_param.NominalMinWakeDur,
			rfeat->twt_param.WakeIntervalMantissa);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(dutCmd->intf, gCmdStr);
		sleep(1);
	}

	if(rfeat->set_ltf == eEnable)
	{
		sprintf(gCmdStr, "sta_rfeatures=ltf-%d", rfeat->ltf);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(dutCmd->intf, gCmdStr);
	}

	if(rfeat->set_gi == eEnable)
	{
		sprintf(gCmdStr, "sta_rfeatures=gi-%d", rfeat->gi);
		DPRINT_INFO(WFA_OUT, "cmd = %s\n", gCmdStr);
		wfa_driver_mtk_set_cmd(dutCmd->intf, gCmdStr);
	}

    caResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_RFEATURE_RESP_TLV, 4, (BYTE *)caResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaStartWfdConnection():
 */
int wfaStaStartWfdConnection(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    //caStaStartWfdConn_t *staStartWfdConn= (caStaStartWfdConn_t *)caCmdBuf; //uncomment and use it

    printf("\n Entry wfaStaStartWfdConnection... ");


    // Fetch the GrpId and WFD session and return
    strcpy(&infoResp.cmdru.wfdConnInfo.wfdSessionId[0], "1234567890");
    strcpy(&infoResp.cmdru.wfdConnInfo.p2pGrpId[0], "WIFI_DISPLAY");
    strcpy(&infoResp.cmdru.wfdConnInfo.result[0], "GO");

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_START_WFD_CONNECTION_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}
/*
 * wfaStaCliCommand():
 */

int wfaStaCliCommand(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    char cmdName[32];
    char *pcmdStr=NULL, *str;
    int  st = 1;
    char CmdStr[WFA_CMD_STR_SZ];
    FILE *wfaCliFd;
    char wfaCliBuff[64];
    char retstr[256];
    int CmdReturnFlag =0;
    char tmp[256];
    FILE * sh_pipe;
    caStaCliCmdResp_t infoResp;

    printf("\nEntry wfaStaCliCommand; command Received: %s\n",caCmdBuf);
    memcpy(cmdName, strtok_r((char *)caCmdBuf, ",", (char **)&pcmdStr), 32);
    sprintf(CmdStr, "%s",cmdName);

    for(;;)
    {
        // construct CLI standard cmd string
        str = strtok_r(NULL, ",", &pcmdStr);
        if(str == NULL || str[0] == '\0')
            break;
        else
        {
            sprintf(CmdStr, "%s /%s",CmdStr,str);
            str = strtok_r(NULL, ",", &pcmdStr);
            sprintf(CmdStr, "%s %s",CmdStr,str);
        }
    }
    // check the return process
    wfaCliFd=fopen("/etc/WfaEndpoint/wfa_cli.txt","r");
    if(wfaCliFd!= NULL)
    {
        while(fgets(wfaCliBuff, 64, wfaCliFd) != NULL)
        {
            //printf("\nLine read from CLI file : %s",wfaCliBuff);
            if(ferror(wfaCliFd))
                break;

            str=strtok(wfaCliBuff,"-");
            if(strcmp(str,cmdName) == 0)
            {
                str=strtok(NULL,",");
                if (str != NULL)
                {
                    if(strcmp(str,"TRUE") == 0)
                        CmdReturnFlag =1;
                }
                else
                    printf("ERR wfa_cli.txt, inside line format not end with , or missing TRUE/FALSE\n");
                break;
            }
        }
        fclose(wfaCliFd);
    }
    else
    {
        printf("/etc/WfaEndpoint/wfa_cli.txt is not exist\n");
        goto cleanup;
    }

    //printf("\n Command Return Flag : %d",CmdReturnFlag);
    memset(&retstr[0],'\0',255);
    memset(&tmp[0],'\0',255);
    sprintf(gCmdStr, "%s",  CmdStr);
    printf("\nCLI Command -- %s\n", gCmdStr);

    sh_pipe = popen(gCmdStr,"r");
    if(!sh_pipe)
    {
        printf ("Error in opening pipe\n");
        goto cleanup;
    }

    sleep(5);
    //tmp_val=getdelim(&retstr,255,"\n",sh_pipe);
    if (fgets(&retstr[0], 255, sh_pipe) == NULL)
    {
        printf("Getting NULL string in popen return\n");
        goto cleanup;
    }
    else
        printf("popen return str=%s\n",retstr);

    sleep(2);
    if(pclose(sh_pipe) == -1)
    {
        printf("Error in closing shell cmd pipe\n");
        goto cleanup;
    }
    sleep(2);

    // find status first in output
    str = strtok_r((char *)retstr, "-", (char **)&pcmdStr);
    if (str != NULL)
    {
        memset(tmp, 0, 10);
        memcpy(tmp, str,  2);
        printf("cli status=%s\n",tmp);
        if(strlen(tmp) > 0)
            st = atoi(tmp);
        else printf("Missing status code\n");
    }
    else
    {
        printf("wfaStaCliCommand no return code found\n");
    }
    infoResp.resFlag=CmdReturnFlag;

cleanup:

    switch(st)
    {
    case 0:
        infoResp.status = STATUS_COMPLETE;
        if (CmdReturnFlag)
        {
            if((pcmdStr != NULL) && (strlen(pcmdStr) > 0) )
            {
                memset(&(infoResp.result[0]),'\0',WFA_CLI_CMD_RESP_LEN-1);
                strncpy(&infoResp.result[0], pcmdStr ,(strlen(pcmdStr) < WFA_CLI_CMD_RESP_LEN ) ? strlen(pcmdStr) : (WFA_CLI_CMD_RESP_LEN-2) );
                printf("Return CLI result string to CA=%s\n", &(infoResp.result[0]));
            }
            else
            {
                strcpy(&infoResp.result[0], "No return string found\n");
            }
        }
        break;
    case 1:
        infoResp.status = STATUS_ERROR;
        break;
    case 2:
        infoResp.status = STATUS_INVALID;
        break;
    }

    wfaEncodeTLV(WFA_STA_CLI_CMD_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    printf("Exit from wfaStaCliCommand\n");
    return TRUE;

}
/*
 * wfaStaConnectGoStartWfd():
 */

int wfaStaConnectGoStartWfd(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
//  caStaConnectGoStartWfd_t *staConnecGoStartWfd= (caStaConnectGoStartWfd_t *)caCmdBuf; //uncomment and use it

    printf("\n Entry wfaStaConnectGoStartWfd... ");

    // connect the specified GO and then establish the wfd session

    // Fetch WFD session and return
    strcpy(&infoResp.cmdru.wfdConnInfo.wfdSessionId[0], "1234567890");

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_CONNECT_GO_START_WFD_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaGenerateEvent():
 */

int wfaStaGenerateEvent(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    caStaGenEvent_t *staGenerateEvent= (caStaGenEvent_t *)caCmdBuf; //uncomment and use it
    caWfdStaGenEvent_t *wfdGenEvent;

    printf("\n Entry wfaStaGenerateEvent... ");


    // Geneate the specified action and return with complete/error.
    if(staGenerateEvent->program == PROG_TYPE_WFD)
    {
        wfdGenEvent = &staGenerateEvent->wfdEvent;
        if(wfdGenEvent ->type == eUibcGen)
        {
        }
        else if(wfdGenEvent ->type == eUibcHid)
        {
        }
        else if(wfdGenEvent ->type == eFrameSkip)
        {

        }
        else if(wfdGenEvent ->type == eI2cRead)
        {
        }
        else if(wfdGenEvent ->type == eI2cWrite)
        {
        }
        else if(wfdGenEvent ->type == eInputContent)
        {
        }
        else if(wfdGenEvent ->type == eIdrReq)
        {
        }
    }

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GENERATE_EVENT_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}




/*
 * wfaStaReinvokeWfdSession():
 */

int wfaStaReinvokeWfdSession(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
//  caStaReinvokeWfdSession_t *staReinvokeSession= (caStaReinvokeWfdSession_t *)caCmdBuf; //uncomment and use it

    printf("\n Entry wfaStaReinvokeWfdSession... ");

    // Reinvoke the WFD session by accepting the p2p invitation   or sending p2p invitation


    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_REINVOKE_WFD_SESSION_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}


int wfaStaGetParameter(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    caStaGetParameter_t *staGetParam= (caStaGetParameter_t *)caCmdBuf; //uncomment and use it


    caStaGetParameterResp_t *paramList = &infoResp.cmdru.getParamValue;

    printf("\n Entry wfaStaGetParameter... ");

    // Check the program type
    if(staGetParam->program == PROG_TYPE_WFD)
    {
        if(staGetParam->getParamValue == eDiscoveredDevList )
        {
            // Get the discovered devices, make space seperated list and return, check list is not bigger than 128 bytes.
            paramList->getParamType = eDiscoveredDevList;
            strcpy((char *)&paramList->devList, "11:22:33:44:55:66 22:33:44:55:66:77 33:44:55:66:77:88");
        }
    }

	if(staGetParam->program == PROG_TYPE_WFDS)
	{

		if(staGetParam->getParamValue == eDiscoveredDevList )
		{
			// Get the discovered devices, make space seperated list and return, check list is not bigger than 128 bytes.
			paramList->getParamType = eDiscoveredDevList;
			strcpy((char *)&paramList->devList, "11:22:33:44:55:66 22:33:44:55:66:77 33:44:55:66:77:88");
			
		}
		if(staGetParam->getParamValue == eOpenPorts)
		{
			// Run the port checker tool 
			// Get all the open ports and make space seperated list and return, check list is not bigger than 128 bytes.
			paramList->getParamType = eOpenPorts;
			strcpy((char *)&paramList->devList, "22 139 445 68 9700");
			
		}
		
	}
	if(staGetParam->program == PROG_TYPE_NAN)
   	{
      if(staGetParam->getParamValue == eMasterPref )
      {
          // Get the master preference of the device and return the value
          paramList->getParamType = eMasterPref;
          strcpy((char *)&paramList->masterPref, "0xff");
      }
    }

	if(staGetParam->program == PROG_TYPE_HE)
	{
		if(staGetParam->getParamValue == eRssi)
		{
			signed char rssi = 0;
			paramList->getParamType = eRssi;
			wfa_driver_mtk_get_oid(staGetParam->intf, OID_802_11_RSSI, &rssi, sizeof(rssi));
			paramList->rssi = rssi;

			/* add HE-5.61.1 workaround */
			if((strcmp(gHeCfgSet_t.ssid,"HE-5.61.1_24G") == 0)
				|| (strcmp(gHeCfgSet_t.ssid,"HE-5.61.1_5G") == 0)
				|| (strcmp(gHeCfgSet_t.ssid,"HE-5.61.1") == 0))
			{
				SetManualTxopDur(staGetParam->intf, TRUE);
			}
		}
	}

    DPRINT_INFO(WFA_OUT, "wfaStaGetParameter:staGetParam(%d,%d),paramList(%d,%d)\n",
		staGetParam->program, staGetParam->getParamValue,
		paramList->getParamType, paramList->rssi);

	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_GET_PARAMETER_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}


int wfaStaNfcAction(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	caStaNfcAction_t *getStaNfcAction = (caStaNfcAction_t *)caCmdBuf;  //uncomment and use it
	
	 printf("\n Entry wfaStaNfcAction... ");

	if(getStaNfcAction->nfcOperation == eNfcHandOver)
	{
		printf("\n NfcAction - HandOver... ");
	
	}
	else if(getStaNfcAction->nfcOperation == eNfcReadTag)
	{
		printf("\n NfcAction - Read Tag... ");

	}
	else if(getStaNfcAction->nfcOperation == eNfcWriteSelect)
	{
		printf("\n NfcAction - Write Select... ");
	
	}
	else if(getStaNfcAction->nfcOperation == eNfcWriteConfig)
	{
		printf("\n NfcAction - Write Config... ");
	
	}
	else if(getStaNfcAction->nfcOperation == eNfcWritePasswd)
	{
		printf("\n NfcAction - Write Password... ");
	
	}
	else if(getStaNfcAction->nfcOperation == eNfcWpsHandOver)
	{
		printf("\n NfcAction - WPS Handover... ");
	
	}
	
	 // Fetch the device mode and put in	 infoResp->cmdru.p2presult 
	 //strcpy(infoResp->cmdru.p2presult, "GO");
	
	 // Fetch the device grp id and put in	 infoResp->cmdru.grpid 
	 //strcpy(infoResp->cmdru.grpid, "AA:BB:CC:DD:EE:FF_DIRECT-SSID");
	 
	 strcpy(infoResp.cmdru.staNfcAction.result, "CLIENT");
	 strcpy(infoResp.cmdru.staNfcAction.grpId, "AA:BB:CC:DD:EE:FF_DIRECT-SSID");
	 infoResp.cmdru.staNfcAction.peerRole = 1;
	
	


	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_NFC_ACTION_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}

int wfaStaExecAction(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	caStaExecAction_t *staExecAction = (caStaExecAction_t *)caCmdBuf;  //comment if not used
	
	 printf("\n Entry wfaStaExecAction... ");

	if(staExecAction->prog == PROG_TYPE_NAN)
	{
		// Perform necessary configurations and actions
		// return the MAC address conditionally as per CAPI specification
	}
	
	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_EXEC_ACTION_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}

int wfaStaInvokeCommand(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	caStaInvokeCmd_t *staInvokeCmd = (caStaInvokeCmd_t *)caCmdBuf;  //uncomment and use it
	
	 printf("\n Entry wfaStaInvokeCommand... ");


	 // based on the command type , invoke API or complete the required procedures
	 // return the  defined parameters based on the command that is received ( example response below)

	if(staInvokeCmd->cmdType == ePrimitiveCmdType && staInvokeCmd->InvokeCmds.primtiveType.PrimType == eCmdPrimTypeAdvt )
	{
		 infoResp.cmdru.staInvokeCmd.invokeCmdRspType = eCmdPrimTypeAdvt;
		 infoResp.cmdru.staInvokeCmd.invokeCmdResp.advRsp.numServInfo = 1;
		 strcpy(infoResp.cmdru.staInvokeCmd.invokeCmdResp.advRsp.servAdvInfo[0].servName,"org.wi-fi.wfds.send.rx");
		 infoResp.cmdru.staInvokeCmd.invokeCmdResp.advRsp.servAdvInfo[0].advtID = 0x0000f;
		 strcpy(infoResp.cmdru.staInvokeCmd.invokeCmdResp.advRsp.servAdvInfo[0].serviceMac,"ab:cd:ef:gh:ij:kl");
	}
	else if (staInvokeCmd->cmdType == ePrimitiveCmdType && staInvokeCmd->InvokeCmds.primtiveType.PrimType == eCmdPrimTypeSeek)
	{
		infoResp.cmdru.staInvokeCmd.invokeCmdRspType = eCmdPrimTypeSeek;
		infoResp.cmdru.staInvokeCmd.invokeCmdResp.seekRsp.searchID = 0x000ff;	
	}
	else if (staInvokeCmd->cmdType == ePrimitiveCmdType && staInvokeCmd->InvokeCmds.primtiveType.PrimType == eCmdPrimTypeConnSession)
	{
		infoResp.cmdru.staInvokeCmd.invokeCmdRspType = eCmdPrimTypeConnSession;
		infoResp.cmdru.staInvokeCmd.invokeCmdResp.connSessResp.sessionID = 0x000ff;  
		strcpy(infoResp.cmdru.staInvokeCmd.invokeCmdResp.connSessResp.result,"GO");
		strcpy(infoResp.cmdru.staInvokeCmd.invokeCmdResp.connSessResp.grpId,"DIRECT-AB WFADUT");
	
	}	
	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_INVOKE_CMD_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}


int wfaStaManageService(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	//caStaMngServ_t *staMngServ = (caStaMngServ_t *)caCmdBuf;  //uncomment and use it
	
	 printf("\n Entry wfaStaManageService... ");

	// based on the manage service type , invoke API's or complete the required procedures
	// return the  defined parameters based on the command that is received ( example response below)
	strcpy(infoResp.cmdru.staManageServ.result, "CLIENT");
	strcpy(infoResp.cmdru.staManageServ.grpId, "AA:BB:CC:DD:EE:FF_DIRECT-SSID");
    infoResp.cmdru.staManageServ.sessionID = 0x000ff;

	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_MANAGE_SERVICE_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}


	
int wfaStaGetEvents(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	caStaGetEvents_t *staGetEvents = (caStaGetEvents_t *)caCmdBuf;  //uncomment and use it
	
	 printf("\n Entry wfaStaGetEvents... ");
	 
	 if(staGetEvents->program == PROG_TYPE_NAN)
	{ 
		// Get all the events from the Log file or stored events
		// return the  received/recorded event details - eventName, remoteInstanceID, localInstanceID, mac
	}

	// Get all the event from the Log file or stored events
	// return the  received/recorded events as space seperated list   ( example response below)
	strcpy(infoResp.cmdru.staGetEvents.result, "SearchResult SearchTerminated AdvertiseStatus SessionRequest ConnectStatus SessionStatus PortStatus");
	
	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_GET_EVENTS_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}

int wfaStaGetEventDetails(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{

	dutCmdResponse_t infoResp;
	caStaGetEventDetails_t *getStaGetEventDetails = (caStaMngServ_t *)caCmdBuf;  //uncomment and use it
	
	 printf("\n Entry wfaStaGetEventDetails... ");


	 // based on the Requested Event type
	 // return the latest corresponding evnet detailed parameters  ( example response below)

	if(getStaGetEventDetails->eventId== eSearchResult )
	{
		// fetch from log file or event history for the search result event and return the parameters
		infoResp.cmdru.staGetEventDetails.eventID= eSearchResult;

		infoResp.cmdru.staGetEventDetails.getEventDetails.searchResult.searchID = 0x00abcd;
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.searchResult.serviceMac,"ab:cd:ef:gh:ij:kl");
		infoResp.cmdru.staGetEventDetails.getEventDetails.searchResult.advID = 0x00dcba;
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.searchResult.serviceName,"org.wi-fi.wfds.send.rx");

		infoResp.cmdru.staGetEventDetails.getEventDetails.searchResult.serviceStatus = eServiceAvilable;
	}
	else if (getStaGetEventDetails->eventId == eSearchTerminated)
	{		// fetch from log file or event history for the search terminated event and return the parameters
		infoResp.cmdru.staGetEventDetails.eventID= eSearchTerminated;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.searchTerminated.searchID = 0x00abcd;
	}
	else if (getStaGetEventDetails->eventId == eAdvertiseStatus)
	{// fetch from log file or event history for the Advertise Status event and return the parameters
		infoResp.cmdru.staGetEventDetails.eventID= eAdvertiseStatus;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.advStatus.advID = 0x00dcba;

		infoResp.cmdru.staGetEventDetails.getEventDetails.advStatus.status = eAdvertised;	
	}	
	else if (getStaGetEventDetails->eventId == eSessionRequest)
	{// fetch from log file or event history for the session request event and return the parameters
		infoResp.cmdru.staGetEventDetails.eventID= eSessionRequest;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.sessionReq.advID = 0x00dcba;
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.sessionReq.sessionMac,"ab:cd:ef:gh:ij:kl");
		infoResp.cmdru.staGetEventDetails.getEventDetails.sessionReq.sessionID = 0x00baba;	
	}	
	else if (getStaGetEventDetails->eventId ==eSessionStatus )
	{// fetch from log file or event history for the session status event and return the parameters
		infoResp.cmdru.staGetEventDetails.eventID= eSessionStatus;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.sessionStatus.sessionID = 0x00baba;	
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.sessionStatus.sessionMac,"ab:cd:ef:gh:ij:kl");
		infoResp.cmdru.staGetEventDetails.getEventDetails.sessionStatus.state = eSessionStateOpen;	
	}	
	else if (getStaGetEventDetails->eventId == eConnectStatus)
	{
		infoResp.cmdru.staGetEventDetails.eventID= eConnectStatus;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.connStatus.sessionID = 0x00baba;	
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.connStatus.sessionMac,"ab:cd:ef:gh:ij:kl");
		infoResp.cmdru.staGetEventDetails.getEventDetails.connStatus.status = eGroupFormationComplete;	
	
	}	
	else if (getStaGetEventDetails->eventId == ePortStatus)
	{
		infoResp.cmdru.staGetEventDetails.eventID= ePortStatus;	
		infoResp.cmdru.staGetEventDetails.getEventDetails.portStatus.sessionID = 0x00baba;	
		strcpy(infoResp.cmdru.staGetEventDetails.getEventDetails.portStatus.sessionMac,"ab:cd:ef:gh:ij:kl");
		infoResp.cmdru.staGetEventDetails.getEventDetails.portStatus.port = 1009;
		infoResp.cmdru.staGetEventDetails.getEventDetails.portStatus.status = eLocalPortAllowed;	
	}	



	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_GET_EVENT_DETAILS_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf); 
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);
	
   return WFA_SUCCESS;
}

	


