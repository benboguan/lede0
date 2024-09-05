//
// Created by boalin on 7/26/17.
//
#include <stdio.h>
#include <string.h>
#include "wfa_types.h"
#include "mtk_resp.h"

extern char Device_Ver_CAPI[];
extern const char *CA_Ver;

int mtk_device_get_info_resp (uint8_t* resp_buf, retType_t status)
{
    printf("enter mtk_device_get_info_resp\n");
    sprintf((char*) resp_buf, "status,COMPLETE,vendor,%s,model,%s,version,%s\r\n", "Mediatek", "MT79AXAP", Device_Ver_CAPI);

    return 0;
}

int mtk_ap_ca_version_resp (uint8_t* resp_buf, retType_t status)
{
    printf("enter mtk_ap_ca_version_resp\n");
    sprintf((char*) resp_buf, "status,COMPLETE,version,%s\r\n", CA_Ver);

    return 0;
}


int mtk_ap_config_commit_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}


int mtk_ap_deauth_sta_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_get_mac_address_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE,mac,%s\r\n", tmp_buf);
    return 0;
}


int mtk_ap_reset_default_resp(uint8_t* resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}


int mtk_ap_send_addba_req_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_send_bcnrpt_req_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_send_bsstrans_mgmt_req_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_send_link_mea_req_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_send_tsmrpt_req_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_set_11d_resp(uint8_t* resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}


int mtk_ap_set_11h_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_set_11n_wireless_resp(uint8_t *resp_buf, retType_t status)

{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_set_apqos_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_set_hs2_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}


int mtk_ap_set_pmf_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}


int mtk_ap_set_radius_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,RADIUS Info Added\r\n");
    return 0;
}


int mtk_ap_set_rfeature_resp(uint8_t *resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}
int mtk_ap_set_rrm_resp(uint8_t* resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}
int mtk_ap_set_security_resp(uint8_t* resp_buf, retType_t status)
{
    if (status == WFA_SUCCESS)
        sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    else
        sprintf((char*) resp_buf, "status,Requested configuration is not supported.\r\n");
    return 0;
}
int mtk_ap_set_staqos_resp(uint8_t * resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}
int mtk_ap_set_wireless_resp(uint8_t * resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE,Success\r\n");
    return 0;
}

int mtk_dev_configure_ie_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE%s\r\n", tmp_buf);
    return 0;
}

int mtk_dev_exec_action_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE%s\r\n", tmp_buf);
    return 0;
}

int mtk_dev_send_frame_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    sprintf((char*) resp_buf, "status,COMPLETE,0\r\n");
    return 0;
}

int mtk_ap_get_parameter_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE,%s\r\n", tmp_buf);
    return 0;
}

int mtk_traffic_send_ping_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE,streamID,%s\r\n", tmp_buf);
    return 0;
}

int mtk_traffic_stop_ping_resp(uint8_t* resp_buf, retType_t status)
{
    char tmp_buf[32];
    strcpy(tmp_buf, (char*) resp_buf);
    sprintf((char*) resp_buf, "status,COMPLETE,%s\r\n", tmp_buf);
    return 0;
}

int mtk_traffic_agent_reset_resp(uint8_t* resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}

int mtk_ignore_capi_resp(uint8_t* resp_buf, retType_t status)
{
    sprintf((char*) resp_buf, "status,COMPLETE\r\n");
    return 0;
}
