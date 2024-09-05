//
// Created by boalin on 7/25/17.
//

#ifndef MTK_DUT_MTK_PARSE_H
#define MTK_DUT_MTK_PARSE_H
#include <ctype.h>

typedef unsigned char uint8_t;

typedef struct capi_data {
    char name[64];
    char program[64];
    char interface[64];
    char *params[42];
    char *values[42];
    int count;
    int cmd_tag;
} capi_data_t;

void strip_char(char *str, char strip);
int parse_device_get_info(char *, uint8_t *, int *);
int parse_ap_ca_version(char *, uint8_t *, int *);
int parse_ap_config_commit(char *, uint8_t *, int *);
int parse_ap_deauth_sta(char *, uint8_t *, int *);
int parse_ap_get_mac_address(char *, uint8_t *, int *);
int parse_ap_reset_default(char *, uint8_t *, int *);

int parse_ap_send_addba_req(char *, uint8_t *, int *);
int parse_ap_send_bcnrpt_req(char *, uint8_t *, int *);
int parse_ap_send_bsstrans_mgmt_req(char *,  uint8_t *, int *);
int parse_ap_send_link_mea_req(char *, uint8_t *, int *);
int parse_ap_send_tsmrpt_req(char *, uint8_t *, int *);

int parse_ap_set_11d(char *, uint8_t *, int *);
int parse_ap_set_11h(char *, uint8_t *, int *);
int parse_ap_set_11n_wireless(char *, uint8_t *, int *);
int parse_ap_set_apqos(char *, uint8_t *, int *);
int parse_ap_set_hs2(char *, uint8_t *, int *);

int parse_ap_set_pmf(char *, uint8_t *, int *);
int parse_ap_set_radius(char *, uint8_t *, int *);
int parse_ap_set_rfeature(char *, uint8_t *, int *);
int parse_ap_set_rrm(char *, uint8_t *, int *);
int parse_ap_set_security(char *, uint8_t *, int *);

int parse_ap_set_staqos(char *, uint8_t *aBuf, int *aLen);
int parse_ap_set_wireless(char *, uint8_t *aBuf, int *aLen);

int parse_dev_configure_ie(char *, uint8_t *aBuf, int *aLen);
int parse_dev_exec_action(char *, uint8_t *aBuf, int *aLen);
int parse_dev_send_frame(char *, uint8_t *aBuf, int *aLen);
int parse_ap_get_parameter(char *, uint8_t *aBuf, int *aLen);
int parse_traffic_send_ping(char *, uint8_t *aBuf, int *aLen);
int parse_traffic_stop_ping(char *, uint8_t *aBuf, int *aLen);
int parse_traffic_agent_reset(char *, uint8_t *aBuf, int *aLen);
int parse_ignore_capi(char *, uint8_t *aBuf, int *aLen);
#endif //MTK_DUT_MTK_PARSE_H
