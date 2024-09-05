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
 *  wfa_typestr.c:
 *  global array of the mapping of command types, command strings
 *  to attached processing function
 *
 */
#include <stdio.h>
#include <pthread.h>
#include "wfa_types.h"
#include "wfa_tlv.h"
#include "wfa_tg.h"
#include "wfa_cmds.h"
#include "wfa_agtctrl.h"
/*
 * define the function pointer;

*/
extern int wfaApCaVersion(int , BYTE *, int *, BYTE *);
extern int wfaApConfigCommit(int , BYTE *, int *, BYTE *);
extern int wfaApDeauthSta(int , BYTE *, int *, BYTE *);
extern int wfaApGetMacAddress(int , BYTE *, int *, BYTE *);
extern int wfaApResetDefault(int , BYTE *, int *, BYTE *);
extern int wfaApSendAbbdaReq(int , BYTE *, int *, BYTE *);
extern int wfaApSendBcnrptReq(int , BYTE *, int *, BYTE *);
extern int wfaApSendBsstransMgmtReq(int , BYTE *, int *, BYTE *);
extern int wfaApSendLinkMeaReq(int , BYTE *, int *, BYTE *);
extern int wfaApSendTsmrptReq(int , BYTE *, int *, BYTE *);
extern int wfaApSet11d(int , BYTE *, int *, BYTE *);
extern int wfaApSet11h(int , BYTE *, int *, BYTE *);
extern int wfaApSet11nWireless(int , BYTE *, int *, BYTE *);
extern int wfaApSetApqos(int , BYTE *, int *, BYTE *);
extern int wfaApSetHs2(int , BYTE *, int *, BYTE *);
extern int wfaApSetPmf(int , BYTE *, int *, BYTE *);
extern int wfaApSetRadius(int , BYTE *, int *, BYTE *);
extern int wfaApSetRfeature(int , BYTE *, int *, BYTE *);
extern int wfaApSetRrm(int , BYTE *, int *, BYTE *);
extern int wfaApSetSecurity(int , BYTE *, int *, BYTE *);
extern int wfaApSetStaqos(int , BYTE *, int *, BYTE *);
extern int wfaApSetWireless(int , BYTE *, int *, BYTE *);

// with resp


extern int wfaApCaVersionResp(BYTE *);
extern int wfaApConfigCommitResp(BYTE *);
extern int wfaApDeauthStaResp(BYTE *);
extern int wfaApGetMacAddressResp(BYTE *);
extern int wfaApResetDefaultResp(BYTE *);

extern int wfaApSendAbbdaReqResp(BYTE *);
extern int wfaApSendBcnrptReqResp(BYTE *);
extern int wfaApSendBsstransMgmtReqResp(BYTE *);
extern int wfaApSendLinkMeaReqResp(BYTE *);
extern int wfaApSendTsmrptReqResp(BYTE *);

extern int wfaApSet11dResp(BYTE *);
extern int wfaApSet11hResp(BYTE *);
extern int wfaApSet11nWirelessResp(BYTE *);
extern int wfaApSetApqosResp(BYTE *);
extern int wfaApSetHs2Resp(BYTE *);

extern int wfaApSetPmfResp(BYTE *);
extern int wfaApSetRadiusResp(BYTE *);
extern int wfaApSetRfeatureResp(BYTE *);
extern int wfaApSetRrmResp(BYTE *);
extern int wfaApSetSecurityResp(BYTE *);
extern int wfaApSetStaqosResp(BYTE *);
extern int wfaApSetWirelessResp(BYTE *);


//
extern int cmdProcNotDefinedYet(char *, BYTE *, int *);
extern int xcCmdProcGetVersion(char *, BYTE *, int *);
extern int xcCmdProcAgentConfig(char *, BYTE *, int *);
extern int xcCmdProcAgentSend(char *, BYTE *, int *);
extern int xcCmdProcAgentRecvStart(char *, BYTE *, int *);
extern int xcCmdProcAgentRecvStop(char *, BYTE *, int *);
extern int xcCmdProcAgentReset(char *, BYTE *, int *);
extern int xcCmdProcStaGetIpConfig(char *, BYTE *, int *);
extern int xcCmdProcStaSetIpConfig(char *, BYTE *, int *);
extern int xcCmdProcStaGetMacAddress(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetMacAddress(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaIsConnected(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaVerifyIpConnection(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaGetBSSID(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaGetStats(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetEncryption(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetPSK(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetEapTLS(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetEapTTLS(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetEapSIM(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetPEAP(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetIBSS(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcDeviceGetInfo(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcDeviceListIF(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaAssociate(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetUAPSD(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaGetInfo(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcAgentSendPing(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcAgentStopPing(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaDebugSet(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaUpload(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaSetMode(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetWMM(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaPresetTestParameters(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetEapFAST(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetEapAKA(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetSystime(char *pcmStr, BYTE *, int *);

extern int xcCmdProcStaSet11n(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetWireless(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSendADDBA(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetRIFS(char *, BYTE *, int *);
extern int xcCmdProcStaSendCoExistMGMT(char *, BYTE *, int *);
extern int xcCmdProcStaResetDefault(char *, BYTE *, int *);
extern int xcCmdProcStaDisconnect(char *, BYTE *, int *);

extern int xcCmdProcStaReAssociate(char *pcmStr, BYTE*, int *);
extern int xcCmdProcStaResetDefault(char *pcmdStr, BYTE *aBuf, int *aLen);
extern int xcCmdProcStaSetPwrSave(char *pcmdStr, BYTE *aBuf, int *aLen);

extern int xcCmdProcStaGetP2pDevAddress(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetP2p(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaP2pConnect(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaP2pStartAutoGo(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaP2pStartGroupFormation(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaP2pDissolve(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSendP2pInvReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaAcceptP2pInvReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSendP2pProvDisReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetWpsPbc(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaWpsReadPin(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaWpsEnterPin(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaGetPsk(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaP2pReset(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaWpsReadLabel(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaGetP2pIpConfig(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSendServiceDiscoveryReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSendP2pPresenceReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetSleepReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaSetOpportunistcPsReq(char *pcmStr, BYTE *, int *);
extern int xcCmdProcStaAddARPTableEntry(char *, BYTE *, int *);
extern int xcCmdProcStaBlockICMPResponse(char *, BYTE *, int *);

extern int xcCmdProcStaDevSendFrame(char *pcmdStr, BYTE *aBuf, int *aLen);
extern int xcCmdProcStaSetSecurity(char *pcmdStr, BYTE *aBuf, int *aLen);

extern int xcCmdProcStaSetRadio(char *pcmdStr, BYTE *aBuf, int *aLen);
extern int xcCmdProcStaSetRFeature(char *pcmdStr, BYTE *aBuf, int *aLen);

extern int xcCmdProcStaStartWfdConnection(char *pcmdStr, BYTE *aBuf, int *aLen);
extern int xcCmdProcStaCliCommand(char *, BYTE *, int *);

extern int xcCmdProcStaConnectGoStartWfd(char *pcmdStr, BYTE *aBuf, int *aLen);
extern int xcCmdProcStaGenerateEvent(char *, BYTE *, int *);
extern int xcCmdProcStaReinvokeWfdSession(char *, BYTE *, int *);
extern int xcCmdProcStaGetParameter(char *, BYTE *, int *);


extern int xcCmdProcStaNfcAction(char *, BYTE *, int *);
extern int xcCmdProcStaExecAction(char *pcmdStr, BYTE *, int *);

extern int xcCmdProcStaInvokeCommand(char *, BYTE *, int *);
extern int xcCmdProcStaManageService(char *, BYTE *, int *);

extern int xcCmdProcStaGetEvents(char *pcmdStr, BYTE *, int *);
extern int xcCmdProcStaGetEventDetails(char *, BYTE *, int *);
// extern the xcCmdProc function based on wfa structure
// 8.2
extern int xcCmdProcApCaVersion(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.3
extern int xcCmdProcApConfigCommit(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.4
extern int xcCmdProcApDeauthSta(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.5
extern int xcCmdApGetMacAddress(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.6
extern int xcCmdProcApResetDefault(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.7
extern int xcCmdProcApSendAbbdaReq(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.8
extern int xcCmdProcApSendBcnrptReq(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.9
extern int xcCmdProcApSendBsstransMgmtReq(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.10
extern int xcCmdProcApSendLinkMeaReq(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.11
extern int xcCmdProcApSendTsmrptReq(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.12
extern int xcCmdProcApSet11d(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.13
extern int xcCmdProcApSet11h(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.14
extern int xcCmdProcApSet11nWireless(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.15
extern int xcCmdProcApSetApqos(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.16
extern int xcCmdProcApSetHs2(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.17
extern int xcCmdProcApSetPmf(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.18
extern int xcCmdApSetRadius(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.19
extern int xcCmdProcApSetRfeature(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.20
extern int xcCmdProcApSetRrm(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.21
extern int xcCmdProcApSetSecurity(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.22
extern int xcCmdProcApSetStaqos(char *pcmdStr, BYTE *aBuf, int *aLen);
// 8.23
extern int xcCmdProcApSetWireless(char *pcmdStr, BYTE *aBuf, int *aLen);



/*
 * Initialize a command name table to its defined type and process function
 * added AP function by CTD/SE2/CS2 Boa-Lin
 */
//typeNameStr_t nameStr[] =
//{
//    {0,                   "NO_USED_STRING", NULL},
//    {WFA_GET_VERSION_TLV, "ca_get_version", xcCmdProcGetVersion},
//    {WFA_TRAFFIC_SEND_PING_TLV, "traffic_send_ping", xcCmdProcAgentSendPing},
//    {WFA_TRAFFIC_STOP_PING_TLV, "traffic_stop_ping", xcCmdProcAgentStopPing},
//    {WFA_TRAFFIC_AGENT_CONFIG_TLV, "traffic_agent_config", xcCmdProcAgentConfig},
//    {WFA_TRAFFIC_AGENT_SEND_TLV, "traffic_agent_send", xcCmdProcAgentSend},
//    {WFA_TRAFFIC_AGENT_RESET_TLV, "traffic_agent_reset", xcCmdProcAgentReset},
//    {WFA_TRAFFIC_AGENT_RECV_START_TLV, "traffic_agent_receive_start", xcCmdProcAgentRecvStart},
//    {WFA_TRAFFIC_AGENT_RECV_STOP_TLV, "traffic_agent_receive_stop", xcCmdProcAgentRecvStop},
//    /* Control Commands */
//    {WFA_STA_GET_IP_CONFIG_TLV, "sta_get_ip_config", xcCmdProcStaGetIpConfig},
//    {WFA_STA_SET_IP_CONFIG_TLV, "sta_set_ip_config", xcCmdProcStaSetIpConfig},
//    {WFA_STA_GET_MAC_ADDRESS_TLV, "sta_get_mac_address", xcCmdProcStaGetMacAddress},
//    {WFA_STA_SET_MAC_ADDRESS_TLV, "sta_set_mac_address", xcCmdProcStaSetMacAddress},
//    {WFA_STA_IS_CONNECTED_TLV, "sta_is_connected", xcCmdProcStaIsConnected},
//    {WFA_STA_VERIFY_IP_CONNECTION_TLV, "sta_verify_ip_connection", xcCmdProcStaVerifyIpConnection},
//    {WFA_STA_GET_BSSID_TLV, "sta_get_bssid", xcCmdProcStaGetBSSID},
//    {WFA_STA_GET_STATS_TLV, "sta_get_stats", xcCmdProcStaGetStats},
//    {WFA_STA_SET_ENCRYPTION_TLV, "sta_set_encryption", xcCmdProcStaSetEncryption},
//    {WFA_STA_SET_PSK_TLV, "sta_set_psk", xcCmdProcStaSetPSK},
//    {WFA_STA_SET_EAPTLS_TLV, "sta_set_eaptls", xcCmdProcStaSetEapTLS},
//    {WFA_STA_SET_EAPTTLS_TLV, "sta_set_eapttls", xcCmdProcStaSetEapTTLS},
//    {WFA_STA_SET_EAPSIM_TLV, "sta_set_eapsim", xcCmdProcStaSetEapSIM},
//    {WFA_STA_SET_PEAP_TLV, "sta_set_peap", xcCmdProcStaSetPEAP},
//    {WFA_STA_SET_IBSS_TLV, "sta_set_ibss", xcCmdProcStaSetIBSS},
//    {WFA_STA_ASSOCIATE_TLV, "sta_associate", xcCmdProcStaAssociate},
//    {WFA_DEVICE_LIST_IF_TLV, "device_list_interfaces", xcCmdProcDeviceListIF},
//    {WFA_DEVICE_GET_INFO_TLV, "device_get_info", xcCmdProcDeviceGetInfo},
//    {WFA_STA_GET_INFO_TLV, "sta_get_info", xcCmdProcStaGetInfo},
//    {WFA_STA_SET_MODE_TLV, "sta_set_mode", xcCmdProcStaSetMode},
//    {WFA_STA_UPLOAD_TLV, "sta_up_load", xcCmdProcStaUpload},
//    {WFA_STA_DEBUG_SET_TLV, "sta_debug_set", xcCmdProcStaDebugSet},
//    {WFA_STA_SET_UAPSD_TLV, "sta_set_uapsd", xcCmdProcStaSetUAPSD},
//    {WFA_STA_SET_WMM_TLV, "sta_set_wmm", xcCmdProcStaSetWMM},
//    {WFA_STA_DISCONNECT_TLV, "sta_disconnect", xcCmdProcStaDisconnect},
//    {WFA_STA_REASSOCIATE_TLV, "sta_reassociate", xcCmdProcStaReAssociate},
//    {WFA_STA_DEV_SEND_FRAME_TLV, "dev_send_frame", xcCmdProcStaDevSendFrame},
//    {WFA_STA_SET_SECURITY_TLV, "sta_set_security", xcCmdProcStaSetSecurity},
//#ifdef WFA_STA_TB
//    {WFA_STA_PRESET_PARAMETERS_TLV, "sta_preset_testparameters", xcCmdProcStaPresetTestParameters},
//#endif
//    {WFA_STA_SET_EAPFAST_TLV, "sta_set_eapfast", xcCmdProcStaSetEapFAST},
//    {WFA_STA_SET_EAPAKA_TLV, "sta_set_eapaka", xcCmdProcStaSetEapAKA},
//    {WFA_STA_SET_SYSTIME_TLV, "sta_set_systime", xcCmdProcStaSetSystime},
//    {WFA_STA_SET_PWRSAVE_TLV, "sta_set_pwrsave", xcCmdProcStaSetPwrSave},
//#ifdef WFA_STA_TB
//    {WFA_STA_RESET_DEFAULT_TLV, "sta_reset_default", xcCmdProcStaResetDefault},
//    {WFA_STA_SET_11N_TLV, "sta_set_11n", xcCmdProcStaSet11n},
//    {WFA_STA_SET_WIRELESS_TLV, "sta_set_wireless", xcCmdProcStaSetWireless},
//    {WFA_STA_SEND_ADDBA_TLV, "sta_send_addba", xcCmdProcStaSendADDBA},
//    {WFA_STA_SET_RIFS_TEST_TLV, "sta_set_rifs_test", xcCmdProcStaSetRIFS},
//    {WFA_STA_SEND_COEXIST_MGMT_TLV, "sta_send_coexist_mgmt", xcCmdProcStaSendCoExistMGMT},
//#endif
//   {WFA_STA_P2P_GET_DEV_ADDRESS_TLV, "sta_get_p2p_dev_address", xcCmdProcStaGetP2pDevAddress},
//   {WFA_STA_P2P_SETP2P_TLV, "sta_set_p2p", xcCmdProcStaSetP2p},
//   {WFA_STA_P2P_CONNECT_TLV, "sta_p2p_connect", xcCmdProcStaP2pConnect},
//   {WFA_STA_P2P_START_AUTO_GO_TLV, "sta_start_autonomous_go", xcCmdProcStaP2pStartAutoGo},
//   {WFA_STA_P2P_START_GRP_FORMATION_TLV, "sta_p2p_start_group_formation", xcCmdProcStaP2pStartGroupFormation},
//   {WFA_STA_P2P_DISSOLVE_TLV, "sta_p2p_dissolve", xcCmdProcStaP2pDissolve},
//   {WFA_STA_P2P_SEND_INV_REQ_TLV, "sta_send_p2p_invitation_req", xcCmdProcStaSendP2pInvReq},
//   {WFA_STA_P2P_ACCEPT_INV_REQ_TLV, "sta_accept_p2p_invitation_req", xcCmdProcStaAcceptP2pInvReq},
//   {WFA_STA_P2P_SEND_PROV_DIS_REQ_TLV, "sta_send_p2p_provision_dis_req", xcCmdProcStaSendP2pProvDisReq},
//   {WFA_STA_WPS_SETWPS_PBC_TLV, "sta_set_wps_pbc", xcCmdProcStaSetWpsPbc},
//   {WFA_STA_WPS_READ_PIN_TLV, "sta_wps_read_pin", xcCmdProcStaWpsReadPin},
//   {WFA_STA_WPS_ENTER_PIN_TLV, "sta_wps_enter_pin", xcCmdProcStaWpsEnterPin},
//   {WFA_STA_P2P_GET_PSK_TLV, "sta_get_psk", xcCmdProcStaGetPsk},
//   {WFA_STA_P2P_RESET_TLV, "sta_p2p_reset", xcCmdProcStaP2pReset},
//   {WFA_STA_WPS_READ_LABEL_TLV, "sta_wps_read_label", xcCmdProcStaWpsReadLabel},
//   {WFA_STA_P2P_GET_IP_CONFIG_TLV, "sta_get_p2p_ip_config", xcCmdProcStaGetP2pIpConfig},
//   {WFA_STA_P2P_SEND_SERVICE_DISCOVERY_REQ_TLV, "sta_send_service_discovery_req", xcCmdProcStaSendServiceDiscoveryReq},
//   {WFA_STA_P2P_SEND_PRESENCE_REQ_TLV, "sta_send_p2p_presence_req", xcCmdProcStaSendP2pPresenceReq},
//   {WFA_STA_P2P_SET_SLEEP_TLV, "sta_set_sleep", xcCmdProcStaSetSleepReq},
//   {WFA_STA_P2P_SET_OPPORTUNISTIC_PS_TLV, "sta_set_opportunistic_ps", xcCmdProcStaSetOpportunistcPsReq},
//   {WFA_STA_P2P_ADD_ARP_TABLE_ENTRY_TLV, "sta_add_arp_table_entry", xcCmdProcStaAddARPTableEntry},
//   {WFA_STA_P2P_BLOCK_ICMP_RESPONSE_TLV, "sta_block_icmp_response", xcCmdProcStaBlockICMPResponse},
//
//   {WFA_STA_SET_RADIO_TLV, "sta_set_radio", xcCmdProcStaSetRadio},
//   {WFA_STA_SET_RFEATURE_TLV, "sta_set_rfeature", xcCmdProcStaSetRFeature},
//
//   {WFA_STA_START_WFD_CONNECTION_TLV, "start_wfd_connection", xcCmdProcStaStartWfdConnection},
//   {WFA_STA_CLI_CMD_TLV, "wfa_cli_cmd", xcCmdProcStaCliCommand},
//   {WFA_STA_CONNECT_GO_START_WFD_TLV, "connect_go_start_wfd", xcCmdProcStaConnectGoStartWfd},
//   {WFA_STA_GENERATE_EVENT_TLV, "sta_generate_event", xcCmdProcStaGenerateEvent},
//   {WFA_STA_REINVOKE_WFD_SESSION_TLV, "reinvoke_wfd_session", xcCmdProcStaReinvokeWfdSession},
//   {WFA_STA_GET_PARAMETER_TLV, "sta_get_parameter", xcCmdProcStaGetParameter},
//
//   {WFA_STA_NFC_ACTION_TLV, "sta_nfc_action", xcCmdProcStaNfcAction},
//   {WFA_STA_EXEC_ACTION_TLV, "sta_exec_action", xcCmdProcStaExecAction},
//
//   {WFA_STA_INVOKE_COMMAND_TLV, "sta_invoke_command", xcCmdProcStaInvokeCommand},
//   {WFA_STA_MANAGE_SERVICE_TLV, "sta_manage_service", xcCmdProcStaManageService},
//   {WFA_STA_GET_EVENTS_TLV, "sta_get_events", xcCmdProcStaGetEvents},
//   {WFA_STA_GET_EVENT_DETAILS_TLV, "sta_get_event_details", xcCmdProcStaGetEventDetails},
//

    // =========================================================================================== //
                // AP function edit by Boa-Lin Lai
//    {WFA_AP_CA_VERSION_TLV, "ap_ca_version", xcCmdProcApCaVersion},
//    {WFA_AP_CONFIG_COMMIT_TLV, "ap_config_commit", xcCmdProcApConfigCommit},
//    {WFA_AP_DEAUTH_STA_TLV, "ap_deauth_sta", xcCmdProcApDeauthSta},
//    {WFA_AP_GET_MAC_ADDRESS_TLV, "ap_get_mac_address", xcCmdApGetMacAddress},
//    {WFA_AP_RESET_DEFAULT_TLV, "ap_reset_default", xcCmdProcApResetDefault},
//    {FA_AP_SEND_ADDBA_REQ_TLV, "ap_send_addba_req", xcCmdProcApSendAbbdaReq},
//    {WFA_AP_SEND_BCNRPT_REQ_TLV, "ap_send_bcnrpt_req", xcCmdProcApSendBcnrptReq},
//    {WFA_AP_SEND_BSSTRANS_MGMT_REQ_TLV, "ap_send_bsstrans_mgmt_req", xcCmdProcApSendBsstransMgmtReq},
//    {WFA_AP_SEND_LINK_MEA_REQ_TLV, "ap_send_link_mea_req", xcCmdProcApSendLinkMeaReq},
//    {WFA_AP_SEND_TSMRPT_REQ_TLV, "ap_send_tsmrpt_req", xcCmdProcApSendTsmrptReq},
//    {WFA_AP_SET_11D_TLV, "ap_set_11d", xcCmdProcApSet11d},
//    {WFA_AP_SET_11H_TLV, "ap_set_11h", xcCmdProcApSet11h},
//    {WFA_AP_SET_11N_WIRELESS_TLV, "ap_set_11n_wireless", xcCmdProcApSet11nWireless},
//    {WFA_AP_SET_APQOS_TLV, "ap_set_apqos", xcCmdProcApSetApqos},
//    {WFA_AP_SET_HS2_TLV, "ap_set_hs2", xcCmdProcApSetHs2},
//    {WFA_AP_SET_PMF_TLV, "ap_set_pmf", xcCmdProcApSetPmf},
//    {WFA_AP_SET_RADIUS_TLV, "ap_set_radius", xcCmdApSetRadius},
//    {WFA_AP_SET_RFEATURE_TLV, "ap_set_rfeature", xcCmdProcApSetRfeature},
//    {WFA_AP_SET_RRM_TLV, "ap_set_rrm", xcCmdProcApSetRrm},
//    {WFA_AP_SET_SECURITY_TLV, "ap_set_security", xcCmdProcApSetSecurity},
//    {WFA_AP_SET_STAQOS_TLV, "ap_set_staqos", xcCmdProcApSetStaqos},
//    {WFA_AP_SET_WIRELESS_TLV, "ap_set_wireless", xcCmdProcApSetWireless},
//
//    // end of AP function
//
//
//   {-1, "", NULL},
//};
