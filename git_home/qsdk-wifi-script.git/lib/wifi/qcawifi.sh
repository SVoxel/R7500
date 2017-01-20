#!/bin/sh
#
# Copyright (c) 2014 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

append DRIVERS "qcawifi"

wlanconfig() {
	[ -n "${DEBUG}" ] && echo wlanconfig "$@"
	/usr/sbin/wlanconfig "$@"
}

iwconfig() {
	[ -n "${DEBUG}" ] && echo iwconfig "$@"
	/usr/sbin/iwconfig "$@"
}

iwpriv() {
	[ -n "${DEBUG}" ] && echo iwpriv "$@"
	/usr/sbin/iwpriv "$@"
}

find_qcawifi_phy() {
	local device="$1"

	local macaddr="$(config_get "$device" macaddr | tr 'A-Z' 'a-z')"
	config_get phy "$device" phy
	[ -z "$phy" -a -n "$macaddr" ] && {
		cd /sys/class/net
		for phy in $(ls -d wifi* 2>&-); do
			[ "$macaddr" = "$(cat /sys/class/net/${phy}/address)" ] || continue
			config_set "$device" phy "$phy"
			break
		done
		config_get phy "$device" phy
	}
	[ -n "$phy" -a -d "/sys/class/net/$phy" ] || {
		echo "phy for wifi device $1 not found"
		return 1
	}
	[ -z "$macaddr" ] && {
		config_set "$device" macaddr "$(cat /sys/class/net/${phy}/address)"
	}
	return 0
}

scan_qcawifi() {
	local device="$1"
	local wds
	local adhoc sta ap monitor disabled

	[ ${device%[0-9]} = "wifi" ] && config_set "$device" phy "$device"

	local ifidx=0
	local radioidx=${device#wifi}

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get_bool disabled "$vif" disabled 0
		[ $disabled = 0 ] || continue

		local vifname
		vifname="ath${ifidx}"

		config_get ifname "$vif" ifname
		config_set "$vif" ifname "${ifname:-$vifname}"
		
		config_get mode "$vif" mode
		case "$mode" in
			adhoc|sta|ap|monitor)
				append $mode "$vif"
			;;
			wds)
				config_get ssid "$vif" ssid
				[ -z "$ssid" ] && continue

				config_set "$vif" wds 1
				config_set "$vif" mode sta
				mode="sta"
				addr="$ssid"
				${addr:+append $mode "$vif"}
			;;
			*) echo "$device($vif): Invalid mode, ignored."; continue;;
		esac

		ifidx=$(($ifidx + 1))
	done

	case "${adhoc:+1}:${sta:+1}:${ap:+1}" in
		# valid mode combinations
		1::) wds="";;
		1::1);;
		:1:1)config_set "$device" nosbeacon 1;; # AP+STA, can't use beacon timers for STA
		:1:);;
		::1);;
		::);;
		*) echo "$device: Invalid mode combination in config"; return 1;;
	esac

	config_set "$device" vifs "${ap:+$ap }${sta:+$sta }${adhoc:+$adhoc }${wds:+$wds }${monitor:+$monitor}"
}


load_qcawifi() {
	local umac_args
	echo 3 > /proc/sys/vm/drop_caches

	config_get_bool testmode qcawifi testmode
	[ -n "$testmode" ] && append umac_args "testmode=$testmode"

	config_get vow_config qcawifi vow_config
	[ -n "$vow_config" ] && append umac_args "vow_config=$vow_config"

	config_get macaddr wifi0 macaddr
	[ -n "$macaddr" ] && MAC_ARGS="mac_addr_2g=$macaddr "
	config_get macaddr wifi1 macaddr
	[ -n "$macaddr" ] && MAC_ARGS=${MAC_ARGS}"mac_addr_5g=$macaddr"
	config_get ol_bk_min_free qcawifi ol_bk_min_free
	[ -n "$ol_bk_min_free" ] && append umac_args "OL_ACBKMinfree=$ol_bk_min_free"

	config_get ol_be_min_free qcawifi ol_be_min_free
	[ -n "$ol_be_min_free" ] && append umac_args "OL_ACBEMinfree=$ol_be_min_free"

	config_get ol_vi_min_free qcawifi ol_vi_min_free
	[ -n "$ol_vi_min_free" ] && append umac_args "OL_ACVIMinfree=$ol_vi_min_free"

	config_get ol_vo_min_free qcawifi ol_vo_min_free
	[ -n "$ol_vo_min_free" ] && append umac_args "OL_ACVOMinfree=$ol_vo_min_free"

	for mod in $(cat /etc/modules.d/33-qca-wifi*); do

		case ${mod} in
			ath_hal) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} ${MAC_ARGS} || { \
					unload_qcawifi
					return 1
				}
			};;
			umac) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} ${umac_args} || { \
					unload_qcawifi
					return 1
				}
			};;

			*) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} || { \
					unload_qcawifi
					return 1
				}
			};;

		esac
	done
	config_get macaddr wifi0 macaddr
	[ -n "$macaddr" ] && iwpriv wifi0 setHwaddr $macaddr
}


unload_qcawifi() {
	for mod in $(cat /etc/modules.d/33-qca-wifi* | sed '1!G;h;$!d'); do
		[ -d /sys/module/${mod} ] && rmmod ${mod}
	done
}


disable_qcawifi() {
	local device="$1"
	local parent

	find_qcawifi_phy "$device" || return 1
	config_get phy "$device" phy

	set_wifi_down "$device"

	clear_wifi_ebtables

	include /lib/network
	cd /sys/class/net
	for dev in *; do
		[ -f /sys/class/net/${dev}/parent ] && { \
			local parent=$(cat /sys/class/net/${dev}/parent)
			[ -n "$parent" -a "$parent" = "$device" ] && { \
				[ -f "/var/run/wifi-${dev}.pid" ] &&
					kill "$(cat "/var/run/wifi-${dev}.pid")"
				[ -f "/var/run/hostapd_cli-${dev}.pid"] &&
					kill "$(cat "/var/run/hostapd_cli-${dev}.pid")"
				ifconfig "$dev" down
				unbridge "$dev"
				wlanconfig "$dev" destroy
			}
		}
	done

	# delete wlan uptime file
	if [ "$phy" = "wifi0" ]; then
		rm /tmp/WLAN_uptime
	elif [ "$phy" = "wifi1" ]; then
		rm /tmp/WLAN_uptime_5G
	fi

	nrvaps=$(find /sys/class/net/ -name 'ath*'|wc -l)
	[ ${nrvaps} -gt 0 ] || unload_qcawifi

	return 0
}

enable_qcawifi() {
	local device="$1"

	load_qcawifi || return 1

	find_qcawifi_phy "$device" || return 1
	config_get phy "$device" phy

	# If the country parameter is number (either hex or decimal), we
	# assume it's a regulatory domain - i.e. we use iwpriv setCountryID.
	# Else we assume it's a country code - i.e. we use iwpriv setCountry.
	config_get country "$device" country
	if [ `expr "$country" : '[0-9].*'` -ne 0 ]; then
		iwpriv "$phy" setCountryID "$country"
	elif [ -n "$country" ]; then
		iwpriv "$phy" setCountry "$country"
	fi

	config_get channel "$device" channel
	config_get vifs "$device" vifs
	config_get txpower "$device" txpower
	config_get tpscale "$device" tpscale
	[ -n "$tpscale" ] && iwpriv "$phy" tpscale "$tpscale"

	[ auto = "$channel" ] && channel=0

	config_get_bool antdiv "$device" diversity
	config_get antrx "$device" rxantenna
	config_get anttx "$device" txantenna
	config_get_bool softled "$device" softled
	config_get antenna "$device" antenna
	config_get distance "$device" distance

	[ -n "$antdiv" ] && echo "antdiv option not supported on this driver"
	[ -n "$antrx" ] && echo "antrx option not supported on this driver"
	[ -n "$anttx" ] && echo "anttx option not supported on this driver"
	[ -n "$softled" ] && echo "softled option not supported on this driver"
	[ -n "$antenna" ] && echo "antenna option not supported on this driver"
	[ -n "$distance" ] && echo "distance option not supported on this driver"

	# Advanced QCA wifi per-radio parameters configuration
	config_get txchainmask "$device" txchainmask
	[ -n "$txchainmask" ] && iwpriv "$phy" txchainmask "$txchainmask"

	config_get rxchainmask "$device" rxchainmask
	[ -n "$rxchainmask" ] && iwpriv "$phy" rxchainmask "$rxchainmask"

	config_get AMPDU "$device" AMPDU
	[ -n "$AMPDU" ] && iwpriv "$phy" AMPDU "$AMPDU"

	config_get ampdudensity "$device" ampdudensity
	[ -n "$ampdudensity" ] && iwpriv "$phy" ampdudensity "$ampdudensity"

	config_get_bool AMSDU "$device" AMSDU
	[ -n "$AMSDU" ] && iwpriv "$phy" AMSDU "$AMSDU"

	config_get AMPDULim "$device" AMPDULim
	[ -n "$AMPDULim" ] && iwpriv "$phy" AMPDULim "$AMPDULim"

	config_get AMPDUFrames "$device" AMPDUFrames
	[ -n "$AMPDUFrames" ] && iwpriv "$phy" AMPDUFrames "$AMPDUFrames"

	config_get AMPDURxBsize "$device" AMPDURxBsize
	[ -n "$AMPDURxBsize" ] && iwpriv "$phy" AMPDURxBsize "$AMPDURxBsize"

	config_get_bool bcnburst "$device" bcnburst 0
	[ "$bcnburst" -gt 0 ] && iwpriv "$phy" set_bcnburst "$bcnburst"

	config_get set_smart_antenna "$device" set_smart_antenna
	[ -n "$set_smart_antenna" ] && iwpriv "$phy" setSmartAntenna "$set_smart_antenna"

	config_get current_ant "$device" current_ant
	[ -n  "$current_ant" ] && iwpriv "$phy" current_ant "$current_ant"

	config_get default_ant "$device" default_ant
	[ -n "$default_ant" ] && iwpriv "$phy" default_ant "$default_ant"

	config_get ant_retrain "$device" ant_retrain
	[ -n "$ant_retrain" ] && iwpriv "$phy" ant_retrain "$ant_retrain"

	config_get retrain_interval "$device" retrain_interval
	[ -n "$retrain_interval" ] && iwpriv "$phy" retrain_interval "$retrain_interval"

	config_get retrain_drop "$device" retrain_drop
	[ -n "$retrain_drop" ] && iwpriv "$phy" retrain_drop "$retrain_drop"

	config_get ant_train "$device" ant_train
	[ -n "$ant_train" ] && iwpriv "$phy" ant_train "$ant_train"

	config_get ant_trainmode "$device" ant_trainmode
	[ -n "$ant_trainmode" ] && iwpriv "$phy" ant_trainmode "$ant_trainmode"

	config_get ant_traintype "$device" ant_traintype
	[ -n "$ant_traintype" ] && iwpriv "$phy" ant_traintype "$ant_traintype"

	config_get ant_pktlen "$device" ant_pktlen
	[ -n "$ant_pktlen" ] && iwpriv "$phy" ant_pktlen "$ant_pktlen"

	config_get ant_numpkts "$device" ant_numpkts
	[ -n "$ant_numpkts" ] && iwpriv "$phy" ant_numpkts "$ant_numpkts"

	config_get ant_numitr "$device" ant_numitr
	[ -n "$ant_numitr" ] && iwpriv "$phy" ant_numitr "$ant_numitr"

	config_get ant_train_thres "$device" ant_train_thres
	[ -n "$ant_train_thres" ] && iwpriv "$phy" train_threshold "$ant_train_thres"

	config_get ant_train_min_thres "$device" ant_train_min_thres
	[ -n "$ant_train_min_thres" ] && iwpriv "$phy" train_threshold "$ant_train_min_thres"

	config_get ant_traffic_timer "$device" ant_traffic_timer
	[ -n "$ant_traffic_timer" ] && iwpriv "$phy" traffic_timer "$ant_traffic_timer"

	config_get dcs_enable "$device" dcs_enable
	[ -n "$dcs_enable" ] && iwpriv "$phy" dcs_enable "$dcs_enable"

	config_get dcs_coch_int "$device" dcs_coch_int
	[ -n "$dcs_coch_int" ] && iwpriv "$phy" set_dcs_coch_int "$dcs_coch_int"

	config_get dcs_errth "$device" dcs_errth
	[ -n "$dcs_errth" ] && iwpriv "$phy" set_dcs_errth "$dcs_errth"

	config_get dcs_phyerrth "$device" dcs_phyerrth
	[ -n "$dcs_phyerrth" ] && iwpriv "$phy" set_dcs_phyerrth "$dcs_phyerrth"

	config_get dcs_usermaxc "$device" dcs_usermaxc
	[ -n "$dcs_usermaxc" ] && iwpriv "$phy" set_dcs_usermaxc "$dcs_usermaxc"

	config_get dcs_debug "$device" dcs_debug
	[ -n "$dcs_debug" ] && iwpriv "$phy" set_dcs_debug "$dcs_debug"

	config_get set_ch_144 "$device" set_ch_144
	[ -n "$set_ch_144" ] && iwpriv "$phy" setCH144 "$set_ch_144"

	config_get_bool ani_enable "$device" ani_enable
	[ -n "$ani_enable" ] && iwpriv "$phy" ani_enable "$ani_enable"

	config_get_bool acs_bkscanen "$device" acs_bkscanen
	[ -n "$acs_bkscanen" ] && iwpriv "$phy" acs_bkscanen "$acs_bkscanen"

	config_get acs_scanintvl "$device" acs_scanintvl
	[ -n "$acs_scanintvl" ] && iwpriv "$phy" acs_scanintvl "$acs_scanintvl"

	config_get acs_rssivar "$device" acs_rssivar
	[ -n "$acs_rssivar" ] && iwpriv "$phy" acs_rssivar "$acs_rssivar"

	config_get acs_chloadvar "$device" acs_chloadvar
	[ -n "$acs_chloadvar" ] && iwpriv "$phy" acs_chloadvar "$acs_chloadvar"

	config_get acs_lmtobss "$device" acs_lmtobss
	[ -n "$acs_lmtobss" ] && iwpriv "$phy" acs_lmtobss "$acs_lmtobss"

	config_get acs_ctrlflags "$device" acs_ctrlflags
	[ -n "$acs_ctrlflags" ] && iwpriv "$phy" acs_ctrlflags "$acs_ctrlflags"

	config_get acs_dbgtrace "$device" acs_dbgtrace
	[ -n "$acs_dbgtrace" ] && iwpriv "$phy" acs_dbgtrace "$acs_dbgtrace"

	config_get_bool dscp_ovride "$device" dscp_ovride
	[ -n "$dscp_ovride" ] && iwpriv "$phy" set_dscp_ovride "$dscp_ovride"

	config_get reset_dscp_map "$device" reset_dscp_map
	[ -n "$reset_dscp_map" ] && iwpriv "$phy" reset_dscp_map "$reset_dscp_map"

	config_get dscp_tid_map "$device" dscp_tid_map
	[ -n "$dscp_tid_map" ] && iwpriv "$phy" set_dscp_tid_map $dscp_tid_map

	config_get_bool igmp_dscp_ovride "$device" igmp_dscp_ovride
	[ -n "$igmp_dscp_ovride" ] && iwpriv "$phy" sIgmpDscpOvrid "$igmp_dscp_ovride"

	config_get igmp_dscp_tid_map "$device" igmp_dscp_tid_map
	[ -n "$igmp_dscp_tid_map" ] && iwpriv "$phy" sIgmpDscpTidMap "$igmp_dscp_tid_map"

	config_get_bool hmmc_dscp_ovride "$device" hmmc_dscp_ovride
	[ -n "$hmmc_dscp_ovride" ] && iwpriv "$phy" sHmmcDscpOvrid "$hmmc_dscp_ovride"

	config_get hmmc_dscp_tid_map "$device" hmmc_dscp_tid_map
	[ -n "$hmmc_dscp_tid_map" ] && iwpriv "$phy" sHmmcDscpTidMap "$hmmc_dscp_tid_map"

	config_get_bool blk_report_fld "$device" blk_report_fld
	[ -n "$blk_report_fld" ] && iwpriv "$phy" setBlkReportFld "$blk_report_fld"

	config_get_bool drop_sta_query "$device" drop_sta_query
	[ -n "$drop_sta_query" ] && iwpriv "$phy" setDropSTAQuery "$drop_sta_query"

	config_get_bool burst "$device" burst
	[ -n "$burst" ] && iwpriv "$phy" burst "$burst"

	config_get burst_dur "$device" burst_dur
	[ -n "$burst_dur" ] && iwpriv "$phy" burst_dur "$burst_dur"

	config_get TXPowLim2G "$device" TXPowLim2G
	[ -n "$TXPowLim2G" ] && iwpriv "$phy" TXPowLim2G "$TXPowLim2G"

	config_get TXPowLim5G "$device" TXPowLim5G
	[ -n "$TXPowLim5G" ] && iwpriv "$phy" TXPowLim5G "$TXPowLim5G"

	config_get_bool enable_ol_stats "$device" enable_ol_stats
	[ -n "$enable_ol_stats" ] && iwpriv "$phy" enable_ol_stats "$enable_ol_stats"

	config_get_bool set_fw_recovery "$device" set_fw_recovery
	[ -n "$set_fw_recovery" ] && iwpriv "$phy" set_fw_recovery "$set_fw_recovery"

	config_get_bool allowpromisc "$device" allowpromisc
	[ -n "$allowpromisc" ] && iwpriv "$phy" allowpromisc "$allowpromisc"

	config_get set_sa_param "$device" set_sa_param
	[ -n "$set_sa_param" ] && iwpriv "$phy" set_sa_param $set_sa_param

	config_get_bool aldstats "$device" aldstats
	[ -n "$aldstats" ] && iwpriv "$phy" aldstats "$aldstats"

	for vif in $vifs; do
		local start_hostapd= vif_txpower= nosbeacon=
		config_get ifname "$vif" ifname
		config_get enc "$vif" encryption "none"
		config_get eap_type "$vif" eap_type
		config_get mode "$vif" mode

		case "$mode" in
			sta) config_get_bool nosbeacon "$device" nosbeacon;;
			adhoc) config_get_bool nosbeacon "$vif" sw_merge 1;;
		esac

		[ "$nosbeacon" = 1 ] || nosbeacon=""
		[ -n "${DEBUG}" ] && echo wlanconfig "$ifname" create wlandev "$phy" wlanmode "$mode" ${nosbeacon:+nosbeacon}
		ifname=$(/usr/sbin/wlanconfig "$ifname" create wlandev "$phy" wlanmode "$mode" ${nosbeacon:+nosbeacon})
		[ $? -ne 0 ] && {
			echo "enable_qcawifi($device): Failed to set up $mode vif $ifname" >&2
			continue
		}
		config_set "$vif" ifname "$ifname"

		config_get hwmode "$device" hwmode auto
		config_get htmode "$device" htmode auto

		pureg=0
		case "$hwmode:$htmode" in
		# The parsing stops at the first match so we need to make sure
		# these are in the right orders (most generic at the end)
			*ng:HT20) hwmode=11NGHT20;;
			*ng:HT40-) hwmode=11NGHT40MINUS;;
			*ng:HT40+) hwmode=11NGHT40PLUS;;
			*ng:HT40) hwmode=11NGHT40;;
			*ng:*) hwmode=11NGHT20;;
			*na:HT20) hwmode=11NAHT20;;
			*na:HT40-) hwmode=11NAHT40MINUS;;
			*na:HT40+) hwmode=11NAHT40PLUS;;
			*na:HT40) hwmode=11NAHT40;;
			*na:*) hwmode=11NAHT40;;
			*ac:HT20) hwmode=11ACVHT20;;
			*ac:HT40+) hwmode=11ACVHT40PLUS;;
			*ac:HT40-) hwmode=11ACVHT40MINUS;;
			*ac:HT40) hwmode=11ACVHT40;;
			*ac:HT80) hwmode=11ACVHT80;;
			*ac:*) hwmode=11ACVHT80;;
			*b:*) hwmode=11B;;
			*bg:*) hwmode=11G;;
			*g:*) hwmode=11G; pureg=1;;
			*a:*) hwmode=11A;;
			*) hwmode=AUTO;;
		esac
		iwpriv "$ifname" mode "$hwmode"
		[ $pureg -gt 0 ] && iwpriv "$ifname" pureg "$pureg"

		config_get puren "$vif" puren
		[ -n "$puren" ] && iwpriv "$ifname" puren "$puren"

		iwconfig "$ifname" channel "$channel"

		config_get_bool hidden "$vif" hidden 0
		iwpriv "$ifname" hide_ssid "$hidden"

		config_get_bool shortgi "$vif" shortgi 1
		[ -n "$shortgi" ] && iwpriv "$ifname" shortgi "${shortgi}"

		config_get_bool disablecoext "$vif" disablecoext
		[ -n "$disablecoext" ] && iwpriv "$ifname" disablecoext "${disablecoext}"
		[ "$disablecoext" -eq "0" ] && iwpriv $ifname extbusythres 30
		[ "$disablecoext" -eq "1" ] && iwpriv $ifname extbusythres 100

		config_get chwidth "$vif" chwidth
		[ -n "$chwidth" ] && iwpriv "$ifname" chwidth "${chwidth}"

		config_get wds "$vif" wds
		case "$wds" in
			1|on|enabled) wds=1;;
			*) wds=0;;
		esac
		iwpriv "$ifname" wds "$wds"

		config_get ODM "$device" ODM
		config_get bridge "$vif" bridge
		[ "$ODM" -eq "dni" ] && [ -n "$wds" ] && brctl stp "$bridge" on

		config_get TxBFCTL "$vif" TxBFCTL
		[ -n "$TxBFCTL" ] && iwpriv "$ifname" TxBFCTL "$TxBFCTL"

		config_get bintval "$vif" bintval
		[ -n "$bintval" ] && iwpriv "$ifname" bintval "$bintval"

		config_get_bool countryie "$vif" countryie
		[ -n "$countryie" ] && iwpriv "$ifname" countryie "$countryie"

		case "$enc" in
			none)
				# If we're in open mode and want to use WPS, we
				# must start hostapd
				config_get_bool wps_pbc "$vif" wps_pbc 0
				config_get config_methods "$vif" wps_config
				[ "$wps_pbc" -gt 0 ] && append config_methods push_button
				[ -n "$config_methods" ] && start_hostapd=1
			;;
			wep*)
				case "$enc" in
					*mixed*)  iwpriv "$ifname" authmode 4;;
					*shared*) iwpriv "$ifname" authmode 2;;
					*)        iwpriv "$ifname" authmode 1;;
				esac
				for idx in 1 2 3 4; do
					config_get key "$vif" "key${idx}"
					config_get key_format "$vif" "key${idx}_format"
					if [ "$key_format" = "ASCII" ]; then
						iwconfig "$ifname" enc s:"${key:-off}" "[$idx]"
					else
						iwconfig "$ifname" enc "[$idx]" "${key:-off}"
					fi
				done
				config_get key "$vif" key
				key="${key:-1}"
				case "$key" in
					[1234]) iwconfig "$ifname" enc "[$key]";;
					*) iwconfig "$ifname" enc "$key";;
				esac
			;;
			mixed*|psk*|wpa*|8021x)
				start_hostapd=1
				config_get key "$vif" key
			;;
			wapi*)
				start_wapid=1
				config_get key "$vif" key
			;;
		esac

		case "$mode" in
			sta|adhoc)
				config_get addr "$vif" bssid
				[ -z "$addr" ] || { 
					iwconfig "$ifname" ap "$addr"
				}
			;;
		esac

		config_get_bool uapsd "$vif" uapsd 1
		iwpriv "$ifname" uapsd "$uapsd"

		config_get mcast_rate "$vif" mcast_rate
		[ -n "$mcast_rate" ] && iwpriv "$ifname" mcast_rate "${mcast_rate%%.*}"

		config_get powersave "$vif" powersave
		[ -n "$powersave" ] && iwpriv "$ifname" powersave "${powersave}"

		config_get_bool ant_ps_on "$vif" ant_ps_on
		[ -n "$ant_ps_on" ] && iwpriv "$ifname" ant_ps_on "${ant_ps_on}"

		config_get ps_timeout "$vif" ps_timeout
		[ -n "$ps_timeout" ] && iwpriv "$ifname" ps_timeout "${ps_timeout}"

		config_get_bool mcastenhance "$vif" mcastenhance
		[ -n "$mcastenhance" ] && iwpriv "$ifname" mcastenhance "${mcastenhance}"

		config_get metimer "$vif" metimer
		[ -n "$metimer" ] && iwpriv "$ifname" metimer "${metimer}"

		config_get metimeout "$vif" metimeout
		[ -n "$metimeout" ] && iwpriv "$ifname" metimeout "${metimeout}"

		config_get_bool medropmcast "$vif" medropmcast
		[ -n "$medropmcast" ] && iwpriv "$ifname" medropmcast "${medropmcast}"

		config_get me_adddeny "$vif" me_adddeny
		[ -n "$me_adddeny" ] && iwpriv "$ifname" me_adddeny ${me_adddeny}

		#support independent repeater mode
		config_get vap_ind "$vif" vap_ind
		[ -n "$vap_ind" ] && iwpriv "$ifname" vap_ind "${vap_ind}"

		#support extender ap & STA
		config_get extap "$vif" extap
		[ -n "$extap" ] && iwpriv "$ifname" extap "${extap}"

		config_get scanband "$vif" scanband
		[ -n "$scanband" ] && iwpriv "$ifname" scanband "${scanband}"

		config_get periodicScan "$vif" periodicScan
		[ -n "$periodicScan" ] && iwpriv "$ifname" periodicScan "${periodicScan}"

                config_get cb "$vif" cb
                if [ "$cb" = "qca" ]; then
                    iwpriv "$ifname" extap 1
                    iwpriv "$ifname" scanband 1
                    iwpriv "$ifname" periodicScan 180000
                elif [ "$cb" = "dni" ]; then
                    iwpriv "$ifname" dni_cb 1
                    iwpriv "$ifname" periodicScan 180000
                fi

		config_get_bool short_preamble "$vif" short_preamble 1
		[ -n "$short_preamble" ] && iwpriv "$ifname" shpreamble "${short_preamble}"

		config_get frag "$vif" frag
		[ -n "$frag" ] && iwconfig "$ifname" frag "${frag%%.*}"

		config_get rts "$vif" rts
		[ -n "$rts" ] && iwconfig "$ifname" rts "${rts%%.*}"

		config_get cwmin "$vif" cwmin
		[ -n "$cwmin" ] && iwpriv "$ifname" cwmin ${cwmin}

		config_get cwmax "$vif" cwmax
		[ -n "$cwmax" ] && iwpriv "$ifname" cwmax ${cwmax}

		config_get aifs "$vif" aifs
		[ -n "$aifs" ] && iwpriv "$ifname" aifs ${aifs}

		config_get txoplimit "$vif" txoplimit
		[ -n "$txoplimit" ] && iwpriv "$ifname" txoplimit ${txoplimit}

		config_get noackpolicy "$vif" noackpolicy
		[ -n "$noackpolicy" ] && iwpriv "$ifname" noackpolicy ${noackpolicy}

		config_get_bool wmm "$vif" wmm
		[ -n "$wmm" ] && iwpriv "$ifname" wmm "$wmm"

		config_get_bool doth "$vif" doth
		[ -n "$doth" ] && iwpriv "$ifname" doth "$doth"

		config_get doth_chanswitch "$vif" doth_chanswitch
		[ -n "$doth_chanswitch" ] && iwpriv "$ifname" doth_chanswitch ${doth_chanswitch}

		config_get quiet "$vif" quiet
		[ -n "$quiet" ] && iwpriv "$ifname" quiet "$quiet"

		config_get mfptest "$vif" mfptest
		[ -n "$mfptest" ] && iwpriv "$ifname" mfptest "$mfptest"

		config_get dtim_period "$vif" dtim_period
		[ -n "$dtim_period" ] && iwpriv "$ifname" dtim_period "$dtim_period"

		config_get noedgech "$vif" noedgech
		[ -n "$noedgech" ] && iwpriv "$ifname" noedgech "$noedgech"

		config_get ps_on_time "$vif" ps_on_time
		[ -n "$ps_on_time" ] && iwpriv "$ifname" ps_on_time "$ps_on_time"

		config_get inact "$vif" inact
		[ -n "$inact" ] && iwpriv "$ifname" inact "$inact"

		config_get wnm "$vif" wnm
		[ -n "$wnm" ] && iwpriv "$ifname" wnm "$wnm"

		config_get ampdu "$vif" ampdu
		[ -n "$ampdu" ] && iwpriv "$ifname" ampdu "$ampdu"

		config_get amsdu "$vif" amsdu
		[ -n "$amsdu" ] && iwpriv "$ifname" amsdu "$amsdu"

		config_get maxampdu "$vif" maxampdu
		[ -n "$maxampdu" ] && iwpriv "$ifname" maxampdu "$maxampdu"

		config_get vhtmaxampdu "$vif" vhtmaxampdu
		[ -n "$vhtmaxampdu" ] && iwpriv "$ifname" vhtmaxampdu "$vhtmaxampdu"

		config_get setaddbaoper "$vif" setaddbaoper
		[ -n "$setaddbaoper" ] && iwpriv "$ifname" setaddbaoper "$setaddbaoper"

		config_get addbaresp "$vif" addbaresp
		[ -n "$addbaresp" ] && iwpriv "$ifname" $addbaresp

		config_get addba "$vif" addba
		[ -n "$addba" ] && iwpriv "$ifname" addba $addba

		config_get delba "$vif" delba
		[ -n "$delba" ] && iwpriv "$ifname" delba $delba

		config_get_bool stafwd "$vif" stafwd 0
		[ -n "$stafwd" ] && iwpriv "$ifname" stafwd "$stafwd"

		config_get maclist "$vif" maclist
		[ -n "$maclist" ] && {
			# flush MAC list
			iwpriv "$ifname" maccmd 3
			for mac in $maclist; do
				iwpriv "$ifname" addmac "$mac"
			done
		}

		config_get macfilter "$vif" macfilter
		case "$macfilter" in
			allow)
				iwpriv "$ifname" maccmd 1
			;;
			deny)
				iwpriv "$ifname" maccmd 2
			;;
			*)
				# default deny policy if mac list exists
				[ -n "$maclist" ] && iwpriv "$ifname" maccmd 2
			;;
		esac

		# 256 QAM capability needs to be parsed first, since
		# vhtmcs enables/disable rate indices 8, 9 for 2G
		# only if vht_11ng is set or not
		config_get_bool vht_11ng "$vif" vht_11ng
		[ -n "$vht_11ng" ] && { 
			iwpriv "$ifname" vht_11ng "$vht_11ng"
			iwpriv "$ifname" 11ngvhtintop "$vht_11ng"
		}
		config_get nss "$vif" nss
		[ -n "$nss" ] && iwpriv "$ifname" nss "$nss"

		config_get vhtmcs "$vif" vhtmcs
		[ -n "$vhtmcs" ] && iwpriv "$ifname" vhtmcs "$vhtmcs"

		config_get vht_mcsmap "$vif" vht_mcsmap
		[ -n "$vht_mcsmap" ] && iwpriv "$ifname" vht_mcsmap "$vht_mcsmap"

		config_get chwidth "$vif" chwidth
		[ -n "$chwidth" ] && iwpriv "$ifname" chwidth "$chwidth"

		config_get chbwmode "$vif" chbwmode
		[ -n "$chbwmode" ] && iwpriv "$ifname" chbwmode "$chbwmode"

		config_get ldpc "$vif" ldpc
		[ -n "$ldpc" ] && iwpriv "$ifname" ldpc "$ldpc"

		config_get rx_stbc "$vif" rx_stbc
		[ -n "$rx_stbc" ] && iwpriv "$ifname" rx_stbc "$rx_stbc"

		config_get tx_stbc "$vif" tx_stbc
		[ -n "$tx_stbc" ] && iwpriv "$ifname" tx_stbc "$tx_stbc"

		config_get cca_thresh "$vif" cca_thresh
		[ -n "$cca_thresh" ] && iwpriv "$ifname" cca_thresh "$cca_thresh"

		config_get set11NRates "$vif" set11NRates
		[ -n "$set11NRates" ] && iwpriv "$ifname" set11NRates "$set11NRates"

		config_get set11NRetries "$vif" set11NRetries
		[ -n "$set11NRetries" ] && iwpriv "$ifname" set11NRetries "$set11NRetries"

		config_get chanbw "$vif" chanbw
		[ -n "$chanbw" ] && iwpriv "$ifname" chanbw "$chanbw"

		config_get maxsta "$vif" maxsta
		[ -n "$maxsta" ] && iwpriv "$ifname" maxsta "$maxsta"

		config_get sko_max_xretries "$vif" sko_max_xretries
		[ -n "$sko_max_xretries" ] && iwpriv "$ifname" sko "$sko_max_xretries"

		config_get extprotmode "$vif" extprotmode
		[ -n "$extprotmode" ] && iwpriv "$ifname" extprotmode "$extprotmode"

		config_get extprotspac "$vif" extprotspac
		[ -n "$extprotspac" ] && iwpriv "$ifname" extprotspac "$extprotspac"

		config_get_bool cwmenable "$vif" cwmenable
		[ -n "$cwmenable" ] && iwpriv "$ifname" cwmenable "$cwmenable"

		config_get_bool protmode "$vif" protmode
		[ -n "$protmode" ] && iwpriv "$ifname" protmode "$protmode"

		config_get enablertscts "$vif" enablertscts
		[ -n "$enablertscts" ] && iwpriv "$ifname" enablertscts "$enablertscts"

		config_get txcorrection "$vif" txcorrection
		[ -n "$txcorrection" ] && iwpriv "$ifname" txcorrection "$txcorrection"

		config_get rxcorrection "$vif" rxcorrection
		[ -n "$rxcorrection" ] && iwpriv "$ifname" rxcorrection "$rxcorrection"

		config_get ssid "$vif" ssid
                [ -n "$ssid" ] && {
                        iwconfig "$ifname" essid on
                        iwconfig "$ifname" essid ${ssid:+-- }"$ssid"
                }

		config_get txqueuelen "$vif" txqueuelen
		[ -n "$txqueuelen" ] && ifconfig "$ifname" txqueuelen "$txqueuelen"

		config_get tdls "$vif" tdls
		[ -n "$tdls" ] && iwpriv "$ifname" tdls "$tdls"

		config_get set_tdls_rmac "$vif" set_tdls_rmac
		[ -n "$set_tdls_rmac" ] && iwpriv "$ifname" set_tdls_rmac "$set_tdls_rmac"

		config_get tdls_qosnull "$vif" tdls_qosnull
		[ -n "$tdls_qosnull" ] && iwpriv "$ifname" tdls_qosnull "$tdls_qosnull"

		config_get tdls_uapsd "$vif" tdls_uapsd
		[ -n "$tdls_uapsd" ] && iwpriv "$ifname" tdls_uapsd "$tdls_uapsd"

		config_get tdls_set_rcpi "$vif" tdls_set_rcpi
		[ -n "$tdls_set_rcpi" ] && iwpriv "$ifname" set_rcpi "$tdls_set_rcpi"

		config_get tdls_set_rcpi_hi "$vif" tdls_set_rcpi_hi
		[ -n "$tdls_set_rcpi_hi" ] && iwpriv "$ifname" set_rcpihi "$tdls_set_rcpi_hi"

		config_get tdls_set_rcpi_lo "$vif" tdls_set_rcpi_lo
		[ -n "$tdls_set_rcpi_lo" ] && iwpriv "$ifname" set_rcpilo "$tdls_set_rcpi_lo"

		config_get tdls_set_rcpi_margin "$vif" tdls_set_rcpi_margin
		[ -n "$tdls_set_rcpi_margin" ] && iwpriv "$ifname" set_rcpimargin "$tdls_set_rcpi_margin"

		config_get tdls_dtoken "$vif" tdls_dtoken
		[ -n "$tdls_dtoken" ] && iwpriv "$ifname" tdls_dtoken "$tdls_dtoken"

		config_get do_tdls_dc_req "$vif" do_tdls_dc_req
		[ -n "$do_tdls_dc_req" ] && iwpriv "$ifname" do_tdls_dc_req "$do_tdls_dc_req"

		config_get tdls_auto "$vif" tdls_auto
		[ -n "$tdls_auto" ] && iwpriv "$ifname" tdls_auto "$tdls_auto"

		config_get tdls_off_timeout "$vif" tdls_off_timeout
		[ -n "$tdls_off_timeout" ] && iwpriv "$ifname" off_timeout "$tdls_off_timeout"

		config_get tdls_tdb_timeout "$vif" tdls_tdb_timeout
		[ -n "$tdls_tdb_timeout" ] && iwpriv "$ifname" tdb_timeout "$tdls_tdb_timeout"

		config_get tdls_weak_timeout "$vif" tdls_weak_timeout
		[ -n "$tdls_weak_timeout" ] && iwpriv "$ifname" weak_timeout "$tdls_weak_timeout"

		config_get tdls_margin "$vif" tdls_margin
		[ -n "$tdls_margin" ] && iwpriv "$ifname" tdls_margin "$tdls_margin"

		config_get tdls_rssi_ub "$vif" tdls_rssi_ub
		[ -n "$tdls_rssi_ub" ] && iwpriv "$ifname" tdls_rssi_ub "$tdls_rssi_ub"

		config_get tdls_rssi_lb "$vif" tdls_rssi_lb
		[ -n "$tdls_rssi_lb" ] && iwpriv "$ifname" tdls_rssi_lb "$tdls_rssi_lb"

		config_get tdls_path_sel "$vif" tdls_path_sel
		[ -n "$tdls_path_sel" ] && iwpriv "$ifname" tdls_pathSel "$tdls_path_sel"

		config_get tdls_rssi_offset "$vif" tdls_rssi_offset
		[ -n "$tdls_rssi_offset" ] && iwpriv "$ifname" tdls_rssi_o "$tdls_rssi_offset"

		config_get tdls_path_sel_period "$vif" tdls_path_sel_period
		[ -n "$tdls_path_sel_period" ] && iwpriv "$ifname" tdls_pathSel_p "$tdls_path_sel_period"

		config_get tdlsmacaddr1 "$vif" tdlsmacaddr1
		[ -n "$tdlsmacaddr1" ] && iwpriv "$ifname" tdlsmacaddr1 "$tdlsmacaddr1"

		config_get tdlsmacaddr2 "$vif" tdlsmacaddr2
		[ -n "$tdlsmacaddr2" ] && iwpriv "$ifname" tdlsmacaddr2 "$tdlsmacaddr2"

		config_get tdlsaction "$vif" tdlsaction
		[ -n "$tdlsaction" ] && iwpriv "$ifname" tdlsaction "$tdlsaction"

		config_get tdlsoffchan "$vif" tdlsoffchan
		[ -n "$tdlsoffchan" ] && iwpriv "$ifname" tdlsoffchan "$tdlsoffchan"

		config_get tdlsswitchtime "$vif" tdlsswitchtime
		[ -n "$tdlsswitchtime" ] && iwpriv "$ifname" tdlsswitchtime "$tdlsswitchtime"

		config_get tdlstimeout "$vif" tdlstimeout
		[ -n "$tdlstimeout" ] && iwpriv "$ifname" tdlstimeout "$tdlstimeout"

		config_get tdlsecchnoffst "$vif" tdlsecchnoffst
		[ -n "$tdlsecchnoffst" ] && iwpriv "$ifname" tdlsecchnoffst "$tdlsecchnoffst"

		config_get tdlsoffchnmode "$vif" tdlsoffchnmode
		[ -n "$tdlsoffchnmode" ] && iwpriv "$ifname" tdlsoffchnmode "$tdlsoffchnmode"

		config_get_bool blockdfschan "$vif" blockdfschan
		[ -n "$blockdfschan" ] && iwpriv "$ifname" blockdfschan "$blockdfschan"

		config_get dbgLVL "$vif" dbgLVL
		[ -n "$dbgLVL" ] && iwpriv "$ifname" dbgLVL "$dbgLVL"

		config_get acsmindwell "$vif" acsmindwell
		[ -n "$acsmindwell" ] && iwpriv "$ifname" acsmindwell "$acsmindwell"

		config_get acsmaxdwell "$vif" acsmaxdwell
		[ -n "$acsmaxdwell" ] && iwpriv "$ifname" acsmaxdwell "$acsmaxdwell"

		config_get acsreport "$vif" acsreport
		[ -n "$acsreport" ] && iwpriv "$ifname" acsreport "$acsreport"

		config_get ch_hop_en "$vif" ch_hop_en
		[ -n "$ch_hop_en" ] && iwpriv "$ifname" ch_hop_en "$ch_hop_en"

		config_get ch_long_dur "$vif" ch_long_dur
		[ -n "$ch_long_dur" ] && iwpriv "$ifname" ch_long_dur "$ch_long_dur"

		config_get ch_nhop_dur "$vif" ch_nhop_dur
		[ -n "$ch_nhop_dur" ] && iwpriv "$ifname" ch_nhop_dur "$ch_nhop_dur"

		config_get ch_cntwn_dur "$vif" ch_cntwn_dur
		[ -n "$ch_cntwn_dur" ] && iwpriv "$ifname" ch_cntwn_dur "$ch_cntwn_dur"

		config_get ch_noise_th "$vif" ch_noise_th
		[ -n "$ch_noise_th" ] && iwpriv "$ifname" ch_noise_th "$ch_noise_th"

		config_get ch_cnt_th "$vif" ch_cnt_th
		[ -n "$ch_cnt_th" ] && iwpriv "$ifname" ch_cnt_th "$ch_cnt_th"

		config_get_bool scanchevent "$vif" scanchevent
		[ -n "$scanchevent" ] && iwpriv "$ifname" scanchevent "$scanchevent"

		config_get_bool send_add_ies "$vif" send_add_ies
		[ -n "$send_add_ies" ] && iwpriv "$ifname" send_add_ies "$send_add_ies"

		config_get_bool ext_ifu_acs "$vif" ext_ifu_acs
		[ -n "$ext_ifu_acs" ] && iwpriv "$ifname" ext_ifu_acs "$ext_ifu_acs"

		config_get_bool rrm "$vif" rrm
		[ -n "$rrm" ] && iwpriv "$ifname" rrm "$rrm"

		config_get_bool rrmslwin "$vif" rrmslwin
		[ -n "$rrmslwin" ] && iwpriv "$ifname" rrmslwin "$rrmslwin"

		config_get_bool rrmstats "$vif" rrmsstats
		[ -n "$rrmstats" ] && iwpriv "$ifname" rrmstats "$rrmstats"

		config_get rrmdbg "$vif" rrmdbg
		[ -n "$rrmdbg" ] && iwpriv "$ifname" rrmdbg "$rrmdbg"

		config_get acparams "$vif" acparams
		[ -n "$acparams" ] && iwpriv "$ifname" acparams $acparams

		config_get setwmmparams "$vif" setwmmparams
		[ -n "$setwmmparams" ] && iwpriv "$ifname" setwmmparams $setwmmparams

		config_get_bool qbssload "$vif" qbssload
		[ -n "$qbssload" ] && iwpriv "$ifname" qbssload "$qbssload"

		config_get_bool proxyarp "$vif" proxyarp
		[ -n "$proxyarp" ] && iwpriv "$ifname" proxyarp "$proxyarp"

		config_get_bool dgaf_disable "$vif" dgaf_disable
		[ -n "$dgaf_disable" ] && iwpriv "$ifname" dgaf_disable "$dgaf_disable"

		config_get setibssdfsparam "$vif" setibssdfsparam
		[ -n "$setibssdfsparam" ] && iwpriv "$ifname" setibssdfsparam "$setibssdfsparam"

		config_get startibssrssimon "$vif" startibssrssimon
		[ -n "$startibssrssimon" ] && iwpriv "$ifname" startibssrssimon "$startibssrssimon"

		config_get setibssrssihyst "$vif" setibssrssihyst
		[ -n "$setibssrssihyst" ] && iwpriv "$ifname" setibssrssihyst "$setibssrssihyst"

		config_get noIBSSCreate "$vif" noIBSSCreate
		[ -n "$noIBSSCreate" ] && iwpriv "$ifname" noIBSSCreate "$noIBSSCreate"

		config_get setibssrssiclass "$vif" setibssrssiclass
		[ -n "$setibssrssiclass" ] && iwpriv "$ifname" setibssrssiclass $setibssrssiclass

		config_get offchan_tx_test "$vif" offchan_tx_test
		[ -n "$offchan_tx_test" ] && iwpriv "$ifname" offchan_tx_test $offchan_tx_test

		handle_vow_dbg_cfg() {
			local value="$1"
			iwpriv "$ifname" vow_dbg_cfg $value
		}

		config_list_foreach "$vif" vow_dbg_cfg handle_vow_dbg_cfg

		config_get_bool vow_dbg "$vif" vow_dbg
		[ -n "$vow_dbg" ] && iwpriv "$ifname" vow_dbg "$vow_dbg"

		brctl addif br0 "$ifname"
		handle_set_max_rate() {
			local value="$1"
			wlanconfig "$ifname" set_max_rate $value
		}
		config_list_foreach "$vif" set_max_rate handle_set_max_rate

		config_get_bool vap_only "$vif" vap_only 0
		if [ "$vap_only" = "1" ]; then
			start_hostapd=
		elif [ "$start_hostapd" = "" ]; then
			ifconfig "$ifname" up
		fi
		brctl addif br0 "$ifname"

		#support nawds
		config_get nawds_mode "$vif" nawds_mode
		[ -n "$nawds_mode" ] && wlanconfig "$ifname" nawds mode "${nawds_mode}"

		handle_nawds() {
			local value="$1"
			wlanconfig "$ifname" nawds add-repeater $value 1
		}
		config_list_foreach "$vif" nawds_add_repeater handle_nawds

		handle_hmwds() {
			local value="$1"
			wlanconfig "$ifname" hmwds add_addr $value
		}
		config_list_foreach "$vif" hmwds_add_addr handle_hmwds

		config_get nawds_override "$vif" nawds_override
		[ -n "$nawds_override" ] && wlanconfig "$ifname" nawds override "${nawds_override}"

		config_get nawds_defcaps "$vif" nawds_defcaps
		[ -n "$nawds_defcaps" ] && wlanconfig "$ifname" nawds defcaps "${nawds_defcaps}"

		handle_hmmc_add() {
			local value="$1"
			wlanconfig "$ifname" hmmc add $value
		}
		config_list_foreach "$vif" hmmc_add handle_hmmc_add

		config_get ODM "$device" ODM

		if [ "$ODM" != "dni" ]; then
			local net_cfg bridge
			net_cfg="$(find_net_config "$vif")"
			[ -z "$net_cfg" ] || {
				bridge="$(bridge_interface "$net_cfg")"
				config_set "$vif" bridge "$bridge"
				start_net "$ifname" "$net_cfg"
			}
		fi

		set_wifi_up "$vif" "$ifname"

		# TXPower settings only work if device is up already
		# while atheros hardware theoretically is capable of per-vif (even per-packet) txpower
		# adjustment it does not work with the current atheros hal/madwifi driver

		config_get vif_txpower "$vif" txpower
		# use vif_txpower (from wifi-iface) instead of txpower (from wifi-device) if
		# the latter doesn't exist
		txpower="${txpower:-$vif_txpower}"
		[ -z "$txpower" ] || iwconfig "$ifname" txpower "${txpower%%.*}"

		case "$mode" in
			ap)
				config_get_bool isolate "$vif" isolate 0
				iwpriv "$ifname" ap_bridge "$((isolate^1))"
                                config_get_bool lan_restricted "$vif" lan_restricted
                                if [ -n "$lan_restricted" ]; then
                                    config_get lan_ipaddr "$vif" lan_ipaddr
                                    wl_lan_restricted_access "$ifname" "$lan_ipaddr"
                                fi

				config_get_bool l2tif "$vif" l2tif
				[ -n "$l2tif" ] && iwpriv "$ifname" l2tif "$l2tif"

				if [ -n "$start_wapid" ]; then
					wapid_setup_vif "$vif" || {
						echo "enable_qcawifi($device): Failed to set up wapid for interface $ifname" >&2
						ifconfig "$ifname" down
						wlanconfig "$ifname" destroy
						continue
					}
				fi

				if [ -n "$start_hostapd" ] && eval "type hostapd_setup_vif" 2>/dev/null >/dev/null; then
					hostapd_setup_vif "$vif" atheros no_nconfig || {
						echo "enable_qcawifi($device): Failed to set up hostapd for interface $ifname" >&2
						# make sure this wifi interface won't accidentally stay open without encryption
						ifconfig "$ifname" down
						wlanconfig "$ifname" destroy
						continue
					}
				fi
			;;
			wds|sta)
				if eval "type wpa_supplicant_setup_vif" 2>/dev/null >/dev/null; then
					wpa_supplicant_setup_vif "$vif" athr || {
						echo "enable_qcawifi($device): Failed to set up wpa_supplicant for interface $ifname" >&2
						ifconfig "$ifname" down
						wlanconfig "$ifname" destroy
						continue
					}
				fi
			;;
			adhoc)
				if eval "type wpa_supplicant_setup_vif" 2>/dev/null >/dev/null; then
					wpa_supplicant_setup_vif "$vif" athr || {
						echo "enable_qcawifi($device): Failed to set up wpa"
						ifconfig "$ifname" down
						wlanconfig "$ifname" destroy
						continue
					}
				fi
		esac
	done

	# enable ol statistic in wireless driver
	iwpriv "$phy" enable_ol_stats 1

	# update wlan uptime file
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		isup=`ifconfig $ifname | grep UP`
		[ -n "$isup" ] && break
	done
	if [ "$isup" != "" -a "$phy" = "wifi0" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
	elif [ "$isup" != "" -a "$phy" = "wifi1" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
	fi

	# workaround for guest network cannot up
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		isup=`ifconfig $ifname | grep UP`
		no_assoc=`iwconfig $ifname | grep Not-Associated`
		if [ "$isup" != "" -a "$no_assoc" != "" ]; then
			ifconfig "$ifname" down
			ifconfig "$ifname" up
		fi
	done
}

wifitoggle_qcawifi()
{
    local device="$1"
    local hw_btn_state="$2"

    find_qcawifi_phy "$device" || return 1

    config_get vifs "$device" vifs
    for vif in $vifs; do
        config_get ifname "$vif" ifname
        if [ "$hw_btn_state" = "on" ]; then
            config_get mode "$vif" mode
            test -f /var/run/wifi-${ifname}.pid && kill $(cat /var/run/wifi-${ifname}.pid)
            if [ "$mode" = "ap" ]; then
                test -f /var/run/hostapd_cli-${ifname}.pid && kill $(cat /var/run/hostapd_cli-${ifname}.pid)
            fi
            ifconfig "$ifname" down
        else
            isup=`ifconfig $ifname | grep UP`
            [ -n "$isup" ] && continue
            config_get enc "$vif" encryption "none"
            case "$enc" in
                none)
                    # If we're in open mode and want to use WPS, we
                    # must start hostapd
                    config_get_bool wps_pbc "$vif" wps_pbc 0
                    config_get config_methods "$vif" wps_config
                    [ "$wps_pbc" -gt 0 ] && append config_methods push_button
                    if [ -n "$config_methods" ]; then 
                        hostapd_setup_vif "$vif" atheros no_nconfig
                    else
                        ifconfig "$ifname" up
                    fi
                    ;;
                mixed*|psk*|wpa*)
                    config_get mode "$vif" mode
                    if [ "$mode" = "ap" ]; then
                        hostapd_setup_vif "$vif" atheros no_nconfig
                    else
                        wpa_supplicant_setup_vif "$vif" athr
                    fi
                    ;;
                *)
                    ifconfig "$ifname" up
                    ;;
            esac
        fi
    done

    # update wlan uptime file
    for vif in $vifs; do
        config_get ifname "$vif" ifname
        isup=`ifconfig $ifname | grep UP`
        [ -n "$isup" ] && break
    done
    if [ "$isup" != "" -a "$phy" = "wifi0" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
    elif [ "$isup" != "" -a "$phy" = "wifi1" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
    elif [ "$isup" = "" -a "$phy" = "wifi0" ]; then
        rm /tmp/WLAN_uptime
    elif [ "$isup" = "" -a "$phy" = "wifi1" ]; then
        rm /tmp/WLAN_uptime_5G
    fi
}

wifischedule_qcawifi()
{
    local device="$1"
    local hw_btn_state="$2"
    local band="$3"
    local newstate="$4"

    find_qcawifi_phy "$device" || return 1

    config_get vifs "$device" vifs
    for vif in $vifs; do
        config_get ifname "$vif" ifname
        if [ "$newstate" = "on" -a "$hw_btn_state" = "on" ]; then
            isup=`ifconfig $ifname | grep UP`
            [ -n "$isup" ] && continue
            config_get enc "$vif" encryption "none"
            case "$enc" in
                none)
                    # If we're in open mode and want to use WPS, we
                    # must start hostapd
                    config_get_bool wps_pbc "$vif" wps_pbc 0
                    config_get config_methods "$vif" wps_config
                    [ "$wps_pbc" -gt 0 ] && append config_methods push_button
                    if [ -n "$config_methods" ]; then
                        hostapd_setup_vif "$vif" atheros no_nconfig
                    else
                        ifconfig "$ifname" up
                    fi
                    ;;
                mixed*|psk*|wpa*)
                    hostapd_setup_vif "$vif" atheros no_nconfig
                    ;;
                *)
                    ifconfig "$ifname" up
                    ;;
            esac
	    logger "[Wireless signal schedule] The wireless signal is ON,"
        else
            test -f /var/run/wifi-${ifname}.pid && kill $(cat /var/run/wifi-${ifname}.pid)
            test -f /var/run/hostapd_cli-${ifname}.pid && kill $(cat /var/run/hostapd_cli-${ifname}.pid)
            ifconfig "$ifname" down
	    logger "[Wireless signal schedule] The wireless signal is OFF,"
        fi
    done

    # update wlan uptime file
    for vif in $vifs; do
        config_get ifname "$vif" ifname
        isup=`ifconfig $ifname | grep UP`
        [ -n "$isup" ] && break
    done
    if [ "$isup" != "" -a "$phy" = "wifi0" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
    elif [ "$isup" != "" -a "$phy" = "wifi1" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
    elif [ "$isup" = "" -a "$phy" = "wifi0" ]; then
        rm /tmp/WLAN_uptime
    elif [ "$isup" = "" -a "$phy" = "wifi1" ]; then
        rm /tmp/WLAN_uptime_5G
    fi
}

wifiradio_qcawifi()
{
    local device=$1

    config_get vifs "$device" vifs

    shift
    while [ "$#" -gt "0" ]; do
        case $1 in
            -s|--status)
                for vif in $vifs; do
                    config_get ifname "$vif" ifname
                    isup=`ifconfig $ifname | grep UP`
                done
                [ -n "$isup" ] && echo "ON" || echo "OFF"
                shift
                ;;
            -c|--channel)
                for vif in $vifs; do
                    config_get ifname "$vif" ifname
                    isap=`iwconfig $ifname | grep Master`
                    [ -z "$isap" ] && continue
                    chan=`iwlist $ifname chan | grep Current | awk '{printf "%d\n", substr($5,1,length($5))}'`
                    echo "$chan"
                    break;
                done
                shift
                ;;
            --coext)
                for vif in $vifs; do
                    config_get ifname "$vif" ifname
                    isap=`iwconfig $ifname | grep Master`
                    [ -z "$isap" ] && continue
                    if [ "$2" = "on" ]; then
                        iwpriv $ifname disablecoext 0
                        iwpriv $ifname extbusythres 30
                    else
                        iwpriv $ifname disablecoext 1
                        iwpriv $ifname extbusythres 100
                    fi
                done
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done
}

wps_qcawifi()
{
    local device=$1
    local hostapd_if=/var/run/hostapd-${device}
    local supplicant_if=var/run/wpa_supplicant-${device}

    config_get vifs "$device" vifs
    vif=`echo $vifs | cut -d" " -f1`

    shift
    while [ "$#" -gt "0" ]; do
        case $1 in
            -c|--client_pin)
                    config_get ifname "$vif" ifname
                    [ -e "$hostapd_if/$ifname" ] && {
                        hostapd_cli -i "$ifname" -p "$hostapd_if" wps_cancel
                        sleep 1
                        hostapd_cli -i "$ifname" -p "$hostapd_if" wps_pin any $2
                    }
                shift 2
                ;;
            -p|--pbc_start)
                    config_get ifname "$vif" ifname
                    [ -e "$hostapd_if/$ifname" ] && {
                        hostapd_cli -i "$ifname" -p "$hostapd_if" wps_cancel
                        sleep 1
                        hostapd_cli -i "$ifname" -p "$hostapd_if" wps_pbc
                    }
                    [ -e "$supplicant_if/$ifname" ] && {
                        wpa_cli -i "$ifname" -p "$hostapd_if" wps_cancel
                        sleep 1
                        wpa_cli -i "$ifname" -p "$hostapd_if" wps_pbc
                    }
                shift
                ;;
            -s|--wps_stop)
                    config_get ifname "$vif" ifname
                    [ -e "$hostapd_if/$ifname" ] && hostapd_cli -i "$ifname" -p "$hostapd_if" wps_cancel
                    [ -e "$supplicant_if/$ifname" ] && wpa_cli -i "$ifname" -p "$hostapd_if" wps_cancel
                shift
                ;;
            *)
                shift
                ;;
        esac
    done
}

wifitrfled_qcawifi()
{
    local device=$1
    local led_option=$2

    [ "$device" != "wifi0" ] && return

    if [ "$led_option" = "0" ]; then
        echo 1 > /proc/sys/qca_dni/blink_2g_led
    elif [ "$led_option" = "1" ]; then
        echo 0 > /proc/sys/qca_dni/blink_2g_led
    elif [ "$led_option" = "2" ]; then
        echo 0 > /proc/sys/qca_dni/blink_2g_led
        /usr/sbin/iwpriv wifi0 gpio_output 1 1
    fi
}

statistic_qcawifi()
{
    local device=$1

    ifconfig $1 > /dev/null 2>&1
    if [ "$?" = "0" ]; then
        tx_packets=`athstats -i $1 -ol | grep 'ast_tx_packets' | cut -f 2`
        rx_packets=`athstats -i $1 -ol | grep 'ast_rx_packets' | cut -f 2`
        collisions=0
        tx_bytes=`athstats -i $1 -ol | grep 'ast_tx_bytes' | cut -f 2`
        rx_bytes=`athstats -i $1 -ol | grep 'ast_rx_bytes' | cut -f 2`
        
        echo "###$1###"
        echo "TX packets:$tx_packets"
        echo "RX packets:$rx_packets"
        echo "collisons:$collisions"
        echo "TX bytes:$tx_bytes"
        echo "RX bytes:$rx_bytes"
        echo ""
    fi
}

pre_qcawifi() {
	local action=${1}

	config_load wireless

	case "${action}" in
		disable)
			eval "type icm_teardown" >/dev/null 2>&1 && icm_teardown

			config_get_bool wps_vap_tie_dbdc qcawifi wps_vap_tie_dbdc 0

			if [ $wps_vap_tie_dbdc -ne 0 ]; then
				kill "$(cat "/var/run/hostapd.pid")"
				[ -f "/tmp/hostapd_conf_filename" ] &&
					rm /tmp/hostapd_conf_filename
			fi

		;;
	esac
}

qcawifi_start_hostapd_cli() {
	local device=$1
	local ifidx=0
	local radioidx=${device#wifi}

	config_get vifs $device vifs

	for vif in $vifs; do
		local config_methods vifname

		config_get vifname "$vif" ifname

		if [ -n $vifname ]; then
			[ $ifidx -gt 0 ] && vifname="ath${radioidx}$ifidx" || vifname="ath${radioidx}"
		fi

		config_get_bool wps_pbc "$vif" wps_pbc 0
		config_get config_methods "$vif" wps_config
		[ "$wps_pbc" -gt 0 ] && append config_methods push_button

		if [ -n "$config_methods" ]; then
			pid=/var/run/hostapd_cli-$vifname.pid
			hostapd_cli -i $vifname -P $pid -a /lib/wifi/wps-hostapd-update-uci -p /var/run/hostapd-$device -B
		fi

		ifidx=$(($ifidx + 1))
	done
}

post_qcawifi() {
	local action=${1}

	case "${action}" in
		enable)
			local icm_enable
			config_get_bool icm_enable icm enable 0
			[ ${icm_enable} -gt 0 ] && \
					eval "type icm_setup" >/dev/null 2>&1 && {
				icm_setup
			}

			# Run a single hostapd instance for all the radio's
			# Enables WPS VAP TIE feature

			config_get_bool wps_vap_tie_dbdc qcawifi wps_vap_tie_dbdc 0

			if [ $wps_vap_tie_dbdc -ne 0 ]; then
				hostapd_conf_file=$(cat "/tmp/hostapd_conf_filename")
				hostapd -P /var/run/hostapd.pid $hostapd_conf_file -B
				config_foreach qcawifi_start_hostapd_cli wifi-device
			fi

		;;
	esac
}

check_qcawifi_device() {
	[ ${1%[0-9]} = "wifi" ] && config_set "$1" phy "$1"
	config_get phy "$1" phy
	[ -z "$phy" ] && {
		find_qcawifi_phy "$1" >/dev/null || return 1
		config_get phy "$1" phy
	}
	[ "$phy" = "$dev" ] && found=1
}


detect_qcawifi() {
	local ODM=$1
	local noinsert=$2
	devidx=0
	[ "$ODM" = "dni" -a "$noinsert" = "1" ] || load_qcawifi
	config_load wireless
	while :; do
		config_get type "radio$devidx" type
		[ -n "$type" ] || break
		devidx=$(($devidx + 1))
	done
	cd /sys/class/net
	[ -d wifi0 ] || return
	for dev in $(ls -d wifi* 2>&-); do
		found=0
		config_foreach check_qcawifi_device wifi-device
		[ "$found" -gt 0 ] && continue

		hwcaps=$(cat ${dev}/hwcaps)
		case "${hwcaps}" in
			*11bgn) mode_11=ng;;
			*11abgn) mode_11=ng;;
			*11an) mode_11=na;;
			*11an/ac) mode_11=ac;;
			*11abgn/ac) mode_11=ac;;
		esac

		cat <<EOF
config wifi-device  wifi$devidx
	option type	qcawifi
	option channel	auto
	option macaddr	$(cat /sys/class/net/${dev}/address)
	option hwmode	11${mode_11}
	# REMOVE THIS LINE TO ENABLE WIFI:
	option disabled 1

config wifi-iface
	option device	wifi$devidx
	option network	lan
	option mode	ap
	option ssid	OpenWrt
	option encryption none

EOF
	devidx=$(($devidx + 1))
	done
}

wl_lan_restricted_access()
{
    ifname=$1
    lan_ipaddr=$2
    ETH_P_ARP=0x0806
    ETH_P_RARP=0x8035
    ETH_P_IP=0x0800
    ETH_P_IPv6=0x86dd
    IPPROTO_ICMP=1
    IPPROTO_UDP=17
    IPPROTO_ICMPv6=58
    DHCPS_DHCPC=67:68
    DHCP6S_DHCP6C=546:547
    PORT_DNS=53
    lan_ipv6addr=$(ifconfig br0 | grep Scope:Link | awk '{print $3}' | awk -F '/' '{print $1}')

    ebtables -D FORWARD -p "$ETH_P_ARP" -j ACCEPT
    ebtables -D FORWARD -p "$ETH_P_RARP" -j ACCEPT
    ebtables -D FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
    ebtables -L | grep  "ath" > /tmp/wifi_rules
    while read loop
    do
        ebtables -D INPUT "$loop";
        ebtables -D FORWARD "$loop";
    done < /tmp/wifi_rules
    rm  /tmp/wifi_rules
    ebtables -P FORWARD ACCEPT
    ebtables -A FORWARD -p "$ETH_P_ARP" -j ACCEPT
    ebtables -A FORWARD -p "$ETH_P_RARP" -j ACCEPT
    ebtables -A FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -P INPUT ACCEPT
    ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT

    ebtables -A FORWARD -i "$ifname" -j DROP
    ebtables -A FORWARD -o "$ifname" -j DROP
    ebtables -A INPUT -i "$ifname" -p "$ETH_P_IP" --ip-dst "$lan_ipaddr" -j DROP
    ebtables -A INPUT -i "$ifname" -p "$ETH_P_IPv6" --ip6-dst "$lan_ipv6addr" -j DROP
}

clear_wifi_ebtables()
{
    ETH_P_ARP=0x0806
    ETH_P_RARP=0x8035
    ETH_P_IP=0x0800
    ETH_P_IPv6=0x86dd
    IPPROTO_ICMP=1
    IPPROTO_UDP=17
    IPPROTO_ICMPv6=58
    DHCPS_DHCPC=67:68
    DHCP6S_DHCP6C=546:547
    PORT_DNS=53

    ebtables -D FORWARD -p "$ETH_P_ARP" -j ACCEPT
    ebtables -D FORWARD -p "$ETH_P_RARP" -j ACCEPT
    ebtables -D FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
    ebtables -L | grep  "ath" > /tmp/wifi_rules
    while read loop
    do
        ebtables -D INPUT $loop;
        ebtables -D FORWARD $loop;
    done < /tmp/wifi_rules
    rm  /tmp/wifi_rules
}
