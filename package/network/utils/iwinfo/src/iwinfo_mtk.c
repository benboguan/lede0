#include <inttypes.h>
#include "iwinfo.h"
#include "iwinfo_mtk.h"
#include "iwinfo_wext.h"

#define MIDFIX5G "x"

static const char *mtk_country_codes[] = {
//	"DB",
	"AE",
	"AL",
	"AR",
	"AT",
	"AM",
	"AU",
	"AZ",
	"BE",
	"BH",
	"BY",
	"BO",
	"BR",
	"BN",
	"BG",
	"BZ",
	"CA",
	"CH",
	"CL",
	"CN",
	"CO",
	"CR",
	"CY",
	"CZ",
	"DE",
	"DK",
	"DO",
	"DZ",
	"EC",
	"EG",
	"EE",
	"ES",
	"FI",
	"FR",
	"GE",
	"GB",
	"GR",
	"GT",
	"HN",
	"HK",
	"HU",
	"HR",
	"IS",
	"IN",
	"ID",
	"IR",
	"IE",
	"IL",
	"IT",
	"JP",
	"JO",
	"KP",
	"KR",
	"KW",
	"KZ",
	"LB",
	"LI",
	"LT",
	"LU",
	"LV",
	"MA",
	"MC",
	"MO",
	"MK",
	"MX",
	"MY",
	"NL",
	"NO",
	"NZ",
	"OM",
	"PA",
	"PE",
	"PH",
	"PL",
	"PK",
	"PT",
	"PR",
	"QA",
	"RO",
	"RU",
	"SA",
	"SG",
	"SK",
	"SI",
	"SV",
	"SE",
	"SY",
	"TH",
	"TN",
	"TR",
	"TT",
	"TW",
	"UA",
	"US",
	"UY",
	"UZ",
	"VE",
	"VN",
	"YE",
	"ZA",
	"ZW",
};

int is_5g(const char *ifname)
{
	if (!strncmp(ifname, "ra"MIDFIX5G, 3))
		return 1;
	else if (!strncmp(ifname, "wds"MIDFIX5G, 4))
		return 1;
	else if (!strncmp(ifname, "apcli"MIDFIX5G, 6))
		return 1;

	return 0;
}

static int mtk_ioctl(const char *ifname, int cmd, struct iwreq *wrq)
{
	if (!strncmp(ifname, "mon.", 4))
		strncpy(wrq->ifr_name, &ifname[4], IFNAMSIZ);
	else
		strncpy(wrq->ifr_name, ifname, IFNAMSIZ);

	return iwinfo_ioctl(cmd, wrq);
}

int mtk_oid_ioctl(const char *ifname, unsigned long oid, char *ptr, unsigned long ptr_len)
{
	struct iwreq wrq;
	int cmd = RT_PRIV_IOCTL;
	strcpy(wrq.ifr_name, ifname);
	wrq.u.data.length = ptr_len;
	wrq.u.data.pointer = ptr;
	wrq.u.data.flags = oid;

	return iwinfo_ioctl(cmd, &wrq);
}

int mtk_probe(const char *ifname)
{
	char data[12];
	int version;
	if (strncmp(ifname,"ra",2) == 0 || strncmp(ifname,"wds",3) == 0 || strncmp(ifname,"apcli",5) == 0)
		return 1;
	if (mtk_oid_ioctl(ifname, RT_OID_VERSION_INFO, data, sizeof(data)) == 0)
	{
		data[1] = data[2];
		data[2] = data[4];
		data[3] = data[6];
		data[4] = '\0';
		version = atoi(data);
		if (version >= 2500)
			return 1;
		else
			return 0;
	}
	else
		return -1;
}

void mtk_close(void)
{
	iwinfo_close();
}

int mtk_get_mode(const char *ifname, int *buf)
{
	struct iwreq wrq;

	if (mtk_ioctl(ifname, SIOCGIWMODE, &wrq) >= 0)
	{
		if (strncmp(ifname, "ra", 2) == 0)
			*buf = IWINFO_OPMODE_MASTER;
		else if (strncmp(ifname, "wds", 3) == 0)
			*buf = IWINFO_OPMODE_WDS;
		else if (strncmp(ifname, "apcli", 5) == 0)
			*buf = IWINFO_OPMODE_CLIENT;
		else {
			switch(wrq.u.mode)
			{
				case 1:
					*buf = IWINFO_OPMODE_ADHOC;
					break;

				case 6:
					*buf = IWINFO_OPMODE_MONITOR;
					break;

				default:
					*buf = IWINFO_OPMODE_UNKNOWN;
					break;
			}
		}

		return 0;
	}

	return -1;
}

int mtk_get_ssid(const char *ifname, char *buf)
{
	struct iwreq wrq;

	wrq.u.essid.pointer = (caddr_t)buf;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;

	if (mtk_ioctl(ifname, SIOCGIWESSID, &wrq) >= 0)
		return 0;

	return -1;
}

int mtk_get_bssid(const char *ifname, char *buf)
{
	struct iwreq wrq;

	if (mtk_ioctl(ifname, SIOCGIWAP, &wrq) >= 0)
	{
		sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
			(uint8_t)wrq.u.ap_addr.sa_data[0], (uint8_t)wrq.u.ap_addr.sa_data[1],
			(uint8_t)wrq.u.ap_addr.sa_data[2], (uint8_t)wrq.u.ap_addr.sa_data[3],
			(uint8_t)wrq.u.ap_addr.sa_data[4], (uint8_t)wrq.u.ap_addr.sa_data[5]);

		return 0;
	}

	return -1;
}

static int32_t mtk_freq2mhz(const struct iw_freq *in)
{
	int i;
	int32_t res = in->m;
	if (in->e == 6) {
		return res;
	} else if (in->e > 6) {
		for (i=in->e; i>6; --i) {
			res *= 10;
		}
	} else {
		for (i=in->e; i<6; ++i) {
			res /= 10;
		}
	}
	return res;
}

int mtk_get_channel(const char *ifname, int *buf)
{
	struct iwreq wrq;

	if (mtk_ioctl(ifname, SIOCGIWFREQ, &wrq) >= 0)
	{
		*buf = wrq.u.freq.i;
		return 0;
	}

	return -1;
}

int mtk_get_center_chan1(const char *ifname, int *buf)
{
	struct iwreq wrq;
	int Ch1;

	if (mtk_get_channel(ifname, &Ch1) < 0)
		return -1;

	wrq.u.data.length = sizeof(Ch1);
	wrq.u.data.pointer = &Ch1;
	wrq.u.data.flags = OID_802_11_GET_CENTRAL_CHAN1;

	if (mtk_ioctl(ifname, RT_PRIV_IOCTL, &wrq) >= 0)
	{
		*buf = Ch1;
		return 0;
	}

	return -1;
}

int mtk_get_center_chan2(const char *ifname, int *buf)
{
	struct iwreq wrq;
	int Ch2;

	if (mtk_get_channel(ifname, &Ch2) < 0)
		return -1;

	wrq.u.data.length = sizeof(Ch2);
	wrq.u.data.pointer = &Ch2;
	wrq.u.data.flags = OID_802_11_GET_CENTRAL_CHAN2;

	if (mtk_ioctl(ifname, RT_PRIV_IOCTL, &wrq) >= 0)
	{
		*buf = Ch2;
		return 0;
	}

	return -1;
}

int mtk_get_frequency(const char *ifname, int *buf)
{
	struct iwreq wrq;

	if (mtk_ioctl(ifname, SIOCGIWFREQ, &wrq) >= 0)
	{
		*buf = mtk_freq2mhz(&wrq.u.freq);
		return 0;
	}
	return -1;
}

int mtk_get_txpower(const char *ifname, int *buf)
{
	*buf = 20;
	return 0;
}

int mtk_get_bitrate(const char *ifname, int *buf)
{
	struct iwreq wrq;

	if (mtk_ioctl(ifname, SIOCGIWRATE, &wrq) >= 0)
	{
		*buf = (wrq.u.bitrate.value / 1000);
		return 0;
	}

	return -1;
}

int mtk_get_signal(const char *ifname, int *buf)
{
	int ra_snr_sum, num;
	char tmp_buf[8192];
	struct iwinfo_assoclist_entry tmp;
	int ret_len, i;

	if (mtk_get_assoclist(ifname, tmp_buf, &ret_len) == 0)
	{
		num = ret_len / sizeof(struct iwinfo_assoclist_entry);
		ra_snr_sum = 0;

		for (i = 0; i < num; i++)
		{
			memset(&tmp, 0, sizeof(struct iwinfo_assoclist_entry));
			memcpy(&tmp, tmp_buf + i * sizeof(struct iwinfo_assoclist_entry), sizeof(struct iwinfo_assoclist_entry));

			ra_snr_sum -= tmp.signal;
		}

		if (num > 0)
			*buf = -(ra_snr_sum / num);
		else
			*buf = -127;

		return 0;
	}

	return -1;
}

int mtk_get_noise(const char *ifname, int *buf)
{
	int ra_snr_sum, num;
	char tmp_buf[8192];
	struct iwinfo_assoclist_entry tmp;
	int ret_len, i;

	if (mtk_get_assoclist(ifname, tmp_buf, &ret_len) == 0)
	{
		num = ret_len / sizeof(struct iwinfo_assoclist_entry);
		ra_snr_sum = 0;

		for (i = 0; i < num; i++)
		{
			memset(&tmp, 0, sizeof(struct iwinfo_assoclist_entry));
			memcpy(&tmp, tmp_buf + i * sizeof(struct iwinfo_assoclist_entry), sizeof(struct iwinfo_assoclist_entry));

			ra_snr_sum -= tmp.noise;
		}

		if (num > 0)
			*buf = -(ra_snr_sum / num);
		else
			*buf = -127;

		return 0;
	}

	return -1;
}

int mtk_get_quality(const char *ifname, int *buf)
{
	int signal;

	if (!mtk_get_signal(ifname, &signal))
	{
		/* A positive signal level is usually just a quality
		 * value, pass through as-is */
		if (signal >= 0)
		{
			*buf = signal;
		}

		/* The mtk wext compat layer assumes a signal range
		 * of -127 dBm to -27 dBm, the quality value is derived
		 * by adding fix 127 to the mtk signal level */
		else
		{
			if (signal < -127)
				signal = -127;
			else if (signal > -27)
				signal = -27;

			*buf = (signal + 127);
		}

		return 0;
	}

	return -1;
}

int mtk_get_quality_max(const char *ifname, int *buf)
{
	/* fix The cfg80211 wext compat layer assumes a maximum
	 * quality of 70+30 */
	*buf = 100;
	return 0;
}

static char *mtk_array_get(char *p, int idx) {
	int i;
	char *tail;
	for (i=0; i<idx; ++i) {
		p = strchr(p, ';');
		if (p == NULL) {
			return NULL;
		}
		p += 1;
	}
	tail = strchr(p, ';');
	if (!tail) {
		tail = strchr(p, '\n');
	}
	if (!tail) {
		*tail = '\0';
	}
	return p;
}

int mtk_get_encryption(const char *ifname, char *buf)
{
	FILE *fp;
	const char *filename;
	int ret = -1;
	char buffer[512] = {0};
	char *p = NULL;
	int idx;
	int gcmp = 0;
	int aes = 0;
	int tkip = 0;
	int tkipaes = 0;

	struct iwinfo_crypto_entry *enc = (struct iwinfo_crypto_entry *)buf;

	char data[10];
	if (mtk_oid_ioctl(ifname, RT_OID_VERSION_INFO, data, sizeof(data)) < 0)
		return -1;

	if (is_5g(ifname)) {
		filename = "/tmp/profiles/mt_dbdc_5g.dat";
	} else {
		filename = "/tmp/profiles/mt_dbdc_2g.dat";
	}
	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "open ifname:%s failed.\n", ifname);
		return -1;
	}
	idx = ifname[strlen(ifname)-1] - '0';
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		if (!strncmp(buffer, "AuthMode=", 9)) {
			p = buffer + 9;
			p = mtk_array_get(p, idx);
			if (!p)
				goto end;
			if (strstr(p, "WPA3"))
			{
				enc->enabled = 1;
				if (strstr(p, "WPA2PSKWPA3PSK"))
					enc->wpa_version = 5;
				else if (strstr(p, "WPA3PSK"))
					enc->wpa_version = 4;

				enc->auth_suites |= IWINFO_KMGMT_SAE;
			}
			else if (strstr(p, "WPA"))
			{
				enc->enabled = 1;
				if (strstr(p, "WPAPSKWPA2PSK"))
					enc->wpa_version = 3;
				else if (strstr(p, "WPA2PSK"))
					enc->wpa_version = 2;
				else if (strstr(p, "WPAPSK"))
					enc->wpa_version = 1;

				enc->auth_suites |= IWINFO_KMGMT_PSK;
			}
			else if (strstr(p, "OWE"))
			{
				enc->enabled = 1;
				if (strstr(p, "OWE"))
					enc->wpa_version = 4;

				enc->auth_suites |= IWINFO_KMGMT_OWE;
			}
			else if (strstr(p, "WEP"))
			{
				enc->enabled = 1;
				enc->auth_algs |= IWINFO_AUTH_OPEN | IWINFO_AUTH_SHARED;
				enc->pair_ciphers |= IWINFO_CIPHER_WEP104 | IWINFO_CIPHER_WEP40;
				enc->auth_suites |= IWINFO_KMGMT_NONE;
				enc->group_ciphers = enc->pair_ciphers;
			}
		} else if (!strncmp(buffer, "EncrypType=", 11)) {
			if (enc->pair_ciphers & (IWINFO_CIPHER_WEP104 | IWINFO_CIPHER_WEP40))
				continue;
			p = buffer + 11;
			p = mtk_array_get(p, idx);
			if (!p)
				goto end;
			if (strstr(p, "GCMP"))
				gcmp = 1;
			if (strstr(p, "AES"))
				aes = 1;
			if (strstr(p, "TKIP"))
				tkip = 1;
			if (strstr(p, "TKIPAES"))
				tkipaes = 1;
		}
	}

	if (enc->enabled && enc->auth_suites & IWINFO_KMGMT_SAE) {
		if (gcmp)
			enc->pair_ciphers |= IWINFO_CIPHER_GCMP;
		if (aes)
			enc->pair_ciphers |= IWINFO_CIPHER_CCMP;

		enc->group_ciphers = enc->pair_ciphers;
	}

	if (enc->enabled && enc->auth_suites & IWINFO_KMGMT_PSK) {
		if (aes)
			enc->pair_ciphers |= IWINFO_CIPHER_CCMP;
		if (tkip)
			enc->pair_ciphers |= IWINFO_CIPHER_TKIP;
		if (tkipaes)
			enc->pair_ciphers |= IWINFO_CIPHER_TKIP & IWINFO_CIPHER_CCMP;

		enc->group_ciphers = enc->pair_ciphers;
	}

	ret = 0;
end:
	fclose(fp);
	return ret;
}

int mtk_get_phyname(const char *ifname, char *buf)
{
	if (is_5g(ifname)) {
		strcpy(buf, "ra"MIDFIX5G);
	} else {
		strcpy(buf, "ra");
	}
	return 0;
}

static void fill_rate_info(HTTRANSMIT_SETTING HTSetting, struct iwinfo_rate_entry *re,
	unsigned int mcs, unsigned int nss)
{
	unsigned long DataRate = 0;

	if (HTSetting.field.MODE >= MODE_HTMIX && HTSetting.field.MODE <= MODE_VHT)
	{
		if (HTSetting.field.ShortGI)
			re->is_short_gi = 1;
	}

	if (HTSetting.field.MODE >= MODE_HTMIX && HTSetting.field.MODE <= MODE_HTGREENFIELD)
		re->is_ht = 1;
	else if (HTSetting.field.MODE == MODE_VHT)
		re->is_vht = 1;

	if (HTSetting.field.BW == BW_20)
		re->mhz = 20;
	else if (HTSetting.field.BW == BW_40)
		re->mhz = 40;
	else if (HTSetting.field.BW == BW_80)
		re->mhz = 80;
	else if (HTSetting.field.BW == BW_160)
		re->mhz = 160;

	re->is_40mhz = (re->mhz == 40);

	if (HTSetting.field.MODE <= MODE_VHT)
		getRate(HTSetting, &DataRate);

	re->rate = (uint32_t)(DataRate * 1000);
}

static void mtk_parse_rateinfo(RT_802_11_MAC_ENTRY *pe,
	struct iwinfo_rate_entry *rx_rate, struct iwinfo_rate_entry *tx_rate)
{
	HTTRANSMIT_SETTING TxRate;
	HTTRANSMIT_SETTING RxRate;

	unsigned int mcs = 0;
	unsigned int nss = 0;

	unsigned int mcs_r = 0;
	unsigned int nss_r = 0;

	TxRate.word = pe->TxRate.word;
	RxRate.word = pe->LastRxRate;

	mcs = TxRate.field.MCS;
	mcs_r = RxRate.field.MCS;

	if (TxRate.field.MODE == MODE_VHT) {
		nss = ((mcs & (0x3 << 4)) >> 4) + 1;
		mcs = mcs & 0xF;
		tx_rate->nss = nss;
	} else {
		mcs = mcs & 0x3f;
		tx_rate->nss = 1;
	}
	tx_rate->mcs = mcs;

	if (RxRate.field.MODE == MODE_VHT) {
		nss_r = ((mcs_r & (0x3 << 4)) >> 4) + 1;
		mcs_r = mcs_r & 0xF;
		rx_rate->nss = nss_r;
	} else {
		rx_rate->nss = 1;
		if (RxRate.field.MODE >= MODE_HTMIX) {
			mcs_r = mcs_r & 0x3f;
		} else if (RxRate.field.MODE == MODE_OFDM) {
			mcs_r = mcs_r & 0xf;
			RxRate.field.MCS = mcs_r;
		} else if (RxRate.field.MODE == MODE_CCK) {
			mcs_r = cck_to_mcs(mcs_r & 0x7);
			RxRate.field.MCS = mcs_r;
		}
	}
	rx_rate->mcs = mcs_r;

	fill_rate_info(TxRate, tx_rate, mcs, nss);
	fill_rate_info(RxRate, rx_rate, mcs_r, nss_r);
}

int mtk_get_assoclist(const char *ifname, char *buf, int *len)
{
	int ret, i;
	int bl = 0;
	struct iwreq wrq = {};
	RT_802_11_MAC_TABLE *table;
	struct iwinfo_assoclist_entry entry;

	table = calloc(1, sizeof(RT_802_11_MAC_TABLE));
	if (!table)
		return -1;

	wrq.u.data.pointer = (caddr_t)table;
	wrq.u.data.length  = sizeof(RT_802_11_MAC_TABLE);
	wrq.u.data.flags = 0;

	ret = mtk_ioctl(ifname, RTPRIV_IOCTL_GET_MAC_TABLE_STRUCT, &wrq);
	if (ret < 0)
	{
		free(table);
		fprintf(stderr, "assoclist ioctl fails\n");
		return -1;
	}

	*len = table->Num * sizeof(struct iwinfo_assoclist_entry);

	for (i = 0; i < table->Num; i++)
	{
		RT_802_11_MAC_ENTRY *pe = &(table->Entry[i]);
		memset(&entry, 0, sizeof(entry));

		memcpy(&entry.mac, &pe->Addr, sizeof(entry.mac));

		entry.signal = pe->AvgRssi1;
		entry.signal_avg = pe->AvgRssi1;
		entry.noise = pe->AvgRssi1 - pe->AvgSnr;
		entry.inactive = pe->ConnectedTime;
//		entry.connected_time = pe->InactiveTime;

		entry.rx_packets = pe->RxPackets;
		entry.tx_packets = pe->TxPackets;
		entry.rx_bytes = pe->RxBytes;
		entry.tx_bytes = pe->TxBytes;

		mtk_parse_rateinfo(pe, &entry.rx_rate, &entry.tx_rate);

		memcpy(&buf[bl], &entry, sizeof(struct iwinfo_assoclist_entry));

		bl += sizeof(struct iwinfo_assoclist_entry);

		*len = bl;
	}

	free(table);
	return 0;
}

int mtk_get_txpwrlist(const char *ifname, char *buf, int *len)
{
	struct iwinfo_txpwrlist_entry entry;
	uint8_t dbm[7] = {0, 8, 11, 14, 17, 19, 20};
	uint16_t mw[7] = {1, 6, 12, 25, 50, 79, 100};
	int i;

	for (i = 0; i < 7; i++)
	{
		entry.dbm = dbm[i];
		entry.mw = mw[i];
		memcpy(&buf[i * sizeof(entry)], &entry, sizeof(entry));
	}

	*len = 7 * sizeof(entry);
	return 0;
}

static void bssid2mac(char *macstr, unsigned char *mac)
{
	unsigned int iMac[6];
	int i;
	sscanf(macstr, "%02X:%02X:%02X:%02X:%02X:%02X", &iMac[0], &iMac[1], &iMac[2], &iMac[3], &iMac[4], &iMac[5]);
	for (i = 0; i < 6; i++)
		mac[i] = (unsigned char)iMac[i];
}

static void parse_security(char *sec, struct iwinfo_crypto_entry *enc)
{
	memset(enc, 0, sizeof(struct iwinfo_crypto_entry));
	enc->enabled = 0;
	if (strstr(sec, "WPA3"))
	{
		enc->enabled = 1;
		if (strstr(sec, "WPA2PSKWPA3PSK"))
			enc->wpa_version = 5;
		else if (strstr(sec, "WPA3PSK"))
			enc->wpa_version = 4;
		enc->auth_suites |= IWINFO_KMGMT_SAE;

		if (strstr(sec, "GCMP"))
			enc->pair_ciphers |= IWINFO_CIPHER_GCMP;
		if (strstr(sec, "AES"))
			enc->pair_ciphers |= IWINFO_CIPHER_CCMP;

		enc->group_ciphers = enc->pair_ciphers;
	}
	else if (strstr(sec, "WPA"))
	{
		enc->enabled = 1;
		if (strstr(sec, "WPAPSKWPA2PSK"))
			enc->wpa_version = 3;
		else if (strstr(sec, "WPA2PSK"))
			enc->wpa_version = 2;
		else if (strstr(sec, "WPAPSK"))
			enc->wpa_version = 1;

		enc->auth_suites |= IWINFO_KMGMT_PSK;
		if (strstr(sec, "AES"))
			enc->pair_ciphers |= IWINFO_CIPHER_CCMP;
		if (strstr(sec, "TKIP"))
			enc->pair_ciphers |= IWINFO_CIPHER_TKIP;
		if (strstr(sec, "TKIPAES"))
			enc->pair_ciphers |= IWINFO_CIPHER_TKIP & IWINFO_CIPHER_CCMP;

		enc->group_ciphers = enc->pair_ciphers;
	}
	else if (strstr(sec, "OWE"))
	{
		enc->enabled = 1;
		if (strstr(sec, "OWE"))
			enc->wpa_version = 4;

		enc->auth_suites |= IWINFO_KMGMT_OWE;
	}
	else if (strstr(sec, "WEP"))
	{
		enc->enabled = 1;
		enc->auth_algs |= IWINFO_AUTH_OPEN | IWINFO_AUTH_SHARED;
		enc->pair_ciphers |= IWINFO_CIPHER_WEP104 | IWINFO_CIPHER_WEP40;
		enc->auth_suites |= IWINFO_KMGMT_NONE;
		enc->group_ciphers = enc->pair_ciphers;
	}
}

int rtrim(char *s)
{
	int i;

	i = strlen(s) - 1;
	while ((s[i] == ' ' || s[i] == '\t') && i >= 0)
	{
		i--;
	};
	s[i + 1] = '\0';
	return i + 1;
}

static void fill_find_entry(char *sp, struct iwinfo_scanlist_entry *e)
{
	char site_channel[4];
	char site_ssid[33];
	char site_bssid[20];
	char site_security[23];
	char site_rssi[5];
	char site_signal[9];
	int ssid_len;

	sp += 4; // skip No
	memcpy(site_channel, sp, 4);
	memcpy(site_ssid, sp + 4, 33);
	memcpy(site_bssid, sp + 37, 20);
	memcpy(site_security, sp + 57, 23);
	memcpy(site_rssi, sp + 80, 5);
	memcpy(site_signal, sp + 85, 9);

	rtrim(site_bssid);
	rtrim(site_channel);
	rtrim(site_security);
	rtrim(site_rssi);
	rtrim(site_signal);

	e->channel = atoi(site_channel);
	bssid2mac((char *)site_bssid, (unsigned char *)e->mac);
	/* Mode */
	e->mode = IWINFO_OPMODE_MASTER;
	//e->crypto.enable = 0;
	parse_security((char *)site_security, &e->crypto);

	int quality = atoi(site_signal);
	int rssi = atoi(site_rssi);
	/*int8_t rssi;
	rssi = (quality * 127 / 100) - 127;

	if (quality < 1)
	{
		rssi = -127;
	}*/

	e->signal = rssi;
	e->quality = quality;
	e->quality_max = 100;

	ssid_len = rtrim(site_ssid);
//	if (!strlen(site_ssid))
//	{
//		strcpy(site_ssid, "*hidden*");
//		len = 8;
//	}
	memcpy(e->ssid, site_ssid, ssid_len);
}

static char *next_line(char *sp) {
	while (*sp != '\n' && *sp != '\0')
		++sp;
	if (*sp == '\n')
		++sp; // skip \n
	return sp;
}

int mtk_get_scanlist(const char *ifname, char *buf, int *len)
{
	struct iwreq wrq;
	char data[8192];
	char cmd[128];
	char *sp, *end;
	int line_len, i;

	int is5g = is_5g(ifname);

	sprintf(cmd, "iwpriv %s set SiteSurvey=", ifname);
	system(cmd);

	sleep(5);

	memset(data, 0, sizeof(data));
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;

	if (mtk_ioctl(ifname, RTPRIV_IOCTL_GSITESURVEY, &wrq) >= 0)
	{
		struct iwinfo_scanlist_entry e;
		// No  Ch  SSID                             BSSID               Security               Siganl(%)W-Mode  ExtCH  NT SSID_Len WPS DPID BcnRept
		line_len = 4 + 33 + 20 + 23 + 5 + 9 + 7 + 7 + 3 + 4 + 5 + 10; // +WPS DPID
		if (wrq.u.data.length < line_len + 5 + 10)
			return -1;
		sp = wrq.u.data.pointer;
		for (i = 0; i < 3; ++i) {
			// skip \n+'Total=xxxx'+\n+HEADER+\n
			sp = next_line(sp);
		}
		end = sp + strlen(sp);
		i = 0;
		while (*sp >= '0' && end > sp)
		{
			memset(&e, 0, sizeof(struct iwinfo_scanlist_entry));

			fill_find_entry(sp, &e);
			if ((e.channel < 34) ^ is5g) {
				memcpy(&buf[i * sizeof(struct iwinfo_scanlist_entry)], &e, sizeof(struct iwinfo_scanlist_entry));
				i++;
			}
			sp += line_len;
			sp = next_line(sp);
		}
		*len = i * sizeof(struct iwinfo_scanlist_entry);
		return 0;
	}
	return -1;
}

#define MTK_MAX_CH_2G 13
static const uint16_t CH5G[]={
	/* 802.11 UNI / HyperLan 2 */
	36, 40, 44, 48, 52, 56, 60, 64, //8

	/* 802.11 HyperLan 2 */
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, //10

	/* 802.11 UNII */
	140, 144, 149, 153, 157, 161, 165, 169, 173, 177
};

int mtk_get_freqlist(const char *ifname, char *buf, int *len)
{
	struct iwinfo_freqlist_entry entry;
	int i, bl;
	bl = 0;

	if (is_5g(ifname)) {
		for (i=0; i<ARRAY_SIZE(CH5G); ++i) {
			entry.band = IWINFO_BAND_5;
			entry.mhz = 5000 + 5 * CH5G[i];
			entry.channel =  CH5G[i];
			entry.restricted = 0;

			memcpy(&buf[bl], &entry, sizeof(struct iwinfo_freqlist_entry));
			bl += sizeof(struct iwinfo_freqlist_entry);
		}
	} else {
		for (i = 0; i < MTK_MAX_CH_2G; i++) {
			entry.band = IWINFO_BAND_24;
			entry.mhz = 2412 + 5 * i;
			entry.channel = i + 1;
			entry.restricted = 0;

			memcpy(&buf[bl], &entry, sizeof(struct iwinfo_freqlist_entry));
			bl += sizeof(struct iwinfo_freqlist_entry);
		}
	}

	*len = bl;
	return 0;
}

int mtk_get_country(const char *ifname, char *buf)
{
	char data[4] = {0};
	struct iwreq wrq;

	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = &data;
	wrq.u.data.flags = OID_802_11_GET_COUNTRY_CODE;

	if (mtk_ioctl(ifname, RT_PRIV_IOCTL, &wrq) >= 0)
	{
		memcpy(buf, data, 2);
		return 0;
	}
	return -1;
}

int mtk_get_countrylist(const char *ifname, char *buf, int *len)
{
	int count = sizeof(mtk_country_codes)/sizeof(mtk_country_codes[0]);
	struct iwinfo_country_entry *c = (struct iwinfo_country_entry *)buf;

	for (int i=0; i<count; i++) {
		c->iso3166 = mtk_country_codes[i][0]<<8 | mtk_country_codes[i][1];
		snprintf(c->ccode, sizeof(c->ccode), "%s", mtk_country_codes[i]);
		c++;
	}

	*len = (count * sizeof(struct iwinfo_country_entry));
	return 0;
}

int mtk_get_hwmodelist(const char *ifname, int *buf)
{
	char chans[IWINFO_BUFSIZE] = { 0 };
	struct iwinfo_freqlist_entry *e = NULL;
	int len = 0;

	*buf = 0;

	if (!mtk_get_freqlist(ifname, chans, &len) )
	{
		for (e = (struct iwinfo_freqlist_entry *)chans; e->channel; e++ )
		{
			if (e->channel <= 14 ) //2.4Ghz
			{
				*buf = (IWINFO_80211_B | IWINFO_80211_G | IWINFO_80211_N);
			}
			else //5Ghz
			{
				*buf = (IWINFO_80211_A | IWINFO_80211_N | IWINFO_80211_AC);
			}
		}

		return 0;
	}

	return -1;
}

int mtk_get_htmodelist(const char *ifname, int *buf)
{
	char chans[IWINFO_BUFSIZE] = { 0 };
	struct iwinfo_freqlist_entry *e = NULL;
	int len = 0;

	*buf = 0;

	if (!mtk_get_freqlist(ifname, chans, &len) )
	{
		for (e = (struct iwinfo_freqlist_entry *)chans; e->channel; e++ )
		{
			if (e->channel <= 14 ) //2.4Ghz
			{
				*buf = (IWINFO_HTMODE_HT20 | IWINFO_HTMODE_HT40);
			}
			else //5Ghz
			{
				*buf = (IWINFO_HTMODE_HT20 | IWINFO_HTMODE_HT40 | IWINFO_HTMODE_VHT20
				| IWINFO_HTMODE_VHT40 | IWINFO_HTMODE_VHT80);
			}
		}

		return 0;
	}

	return -1;
}

int mtk_get_mbssid_support(const char *ifname, int *buf)
{
	char data[10];

	if (mtk_oid_ioctl(ifname, RT_OID_VERSION_INFO, data, sizeof(data)) < 0)
		return -1;
	*buf = 1;
	return 0;
}

int mtk_get_hardware_id_from_l1profile(const char* chip, struct iwinfo_hardware_id *id)
{
	if (!strcmp(chip, "MT7615")) {
		id->vendor_id = 0x14c3;
		id->device_id = 0x7615;
		id->subsystem_vendor_id = id->vendor_id;
		id->subsystem_device_id = id->device_id;
	} else if (!strcmp(chip, "MT7915")) {
		id->vendor_id = 0x14c3;
		id->device_id = 0x7915;
		id->subsystem_vendor_id = id->vendor_id;
		id->subsystem_device_id = id->device_id;
	} else {
		return -1;
	}

	return 0;
}

int mtk_get_id_from_l1profile(struct iwinfo_hardware_id *id)
{
	char buf[16] = {0};

	/* INDEX1 */
	if (mtk_get_l1profile_attr("INDEX1", buf, sizeof(buf)) != 0)
		return -1;

	/* INDEX0 */
	if (mtk_get_l1profile_attr("INDEX0", buf, sizeof(buf)) != 0)
		return -1;

	return (mtk_get_hardware_id_from_l1profile(buf, id));
}

int mtk_get_hardware_id(const char *ifname, char *buf)
{
	struct iwinfo_hardware_id *id = (struct iwinfo_hardware_id *)buf;
	int ret = 0;

	memset(id, 0, sizeof(*id));

	ret = iwinfo_hardware_id_from_mtd(id);

	if (ret != 0)
		ret = mtk_get_id_from_l1profile(id);

	return ret;
}

static const struct iwinfo_hardware_entry *
mtk_get_hardware_entry(const char *ifname)
{
	struct iwinfo_hardware_id id;

	if (mtk_get_hardware_id(ifname, (char *)&id))
		return NULL;

	return iwinfo_hardware(&id);
}

int mtk_get_hardware_name(const char *ifname, char *buf)
{
	const struct iwinfo_hardware_entry *hw;

	if (!(hw = mtk_get_hardware_entry(ifname)))
		sprintf(buf, "%s", "MediaTek");
	else
		sprintf(buf, "%s %s", hw->vendor_name, hw->device_name);

	return 0;
}

int mtk_get_txpower_offset(const char *ifname, int *buf)
{
	const struct iwinfo_hardware_entry *hw;

	if (!(hw = mtk_get_hardware_entry(ifname)))
		return -1;

	*buf = hw->txpower_offset;
	return 0;
}

int mtk_get_frequency_offset(const char *ifname, int *buf)
{
	return mtk_get_frequency(ifname, buf);
}
