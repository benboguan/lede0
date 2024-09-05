//
// Created by boalin on 7/25/17.
//

#ifndef MTK_DUT_MTK_AP_H
#define MTK_DUT_MTK_AP_H
#include "mtk_parse.h"
#include "mtk_dict.h"
#include <wfa_tlv.h>
#include "mtk_resp.h"
#include <ctype.h>

#define PROC_NET_WIRELESS	"/proc/net/wireless"
#define PROFILE_INF "/etc/wireless/l1profile.dat"
#define INTF_NUM 2
#define POST_CMD_NUM 10

#define ADD_POST_CMD(gCmdStr)     if (mtk_ap_buf->post_cmd_idx >= POST_CMD_NUM) { \
        printf("Post CMD Number exceed %d, reset to 0\n", POST_CMD_NUM); \
            mtk_ap_buf->post_cmd_idx = 0;} \
        strcpy(mtk_ap_buf->post_commit_cmd[mtk_ap_buf->post_cmd_idx++], gCmdStr);

typedef unsigned char uint8_t;

typedef enum wifi_mode {
    WIFI_2G,
    WIFI_5G,
    WIFI_6G,
    NORMAL
} wifi_mode;

typedef enum device_type {
    DUT,
    TESTBED,
    UNKOWN
} device_type;

typedef struct intf_profile {
     char name[16]; //interface name ra0, rax0, etc
     char profile[60]; //profile directory and name
     char profile_bak[60]; //original profile
     char profile_cmt[60]; //profile for last committed
     char sigma_dut_profile[60];
     char sigma_tb_profile[60];
} intf_profile_t;

typedef struct intf_desc {
    char name[16];
    wifi_mode mode;
    dict_t dict_table;
    intf_profile_t* profile_names;
    int status;
    int mbss_en;
    int bss_idx;
    int bss_num;
    int WLAN_TAG[5];
    int WLAN_TAG_bss_num;
    int security_set;
    int UL_MUMIMO;
    int DL;
    char PMF_MFPC[15];
    char PMF_MFPR[15];
    char PWDIDR[15];
    char AuthModeBSSID[5][20];
    char EncryptBSSID[5][10];
    char SSID[32];
} intf_desc_t;

typedef struct cmdline_cfg {
    char program[20];
    char mode[20];
    int intf_rst_delay;
    int post_intf_rst_delay;
} cmdline_cfg_t;

typedef struct internal_flag {
    int commited;
    int BW_5G_set;
    int vie_op;
    int capi_dual_pf;
} internal_flag_t;

typedef struct band_6G_only {
    int intf_6G_only;
    int intf_2G_orig_stat;
    int intf_5G_orig_stat;
} band_6G_only_t;

typedef struct WLAN_TAG {
    int TAG[3];
    int TxBSS;
} WLAN_TAG_t;

typedef struct mtk_ap_buf {
    cmdline_cfg_t cmd_cfg;
    internal_flag_t intern_flag;
    char lan_IPaddr[20];
    device_type dev_type;
    intf_profile_t profile_names[INTF_NUM];
    int tb_profile_exist;
    int ioctl_sock; /* socket for ioctl() use */
    wifi_mode def_mode;
    capi_data_t* capi_data;
    dict_t commit_dict;
    dict_t key_translation_table;
    int WLAN_TAG;
    intf_desc_t intf_2G, intf_5G, intf_6G;
    intf_desc_t* WLAN_TAG_inf[3];
    WLAN_TAG_t WLAN_TAG_2G, WLAN_TAG_5G, WLAN_TAG_6G;
    intf_desc_t* def_intf;
    int post_cmd_idx;
    char post_commit_cmd[POST_CMD_NUM][120];
    int WappEnable;
    char Reg_Domain[20];
    int DisAssoc_Imnt;
    band_6G_only_t Band6Gonly;
} mtk_ap_buf_t;

// define ap CAPI function
typedef int (*cmd_parse_ptr)(char *, uint8_t *, int *);
typedef int (*cmd_mtk_ptr) (int , uint8_t *, int *, uint8_t *);
typedef int (*cmd_resp_ptr)(uint8_t *, retType_t);


typedef struct type_dut_table
{
    int type;
    char name[32];
    cmd_parse_ptr cmd_parse;
    cmd_mtk_ptr cmd_mtk;
    cmd_resp_ptr cmd_resp;
} typeDUT_t;

typedef struct capi_to_profile_table
{
    char capi_name[64];
    char dat_name[64];
} capi_profile_t;

typedef int (*iw_enum_handler)(int	skfd,
			       char *	ifname,
			       char *	args,
			       int	count);

int device_get_ver();
int init_profile_name(mtk_ap_buf_t*);
int is_wifi_interface_exist(int, const char*);
int is_interface_up(int, const char*);
int wifi_interface_chan(int, const char*);
char * wifi_interface_ssid(int, const char*);
void turn_all_interface_down_up(mtk_ap_buf_t*);
int check_turn_interface_down(int, char *, void * , int);
int fillup_intf(int, char *, void * , int);
int wifi_enum_devices(int, iw_enum_handler, void *, int);
int ap_init(mtk_ap_buf_t*);
int mtk_ap_exec(mtk_ap_buf_t*, capi_data_t*, uint8_t*, int, int*, int );

// MTK DUT execution
int mtk_device_get_info(int , uint8_t *, int *, uint8_t *);
int mtk_ap_ca_version(int , uint8_t *, int *, uint8_t *);
int mtk_ap_config_commit(int , uint8_t *, int *, uint8_t *);
int mtk_ap_deauth_sta(int , uint8_t *, int *, uint8_t *);
int mtk_ap_get_mac_address(int , uint8_t *, int *, uint8_t *);
int mtk_ap_reset_default(int , uint8_t *, int *, uint8_t *);

int mtk_ap_send_addba_req(int , uint8_t *, int *, uint8_t *);
int mtk_ap_send_bcnrpt_req(int , uint8_t *, int *, uint8_t *);
int mtk_ap_send_bsstrans_mgmt_req(int , uint8_t *, int *, uint8_t *);
int mtk_ap_send_link_mea_req(int , uint8_t *, int *, uint8_t *);
int mtk_ap_send_tsmrpt_req(int , uint8_t *, int *, uint8_t *);

int mtk_ap_set_11d(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_11h(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_11n_wireless(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_apqos(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_hs2(int , uint8_t *, int *, uint8_t *);

int mtk_ap_set_pmf(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_radius(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_rfeature(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_rrm(int , uint8_t *, int *, uint8_t *);
int mtk_ap_set_security(int , uint8_t *, int *, uint8_t *);

int mtk_ap_set_staqos(int, uint8_t *, int *, uint8_t *);
int mtk_ap_set_wireless(int, uint8_t *, int *, uint8_t *);
int mtk_dev_configure_ie(int, uint8_t *, int *, uint8_t *);
int mtk_dev_exec_action(int, uint8_t *, int *, uint8_t *);
int mtk_dev_send_frame(int, uint8_t *, int *, uint8_t *);
int mtk_ap_get_parameter(int, uint8_t *, int *, uint8_t *);
int mtk_traffic_send_ping(int, uint8_t *, int *, uint8_t *);
int mtk_traffic_stop_ping(int, uint8_t *, int *, uint8_t *);
int mtk_traffic_agent_reset(int, uint8_t *, int *, uint8_t *);
int mtk_ignore_capi(int, uint8_t *, int *, uint8_t *);

#define RT_PRIV_IOCTL       (SIOCIWFIRSTPRIV + 0x01)
#define OID_DUT_GET_REJECTED_GROUP  0x1100

// define table for mapping
static  typeDUT_t dut_tbl[] =
        {
                {0,                   "NO_USED_STRING", NULL, NULL, NULL},
                {WFA_GET_VERSION_TLV, "ca_get_version", parse_ap_ca_version, mtk_ap_ca_version, mtk_ap_ca_version_resp},
                {WFA_DEVICE_GET_INFO_TLV, "device_get_info", parse_device_get_info, mtk_device_get_info, mtk_device_get_info_resp},
                {WFA_AP_CA_VERSION_TLV, "ap_ca_version", parse_ap_ca_version, mtk_ap_ca_version, mtk_ap_ca_version_resp},
                {WFA_AP_CONFIG_COMMIT_TLV, "ap_config_commit", parse_ap_config_commit, mtk_ap_config_commit, mtk_ap_config_commit_resp},
                {WFA_AP_DEAUTH_STA_TLV, "ap_deauth_sta", parse_ap_deauth_sta, mtk_ap_deauth_sta, mtk_ap_deauth_sta_resp},
                {WFA_AP_GET_MAC_ADDRESS_TLV, "ap_get_mac_address", parse_ap_get_mac_address, mtk_ap_get_mac_address, mtk_ap_get_mac_address_resp},
                {WFA_AP_RESET_DEFAULT_TLV, "ap_reset_default", parse_ap_reset_default, mtk_ap_reset_default, mtk_ap_reset_default_resp},

                {WFA_AP_SEND_ADDBA_REQ_TLV, "ap_send_addba_req", parse_ap_send_addba_req, mtk_ap_send_addba_req, mtk_ap_send_addba_req_resp},
                {WFA_AP_SEND_BCNRPT_REQ_TLV, "ap_send_bcnrpt_req", parse_ap_send_bcnrpt_req, mtk_ap_send_bcnrpt_req, mtk_ap_send_bcnrpt_req_resp},
                {WFA_AP_SEND_BSSTRANS_MGMT_REQ_TLV, "ap_send_bsstrans_mgmt_req", parse_ap_send_bsstrans_mgmt_req, mtk_ap_send_bsstrans_mgmt_req, mtk_ap_send_bsstrans_mgmt_req_resp},
                {WFA_AP_SEND_LINK_MEA_REQ_TLV, "ap_send_link_mea_req", parse_ap_send_link_mea_req, mtk_ap_send_link_mea_req, mtk_ap_send_link_mea_req_resp},
                {WFA_AP_SEND_TSMRPT_REQ_TLV, "ap_send_tsmrpt_req", parse_ap_send_tsmrpt_req, mtk_ap_send_tsmrpt_req, mtk_ap_send_tsmrpt_req_resp},

                {WFA_AP_SET_11D_TLV, "ap_set_11d", parse_ap_set_11d, mtk_ap_set_11d, mtk_ap_set_11d_resp},
                {WFA_AP_SET_11H_TLV, "ap_set_11h", parse_ap_set_11h, mtk_ap_set_11h, mtk_ap_set_11h_resp},
                //{WFA_AP_SET_11N_WIRELESS_TLV, "ap_set_11n_wireless", parse_ap_set_11n_wireless, mtk_ap_set_11n_wireless, mtk_ap_set_11n_wireless_resp},
                {WFA_AP_SET_11N_WIRELESS_TLV, "ap_set_11n_wireless", parse_ap_set_wireless, mtk_ap_set_wireless, mtk_ap_set_wireless_resp},
                {WFA_AP_SET_APQOS_TLV, "ap_set_apqos", parse_ap_set_apqos, mtk_ap_set_apqos, mtk_ap_set_apqos_resp},
                {WFA_AP_SET_HS2_TLV, "ap_set_hs2", parse_ap_set_hs2, mtk_ap_set_hs2, mtk_ap_set_hs2_resp},

                {WFA_AP_SET_PMF_TLV, "ap_set_pmf", parse_ap_set_pmf, mtk_ap_set_pmf, mtk_ap_set_pmf_resp},
                {WFA_AP_SET_RADIUS_TLV, "ap_set_radius", parse_ap_set_radius, mtk_ap_set_radius, mtk_ap_set_radius_resp},
                {WFA_AP_SET_RFEATURE_TLV, "ap_set_rfeature", parse_ap_set_rfeature, mtk_ap_set_rfeature, mtk_ap_set_rfeature_resp},
                {WFA_AP_SET_RRM_TLV, "ap_set_rrm", parse_ap_set_rrm, mtk_ap_set_rrm, mtk_ap_set_rrm_resp},
                {WFA_AP_SET_SECURITY_TLV, "ap_set_security", parse_ap_set_security, mtk_ap_set_security, mtk_ap_set_security_resp},

                {WFA_AP_SET_STAQOS_TLV, "ap_set_staqos", parse_ap_set_staqos, mtk_ap_set_staqos, mtk_ap_set_staqos_resp},
                {WFA_AP_SET_WIRELESS_TLV, "ap_set_wireless", parse_ap_set_wireless, mtk_ap_set_wireless, mtk_ap_set_wireless_resp},

                {WFA_DEV_CONFIGURE_IE_TLV, "dev_configure_ie", parse_dev_configure_ie, mtk_dev_configure_ie, mtk_dev_configure_ie_resp},
                {WFA_DEV_EXEC_ACTION_TLV, "dev_exec_action", parse_dev_exec_action, mtk_dev_exec_action, mtk_dev_exec_action_resp},
                {WFA_DEV_SEND_FRAME_TLV, "dev_send_frame", parse_dev_send_frame, mtk_dev_send_frame, mtk_dev_send_frame_resp},
                {WFA_AP_GET_PARAMETER_TLV, "ap_get_parameter", parse_ap_get_parameter, mtk_ap_get_parameter, mtk_ap_get_parameter_resp},

                {WFA_TRAFFIC_SEND_PING_TLV, "traffic_send_ping", parse_traffic_send_ping, mtk_traffic_send_ping, mtk_traffic_send_ping_resp},
                {WFA_TRAFFIC_STOP_PING_TLV, "traffic_stop_ping", parse_traffic_stop_ping, mtk_traffic_stop_ping, mtk_traffic_stop_ping_resp},
                {WFA_TRAFFIC_AGENT_RESET_TLV, "traffic_agent_reset", parse_traffic_agent_reset, mtk_traffic_agent_reset, mtk_traffic_agent_reset_resp},

                {WFA_AP_IGNORE_CAPI_TLV, "AccessPoint", parse_ap_reset_default, mtk_ap_reset_default, mtk_ap_reset_default_resp},

                {-1, "", NULL, NULL, NULL},
        };

static capi_profile_t capi_key_tbl[] =
        {
                {"BssidNum",             "BssidNum="},
                {"SSID",                     "SSID1="},
                {"SSID2",                   "SSID2="},
                {"SSID3",                   "SSID3="},
                {"SSID4",                   "SSID4="},
                {"SSID5",                   "SSID5="},
                {"Channel",                "Channel="},
                {"set40MHZ",             "HtBw="},
                {"Band",                    "WirelessMode="},
                {"FragThr",                "FragThreshold="},
                {"BcnInt",                  "BeaconPeriod="},
                {"DTIM",                    "DtimPeriod="},
                {"RTSThr",                 "RTSThreshold="},
                {"ShortGI20",             "HT_GI="},
                {"WME",                     "WmmCapable="},
                {"WMMPS",                 "APSDCapable="},
                {"MODE",                   "WirelessMode="},
                {"E2pAccessMode",     "E2pAccessMode="},
                /* Security related keys */
                {"AuthMode",               "AuthMode="},
                {"Encrypt",                "EncrypType="},
                {"WdsEncrypType",          "WdsEncrypType="},
                {"IEEE1x",                 "IEEE8021X="},
                {"IEEE_80211H",            "IEEE80211H="},
                {"ocvc",                            "OCVSupport="},
                {"OCI_Global_Op_Class",  "OpClass="},
                {"PSK",                    "WPAPSK1="},
                {"PSK2",                    "WPAPSK2="},
                {"PSK3",                    "WPAPSK3="},
                {"PSK4",                    "WPAPSK4="},
                {"PSK5",                    "WPAPSK5="},
                {"WEPKey",                 "Key1Str1="},
                {"PreAuth",                "PreAuth="},
                {"PSKPassword",             "WPAPSK1="},
                {"PWDID",                      "PWDID1="},
                {"PWDIDR",                    "PWDIDR="},
                {"PweMethod",                "PweMethod="},
                {"SAE_PK",                     "SAEPK="},
                {"SAE_PK_KeyPair",        "SAEPKKey1="},
                {"SAE_PK_Modifier",        "SAEPKStartM1="},
                {"SAE_PKCfg",                 "SAEPKCfg="},
                {"SAE_PKGroup",             "SAEPKGroup="},
                {"TestbedMode",              "TestbedMode="},
                {"Transition_Disable",       "TransitionDisable="},
                /* WMM related keys */
                {"CWMin_1",                "APCwmin="},
                {"CWMax_1",                "APCwmax="},
                {"AIFS_1",                 "APAifsn="},
                {"TXOP_1",                 "APTxop="},
                {"TXOP_0",                 "BSSTxop="},
                {"ACM_1",                  "APACM="},
                {"CWMin_0",                "BSSCwmin="},
                {"CWMax_0",                "BSSCwmax="},
                {"AIFS_0",                 "BSSAifsn="},
                {"ACM_0",                  "BSSACM="},
                {"NoAck",                  "AckPolicy="},
                {"HT_OpMode",              "HT_OpMode="},
                {"40_INTOLERANT",          "HT_40MHZ_INTOLERANT="},
                {"Reg_Domain",              "RegDomain="},
                {"MBO",                         "MboSupport="},
                {"RRM",                     "RRMEnable="},
                {"FT_OA",                  "FtSupport="},
                {"FT_DS",                  "FtOtd="},
                {"FT_ONLY",                  "FtOnly="},
                {"DOMAIN",                 "FtMdId1="},
                {"QTE",                    "RRMQUIETEnable="},
                {"BSSTrans",               "WNMBSSEnable="},
                {"ADDBA_REJECT",           "HT_BADecline="},
                {"AMPDU",                  "HT_AutoBA="},
                {"AMPDU_EXP",              "HT_AMPDU_EXP="},
                {"AMSDU",                  "HT_AMSDU="},
                {"FORCE_AMSDU",            "FORCE_AMSDU="},
                {"ForceGF",                "ForceGF="},
                {"MPDU_MIN_START_SPACING", "HT_MpduDensity="},
                {"ExChannelOffset",        "HT_EXTCHA="},
                {"SupportedChannelWidth",  "HT_BW="},
                {"RADIUS_Server",          "RADIUS_Server="},
                {"RADIUS_Port",            "RADIUS_Port="},
                {"RADIUS_Key1",            "RADIUS_Key1="},
                {"OWN_IP_ADDR",            "own_ip_addr="},
                {"width",                  "HT_BW="},
                {"widthScan",              "OBSSScanParam="},
                {"ShortGI20",              "HT_GI="},
                {"ShortGI40",              "HT_GI="},
                {"HT_BW",                  "HT_BW="},
                {"VHT_BW",                 "VHT_BW="},
                {"HT_DisallowTKIP",        "HT_DisallowTKIP="},
                {"CountryCode",              "CountryCode="},
                {"CountryRegion",            "CountryRegion="},
                {"CountryRegionABand",    "CountryRegionABand="},
                {"ShortGI40",              "ForceShortGI="},
                {"STBC",                   "HT_STBC="},
                {"VHT_STBC",                   "VHT_STBC="},
                {"FORCE_STBC",                   "ForceSTBC="},
                {"HT_MCS",                 "HT_MCS="},
                {"SPATIAL_TX_STREAM",           "HT_TxStream="},
                {"SPATIAL_RX_STREAM",           "HT_RxStream="},
                {"PMF_MFPC",               "PMFMFPC="},
                {"PMF_MFPR",               "PMFMFPR="},
                {"PMF_SHA256",             "PMFSHA256="},
                {"RADIO",                  "RadioOn="},
                {"HT_LDPC",                "HT_LDPC="},
                {"VHT_LDPC",               "VHT_LDPC="},
                {"HE_LDPC",                "HeLdpc="},
                {"PPDUTxType",            "PPDUTxType="},
                {"OFDMA",                    "OFDMA="},
                {"MuOfdmaDlEnable",    "MuOfdmaDlEnable="},
                {"MuOfdmaUlEnable",    "MuOfdmaUlEnable="},
                {"NumUsersOFDMA",     "NumUsersOFDMA="},
                {"MCS_FixedRate",        "FixedMcs="},
                {"HT_BAWinSize",          "HT_BAWinSize="},
                {"TXOPDurRTSThr",        "HE_TXOP_RTS_THLD="},
                {"DOT11V_MBSSID",      "Dot11vMbssid="},
                {"MuEdcaOverride",        "MuEdcaOverride="},
                {"MIMO_DL",                   "MuMimoDlEnable="},
                {"MIMO_UL",                   "MuMimoUlEnable="},
                {"MU_TxBF",                   "ETxBfEnCond="},
                {"UnsolicitedProbeResp",  "He6gIobType="},
                {"FILSDscv",                    "He6gIobType="},
                {"Cadence_UnsolicitedProbeResp", "He6gIobTu="},
                {"ActiveInd_UnsolicitedProbeResp", "He6gOob="},
                {"BeaconProtection",        "BcnProt="},
                {"KeyRotation",                "RekeyMethod="},
                {"KeyRotationInterval",     "RekeyInterval="},
                {"TXBF",                           "ETxBfEnCond="},
                {"BFBACKOFFenable",       "BFBACKOFFenable="},
                {"BfSmthIntlBbypass",       "BfSmthIntlBbypass="},
                {"ITxBfEn",                        "ITxBfEn="},
                {"TWTinfoFrameRx",          "TWTInfoFrame="},
                {"TWT_RespSupport",        "TWTSupport="},
                {"VHT_BW_SIGNAL",             "VHT_BW_SIGNAL="},
                {"RTSThreshold",               "RTSThreshold="},
       };

static str_to_str_tbl_t DisEn_01_tbl[] =
{
        {"disable",      "0"},
        {"Disabled",    "0"},
        {"Off",            "0"},
        {"enable",       "1"},
        {"Enabled",     "1"},
        {"On",            "1"},
        0
};

static str_to_str_tbl_t E2pAccessMode_tbl[] =
{
        {"efuse", "1"},
        {"flash", "2"},
        0
};

static str_to_str_tbl_t TxBandwidth_tbl[] =
{
        {"80", "2"},
        {"40", "1"},
        {"20", "0"},
        0
};

static str_to_str_tbl_t GI_tbl[] =
{
        {"0.8", "0"},
        {"1.6", "1"},
        {"3.2", "2"},
        0
};

static str_to_str_tbl_t ExtChStr_tbl[] =
{
        {"above", "1"},
        {"below", "3"},
        0
};


static str_to_str_tbl_t LTF_tbl[] =
{
        {"3.2", "0"},
        {"6.4", "1"},
        {"12.8", "2"},
        0
};

static str_to_str_tbl_t PPDUTxType_tbl[] =
{
        {"SU", "0"},
        {"MU", "1"},
        {"ER", "2"},
        {"TB", "3"},
        {"Legacy", "4"},
        0
};

static str_to_str_tbl_t mode_tbl[] =
{
        {"11ax_2g", "16"},
        {"11ax_5g", "17"},
        {"11ax_6g", "18"},
        {"11ac", "14"},
        {"11ng", "9"},
        {"11na", "8"},
        {"11n", "3"},
        {"11a", "2"},
        {"11b", "1"},
        {"11g", "0"},
        0
};

static str_to_str_tbl_t mode_HTOp_tbl[] =
{
        {"11ax_2g", "0"},
        {"11ax_5g", "0"},
        {"11ax_6g", "0"},
        {"11ac", "0"},
        {"11ng", "1"},
        {"11na", "1"},
        {"11n", "1"},
        {"11a", "0"},
        {"11b", "0"},
        {"11g", "0"},
        0
};

static str_to_str_tbl_t SPATIAL_STREAM_tbl[] =
{
        {"1SS", "1"},
        {"2SS", "2"},
        {"3SS", "3"},
        {"4SS", "4"},
        0
};

static str_to_str_tbl_t width_HTBW_tbl[] =
{
        {"160", "1"},
        {"80", "1"},
        {"40", "1"},
        {"20", "0"},
        0
};

static str_to_str_tbl_t width_VHTBW_tbl[] =
{
        {"160", "2"},
        {"80", "1"},
        {"40", "0"},
        {"20", "0"},
        0
};

static str_to_str_tbl_t ofdma_dir_tbl[] =
{
        {"AUTO", "0"},
        {"DL", "1"},
        {"UL", "2"},
        {"DL-20and80", "3"},
        0
};

static str_to_str_tbl_t PweMethod_tbl[] =
{
        {"looping", "1"},
        {"h2e", "2"},
        0
};

static str_to_str_tbl_t WappCmd[] =
{
        {"Cellular_Cap_Pref", "mbo ap_cdcp"},
        {"BTMReq_DisAssoc_Imnt", "mbo disassoc_imnt"},
        {"BTMReq_Term_Bit", "mbo bss_term_onoff"},
        {"BSS_Term_Duration", "mbo bss_term_duration"},
        {"BSS_Term_TSF", "mbo bss_term_tsf"},
        {"Assoc_Disallow", "mbo assoc_disallow"},
        {"Nebor_BSSID", "mbo nebor_bssid"},
        {"Nebor_Op_Class", "mbo nebor_op_class"},
        {"Nebor_Op_Ch", "mbo nebor_op_ch"},
        {"Disassoc_Timer", "mbo disassoc_timer"},
        {"Assoc_Delay", "mbo retry_delay"},
        {"Nebor_Pref", "mbo nebor_pref"},
        {"Nebor_Test", "mbo add_test_nr"},
        {"send_BTMReq", "mbo send_btm_req"},
        {"Gas_CB_Delay", "set gas_cb_delay"},
        {"Mpdu_Size", "set mmpdu_size"},
        {"Command", ""},
        0
};

static str_to_str_tbl_t AKM_keymgnt_tbl[] =
{
        {"1", "WPA2-ENT"},
        {"2", "WPA2-PSK"},
        {"3", "WPA2-ENT"},
        {"4", "WPA2-PSK"},
        {"5", "WPA2-ENT"},
        {"6", "WPA2-PSK"},
        {"8", "SAE"},
        {"9", "SAE"},
        {"2;8", "WPA2-PSK-SAE"},
        {"2;4;8;9", "WPA2-PSK-SAE"},
        {"2;4;6;8;9", "WPA2-PSK-SAE-Mixed"},
        {"1;3", "WPA2"},
        {"1;3;5", "WPA2-MIX"},
        {"3;5", "WPA2-ENT"},
        {"8;9", "SAE"},
        0
};

static str_to_str_tbl_t TonePlan_Idx[] =
{
        {"26:26:26:26",         "0:0:0:0:0"},
        {"52:52:52:52",         "15:0:0:0:0"},
        {"106:106",                "96:0:0:0:0"},
        {"106:106:106:106", "96:96:0:0:0"},
        {"242:242:242:242", "192:192:192:192:0"},
        {"484:484",                "200:114:200:114:0"},
        {"484:484:484:484", "200:114:200:114:0:200:114:200:114:0"},
        {"996:996",                "208:115:115:115:0:208:115:115:115:0"},
        0
};


static str_to_str_tbl_t RuAlloc_Idx[] =
{
        {"26", "0"},
        {"52", "37"},
        {"106", "53"},
        {"242", "61"},
        {"484", "65"},
        {"996", "67"},
        0
};

static str_to_str_long_tbl_t SAE_PK_KeyPair_tbl[] =
{
        {"saepk1.pem", "334d0edb6375098b572af50cead689aa5b32dd792c39f886730b217ac12c037e"},
        {"saepk2.pem", "5e0b1a4137d157487897b56553a8dd39a4bd3f51291c77d3ec8ec7ff6785063d"},
        {"saepkP256.pem", "02481a57e7b5ea270dbeeb4323ff486423c33ad90277eb5c89b8dbcd1559a8d8"},
        {"saepkP384.pem", "7b88ca11dac7b4b16ae25d8084b222ddf161446e5c56942a9be640fb26065f4238f2946c2a3a0204ec94a138e8c4d19d"},
        {"saepkP521.pem", "01b8d2929ce418e5955d0605a25dc2f51abfd6f950b0ebd9ceb5ed604f8b2a755795de0ac29791c9c688151b62c576d9c40cb26bb3402d16814df675a722e32ef995"},
        0
};

static str_to_str_tbl_t SAE_PK_Group_tbl[] =
{
        {"saepk1.pem", "19"},
        {"saepk2.pem", "19"},
        {"saepkP256.pem", "19"},
        {"saepkP384.pem", "20"},
        {"saepkP521.pem", "21"},
        0
};

/*
 * about dictionary and IO for AP mode
 */
void backup_profile(mtk_ap_buf_t*);
void apply_sigma_profile(mtk_ap_buf_t*, device_type);
void restore_profile(mtk_ap_buf_t*);
dict_t init_capi_key_dict ();
dict_t init_2g_dict (mtk_ap_buf_t*);
dict_t init_5g_dict (mtk_ap_buf_t*);
dict_t init_6g_dict (mtk_ap_buf_t*);
#endif //MTK_DUT_MTK_AP_H
