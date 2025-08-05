#ifndef _MTK_H
#define _MTK_H

#include "iwinfo.h"

#define USHORT  unsigned short
#define UCHAR   unsigned char
#define ULONG	unsigned long
#define UINT8	unsigned char
#define UINT16	unsigned short
#define UINT32	unsigned int
#define UINT64	unsigned long long
#define CHAR	char
#define SHORT 	short
#define INT32	int
#define INT64	long long
#define INT 	int

#define min(x, y) ((x) < (y)) ? (x) : (y)

#define MAX_NUMBER_OF_MAC		    116

#define RT_PRIV_IOCTL				(SIOCIWFIRSTPRIV + 0x01)
#define RTPRIV_IOCTL_SET			(SIOCIWFIRSTPRIV + 0x02)
//#define RTPRIV_IOCTL_SET			(SIOCIWFIRSTPRIV + 0x0B)
#define RTPRIV_IOCTL_E2P			(SIOCIWFIRSTPRIV + 0x07)

#define RTPRIV_IOCTL_GCHANLIST              (SIOCIWFIRSTPRIV + 0x10)
#define RTPRIV_IOCTL_GSCANINFO              (SIOCIWFIRSTPRIV + 0x14)
#define RTPRIV_IOCTL_GBSSINFO				(SIOCIWFIRSTPRIV + 0x1C)
#define RTPRIV_IOCTL_GSTAINFO				(SIOCIWFIRSTPRIV + 0x1E)
#define RTPRIV_IOCTL_GET_MAC_TABLE			(SIOCIWFIRSTPRIV + 0x0F)
#define RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT	(SIOCIWFIRSTPRIV + 0x1F)
#define RTPRIV_IOCTL_SHOW                	(SIOCIWFIRSTPRIV + 0x11)
#define RTPRIV_IOCTL_GSITESURVEY            (SIOCIWFIRSTPRIV + 0x0D)
#define RTPRIV_IOCTL_PHY_STATE              (SIOCIWFIRSTPRIV + 0x21)
#define RTPRIV_IOCTL_GET_DRIVER_INFO        (SIOCIWFIRSTPRIV + 0x1D)
#define OID_802_11_GET_COUNTRY_CODE			0x0716
#define OID_802_11_COUNTRYCODE				0x1907
#define OID_802_11_BW						0x1903
#define OID_GET_CHAN_LIST					0x0998
#define OID_GET_CHANNEL_LIST				0x09C0
#define OID_GET_WIRELESS_BAND				0x09B4
#define OID_802_11_SECURITY_TYPE			0x093e
#define RT_OID_802_11_PHY_MODE				0x050C
#define RT_OID_VERSION_INFO					0x0608
#define OID_802_11_GET_CENTRAL_CHAN1					0x0978
#define OID_802_11_GET_CENTRAL_CHAN2					0x0979
#define GET_MAC_TABLE_STRUCT_FLAG_RAW_SSID	0x1

#define MODE_CCK		0
#define MODE_OFDM		1
#define MODE_HTMIX		2
#define MODE_HTGREENFIELD	3
#define MODE_VHT 4

#define TMI_TX_RATE_OFDM_6M     11
#define TMI_TX_RATE_OFDM_9M     15
#define TMI_TX_RATE_OFDM_12M    10
#define TMI_TX_RATE_OFDM_18M    14
#define TMI_TX_RATE_OFDM_24M    9
#define TMI_TX_RATE_OFDM_36M    13
#define TMI_TX_RATE_OFDM_48M    8
#define TMI_TX_RATE_OFDM_54M    12

#define TMI_TX_RATE_CCK_1M_LP   0
#define TMI_TX_RATE_CCK_2M_LP   1
#define TMI_TX_RATE_CCK_5M_LP   2
#define TMI_TX_RATE_CCK_11M_LP  3

#define TMI_TX_RATE_CCK_2M_SP   5
#define TMI_TX_RATE_CCK_5M_SP   6
#define TMI_TX_RATE_CCK_11M_SP  7

/* HT */
#define MCS_0          0       /* 1S */
#define MCS_1          1
#define MCS_2          2
#define MCS_3          3
#define MCS_4          4
#define MCS_5          5
#define MCS_6          6
#define MCS_7          7
#define MCS_8          8       /* 2S */
#define MCS_9          9
#define MCS_10         10
#define MCS_11         11
#define MCS_12         12
#define MCS_13         13
#define MCS_14         14
#define MCS_15         15
#define MCS_16         16      /* 3*3 */
#define MCS_17         17
#define MCS_18         18
#define MCS_19         19
#define MCS_20         20
#define MCS_21         21
#define MCS_22         22
#define MCS_23         23
#define MCS_24         24      /* 3*3 */
#define MCS_25         25
#define MCS_26         26
#define MCS_27         27
#define MCS_28         28
#define MCS_29         29
#define MCS_30         30
#define MCS_31         31
#define MCS_32         32
#define MCS_AUTO	33

#define GI_HE_800		0
#define GI_HE_1600		1
#define GI_HE_3200		2

/* Extension channel offset */
#define EXTCHA_NONE			0
#define EXTCHA_ABOVE		0x1
#define EXTCHA_BELOW		0x3
#define EXTCHA_NOASSIGN		0xf

/* BW */
enum oid_bw {
	BAND_WIDTH_20,
	BAND_WIDTH_40,
	BAND_WIDTH_80,
	BAND_WIDTH_160,
	BAND_WIDTH_10,
	BAND_WIDTH_5,
	BAND_WIDTH_8080,
	BAND_WIDTH_BOTH,
	BAND_WIDTH_25,
	BAND_WIDTH_20_242TONE,
	BAND_WIDTH_NUM
};

#define BW_20		BAND_WIDTH_20
#define BW_40		BAND_WIDTH_40
#define BW_80		BAND_WIDTH_80
#define BW_160		BAND_WIDTH_160
#define BW_10		BAND_WIDTH_10
#define BW_5		BAND_WIDTH_5
#define BW_8080		BAND_WIDTH_8080
#define BW_25		BAND_WIDTH_25
#define BW_20_242TONE	BAND_WIDTH_20_242TONE
#define BW_NUM		BAND_WIDTH_NUM

#define 		MAC_ADDR_LEN 6
#define 		ETH_LENGTH_OF_ADDRESS 6
#define 		MAX_LEN_OF_MAC_TABLE 128

struct security_info {
	unsigned int ifindex;
	unsigned int auth_mode;
	unsigned int encryp_type;
};

typedef union _HTTRANSMIT_SETTING {
	struct  {
		unsigned short	MCS:6;  // MCS
		unsigned short	ldpc:1;
		unsigned short	BW:2;   //channel bandwidth 20MHz or 40 MHz
		unsigned short	ShortGI:1;
		unsigned short	STBC:1; //SPACE
		unsigned short	eTxBF:1;
		unsigned short	iTxBF:1;
		unsigned short	MODE:3; // Use definition MODE_xxx.
	} field;
	unsigned short	word;
} HTTRANSMIT_SETTING, *PHTTRANSMIT_SETTING;

typedef struct _RT_802_11_MAC_ENTRY {
	unsigned char		ApIdx;
	unsigned char		Addr[6];
	unsigned char		Aid;
	unsigned char		Psm;     // 0:PWR_ACTIVE, 1:PWR_SAVE
	unsigned char		MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
	char			AvgRssi0;
	char			AvgRssi1;
	char			AvgRssi2;
	char			AvgRssi3;
	unsigned int		ConnectedTime;
	HTTRANSMIT_SETTING	TxRate;
	unsigned int		LastRxRate;
	unsigned int		InactiveTime;
	unsigned int		AvgSnr;
	short			StreamSnr[3];
	short			SoundingRespSnr[3];
	unsigned long		TxPackets;
	unsigned long		RxPackets;
	unsigned long		TxBytes;
	unsigned long		RxBytes;
	unsigned int		EncryMode;
	unsigned int		AuthMode;
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;

typedef struct _RT_802_11_MAC_TABLE {
	unsigned long		Num;
	RT_802_11_MAC_ENTRY	Entry[MAX_NUMBER_OF_MAC];
} RT_802_11_MAC_TABLE, *PRT_802_11_MAC_TABLE;

typedef enum _SEC_CIPHER_MODE {
	SEC_CIPHER_NONE,
	SEC_CIPHER_WEP40,
	SEC_CIPHER_WEP104,
	SEC_CIPHER_WEP128,
	SEC_CIPHER_TKIP,
	SEC_CIPHER_CCMP128,
	SEC_CIPHER_CCMP256,
	SEC_CIPHER_GCMP128,
	SEC_CIPHER_GCMP256,
	SEC_CIPHER_BIP_CMAC128,
	SEC_CIPHER_BIP_CMAC256,
	SEC_CIPHER_BIP_GMAC128,
	SEC_CIPHER_BIP_GMAC256,
	SEC_CIPHER_WPI_SMS4, /* WPI SMS4 support */
	SEC_CIPHER_MAX /* Not a real mode, defined as upper bound */
} SEC_CIPHER_MODE;

#define IS_CIPHER_NONE(_Cipher)          (((_Cipher) & (1 << SEC_CIPHER_NONE)) > 0)
#define IS_CIPHER_WEP40(_Cipher)          (((_Cipher) & (1 << SEC_CIPHER_WEP40)) > 0)
#define IS_CIPHER_WEP104(_Cipher)        (((_Cipher) & (1 << SEC_CIPHER_WEP104)) > 0)
#define IS_CIPHER_WEP128(_Cipher)        (((_Cipher) & (1 << SEC_CIPHER_WEP128)) > 0)
#define IS_CIPHER_WEP(_Cipher)              (((_Cipher) & ((1 << SEC_CIPHER_WEP40) | (1 << SEC_CIPHER_WEP104) | (1 << SEC_CIPHER_WEP128))) > 0)
#define IS_CIPHER_TKIP(_Cipher)              (((_Cipher) & (1 << SEC_CIPHER_TKIP)) > 0)
#define IS_CIPHER_WEP_TKIP_ONLY(_Cipher)     ((IS_CIPHER_WEP(_Cipher) || IS_CIPHER_TKIP(_Cipher)) && (_Cipher < (1 << SEC_CIPHER_CCMP128)))
#define IS_CIPHER_CCMP128(_Cipher)      (((_Cipher) & (1 << SEC_CIPHER_CCMP128)) > 0)
#define IS_CIPHER_CCMP256(_Cipher)      (((_Cipher) & (1 << SEC_CIPHER_CCMP256)) > 0)
#define IS_CIPHER_GCMP128(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_GCMP128)) > 0)
#define IS_CIPHER_GCMP256(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_GCMP256)) > 0)
#define IS_CIPHER_BIP_CMAC128(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_BIP_CMAC128)) > 0)
#define IS_CIPHER_BIP_CMAC256(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_BIP_CMAC256)) > 0)
#define IS_CIPHER_BIP_GMAC128(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_BIP_GMAC128)) > 0)
#define IS_CIPHER_BIP_GMAC256(_Cipher)     (((_Cipher) & (1 << SEC_CIPHER_BIP_GMAC256)) > 0)

/* 802.11 authentication and key management */
typedef enum _SEC_AKM_MODE {
	SEC_AKM_OPEN,
	SEC_AKM_SHARED,
	SEC_AKM_AUTOSWITCH,
	SEC_AKM_WPA1, /* Enterprise security over 802.1x */
	SEC_AKM_WPA1PSK,
	SEC_AKM_WPANone, /* For Win IBSS, directly PTK, no handshark */
	SEC_AKM_WPA2, /* Enterprise security over 802.1x */
	SEC_AKM_WPA2PSK,
	SEC_AKM_FT_WPA2,
	SEC_AKM_FT_WPA2PSK,
	SEC_AKM_WPA2_SHA256,
	SEC_AKM_WPA2PSK_SHA256,
	SEC_AKM_TDLS,
	SEC_AKM_SAE_SHA256,
	SEC_AKM_FT_SAE_SHA256,
	SEC_AKM_SUITEB_SHA256,
	SEC_AKM_SUITEB_SHA384,
	SEC_AKM_FT_WPA2_SHA384,
	SEC_AKM_WAICERT, /* WAI certificate authentication */
	SEC_AKM_WAIPSK, /* WAI pre-shared key */
	SEC_AKM_OWE,
	SEC_AKM_DPP,
	SEC_AKM_FILS_SHA256,
	SEC_AKM_FILS_SHA384,
	SEC_AKM_WPA3, /* WPA3(ent) = WPA2(ent) + PMF MFPR=1 => WPA3 code flow is same as WPA2, the usage of SEC_AKM_WPA3 is to force pmf on */
	SEC_AKM_MAX /* Not a real mode, defined as upper bound */
} SEC_AKM_MODE, *PSEC_AKM_MODE;

#define IS_AKM_OPEN(_AKMMap)                           ((_AKMMap & (1 << SEC_AKM_OPEN)) > 0)
#define IS_AKM_SHARED(_AKMMap)                       ((_AKMMap & (1 << SEC_AKM_SHARED)) > 0)
#define IS_AKM_AUTOSWITCH(_AKMMap)              ((_AKMMap & (1 << SEC_AKM_AUTOSWITCH)) > 0)
#define IS_AKM_WPA1(_AKMMap)                           ((_AKMMap & (1 << SEC_AKM_WPA1)) > 0)
#define IS_AKM_WPA1PSK(_AKMMap)                    ((_AKMMap & (1 << SEC_AKM_WPA1PSK)) > 0)
#define IS_AKM_WPANONE(_AKMMap)                  ((_AKMMap & (1 << SEC_AKM_WPANone)) > 0)
#define IS_AKM_WPA2(_AKMMap)                          ((_AKMMap & (1 << SEC_AKM_WPA2)) > 0)
#define IS_AKM_WPA2PSK(_AKMMap)                    ((_AKMMap & (1 << SEC_AKM_WPA2PSK)) > 0)
#define IS_AKM_FT_WPA2(_AKMMap)                     ((_AKMMap & (1 << SEC_AKM_FT_WPA2)) > 0)
#define IS_AKM_FT_WPA2PSK(_AKMMap)              ((_AKMMap & (1 << SEC_AKM_FT_WPA2PSK)) > 0)
#define IS_AKM_WPA2_SHA256(_AKMMap)            ((_AKMMap & (1 << SEC_AKM_WPA2_SHA256)) > 0)
#define IS_AKM_WPA2PSK_SHA256(_AKMMap)      ((_AKMMap & (1 << SEC_AKM_WPA2PSK_SHA256)) > 0)
#define IS_AKM_TDLS(_AKMMap)                             ((_AKMMap & (1 << SEC_AKM_TDLS)) > 0)
#define IS_AKM_SAE_SHA256(_AKMMap)                ((_AKMMap & (1 << SEC_AKM_SAE_SHA256)) > 0)
#define IS_AKM_FT_SAE_SHA256(_AKMMap)          ((_AKMMap & (1 << SEC_AKM_FT_SAE_SHA256)) > 0)
#define IS_AKM_SUITEB_SHA256(_AKMMap)          ((_AKMMap & (1 << SEC_AKM_SUITEB_SHA256)) > 0)
#define IS_AKM_SUITEB_SHA384(_AKMMap)          ((_AKMMap & (1 << SEC_AKM_SUITEB_SHA384)) > 0)
#define IS_AKM_FT_WPA2_SHA384(_AKMMap)      ((_AKMMap & (1 << SEC_AKM_FT_WPA2_SHA384)) > 0)
#define IS_AKM_WPA3(_AKMMap)	 ((_AKMMap & (1 << SEC_AKM_WPA3)) > 0)
#define IS_AKM_WPA3PSK(_AKMMap) (IS_AKM_SAE_SHA256(_AKMMap))
#define IS_AKM_WPA3_192BIT(_AKMMap)	(IS_AKM_SUITEB_SHA384(_AKMMap))
#define IS_AKM_WAICERT(_AKMMap)                      ((_AKMMap & (1 << SEC_AKM_WAICERT)) > 0)
#define IS_AKM_WPIPSK(_AKMMap)                        ((_AKMMap & (1 << SEC_AKM_WAIPSK)) > 0)
#define IS_AKM_OWE(_AKMMap) ((_AKMMap & (1 << SEC_AKM_OWE)) > 0)
#define IS_AKM_DPP(_AKMMap) ((_AKMMap & (1 << SEC_AKM_DPP)) > 0)
#define IS_AKM_FILS_SHA256(_AKMMap)                ((_AKMMap & (1 << SEC_AKM_FILS_SHA256)) > 0)
#define IS_AKM_FILS_SHA384(_AKMMap)                ((_AKMMap & (1 << SEC_AKM_FILS_SHA384)) > 0)

#define MTK_L1_PROFILE_PATH		"/etc/wireless/l1profile.dat"

void getRate(HTTRANSMIT_SETTING HTSetting, unsigned long *fLastTxRxRate);
unsigned int cck_to_mcs(unsigned int mcs);
//int mtk_get_assoclist(const char *ifname, char *buf, int *len);

#endif
