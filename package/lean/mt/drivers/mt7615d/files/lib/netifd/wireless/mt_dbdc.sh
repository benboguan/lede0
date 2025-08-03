#!/bin/sh
#
# Copyright (c) 2013-2015 D-Team Technology Co.,Ltd. ShenZhen
# Copyright (c) 2005-2015, lintel <lintel.huang@gmail.com>
# Copyright (c) 2013, Hoowa <hoowa.sun@gmail.com>
# Copyright (c) 2015-2017, GuoGuo <gch981213@gmail.com>
# Copyright (c) 2020,2023, jjm2473 <jjm2473@gmail.com>
# Copyright (c) 2022,2023, nanchuci <nanchuci023@gmail.com>
#
# 	netifd config script for MT7615/MT7915/MT798X DBDC mode.
#
# 	嘿，对着屏幕的哥们,为了表示对原作者辛苦工作的尊重，任何引用跟借用都不允许你抹去所有作者的信息,请保留这段话。
#
. /lib/netifd/netifd-wireless.sh

init_wireless_driver "$@"

#Default configurations
MTWIFI_PROFILE_DIR="/tmp/profiles/"
MTWIFI_PROFILE_PATH=""
MTWIFI_CMD_PATH=""
MTWIFI_CMD_OPATH=""
APCLI_IF=""
WIFI_OP_LOCK=$MTWIFI_PROFILE_DIR"mt_dbdc.lock"
MTWIFI_IFPREFIX=""
MTWIFI_DEF_BAND=""
MTWIFI_FORCE_HT=0
MTWIFI_WDS_MAX_BSSID=4
MTWIFI_DEF_MAX_BSSID=8

mt_cmd() {
	echo "$@" >> $MTWIFI_CMD_PATH
	#eval $@
}

#读取device相关设置项并写入json
drv_mt_dbdc_init_device_config() {
	config_add_string path channel hwmode htmode country 'macaddr:macaddr' twt
	config_add_string txburst cell_density
	config_add_string distance
	config_add_int beacon_int chanbw frag rts dtim_period vendor_vht mu_beamformer whnat
	config_add_int rxantenna txantenna antenna_gain txpower min_tx_power noscan
	config_add_int num_global_macaddr multiple_bssid legacy_rates
	config_add_boolean greenap diversity noscan ht_coex acs_exclude_dfs background_radar
	config_add_int powersave doth
	config_add_int maxassoc
	config_add_boolean hidessid bndstrg isolate dfs
	config_add_array channels
	config_add_array scan_list
}

#读取iface相关设置项并写入json
drv_mt_dbdc_init_iface_config() {
	config_add_boolean disabled
	config_add_string mode ifname 'macaddr:macaddr' bssid 'ssid:string' encryption
	config_add_string auth_server auth_port auth_secret acct_secret own_ip_addr own_radius_port
	config_add_boolean hidden isolate isolate_mb br_isolate_mode ieee80211k ieee80211v ieee80211r
	config_add_boolean powersave enable coloring ldpc lofdm
	config_add_string key key1 key2 key3 key4
	config_add_string wds_bridge wps_pushbutton pin mesh_id mapmode
	config_add_string macfilter 'macfile:file' nasid mobility_domain r1_key_holder reassociation_deadline ft_over_ds
	config_add_array 'maclist:list(macaddr)' r0kh r1kh

	config_add_boolean wds wmm wnm_sleep_mode bss_transition mbo rrm_neighbor_report rrm_beacon_report ft_psk_generate_local pmk_r1_push
	config_add_int short_preamble wpa_group_rekey rsn_preauth
	config_add_int max_listen_int ieee80211w time_advertisement 'port:port'
	config_add_int disassoc_low_ack kicklow assocthres
	config_add_string wdsenctype wdskey wdsphymode macaddr time_zone
	config_add_int wdsen
	config_add_int start_disabled
}

get_wep_key_type() {
	local KeyLen=$(expr length "$1")
	if [ $KeyLen -eq 10 ] || [ $KeyLen -eq 26 ] || [ $KeyLen -eq 32 ]; then
		echo 0
	else
		echo 1
	fi	
}

mt_dbdc_ap_vif_pre_config() {
	local name="$1"

	json_select config
	json_get_vars disabled encryption auth_secret acct_secret auth_server auth_port acct_server \
		acct_port key key1 key2 key3 key4 wmm own_ip_addr own_radius_port macaddr short_preamble wpa_group_rekey \
		ssid mode wps_pushbutton pin pbc isolate hidden disassoc_low_ack kicklow assocthres rsn_preauth \
		ieee80211k ieee80211v ieee80211r ieee80211w macfilter nasid mobility_domain r1_key_holder reassociation_deadline r0kh r1kh \
		ft_over_ds ft_psk_generate_local pmk_r1_push rrm_neighbor_report rrm_beacon_report wnm_sleep_mode bss_transition
	json_get_values maclist maclist
	set_default wmm 1
	set_default isolate 0
	set_default short_preamble 1
	set_default wpa_group_rekey 3600
	set_default disassoc_low_ack 0
	set_default kicklow 0
	set_default assocthres 0
	set_default ieee80211k 0
	set_default ieee80211v 0
	set_default ieee80211r 0
	set_default auth_port 1812
	set_default acct_port 1813
	json_select ..

	[[ "$disabled" = "1" ]] && return
	[ ${ApBssidNum} -gt ${MTWIFI_DEF_MAX_BSSID} ] && return 

	echo "Generating ap config for interface ra${MTWIFI_IFPREFIX}${ApBssidNum}"
	ifname="ra${MTWIFI_IFPREFIX}${ApBssidNum}"

	json_add_object data
	json_add_string ifname "$ifname"
	json_close_object

	#MAC过滤方式和自定义MAC地址相关设定 由于编号问题......扔在这了......
	ra_maclist="${maclist// /;};"
	case "$macfilter" in
	allow)
		echo "Interface ${ifname} has Macfilter.Allow list:${ra_maclist}"
		echo "AccessPolicy${ApBssidNum}=1" >> $MTWIFI_PROFILE_PATH
		echo "AccessControlList${ApBssidNum}=${ra_maclist}" >> $MTWIFI_PROFILE_PATH
	;;
	deny)
		echo "Interface ${ifname} has Macfilter.Deny list:${ra_maclist}"
		echo "AccessPolicy${ApBssidNum}=2" >> $MTWIFI_PROFILE_PATH
		echo "AccessControlList${ApBssidNum}=${ra_maclist}" >> $MTWIFI_PROFILE_PATH
	;;
	esac
	if [ "$ApBssidNum" == "0" ]; then
		echo "MacAddress=${macaddr}" >> $MTWIFI_PROFILE_PATH
	else
		echo "MacAddress${ApBssidNum}=${macaddr}" >> $MTWIFI_PROFILE_PATH
	fi

	let ApBssidNum+=1
	echo "SSID${ApBssidNum}=${ssid}" >> $MTWIFI_PROFILE_PATH #SSID
	case "$encryption" in #加密方式
	wpa*|psk*|WPA*|sae*|*SAE*|owe*|*8021x*|*eap*|Mixed|mixed)
		local enc
		local crypto
		case "$encryption" in
			Mixed|mixed|psk+psk2|psk-mixed*)
				enc=WPAPSKWPA2PSK
			;;
			psk2*)
				enc=WPA2PSK
			;;
			psk*)
				enc=WPAPSK
			;;
			SAE*|psk3|sae)
				enc=WPA3PSK
			;;
			psk2+psk3|psk3-mixed|sae-mixed)
				enc=WPA2PSKWPA3PSK
			;;
			8021x*|eap|wpa)
				enc=WPA
			;;
			8021x*|eap2*|wpa2)
				enc=WPA2
			;;
			8021x*|eap+eap2|wpa-mixed)
				enc=WPA1WPA2
			;;
			8021x*|wpa3) #在mt_wifi驱动中，WPA3也就是SHA256的WPA2，所以加密模式实际为WPA2-EAP/SHA256。
				enc=WPA3
			;;
			8021x*|eap3-mixed|wpa3-mixed) #在mt_wifi驱动中，WPA3也就是SHA256的WPA2，所以选择WPA2MIX。
				enc=WPA2MIX
			;;
			8021x*|eap192*|wpa3-192)
				enc=WPA3-192
			;;
			OWE*|owe)
				enc=OWE
			;;
		esac
			crypto="AES"
		case "$encryption" in
			*tkipaes*|*tkip+ccmp*|*tkip+aes*|*aes+tkip*|*ccmp+tkip*)
				crypto="TKIPAES"
			;;
			*gcmp256*)
				crypto="GCMP256"
			;;
			*ccmp256*)
				crypto="CCMP256"
			;;
			*gcmp*|*gcmp128*)
				crypto="GCMP"
			;;
			*aes*|*ccmp*|*ccmp128*)
				crypto="AES"
			;;
			*tkip*) 
				crypto="TKIP"
				echo "Warning!!! TKIP is not support in 802.11n 40Mhz!!!"
			;;
		esac
			if [ "$encryption" == "wpa3-192" ]; then
				ApAuthMode="${ApAuthMode}${enc};"
				ApEncrypType="${ApEncrypType}GCMP256;"
			else
				ApAuthMode="${ApAuthMode}${enc};"
				ApEncrypType="${ApEncrypType}${crypto};"
			fi
			ApDefKId="${ApDefKId}2;"
			echo "WPAPSK${ApBssidNum}=${key}" >> $MTWIFI_PROFILE_PATH
			echo "RADIUS_Key${ApBssidNum}=${auth_secret:-0}" >> $MTWIFI_PROFILE_PATH
	;;
	WEP|wep|wep-open|wep-shared)
		if [ "$encryption" == "wep-shared" ]; then
			ApAuthMode="${ApAuthMode}SHARED;"
		else
			ApAuthMode="${ApAuthMode}OPEN;"
		fi
		ApEncrypType="${ApEncrypType}WEP;"
		K1Tp=$(get_wep_key_type "$key1")
		K2Tp=$(get_wep_key_type "$key2")
		K3Tp=$(get_wep_key_type "$key3")
		K4Tp=$(get_wep_key_type "$key4")

		[ $K1Tp -eq 1 ] && key1=$(echo $key1 | cut -d ':' -f 2- )
		[ $K2Tp -eq 1 ] && key2=$(echo $key2 | cut -d ':' -f 2- )
		[ $K3Tp -eq 1 ] && key3=$(echo $key3 | cut -d ':' -f 2- )
		[ $K4Tp -eq 1 ] && key4=$(echo $key4 | cut -d ':' -f 2- )
		echo "Key1Str${ApBssidNum}=${key1}" >> $MTWIFI_PROFILE_PATH
		echo "Key2Str${ApBssidNum}=${key2}" >> $MTWIFI_PROFILE_PATH
		echo "Key3Str${ApBssidNum}=${key3}" >> $MTWIFI_PROFILE_PATH
		echo "Key4Str${ApBssidNum}=${key4}" >> $MTWIFI_PROFILE_PATH
		ApDefKId="${ApDefKId}${key};"
	;;
	none|open)
		ApAuthMode="${ApAuthMode}OPEN;"
		ApEncrypType="${ApEncrypType}NONE;"
		ApDefKId="${ApDefKId}1;"
	;;
	esac
	if [ "$encryption" == "open" -o "$encryption" == "owe" ]; then
		ApRekeyMethod="${ApRekeyMethod}DISABLE;"
	else
		ApRekeyMethod="${ApRekeyMethod}TIME;"
	fi

	if [ "$encryption" == "wpa" -o "$encryption" == "wpa-mixed" -o "$encryption" == "wpa2" -o "$encryption" == "wpa3" -o "$encryption" == "wpa3-mixed" -o "$encryption" == "wpa3-192" ]; then
		echo "NasId${ApBssidNum}=${nasid}" >> $MTWIFI_PROFILE_PATH
	else
		echo "FtR0khId${ApBssidNum}=${nasid}" >> $MTWIFI_PROFILE_PATH
	fi

	ApK1Tp="${ApK1Tp}${K1Tp:-0};"
	ApK2Tp="${ApK2Tp}${K2Tp:-0};"
	ApK3Tp="${ApK3Tp}${K3Tp:-0};"
	ApK4Tp="${ApK4Tp}${K4Tp:-0};"
	ApHideESSID="${ApHideESSID}${hidden:-0};"
	ApWmmCapable="${ApWmmCapable}${wmm};"
	ApRADIUSServer="${ApRADIUSServer}${auth_server:-0};"
	ApRADIUSPort="${ApRADIUSPort}${auth_port};"
	ApRADIUSAcctServer="${ApRADIUSAcctServer}${acct_server};"
	ApRADIUSAcctPort="${ApRADIUSAcctPort}${acct_port};"
	ApRADIUSAcctKey="${ApRADIUSAcctKey}${acct_secret:-0};"
	ApPreAuth="${ApPreAuth}${rsn_preauth:-0};"
	ApRRMEnable="${ApRRMEnable}${ieee80211k};"
	ApFtSupport="${ApFtSupport}${ieee80211r};"
	ApNoForwarding="${ApNoForwarding}${isolate};"
	ApRekeyInterval="${ApRekeyInterval}${wpa_group_rekey};"
	ApFtOtd="${ApFtOtd}${ft_over_ds:-0};"
	ApFtOnly="${ApFtOnly}${ft_psk_generate_local:-0};"
	ApFtRic="${ApFtRic}${pmk_r1_push:-0};"
	echo "FtMdId${ApBssidNum}=${mobility_domain}" >> $MTWIFI_PROFILE_PATH
	echo "FtR1khId${ApBssidNum}=${r1_key_holder}" >> $MTWIFI_PROFILE_PATH
	echo "AssocDeadLine${ApBssidNum}=${reassociation_deadline}" >> $MTWIFI_PROFILE_PATH

	mt_cmd ifconfig $ifname up
	mt_cmd echo "Interface $ifname now up."
	if [ "$ieee80211w" == "1" ] || [ "$encryption" == "sae-mixed" -o "$encryption" == "wpa3-mixed" ]; then
		ApPMFMFPC="${ApPMFMFPC}${PMFMFPC:-1};"
		ApPMFMFPR="${ApPMFMFPR}${PMFMFPR:-0};"
	elif [ "$ieee80211w" == "2" ] || [ "$encryption" == "sae" -o "$encryption" == "owe" -o "$encryption" == "wpa3" -o "$encryption" == "wpa3-192" ]; then
		ApPMFMFPC="${ApPMFMFPC}${PMFMFPC:-1};"
		ApPMFMFPR="${ApPMFMFPR}${PMFMFPR:-1};"
	else
		ApPMFMFPC="${ApPMFMFPC}${PMFMFPC:-0};"
		ApPMFMFPR="${ApPMFMFPR}${PMFMFPR:-0};"
	fi
	# if [ "$wps" = "pbc" -o \( "$wps" = "pin" -a "$encryption" != "none" \) ]; then
	if [ "$wps_pushbutton" == "1" ] && [ "$encryption" != "none" ]; then
		mt_cmd echo "Enable WPS PIN for ${ifname}."
		mt_cmd iwpriv $ifname set WscConfMode=4
		mt_cmd iwpriv $ifname set WscConfStatus=2
		mt_cmd iwpriv $ifname set WscMode=1
		mt_cmd iwpriv $ifname set WscGetConf=1
		mt_cmd iwpriv $ifname set WscGenPinCode=1
		mt_cmd iwpriv $ifname set WscV2Support=1
		mt_cmd iwpriv $ifname set WscPinCode=$pin
	elif [ "$wps_pushbutton" == "2" ] && [ "$encryption" != "none" ]; then
		mt_cmd echo "Enable WPS PBC for ${ifname}."
		mt_cmd iwpriv $ifname set WscConfMode=4
		mt_cmd iwpriv $ifname set WscConfStatus=2
		mt_cmd iwpriv $ifname set WscMode=2
		mt_cmd iwpriv $ifname set WscGetConf=1
		mt_cmd iwpriv $ifname set WscV2Support=1
	else
		mt_cmd echo "Disabled WPS for ${ifname}."
		mt_cmd iwpriv $ifname set WscConfMode=0
	fi
	mt_cmd echo "Other settings for ${ifname}."
	[ -n "$disassoc_low_ack" ] && [ "$disassoc_low_ack" != "0" ] && {
		mt_cmd iwpriv $ifname set KickStaRssiLow=$kicklow
		mt_cmd iwpriv $ifname set AssocReqRssiThres=$assocthres
	}

	# PMF(802.11W) should be disabled if you want your device to support both iPhone and Android STAs
	[ -n "$ieee80211r" ]  && [ "$ieee80211r" != "0" ] && {
		mt_cmd iwpriv $ifname set ftenable=1
		mt_cmd iwpriv $ifname set PMFMFPC=0
		mt_cmd iwpriv $ifname set PMFMFPR=0
	}
	[ -n "$ieee80211k" ] && [ "$ieee80211k" != "0" ] && mt_cmd iwpriv $ifname set rrmenable=1
	# [ -n "$ieee80211v" ] && [ "$ieee80211v" != "0" ] && mt_cmd iwpriv $ifname set wnmenable=1
	# [ -n "$ieee80211w" ] && [ "$ieee80211w" != "0" ] && mt_cmd iwpriv $ifname set pmfenable=1
}

mt_dbdc_wds_vif_pre_config() {
	local name="$1"

	json_select config
	json_get_vars disabled encryption key key1 key2 key3 key4 mode bssid wdsen wdsenctype wdskey wdswepid wdsphymode wdstxmcs
	set_default wdsen 0
	set_default wdsphymode "VHT"
	json_select ..

	[[ "$disabled" = "1" ]] && return
	[ ${WDSBssidNum} -gt ${MTWIFI_WDS_MAX_BSSID} ] && return

	echo "Generating WDS config for interface wds${MTWIFI_IFPREFIX}${WDSBssidNum}"
	ifname="wds${MTWIFI_IFPREFIX}${WDSBssidNum}"

	json_add_object data
	json_add_string ifname "$ifname"
	json_close_object

	case "$encryption" in #加密方式
	psk*|psk2*|psk3*|sae*|*SAE*)
		local enc
		local crypto
		case "$encryption" in
			psk2*)
				enc=WPA2PSK
			;;
			psk*)
				enc=WPAPSK
			;;
			SAE*|psk3*|sae)
				enc=WPA3PSK
			;;
		esac
			crypto="AES"
		case "$encryption" in
			*tkipaes*|*tkip+ccmp*|*tkip+aes*|*aes+tkip*|*ccmp+tkip*)
				crypto="TKIPAES"
			;;
			*aes*|*ccmp*|*ccmp128*)
				crypto="AES"
			;;
			*tkip*)
				crypto="TKIP"
				echo "Warning!!! TKIP is not support in 802.11n 40Mhz!!!"
			;;
		esac
			WdsEncrypType="${WdsEncrypType}${crypto};"
			WdsDefKId="${WdsDefKId}2;"
			# echo "Wds${WDSBssidNum}Key=${key}" >> $MTWIFI_PROFILE_PATH #WDS Key
			;;
	WEP|wep|wep-open|wep-shared)
		WdsEncrypType="${WdsEncrypType}WEP;"
		WdsK1Tp=$(get_wep_key_type "$key1")
		[ $WdsK1Tp -eq 1 ] && key1=$(echo $key1 | cut -d ':' -f 2- )
		WdsDefKId="${WdsDefKId}1;"
		;;
	none|open)
		WdsEncrypType="${WdsEncrypType}NONE;"
		WdsDefKId="${WdsDefKId}1;"
		;;
	esac

	if [ ! -z "$bssid" ] && [ "$wdsen" -eq 3 ] || [ "$wdsen" -eq 4 ]; then
		WdsList="${WdsList}$(echo $bssid | tr 'A-Z' 'a-z');"
	elif [ "$wdsen" -eq 2 ]; then
		WdsList="${WdsList}${bssid};"
	fi

	if [ "$encryption" == "wep-open" -o "$encryption" == "wep-shared" ]; then
		echo "Wds${WDSBssidNum}Key=${key1}" >> $MTWIFI_PROFILE_PATH #WDS Key
	else
		echo "Wds${WDSBssidNum}Key=${key}" >> $MTWIFI_PROFILE_PATH #WDS Key
	fi

	Wdsen="${Wdsen}${wdsen:-0};"
	WdsPhyMode="${WdsPhyMode}${wdsphymode};"

	mt_cmd ifconfig $ifname up
	mt_cmd echo "WDS interface $ifname now up."
	let WDSBssidNum+=1
}

mt_dbdc_sta_vif_pre_config() {
	local name="$1"

	json_select config
	json_get_vars disabled encryption key key1 key2 key3 key4 ssid mode bssid wps_pushbutton pin pbc ieee80211w macaddr
	json_select ..

	[ $stacount -gt 1 ] && {
		return
	}

	[[ "$disabled" = "1" ]] && return

	json_add_object data
	json_add_string ifname "$APCLI_IF"
	json_close_object

	# local ApCliAuthMode=${ApCliAuthMode} ApCliEncrypType=${ApCliEncrypType}
	case "$encryption" in #加密方式
	psk*|sae*|*SAE*|owe*|Mixed|mixed)
		local enc
		local crypto
		case "$encryption" in
			Mixed|mixed|psk+psk2|psk-mixed*)
				enc=WPAPSKWPA2PSK
			;;
			psk2*)
				enc=WPA2PSK
			;;
			psk*)
				enc=WPAPSK
			;;
			SAE*|psk3*|sae)
				enc=WPA3PSK
			;;
			SAE*|psk2+psk3|sae-mixed)
				enc=WPA2PSKWPA3PSK
			;;
			OWE*|owe)
				enc=OWE
			;;
		esac
			crypto="AES"
		case "$encryption" in
			*tkipaes*|*tkip+ccmp*|*tkip+aes*|*aes+tkip*|*ccmp+tkip*)
				crypto="TKIPAES"
			;;
			*gcmp256*)
				crypto="GCMP256"
			;;
			*ccmp256*)
				crypto="CCMP256"
			;;
			*gcmp*|*gcmp128*)
				crypto="GCMP"
			;;
			*aes*|*ccmp*|*ccmp128*)
				crypto="AES"
			;;
			*tkip*)
				crypto="TKIP"
				echo "Warning!!! TKIP is not support in 802.11n 40Mhz!!!"
			;;
		esac
			ApCliAuthMode="${enc}"
			ApCliEncrypType="${crypto}"
			ApCliDefKId="2"
			ApCliWPAPSK="${key}"
	;;
	WEP|wep|wep-open|wep-shared)
		if [[ "$encryption" = "wep-shared" ]]; then
			ApCliAuthMode="SHARED"
		else
			ApCliAuthMode="OPEN"
		fi
		ApCliEncrypType="WEP"
		K1Tp=$(get_wep_key_type "$key1")
		K2Tp=$(get_wep_key_type "$key2")
		K3Tp=$(get_wep_key_type "$key3")
		K4Tp=$(get_wep_key_type "$key4")

		[ $K1Tp -eq 1 ] && key1=$(echo $key1 | cut -d ':' -f 2- )
		[ $K2Tp -eq 1 ] && key2=$(echo $key2 | cut -d ':' -f 2- )
		[ $K3Tp -eq 1 ] && key3=$(echo $key3 | cut -d ':' -f 2- )
		[ $K4Tp -eq 1 ] && key4=$(echo $key4 | cut -d ':' -f 2- )
		ApCliDefKId="${key}"
		;;
	none|open)
		ApCliAuthMode="OPEN"
		ApCliEncrypType="NONE"
		ApCliDefKId="1"
		;;
	esac
	ApCliK1Tp="${K1Tp:-0}"
	ApCliK2Tp="${K2Tp:-0}"
	ApCliK3Tp="${K3Tp:-0}"
	ApCliK4Tp="${K4Tp:-0}"

	mt_cmd ifconfig $APCLI_IF up
	mt_cmd echo "Interface $APCLI_IF now up."
	mt_cmd iwpriv $APCLI_IF set ApCliEnable=1
	mt_cmd iwpriv $APCLI_IF set ApCliAutoConnect=3
	mt_cmd iwpriv $APCLI_IF set ApCliAuthMode=${ApCliAuthMode}
	mt_cmd iwpriv $APCLI_IF set ApCliEncrypType=${ApCliEncrypType}
	if [[ "${ApCliEncrypType}" = "WEP" ]]; then
		mt_cmd iwpriv $APCLI_IF set ApCliDefaultKeyID=${ApCliDefKId}
		# mt_cmd iwpriv $APCLI_IF set ApCliKey1Type=1 # 0:hex, 1:ascii
		mt_cmd iwpriv $APCLI_IF set ApCliKey1Str=${key1##*:}
		# mt_cmd iwpriv $APCLI_IF set ApCliKey2Type=1
		mt_cmd iwpriv $APCLI_IF set ApCliKey2Str=${key2##*:}
		# mt_cmd iwpriv $APCLI_IF set ApCliKey3Type=1
		mt_cmd iwpriv $APCLI_IF set ApCliKey3Str=${key3##*:}
		# mt_cmd iwpriv $APCLI_IF set ApCliKey4Type=1
		mt_cmd iwpriv $APCLI_IF set ApCliKey4Str=${key4##*:}
	elif ! [[ "${ApCliEncrypType}" = "NONE" ]]; then
		mt_cmd iwpriv $APCLI_IF set ApCliWPAPSK=${key}
	fi
	if [[ "${ApCliAuthMode}" = "OWE" ]]; then
		mt_cmd iwpriv $APCLI_IF set ApCliOWETranIe=1
		echo "ApCliOWETranIe=${ApCliOWETranIe:-1}" >> $MTWIFI_PROFILE_PATH
	fi
	[ -z "$bssid" ] || mt_cmd iwpriv $APCLI_IF set ApCliBssid=$(echo $bssid | tr 'A-Z' 'a-z')
	[ -n "$bssid" ] && {
		mt_cmd iwpriv ra${MTWIFI_IFPREFIX}0 set MACRepeaterEn=1
		echo "MACRepeaterEn=1" >> $MTWIFI_PROFILE_PATH
	}
	mt_cmd iwpriv $APCLI_IF set ApCliSsid=${ssid}
	mt_cmd iwpriv $APCLI_IF set ApCliDelPMKIDList=1
	if [ "$wps_pushbutton" == "1" ] && [ "${ApCliAuthMode}" != "none" ]; then
		mt_cmd echo "Enable WPS PIN for ${APCLI_IF}."
		mt_cmd iwpriv $APCLI_IF set WscConfMode=1
		mt_cmd iwpriv $APCLI_IF set WscMode=1
		mt_cmd iwpriv $APCLI_IF show WscPin
		[ -n "$ssid" ] && mt_cmd iwpriv $APCLI_IF set ApCliWscSsid="${ssid}"
		mt_cmd iwpriv $APCLI_IF set WscGetConf=1
		mt_cmd iwpriv $APCLI_IF set WscGenPinCode=1
		mt_cmd iwpriv $APCLI_IF set WscPinCode=$pin
		# echo "ApCliWscSsid=${ssid}" >> $MTWIFI_PROFILE_PATH
	elif [ "$wps_pushbutton" == "2" ] && [ "${ApCliAuthMode}" != "none" ]; then
		mt_cmd echo "Enable WPS PBC for ${APCLI_IF}."
		mt_cmd iwpriv $APCLI_IF set WscConfMode=1
		mt_cmd iwpriv $APCLI_IF set WscMode=2
		mt_cmd iwpriv $APCLI_IF set WscGetConf=1
	else
		mt_cmd echo "Disabled WPS for ${APCLI_IF}."
		mt_cmd iwpriv $APCLI_IF set WscConfMode=0
	fi
	if [[ "$ieee80211w" = "1" ]] || [ "$encryption" == "sae-mixed" ]; then
		ApCliPMFMFPC="${ApCliPMFMFPC:-1}"
		ApCliPMFMFPR="${ApCliPMFMFPR:-0}"
	elif [[ "$ieee80211w" = "2" ]] || [ "$encryption" == "sae" -o "$encryption" == "owe" ]; then
		ApCliPMFMFPC="${ApCliPMFMFPC:-1}"
		ApCliPMFMFPR="${ApCliPMFMFPR:-1}"
	else
		ApCliPMFMFPC="${ApCliPMFMFPC:-0}"
		ApCliPMFMFPR="${ApCliPMFMFPR:-0}"
	fi
	ApCliEnable="${ApCliEnable:-1}"
	ApCliSsid="${ssid}"
	ApCliBssid="$(echo $bssid | tr 'A-Z' 'a-z')"
	let stacount+=1
}

mt_dbdc_vif_post_config() {
	local name="$1"
	json_select config
	json_get_vars disabled
	json_select ..

	json_select data
	json_get_vars ifname
	json_select ..

	[ "$disabled" = "1" -o -z "$ifname" ] && return
	logger -t "mt_dbdc" "wireless_add_vif $name $ifname"
	wireless_add_vif "$name" "$ifname"
}

mt_dbdc_vif_down() {
	phy_name=${1}
	case "$phy_name" in
		rax0)
			for vif in ra0 ra1 ra2 ra3 ra4 ra5 ra6 ra7 wds0 wds1 wds2 wds3 apcli0; do
				[ -d "/sys/class/net/$vif" ] && ifconfig $vif down
			done
		;;
		ra0)
			for vif in rax0 rax1 rax2 rax3 rax4 rax5 rax6 rax7 wdsx0 wdsx1 wdsx2 wdsx3 apclix0; do
				[ -d "/sys/class/net/$vif" ] && ifconfig $vif down
			done
		;;
	esac
}

drv_mt_dbdc_cleanup() {
	return
}

drv_mt_dbdc_teardown() {
	phy_name=${1}
	case "$phy_name" in
		ra0)
			for vif in ra0 ra1 ra2 ra3 ra4 ra5 ra6 ra7 wds0 wds1 wds2 wds3 apcli0; do
				# iwpriv $vif set DisConnectAllSta=1
				[ -d "/sys/class/net/$vif" ] && ifconfig $vif down
			done
		;;
		rax0)
			for vif in rax0 rax1 rax2 rax3 rax4 rax5 rax6 rax7 wdsx0 wdsx1 wdsx2 wdsx3 apclix0; do
				# iwpriv $vif set DisConnectAllSta=1
				[ -d "/sys/class/net/$vif" ] && ifconfig $vif down
			done
		;;
	esac
}

#接口启动
drv_mt_dbdc_setup() {
	json_select config
	json_get_vars main_if phy_name macaddr channel mode hwmode htmode \
		txpower country macfilter maclist greenap \
		diversity frag rts hidden \
		disabled noscan:1 ht_coex #device所有配置项

	json_get_vars \
			ldpc:1 \
			txburst:1 \
			twt:0 \
			doth:0 \
			whnat:1 \
			legacy_rates:0 \
			maxassoc:64 \
			frag:2346 \
			rts:2347 \
			dtim_period:1 \
			distance:0 \
			beacon_int:100 \
			greenfield:0 \
			short_gi_20:1 \
			short_gi_40:1 \
			tx_stbc:1 \
			rx_stbc:3 \
			max_amsdu:1 \
			vendor_vht:1 \
			dsss_cck_40:1
			
	json_get_vars \
			dfs:0 \
			rxldpc:1 \
			short_gi_80:1 \
			short_gi_160:1 \
			tx_stbc_2by1:1 \
			su_beamformer:1 \
			su_beamformee:1 \
			mu_beamformer:1 \
			mu_beamformee:1 \
			vht_txop_ps:1 \
			htc_vht:1 \
			beamformee_antennas:4 \
			beamformer_antennas:4 \
			rx_antenna_pattern:1 \
			tx_antenna_pattern:1 \
			vht_max_a_mpdu_len_exp:7 \
			vht_max_mpdu:11454 \
			rx_stbc:4 \
			vht_link_adapt:3 \
			vht160:2

	json_select ..

	phy_name=${1}
	wireless_set_data phy=${phy_name}
	case "$phy_name" in
		ra0)
			WirelessMode=9
			APCLI_IF="apcli0"
			MTWIFI_IFPREFIX=""
			MTWIFI_DEF_BAND="g"
			MTWIFI_PROFILE_PATH="${MTWIFI_PROFILE_DIR}mt_dbdc_2g.dat"
			MTWIFI_CMD_PATH="${MTWIFI_PROFILE_DIR}mt_dbdc_cmd_2g.sh"
			MTWIFI_CMD_OPATH="${MTWIFI_PROFILE_DIR}mt_dbdc_cmd_5g.sh"
		;;
		rax0)
			WirelessMode=14
			APCLI_IF="apclix0"
			MTWIFI_IFPREFIX="x"
			MTWIFI_DEF_BAND="a"
			MTWIFI_PROFILE_PATH="${MTWIFI_PROFILE_DIR}mt_dbdc_5g.dat"
			MTWIFI_CMD_PATH="${MTWIFI_PROFILE_DIR}mt_dbdc_cmd_5g.sh"
			MTWIFI_CMD_OPATH="${MTWIFI_PROFILE_DIR}mt_dbdc_cmd_2g.sh"
		;;
		*)
			echo "Unknown phy:$phy_name"
			return 1
	esac

#检查配置文件目录是否存在，否则创建目录
	[ ! -d $MTWIFI_PROFILE_DIR ] && mkdir $MTWIFI_PROFILE_DIR
	echo > $MTWIFI_CMD_PATH

	hwmode=${hwmode##11}
	case "$hwmode" in
		a)
			WirelessMode=14
			ITxBfEn=1
			HT_HTC=1
		;;
		g)
			WirelessMode=9
			ITxBfEn=1
			HT_HTC=1
		;;
		*)
			echo "Unknown wireless mode.Use default value:${WirelessMode}"
			hwmode=${MTWIFI_DEF_BAND}
		;;
	esac

#HT默认模式设定
	HT_BW=1  #允许HT40
	HT_CE=1  #允许HT20/40共存
	HT_DisallowTKIP=0 #是否允许TKIP加密
	HT_GI=1 #HT_SHORT_GI
	VHT_SGI=1 #VHT_SHORT_GI
	#HT_MIMOPSMode用于省电模式设置
	#HT_MIMOPSMode=3

#HT/VHT默认模式设定
	VHT_BW=1 #允许VHT
	VHT_DisallowNonVHT=0 #是否禁止非VHT客户端连接，VHT80 only

	[ "$short_gi_20" == "0" -o "$short_gi_40" == "0" ] && HT_GI=0
	[ "$short_gi_80" == "0" -o "$short_gi_160" == "0" ] && VHT_SGI=0

	case "$htmode" in
		HT20 |\
		VHT20)
			HT_BW=0
			VHT_BW=0
		;;
		HT40 |\
		VHT40)
			HT_BW=1
			VHT_BW=0
			VHT_DisallowNonVHT=0
		;;
		VHT80)
			HT_BW=1
			VHT_BW=1
		;;
		VHT160)
			echo "only VHT80 support!!"
			HT_BW=1
			VHT_BW=2
		;;
		*) 
		echo "Unknown HT Mode."
		;;
	esac

#仅HT20以外才需要设置的参数
	[ "$htmode" != "HT20" ] && {
#强制HT40/VHT80
		[[ "$noscan" = "1" ]] && HT_CE=0 && MTWIFI_FORCE_HT=1
#HT HTC
		HT_HTC=1
	}

#TxPower功率设置
	[ "${txpower}" -lt "100" ] && {
		PERCENTAGEenable=1
		txpower=${txpower}
	}
#或者
	[ "${txpower}" -eq "100" ] && {
		PERCENTAGEenable=0
		txpower=100
	}

#BG保护功能设置
	[ "${legacy_rates}" == "0" ] && {
		BGProtection=2
	}
#或者
	[ "${legacy_rates}" == "1" ] && {
		BGProtection=1
	}

#igmp_snooping功能设置
	igmp_snooping="$(uci -q get network.@device[0].igmp_snooping)"

#处理CountryRegion:指定信道
	[ "${country}" == "DB" ] && countryregion_a=7 && countryregion=5
	[ "${country}" == "AE" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "AL" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "DZ" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "AR" ] && countryregion_a=3 && countryregion=1
	[ "${country}" == "AM" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "AU" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "AT" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "AZ" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "BH" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "BY" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "BE" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "BZ" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "BO" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "BR" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "BN" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "BG" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "CA" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "CL" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "CN" ] && countryregion_a=0 && countryregion=1 && RDRegion=CHN
	[ "${country}" == "CO" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "CR" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "HR" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "CY" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "CZ" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "DK" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "DO" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "EC" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "EG" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "SV" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "EE" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "FI" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "FR" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "GE" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "DE" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "GR" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "GT" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "HN" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "HK" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "HU" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "IS" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "IN" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "ID" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "IR" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "IE" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "IL" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "IT" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "JP" ] && countryregion_a=9 && countryregion=1 && RDRegion=JAP
	[ "${country}" == "JO" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "KZ" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "KP" ] && countryregion_a=5 && countryregion=1
	[ "${country}" == "KR" ] && countryregion_a=5 && countryregion=1 && RDRegion=KR
	[ "${country}" == "KW" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "LV" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "LB" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "LI" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "LT" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "LU" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "MO" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "MK" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "MY" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "MX" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "MC" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "MA" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "NL" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "NZ" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "NO" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "OM" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "PK" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "PA" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "PE" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "PH" ] && countryregion_a=4 && countryregion=1
	[ "${country}" == "PL" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "PT" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "PR" ] && countryregion_a=0 && countryregion=0
	[ "${country}" == "QA" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "RO" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "RU" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "SA" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "SG" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "SK" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "SI" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "ZA" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "ES" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "SE" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "CH" ] && countryregion_a=1 && countryregion=1
	[ "${country}" == "SY" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "TW" ] && countryregion_a=3 && countryregion=0
	[ "${country}" == "TH" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "TT" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "TN" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "TR" ] && countryregion_a=2 && countryregion=1
	[ "${country}" == "UA" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "AE" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "GB" ] && countryregion_a=1 && countryregion=1
	# [ "${country}" == "US" ] && countryregion_a=7 && countryregion=5 && RDRegion=FCC
	[ "${country}" == "UY" ] && countryregion_a=5 && countryregion=1
	[ "${country}" == "UZ" ] && countryregion_a=1 && countryregion=0
	[ "${country}" == "VE" ] && countryregion_a=5 && countryregion=1
	[ "${country}" == "VN" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "YE" ] && countryregion_a=0 && countryregion=1
	[ "${country}" == "ZW" ] && countryregion_a=0 && countryregion=1
	# [ "${country}" == "00" ] && countryregion_a=26 && countryregion=5 && RDRegion=CE

#其它相关
	case "$hwmode" in
		a)
			EXTCHA=1
			case "$channel" in
				40|48|56|64|104|112|120|128| \
				136|144|153|161|169|177) EXTCHA=0;;
			esac
			[ "${channel}" == "auto" -o "${channel}" == "0" ] && {
				AutoChannelSelect=3
				channel=0
			}
			[ "${country}" == "US" ] && {
				countryregion_a=7 && RDRegion=FCC
				ACSSKIP="100;104;108;112;116;120;124;128;132;136;140;144;169;173;177"
			}
			[ "${country}" == "CN" ] && {
				countryregion_a=0
				ACSSKIP="100;104;108;112;116;120;124;128;132;136;140;144;169;173;177"
			}
			# ACSSKIP="100;104;108;112;116;120;124;128;132;136;140;144;169;173;177"
		;;
		g)
			EXTCHA=0
			[ "${channel}" != "auto" ] && [ "${channel}" != "0" ] && [ "${channel}" -lt "7" ] && EXTCHA=1
			[ "${channel}" == "auto" -o "${channel}" == "0" ] && {
				AutoChannelSelect=3
				channel=0
				EXTCHA=1
			}
			[ "${country}" == "US" ] && {
				countryregion=5 && RDRegion=FCC
				ACSSKIP="14"
			}
			[ "${country}" == "CN" ] && {
				countryregion=1
				ACSSKIP="14"
			}
			# ACSSKIP="14"
		;;
	esac

#设备配置文件生成
	cat > $MTWIFI_PROFILE_PATH <<EOF
#The word of "Default" must not be removed
Default
BssidNum=${MTWIFI_DEF_MAX_BSSID:-1}
DBDC_MODE=1
AckPolicy=0;0;0;0
#ACSCheckTime=0
AntCtrl=
APACM=0;0;0;0
APAifsn=3;7;1;1
ApCliBcnProt=0
ApCliPMFSHA256=0
ApCliWirelessMode=
APCwmax=6;10;4;3
APCwmin=4;4;3;2
ApMWDS=1
ApCliMWDS=0
ApProbeRspTimes=3
APSDCapable=1
APTxop=0;0;94;47
AssocReqFloodThreshold=64
AuthFloodThreshold=64
AutoChannelSelect=${AutoChannelSelect:-0}
AutoChannelSkipList=${ACSSKIP}
AutoProvisionEn=0
AutoRoaming=0
AvgRssiReq=0
BandDeltaRssi=-12
BandSteering=0
BasicRate=15
BcnProt=0
BeaconPeriod=${beacon_int:-100}
BFBACKOFFenable=0
BGMultiClient=${legacy_rates:-1}
BgndScanSkipCh=
BGProtection=${BGProtection:-0}
BlockCh=
BndStrgAge=600000
BndStrgCheckTime=6000
BndStrgHoldTime=3000
BndStrgRssiDiff=15
BndStrgRssiLow=-86
BndStrgBssIdx=
BSSACM=0;0;0;0
BSSAifsn=3;7;2;2
BSSCwmax=10;10;4;3
BSSCwmin=4;4;3;2
BSSTxop=0;0;94;47
BusyIdleFromCfg=0
BW_Enable=0
BW_Guarantee_Rate=
BW_Maximum_Rate=
BW_Priority=
BW_Root=0
CalCacheApply=0
CarrierDetect=0
Channel=${channel:-0}
ChannelGrp=0:0:0:0
CountryCode=${country:-CN}
CountryRegion=${countryregion:-1}
CountryRegionABand=${countryregion_a:-0}
CP_SUPPORT=2
CSPeriod=6
WirelessMode=${WirelessMode}
DeauthFloodThreshold=64
DebugFlags=0
DfsApplyStopWifi=0
DfsCalibration=0
DfsDedicatedZeroWait=0
DfsEnable=${dfs:-0}
DfsFalseAlarmPrevent=0
DfsLowerLimit=0
DfsOutdoor=0
DFSParamFromConfig=0
DfsRssiHighFromCfg=0
DfsRssiLowFromCfg=0
DfsUpperLimit=0
DfsZeroWait=0
DfsZeroWaitCacTime=255
DFS_R66=0
DisableOLBC=0
DisassocFloodThreshold=64
DLSCapable=0
Dot11vMbssid=0
DppEnable=1
DscpPriMapBss=
DscpPriMapEnable=1
DtimPeriod=${dtim_period:-1}
DyncVgaEnable=1
E2pAccessMode=2
EAPifname=
EapReqFloodThreshold=64
EDCCAEnable=1
EDCCAThreshold=3:127
EDCCACfgMode=0
EnhanceMultiClient=1
EthConvertMode=dongle
EtherTrafficBand=0
Ethifname=
ETxBfEnCond=${ITxBfEn:-1}
ETxBfIncapable=0
ETxBfNoncompress=0
ETxBfTimeout=0
FastRoaming=0
FineAGC=0
FixDfsLimit=0
FixedTxMode=
ForceRoamSupport=
FragThreshold=${frag:-2346}
FreqDelta=0
GreenAP=${greenap:-0}
G_BAND_256QAM=${vendor_vht:-1}
HSCounter=0
HT_AMSDU=1
HT_AutoBA=1
HT_BADecline=0
HT_BAWinSize=64
HT_BSSCoexApCntThr=10
HT_BSSCoexistence=${HT_CE:-1}
HT_BW=${HT_BW:-1}
HT_DisallowTKIP=${HT_DisallowTKIP:-0}
HT_EXTCHA=${EXTCHA:-1}
HT_GI=${HT_GI:-1}
HT_HTC=${HT_HTC:-1}
HT_LDPC=${ldpc:-1}
HT_LinkAdapt=0
HT_MCS=33
HT_MIMOPSMode=3
HT_MpduDensity=4
HT_OpMode=${greenfield:-0}
HT_PROTECT=1
HT_RDG=1
HT_RxStream=2
HT_STBC=${tx_stbc:-1}
HT_TxStream=2
IcapMode=0
idle_timeout_interval=0
IdsEnable=0
IEEE80211H=${doth:-0}
IEEE8021X=0
IgmpSnEnable=${igmp_snooping:-0}
IsICAPFW=0
ITxBfEn=${ITxBfEn:-0}
ITxBfTimeout=0
LinkTestSupport=0
MACRepeaterOuiMode=2
MapEnable=1
MapAccept3Addr=1
MAP_Turnkey=1
MAP_Ext=0
MboSupport=1
MaxStaNum=${maxassoc}
MbssMaxStaNum=${maxassoc:-64}
MBSSWirelessMode=
MUTxRxEnable=${mu_beamformer:-1}
NoForwardingBTNBSSID=0
NoForwardingMBCast=0
NonTxBSSIndex=0
OCE_FD_FRAME=
OCE_FILS_CACHE=0
OCE_FILS_DhcpServer=
OCE_FILS_DhcpServerPort=
OCE_FILS_HLP=0
OCE_FILS_REALMS=
OCE_RNR_SUPPORT=
OCE_SUPPORT=0
OFDMA=1
PcieAspm=0
PERCENTAGEenable=${PERCENTAGEenable:-0}
PhyRateLimit=0
PktAggregate=1
PMFSHA256=0
PMKCachePeriod=10
PowerUpCckOfdm=0:0:0:0:0:0:0
PowerUpHT20=0:0:0:0:0:0:0
PowerUpHT40=0:0:0:0:0:0:0
PowerUpVHT160=0:0:0:0:0:0:0
PowerUpVHT20=0:0:0:0:0:0:0
PowerUpVHT40=0:0:0:0:0:0:0
PowerUpVHT80=0:0:0:0:0:0:0
PPDUTxType=4
PreAntSwitch=1
PreAuthifname=
RadioLinkSelection=0
ProbeReqFloodThreshold=64
RadioOn=1
RDRegion=${RDRegion}
ReassocReqFloodThreshold=64
RED_Enable=1
RegDomain=Global
ResetCounter=1
RTSThreshold=${rts:-2347}
ScsEnable=0
SCSEnable=1
session_timeout_interval=0
quiet_interval=0
radius_acct_authentic=1
acct_interim_interval=0
acct_enable=1
SlotTime=9
ShortSlot=1
SKUenable=0
SkuTableIdx=0
SRDPDEnable=1
SREnable=0
SRMode=0
SRSDEnable=1
SSID=
StationKeepAlive=0
StreamMode=0
StreamModeMac0=
StreamModeMac1=
StreamModeMac2=
StreamModeMac3=
SymRoundFromCfg=0
TGnWifiTest=0
Thermal=100
ThermalRecal=0
TurboRate=0
TxBurst=${txburst:-1}
TxPower=${txpower:-100}
TxRate=0
UAPSDCapable=1
UseVhtRateFor2g=${vendor_vht:-1}
VHT_BW=${VHT_BW:-1}
VHT_BW_SIGNAL=0
VHT_DisallowNonVHT=${VHT_DisallowNonVHT:-0}
VHT_LDPC=${ldpc:-1}
VHT_Sec80_Channel=0
VHT_SGI=${VHT_SGI:-1}
VHT_STBC=${tx_stbc:-1}
VLANID=0
VLANPriority=0
VLANTag=0
VOW_Airtime_Ctrl_En=
VOW_Airtime_Fairness_En=1
VOW_BW_Ctrl=0
VOW_Group_Backlog=
VOW_Group_DWRR_Max_Wait_Time=
VOW_Group_DWRR_Quantum=
VOW_Group_Max_Airtime_Bucket_Size=
VOW_Group_Max_Rate=
VOW_Group_Max_Rate_Bucket_Size=
VOW_Group_Max_Ratio=
VOW_Group_Max_Wait_Time=
VOW_Group_Min_Airtime_Bucket_Size=
VOW_Group_Min_Rate=
VOW_Group_Min_Rate_Bucket_Size=
VOW_Group_Min_Ratio=
VOW_Rate_Ctrl_En=
VOW_Refill_Period=
VOW_RX_En=1
VOW_Sta_BE_DWRR_Quantum=
VOW_Sta_BK_DWRR_Quantum=
VOW_Sta_DWRR_Max_Wait_Time=
VOW_Sta_VI_DWRR_Quantum=
VOW_Sta_VO_DWRR_Quantum=
VOW_WATF_Enable=
VOW_WATF_MAC_LV0=
VOW_WATF_MAC_LV1=
VOW_WATF_MAC_LV2=
VOW_WATF_MAC_LV3=
VOW_WATF_Q_LV0=
VOW_WATF_Q_LV1=
VOW_WATF_Q_LV2=
VOW_WATF_Q_LV3=
VOW_WMM_Search_Rule_Band0=
VOW_WMM_Search_Rule_Band1=
WapiAsCertPath=
WapiAsIpAddr=
WapiAsPort=
Wapiifname=
WapiPsk10=
WapiPsk11=
WapiPsk12=
WapiPsk13=
WapiPsk14=
WapiPsk15=
WapiPsk16=
WapiPsk1=
WapiPsk2=
WapiPsk3=
WapiPsk4=
WapiPsk5=
WapiPsk6=
WapiPsk7=
WapiPsk8=
WapiPsk9=
WapiPskType=
WapiUserCertPath=
MeshAutoLink=0
MeshAuthMode=
MeshEncrypType=
MeshDefaultkey=0
MeshWEPKEY=
MeshWPAKEY=
MeshId=
HSCounter=0
WCNTest=0
WdsTxMcs=33
WHNAT=${whnat:-1}
WiFiTest=0
WirelessEvent=1
WscConfMethods=238c
WscConfMode=0
WscConfStatus=2
WscDeviceName=OpenWrt WiFi
WscManufacturer=OpenWrt
WscModelName=OpenWrt Wireless Router
WscVendorPinCode=
WscModelNumber=
WscSerialNumber=
WscV2Support=0
ApCliUAPSDCapable=1
QuickChannelSwitch=1
WDS_VLANID=
SRMeshUlMode=1
WdsMac=
EOF

#接口配置生成
#	AP模式
#	统一设置的内容:
	ApBssidNum=0
	ApAuthMode=""
	ApEncrypType=""
	ApRADIUSServer=""
	ApRADIUSPort=""
	ApRADIUSAcctServer=""
	ApRADIUSAcctPort=""
	ApRADIUSAcctKey=""
	ApPreAuth=""
	ApRekeyMethod=""
	ApDefKId=""
	ApK1Tp=""
	ApK2Tp=""
	ApK3Tp=""
	ApK4Tp=""
	ApHideESSID=""
	ApWmmCapable=""
	ApRRMEnable=""
	ApFtSupport=""
	ApNoForwarding=""
	ApRekeyInterval=""
	ApPMFMFPC=""
	ApPMFMFPR=""
	ApFtOtd=""
	ApFtOnly=""
	ApFtRic=""
	for_each_interface "ap" mt_dbdc_ap_vif_pre_config

#For DBDC profile merging......
	while [ $ApBssidNum -lt ${MTWIFI_DEF_MAX_BSSID} ]
	do
		ApEncrypType="${ApEncrypType}NONE;"
		ApAuthMode="${ApAuthMode}OPEN;"
		ApRADIUSAcctKey="${ApRADIUSAcctKey};"
		ApRADIUSAcctPort="${ApRADIUSAcctPort};"
		ApRADIUSAcctServer="${ApRADIUSAcctServer};"
		ApRADIUSPort="${ApRADIUSPort};"
		ApRADIUSServer="${ApRADIUSServer};"
		ApPreAuth="${ApPreAuth};"
		ApRekeyMethod="${ApRekeyMethod};"
		ApHideESSID="${ApHideESSID}0;"
		ApDefKId="${ApDefKId}0;"
		ApRRMEnable="${ApRRMEnable};"
		ApFtSupport="${ApFtSupport};"
		ApK1Tp="${ApK1Tp}0;"
		ApK2Tp="${ApK2Tp}0;"
		ApK3Tp="${ApK3Tp}0;"
		ApK4Tp="${ApK4Tp}0;"
		ApWmmCapable="${ApWmmCapable};"
		ApNoForwarding="${ApNoForwarding};"
		ApRekeyInterval="${ApRekeyInterval};"
		ApPMFMFPC="${ApPMFMFPC};"
		ApPMFMFPR="${ApPMFMFPR};"
		ApFtOtd="${ApFtOtd};"
		ApFtOnly="${ApFtOnly};"
		ApFtRic="${ApFtRic};"
		let ApBssidNum+=1
	done

	# BssidNum=${ApBssidNum:-1}
	# sed -i "s/BssidNum=1/BssidNum=${BssidNum}/g" $MTWIFI_PROFILE_PATH
	# eval sed -i 's/BssidNum=1/BssidNum=${BssidNum}/g' $MTWIFI_PROFILE_PATH
	# echo "BssidNum=${ApBssidNum:-1}" >> $MTWIFI_PROFILE_PATH
	echo "HideSSID=${ApHideESSID%?}" >> $MTWIFI_PROFILE_PATH
	echo "WmmCapable=${ApWmmCapable%?}" >> $MTWIFI_PROFILE_PATH
	echo "AuthMode=${ApAuthMode%?}" >> $MTWIFI_PROFILE_PATH
	echo "EncrypType=${ApEncrypType%?}" >> $MTWIFI_PROFILE_PATH
	echo "RADIUS_Server=${ApRADIUSServer%?}" >> $MTWIFI_PROFILE_PATH
	echo "own_ip_addr=${own_ip_addr}" >> $MTWIFI_PROFILE_PATH
	echo "own_radius_port=${own_radius_port}" >> $MTWIFI_PROFILE_PATH
	echo "RADIUS_Port=${ApRADIUSPort%?}" >> $MTWIFI_PROFILE_PATH
	echo "RADIUS_Acct_Server=${ApRADIUSAcctServer%?}" >> $MTWIFI_PROFILE_PATH
	echo "RADIUS_Acct_Key=${ApRADIUSAcctKey%?}" >> $MTWIFI_PROFILE_PATH
	echo "RADIUS_Acct_Port=${ApRADIUSAcctPort%?}" >> $MTWIFI_PROFILE_PATH
	echo "PreAuth=${ApPreAuth%?}" >> $MTWIFI_PROFILE_PATH
	echo "DefaultKeyID=${ApDefKId%?}" >> $MTWIFI_PROFILE_PATH
	echo "Key1Type=${ApK1Tp%?}" >> $MTWIFI_PROFILE_PATH
	echo "Key2Type=${ApK2Tp%?}" >> $MTWIFI_PROFILE_PATH
	echo "Key3Type=${ApK3Tp%?}" >> $MTWIFI_PROFILE_PATH
	echo "Key4Type=${ApK4Tp%?}" >> $MTWIFI_PROFILE_PATH
	echo "RekeyMethod=${ApRekeyMethod%?}" >> $MTWIFI_PROFILE_PATH
	echo "RRMEnable=${ApRRMEnable%?}" >> $MTWIFI_PROFILE_PATH
	echo "FtSupport=${ApFtSupport%?}" >> $MTWIFI_PROFILE_PATH
	echo "FtOtd=${ApFtOtd%?}" >> $MTWIFI_PROFILE_PATH
	echo "FtOnly=${ApFtOnly%?}" >> $MTWIFI_PROFILE_PATH
	echo "FtRic=${ApFtRic%?}" >> $MTWIFI_PROFILE_PATH
	echo "PMFMFPC=${ApPMFMFPC%?}" >> $MTWIFI_PROFILE_PATH
	echo "PMFMFPR=${ApPMFMFPR%?}" >> $MTWIFI_PROFILE_PATH
	echo "NoForwarding=${ApNoForwarding%?}" >> $MTWIFI_PROFILE_PATH
	echo "RekeyInterval=${ApRekeyInterval%?}" >> $MTWIFI_PROFILE_PATH
	echo "TxPreamble=${short_preamble}" >> $MTWIFI_PROFILE_PATH
	echo "KickStaRssiLow=${kicklow}" >> $MTWIFI_PROFILE_PATH
	echo "AssocReqRssiThres=${assocthres}" >> $MTWIFI_PROFILE_PATH

#	WDS接口
	WDSBssidNum=0
	WdsList=""
	Wdsen=""
	WdsEncrypType=""
	WdsDefKId=""
	WdsPhyMode=""
	for_each_interface "wds" mt_dbdc_wds_vif_pre_config

#For WDS profile merging......
	echo "WdsNum=${WDSBssidNum:-0}" >> $MTWIFI_PROFILE_PATH
	echo "WdsEnable=${Wdsen%?}" >> $MTWIFI_PROFILE_PATH
	echo "WdsList=${WdsList%?}" >> $MTWIFI_PROFILE_PATH
	echo "WdsEncrypType=${WdsEncrypType%?}" >> $MTWIFI_PROFILE_PATH
	echo "WdsDefaultKeyID=${WdsDefKId%?}" >> $MTWIFI_PROFILE_PATH
	echo "WdsPhyMode=${WdsPhyMode%?}" >> $MTWIFI_PROFILE_PATH

#	STA模式
	stacount=0
	ApCliAuthMode=""
	ApCliEncrypType=""
	ApCliSsid=""
	ApCliBssid=""
	ApCliDefKId=""
	ApCliWPAPSK=""
	ApCliKey1Str=""
	ApCliKey2Str=""
	ApCliKey3Str=""
	ApCliKey4Str=""
	ApCliK1Tp=""
	ApCliK2Tp=""
	ApCliK3Tp=""
	ApCliK4Tp=""
	ApCliPMFMFPC=""
	ApCliPMFMFPC=""
	for_each_interface "sta" mt_dbdc_sta_vif_pre_config

#For STA profile merging......
	echo "ApCliEnable=${ApCliEnable:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliSsid=${ApCliSsid}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliBssid=${ApCliBssid}" >> $MTWIFI_PROFILE_PATH
#	echo "ApCliMacAddress=${macaddr}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliAuthMode=${ApCliAuthMode}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliEncrypType=${ApCliEncrypType}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliDefaultKeyID=${ApCliDefKId:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliWPAPSK=${ApCliWPAPSK}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey1Str=${ApCliKey1Str}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey2Str=${ApCliKey2Str}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey3Str=${ApCliKey3Str}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey4Str=${ApCliKey4Str}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey1Type=${ApCliK1Tp:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey2Type=${ApCliK2Tp:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey3Type=${ApCliK3Tp:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliKey4Type=${ApCliK4Tp:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliPMFMFPC=${ApCliPMFMFPC:-0}" >> $MTWIFI_PROFILE_PATH
	echo "ApCliPMFMFPR=${ApCliPMFMFPR:-0}" >> $MTWIFI_PROFILE_PATH

#接口上线
#加锁
	echo "MTK Interfaces Pending..."
	if lock -n $WIFI_OP_LOCK; then
		sleep 2
		drv_mt_dbdc_teardown $phy_name
		mt_dbdc_vif_down $phy_name
#Start root device
		ifconfig ra0 up
#restore interfaces
		if [[ "$phy_name" = "ra0" ]]; then
			sh $MTWIFI_CMD_OPATH
			sh $MTWIFI_CMD_PATH
		else
			sh $MTWIFI_CMD_PATH
			sh $MTWIFI_CMD_OPATH
		fi
	else
		echo "Wait other process reload wifi"
		lock $WIFI_OP_LOCK
	fi

#AP模式
	#ApBssidNum=0
	for_each_interface "ap" mt_dbdc_vif_post_config
#WDS接口
	#WDSBssidNum=0
	for_each_interface "wds" mt_dbdc_vif_post_config
#STA模式
	#stacount=0
	for_each_interface "sta" mt_dbdc_vif_post_config

#重启HWNAT
	[ -d /sys/module/mtkhnat ] && {
		echo "Wait restart turboacc"
		/etc/init.d/turboacc restart
	}

#设置无线上线
	wireless_set_up
#解锁
	lock -u $WIFI_OP_LOCK
}

add_driver mt_dbdc
