//
// Created by boalin on 7/25/17.
//

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
 *      File:  mtk_parse.c
 *      parse the CAPI CMD data and put them in CAPI_data_t
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <mtk_parse.h>
#include "wfa_debug.h"
#include "wfa_types.h"

extern unsigned short wfa_defined_debug;
extern char program[];

/*
 * Starts define AP function
 * edited by Boa-Lin Lai
 * This function does the sanity check, and wrap the element for DUT Control layer.
 * capi_str will be the raw data.
 * STA approach use TLV data format, we can use Dict for test.
 * uint8_t structure is for sending data through sockets
 */

void strip_char(char *str, char strip)
{
    char *p, *q;

    for (q = p = str; *p; p++)
        if (*p != strip)
            *q++ = *p;
    *q = '\0';
}

/*
 *  check the buffer
 */
static int buf_check(uint8_t *ret_buf, int *ret_len)
{
    if (ret_buf == NULL) return FALSE;

    memset(ret_buf, 0, *ret_len);
    return TRUE;
}


static void str_lower(char* str) {

    int i = 0;

    while(str[i]) {
        char c = tolower(str[i]);
        str[i] = c;
        i++;
    }
}

static char* parse_value(capi_data_t* c_data, char* token, char** save_ptr) {

    int c = c_data->count;

    c_data->params[c] = strdup(token);
    DPRINT_INFO(WFA_OUT, "para: %s\n", c_data->params[c]);
    str_lower(c_data->params[c]);
    token = strtok_r(NULL, ",", save_ptr);
    if (token) {
        c_data->values[c] = strdup(token);
        strip_char(c_data->values[c], ' ');
        if (strcasecmp(c_data->values[c], "EmptyData") == 0)
            strcpy(c_data->values[c], "");

        DPRINT_INFO(WFA_OUT, "value: %s\n", c_data->values[c]);
        c_data->count = ++c;
    } else {
        DPRINT_INFO(WFA_OUT, "Value is NULL, stop here!\n");
    }

    return (char*) *save_ptr;
}

static char* parse_value_no_strip(capi_data_t* c_data, char* token, char** save_ptr) {

    int c = c_data->count;

    c_data->params[c] = strdup(token);
    DPRINT_INFO(WFA_OUT, "para: %s\n", c_data->params[c]);
    str_lower(c_data->params[c]);
    token = strtok_r(NULL, ",", save_ptr);
    if (token) {
        c_data->values[c] = strdup(token);
        if (strcasecmp(c_data->values[c], "EmptyData") == 0)
            strcpy(c_data->values[c], "");

        DPRINT_INFO(WFA_OUT, "value: %s\n", c_data->values[c]);
        c_data->count = ++c;
    } else {
        DPRINT_INFO(WFA_OUT, "Value is NULL, stop here!\n");
    }

    return (char*) *save_ptr;
}

static char* parse_name(capi_data_t* c_data, char* token, char** save_ptr) {
    token = strtok_r(NULL, ",", save_ptr);
    strcpy(c_data->name, token);
    strip_char(c_data->name, ' ');

    DPRINT_INFO(WFA_OUT, "name: %s\n", c_data->name);
    return *save_ptr;
}


static char* parse_program(capi_data_t* c_data, char* token, char** save_ptr)
{
    token = strtok_r(NULL, ",", save_ptr);
    strcpy(c_data->program, token);
    strip_char(c_data->program, ' ');
    strcpy(program, c_data->program);

    DPRINT_INFO(WFA_OUT, "program: %s\n", c_data->program);
    return *save_ptr;
}

static char* parse_interface(capi_data_t* c_data, char* token, char** save_ptr)
{
    token = strtok_r(NULL, ",", save_ptr);
    strcpy(c_data->interface, token);
    strip_char(c_data->interface, ' ');

    DPRINT_INFO(WFA_OUT, "interface: %s\n", c_data->interface);
    return *save_ptr;
}

int parse_device_get_info(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    printf("total count:%d\n", c_data->count);

    return WFA_SUCCESS;
}

// 8.2
int parse_ap_ca_version(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.3
int parse_ap_config_commit(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    printf("raw %s\n", capi_str);
    c_data->count = 0;

    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    printf("start parsing!\n");
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.4
int parse_ap_deauth_sta(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    printf("raw : %s \n",capi_str);
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STA_MAC_Address") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.5
int parse_ap_get_mac_address(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NonTxBSSIndex") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WLAN_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.6
int parse_ap_reset_default(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    printf("raw : %s \n", capi_str);
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Type") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.7
int parse_ap_send_addba_req(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    printf("raw : %s \n", capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STA_MAC_Address") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.8
int parse_ap_send_bcnrpt_req(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "APChanRpt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Channel") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DestMAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaDur") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaDurMand") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaMode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RandInt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RegClass") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ReqInfo") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RptCond") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RptDet") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SSID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.9
int parse_ap_send_bsstrans_mgmt_req(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Candidate_List") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DestAddr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Dis_Assoc_Time") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Req_Mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Validity_Int") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.10
int parse_ap_send_link_mea_req(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Count") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DestAddr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Interval") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.11
int parse_ap_send_tsmrpt_req(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Avg_Err_Thr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BRang") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Con_Err_Thr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Del_MSDU_Cnt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DestAddr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Duration") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Mea_Dur") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PeerAddr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RandInt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Repeatition") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TrigRpt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TrigTimeOut") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.12
int parse_ap_set_11d(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "CountryCode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Regulatory_Mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.13
int parse_ap_set_11h(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "DFS_Chan") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DFS_Mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Regulatory_Mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.14
int parse_ap_set_11n_wireless(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "AMPDU") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Sgi20") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Spatial_Rx_Stream") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Spatial_Tx_Stream") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Width") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.15
int parse_ap_set_apqos(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "ACM_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.16
int parse_ap_set_hs2(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Accs_Net_Type") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ANQP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSS_LOAD") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Conn_Cap") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DGAF_Disable") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Domain_List") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "GAS_CB_Delay") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HESSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ICMPv4_Echo") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Interworking") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "IP_Add_Type_Avail") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "L2_Traffic_Inspect") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NAI_Realm_List") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Net_Auth_Type") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Oper_Class") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Oper_Name") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OSU _METHOD") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OSU_ICON_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OSU_PROVIDER_LIST") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OSU_SERVER_URI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OSU_SSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PLMN_MCC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PLMN_MNC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Proxy_ARP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "QoS_MAP_SET") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Roaming_Cons") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STA_MAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Venue_Name") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WAN_Metrics") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WLAN_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.17
int parse_ap_set_pmf(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PMF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.18
int parse_ap_set_radius(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "IPAddr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Password") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Port") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WLAN_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.19
int parse_ap_set_rfeature(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "ChNum_Band") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CTS_Width") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NDPA_STAinfo_MAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "nss_mcs_opt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Opt_Md_Notif_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RTS_Force") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxBandwidth") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Type") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "GI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "LTF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PPDUTxType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TriggerType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Trigger_TxBF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DisableTriggerType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACKType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AckPolicy") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AckPolicy_MAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Assoc_Disallow") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BTMReq_DisAssoc_Imnt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BTMReq_Term_Bit") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSS_Term_Duration") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSS_Term_TSF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NAV_Update") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Nebor_BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Nebor_Op_Ch") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Nebor_Op_Class") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Nebor_Pref") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RUAllocTones") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Channel_Switch_Announcement") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "channelswitchcount") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);            
        } else if (strcasecmp(str, "ReassocResp_RSNXE_ProtectedTWT") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ReassocResp_RSNXE_Used") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxPower") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TriggerCoding") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Trig_ComInfo_BW") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Trig_UsrInfo_SSAlloc_RA-RU") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Trig_ComInfo_GI-LTF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Trig_UsrInfo_RUAlloc") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "UnsolicitedProbeResp") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FILSDscv") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Cadence_UnsolicitedProbeResp") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Transition_Disable") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Transition_Disable_Index") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OCIFrameType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OCIChannel") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE_ChanWidth") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE_NSS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE_BIPNResue") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE_InvalidMIC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OMN_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CSA_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Protected") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "stationID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RTS_FORCE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.20
int parse_ap_set_rrm(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "BAE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BLE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSSTrans") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PCE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "QTE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.21
int parse_ap_set_security(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;

    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Encrypt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyMgnt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AKMSuiteType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AntiCloggingThreshold") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BeaconProtection") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ChnlFreq") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
            if (atoi(c_data->values[c_data->count -1] )> 5939){
                strcpy(c_data->interface, "6G");
                printf("Convert Frequency to interface, %s\n", c_data->interface);
            }
        } else if (strcasecmp(str, "Clear_RSNXE") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ECGroupID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FILS_KeyConfirm_Omit") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FILS_PublicKey_Omit") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyRotation") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyRotation_BIGTK_STADisassoc") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyRotationInterval") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NonTxBSSIndex") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PairwiseCipher") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "GroupCipher") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PMF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PMKSACaching") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PreAuthentication") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PSK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PSKPassPhrase") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RSNXE_Content") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "InvalidSAEElement") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "reflection") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RSNXE_ProtectedTWT") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_Confirm_Immediate") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAEPasswords") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_H2E") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_Commit_StatusCode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_KeyPair") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_KeyPairMism") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_Modifier") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_ModifierMism") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_KeyPairSigOverride") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PK_Omit") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAE_PWE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SHA256AD") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SSID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Transition_Disable") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Transition_Disable_Index") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WEPKey") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WLAN_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.22
int parse_ap_set_staqos(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "ACM_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ACM_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AIFS_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMax_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CWMin_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_VO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_VI") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_BE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxOp_BK") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

// 8.23
int parse_ap_set_wireless(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "40_Intolerant") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ADDBA_Reject") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ADDBAReq_BufSize") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ADDBAResp_BufSize") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AMPDU") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "AMSDU") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ANQP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Band6Gonly") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BA_Recv_Status") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BCC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BCNInt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BW_Sgnl") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Cellular_Cap_Pref") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ChnlFreq") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
            if (atoi(c_data->values[c_data->count -1] )> 5939){
                strcpy(c_data->interface, "6G");
                printf("Convert Frequency to interface, %s\n", c_data->interface);
            }
        } else if (strcasecmp(str, "Channel") == 0) {
            char value[10] = {0}, param[5] = {0}, *token = NULL;
            int chan = 0;

            capi_str = parse_value(c_data, str, &capi_str);
            if  (strcasecmp(c_data->interface, "6G") != 0) {
                strcpy(value,c_data->values[c_data->count -1]);
                if (NULL != strstr(value, ";")) {
                    token = strtok(value, ";");
                    strcpy(param, token);
                    chan = atoi(param);
                }
                else {
                    chan = atoi(c_data->values[c_data->count -1]);
                }

                if((chan >= 1)&&(chan <= 14)) {
                    strcpy(c_data->interface, "2G");
                } else if (chan >= 36) {
                    strcpy(c_data->interface, "5G");
                }
                printf("Convert Channel to interface, %s\n", c_data->interface);
            }
        } else if (strcasecmp(str, "ChannelUsage") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CountryCode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Domain") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DTIM") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Dyn_BW_Sgnl") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ExpBcnLength") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FILSDscv") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Frgmnt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FT_BSS_LIST") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FT_DS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FT_OA") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "GAS_CB_Delay") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "GAS_Frag_Thr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Greenfield") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HE_TXOPDurRTSThr") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HS2") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_TKIP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HTC-VHT") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "LDPC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MBSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MCS_32") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MCS_FixedRate") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MCS_Mandatory") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MCS_Supported") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MIMO") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MU_EDCA") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MU_TxBF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MU_NDPA_FrameFormat") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NEIBRPT") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NoAck") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NonTxBSSIndex") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NSS_MCS_Cap") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NumNonTxBSS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NumSoundDim") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "NumUsersOFDMA") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ocvc") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OCI_Global_Op_Class") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OFDMA") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Offset") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OMCtrl_ULMUDataDisableRx") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "P2PCoexistence") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "P2PMgmtBit") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PPDUTxType") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "PreAuthentication") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Prog") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Pwr_Const") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Radio") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Reg_Domain") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RIFS_Test") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RRM") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RTS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SGI_20") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SGI20") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SGI40") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SGI80") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Spatial_RX_Stream") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Spatial_TX_Stream") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SpectrumMgt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SSID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STBC_Rx") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STBC_TX") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TDLSChswitchProhibit") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TDLSProhibit") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TWTinfoFrameRx") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TWT_RespSupport") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TxBF") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "UnsolicitedProbeResp") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ActiveInd_UnsolicitedProbeResp") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Cadence_UnsolicitedProbeResp") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "VHT_TKIP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "VHT_WEP") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Width") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Width_Scan") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WLAN_TAG") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WME") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "WMMPS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Zero_CRC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BW_SGNL") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DYN_BW_SGNL") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_dev_configure_ie(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    //printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "IE_Name") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Contents") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_dev_exec_action(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    //printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Dest_MAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyRotation") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "KeyRotationInterval") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Rejected_DH_Groups") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_dev_send_frame(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    char *end;
    char buf_tmp[200];

    //printf("raw : %s \n",capi_str);
    for (;;) {
        end =  strstr(capi_str, ",,");
        if(end == NULL) {
            break;
        } else {
            strcpy(buf_tmp, end+1);
            end[1] = '\0';
            strcat(capi_str, "EmptyData");
            strcat(capi_str, buf_tmp);
            printf("Fill up empty field: %s \n",capi_str);
        }
    }
    
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element
    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Dest_MAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "DestMAC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "FrameName") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ESS_DISASSOC_IMM") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Disassoc_Timer") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SESS_INFO_URL") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Cand_List") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RegClass") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);            
        } else if (strcasecmp(str, "Channel") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);            
        } else if (strcasecmp(str, "channelwidth") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);            
        } else if (strcasecmp(str, "channelswitchcount") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);            
        } else if (strcasecmp(str, "nss") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RandInt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaDur") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaMode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "BSSID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RptCond") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SSID") == 0) {
            capi_str = parse_value_no_strip(c_data, str, &capi_str);
        } else if (strcasecmp(str, "RptDet") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MeaDurMand") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "APChanRpt") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "LastBeaconRptIndication") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ReqInfo") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "TXOPDur") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "interval") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "source") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE_ChanWidth") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "HT_Opt_IE_NSS") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE_BIPNResue") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "MMIC_IE_InvalidMIC") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OMN_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "CSA_IE") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Protected") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "stationID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "ChannelSwitchAnncment") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "OCIChannel") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "SAQueryReq") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "mode") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_ap_get_parameter(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    //printf("raw : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "STA_MAC_Address") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Parameter") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_traffic_send_ping(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    //printf("Do nothing for : %s \n",capi_str);
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "destination") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "framesize") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "frameRate") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "duration") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        } else if (strcasecmp(str, "iptype") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_traffic_stop_ping(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        } else if (strcasecmp(str, "streamID") == 0) {
            capi_str = parse_value(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_traffic_agent_reset(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

int parse_ignore_capi(char *capi_str, uint8_t *ret_buf, int *ret_len)
{
    capi_data_t* c_data = (capi_data_t*) ret_buf;
    if (buf_check(ret_buf, ret_len) == FALSE) return FALSE;
    char *str; // token for iter throug the element

    for (;;) {
        str = strtok_r(NULL, ",", &capi_str);
        if (str == NULL || str[0] == '\0')
            break;
        if (strcasecmp(str, "Interface") == 0) {
            capi_str = parse_interface(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Name") == 0) {
            capi_str = parse_name(c_data, str, &capi_str);
        } else if (strcasecmp(str, "Program") == 0) {
            capi_str = parse_program(c_data, str, &capi_str);
        }
    }
    printf("total count:%d\n", c_data->count);
    return WFA_SUCCESS;
}

