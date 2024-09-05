//
// Created by boalin on 7/26/17.
//

#ifndef MTK_DUT_MTK_RESP_H
#define MTK_DUT_MTK_RESP_H


typedef unsigned char uint8_t;

// mtk CAPI part

int mtk_device_get_info_resp (uint8_t* resp_buf, retType_t status);
int mtk_ap_ca_version_resp (uint8_t* resp_buf, retType_t status);
int mtk_ap_config_commit_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_deauth_sta_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_get_mac_address_resp(uint8_t* resp_buf, retType_t status);
int mtk_ap_reset_default_resp(uint8_t* resp_buf, retType_t status);
int mtk_ap_send_addba_req_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_send_bcnrpt_req_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_send_bsstrans_mgmt_req_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_send_link_mea_req_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_send_tsmrpt_req_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_11d_resp(uint8_t* resp_buf, retType_t status);
int mtk_ap_set_11h_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_11n_wireless_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_apqos_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_hs2_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_pmf_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_radius_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_rfeature_resp(uint8_t *resp_buf, retType_t status);
int mtk_ap_set_rrm_resp(uint8_t* resp_buf, retType_t status);
int mtk_ap_set_security_resp(uint8_t* resp_buf, retType_t status);
int mtk_ap_set_staqos_resp(uint8_t * resp_buf, retType_t status);
int mtk_ap_set_wireless_resp(uint8_t * resp_buf, retType_t status);
int mtk_dev_configure_ie_resp(uint8_t * resp_buf, retType_t status);
int mtk_dev_exec_action_resp(uint8_t * resp_buf, retType_t status);
int mtk_dev_send_frame_resp(uint8_t * resp_buf, retType_t status);
int mtk_ap_get_parameter_resp(uint8_t* resp_buf, retType_t status);
int mtk_traffic_send_ping_resp(uint8_t* resp_buf, retType_t status);
int mtk_traffic_stop_ping_resp(uint8_t* resp_buf, retType_t status);
int mtk_traffic_agent_reset_resp(uint8_t* resp_buf, retType_t status);
int mtk_ignore_capi_resp(uint8_t* resp_buf, retType_t status);
#endif //MTK_DUT_MTK_RESP_H
