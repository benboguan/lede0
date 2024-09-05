//
// Created by boalin on 7/25/17.
//

/****************************************************************************
Copyright (c) 2016 Wi-Fi Alliance.  All Rights Reserved

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

/*
 * AP Sigma Daemon main implementation code, it will either change .dat file or call iwpriv cmd
 * Managed and maintained by MUS_CSD_CSD4_SD17 Yanfang Liu
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>	/* just for caddr_t */
#include <linux/socket.h>
#include <linux/wireless.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
//#include <net/if.h>

/* Fixup to be able to include kernel includes in userspace.
 * Basically, kill the sparse annotations... Jean II */
#ifndef __user
#define __user
#endif

#include "mtk_ap.h"
#include "mtk_parse.h"
#include "mtk_dict.h"
#include "wfa_sock.h"
#include "wfa_main.h"
#include "wfa_debug.h"

#define CERTIFICATES_PATH    "/etc/wpa_supplicant"

#define PROFILE_TMP_FILE    "/etc/wireless/mediatek/mt7915.tmp.dat"

char gCmdStr[WFA_CMD_STR_SZ];
char gTmpChar1[250];
char gTmpChar2[250];

char Device_Ver[60] = "1.0.0";
char Device_Ver_CAPI[40] = "1.0.0";
const char *CA_Ver = "ax_2.3.2";
char program[10] = "HE";
char AuthModeBSSID[5][20] = {0};
char EncryptBSSID[5][10] = {0};
char PROFILE_2G_FILE[60] = "/etc/wireless/mediatek/mt7915.1.dat";
char PROFILE_5G_FILE[60] = "/etc/wireless/mediatek/mt7915.2.dat";

int Force_UnsolicitedProbeResp = 0;


/* Some device may only support UDP ECHO, activate this line */
//#define WFA_PING_UDP_ECHO_ONLY 1

#define WFA_ENABLED 1

extern unsigned short wfa_defined_debug;
extern dict_t global_interface1_dat_dict;
extern dict_t global_interface2_dat_dict;
extern dict_t global_interface3_dat_dict;
extern dict_t global_key_dict;
extern char gIPaddr[20];

static int streamId = 0;

int device_get_ver()
{
    FILE *fp;
    char *start, *next;
    char tmp[15];
 
    //printf("===== running %s function ===== \n", __func__);

    sprintf(gCmdStr, "iwpriv ra0 get_driverinfo\n");
    //DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
    fp = popen(gCmdStr, "r");
    if (fp == NULL) {
        printf("Failed to run command!!!\n");
        return WFA_ERROR;
    }
 
    while (fgets(gCmdStr, sizeof(gCmdStr), fp) != NULL) {
        start = strstr(gCmdStr, "version:");
        if(start == NULL)
            continue;
        /* Replace the last charactor from '\n' to '\0' */
        start = start + 9;
        start[strlen(start)-2] = '\0';
        sprintf(Device_Ver, "drv_%s-", start);
        break;
    }

    while (fgets(gCmdStr, sizeof(gCmdStr), fp) != NULL) {
        start = strstr(gCmdStr, "FW ver:");
        if(start == NULL)
            continue;
        start = start + 10;
        next = strstr(start, ",");
        strncpy(tmp, start, next-start);
        tmp[next-start] = '\0';
        sprintf(Device_Ver_CAPI, "%sfw_%s", Device_Ver, tmp);
        sprintf(Device_Ver, "%sfw_%s-", Device_Ver, tmp);
        //strcpy(Device_Ver_CAPI, Device_Ver);
        //Device_Ver_CAPI[strlen(Device_Ver_CAPI)-1] = '\0';

        start = strstr(next, "HW ver:");
        if(start == NULL)
            break;
        start = start + 10;
        next = strstr(start, ",");
        strncpy(tmp, start, next-start);
        tmp[next-start] = '\0';
        sprintf(Device_Ver, "%shw_%s-", Device_Ver, tmp);

        start = strstr(next, "CHIP ID:");
        if(start == NULL)
            break;
        next = start + 11;
        next[strlen(next)] = '\0';
        sprintf(Device_Ver, "%schip_%s", Device_Ver, next);

        break;
    }
    pclose(fp);

    return WFA_SUCCESS;
}


// private funciton for mtk_ap.c
static void read_file_to_dict(dict_t local_d, FILE* file)
{
    char line[256];
    char key[64], value[64];
    char * token;

    // get rid of the first 2 lines

    if( fgets(line, sizeof(line), file) == NULL) {
        printf("error\n");
    }

    if( fgets(line, sizeof(line), file) == NULL) {
        printf("error\n");
    }

    while (fgets(line, sizeof(line), file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        token = strtok(line, "=");
        strcpy(key, token);
        token = strtok(NULL, "=");
	if (token) {
           strcpy(value, token);
           strcat(key, "=");
           dict_insert(local_d, key, value);
	}
    }
}


static void update_dat(dict_t commit_table, const char* read_file_name)
{
    FILE* write_file;
    FILE* read_file;
    char line[256];
    char key[64], value[128];
    char * token;
    int i;
    struct elt *e;

    write_file = fopen(PROFILE_TMP_FILE, "w");
    if(write_file == NULL) {
        printf("Can not find temp file %s\n", PROFILE_TMP_FILE);
        return;
    }

    read_file = fopen(read_file_name, "r");
    if(read_file == NULL) {
        printf("Can not find read file %s\n", read_file_name);
        return;
    }

    // write the first 2 lines
    if (fgets(line, sizeof(line), read_file)) {
        fprintf(write_file, "%s", line);
    }
    if (fgets(line, sizeof(line), read_file)) {
        fprintf(write_file, "%s", line);
    }
    // read files

    while (fgets(line, sizeof(line), read_file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        token = strtok(line, "=");
        strcpy(key, token);
        strcat(key, "=");
        if (dict_search(commit_table, key) != 0) {
            strcpy(value, dict_search(commit_table, key));
            fprintf(write_file, "%s%s", key, value);
            dict_delete(commit_table, key);
        }
    }


    for(i = 0; i <commit_table->size; i++) {
        for(e = commit_table->table[i]; e != 0; e = e->next) {
            fprintf(write_file, "%s%s", e->key, e->value);
        }
    }

    fclose(read_file);
    fclose(write_file);

    sprintf(gCmdStr, "cp %s %s", PROFILE_TMP_FILE, read_file_name);
    system(gCmdStr );
    system("sync");
    sprintf(gCmdStr, "rm %s", PROFILE_TMP_FILE);
    system(gCmdStr );
    dict_destroy(commit_table);
}

static void parse_semi_colon (char* str, char** val1, char** val2)
{
    if (strchr(str,';') != NULL) {
        str = strtok(str, ";");
        *val1 = strdup(str);
        str = strtok(NULL, ";");
        *val2 = strdup(str);
    } else {
        printf("no colon\n");
        *val1 = strdup(str);
        *val2 = strdup(str);
    }
}

static void free_mtk_ap_buf(mtk_ap_buf_t* mtk_ap_buf)
{
    int i;

    for (i = 0; i < mtk_ap_buf->capi_data->count; i++) {
        // free CAPI data
        free((mtk_ap_buf->capi_data->params)[i]);
        free((mtk_ap_buf->capi_data->values)[i]);
    }
}

int RT_ioctl(
        int             sid,
        int             param,
        char          *data,
        int             data_len,
        char          *intf_name,
        int             flags)
{
    int     ret = 1;
    struct iwreq    wrq;

    sprintf(wrq.ifr_name, "%s", intf_name);

    wrq.u.data.flags = flags;
    wrq.u.data.length = data_len;
    wrq.u.data.pointer = (caddr_t) data;

    ret = ioctl(sid, param, &wrq);

    return ret;
}

static void str_lower(char* str)
{
    int i = 0;
    char c;

    while(str[i]) {
        c = tolower(str[i]);
        str[i] = c;
        i++;
    }
}

static void init_intf_default_param(uint8_t *ap_buf, wifi_mode mode)
{
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;

    mtk_ap_buf->WappEnable = 0;
    if (mode == WIFI_2G) {
        mtk_ap_buf->intf_2G.mode= WIFI_2G;
        mtk_ap_buf->intf_2G.mbss_en = 0;
        mtk_ap_buf->intf_2G.bss_idx = 0;
        mtk_ap_buf->intf_2G.bss_num = 0;
        mtk_ap_buf->intf_2G.WLAN_TAG_bss_num = 0;
        mtk_ap_buf->intf_2G.security_set = 0;
        mtk_ap_buf->intf_2G.UL_MUMIMO= 0;
        mtk_ap_buf->intf_2G.DL= 0;
        memset(&mtk_ap_buf->intf_2G.AuthModeBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_2G.AuthModeBSSID));
        memset(&mtk_ap_buf->intf_2G.EncryptBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_2G.EncryptBSSID));
        strcpy(mtk_ap_buf->intf_2G.PMF_MFPC, "");
        strcpy(mtk_ap_buf->intf_2G.PMF_MFPR, "");
        strcpy(mtk_ap_buf->intf_2G.PWDIDR, "");
        memset(mtk_ap_buf->intf_2G.SSID, 0, sizeof(mtk_ap_buf->intf_2G.SSID));
        printf("Init 2G interface parameters!\n");
        return;
    } 
    if (mode == WIFI_5G) {
        mtk_ap_buf->intf_5G.mode= WIFI_5G;
        mtk_ap_buf->intf_5G.mbss_en = 0;
        mtk_ap_buf->intf_5G.bss_idx = 0;
        mtk_ap_buf->intf_5G.bss_num = 0;
        mtk_ap_buf->intf_5G.WLAN_TAG_bss_num = 0;
        mtk_ap_buf->intf_5G.security_set = 0;
        mtk_ap_buf->intf_5G.UL_MUMIMO= 0;
        mtk_ap_buf->intf_5G.DL= 0;
        memset(&mtk_ap_buf->intf_5G.AuthModeBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_5G.AuthModeBSSID));
        memset(&mtk_ap_buf->intf_5G.EncryptBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_5G.EncryptBSSID));
        strcpy(mtk_ap_buf->intf_5G.PMF_MFPC, "");
        strcpy(mtk_ap_buf->intf_5G.PMF_MFPR, "");
        strcpy(mtk_ap_buf->intf_5G.PWDIDR, "");
        memset(mtk_ap_buf->intf_5G.SSID, 0, sizeof(mtk_ap_buf->intf_5G.SSID));
        printf("Init 5G interface parameters!\n");
        return;
    }

    if (mode == WIFI_6G) {
        mtk_ap_buf->intf_6G.mode= WIFI_6G;
        mtk_ap_buf->intf_6G.mbss_en = 0;
        mtk_ap_buf->intf_6G.bss_idx = 0;
        mtk_ap_buf->intf_6G.bss_num = 0;
        mtk_ap_buf->intf_6G.WLAN_TAG_bss_num = 0;
        mtk_ap_buf->intf_6G.security_set = 0;
        mtk_ap_buf->intf_6G.UL_MUMIMO= 0;
        mtk_ap_buf->intf_6G.DL= 0;
        memset(&mtk_ap_buf->intf_6G.AuthModeBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_6G.AuthModeBSSID));
        memset(&mtk_ap_buf->intf_6G.EncryptBSSID[0][0], '\0',
            sizeof(mtk_ap_buf->intf_6G.EncryptBSSID));
        strcpy(mtk_ap_buf->intf_6G.PMF_MFPC, "");
        strcpy(mtk_ap_buf->intf_6G.PMF_MFPR, "");
        strcpy(mtk_ap_buf->intf_6G.PWDIDR, "");
        memset(mtk_ap_buf->intf_6G.SSID, 0, sizeof(mtk_ap_buf->intf_6G.SSID));
        printf("Init 6G interface parameters!\n");
        return;
    }
}

static void set_default_intf(uint8_t *ap_buf, wifi_mode mode)
{
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;

    if (mode == WIFI_2G) {
        mtk_ap_buf->intf_2G.status = 1;
        mtk_ap_buf->def_mode = WIFI_2G;
        mtk_ap_buf->commit_dict = mtk_ap_buf->intf_2G.dict_table;
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_2G);
        printf("Set default interface to 2G!\n");
        return;
    }
    if (mode == WIFI_5G) {
        mtk_ap_buf->intf_5G.status = 1;
        mtk_ap_buf->def_mode = WIFI_5G;
        mtk_ap_buf->commit_dict = mtk_ap_buf->intf_5G.dict_table;
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_5G);
        printf("Set default interface to 5G!\n");
        return;
    }
    if (mode == WIFI_6G) {
        mtk_ap_buf->intf_6G.status = 1;
        mtk_ap_buf->def_mode = WIFI_6G;
        mtk_ap_buf->commit_dict = mtk_ap_buf->intf_6G.dict_table;
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_6G);
        printf("Set default interface to 6G!\n");
        return;
    }
}

int init_profile_name(mtk_ap_buf_t* mtk_ap_buf)
{
    int i  = 0;
    int num, ret;
    char line[256];
    char *start, *end, *p;
    FILE* file;
    int search_profile = 1;

    printf("Open %s\n", PROFILE_INF);
    file = fopen(PROFILE_INF, "r");
    if(file == NULL) {
        printf("Can't find profile info file: %s, use default name\n", PROFILE_INF);
        return WFA_FAILURE;
    }

    ret = WFA_SUCCESS;
    while(fgets(line, sizeof(line), file))
    {
        /* Skip empty or almost empty lines. It seems that in some
         * cases fgets return a line with only a newline. */
        if((line[0] == '\0') || (line[1] == '\0') || (sizeof(line) < 7))
            continue;

        if (search_profile) {
            start = strstr(line, "profile_path=");
            if(start == NULL)
                continue;
            /* Replace the last charactor from '\n' to '\0' */
            start[strlen(start)-1] = '\0';
            start = start + 13;
            end = strstr(start, ";");
            if(end == NULL) {
                strcpy(mtk_ap_buf->profile_names[i].profile, start);
                printf("Profile name %d: %s \n", i, mtk_ap_buf->profile_names[i].profile);

                strcpy(mtk_ap_buf->profile_names[i].profile_bak, start);
                strcat(mtk_ap_buf->profile_names[i].profile_bak, ".bak");
                printf("Profile bak name: %s\n", mtk_ap_buf->profile_names[i].profile_bak);
                strcpy(mtk_ap_buf->profile_names[i].profile_cmt, start);
                strcat(mtk_ap_buf->profile_names[i].profile_cmt, ".cmt");
                printf("Profile name for last committed: %s\n", mtk_ap_buf->profile_names[i].profile_cmt);

                /* Extract profile number */
                p = strstr(start, ".dat");
                p[0] = '\0';
                num = atoi(--p);
                if ((num < 1) ||(num > 2)) {
                    ret = WFA_FAILURE;
                    break;
                }

                sprintf(gCmdStr, "/etc/wireless/mediatek/wifi_cert.%d.dat", num);
                if (access(gCmdStr, F_OK) == -1) {
                    sprintf(gCmdStr, "/etc/wireless/sigma_test/wifi_cert_b%d.dat", num-1);
                }
                printf("Sigma DUT profile name: %s\n", gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i].sigma_dut_profile, gCmdStr);

                mtk_ap_buf->tb_profile_exist = 1;
                sprintf(gCmdStr, "/etc/wireless/mediatek/wifi_cert_tb.%d.dat", num);
                if (access(gCmdStr, F_OK) == -1) {
                    strcpy(gCmdStr, mtk_ap_buf->profile_names[i].sigma_dut_profile);
                    mtk_ap_buf->tb_profile_exist = 0;
                }
                printf("Sigma Testbed profile name: %s\n", gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i].sigma_tb_profile, gCmdStr);
            } else {
                end[0] = '\0';
                strcpy(mtk_ap_buf->profile_names[i].profile, start);
                printf("Profile name %d: %s \n", i, mtk_ap_buf->profile_names[i].profile);
                strcpy(mtk_ap_buf->profile_names[i].profile_bak, start);
                strcat(mtk_ap_buf->profile_names[i].profile_bak, ".bak");
                printf("Profile bak name: %s\n", mtk_ap_buf->profile_names[i].profile_bak);
                strcpy(mtk_ap_buf->profile_names[i].profile_cmt, start);
                strcat(mtk_ap_buf->profile_names[i].profile_cmt, ".cmt");
                printf("Profile name for last committed: %s\n", mtk_ap_buf->profile_names[i].profile_cmt);

                p = strstr(start, ".dat");
                p[0] = '\0';
                num = atoi(--p);
                if ((num < 0) ||(num > 1)) {
                    ret = WFA_FAILURE;
                    break;
                }
                sprintf(gCmdStr, "/etc/wireless/mediatek/wifi_cert_b%d.dat", num);
                if (access(gCmdStr, F_OK) == -1) {
                    sprintf(gCmdStr, "/etc/wireless/sigma_test/wifi_cert_b%d.dat", num);
                }
                printf("Sigma profile name: %s\n", gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i].sigma_dut_profile, gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i].sigma_tb_profile, gCmdStr);

                end++;
                strcpy(mtk_ap_buf->profile_names[i+1].profile, end);
                printf("Profile name %d: %s \n", i+1, mtk_ap_buf->profile_names[i+1].profile);
                strcpy(mtk_ap_buf->profile_names[i+1].profile_bak, end);
                strcat(mtk_ap_buf->profile_names[i+1].profile_bak, ".bak");
                printf("Profile bak name: %s\n", mtk_ap_buf->profile_names[i+1].profile_bak);
                strcpy(mtk_ap_buf->profile_names[i+1].profile_cmt, end);
                strcat(mtk_ap_buf->profile_names[i+1].profile_cmt, ".cmt");
                printf("Profile name for last committed: %s\n", mtk_ap_buf->profile_names[i+1].profile_cmt);

                p = strstr(end, ".dat");
                p[0] = '\0';
                num = atoi(--p);
                if ((num < 0) ||(num > 1)) {
                    ret = WFA_FAILURE;
                    break;
                }
                sprintf(gCmdStr, "/etc/wireless/mediatek/wifi_cert_b%d.dat", num);
                if (access(gCmdStr, F_OK) == -1) {
                    sprintf(gCmdStr, "/etc/wireless/sigma_test/wifi_cert_b%d.dat", num);
                }
                printf("Sigma profile name: %s\n", gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i+1].sigma_dut_profile, gCmdStr);
                strcpy(mtk_ap_buf->profile_names[i+1].sigma_tb_profile, gCmdStr);
            }
            search_profile = 0;
            continue;
        } else {
            start = strstr(line, "main_ifname=");
            if(start == NULL)
                continue;
            /* Replace the last charactor from '\n' to '\0' */
            start[strlen(start)-1] = '\0';
            start = start + 12;
            end = strstr(start, ";");
            if(end == NULL) {
                strcpy(mtk_ap_buf->profile_names[i].name, start);
                printf("interface %d: %s \n\n", i, mtk_ap_buf->profile_names[i].name);;
                i++;
            } else {
                end[0] = '\0';
                strcpy(mtk_ap_buf->profile_names[i].name, start);
                printf("interface %d: %s \n", i, mtk_ap_buf->profile_names[i].name);
                i++;
                strcpy(mtk_ap_buf->profile_names[i].name, end+1);
                printf("interface %d: %s \n\n", i, mtk_ap_buf->profile_names[i].name);
                i++;
            }
            search_profile = 1;
            if (i >= INTF_NUM)
                break;
        }
    }
    fclose (file);
    return ret;
}

static void turn_interface_down_up(const char* interface)
{
    struct ifreq intf_req;
    int sock_fd;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&intf_req, sizeof(intf_req));
    strncpy(intf_req.ifr_name, interface, IFNAMSIZ);
    ioctl(sock_fd, SIOCGIFFLAGS, &intf_req);

    // setting the flag to 0
    intf_req.ifr_flags &= ~IFF_UP;
    printf("turn interface %s down!\n", interface);
    ioctl(sock_fd, SIOCSIFFLAGS, &intf_req);
    printf("it is down, now turn it on !\n");
    // setting the flag up
    intf_req.ifr_flags |= IFF_UP;
    ioctl(sock_fd, SIOCSIFFLAGS, &intf_req);
    printf("interface %s should be up now!\n", interface);

    close(sock_fd);
}


static void turn_interface_down(const char* interface)
{
    struct ifreq intf_req;
    int sock_fd;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&intf_req, sizeof(intf_req));
    strncpy(intf_req.ifr_name, interface, IFNAMSIZ);
    ioctl(sock_fd, SIOCGIFFLAGS, &intf_req);
    // set the flag to 0
    intf_req.ifr_flags &= ~IFF_UP;
    printf("turn interface:%s down!\n", interface);
    ioctl(sock_fd, SIOCSIFFLAGS, &intf_req);
    //printf("interface is down!\n");
    close(sock_fd);
}


static void turn_interface_up(const char* interface)
{
    struct ifreq intf_req;
    int sock_fd;
    int res = 0;
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&intf_req, sizeof(intf_req));
    strncpy(intf_req.ifr_name, interface, IFNAMSIZ);
    ioctl(sock_fd, SIOCGIFFLAGS, &intf_req);

    // setting the interface to 1
    intf_req.ifr_flags |= IFF_UP;
    res = ioctl(sock_fd, SIOCSIFFLAGS, &intf_req);
    printf("ioctl result %d!\n", res);
    close(sock_fd);
}

static void ifconfig_interface_down(const char* interface)
{
    sprintf(gCmdStr, "ifconfig %s down", interface);
    printf("%s\n", gCmdStr);
    system(gCmdStr );
}

static void ifconfig_interface_up(const char* interface)
{
    sprintf(gCmdStr, "ifconfig %s up", interface);
    printf("%s\n", gCmdStr);
    system(gCmdStr );
}

static void read_mac_address_file(char* mac_address_buf, char* interface)
{

    char file_name[100];
    char line[256];
    FILE* file;

    //printf("enter read mac address file %s\n", interface);
    sprintf(file_name, "/sys/class/net/%s/address", interface);
    file = fopen(file_name, "r");
    if(file == NULL) {
        sprintf(gCmdStr,"interface %s not exist:\n", interface);
        strcpy(mac_address_buf, gCmdStr);
        return;
    }

    // write the first 2 lines
    if (fgets(line, sizeof(line), file)) {
        strcpy(mac_address_buf, line);
    }
    //printf("after read mac address file: %s\n", interface);
    fclose(file);
}

// END OF PRIVATE FUNCTION FOR MTK_AP.C
int is_wifi_interface_exist(int skfd, const char* interface)
{
    struct iwreq		wrq;

    memset((char *) &wrq, 0, sizeof(struct iwreq));
    strncpy(wrq.ifr_name, interface, IFNAMSIZ);
    if (ioctl(skfd, SIOCGIWNAME, &wrq) < 0)
        return 0;

    return 1;
 }

int is_interface_up(int skfd, const char* interface) {
    struct ifreq ifr;
    
    //int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
            perror("SIOCGIFFLAGS");
    }
    //close(sock);
    return !!(ifr.ifr_flags & IFF_UP);
}

int wifi_interface_chan(int skfd, const char* interface)
{
    struct iwreq		wrq;
 
    memset((char *) &wrq, 0, sizeof(struct iwreq));
    strncpy(wrq.ifr_name, interface, IFNAMSIZ);
    if (ioctl(skfd, SIOCGIWFREQ, &wrq) < 0)
        return -1;

    return (int)(wrq.u.freq.m);
}

char * wifi_interface_ssid(int skfd, const char* interface)
{
    int           fd;
    struct iwreq  wrq;
    char          essid[IW_ESSID_MAX_SIZE];

    if (!interface) return NULL;

    if (!skfd)
        fd = socket(AF_INET, SOCK_DGRAM, 0);
    else
        fd = skfd;

    strncpy (wrq.ifr_ifrn.ifrn_name, interface, IFNAMSIZ);
    memset (essid, 0, IW_ESSID_MAX_SIZE);
    wrq.u.essid.pointer = (caddr_t *) essid;
    wrq.u.data.length = IW_ESSID_MAX_SIZE;
    wrq.u.data.flags = 0;

    ioctl (fd, SIOCGIWESSID, &wrq);
    if (!skfd)
        close (fd);

    return strdup (essid);
}

void turn_all_interface_down_up(mtk_ap_buf_t* mtk_ap_buf)
{
    char intf[10];
    int wifi_intf_fd;
    int bss_num;
    int i;

    if ((wifi_intf_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        DPRINT_ERR(WFA_ERR, "create WIFI Interface socket() failed");
        return;
    }
    wifi_enum_devices(wifi_intf_fd, &check_turn_interface_down, mtk_ap_buf, 0);
    close(wifi_intf_fd);

    DPRINT_INFO(WFA_OUT, "Delay %d seconds before turn on interface!\n", mtk_ap_buf->cmd_cfg.intf_rst_delay);
    sleep(mtk_ap_buf->cmd_cfg.intf_rst_delay);

    if (mtk_ap_buf->intf_2G.status) {
        strcpy(intf, mtk_ap_buf->intf_2G.name);
    /* Somehow on 7622, call ioctl() to turn on interface will trap process into kernel mode, 
         then exception happens later when calling system(). The solution is to use 
         "system("ifconfig intf on")" instead of ioctl, although it is slower.*/
#if defined(CONFIG_CHIP_MT7622) || (CONFIG_CHIP_MT7622==y)
        ifconfig_interface_up(intf);
#else
        turn_interface_up(intf);
#endif

        if (mtk_ap_buf->intf_2G.mbss_en) {
            bss_num = max(mtk_ap_buf->intf_2G.bss_num, mtk_ap_buf->intf_2G.WLAN_TAG_bss_num);
            for (i = 1; i < bss_num; i++) {
                intf[strlen(intf)-1] = '0'+i;
                printf("non_tx_bss inf is %s\n", intf);
                turn_interface_up(intf);
                sleep(2);
                sprintf(gCmdStr, "brctl addif br-lan %s", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        }
    }

    if (mtk_ap_buf->intf_5G.status) {
        strcpy(intf, mtk_ap_buf->intf_5G.name);
#if defined(CONFIG_CHIP_MT7622) || (CONFIG_CHIP_MT7622==y)
        ifconfig_interface_up(intf);
#else
        turn_interface_up(intf);
#endif

        if (mtk_ap_buf->intf_5G.mbss_en) {
            bss_num = max(mtk_ap_buf->intf_5G.bss_num, mtk_ap_buf->intf_5G.WLAN_TAG_bss_num);
            for (i = 1; i < bss_num; i++) {
                intf[strlen(intf)-1] = '0'+i;
                printf("non_tx_bss inf is %s\n", intf);
                turn_interface_up(intf);
                sleep(2);
                sprintf(gCmdStr, "brctl addif br-lan %s", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
        }
    }

    if (mtk_ap_buf->intf_6G.status) {
        strcpy(intf, mtk_ap_buf->intf_6G.name);
#if defined(CONFIG_CHIP_MT7622) || (CONFIG_CHIP_MT7622==y)
        ifconfig_interface_up(intf);
#else
        turn_interface_up(intf);
#endif

        if (mtk_ap_buf->intf_6G.mbss_en) {
            bss_num = max(mtk_ap_buf->intf_6G.bss_num, mtk_ap_buf->intf_6G.WLAN_TAG_bss_num);
            for (i = 1; i < bss_num; i++) {
                intf[strlen(intf)-1] = '0'+i;
                printf("non_tx_bss inf is %s\n", intf);
                turn_interface_up(intf);
                sleep(2);
                sprintf(gCmdStr, "brctl addif br-lan %s", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        }
    }
    if (mtk_ap_buf->cmd_cfg.post_intf_rst_delay) {
        DPRINT_INFO(WFA_OUT, "Delay %d seconds after interfaces turn on!\n", mtk_ap_buf->cmd_cfg.post_intf_rst_delay);
        sleep(mtk_ap_buf->cmd_cfg.post_intf_rst_delay);
    }

    return;
}

/*------------------------------------------------------------------*/
/*
 * Extract the interface name out of /proc/net/wireless or /proc/net/dev.
 */
static inline char *
wifi_get_ifname(char *	name,	/* Where to store the name */
	      int	nsize,	/* Size of name buffer */
	      char *	buf)	/* Current position in buffer */
{
  char *	end;

  /* Skip leading spaces */
  while(isspace(*buf))
    buf++;

  /* Get name up to ": "
   * Note : we compare to ": " to make sure to process aliased interfaces
   * properly. */
  end = strstr(buf, ": ");

  /* Not found ??? To big ??? */
  if((end == NULL) || (((end - buf) + 1) > nsize))
    return(NULL);

  /* Copy */
  memcpy(name, buf, (end - buf));
  name[end - buf] = '\0';

  /* Return value currently unused, just make sure it's non-NULL */
  return(end);
}

int check_turn_interface_down(int	skfd,
	   char *	ifname,
	   void *	args,
	   int		count)
{
    mtk_ap_buf_t* mtk_ap_buf;   
    int chan;

    /* Avoid "Unused parameter" warning */
    mtk_ap_buf = (mtk_ap_buf_t* )args; count = count;

    //printf("intf from /proc/net/wireless:%s.\n", ifname);
    if(is_wifi_interface_exist(skfd, ifname) && is_interface_up(skfd, ifname))
    {
                turn_interface_down(ifname);
    }

    return 1;
}

static int assign_profile_pointer_to_intf(mtk_ap_buf_t* mtk_ap_buf, char *ifname, intf_profile_t** profile_names)
{
    int i;
    for (i = 0; i < INTF_NUM; i++) {
        if (strcasecmp(ifname, mtk_ap_buf->profile_names[i].name) == 0){
            *profile_names = &mtk_ap_buf->profile_names[i];
            printf("Assign inerface %s with profile name: %s \n", ifname, (*profile_names)->profile);
            return 1;
        }
    }
    return 0;
}


int fillup_intf(int	skfd,
	   char *	ifname,
	   void *	args,
	   int		count)
{
    mtk_ap_buf_t* mtk_ap_buf;
    int chan;

    /* Avoid "Unused parameter" warning */
    mtk_ap_buf = (mtk_ap_buf_t* )args; count = count;

    if(is_wifi_interface_exist(skfd, ifname) && is_interface_up(skfd, ifname))
    {
        chan = wifi_interface_chan(skfd, ifname);

        if ((strcasecmp(ifname, "rai0") == 0) /* && !mtk_ap_buf->intf_6G.status */) {
            strcpy(mtk_ap_buf->intf_6G.name, ifname);
            printf("6G intf:%s, channel=%d\n", mtk_ap_buf->intf_6G.name, chan);
            /* Only enable 6G interface when CAPI request it*/
            //mtk_ap_buf->intf_6G.status = 1;
            mtk_ap_buf->intf_6G.mode = WIFI_6G;
            mtk_ap_buf->intf_6G.mbss_en = 0;

            if (!assign_profile_pointer_to_intf(mtk_ap_buf, mtk_ap_buf->intf_6G.name, &mtk_ap_buf->intf_6G.profile_names)) {
                printf("Profile name can't be found, use default!\n");
                return 0;
            }
        }

        if ((chan >= 1 && chan <=14) && !mtk_ap_buf->intf_2G.status) {
            strcpy(mtk_ap_buf->intf_2G.name, ifname);
            mtk_ap_buf->intf_2G.status = 1;
            mtk_ap_buf->intf_2G.mode = WIFI_2G;
            mtk_ap_buf->intf_2G.mbss_en = 0;
            printf("2G intf:%s, channel=%d\n", mtk_ap_buf->intf_2G.name, chan);

            if (!assign_profile_pointer_to_intf(mtk_ap_buf, mtk_ap_buf->intf_2G.name, &mtk_ap_buf->intf_2G.profile_names)) {
                printf("Profile name can't be found, use default!\n");
                return 0;
            }
        } else if((chan >= 36) && !mtk_ap_buf->intf_5G.status) {
            strcpy(mtk_ap_buf->intf_5G.name, ifname);
            printf("5G intf:%s, channel=%d\n", mtk_ap_buf->intf_5G.name, chan);
            mtk_ap_buf->intf_5G.status = 1;
            mtk_ap_buf->intf_5G.mode = WIFI_5G;
            mtk_ap_buf->intf_5G.mbss_en = 0;

            if (!assign_profile_pointer_to_intf(mtk_ap_buf, mtk_ap_buf->intf_5G.name, &mtk_ap_buf->intf_5G.profile_names)) {
                printf("Profile name can't be found, use default!\n");
                return 0;
            }
        }
    }
    return 1;
}

int wifi_enum_devices(int skfd,
		iw_enum_handler	fn,
		void *		args,
		int		reset_stat)
{
  char	buff[1024];
  FILE *	fh;
  struct ifconf ifc;
  struct ifreq *ifr;
  int		i;
  int   intf_found = 0;
  mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t* )args;

   if (reset_stat) {
       mtk_ap_buf->intf_2G.status = 0;
       mtk_ap_buf->intf_5G.status = 0;
   }
  /* Check if /proc/net/wireless is available */
  fh = fopen(PROC_NET_WIRELESS, "r");

  if(fh != NULL)
    {
      /* Success : use data from /proc/net/wireless */

      /* Eat 2 lines of header */
      fgets(buff, sizeof(buff), fh);
      fgets(buff, sizeof(buff), fh);

      /* Read each device line */
      while(fgets(buff, sizeof(buff), fh))
	{
	  char name[IFNAMSIZ + 1];
	  char *s;

	  /* Skip empty or almost empty lines. It seems that in some
	   * cases fgets return a line with only a newline. */
	  if((buff[0] == '\0') || (buff[1] == '\0'))
	    continue;

	  /* Extract interface name */
	  s = wifi_get_ifname(name, sizeof(name), buff);

	  if(!s)
	    {
	      /* Failed to parse, complain and continue */
	      fprintf(stderr, "Cannot parse " PROC_NET_WIRELESS "\n");
	    }
	  else {
	    /* Got it, print info about this interface */
            intf_found = 1;
	    (*fn)(skfd, name, args, 0);
            }
	}

        fclose(fh);
        if (intf_found) {
            if (mtk_ap_buf->intf_2G.status || mtk_ap_buf->intf_5G.status)
                return WFA_SUCCESS;
            else {
                DPRINT_INFO(WFA_OUT, "!!!No interface is up!Check and enable interface with 'ifconfig'\n");
                return WFA_ERROR;
            }
        }
        else {
            DPRINT_INFO(WFA_OUT, "!!!Can't find interface entry in %s!\n", PROC_NET_WIRELESS);
            return WFA_ERROR;
        }
    }else {
        DPRINT_INFO(WFA_OUT, "!!!Can't open %s!\n", PROC_NET_WIRELESS);
        return WFA_ERROR;
    }
}

int ap_init(mtk_ap_buf_t* mtk_ap_buf)
{
    if(global_interface1_dat_dict)
         dict_destroy(global_interface1_dat_dict);

     global_interface1_dat_dict = init_2g_dict(mtk_ap_buf);
     if(global_interface1_dat_dict == NULL) {
         printf("global_interface1_dat_dict fail.\n");
         return WFA_FAILURE;
    }
     printf("interface 2g dict1 load.\n");

    if(global_interface2_dat_dict)
         dict_destroy(global_interface2_dat_dict);
    
     global_interface2_dat_dict = init_5g_dict(mtk_ap_buf);
     if(global_interface2_dat_dict == NULL) {
         dict_destroy(global_interface1_dat_dict);
         printf("global_interface2_dat_dict fail.\n");
         return WFA_FAILURE;
    }
     printf("interface 5g dict2 load.\n");

    if (mtk_ap_buf->intf_6G.status) {
        if(global_interface3_dat_dict)
             dict_destroy(global_interface3_dat_dict);

         global_interface3_dat_dict = init_6g_dict(mtk_ap_buf);
         if(global_interface3_dat_dict == NULL) {
             dict_destroy(global_interface1_dat_dict);
             dict_destroy(global_interface2_dat_dict);
             printf("global_interface3_dat_dict fail.\n");
             return WFA_FAILURE;
        }
        printf("interface 6g dict3 load.\n");
        mtk_ap_buf->intf_6G.dict_table = global_interface3_dat_dict;
        init_intf_default_param(mtk_ap_buf, WIFI_6G);
    }

    mtk_ap_buf->intf_2G.dict_table = global_interface1_dat_dict;
    mtk_ap_buf->intf_5G.dict_table = global_interface2_dat_dict;
    mtk_ap_buf->key_translation_table = global_key_dict;
    init_intf_default_param(mtk_ap_buf, WIFI_2G);
    init_intf_default_param(mtk_ap_buf, WIFI_5G);

    if (mtk_ap_buf->intf_2G.status) {
        set_default_intf(mtk_ap_buf, WIFI_2G);
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(global_key_dict, "OWN_IP_ADDR"), gIPaddr);
        mtk_ap_buf->intf_2G.bss_num = atoi(dict_search(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(global_key_dict, "BssidNum")));
        if (mtk_ap_buf->intf_2G.bss_num > 1)
            mtk_ap_buf->intf_2G.mbss_en = 1;
    }

    if(mtk_ap_buf->intf_5G.status) {
        set_default_intf(mtk_ap_buf, WIFI_5G);
        dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(global_key_dict, "OWN_IP_ADDR"), gIPaddr);
        mtk_ap_buf->intf_5G.bss_num = atoi(dict_search(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(global_key_dict, "BssidNum")));
        if (mtk_ap_buf->intf_5G.bss_num > 1)
            mtk_ap_buf->intf_5G.mbss_en = 1;
    }

    if(mtk_ap_buf->intf_6G.status) {
        //set_default_intf(mtk_ap_buf, WIFI_5G);
        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "OWN_IP_ADDR"), gIPaddr);
        mtk_ap_buf->intf_6G.bss_num = atoi(dict_search(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "BssidNum")));
        if (mtk_ap_buf->intf_6G.bss_num > 1)
            mtk_ap_buf->intf_6G.mbss_en = 1;
    }
    
    if (!(mtk_ap_buf->intf_2G.status || mtk_ap_buf->intf_5G.status)) {
        DPRINT_ERR(WFA_ERR, "%s No valid interface!!!\n", __func__);
        dict_destroy(global_interface1_dat_dict);
        dict_destroy(global_interface2_dat_dict);
        if(mtk_ap_buf->intf_6G.status) {
            dict_destroy(global_interface3_dat_dict);
        }
        return WFA_ERROR;
    }

    mtk_ap_buf->WLAN_TAG = 0;
    mtk_ap_buf->post_cmd_idx = 0;
    strcpy(mtk_ap_buf->Reg_Domain, "");

    return WFA_SUCCESS;
}

int mtk_ap_exec(mtk_ap_buf_t* mtk_ap_buf, capi_data_t* capi_data,
                    uint8_t* resp_buf, int cmd_len, int* resp_len_ptr, int cmd_tag)
{
    retType_t status;
    mtk_ap_buf->capi_data = capi_data;

    printf("===== Run command =====\n");
    status = dut_tbl[cmd_tag].cmd_mtk(cmd_len, (uint8_t * ) mtk_ap_buf, resp_len_ptr, (uint8_t *) resp_buf);

    free_mtk_ap_buf(mtk_ap_buf);
    return status;
}

void backup_profile(mtk_ap_buf_t* mtk_ap_buf)
{
    FILE* file;

    if (mtk_ap_buf->intf_2G.status && !mtk_ap_buf->intf_5G.status) {
        strcpy(mtk_ap_buf->intf_5G.name, mtk_ap_buf->intf_2G.name);
        mtk_ap_buf->intf_5G.profile_names = mtk_ap_buf->intf_2G.profile_names;
    }
    if (!mtk_ap_buf->intf_2G.status && mtk_ap_buf->intf_5G.status) {
        strcpy(mtk_ap_buf->intf_2G.name, mtk_ap_buf->intf_5G.name);
        mtk_ap_buf->intf_2G.profile_names = mtk_ap_buf->intf_5G.profile_names;
    }

    if (mtk_ap_buf->intf_2G.status) {
        file = fopen(mtk_ap_buf->intf_2G.profile_names->profile_bak, "r");
        if(file == NULL) {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_2G.profile_names->profile,
                mtk_ap_buf->intf_2G.profile_names->profile_bak);
            system(gCmdStr );
            system("sync");
            DPRINT_INFO(WFA_OUT, "First time to use daemon, backup 2G profile to %s!\n",
                mtk_ap_buf->intf_2G.profile_names->profile_bak);
        } else {
            fclose(file);
        }
    }
    if (mtk_ap_buf->intf_5G.status) {
        file = fopen(mtk_ap_buf->intf_5G.profile_names->profile_bak, "r");
        if(file == NULL) {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_5G.profile_names->profile,
                mtk_ap_buf->intf_5G.profile_names->profile_bak);
            system(gCmdStr );
            system("sync");
            DPRINT_INFO(WFA_OUT, "First time to use daemon, backup 5G profile to %s!\n",
                mtk_ap_buf->intf_5G.profile_names->profile_bak);
        } else {
            fclose(file);
        }
    }

    if (mtk_ap_buf->intf_6G.status) {
        file = fopen(mtk_ap_buf->intf_6G.profile_names->profile_bak, "r");
        if(file == NULL) {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_6G.profile_names->profile,
                mtk_ap_buf->intf_6G.profile_names->profile_bak);
            system(gCmdStr );
            system("sync");
            DPRINT_INFO(WFA_OUT, "First time to use daemon, backup 6G profile to %s!\n",
                mtk_ap_buf->intf_6G.profile_names->profile_bak);
        } else {
            fclose(file);
        }
    }

    return;
 }

void apply_sigma_profile(mtk_ap_buf_t* mtk_ap_buf, device_type dev_type)
{
    if (dev_type == TESTBED) {
        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_2G.profile_names->sigma_tb_profile,
            mtk_ap_buf->intf_2G.profile_names->profile);
    } else {
        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_2G.profile_names->sigma_dut_profile,
            mtk_ap_buf->intf_2G.profile_names->profile);
    }
    system(gCmdStr );
    system("sync");

    if (strcasecmp(mtk_ap_buf->intf_2G.profile_names->profile,mtk_ap_buf->intf_5G.profile_names->profile) != 0) {
        if (dev_type == TESTBED) {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_5G.profile_names->sigma_tb_profile,
                mtk_ap_buf->intf_5G.profile_names->profile);
        } else {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_5G.profile_names->sigma_dut_profile,
                mtk_ap_buf->intf_5G.profile_names->profile);
        }
        system(gCmdStr );
        system("sync");
    }
    if (mtk_ap_buf->intf_6G.status) {
        if (dev_type == TESTBED) {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_6G.profile_names->sigma_tb_profile,
                mtk_ap_buf->intf_6G.profile_names->profile);
        } else {
            sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_6G.profile_names->sigma_dut_profile,
                mtk_ap_buf->intf_6G.profile_names->profile);
        }
        system(gCmdStr );
        system("sync");
    }

    DPRINT_INFO(WFA_OUT, "Apply default profile!\n");

    return;
}

void restore_profile(mtk_ap_buf_t* mtk_ap_buf)
{
    sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_2G.profile_names->profile,
        mtk_ap_buf->intf_2G.profile_names->profile_cmt);
    system(gCmdStr );
    system("sync");

    sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_2G.profile_names->profile_bak,
        mtk_ap_buf->intf_2G.profile_names->profile);
    system(gCmdStr );
    system("sync");

    if (strcasecmp(mtk_ap_buf->intf_2G.profile_names->profile,mtk_ap_buf->intf_5G.profile_names->profile) != 0) {
        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_5G.profile_names->profile,
            mtk_ap_buf->intf_5G.profile_names->profile_cmt);
        system(gCmdStr );
        system("sync");

        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_5G.profile_names->profile_bak,
            mtk_ap_buf->intf_5G.profile_names->profile);
        system(gCmdStr );
        system("sync");
    }

    if (mtk_ap_buf->intf_6G.status) {
        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_6G.profile_names->profile,
            mtk_ap_buf->intf_6G.profile_names->profile_cmt);
        system(gCmdStr );
        system("sync");

        sprintf(gCmdStr, "cp %s %s", mtk_ap_buf->intf_6G.profile_names->profile_bak,
            mtk_ap_buf->intf_6G.profile_names->profile);
        system(gCmdStr );
        system("sync");
    }

    DPRINT_INFO(WFA_OUT, "Restore profile!\n");

    return;
}

dict_t init_capi_key_dict ()
{
    dict_t local_d = dict_create();
    int k = 0;

    while(strcmp(capi_key_tbl[k].capi_name, "") != 0) {
        str_lower(capi_key_tbl[k].capi_name);
        dict_insert(local_d, capi_key_tbl[k].capi_name, capi_key_tbl[k].dat_name);
        k++;
    }

    return local_d;
}

dict_t init_2g_dict (mtk_ap_buf_t* mtk_ap_buf)
{
    dict_t local_d = dict_create();
    FILE* file;

    file = fopen(mtk_ap_buf->intf_2G.profile_names->profile, "r");
    if (!file) {
        printf("Open PROFILE_2G_FILE %s fail!\n", mtk_ap_buf->intf_2G.profile_names->profile);
        dict_destroy(local_d);
        return NULL;
    }

    read_file_to_dict(local_d, file);
    fclose(file);
    return local_d;
}

dict_t init_5g_dict (mtk_ap_buf_t* mtk_ap_buf)
{
    dict_t local_d = dict_create();
    FILE* file;

    file = fopen(mtk_ap_buf->intf_5G.profile_names->profile, "r");
    if (!file) {
        printf("Open PROFILE_5G_FILE %s fail!\n", mtk_ap_buf->intf_5G.profile_names->profile);
        dict_destroy(local_d);
        return NULL;
    }

    read_file_to_dict(local_d, file);
    fclose(file);
    return local_d;
}

dict_t init_6g_dict (mtk_ap_buf_t* mtk_ap_buf)
{
    dict_t local_d = dict_create();
    FILE* file;

    file = fopen(mtk_ap_buf->intf_6G.profile_names->profile, "r");
    if (!file) {
        printf("Open PROFILE_6G_FILE %s fail!\n", mtk_ap_buf->intf_6G.profile_names->profile);
        dict_destroy(local_d);
        return NULL;
    }

    read_file_to_dict(local_d, file);
    fclose(file);
    return local_d;
}

static int CAPI_set_intf(mtk_ap_buf_t* mtk_ap_buf)
{
    const char *drv_value;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    printf("(%s)\n", data->interface);

    drv_value = table_search_lower(E2pAccessMode_tbl, mtk_ap_buf->cmd_cfg.mode);
    if ((strcasecmp(data->interface, "5G") == 0) || (strcasecmp(data->interface, "5.0") == 0)) {
        if (mtk_ap_buf->intf_5G.status) {
            set_default_intf(mtk_ap_buf, WIFI_5G);
        } else if (mtk_ap_buf->intf_2G.status){//Single interface, swtich to another band
            set_default_intf(mtk_ap_buf, WIFI_5G);
            strcpy(mtk_ap_buf->intf_5G.name, mtk_ap_buf->intf_2G.name);
            mtk_ap_buf->intf_2G.status = 0;
            printf("Single interface, Switch 2G to 5G, interface=%s\n", mtk_ap_buf->intf_5G.name);
        } else {
            printf("No valid interface exist, skip!\n");
        }
    } else if ((strcasecmp(data->interface, "2G") == 0) ||(strcasecmp(data->interface, "24G") == 0)
        ||(strcasecmp(data->interface, "2.4") == 0)) {
        if (mtk_ap_buf->intf_2G.status) {
            set_default_intf(mtk_ap_buf, WIFI_2G);
         } else  if (mtk_ap_buf->intf_5G.status){//Single interface, swtich to another band
            set_default_intf(mtk_ap_buf, WIFI_2G);
             strcpy(mtk_ap_buf->intf_2G.name, mtk_ap_buf->intf_5G.name);
             mtk_ap_buf->intf_5G.status = 0;
             printf("Single interface, Switch 5G to 2G, interface=%s\n", mtk_ap_buf->intf_2G.name);
        } else {
            printf("No valid interface exist, skip!\n");
        }
    } else if (strcasecmp(data->interface, "6G") == 0) {
        if (strcasecmp(mtk_ap_buf->intf_6G.name, "rai0") == 0) {
            if (!mtk_ap_buf->intf_6G.status) {
                if (mtk_ap_buf->intf_2G.status ||mtk_ap_buf->intf_5G.status) {
                    strcpy(mtk_ap_buf->intf_5G.name, mtk_ap_buf->intf_2G.name);
                    mtk_ap_buf->intf_5G.status = 1;
                    mtk_ap_buf->intf_2G.status = 0;
                    if (!assign_profile_pointer_to_intf(mtk_ap_buf, mtk_ap_buf->intf_5G.name, &mtk_ap_buf->intf_5G.profile_names)) {
                        printf("Profile name can't be found, use default!\n");
                        return 0;
                    }
                }
                if(global_interface3_dat_dict)
                     dict_destroy(global_interface3_dat_dict);

                 global_interface3_dat_dict = init_6g_dict(mtk_ap_buf);
                 if(global_interface3_dat_dict == NULL) {
                     dict_destroy(global_interface1_dat_dict);
                     dict_destroy(global_interface2_dat_dict);
                     printf("global_interface3_dat_dict fail.\n");
                     return 0;
                }
                printf("interface 6g dict3 load.\n");
                mtk_ap_buf->intf_6G.dict_table = global_interface3_dat_dict;
                init_intf_default_param(mtk_ap_buf, WIFI_6G);

                if ((!mtk_ap_buf->tb_profile_exist) && (mtk_ap_buf->dev_type == TESTBED)) {
                    if ((strcasecmp(data->program, "MBO") == 0) || (strcasecmp(data->program, "HE") == 0)) {
                        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "MBO"), "1");
                        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "RRM"), "1");
                    }
                    if (strcasecmp(data->program, "WPA3") == 0){
                        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "Transition_Disable"), "0");
                    }
                    dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "HT_DisallowTKIP"), "0");
                    dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "TestbedMode"), "1");
                    dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "PweMethod"), "2");
                }
                dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "OWN_IP_ADDR"), gIPaddr);
                dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(global_key_dict, "E2pAccessMode"), drv_value);
            }
            set_default_intf(mtk_ap_buf, WIFI_6G);
        } else {
            printf("No valid interface exist, skip!\n");
        }
    } else {
        return 0;
    }

    return 1;
}

static void  config_OPEN_OWE_MBSS(uint8_t *ap_buf, intf_desc_t* intf_p ) {
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;

    if (intf_p->status) {
        dict_t commit_table = intf_p->dict_table;
        char SSID_OWE[32], AuthMode[32];
        char owe_trans_mac[20], owe_trans_ssid_ascii[20], tmp[4];
        char intf0[6], intf1[6];
        int len, ssid_len, i;

        strcpy(AuthMode, dict_search(commit_table, dict_search_lower(global_key_dict, "AuthMode")));
        AuthMode[strlen(AuthMode) - 1] = 0;
        if( strcasecmp(AuthMode, "OPEN;OWE") == 0) {
            strcpy(intf0, intf_p->name);
            strcpy(SSID_OWE, dict_search(commit_table, "SSID2="));
            SSID_OWE[strlen(SSID_OWE) - 1] = 0;
            ssid_len = strlen(SSID_OWE);
            len = ssid_len + 6 + 5;

            strcpy(intf1, intf0);
            intf1[strlen(intf1)-1] = '1';
            read_mac_address_file(owe_trans_mac, intf1);
            strip_char(owe_trans_mac, ':');
            owe_trans_mac[strlen(owe_trans_mac) - 1] = 0;
            strcpy(owe_trans_ssid_ascii, "");
            for (i = 0; i < ssid_len; i++) {
                sprintf(tmp, "%2x", SSID_OWE[i]);
                strcat(owe_trans_ssid_ascii, tmp);
            }

            sprintf(gCmdStr, "iwpriv %s set vie_op=1-frm_map:5-oui:506f9a-length:%d-ctnt:1c%s%02x%s",intf0 , len, owe_trans_mac, ssid_len, owe_trans_ssid_ascii);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
            sprintf(gTmpChar1, "iwpriv %s set vie_op=3-frm_map:5-oui:506f9a-length:%d-ctnt:1c%s%02x%s",intf0 , len, owe_trans_mac, ssid_len, owe_trans_ssid_ascii);


            strcpy(SSID_OWE, dict_search(commit_table, "SSID1="));
            SSID_OWE[strlen(SSID_OWE) - 1] = 0;
            ssid_len = strlen(SSID_OWE);
            len = ssid_len + 6 + 5;

            read_mac_address_file(owe_trans_mac, intf0);
            strip_char(owe_trans_mac, ':');
            owe_trans_mac[strlen(owe_trans_mac) - 1] = 0;
            strcpy(owe_trans_ssid_ascii, "");
            for (i = 0; i < ssid_len; i++) {
                sprintf(tmp, "%2x", SSID_OWE[i]);
                strcat(owe_trans_ssid_ascii, tmp);
            }

            sprintf(gCmdStr, "iwpriv %s set vie_op=1-frm_map:5-oui:506f9a-length:%d-ctnt:1c%s%02x%s",intf1 , len, owe_trans_mac, ssid_len, owe_trans_ssid_ascii);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
            sprintf(gTmpChar2, "iwpriv %s set vie_op=3-frm_map:5-oui:506f9a-length:%d-ctnt:1c%s%02x%s",intf1 , len, owe_trans_mac, ssid_len, owe_trans_ssid_ascii);

            sprintf(gCmdStr, "iwpriv %s set HideSSID=0", intf0);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);

            sprintf(gCmdStr, "iwpriv %s set HideSSID=1", intf1);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
            mtk_ap_buf->intern_flag.vie_op = 1;
        }
    }
}


// CAPI Command implementation
int mtk_device_get_info(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_device_get_info function ===== \n");
    return WFA_SUCCESS;
}

int mtk_ap_ca_version(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_ap_ca_version function ===== \n");
    return WFA_SUCCESS;
}


int mtk_ap_config_commit(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_ap_config_commit function ===== \n");
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    char interface[8], tmp_str[10];
    int def_intf_stat =1;
    int Set_Reg_Domain = 0;
    int i;

    if (strcasecmp(mtk_ap_buf->Reg_Domain, "") != 0)
        Set_Reg_Domain = 1;

    if (mtk_ap_buf->intf_2G.status && mtk_ap_buf->intf_2G.security_set) {
        strcpy(gCmdStr, mtk_ap_buf->intf_2G.AuthModeBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_2G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_2G.AuthModeBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "AuthMode"), gCmdStr);
        strcpy(gCmdStr, mtk_ap_buf->intf_2G.EncryptBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_2G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_2G.EncryptBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "Encrypt") , gCmdStr);
    }
    if (mtk_ap_buf->intf_2G.status && Set_Reg_Domain) {
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "Reg_Domain") , mtk_ap_buf->Reg_Domain);
    }

    if (mtk_ap_buf->intf_5G.status && mtk_ap_buf->intf_5G.security_set) {
        strcpy(gCmdStr, mtk_ap_buf->intf_5G.AuthModeBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_5G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_5G.AuthModeBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "AuthMode"), gCmdStr);
        strcpy(gCmdStr, mtk_ap_buf->intf_5G.EncryptBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_5G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_5G.EncryptBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "Encrypt") , gCmdStr);
    }
    if (mtk_ap_buf->intf_5G.status && Set_Reg_Domain) {
        dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "Reg_Domain") , mtk_ap_buf->Reg_Domain);
    }

    if (mtk_ap_buf->intf_6G.status && mtk_ap_buf->intf_6G.security_set) {
        strcpy(gCmdStr, mtk_ap_buf->intf_6G.AuthModeBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_6G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_6G.AuthModeBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "AuthMode"), gCmdStr);
        strcpy(gCmdStr, mtk_ap_buf->intf_6G.EncryptBSSID[0]);
        for (i = 1; i < mtk_ap_buf->intf_6G.bss_num; i++) {
            strcat(gCmdStr, mtk_ap_buf->intf_6G.EncryptBSSID[i]);
        }
        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "Encrypt") , gCmdStr);
    }
    if (mtk_ap_buf->intf_6G.status && Set_Reg_Domain) {
        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "Reg_Domain") , mtk_ap_buf->Reg_Domain);
    }

    if (mtk_ap_buf->WappEnable) {
        strcpy(gCmdStr, "1");
        for (i=1; i<mtk_ap_buf->def_intf->bss_num; i++)
            strcat(gCmdStr, ";1");
        printf("gCmdStr for MBO profile keys: in str %s\n", gCmdStr);
        if (strcasecmp(gCmdStr, "1") != 0) {
            if (mtk_ap_buf->intf_2G.status) {
                dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "MBO"), gCmdStr);
                dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "RRM"), gCmdStr);
            }
            if (mtk_ap_buf->intf_5G.status) {
                dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "MBO"), gCmdStr);
                dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "RRM"), gCmdStr);
            }
            if (mtk_ap_buf->intf_6G.status) {
                dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "MBO"), gCmdStr);
                dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "RRM"), gCmdStr);
            }
        }
    }


    if (mtk_ap_buf->intf_2G.status) {
        printf("+++++setting 2G interface+++++\n");
        // write 2g.dat with interface dict. check current intf is 2g
        update_dat(mtk_ap_buf->intf_2G.dict_table, mtk_ap_buf->intf_2G.profile_names->profile);
        /* The following code is a WAR, because the current driver easily crash 
             when interface down at Radio off condition */
        sprintf(gCmdStr, "iwpriv %s set RadioOn=1", mtk_ap_buf->intf_2G.name);
        system(gCmdStr);
        DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
    }
    if (mtk_ap_buf->intf_5G.status) {
        printf("+++++setting 5G interface+++++\n");
        // write 5g.dat with interface dict. check current intf is 5g
        update_dat(mtk_ap_buf->intf_5G.dict_table, mtk_ap_buf->intf_5G.profile_names->profile);
        sprintf(gCmdStr, "iwpriv %s set RadioOn=1", mtk_ap_buf->intf_5G.name);
        system(gCmdStr);
        DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
    }
    if (mtk_ap_buf->intf_6G.status) {
        printf("+++++setting 6G interface+++++\n");
        // write 6g.dat with interface dict. check current intf is 6g
        update_dat(mtk_ap_buf->intf_6G.dict_table, mtk_ap_buf->intf_6G.profile_names->profile);
        sprintf(gCmdStr, "iwpriv %s set RadioOn=1", mtk_ap_buf->intf_6G.name);
        system(gCmdStr);
        DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
    }
    mtk_ap_buf->commit_dict = NULL;

    turn_all_interface_down_up(mtk_ap_buf);

    sprintf(gCmdStr, "killall wapp;rm -f /tmp/wapp* \n");
    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
    system(gCmdStr);

    strcpy(gCmdStr, "wapp -d1 -v2 ");
    if (mtk_ap_buf->intf_2G.status) {
        sprintf(tmp_str, "-c%s ", mtk_ap_buf->intf_2G.name);
        strcat(gCmdStr, tmp_str);
    }
    if (mtk_ap_buf->intf_5G.status) {
        sprintf(tmp_str, "-c%s ", mtk_ap_buf->intf_5G.name);
        strcat(gCmdStr, tmp_str);
    }
    if (mtk_ap_buf->intf_6G.status) {
        sprintf(tmp_str, "-c%s ", mtk_ap_buf->intf_6G.name);
        strcat(gCmdStr, tmp_str);
    }
    strcat(gCmdStr, "> /tmp/wapp.log \n");
    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
    system(gCmdStr);
    sleep(1);

    if(mtk_ap_buf->WappEnable) {
        if (mtk_ap_buf->intf_2G.status) {
            sprintf(gCmdStr, "wappctrl %s %s 1024 \n", mtk_ap_buf->intf_2G.name,
                table_search_lower(WappCmd, "Mpdu_Size"));
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        if (mtk_ap_buf->intf_5G.status) {
            sprintf(gCmdStr, "wappctrl %s %s 1024 \n", mtk_ap_buf->intf_5G.name,
                table_search_lower(WappCmd, "Mpdu_Size"));
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        if (mtk_ap_buf->intf_6G.status) {
            sprintf(gCmdStr, "wappctrl %s %s 1024 \n", mtk_ap_buf->intf_6G.name, 
                table_search_lower(WappCmd, "Mpdu_Size"));
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        sprintf(gCmdStr, "mbo_nr.sh 1 \n");
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    }

    //Issue the post commit iwpriv command
    for (i = 0; i < mtk_ap_buf->post_cmd_idx; i++) {
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", mtk_ap_buf->post_commit_cmd[i]);
            system(mtk_ap_buf->post_commit_cmd[i]);
            sleep(1);
    }
    mtk_ap_buf->post_cmd_idx = 0;

    /* Since single interface can be switched between 2G and 5G band between test case, 
      * init their structure no matter if the interface is up or not
      */
    global_interface1_dat_dict = init_2g_dict(mtk_ap_buf);
    mtk_ap_buf->intf_2G.dict_table = global_interface1_dat_dict;
    //init_intf_default_param(mtk_ap_buf, WIFI_2G);
    global_interface2_dat_dict = init_5g_dict(mtk_ap_buf);
    mtk_ap_buf->intf_5G.dict_table = global_interface2_dat_dict;
    //init_intf_default_param(mtk_ap_buf, WIFI_5G);

    if (mtk_ap_buf->intf_6G.status) {
        global_interface3_dat_dict = init_6g_dict(mtk_ap_buf);
        mtk_ap_buf->intf_6G.dict_table = global_interface3_dat_dict;
        //init_intf_default_param(mtk_ap_buf, WIFI_6G);

        if(mtk_ap_buf->dev_type == TESTBED && (!Force_UnsolicitedProbeResp)) {
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=unsolicit_probe_rsp-0\n", mtk_ap_buf->intf_6G.name);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
   }

    if (mtk_ap_buf->intf_2G.status) {
        config_OPEN_OWE_MBSS(mtk_ap_buf, &mtk_ap_buf->intf_2G);
    }

    if (mtk_ap_buf->intf_5G.status) {
        config_OPEN_OWE_MBSS(mtk_ap_buf, &mtk_ap_buf->intf_5G);
    }
    mtk_ap_buf->intern_flag.commited = 1;
    mtk_ap_buf->intern_flag.capi_dual_pf = 0;
    restore_profile(mtk_ap_buf);

    return WFA_SUCCESS;
}


int mtk_ap_deauth_sta(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    char ** value_ptr;
    char mac_address_buf[32];
    char intf[10];
    int i;

    value_ptr = data->values;
    strcpy(intf,mtk_ap_buf->def_intf->name);

    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "STA_MAC_Address") == 0) {
            sprintf(gCmdStr, "iwpriv %s set DisConnectSta=%s", intf, (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
    }

    return WFA_SUCCESS;
}


int mtk_ap_get_mac_address(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_ap_get_mac_address function ===== \n");
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    printf("(%s)\n", data->interface);
    char ** value_ptr;
    intf_desc_t* default_intf;
    char mac_address_buf[32];
    char intf[10];
    int i;
    int non_tx_bss_idx = 0;

    value_ptr = data->values;

    default_intf = mtk_ap_buf->def_intf;

    if (strcasecmp(data->interface, "2G") == 0 || strcasecmp(data->interface, "24G") == 0
            || strcasecmp(data->interface, "2.4") == 0) {
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_2G);
    } else if (strcasecmp(data->interface, "5G") == 0 || strcasecmp(data->interface, "5.0") == 0) {
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_5G);
    } else if (strcasecmp(data->interface, "6G") == 0 || strcasecmp(data->interface, "6.0") == 0) {
        mtk_ap_buf->def_intf = &(mtk_ap_buf->intf_6G);
    }
    strcpy(intf, mtk_ap_buf->def_intf->name);

    for (i = 0; i < data->count; i++) {
        int j;
        if (strcasecmp((data->params)[i], "WLAN_TAG") == 0){
            if (mtk_ap_buf->def_intf->mbss_en) {
                for (j=0; j<mtk_ap_buf->def_intf->WLAN_TAG_bss_num; j++) {
                    printf("WLAN_TAG[%d]=%d\n", j, mtk_ap_buf->def_intf->WLAN_TAG[j]);
                    if (mtk_ap_buf->def_intf->WLAN_TAG[j] == atoi((value_ptr)[i])) {
                        non_tx_bss_idx = j;
                        break;
                    }
                }
            }
        } else if (strcasecmp((data->params)[i], "NonTxBSSIndex") == 0){
            non_tx_bss_idx = atoi((value_ptr)[i]);
        }
    }

    if ((non_tx_bss_idx > 0) && (non_tx_bss_idx < 10)) {
        intf[strlen(intf)-1] = '0'+non_tx_bss_idx;
    }
    printf("non_tx_bss_idx is %d, inf is %s\n", non_tx_bss_idx, intf);
    read_mac_address_file(mac_address_buf, intf);

    sprintf((char *) resp_buf, "%s", mac_address_buf);
    printf("%s\n", resp_buf);
    return WFA_SUCCESS;
}

int mtk_ap_reset_default(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    retType_t status;
    char ** value_ptr;
    const char *drv_value;
    int i;

    printf("===== running mtk_ap_reset_default function ===== \n");
    printf("Reset the status and replace the WIFI profiles with Sigma default profiles \n");

    if (mtk_ap_buf->intern_flag.vie_op) {
        DPRINT_INFO(WFA_OUT, "Reset vie_op\n");
        DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gTmpChar1);
        system(gTmpChar1);
        DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gTmpChar2);
        system(gTmpChar2);
        mtk_ap_buf->intern_flag.vie_op= 0;
    }

    value_ptr = data->values;
    mtk_ap_buf->dev_type = UNKOWN;
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "type") == 0) {
            if (strcasecmp((value_ptr)[i], "DUT") == 0) {
                mtk_ap_buf->dev_type = DUT;
            } else if (strcasecmp((value_ptr)[i], "TestBed") == 0) {
                mtk_ap_buf->dev_type = TESTBED;
            }
        }
    }

    if(mtk_ap_buf->Band6Gonly.intf_6G_only) {
        mtk_ap_buf->Band6Gonly.intf_6G_only = 0;
        if(mtk_ap_buf->Band6Gonly.intf_2G_orig_stat) {
            ifconfig_interface_up(mtk_ap_buf->intf_2G.name);
            sleep(3);
            mtk_ap_buf->Band6Gonly.intf_2G_orig_stat = 0;
            mtk_ap_buf->intf_2G.status = 1;
        }
        if(mtk_ap_buf->Band6Gonly.intf_5G_orig_stat) {
            if (strcasecmp(mtk_ap_buf->intf_2G.name, mtk_ap_buf->intf_5G.name) != 0) {
                ifconfig_interface_up(mtk_ap_buf->intf_5G.name);
                sleep(3);
            }
            mtk_ap_buf->Band6Gonly.intf_5G_orig_stat = 0;
            mtk_ap_buf->intf_5G.status = 1;
        }
    }

    apply_sigma_profile(mtk_ap_buf, mtk_ap_buf->dev_type);

    if (mtk_ap_buf->intf_6G.status) {
        strcpy(mtk_ap_buf->intf_5G.name, mtk_ap_buf->intf_6G.name);
        mtk_ap_buf->intf_5G.status = 1;
        mtk_ap_buf->intf_2G.status = 1;
        if (!assign_profile_pointer_to_intf(mtk_ap_buf, mtk_ap_buf->intf_5G.name, &mtk_ap_buf->intf_5G.profile_names)) {
            printf("Profile name can't be found, use default!\n");
            return 0;
        }

        mtk_ap_buf->intf_6G.status = 0;
    }

    status = ap_init(mtk_ap_buf);

    if ((!mtk_ap_buf->tb_profile_exist) && (mtk_ap_buf->dev_type == TESTBED)) {
        if ((strcasecmp(data->program, "MBO") == 0) || (strcasecmp(data->program, "HE") == 0)) {
            mtk_ap_buf->WappEnable = 1;
            dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "MBO"), "1");
            dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "RRM"), "1");
            sprintf(gCmdStr, "wappctrl %s %s 0\n", mtk_ap_buf->intf_2G.name,
                table_search_lower(WappCmd, "Gas_CB_Delay"));
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(data->program, "WPA3") == 0){
            dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "Transition_Disable"), "0");
        }
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "HT_DisallowTKIP"), "0");
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "TestbedMode"), "1");
        dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "PweMethod"), "0");

        if (strcasecmp(mtk_ap_buf->intf_2G.profile_names->profile, mtk_ap_buf->intf_5G.profile_names->profile) != 0) {
            if ((strcasecmp(data->program, "MBO") == 0) || (strcasecmp(data->program, "HE") == 0)) {
                dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "MBO"), "1");
                dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "RRM"), "1");
                sprintf(gCmdStr, "wappctrl %s %s 0\n", mtk_ap_buf->intf_5G.name,
                    table_search_lower(WappCmd, "Gas_CB_Delay"));
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
            if (strcasecmp(data->program, "WPA3") == 0){
                dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "Transition_Disable"), "0");
            }
            dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "HT_DisallowTKIP"), "0");
            dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "TestbedMode"), "1");
            dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "PweMethod"), "0");
        }
    }

    Force_UnsolicitedProbeResp = 0;

    drv_value = table_search_lower(E2pAccessMode_tbl, mtk_ap_buf->cmd_cfg.mode);
    dict_update(mtk_ap_buf->intf_2G.dict_table, dict_search_lower(trans_table, "E2pAccessMode"), drv_value);
    dict_update(mtk_ap_buf->intf_5G.dict_table, dict_search_lower(trans_table, "E2pAccessMode"), drv_value);
    if (mtk_ap_buf->intf_6G.status) {
        dict_update(mtk_ap_buf->intf_6G.dict_table, dict_search_lower(trans_table, "E2pAccessMode"), drv_value);
    }
    mtk_ap_buf->intern_flag.commited = 0;
    mtk_ap_buf->intern_flag.BW_5G_set = 0;

    /* Flush arp */
    system("ip -s -s neigh flush all");

    return status;
}

int mtk_ap_send_addba_req(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_send_bcnrpt_req(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_send_bsstrans_mgmt_req(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_send_link_mea_req(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_send_tsmrpt_req(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_set_11d(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
     printf("===== running %s function ===== \n", __func__);

    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    dict_t commit_table = mtk_ap_buf->commit_dict;
    char ** value_ptr;
    char* CAPI_key;
    int i;

    value_ptr = data->values;

    printf("-----start looping: %d\n", data->count);
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "CountryCode") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "CountryCode"), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "Regulatory_Mode") == 0) {
            printf("Set %s to %s, do nothing!\n",(data->params)[i], (value_ptr)[i]);
        } else {
            CAPI_key = dict_search_lower(trans_table, (data->params)[i]);
            if (CAPI_key != 0) {
                printf("find something in table: %s\n", CAPI_key);
                dict_update(commit_table, CAPI_key, (value_ptr)[i]);
            } else {
                printf("!!!!!not in table!!!!!%s\n", data->params[i]);
            }
        }
    }
   return WFA_SUCCESS;
}

int mtk_ap_set_11h(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_set_11n_wireless(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_set_apqos(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_set_hs2(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

int mtk_ap_set_pmf(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);

    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    char ** value_ptr;
    char intf[16];
    int i;

    value_ptr = data->values;

    printf("-----start looping: %d\n", data->count);

    if ((strcasecmp(data->interface, "5G") == 0) || (strcasecmp(data->interface, "5.0") == 0)) {
        if (mtk_ap_buf->intf_5G.status) {
            strcpy(intf,mtk_ap_buf->intf_5G.name);
            printf("Set  to 5G band interface!\n");
        } else {
            strcpy(intf,mtk_ap_buf->def_intf->name);
            printf("5G interface is not supported, use default interface!\n");
        }
    } else if ((strcasecmp(data->interface, "2G") == 0) ||(strcasecmp(data->interface, "24G") == 0)
        ||(strcasecmp(data->interface, "2.4") == 0)) {
        if (mtk_ap_buf->intf_2G.status) {
            strcpy(intf,mtk_ap_buf->intf_2G.name);
            printf("Set  to 2G band interface!\n");
        } else {
            strcpy(intf,mtk_ap_buf->def_intf->name);
            printf("2G interface is not supported, use default interface!\n");
        }
    }
    
    for (i = 0; i < data->count; i++) {   
        if (strcasecmp((data->params)[i], "PMF") == 0) {
            if (strcasecmp((value_ptr)[i], "Required") == 0 ) {
                sprintf(gCmdStr, "iwpriv %s set PMFMFPC=1", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set PMFMFPR=1", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            } else if (strcasecmp((value_ptr)[i], "Optional") == 0 ) {
                sprintf(gCmdStr, "iwpriv %s set PMFMFPC=1", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set PMFMFPR=0", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            } else if (strcasecmp((value_ptr)[i], "Disabled") == 0 ) {
                sprintf(gCmdStr, "iwpriv %s set PMFMFPC=0", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set PMFMFPR=0", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
            char *essid = NULL;
            essid = wifi_interface_ssid(0, intf);
            sprintf(gCmdStr, "iwpriv %s set SSID=%s", intf, essid);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            if (essid) 
                system(gCmdStr);
            free(essid);
        }
    }
    return WFA_SUCCESS;
}

int mtk_ap_set_radius(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    //dict_t commit_table = mtk_ap_buf->commit_dict;
    dict_t commit_table_2G = mtk_ap_buf->intf_2G.dict_table;
    dict_t commit_table_5G = mtk_ap_buf->intf_5G.dict_table;
    dict_t commit_table_6G = mtk_ap_buf->intf_6G.dict_table;
    char ** value_ptr;
    char tmp[5];
    int i;

    value_ptr = data->values;

    printf("-----start looping: %d\n", data->count);
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "IPADDR") == 0) {
            printf("found! IPADDR\n");
            dict_update(commit_table_2G, dict_search_lower(trans_table, "RADIUS_Server"), value_ptr[i]);
            dict_update(commit_table_5G, dict_search_lower(trans_table, "RADIUS_Server"), value_ptr[i]);
            if (mtk_ap_buf->intf_6G.status) {
                dict_update(commit_table_6G, dict_search_lower(trans_table, "RADIUS_Server"), value_ptr[i]);
            }
        } else if (strcasecmp((data->params)[i], "PASSWORD") == 0) {
            printf("found! PASSWORD\n");
            dict_update(commit_table_2G, dict_search_lower(trans_table, "RADIUS_Key1"), value_ptr[i]);
            dict_update(commit_table_5G, dict_search_lower(trans_table, "RADIUS_Key1"), value_ptr[i]);
            if (mtk_ap_buf->intf_6G.status) {
                dict_update(commit_table_6G, dict_search_lower(trans_table, "RADIUS_Key1"), value_ptr[i]);
            }
        } else if (strcasecmp((data->params)[i], "PORT") == 0) {
            printf("found! PORT\n");
            dict_update(commit_table_2G, dict_search_lower(trans_table, "RADIUS_Port"), value_ptr[i]);
            dict_update(commit_table_5G, dict_search_lower(trans_table, "RADIUS_Port"), value_ptr[i]);
            if (mtk_ap_buf->intf_6G.status) {
                dict_update(commit_table_6G, dict_search_lower(trans_table, "RADIUS_Port"), value_ptr[i]);
            }
        }
    }
    sprintf(gCmdStr, "killall 8021xd");
    ADD_POST_CMD(gCmdStr);
    if (mtk_ap_buf->intf_2G.status) {
        strcpy(tmp, mtk_ap_buf->intf_2G.name);
        tmp[strlen(tmp)-1] = '\0';
        sprintf(gCmdStr, "8021xd -p %s -i %s -d 3", tmp, mtk_ap_buf->intf_2G.name);
        ADD_POST_CMD(gCmdStr);
    }
    if (mtk_ap_buf->intf_5G.status) {
        strcpy(tmp, mtk_ap_buf->intf_5G.name);
        tmp[strlen(tmp)-1] = '\0';
        sprintf(gCmdStr, "8021xd -p %s -i %s -d 3", tmp, mtk_ap_buf->intf_5G.name);
        ADD_POST_CMD(gCmdStr);
    }
    if (mtk_ap_buf->intf_6G.status) {
        strcpy(tmp, mtk_ap_buf->intf_6G.name);
        tmp[strlen(tmp)-1] = '\0';
        sprintf(gCmdStr, "8021xd -p %s -i %s -d 3", tmp, mtk_ap_buf->intf_6G.name);
        ADD_POST_CMD(gCmdStr);
    }

    return WFA_SUCCESS;
}


int mtk_ap_set_rfeature(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    dict_t commit_table = mtk_ap_buf->commit_dict;
    char ** value_ptr;
    int i;
    char tmp_str[4];
    const char *drv_value;
    char intf[10], Type[10];
    int muru_update = 0;
    int UL_MUMIMO_triggered = 0;
    char *token;
    int AID_STA_num = 0;
    char coding[20];
    int code_type = -1;
    char ack_policy[3] = "0";
    char FixedBw[3] = "2";
    char ht_bw[3] = "0";
    char vht_bw[3] = "0";
    int LTF=0;
    int GI=0;

    memset(Type, 0, sizeof(Type));
    strcpy(intf, mtk_ap_buf->def_intf->name);
    value_ptr = data->values;

    printf("-----start looping: %d\n", data->count);
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "CTS_Width") == 0) {
            printf("ap_set_rfeature %s Not implmented yet!\n", data->params[i]);
        } else if (strcasecmp((data->params)[i], "NDPA_STAinfo_MAC") == 0) {
            printf("ap_set_rfeature %s Not implmented yet!\n", data->params[i]);
        } else if (strcasecmp((data->params)[i], "Opt_Md_Notif_IE") == 0) {
            printf("ap_set_rfeature %s Not implmented yet!\n", data->params[i]);
        } else if (strcasecmp((data->params)[i], "RTS_Force") == 0) {
            printf("ap_set_rfeature %s Not implmented yet!\n", data->params[i]);
        } else if (strcasecmp((data->params)[i], "TxBandwidth") == 0) {
            drv_value= table_search_lower(TxBandwidth_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "TxBandwidth value is wrong %s, must be 80,40 or 20\n", (value_ptr)[i]);
                continue;
            }
            strcpy(FixedBw, drv_value);
            if (strcasecmp(Type, "HE") == 0) {
                if (strncmp(FixedBw, "0", 1) == 0) {
                    strcpy(ht_bw, "0");
                    strcpy(vht_bw, "0");
                }
                else if (strncmp(FixedBw, "1", 1) == 0) {
                    strcpy(ht_bw, "1");
                    strcpy(vht_bw, "0");
                }
                else if (strncmp(FixedBw, "2", 1) == 0) {
                    strcpy(ht_bw, "1");
                    strcpy(vht_bw, "1");
                }
                /* VhtBw first */
                sprintf(gCmdStr, "iwpriv %s set VhtBw=%s\n", intf, vht_bw);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                sleep(1);
                /* HtBw */
                sprintf(gCmdStr, "iwpriv %s set HtBw=%s\n", intf, ht_bw);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set SSID=%s\n", intf, mtk_ap_buf->def_intf->SSID);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
            else {
                sprintf(gCmdStr, "iwpriv %s set FixedBw=%s\n", intf, drv_value);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                printf("Not sure if ap_set_rfeature %s is implmented in driver correct or not!\n", data->params[i]);
            }
        } else if (strcasecmp((data->params)[i], "Type") == 0) {
            strcpy(Type, (value_ptr)[i]);
            
            continue;
        } else if (strcasecmp((data->params)[i], "GI") == 0) {
            drv_value= table_search_lower(GI_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "GI value is wrong %s, must be 0.8,1.6 or 3.2\n", (value_ptr)[i]);
                continue;
            }
            GI = 1;
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=he_gi-%s\n", intf, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "LTF") == 0) {
            drv_value= table_search_lower(LTF_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "LTF value is wrong %s, must be 3.2,6.4 or 12.8\n", (value_ptr)[i]);
                continue;
            }
            LTF = 1;
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=he_ltf-%s\n", intf, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "PPDUTxType") == 0) {
            drv_value= table_search_lower(PPDUTxType_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "PPDUTxType value is wrong %s, must be SU,MU,ER,TB or Legacy\n", (value_ptr)[i]);
                continue;
            }
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=ppdu_tx_type-%s\n", intf, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
       }   else if (strcasecmp((data->params)[i], "TriggerType") == 0) {
            if(strcasecmp((value_ptr)[i], "0") == 0) {
                if (mtk_ap_buf->def_intf->UL_MUMIMO) {
                    UL_MUMIMO_triggered = 1;
                    AID_STA_num = 2;
                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_user_cnt:%d\n", intf, AID_STA_num);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    if(mtk_ap_buf->def_intf->mode == WIFI_2G)
                        sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_bw:0\n", intf);
                    else
                        sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_bw:2\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    if(mtk_ap_buf->def_intf->mode == WIFI_2G)
                        sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_ru_alloc:0:61:0:61\n", intf);
                    else
                        sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_ru_alloc:0:67:0:67\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_mcs:7:7\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_cod:1:1\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_ssAlloc_raru:0:0\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_gi_ltf:2\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);

                    sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=update\n", intf);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);
                }else {
                    if(mtk_ap_buf->def_intf->DL != 1) {
                        sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=trig_type-%s\n", intf, (value_ptr)[i]);
                        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                        system(gCmdStr);
                    }
                }
            } else {
                sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=trig_type-%s\n", intf, (value_ptr)[i]);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
       }   else if (strcasecmp((data->params)[i], "Trigger_TxBF") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=trig_txbf-%s\n", intf, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "DisableTriggerType") == 0) {
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=disable_trig_type-%s\n", intf, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }  else if (strcasecmp((data->params)[i], "ACKType") == 0) {
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=ack_type-%s\n", intf, (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }  else if (strcasecmp((data->params)[i], "AckPolicy") == 0) {
            if ((data->params)[i+1] && (strcasecmp((data->params)[i+1], "AckPolicy_MAC") == 0)) {
                strcpy(ack_policy, (value_ptr)[i]);
            } else {
                sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=ack_policy-%s\n", intf, (value_ptr)[i]);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
                }
        }  else if (strcasecmp((data->params)[i], "AckPolicy_MAC") == 0) {
            sprintf(gCmdStr, "iwpriv %s set muru_dl_ack_policy_mac=%s-%s\n", intf, ack_policy, (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Assoc_Disallow") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "Assoc_Disallow"), drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "BTMReq_DisAssoc_Imnt") == 0) {
            mtk_ap_buf->DisAssoc_Imnt = atoi((value_ptr)[i]);
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "BTMReq_DisAssoc_Imnt"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "BTMReq_Term_Bit") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "BTMReq_Term_Bit"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "BSS_Term_Duration") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "BSS_Term_Duration"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "BSS_Term_TSF") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "BSS_Term_TSF"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Nebor_BSSID") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "Nebor_BSSID"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Nebor_Op_Ch") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "Nebor_Op_Ch"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Nebor_Op_Class") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "Nebor_Op_Class"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Nebor_Pref") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
                table_search_lower(WappCmd, "Nebor_Pref"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            
            sprintf(gCmdStr, "wappctrl %s %s\n", intf,
                table_search_lower(WappCmd, "Nebor_Test"));
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "NAV_Update") == 0) {
            if (strcasecmp((value_ptr)[i], "disable") == 0) 
                strcpy(tmp_str, "1");
            else
                strcpy(tmp_str, "0");
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=ignore_nav-%s\n", intf, tmp_str);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "RUAllocTones") == 0) {
            char *tone_plan_idx, *ru_alloc_idx;
            char tone_plan[20], ru_alloc[20];
            int STA_num = 0;
            int band_width = 0;
            int ru_alloc_base;

            memset(tone_plan, 0, sizeof(tone_plan));
            memset(ru_alloc, 0, sizeof(ru_alloc));

            tone_plan_idx= table_search_lower(TonePlan_Idx, (value_ptr)[i]);
            token = strtok((value_ptr)[i], ":");
            ru_alloc_idx= table_search_lower(RuAlloc_Idx, token);
            ru_alloc_base = atoi(ru_alloc_idx);
            while (token != NULL) {
                sprintf(ru_alloc + strlen(ru_alloc), ":0:%d", ru_alloc_base + STA_num);
                band_width += atoi(token);
                token = strtok(NULL, ":");
                STA_num++;
            }

            if (band_width > 996)
                strcpy(FixedBw, "3");

            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=dl_comm_user_cnt:%d\n", intf, STA_num);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=dl_comm_bw:%s\n", intf, FixedBw);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=dl_comm_toneplan:%s\n", intf, tone_plan_idx);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=dl_user_ru_alloc%s\n", intf, ru_alloc);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=update\n", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "AID") == 0) {
            AID_STA_num = 0;

            token = strtok((value_ptr)[i], " ");
            while (token != NULL) {
                token = strtok(NULL, " ");
                AID_STA_num++;
            }
            if (AID_STA_num > 4)
                AID_STA_num = 4;
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_user_cnt:%d\n", intf, AID_STA_num);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            muru_update = 1;
        }  else if (strcasecmp((data->params)[i], "Trig_ComInfo_BW") == 0) {
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_bw:%s\n", intf, (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            muru_update = 1;
        }  else if (strcasecmp((data->params)[i], "Trig_ComInfo_GI-LTF") == 0) {
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_comm_gi_ltf:%s\n", intf, (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            muru_update = 1;
        }  else if (strcasecmp((data->params)[i], "Trig_UsrInfo_RUAlloc") == 0) {
            char *ru_alloc_idx;
            char ru_alloc[20];
            int ru_alloc_base;
            int inc_num = 1;

            memset(ru_alloc, 0, sizeof(ru_alloc));

            token = strtok((value_ptr)[i], ":");
            if ((strcasecmp(token, "52") == 0) || (strcasecmp(token, "106") == 0))
                inc_num = 2;
            ru_alloc_idx= table_search_lower(RuAlloc_Idx, token);
            ru_alloc_base = atoi(ru_alloc_idx);
            while (token != NULL) {
                sprintf(ru_alloc + strlen(ru_alloc), ":0:%d", ru_alloc_base);
                token = strtok(NULL, ":");
                ru_alloc_base += inc_num;
            }
            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_ru_alloc%s\n", intf, ru_alloc);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            muru_update = 1;
        }  else if (strcasecmp((data->params)[i], "Trig_UsrInfo_SSAlloc_RA-RU") == 0) {
            char RA_RU[20];
            strcpy(RA_RU, (value_ptr)[i]);
            replace_char(RA_RU, ' ', ':');

            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_ssAlloc_raru:%s\n", intf, RA_RU);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            muru_update = 1;
        } else if (strcasecmp((data->params)[i], "UnsolicitedProbeResp") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=unsolicit_probe_rsp-%s\n", mtk_ap_buf->intf_6G.name , drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "FILSDscv") == 0) {
            if (strcasecmp((value_ptr)[i], "disable") == 0 )
                sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=unsolicit_probe_rsp-1\n", mtk_ap_buf->intf_6G.name);
            else if (strcasecmp((value_ptr)[i], "enable") == 0 )
                sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=unsolicit_probe_rsp-0\n", mtk_ap_buf->intf_6G.name);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "Cadence_UnsolicitedProbeResp") == 0) {
            sprintf(gCmdStr, "iwpriv %s set 6giob=1-%s-2\n", mtk_ap_buf->intf_6G.name , (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "nss_mcs_opt") == 0) {
            char nss[8], mcs[8];
            token = strtok((value_ptr)[i], ";");
            strcpy(nss, token);
            token = strtok(NULL, ";");
            strcpy(mcs, token);
            DPRINT_INFO(WFA_OUT, "nss=%s, mcs=%s\n", nss, mcs);

            if (strcasecmp(Type, "WPA3") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=5:%s", intf, nss);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            } else {
                if (strcasecmp(nss, "def") != 0) {
                    int cmd_len = 10;
                    int resp_len_ptr;
                    uint8_t sock_resp_buf[10];
                    
                    memset(sock_resp_buf, 0, 10);
                    set_default_intf(mtk_ap_buf, mtk_ap_buf->def_mode);
                    commit_table = mtk_ap_buf->commit_dict;
                    dict_update(commit_table, dict_search_lower(trans_table, "SPATIAL_TX_STREAM"), nss);
                    dict_update(commit_table, dict_search_lower(trans_table, "SPATIAL_RX_STREAM"), nss);
                    mtk_ap_config_commit(cmd_len, (uint8_t * ) mtk_ap_buf, &resp_len_ptr, (uint8_t *) sock_resp_buf);

                    //sprintf(gCmdStr, "iwpriv %s set FixedVhtNss=%s\n", mtk_ap_buf->def_intf->name, token);
                    //DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    //system(gCmdStr);
                }
                if (mcs && (strcasecmp(token, "def") != 0)) {
                    sprintf(gCmdStr, "iwpriv %s set FixedMcs=%s\n", intf, mcs);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                    system(gCmdStr);
                }
            }
        }  else if (strcasecmp((data->params)[i], "TriggerCoding") == 0) {

            if (strcasecmp((value_ptr)[i], "BCC") == 0) {
                code_type = 0;
            } else if (strcasecmp((value_ptr)[i], "LDPC") == 0){
               code_type = 1;
            }
        } else if (strcasecmp((data->params)[i], "Channel_Switch_Announcement") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=6", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "ReassocResp_RSNXE_ProtectedTWT") == 0) {
            if (strcasecmp((value_ptr)[i], "0") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=0", intf);
            } else if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=9", intf);
            }
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "ReassocResp_RSNXE_Used") == 0) {
            if (strcasecmp((value_ptr)[i], "0") == 0) {
            } else if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=8", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "Transition_Disable") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                if (strcasecmp((data->params)[i+1], "Transition_Disable_Index") == 0) {
                    char *index= (value_ptr)[i+1];
                    char idx_num[2];
                    int j, bitmask = 0;

                    memset(idx_num, 0, sizeof(idx_num));
                    for (j = 0; j < strlen(index); j++) {
                        idx_num[0] = index[j];
                        bitmask |= 1 << atoi(idx_num);
                    }

                    sprintf(gCmdStr, "iwpriv %s set transition_disable=1:%d", intf, bitmask);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                    system(gCmdStr);
                    i++;
                }
            } else if (strcasecmp((value_ptr)[i], "0") == 0) {
                sprintf(gCmdStr, "iwpriv %s set transition_disable=0", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "OCIFrameType") == 0) {
            if (strcasecmp((data->params)[i+1], "OCIChannel") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=12:%s", intf, (value_ptr)[i+1]);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
                i++;
            }
        } else if (strcasecmp((data->params)[i], "MMIC_IE_InvalidMIC") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=6", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "MMIC_IE_BIPNResue") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=7", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "OMN_IE") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:0", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "CSA_IE") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:1", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "MMIC_IE") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:2", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "HT_Opt_IE") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:3", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "ChNum_Band") == 0) {
            char Channel[8], Band[8], delim[3];
            //DPRINT_INFO(WFA_OUT, "nss=%s, mcs=%s\n", Channel, Band);

            if (strstr((value_ptr)[i], ":") ) {
                strcpy(delim, ":");
            } else if (strstr((value_ptr)[i], ";") ) {
                strcpy(delim, ";");
            } else {
                DPRINT_INFO(WFA_OUT, "No delimiter found, find out reason!!!\n");
                continue;
            }
            
            token = strtok((value_ptr)[i], delim);
            strcpy(Channel, token);
            token = strtok(NULL, delim);
            strcpy(Band, token);

            if (strcasecmp(Type, "WPA3") == 0) {
                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=1:%s", intf, Channel);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);

                sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=2:%s", intf, Band);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
	    } else if (strcasecmp(Type, "VHT") == 0) {
                sprintf(gCmdStr, "iwpriv %s set Channel=%s", intf, Channel);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);

                sleep(3);

                drv_value= table_search_lower(width_HTBW_tbl, Band);
                if (!drv_value) {
                    DPRINT_INFO(WFA_OUT, "Band %s is wrong, must be 160,80,40 or 20\n", Band);
                    continue;
                }
                sprintf(gCmdStr, "iwpriv %s set HtBw=%s", intf, drv_value);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);

                drv_value= table_search_lower(width_VHTBW_tbl, Band);
                if (!drv_value) {
                    DPRINT_INFO(WFA_OUT, "Band %s is wrong, must be 160,80,40 or 20\n", Band);
                    continue;
                }
                sprintf(gCmdStr, "iwpriv %s set VhtBw=%s", intf, drv_value);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            } else {
                printf("ap_set_rfeature %s Not implmented yet!\n", data->params[i]);
            }
        } else if (strcasecmp((data->params)[i], "channelswitchcount") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=3:%s", intf, value_ptr[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "HT_Opt_IE_ChanWidth") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=4:%s", intf, value_ptr[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "HT_Opt_IE_NSS") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=6:%s", intf, value_ptr[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        } else if (strcasecmp((data->params)[i], "TxPower") == 0) {
            if (strcasecmp((value_ptr)[i], "low") == 0) {
                sprintf(gCmdStr, "iwpriv %s set PercentageCtrl=1", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set DecreasePower=20", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                system(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "RTS_FORCE") == 0) {
                printf("RTS_FORCE value %s!\n", value_ptr[i]);
                if (strcasecmp((value_ptr)[i], "enable") == 0) {
                    dict_update(commit_table, dict_search_lower(trans_table, "RTSThreshold"), "1");
                }
                else {
                    dict_update(commit_table, dict_search_lower(trans_table, "RTSThreshold"), "0");
                }
            }
        else {
            printf("ap_set_rfeature %s  Command is ignored or invalid!\n", data->params[i]);
        }
    }

    if (muru_update) {
        int j;

        if ((code_type == 0) || (code_type == 1)) {
            strcpy(coding, "0:0:0:0");
            if (code_type == 0) {
                strcpy(coding, "0");
                for(j = 0; j < AID_STA_num-1; j++)
                    strcat(coding, ":0");
            } else if (code_type == 1) {
               strcpy(coding, "1");
                for(j = 0; j < AID_STA_num-1; j++)
                    strcat(coding, ":1");
            }

            sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=ul_user_cod:%s\n", intf, coding);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        code_type = -1;

        sprintf(gCmdStr, "iwpriv %s set set_muru_manual_config=update\n", intf);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
        muru_update = 0;
    }
    
    if (LTF && GI) {
        sprintf(gCmdStr, "iwpriv %s set FixedBw=1-2\n", intf);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
        sprintf(gCmdStr, "iwpriv %s set FixedMcs=1-7\n", intf);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    }

     if(UL_MUMIMO_triggered) {
         sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=trig_type-0\n", intf);
         DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
         system(gCmdStr);
         UL_MUMIMO_triggered = 0;
     }

    return WFA_SUCCESS;
}


int mtk_ap_set_rrm(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}

#define SET_SAE_PKCFG(value) {\
            char SAE_PKCfg[16], *p;\
            int SAE_PKCfg_i;\
            if (strcasecmp((value_ptr)[i], "1") == 0) {\
                p = dict_search(commit_table, dict_search_lower(trans_table, "SAE_PKCfg"));\
                if (p) {\
                    SAE_PKCfg_i = (int)strtol(p, NULL, 16);\
                } else {\
                    SAE_PKCfg_i = 0;\
                }\
                SAE_PKCfg_i |= value;\
                sprintf(SAE_PKCfg, "%x", SAE_PKCfg_i);\
                dict_update(commit_table, dict_search_lower(trans_table, "SAE_PKCfg"), SAE_PKCfg);\
            }\
        }
void update_capi_dual_table(uint8_t *ap_buf, char *key_ptr, char *value_ptr)
{
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    dict_t commit_table = mtk_ap_buf->commit_dict;
    dict_t commit_table2 = NULL;
    intf_desc_t* def_intf = mtk_ap_buf->def_intf;
    intf_desc_t* def_intf2 = NULL;
    const char *drv_value;
    char key_str[128] = {0};
    char value[32], param1[16], param2[16],*token = NULL;

    strcpy(value,value_ptr);
    if (NULL != (strstr(value, ";"))) {
        token = strtok(value, ";");
        strcpy(param1, token);
        token = strtok(NULL, ";");
        strcpy(param2, token);
    } else {
        strcpy(param1, value);
        strcpy(param2, value);
    }

    if (mtk_ap_buf->def_mode == WIFI_5G) {
        commit_table2 = mtk_ap_buf->intf_2G.dict_table;
        def_intf2 = &mtk_ap_buf->intf_2G;
    }
    else if (mtk_ap_buf->def_mode == WIFI_2G) {
        commit_table2 = mtk_ap_buf->intf_5G.dict_table;
        def_intf2 = &mtk_ap_buf->intf_5G;
    }

    if (strcasecmp(key_ptr, "SSID") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "SSID"), param1);
        dict_update(commit_table2, dict_search_lower(trans_table, "SSID"), param2);
        strcpy(def_intf->SSID, param1);
        strcpy(def_intf2->SSID, param2);
    }
    else if (strcasecmp(key_ptr, "CHANNEL") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "Channel"), param1);
        dict_update(commit_table2, dict_search_lower(trans_table, "Channel"), param2);
    }
    else if (strcasecmp(key_ptr, "Mode") == 0) {
        drv_value= table_search_lower(mode_HTOp_tbl, param1);
        dict_update(commit_table, dict_search_lower(trans_table, "HT_OpMode"), drv_value);
        drv_value= table_search_lower(mode_HTOp_tbl, param2);
        dict_update(commit_table2, dict_search_lower(trans_table, "HT_OpMode"), drv_value);
        drv_value= table_search_lower(mode_tbl, param1);
        dict_update(commit_table, dict_search_lower(trans_table, "Mode"), drv_value);
        drv_value= table_search_lower(mode_tbl, param2);
        dict_update(commit_table2, dict_search_lower(trans_table, "Mode"), drv_value);
    }
    else if (strcasecmp(key_ptr, "BCNINT") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "BcnInt"), param1);
        dict_update(commit_table2, dict_search_lower(trans_table, "BcnInt"), param2);
    }
    else if (strcasecmp(key_ptr, "PSK") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "PSK"), param1);
        dict_update(commit_table, dict_search_lower(trans_table, "PWDIDR"), mtk_ap_buf->def_intf->PWDIDR);
        dict_update(commit_table2, dict_search_lower(trans_table, "PSK"), param2);
        dict_update(commit_table2, dict_search_lower(trans_table, "PWDIDR"), mtk_ap_buf->def_intf->PWDIDR);
        mtk_ap_buf->intf_2G.security_set = 1;
        mtk_ap_buf->intf_5G.security_set = 1;
    }
    else if (strcasecmp(key_ptr, "WPA2-ENT") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA2");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "AES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
        dict_update(commit_table2, dict_search_lower(trans_table, "AuthMode"), "WPA2");
        dict_update(commit_table2, dict_search_lower(trans_table, "Encrypt") , "AES");
        dict_update(commit_table2, dict_search_lower(trans_table, "IEEE1x"), "0");
    }
    else if (strcasecmp(key_ptr, "WPA2-PSK") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
        dict_update(commit_table2, dict_search_lower(trans_table, "IEEE1x"), "0");
        strcpy(mtk_ap_buf->intf_2G.AuthModeBSSID[0], "WPA2PSK");
        strcpy(mtk_ap_buf->intf_5G.AuthModeBSSID[0], "WPA2PSK");
        strcpy(mtk_ap_buf->intf_2G.EncryptBSSID[0], "AES");
        strcpy(mtk_ap_buf->intf_5G.EncryptBSSID[0], "AES");
    }
}



int mtk_ap_set_security(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);

    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    dict_t commit_table = mtk_ap_buf->commit_dict;
    char ** value_ptr;
    char* CAPI_key;
    char keymgnt[42], pairwise_cipher[42], group_cipher[42];
    const char *drv_value;
    int i;
    int SAE_Commit_StatusCode = 0;
    int SAE_PK_Omit = 0;
    int intf_set = 1;
    int WLAN_TAG_bss_idx = 0;

    strcpy(keymgnt, "");
    strcpy(pairwise_cipher, "");
    strcpy(group_cipher, "");
    value_ptr = data->values;

    printf("-----start looping: %d\n", data->count);

    intf_set =  CAPI_set_intf(mtk_ap_buf);
    commit_table = mtk_ap_buf->commit_dict;

    for (i = 0; i < data->count; i++) {
         if (strcasecmp((data->params)[i], "WLAN_TAG") == 0) {
            int j;

            if(intf_set == 0) {
                mtk_ap_buf->WLAN_TAG = atoi((value_ptr)[i]) - 1;
                if (mtk_ap_buf->WLAN_TAG < 0)
                    mtk_ap_buf->WLAN_TAG = 0;
                if(mtk_ap_buf->WLAN_TAG_inf[mtk_ap_buf->WLAN_TAG]) { 
                    set_default_intf(mtk_ap_buf, mtk_ap_buf->WLAN_TAG_inf[mtk_ap_buf->WLAN_TAG]->mode);
                    commit_table = mtk_ap_buf->commit_dict;
                }
            }
            printf("Default interface=%s\n", mtk_ap_buf->def_intf->name);

            WLAN_TAG_bss_idx = 0;
            if (mtk_ap_buf->def_intf->mbss_en) {

                for (j=0; j<mtk_ap_buf->def_intf->WLAN_TAG_bss_num; j++) {
                    printf("WLAN_TAG[%d]=%d\n", j, mtk_ap_buf->def_intf->WLAN_TAG[j]);
                    if (mtk_ap_buf->def_intf->WLAN_TAG[j] == atoi((value_ptr)[i])) {
                        WLAN_TAG_bss_idx = j + 1;
                        break;
                    }
                }
            }
            //printf("bss_idx %d, WLAN_TAG_bss_num %d\n", WLAN_TAG_bss_idx, mtk_ap_buf->def_intf->WLAN_TAG_bss_num);
            continue;
        } else if (strcasecmp((data->params)[i], "keymgnt") == 0) {
            strcpy(keymgnt, (value_ptr)[i]);
            printf("%s\n", keymgnt);
            /*if ((strcasecmp(keymgnt, "WPA2-PSK-Mixed") == 0) && (strcasecmp(program, "HE") == 0)) {
                printf("!!!!!HE doesn't support WPA2-PSK-Mixed !!!!\n");
                return WFA_ERROR;
            }*/
        } else if (strcasecmp((data->params)[i], "AKMSuiteType") == 0) {
            drv_value = table_search_lower(AKM_keymgnt_tbl, (value_ptr)[i]);
            strcpy(keymgnt, drv_value);

            if (strcasecmp(keymgnt, "") == 0) {
                printf("AKMSuiteType:%s is not valid\n", (value_ptr)[i]);
                continue;
            } else {
                printf("AKMSuiteType:%s, keymgnt: %s\n", (value_ptr)[i], keymgnt);
            }

            if ((strcasecmp((value_ptr)[i], "3") == 0) ||(strcasecmp((value_ptr)[i], "4") == 0) ||
                (strcasecmp((value_ptr)[i], "9") == 0) || (strcasecmp((value_ptr)[i], "1;3") == 0) ||
                (strcasecmp((value_ptr)[i], "3;5") == 0) || (strcasecmp((value_ptr)[i], "1;3;5") == 0) ||
                (strcasecmp((value_ptr)[i], "8;9") == 0) ||
                (strcasecmp((value_ptr)[i], "2;4;8;9") == 0) || (strcasecmp((value_ptr)[i], "2;4;6;8;9") == 0)) {
                dict_update(commit_table, dict_search_lower(trans_table, "FT_OA"), "1");
            } 
            if ((strcasecmp((value_ptr)[i], "3") == 0) ||(strcasecmp((value_ptr)[i], "4") == 0) ||
                (strcasecmp((value_ptr)[i], "9") == 0) ) {
                dict_update(commit_table, dict_search_lower(trans_table, "FT_ONLY"), "1");
            }
            if ((strcasecmp((value_ptr)[i], "5") == 0) ||(strcasecmp((value_ptr)[i], "6") == 0) ||
                (strcasecmp((value_ptr)[i], "3;5") == 0)) {
                dict_update(commit_table, dict_search_lower(trans_table, "PMF_MFPC"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "PMF_SHA256"), "1");
            }
        } else if (strcasecmp((data->params)[i], "PMF") == 0) {
            if((mtk_ap_buf->def_intf->bss_idx> 1) && (mtk_ap_buf->def_intf->bss_idx < 6)){
                if (strcasecmp((value_ptr)[i], "Required") == 0 ) {
                    strcat(mtk_ap_buf->def_intf->PMF_MFPC, ";1");
                    strcat(mtk_ap_buf->def_intf->PMF_MFPR, ";1");
                } else if (strcasecmp((value_ptr)[i], "Optional") == 0) {
                    strcat(mtk_ap_buf->def_intf->PMF_MFPC, ";1");
                    strcat(mtk_ap_buf->def_intf->PMF_MFPR, ";0");
                } else if (strcasecmp((value_ptr)[i], "Disabled") == 0) {
                    strcat(mtk_ap_buf->def_intf->PMF_MFPC, ";0");
                    strcat(mtk_ap_buf->def_intf->PMF_MFPR, ";0");
                }
            } else if (mtk_ap_buf->def_intf->bss_idx <= 1){
                if (strcasecmp((value_ptr)[i], "Required") == 0 ) {
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPC, "1");
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPR, "1");
                } else if (strcasecmp((value_ptr)[i], "Optional") == 0) {
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPC, "1");
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPR, "0");
                } else if (strcasecmp((value_ptr)[i], "Disabled") == 0) {
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPC, "0");
                    strcpy(mtk_ap_buf->def_intf->PMF_MFPR, "0");
                }
            }
            dict_update(commit_table, dict_search_lower(trans_table, "PMF_MFPC"), mtk_ap_buf->def_intf->PMF_MFPC);
            dict_update(commit_table, dict_search_lower(trans_table, "PMF_MFPR"), mtk_ap_buf->def_intf->PMF_MFPR);
        } else if (strcasecmp((data->params)[i], "BeaconProtection") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "SHA256AD") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "PMF_SHA256"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "NonTxBSSIndex") == 0) {
            mtk_ap_buf->def_intf->bss_idx = atoi((value_ptr)[i]) + 1;
        } else if ((strcasecmp((data->params)[i], "PSK") == 0) ||
            (strcasecmp((data->params)[i], "PSKPassPhrase") == 0)) {
            if((mtk_ap_buf->def_intf->bss_idx > 1) && (mtk_ap_buf->def_intf->bss_idx < 6)){
                sprintf(gCmdStr,  "PSK%d", (mtk_ap_buf->def_intf->bss_idx));
                dict_update(commit_table, dict_search_lower(trans_table, gCmdStr), (value_ptr)[i]);
                strcat(mtk_ap_buf->def_intf->PWDIDR, ";0");
            } else if (mtk_ap_buf->def_intf->bss_idx <= 1) {
                if (mtk_ap_buf->intern_flag.capi_dual_pf)
                    update_capi_dual_table(ap_buf, "PSK", (value_ptr)[i]);
                else {
                    dict_update(commit_table, dict_search_lower(trans_table, "PSK"), (value_ptr)[i]);
                }
                strcpy(mtk_ap_buf->def_intf->PWDIDR, "0");
            }
            dict_update(commit_table, dict_search_lower(trans_table, "PWDIDR"), mtk_ap_buf->def_intf->PWDIDR);
        } else if (strcasecmp((data->params)[i], "WEPKey") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "WEPKey"), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "ENCRYPT") == 0) {
            /*if ((strcasecmp((value_ptr)[i], "WEP") == 0) && (strcasecmp(program, "HE") == 0)) {
                printf("!!!!!HE doesn't support WEP !!!!!\n");
                return WFA_ERROR;
            }*/
            dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "PairwiseCipher") == 0) {
            strcpy(pairwise_cipher, (value_ptr)[i]);
            printf("PairwiseCipher:%s:%d\n", pairwise_cipher, strlen(pairwise_cipher));
        } else if (strcasecmp((data->params)[i], "GroupCipher") == 0) {
            strcpy(group_cipher, (value_ptr)[i]);
            printf("GroupCipher:%s:%d\n", group_cipher, strlen(group_cipher));
        } else if (strcasecmp((data->params)[i], "RSNXE_Content") == 0) {
            sprintf(gCmdStr, "iwpriv %s set wpa3_test=1", mtk_ap_buf->def_intf->name);
            ADD_POST_CMD(gCmdStr);
        } else if (strcasecmp((data->params)[i], "AntiCloggingThreshold") == 0) {
            sprintf(gCmdStr, "iwpriv %s set sae_anti_clogging_th=%s", mtk_ap_buf->def_intf->name, (value_ptr)[i]);
            ADD_POST_CMD(gCmdStr);
        } else if (strcasecmp((data->params)[i], "reflection") == 0) {
            if (strcasecmp((value_ptr)[i], "SAE") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=3", mtk_ap_buf->def_intf->name);
                ADD_POST_CMD(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "InvalidSAEElement") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=4", mtk_ap_buf->def_intf->name);
                ADD_POST_CMD(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set sae_commit_msg=%s", mtk_ap_buf->def_intf->name, (value_ptr)[i]);
                ADD_POST_CMD(gCmdStr);
        } else if (strcasecmp((data->params)[i], "SAE_Confirm_Immediate") == 0) {
            if (strcasecmp((value_ptr)[i], "0") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=0", mtk_ap_buf->def_intf->name);
                ADD_POST_CMD(gCmdStr);
            } else if (strcasecmp((value_ptr)[i], "1") == 0) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=2", mtk_ap_buf->def_intf->name);
                ADD_POST_CMD(gCmdStr);
            }
        } else if (strcasecmp((data->params)[i], "ECGroupID") == 0) {
            if (strstr((value_ptr)[i], " ") == NULL) {
                sprintf(gCmdStr, "iwpriv %s set wpa3_test=5", mtk_ap_buf->def_intf->name);
                ADD_POST_CMD(gCmdStr);
                sprintf(gCmdStr, "iwpriv %s set sae_fixed_group_id=%s", mtk_ap_buf->def_intf->name, (value_ptr)[i]);
                ADD_POST_CMD(gCmdStr);
            } else {
                printf("ECGroupID %s, do nothing!\n", (value_ptr)[i]);
            }
        } else if (strcasecmp((data->params)[i], "Clear_RSNXE") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "PweMethod"), (value_ptr)[i]);
            sprintf(gCmdStr, "iwpriv %s set clear_rsnxe=%s", mtk_ap_buf->def_intf->name, (value_ptr)[i]);
            ADD_POST_CMD(gCmdStr);
        } else if (strcasecmp((data->params)[i], "KeyRotation") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), "TIME");
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), "DISABLE");
            }
        } else if (strcasecmp((data->params)[i], "KeyRotation_BIGTK_STADisassoc") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "KeyRotation"), "DISCONNECT");
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "KeyRotation"), "DISABLE");
            }
        } else if (strcasecmp((data->params)[i], "KeyRotationInterval") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "SAEPasswords") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "PWDIDR"), "1");
            dict_update(commit_table, dict_search_lower(trans_table, "PWDID"), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "Transition_Disable") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                if (strcasecmp((data->params)[i+1], "Transition_Disable_Index") == 0) {
                    char *index= (value_ptr)[i+1];
                    char idx_num[2], idx_char[5];
                    int j, bitmask = 0;

                    memset(idx_num, 0, sizeof(idx_num));
                    for (j = 0; j < strlen(index); j++) {
                        idx_num[0] = index[j];
                        bitmask |= 1 << atoi(idx_num);
                    }
                    sprintf(idx_char, "%d", bitmask);
                    dict_update(commit_table, dict_search_lower(trans_table, "Transition_Disable"), idx_char);
                    i++;
                }
            } else if (strcasecmp((value_ptr)[i], "0") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "Transition_Disable"), "0");
            }
        } else if (strcasecmp((data->params)[i], "PMKSACaching") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "RSNXE_ProtectedTWT") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "SAE_H2E") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "SAE_PWE") == 0) {
            drv_value= table_search_lower(PweMethod_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "PweMethod"), drv_value);
        } else if (strcasecmp((data->params)[i], "SAE_PK") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), (value_ptr)[i]);
        } else if ((strcasecmp((data->params)[i], "SAE_PK_KeyPair") == 0) ||
            (strcasecmp((data->params)[i], "SAE_PK_KeyPairMism") == 0)) {
            drv_value= long_table_search_lower(SAE_PK_KeyPair_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "SAE_PK_KeyPair"), drv_value);
            drv_value= table_search_lower(SAE_PK_Group_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "SAE_PKGroup"), drv_value);
            if (strcasecmp((data->params)[i], "SAE_PK_KeyPairMism") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "SAE_PKCfg"), "40");
            }
        } else if ((strcasecmp((data->params)[i], "SAE_PK_Modifier") == 0) ||
            (strcasecmp((data->params)[i], "SAE_PK_ModifierMism") == 0)) {
            dict_update(commit_table, dict_search_lower(trans_table, "SAE_PK_Modifier"), (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "SAE_PK_KeyPairSigOverride") == 0) {
            drv_value= table_search_lower(SAE_PK_KeyPair_tbl, (value_ptr)[i]);
            sprintf(gCmdStr, "iwpriv %s set sae_pk_private_key_overwrite=%s", mtk_ap_buf->def_intf->name, drv_value);
            ADD_POST_CMD(gCmdStr);
            sprintf(gCmdStr, "iwpriv %s set sae_pk_test=20", mtk_ap_buf->def_intf->name);
            ADD_POST_CMD(gCmdStr);
        } else if (strcasecmp((data->params)[i], "SAE_Commit_StatusCode") == 0) {
            if (strcasecmp((value_ptr)[i], "126") == 0) {
                //SAE_Commit_StatusCode = 1;
                SAE_PK_Omit |= 0x8;
            }
        } else if (strcasecmp((data->params)[i], "SAE_PK_Omit") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                SAE_PK_Omit |= 0x10;
            }
        } else if (strcasecmp((data->params)[i], "FILS_PublicKey_Omit") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                SAE_PK_Omit |= 0x80;
            }
            //SET_SAE_PKCFG(0x80);
        } else if (strcasecmp((data->params)[i], "FILS_KeyConfirm_Omit") == 0) {
            if (strcasecmp((value_ptr)[i], "1") == 0) {
                SAE_PK_Omit |= 0x100;
            }
            //SET_SAE_PKCFG(0x100);
        } else {
            CAPI_key = dict_search_lower(trans_table, (data->params)[i]);
            if (CAPI_key != 0) {
                printf("find something in table: %s\n", CAPI_key);
                dict_update(commit_table, CAPI_key, (value_ptr)[i]);
            } else {
                printf("!!!!!not in table!!!!!%s\n", data->params[i]);
            }
        }
    }

    if(SAE_PK_Omit) {
        sprintf(gCmdStr, "iwpriv %s set sae_pk_test=%x", mtk_ap_buf->def_intf->name, SAE_PK_Omit);
        ADD_POST_CMD(gCmdStr);
    }
#if 0    
    if (SAE_Commit_StatusCode && SAE_PK_Omit) {
        sprintf(gCmdStr, "iwpriv %s set sae_pk_test=18", mtk_ap_buf->def_intf->name);
        ADD_POST_CMD(gCmdStr);
    } else {
        if (SAE_Commit_StatusCode) {
            sprintf(gCmdStr, "iwpriv %s set sae_pk_test=8", mtk_ap_buf->def_intf->name);
            ADD_POST_CMD(gCmdStr);
        }
        if (SAE_PK_Omit) {
            sprintf(gCmdStr, "iwpriv %s set sae_pk_test=10", mtk_ap_buf->def_intf->name);
            ADD_POST_CMD(gCmdStr);
        }
    }
#endif

    printf("start implementation\n");
    if (strcasecmp(keymgnt, "None") == 0) {
        printf("set none attr!!\n");
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "OPEN");
        printf("set AuthMode pass!!\n");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "NONE");
        printf("set Encrypt pass!!\n");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
        printf("set IEEE1x pass!!\n");
    } else if (strcasecmp(keymgnt, "WPA-PSK") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPAPSK");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "TKIP");
        //dict_update(commit_table, dict_search_lower(trans_table, "PSK"), dict_search(commit_table, "psk"));
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-PSK") == 0) {
        if((mtk_ap_buf->def_intf->bss_idx > 1) && (mtk_ap_buf->def_intf->bss_idx < 6)){
            strcpy(mtk_ap_buf->def_intf->AuthModeBSSID[mtk_ap_buf->def_intf->bss_idx -1], ";WPA2PSK");
            strcpy(mtk_ap_buf->def_intf->EncryptBSSID[mtk_ap_buf->def_intf->bss_idx-1], ";AES");
        } else if (mtk_ap_buf->def_intf->bss_idx <= 1){
            mtk_ap_buf->def_intf->security_set = 1;
            strcpy(mtk_ap_buf->def_intf->AuthModeBSSID[0], "WPA2PSK");
            strcpy(mtk_ap_buf->def_intf->EncryptBSSID[0], "AES");
            if (mtk_ap_buf->intern_flag.capi_dual_pf) {
                update_capi_dual_table(ap_buf, "WPA2-PSK", "0");
            }
            else
                dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
        }
    } else if (strcasecmp(keymgnt, "SAE") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA3PSK");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "AES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-PSK-SAE") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA2PSKWPA3PSK");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "AES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-PSK-SAE-Mixed") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA2PSKMIXWPA3PSK");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "AES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "OWE") == 0) {
        if((mtk_ap_buf->def_intf->WLAN_TAG_bss_num> 1) && (mtk_ap_buf->def_intf->WLAN_TAG_bss_num < 6)) {
            char SSID_OWE[32], AuthMode[32], Encrypt[32];

            strcpy(SSID_OWE, dict_search(commit_table, "SSID1="));
            SSID_OWE[strlen(SSID_OWE) - 1] = 0;
            strcat(SSID_OWE, "-owe");
            printf("OWE SSID %s\n", SSID_OWE);
            sprintf(gCmdStr,  "SSID%d", WLAN_TAG_bss_idx);
            dict_update(commit_table, dict_search_lower(trans_table, gCmdStr), SSID_OWE);

            strcpy(AuthMode, dict_search(commit_table, dict_search_lower(trans_table, "AuthMode")));
            AuthMode[strlen(AuthMode) - 1] = 0;
            strcat(AuthMode, ";OWE");
            //printf("AuthMode %s\n", AuthMode);
            dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), AuthMode);

            strcpy(Encrypt, dict_search(commit_table, dict_search_lower(trans_table, "Encrypt")));
            Encrypt[strlen(Encrypt) - 1] = 0;
            strcat(Encrypt, ";AES");
            //printf("Encrypt %s\n", Encrypt);
            dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , Encrypt);
        } else {
            dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "OWE");
            dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "AES");
        }
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-PSK-Mixed") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPAPSKWPA2PSK");
        dict_update(commit_table, dict_search_lower(trans_table,"Encrypt"), "TKIPAES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WEP") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "OPEN");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "WEP");
        dict_update(commit_table, "WEPKey=", "1");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-Mixed") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA1WPA2");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "TKIPAES");
        dict_update(commit_table, "WdsEncrypType=", "NONE");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-MIX") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA2MIX");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "AES");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA-ENT") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt") , "TKIP");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else if (strcasecmp(keymgnt, "WPA2-ENT") == 0) {
        if (mtk_ap_buf->intern_flag.capi_dual_pf) {
            update_capi_dual_table(ap_buf, "WPA2-ENT", "0");
        }
        else {
            dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA2");
            dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "AES");
            dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
        }
    } else if (strcasecmp(keymgnt, "SuiteB") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA3-192");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "GCMP256");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "1");
    } else if (strcasecmp(keymgnt, "WPA") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "AuthMode"), "WPA");
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "TKIP");
        dict_update(commit_table, "WdsEncrypType=", "NONE");
        dict_update(commit_table, dict_search_lower(trans_table, "IEEE1x"), "0");
    } else {
        printf("Invalid Security mode\n");
    }

    if (strcasecmp(pairwise_cipher, "") == 0) {
        printf("!!!!!No configuration, use default!!!!!\n");
    } else if (strcasecmp(pairwise_cipher, "AES-GCMP-256") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "GCMP256");
    } else if (strcasecmp(pairwise_cipher, "AES-CCMP-256") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "CCMP256");
    } else if (strcasecmp(pairwise_cipher, "AES-GCMP-128") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "GCMP128");
    } else if (strcasecmp(pairwise_cipher, "AES-CCMP-128") == 0) {
        dict_update(commit_table, dict_search_lower(trans_table, "Encrypt"), "AES");
    } else {
        printf("!!!!!Invalid PairwiseCipher, use default!!!!!\n");
    }

    //printf("++++++ updated table size:%d ++++++\n", commit_table->size);
    return WFA_SUCCESS;
}


int mtk_ap_set_staqos(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    return WFA_SUCCESS;
}


int mtk_ap_set_wireless(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    dict_t trans_table = mtk_ap_buf->key_translation_table;
    dict_t commit_table = mtk_ap_buf->commit_dict;
    char ** value_ptr;
    char* CAPI_key;
    int i, j, value_i;
    const char *drv_value;
    int chan=0;
    int intf_set = 1;

    value_ptr = data->values;
    printf("-----start looping: %d\n", data->count);

    intf_set = CAPI_set_intf(mtk_ap_buf);
    commit_table = mtk_ap_buf->commit_dict;

    if (commit_table  && (commit_table->size > 0) ) {
        if (((mtk_ap_buf->def_mode == WIFI_5G) ||(mtk_ap_buf->def_mode == WIFI_6G)) && (!mtk_ap_buf->intern_flag.BW_5G_set)){
            printf("Set 5G/6G interface default VTH BW to 80MHz!\n");
            dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW"), "1");
        }
    }

    mtk_ap_buf->WLAN_TAG = -1;
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "WLAN_TAG") == 0) {

            mtk_ap_buf->WLAN_TAG = atoi((value_ptr)[i]) - 1;
            if (mtk_ap_buf->WLAN_TAG < 0)
                mtk_ap_buf->WLAN_TAG = 0;

            if (intf_set) {
                mtk_ap_buf->WLAN_TAG_inf[mtk_ap_buf->WLAN_TAG] = mtk_ap_buf->def_intf;

                for (j=0; j<mtk_ap_buf->def_intf->WLAN_TAG_bss_num; j++) {
                    printf("WLAN_TAG[%d]=%d\n", j, mtk_ap_buf->def_intf->WLAN_TAG[j]);
                    if (mtk_ap_buf->def_intf->WLAN_TAG[j] == atoi((value_ptr)[i])) {
                        break;
                    }
                }
                if(j==mtk_ap_buf->def_intf->WLAN_TAG_bss_num) {
                    mtk_ap_buf->def_intf->WLAN_TAG[mtk_ap_buf->def_intf->WLAN_TAG_bss_num++] = atoi((value_ptr)[i]);
                    if (mtk_ap_buf->def_intf->WLAN_TAG_bss_num > 1) {
                        if (mtk_ap_buf->def_intf->mbss_en == 0)
                            mtk_ap_buf->def_intf->mbss_en = 1;

                        printf("intf %s BSS Num set from WLAN_TAG: %d\n", mtk_ap_buf->def_intf->name,
                            mtk_ap_buf->def_intf->WLAN_TAG_bss_num);
                        sprintf(gCmdStr, "%d", (mtk_ap_buf->def_intf->WLAN_TAG_bss_num));
                        dict_update(commit_table, dict_search_lower(trans_table, "BssidNum"), gCmdStr);
                    }
                }
            } else {
                if(mtk_ap_buf->WLAN_TAG_inf[mtk_ap_buf->WLAN_TAG]) {
                    set_default_intf(mtk_ap_buf, mtk_ap_buf->WLAN_TAG_inf[mtk_ap_buf->WLAN_TAG]->mode);
                    commit_table = mtk_ap_buf->commit_dict;
                }
            }
            printf("Default interface=%s\n", mtk_ap_buf->def_intf->name);
            continue;
        } else if (strcasecmp((data->params)[i], "ChnlFreq") == 0) {
            continue;
        } else if (strcasecmp((data->params)[i], "Reg_Domain") == 0) {
            strcpy(mtk_ap_buf->Reg_Domain, (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "SSID") == 0) {
            if (mtk_ap_buf->def_intf->bss_idx > 1) {
                sprintf(gCmdStr,  "SSID%d", (mtk_ap_buf->def_intf->bss_idx));
                dict_update(commit_table, dict_search_lower(trans_table, gCmdStr), (value_ptr)[i]);
                continue;
            }

            if (mtk_ap_buf->intern_flag.capi_dual_pf) {
                update_capi_dual_table(ap_buf, "SSID", (value_ptr)[i]);
            } else {
                strcpy(mtk_ap_buf->def_intf->SSID, (value_ptr)[i]);
                dict_update(commit_table, dict_search_lower(trans_table, "SSID"), (value_ptr)[i]);
            }
            continue;
        } else if (strcasecmp((data->params)[i], "Mode") == 0) {
            if (strcasecmp((value_ptr)[i], "11ax") == 0)  {
                //dict_update(commit_table, dict_search_lower(trans_table, "HT_OpMode"), "0");
                free(value_ptr[i]);
                if (mtk_ap_buf->def_mode == WIFI_2G)
                    value_ptr[i] = strdup("11ax_2g");
                else if (mtk_ap_buf->def_mode == WIFI_5G)
                    value_ptr[i] = strdup("11ax_5g");
                else if (mtk_ap_buf->def_mode == WIFI_6G)
                    value_ptr[i] = strdup("11ax_6g");
            }

            if (mtk_ap_buf->intern_flag.capi_dual_pf) {
                update_capi_dual_table(ap_buf, "Mode", (value_ptr)[i]);
            } else {
                drv_value= table_search_lower(mode_HTOp_tbl, (value_ptr)[i]);
                dict_update(commit_table, dict_search_lower(trans_table, "HT_OpMode"), drv_value);
                drv_value= table_search_lower(mode_tbl, (value_ptr)[i]);
                dict_update(commit_table, dict_search_lower(trans_table, "Mode"), drv_value);
            }
            continue;
        } else if (strcasecmp((data->params)[i], "WME") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "WME"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "WMMPS") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "WMMPS"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "FRGMNT") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "FragThr"), (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "RTS") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "RTSThr"), (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "Offset") == 0) {
            drv_value= table_search_lower(ExtChStr_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "ExChannelOffset"), drv_value);
            continue;
        } else if ((strcasecmp((data->params)[i], "SGI20") == 0) ||
            (strcasecmp((data->params)[i], "SGI_20") == 0)) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "ShortGI20"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "40_Intolerant") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "40_Intolerant"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "BCC") == 0) {
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "HT_LDPC"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_LDPC"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "HE_LDPC"), "0");
            } else if (strcasecmp((value_ptr)[i], "disable") == 0){
                dict_update(commit_table, dict_search_lower(trans_table, "HT_LDPC"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_LDPC"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "HE_LDPC"), "1");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "LDPC") == 0) {
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "HT_LDPC"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_LDPC"), "1");
                 dict_update(commit_table, dict_search_lower(trans_table, "HE_LDPC"), "1");
           } else if (strcasecmp((value_ptr)[i], "disable") == 0){
                dict_update(commit_table, dict_search_lower(trans_table, "HT_LDPC"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_LDPC"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "HE_LDPC"), "0");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "PPDUTxType") == 0) {
            drv_value= table_search_lower(PPDUTxType_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "%s value is wrong %s, must be SU, MU, ER, TB, or Legacy\n",
                    (data->params)[i], (value_ptr)[i]);
                continue;
            }
            free(value_ptr[i]);
            value_ptr[i] = strdup(drv_value);
#if 0
            sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=ppdu_tx_type-%s", mtk_ap_buf->def_intf->name, drv_value);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            ADD_POST_CMD(gCmdStr);
#endif
        } else if (strcasecmp((data->params)[i], "MIMO") == 0) {
            if (strcasecmp((value_ptr)[i], "UL") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "MIMO_DL"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "MIMO_UL"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "HT_BAWinSize"), "64");
                dict_update(commit_table, dict_search_lower(trans_table, "MU_TxBF"), "0");
                mtk_ap_buf->def_intf->UL_MUMIMO = 1;
             } else if (strcasecmp((value_ptr)[i], "DL") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "MIMO_DL"), "1");
                dict_update(commit_table, dict_search_lower(trans_table, "MU_TxBF"), "1");
                mtk_ap_buf->def_intf->DL = 1;
            }
            continue;
        } else if (strcasecmp((data->params)[i], "OFDMA") == 0) {
            drv_value= table_search_lower(ofdma_dir_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "%s value is wrong %s, must be AUTO, DL, UL, or 20and80\n",
                    (data->params)[i], (value_ptr)[i]);
                continue;
            }
            if (strcasecmp((value_ptr)[i], "UL") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "MuOfdmaUlEnable"), "1");
            } else if ((strcasecmp((value_ptr)[i], "DL") == 0) || (strcasecmp((value_ptr)[i], "DL-20and80") == 0)) {
                dict_update(commit_table, dict_search_lower(trans_table, "MuOfdmaDlEnable"), "1");
                mtk_ap_buf->def_intf->DL = 1;
            }
            free(value_ptr[i]);
            value_ptr[i] = strdup(drv_value);
        } else if (strcasecmp((data->params)[i], "RRM") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "RRM"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "radio") == 0) {
            if (strcasecmp((value_ptr)[i], "on") == 0) {
                value_i = 1;
            } else if (strcasecmp((value_ptr)[i], "off") == 0){
                value_i = 0;
            }
            if ((value_i == 0) || (value_i == 1)) {
                if (value_i == 0)
                    sleep(6);
                if (mtk_ap_buf->intf_2G.status) {
                    sprintf(gCmdStr, "iwpriv %s set RadioOn=%d", mtk_ap_buf->intf_2G.name, value_i);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                    system(gCmdStr);
                }
                if (mtk_ap_buf->intf_5G.status) {
                    sprintf(gCmdStr, "iwpriv %s set RadioOn=%d", mtk_ap_buf->intf_5G.name, value_i);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                    system(gCmdStr);
                }
                if (mtk_ap_buf->intf_6G.status) {
                    sprintf(gCmdStr, "iwpriv %s set RadioOn=%d", mtk_ap_buf->intf_6G.name, value_i);
                    DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
                    system(gCmdStr);
                }
            }
            continue;
        } else if ((strcasecmp((data->params)[i], "SPATIAL_RX_STREAM") == 0) ||
            (strcasecmp((data->params)[i], "SPATIAL_TX_STREAM") == 0)){
            if ((strcasecmp((value_ptr)[i], "1") == 0) || (strcasecmp((value_ptr)[i], "2") == 0) ||
                (strcasecmp((value_ptr)[i], "3") == 0) || (strcasecmp((value_ptr)[i], "4") == 0)) {
                value_i = value_i; /* Basically do nothing */
            } else if (drv_value= table_search_lower(SPATIAL_STREAM_tbl, (value_ptr)[i])){
                free(value_ptr[i]);
                value_ptr[i] = strdup(drv_value);
            } else {
                     DPRINT_INFO(WFA_OUT, "%s value is wrong %s, must be 1SS,2SS,3SS,4SS,1,2,3,4\n", (data->params)[i],(value_ptr)[i]);
                    continue;
            }
        } else if (strcasecmp((data->params)[i], "WIDTH") == 0) {
            drv_value= table_search_lower(width_HTBW_tbl, (value_ptr)[i]);
            if (!drv_value)  {
                DPRINT_INFO(WFA_OUT, "WIDTH value is wrong %s, must be 160,80,40 or 20\n", (value_ptr)[i]);
                continue;
            }
            dict_update(commit_table, dict_search_lower(trans_table, "HT_BW"), drv_value);
            drv_value= table_search_lower(width_VHTBW_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW"), drv_value);
            mtk_ap_buf->intern_flag.BW_5G_set = 1;
            continue;
        } else if (strcasecmp((data->params)[i], "TWTinfoFrameRx") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "TWT_RespSupport") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, (data->params)[i]), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "TxBF") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "TxBF"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "STBC_TX") == 0) {
            dict_update(commit_table, dict_search_lower(trans_table, "STBC"), value_ptr[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "VHT_STBC"), value_ptr[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "FORCE_STBC"), value_ptr[i]);
            if (strcasecmp(value_ptr[i], "1") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "TXBF"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "BFBACKOFFenable"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "BfSmthIntlBbypass"), "0");
                dict_update(commit_table, dict_search_lower(trans_table, "ITxBfEn"), "0");
            }
            continue;
        } else if ((strcasecmp((data->params)[i], "ADDBAReq_BufSize") == 0) ||
            (strcasecmp((data->params)[i], "ADDBAResp_BufSize") == 0)){
            if (strcasecmp((value_ptr)[i], "gt64") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "HT_BAWinSize"), "256");
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "HT_BAWinSize"), "64");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "HE_TXOPDurRTSThr") == 0){
            if (strcasecmp((value_ptr)[i], "Enable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "TXOPDurRTSThr"), "100");
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "TXOPDurRTSThr"), "1023");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "NumNonTxBSS") == 0){
            mtk_ap_buf->def_intf->bss_num = atoi((value_ptr)[i]) + 1;
            printf("NumNonTxBSS: in str %s, in integer %d\n", (value_ptr)[i], 
                mtk_ap_buf->def_intf->bss_num - 1);
            if (mtk_ap_buf->def_intf->bss_num > 1) {
                sprintf(gCmdStr, "%d", (mtk_ap_buf->def_intf->bss_num));
                dict_update(commit_table, dict_search_lower(trans_table, "BssidNum"), gCmdStr);

                strcpy(gCmdStr, "1");
                for (j=1; j<mtk_ap_buf->def_intf->bss_num; j++)
                    strcat(gCmdStr, ";1");
                printf("gCmdStr: in str %s\n", gCmdStr);
                dict_update(commit_table, dict_search_lower(trans_table, "DOT11V_MBSSID"), gCmdStr);
            }
            continue;
        } else if (strcasecmp((data->params)[i], "MBSSID") == 0){
            if (strcasecmp((value_ptr)[i], "enable") == 0)
                mtk_ap_buf->def_intf->mbss_en = 1;
            else if (strcasecmp((value_ptr)[i], "disable") == 0)
                mtk_ap_buf->def_intf->mbss_en = 0;
            continue;
        } else if (strcasecmp((data->params)[i], "NonTxBSSIndex") == 0){
            mtk_ap_buf->def_intf->bss_idx = atoi((value_ptr)[i]) + 1;
            continue;
        } else if (strcasecmp((data->params)[i], "BA_Recv_Status") == 0) {
            printf("%s is supported in driver by default. No need to do anything!\n", (data->params)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "FT_OA") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "FT_OA"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "FT_DS") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "FT_DS"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "Cellular_Cap_Pref") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", mtk_ap_buf->def_intf->name, 
            table_search_lower(WappCmd, "Cellular_Cap_Pref"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            continue;
        } else if (strcasecmp((data->params)[i], "GAS_CB_Delay") == 0) {
            sprintf(gCmdStr, "wappctrl %s %s %s\n", mtk_ap_buf->def_intf->name, 
            table_search_lower(WappCmd, "Gas_CB_Delay"), (value_ptr)[i]);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
            continue;
        } else if (strcasecmp((data->params)[i], "MU_EDCA") == 0){
            if (strcasecmp((value_ptr)[i], "override") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "MuEdcaOverride"), "1");
            } else if (strcasecmp((value_ptr)[i], "disable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "MuEdcaOverride"), "0");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "MU_TxBF") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "MU_TxBF"), drv_value);
            dict_update(commit_table, dict_search_lower(trans_table, "MIMO_DL"), drv_value);
            dict_update(commit_table, dict_search_lower(trans_table, "PPDUTxType"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "UnsolicitedProbeResp") == 0) {
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "UnsolicitedProbeResp"), "1");
                Force_UnsolicitedProbeResp = 1;
            }
            continue;
        } else if (strcasecmp((data->params)[i], "FILSDscv") == 0) {
            if (strcasecmp((value_ptr)[i], "enable") == 0)
                dict_update(commit_table, dict_search_lower(trans_table, "FILSDscv"), "2");
            continue;
        } else if (strcasecmp((data->params)[i], "ActiveInd_UnsolicitedProbeResp") == 0) {
            drv_value= table_search_lower(DisEn_01_tbl, (value_ptr)[i]);
            dict_update(commit_table, dict_search_lower(trans_table, "ActiveInd_UnsolicitedProbeResp"), drv_value);
            continue;
        } else if (strcasecmp((data->params)[i], "ExpBcnLength") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "FT_BSS_LIST") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "OMCtrl_ULMUDataDisableRx") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "NumSoundDim") == 0) {
            printf("%s %s, Default value is 4, do nothing!\n",(data->params)[i], (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "ANQP") == 0) {
            printf("%s %s, temporarily do nothing!\n",(data->params)[i], (value_ptr)[i]);
            continue;
        } else if (strcasecmp((data->params)[i], "Band6Gonly") == 0) {
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                mtk_ap_buf->Band6Gonly.intf_6G_only = 1;
                if(mtk_ap_buf->intf_2G.status) {
                    ifconfig_interface_down(mtk_ap_buf->intf_2G.name);
                    mtk_ap_buf->Band6Gonly.intf_2G_orig_stat = mtk_ap_buf->intf_2G.status;
                    mtk_ap_buf->intf_2G.status = 0;
                }
                if(mtk_ap_buf->intf_5G.status) {
                    if (strcasecmp(mtk_ap_buf->intf_2G.name, mtk_ap_buf->intf_5G.name) != 0) {
                        ifconfig_interface_down(mtk_ap_buf->intf_5G.name);
                    }
                    mtk_ap_buf->Band6Gonly.intf_5G_orig_stat = mtk_ap_buf->intf_5G.status;
                    mtk_ap_buf->intf_5G.status = 0;
                }
                dict_update(commit_table, dict_search_lower(trans_table, "CountryCode"), "US");
                dict_update(commit_table, dict_search_lower(trans_table, "CountryRegion"), "5");
                dict_update(commit_table, dict_search_lower(trans_table, "CountryRegionABand"), "7");
                printf("6G only interface is enabled, turn off 2G/5G interface!\n");
            }
            continue;
       }
        else if (strcasecmp((data->params)[i], "BW_SGNL") == 0) {
            printf("BW_SGNL value %s!\n", value_ptr[i]);
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW_SIGNAL"), "2");
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW_SIGNAL"), "0");
            }
            continue;
        }
        else if (strcasecmp((data->params)[i], "DYN_BW_SGNL") == 0) {
            printf("DYN_BW_SGNL value %s!\n", value_ptr[i]);
            if (strcasecmp((value_ptr)[i], "enable") == 0) {
                value_i = 2;
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW_SIGNAL"), "2");
            }
            else {
                value_i = 1;
                dict_update(commit_table, dict_search_lower(trans_table, "VHT_BW_SIGNAL"), "1");
            }
            continue;
        } else if (strcasecmp((data->params)[i], "CHANNEL") == 0) {
            if (NULL != (strstr((value_ptr)[i], ";"))) {
                mtk_ap_buf->intern_flag.capi_dual_pf = 1;
            }
            if (mtk_ap_buf->intern_flag.capi_dual_pf) {
                update_capi_dual_table(ap_buf, "CHANNEL", (value_ptr)[i]);
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "Channel"), (value_ptr)[i]);
            }
            continue;
        }
        else if (strcasecmp((data->params)[i], "BCNINT") == 0) {
            if (mtk_ap_buf->intern_flag.capi_dual_pf) {
                update_capi_dual_table(ap_buf, "BCNINT", (value_ptr)[i]);
            } else {
                dict_update(commit_table, dict_search_lower(trans_table, "BcnInt"), (value_ptr)[i]);
            }
            continue;
        }

        CAPI_key = dict_search(trans_table, (data->params)[i]);
        if (CAPI_key != 0) {
            dict_update(commit_table, CAPI_key, (value_ptr)[i]);
        } else {
            printf("!!!!!not in table!!!!!%s\n", data->params[i]);
        }
    }
    //printf("++++++ updated table size:%d ++++++\n", commit_table->size);
    if (mtk_ap_buf->WLAN_TAG < 0)
        mtk_ap_buf->WLAN_TAG = 0;
    return WFA_SUCCESS;
}

int mtk_dev_configure_ie(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    char ** value_ptr;
    char result[48];
    char intf[10];
    int i;
    char IE_Name[30], Contents[512];

    strcpy(result, "");

    memset(IE_Name, 0, sizeof(IE_Name));
    memset(Contents, 0, sizeof(Contents));
    strcpy(intf, mtk_ap_buf->def_intf->name);
    value_ptr = data->values;

    CAPI_set_intf(mtk_ap_buf);

    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "IE_Name") == 0) {
            strcpy(IE_Name, (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "Contents") == 0) {
            strcpy(Contents, (value_ptr)[i]);
        }
    }

    if ((strcasecmp(IE_Name, "RSNE") == 0) || (strcasecmp(IE_Name, "WPA_IE") == 0)) {
        str_lower(Contents);
        sprintf(gCmdStr, "iwpriv %s set bcn_rsne=%s\n", intf, Contents);
        if(mtk_ap_buf->intern_flag.commited) {
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        } else {
            ADD_POST_CMD(gCmdStr);
        }
    }

    sprintf((char *) resp_buf, "%s", result);
    printf("%s\n", resp_buf);
    return WFA_SUCCESS;
}

int mtk_dev_exec_action(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_dev_exec_action function ===== \n");
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    printf("(%s)\n", data->interface);
    char ** value_ptr;
    char result[48];
    char intf[10];
    int i, j, ret;
    unsigned int rejected_group = 0;
    char STA_MAC[20];
    FILE *fp;
    char *start, *end;
    strcpy(result, "");

    memset(STA_MAC, 0, sizeof(STA_MAC));
    strcpy(intf, mtk_ap_buf->def_intf->name);
    value_ptr = data->values;
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "Rejected_DH_Groups") == 0){

            if(STA_MAC == NULL) {
                printf("STA_MAC_Address can't be NULL, return!!!\n");
                return WFA_ERROR;
            }
            sprintf(gCmdStr, "iwpriv %s show sae_rejected_group=%s\n", intf, STA_MAC);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            fp = popen(gCmdStr, "r");
            if (fp == NULL) {
                printf("Failed to run command!!!\n");
                return WFA_ERROR;
            }

            while (fgets(gCmdStr, sizeof(gCmdStr), fp) != NULL) {
                start = strstr(gCmdStr, "show:");
                if(start == NULL)
                    continue;
                /* Replace the last charactor from '\n' to '\0' */
                start = start +5;
                start[strlen(start)-1] = '\0';
                rejected_group = (unsigned int)strtol(start, NULL, 16);
                strcpy(result, ",Rejected_DH_Groups,");
                for (j = 0; j < 32; j++) {
                    if (rejected_group & (1 << j))
                        sprintf(result + strlen(result),"%d ", j);
                }
                if (strlen(result)>1)
                    result[strlen(result)-1] = '\0';

                break;
            }
            pclose(fp);
        } else if (strcasecmp((data->params)[i], "Dest_MAC") == 0) {
            strcpy(STA_MAC, (value_ptr)[i]);
        } else if (strcasecmp((data->params)[i], "KeyRotation") == 0) {
            if ((strcasecmp((value_ptr)[i], "1") == 0)) {
                sprintf(gCmdStr, "iwpriv %s set RekeyInterval=30\n", intf);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                system(gCmdStr);
            }
        }
    }
    sprintf((char *) resp_buf, "%s", result);
    printf("%s\n", resp_buf);
    return WFA_SUCCESS;
}

int mtk_dev_send_frame(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    char ** value_ptr;
    char intf[10], FrameName[20], Dest_MAC[20], Cand_List[3], RegClass[5], Channel[5], RandInt[5], MeaDur[5];
    char MeaMode[10], BSSID[20], SSID[32], RptCond[3], RptDet[3], MeaDurMand[3], APChanRpt[20], ReqInfo[20], LastBeaconRptIndication[3];
    char DisassocTimer[10];
    char TXOPDur[5], interval[5];
    char MMIC_IE[3], MMIC_IE_BIPNResue[3], MMIC_IE_InvalidMIC[3], OMN_IE[3], CSA_IE[3], channelwidth[5], channelswitchcount[5];
    char nss[3], HT_Opt_IE[3], HT_Opt_IE_ChanWidth[5], HT_Opt_IE_NSS[3];
    char Protected[20], stationID[24], mode[15], source[24];
    int i;

    memset(APChanRpt, 0, sizeof(APChanRpt));
    memset(RandInt, 0, sizeof(RandInt));
    memset(LastBeaconRptIndication, 0, sizeof(LastBeaconRptIndication));
    memset(SSID, 0, sizeof(SSID));
    memset(RegClass, 0, sizeof(RegClass));
    memset(Channel, 0, sizeof(Channel));
    memset(channelwidth, 0, sizeof(channelwidth));
    memset(channelswitchcount, 0, sizeof(channelswitchcount));
    memset(nss, 0, sizeof(nss));
    memset(HT_Opt_IE_ChanWidth, 0, sizeof(HT_Opt_IE_ChanWidth));
    memset(HT_Opt_IE_NSS, 0, sizeof(HT_Opt_IE_NSS));
    memset(source, 0, sizeof(source));
    strcpy(intf, mtk_ap_buf->def_intf->name);
    value_ptr = data->values;
    if ((strcasecmp(data->interface, "5G") == 0) || (strcasecmp(data->interface, "5.0") == 0)) {
        if (mtk_ap_buf->intf_5G.status) {
            strcpy(intf, mtk_ap_buf->intf_5G.name);
        } else {
            printf("5G interface is not supported, skip!\n");
        }
    } else if ((strcasecmp(data->interface, "2G") == 0) ||(strcasecmp(data->interface, "24G") == 0)
        ||(strcasecmp(data->interface, "2.4") == 0)) {
        if (mtk_ap_buf->intf_2G.status) {
            strcpy(intf, mtk_ap_buf->intf_2G.name);
        } else {
            printf("2G interface is not supported, skip!\n");
        }
    } else if (strcasecmp(data->interface, "6G") == 0) {
        if (mtk_ap_buf->intf_6G.status) {
            strcpy(intf, mtk_ap_buf->intf_6G.name);
        } else {
            printf("6G interface is not supported, skip!\n");
        }
    }

    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "FrameName") == 0) {
            strcpy(FrameName, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "Dest_MAC") == 0) {
            strcpy(Dest_MAC, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "DestMAC") == 0) {
            strcpy(Dest_MAC, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "Cand_List") == 0) {
            strcpy(Cand_List, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "RegClass") == 0) {
            strcpy(RegClass, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "Channel") == 0) {
            strcpy(Channel, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "OCIChannel") == 0) {
            strcpy(Channel, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "RandInt") == 0) {
            strcpy(RandInt, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "MeaDur") == 0) {
            strcpy(MeaDur, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "MeaMode") == 0) {
            if (strcasecmp((value_ptr)[i], "PASSIVE") == 0) {
                strcpy(MeaMode, "0");
            } else if (strcasecmp((value_ptr)[i], "ACTIVE") == 0) {
                strcpy(MeaMode, "1");
            } else if (strcasecmp((value_ptr)[i], "TABLE") == 0) {
                strcpy(MeaMode, "2");
            } 
        }  else if (strcasecmp((data->params)[i], "BSSID") == 0) {
            strcpy(BSSID, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "SSID") == 0) {
            strcpy(SSID, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "RptCond") == 0) {
            strcpy(RptCond, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "RptDet") == 0) {
            strcpy(RptDet, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "MeaDurMand") == 0) {
            strcpy(MeaDurMand, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "APChanRpt") == 0) {
            strcpy(APChanRpt, (value_ptr)[i]);
            replace_char(APChanRpt, '_', '#');
        }  else if (strcasecmp((data->params)[i], "LastBeaconRptIndication") == 0) {
            strcpy(LastBeaconRptIndication, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "ReqInfo") == 0) {
            strcpy(ReqInfo, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "type") == 0) {
            continue;
        }  else if (strcasecmp((data->params)[i], "TXOPDur") == 0) {
            strcpy(TXOPDur, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "interval") == 0) {
            strcpy(interval, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "source") == 0) {
            strcpy(source, (value_ptr)[i]);
        }  else if (strcasecmp(FrameName, "beacon") == 0) {
            if (strcasecmp((data->params)[i], "MMIC_IE_InvalidMIC") == 0) {
                strcpy(MMIC_IE_InvalidMIC, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "MMIC_IE_BIPNResue") == 0) {
                strcpy(MMIC_IE_BIPNResue, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "MMIC_IE") == 0) {
                strcpy(MMIC_IE, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "OMN_IE") == 0) {
                strcpy(OMN_IE, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "CSA_IE") == 0) {
                strcpy(CSA_IE, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "channelwidth") == 0) {
                strcpy(channelwidth, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "channelswitchcount") == 0) {
                strcpy(channelswitchcount, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "nss") == 0) {
                strcpy(nss, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "HT_Opt_IE") == 0) {
                strcpy(HT_Opt_IE, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "HT_Opt_IE_ChanWidth") == 0) {
                strcpy(HT_Opt_IE_ChanWidth, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "HT_Opt_IE_NSS") == 0) {
                strcpy(HT_Opt_IE_NSS, (value_ptr)[i]);
            }
        }  else if (strcasecmp(FrameName, "deauth") == 0) {
            if (strcasecmp((data->params)[i], "Protected") == 0) {
                strcpy(Protected, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "stationID") == 0) {
                strcpy(stationID, (value_ptr)[i]);
            }  else if (strcasecmp((data->params)[i], "mode") == 0) {
                strcpy(mode, (value_ptr)[i]);
            }
        } else {
            printf("dev_send_frame %s  Command is ignored or invalid!\n", data->params[i]);
        }
    }

    if (source[0] != 0) {
        char mac_address_buf[24];
        read_mac_address_file(mac_address_buf, intf);
        DPRINT_INFO(WFA_OUT, "%s MAC is %s\n", intf, mac_address_buf);
        if (strncasecmp(mac_address_buf, source, 17) == 0) {
            DPRINT_INFO(WFA_OUT, "Source %s is %s\n", source, intf);
        } else {
            DPRINT_INFO(WFA_OUT, "Source %s is not %s\n", source, intf);
        }
    }

    if (strcasecmp(FrameName, "BTMReq") == 0) {
        if (mtk_ap_buf->DisAssoc_Imnt == 0)
            strcpy(DisassocTimer, "0");
        else
            strcpy(DisassocTimer, "10");

        sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
            table_search_lower(WappCmd, "Disassoc_Timer"), DisassocTimer);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);

        sprintf(gCmdStr, "wappctrl %s %s %s\n", intf,
            table_search_lower(WappCmd, "send_BTMReq"), Dest_MAC);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    } else if (strcasecmp(FrameName, "BcnRptReq") == 0) {
        sprintf(gCmdStr, "iwpriv %s set BcnReqRandInt=%s\n", intf, RandInt);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);

        if (strcasecmp(LastBeaconRptIndication, "") != 0) {
            sprintf(gCmdStr, "iwpriv %s set LastBeaconRptIndication=%s\n", intf, LastBeaconRptIndication);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(SSID, "ZeroLength") == 0) {
            memset(SSID, 0, sizeof(SSID));
        }

        sprintf(gCmdStr, "iwpriv %s set BcnReq=%s!%s!%s!%s!%s!%s!%s!%s!%s!%s\n", intf,
           Dest_MAC, MeaDur, RegClass, BSSID, SSID, Channel, MeaMode, RegClass, APChanRpt, RptDet);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    } else if (strcasecmp(FrameName, "S-MPDU") == 0) {
        sprintf(gCmdStr, "iwpriv %s set dev_send_frame=smpdu-%s!%s!%s\n", intf,
            Dest_MAC, TXOPDur, interval);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    } else if ((strcasecmp(FrameName, "QoSNull") == 0) ||
        (strcasecmp(FrameName, "MsntPilot") == 0)) {
        sprintf(gCmdStr, "iwpriv %s set ap_rfeatures=qos_null_injector-1\n", intf);
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    } else if (strcasecmp(FrameName, "beacon") == 0) {
        if (strcasecmp(MMIC_IE_InvalidMIC, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set wpa3_test=6", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(MMIC_IE_BIPNResue, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set wpa3_test=7", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(OMN_IE, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:0", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(CSA_IE, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:1", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(MMIC_IE, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:2", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (strcasecmp(HT_Opt_IE, "1") == 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=0:3", intf);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (Channel[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=1:%s", intf, Channel);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (channelwidth[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=2:%s", intf, channelwidth);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (channelswitchcount[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=3:%s", intf, channelswitchcount);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (HT_Opt_IE_ChanWidth[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=4:%s", intf, HT_Opt_IE_ChanWidth);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (nss[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=5:%s", intf, nss);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
        if (HT_Opt_IE_NSS[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set bcn_prot_test=6:%s", intf, HT_Opt_IE_NSS);
            DPRINT_INFO(WFA_OUT, "run command ==> %s\n", gCmdStr);
            system(gCmdStr);
        }
    } else if (strcasecmp(FrameName, "deauth")== 0 ) {
         if (strcasecmp(Protected, "Unprotected")== 0 ) {
            sprintf(gCmdStr, "iwpriv %s set dev_send_frame=unicast_deauth-0-%s\n", intf, stationID);
        } else  if ((strcasecmp(Protected, "Protected")== 0 ) || (strcasecmp(Protected, "CorrectKey")== 0 )) {
            sprintf(gCmdStr, "iwpriv %s set dev_send_frame=unicast_deauth-1-%s\n", intf, stationID);
        } else  if (strcasecmp(Protected, "IncorrectKey")== 0 ) {
            sprintf(gCmdStr, "iwpriv %s set dev_send_frame=unicast_deauth-2-%s\n", intf, stationID);
        } else if (strcasecmp(mode, "Unprotected")== 0 ) {
            sprintf(gCmdStr, "iwpriv %s set dev_send_frame=unicast_deauth-0-%s\n", intf, Dest_MAC);
        }
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    } else if (strcasecmp(FrameName, "ChannelSwitchAnncment") == 0) {
         if (Channel[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set channel=%s\n", intf, Channel );
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
    } else if (strcasecmp(FrameName, "SAQueryReq") == 0) {
        if (Channel[0] != 0) {
            sprintf(gCmdStr, "iwpriv %s set wpa3_test=12:%s\n", intf, Channel);
            DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
            system(gCmdStr);
        }
        sprintf(gCmdStr, "iwpriv %s set PMFSA_Q=%s\n", intf, Dest_MAC );
        DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
        system(gCmdStr);
    }
    return WFA_SUCCESS;
}

int mtk_ap_get_parameter(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running mtk_dev_exec_action function ===== \n");
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data; // Copy the capi cmd to local. start reading the dat ar
    char ** value_ptr;
    char result[200];
    char intf[10];
    int i, ret;
    char STA_MAC[20];
    FILE *fp;
    char *start;
    strcpy(result, "");

    memset(STA_MAC, 0, sizeof(STA_MAC));
    strcpy(intf, mtk_ap_buf->def_intf->name);
    value_ptr = data->values;
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "Parameter") == 0){
            if (strcasecmp((value_ptr)[i], "SSID") == 0) {
                fp = popen("iwconfig ra0", "r");
                if (fp == NULL) {
                    printf("Failed to run command!!!\n");
                    return WFA_ERROR;
                }

                while (fgets(gCmdStr, sizeof(gCmdStr), fp) != NULL) {
                    start = strstr(gCmdStr, "ESSID:");
                    if(start == NULL)
                        continue;
                    /* Replace the last charactor from '\n' to '\0' */
                    start = start +7;
                    start[strlen(start)-4] = '\0';
                    sprintf(result, "SSID,%s", start);
                    break;
                }

                pclose(fp);
            } else if (strcasecmp((value_ptr)[i], "PSK") == 0){
            } else if (strcasecmp((value_ptr)[i], "PMK") == 0){
                if(STA_MAC == NULL) {
                    printf("STA_MAC_Address can't be NULL, return!!!\n");
                    return WFA_ERROR;
                }
                sprintf(gCmdStr, "iwpriv %s show PMK=%s\n", intf, STA_MAC);
                DPRINT_INFO(WFA_OUT, "run command ==> %s", gCmdStr);
                fp = popen(gCmdStr, "r");
                if (fp == NULL) {
                    printf("Failed to run command!!!\n");
                    return WFA_ERROR;
                }

                while (fgets(gCmdStr, sizeof(gCmdStr), fp) != NULL) {
                    start = strstr(gCmdStr, "show:");
                    if(start == NULL)
                        continue;
                    /* Replace the last charactor from '\n' to '\0' */
                    start = start +5;
                    start[strlen(start)-1] = '\0';
                    sprintf(result, "PMK,%s", start);
                    break;
                }
                pclose(fp);
            }
        } else if (strcasecmp((data->params)[i], "STA_MAC_Address") == 0) {
            strcpy(STA_MAC, (value_ptr)[i]);
        }
    }
    sprintf((char *) resp_buf, "%s", result);
    printf("%s\n", resp_buf);
    return WFA_SUCCESS;
}

int mtk_traffic_send_ping(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== running %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data;
    char ** value_ptr;
    int i;
    char destination[20], framesize[8], frameRate[5], duration[5], iptype[3];
    value_ptr = data->values;
    int totalpkts, tos = 0;
    int streamid;
    //float interval;      /* it could be subseconds/100s minisecond */

    strcpy(framesize, "100");
    strcpy(frameRate, "1");
    strcpy(duration, "30");
    strcpy(iptype, "1");

    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "destination") == 0) {
            strcpy(destination, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "framesize") == 0) {
            strcpy(framesize, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "frameRate") == 0) {
            strcpy(frameRate, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "duration") == 0) {
            strcpy(duration, (value_ptr)[i]);
        }  else if (strcasecmp((data->params)[i], "iptype") == 0) {
            strcpy(iptype, (value_ptr)[i]);
        } else {
            printf("traffic_send_ping %s  parameter is ignored or invalid!\n", data->params[i]);
        }
    }
    //interval = (float) 1/atoi(frameRate);
    totalpkts = (int)(atoi(duration) * atoi(frameRate));

    streamId = streamId%3;
    streamid = ++streamId;

    if (strcasecmp(iptype, "2") == 0)  {
        if ( tos > 0)
            sprintf(gCmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s -c %i -Q %d -s %s -q >> /tmp/spout_%d.txt 2>/dev/null",
                streamid, streamid, destination, totalpkts, tos,  framesize, streamid);
        else
            sprintf(gCmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping6.sh %s -c %i -s %s -q >> /tmp/spout_%d.txt 2>/dev/null",
                streamid, streamid, destination, totalpkts, framesize, streamid);

        system(gCmdStr);
        printf("\nCS : The command string is %s", gCmdStr);
    } else {
        if (tos > 0)
            sprintf(gCmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s -c %i  -Q %d -s %s -q >> /tmp/spout_%d.txt 2>/dev/null",
                streamid, streamid, destination, totalpkts, tos, framesize, streamid);
        else
            sprintf(gCmdStr, "echo streamid=%i > /tmp/spout_%d.txt;wfaping.sh %s -c %i -s %s -q >> /tmp/spout_%d.txt 2>/dev/null",
                streamid, streamid, destination, totalpkts, framesize, streamid);

        system(gCmdStr);
        printf("\nCS : The command string is %s", gCmdStr);
    }

    sprintf(gCmdStr, "updatepid.sh /tmp/spout_%d.txt", streamid);
    system(gCmdStr);
    printf("\nCS : The command string is %s", gCmdStr);

    sprintf((char *) resp_buf, "%d", streamid);
    printf("%s\n", resp_buf);

    return WFA_SUCCESS;
}

int mtk_traffic_stop_ping(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== Runing  %s function ===== \n", __func__);
    mtk_ap_buf_t* mtk_ap_buf = (mtk_ap_buf_t*) ap_buf;
    capi_data_t* data = mtk_ap_buf->capi_data;
    char ** value_ptr;
    int i;
    FILE *tmpfile = NULL;
    int streamid = -1;
    int sendCnt = 0;
    int repliedCnt = 0;
    char result[48];
    strcpy(result, "");

    value_ptr = data->values;
    for (i = 0; i < data->count; i++) {
        if (strcasecmp((data->params)[i], "streamID") == 0) {
            streamid = atoi((value_ptr)[i]);
        } else {
            printf("traffic_stop_ping %s  parameter is ignored or invalid!\n", data->params[i]);
        }
    }

    if ( streamid == -1 ) {
        sprintf((char *) resp_buf, "fail");
        return WFA_SUCCESS;
    }

    sprintf(gCmdStr, "getpid.sh /tmp/spout_%d.txt /tmp/pid.txt", streamid);
    system(gCmdStr);
    printf("\n The command string is %s", gCmdStr);
    system("stoping.sh /tmp/pid.txt ; sleep 2");
    sprintf(gCmdStr, "getpstats.sh /tmp/spout_%d.txt", streamid);
    system(gCmdStr);
    printf("\n The command string is %s", gCmdStr);
    tmpfile = fopen("/tmp/stpsta.txt", "r+");

    if (tmpfile == NULL)
        return WFA_FAILURE;

    if (fscanf(tmpfile, "%s", gCmdStr) != EOF) {
        if (*gCmdStr == '\0')
            sendCnt = 0;
        else
            sendCnt = atoi(gCmdStr);
	}

    if (fscanf(tmpfile, "%s", gCmdStr) != EOF) {
        if (*gCmdStr == '\0')
            repliedCnt = 0;
        else
            repliedCnt = atoi(gCmdStr);
    }
    fclose(tmpfile);

    sprintf(result, "sent,%d,replies,%d", sendCnt, repliedCnt);
    sprintf((char *) resp_buf, "%s", result);
    printf("%s\n", resp_buf);

    return WFA_SUCCESS;
}

int mtk_traffic_agent_reset(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== Do nothing for   %s function ===== \n", __func__);
    char result[48];
    strcpy(result, "");

    return WFA_SUCCESS;
}

int mtk_ignore_capi(int len, uint8_t *ap_buf, int *resp_len_ptr, uint8_t *resp_buf)
{
    printf("===== Do nothing for   %s function ===== \n", __func__);
    char result[48];
    strcpy(result, "");

    return WFA_SUCCESS;
}

