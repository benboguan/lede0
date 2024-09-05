
#if WIRELESS_EXT <= 11
#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE			0x8BE0
#endif
#define SIOCIWFIRSTPRIV			SIOCDEVPRIVATE
#endif

#define RT_PRIV_IOCTL			(SIOCIWFIRSTPRIV + 0x01)
#define RTPRIV_IOCTL_SET		(SIOCIWFIRSTPRIV + 0x02)
#define OID_GET_SET_TOGGLE		0x8000

#define OID_802_DOT1X_IDLE_TIMEOUT			0x0545
#define RT_OID_802_DOT1X_IDLE_TIMEOUT		(OID_GET_SET_TOGGLE | OID_802_DOT1X_IDLE_TIMEOUT)

#define	OID_802_11_SSID                 0x0509
#define	OID_802_11_BSSID                0x050A
#define	OID_GEN_MEDIA_CONNECT_STATUS    0x060B
#define	OID_MTK_CHIP_ID                 0x068A
#define	OID_MTK_DRVER_VERSION           0x068B
#define	OID_802_11_RSSI					0x060D
#define	OID_802_11_GET_SSID_BSSID		0x0689

#ifdef FIRST_CARD_MT7615_SECOND_CARD_MT7615
#define WIFI_DEF_PROFILE_1 "/etc/wireless/sigma_test/wifi_cert.1.dat"
#define WIFI_DEF_PROFILE_2 "/etc/wireless/sigma_test/wifi_cert.2.dat"

#define WIFI_DEF_PROFILE_DBDC_1 "/etc/wireless/sigma_test/wifi_cert_b0.dat"
#define WIFI_DEF_PROFILE_DBDC_2 "/etc/wireless/sigma_test/wifi_cert_b1.dat"

#define WIFI_PROFILE_1 "/etc/wireless/mediatek/mt7615e.1.dat"
#define WIFI_PROFILE_2 "/etc/wireless/mediatek/mt7615e.2.dat"

#define WIFI_PROFILE_DBDC_1 "/etc/wireless/mediatek/mt7615.dbdc.b0.dat"
#define WIFI_PROFILE_DBDC_2 "/etc/wireless/mediatek/mt7615.dbdc.b1.dat"
#else
#define WIFI_DEF_PROFILE_1 "/etc/wireless/mediatek/wifi_cert.1.dat"
#define WIFI_DEF_PROFILE_2 "/etc/wireless/mediatek/wifi_cert.2.dat"

#define WIFI_DEF_PROFILE_DBDC_1 "/etc/wireless/sigma_test/wifi_cert_b0.dat"
#define WIFI_DEF_PROFILE_DBDC_2 "/etc/wireless/sigma_test/wifi_cert_b1.dat"

#define WIFI_PROFILE_1 "/etc/wireless/mediatek/mt7915.1.dat"
#define WIFI_PROFILE_2 "/etc/wireless/mediatek/mt7915.2.dat"

#define WIFI_PROFILE_DBDC_1 "/etc/wireless/mediatek/mt7915.dbdc.b0.dat"
#define WIFI_PROFILE_DBDC_2 "/etc/wireless/mediatek/mt7915.dbdc.b1.dat"
#endif

typedef struct _wfa_driver_mtk_config {
	int ioctl_sock;
#ifdef TGAC_DAEMON	
	int fd;
#endif
	char ifname[32];
	unsigned char dev_addr[6];
	unsigned char addr[18];
	unsigned char ssid[32];
	unsigned char conn_stat;
} wfa_driver_mtk_config, *pwfa_driver_mtk_config;

typedef struct _wfa_wifi_profile {
	char default_profile[64];
	char testing_profile[64];
} wfa_wifi_profile, *p_wfa_wifi_profile;

void wfa_driver_mtk_sock_int();
void wfa_driver_mtk_sock_exit();
int wfa_driver_mtk_set_oid(char *ifname, int oid, char *data, int len);
int wfa_driver_mtk_get_oid(char *intf, int oid, unsigned char *data, int len);
int wfa_driver_mtk_set_cmd(char *pIntfName, char *pCmdStr);
int wfa_driver_mtk_check_ssid(char *pIntfName, char *pSsid);
void wfa_driver_mtk_interface_name_send(void);
void wfa_driver_mtk_reset_profile(void);

