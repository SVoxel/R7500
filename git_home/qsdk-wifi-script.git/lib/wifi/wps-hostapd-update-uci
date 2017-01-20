#!/bin/sh
#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

IFNAME=$1
CMD=$2
. /sbin/wlan detect 1
qca_hostapd_config_file=/var/run/hostapd-`echo $IFNAME`.conf

local parent=$(cat /sys/class/net/${IFNAME}/parent)

is_section_ifname() {
	local config=$1
	local ifname
	config_get ifname "$config" ifname
	[ "${ifname}" = "$2" ] && echo ${config}
}

case "$CMD" in
	WPS-NEW-AP-SETTINGS)
		local ssid=$(hostapd_cli -i$IFNAME -p/var/run/hostapd-$parent wps_get_config | grep ^ssid= | cut -f2- -d =)
		local wpa=$(hostapd_cli -i$IFNAME -p/var/run/hostapd-$parent wps_get_config | grep ^wpa= | cut -f2- -d=)
		local psk=$(hostapd_cli -i$IFNAME -p/var/run/hostapd-$parent wps_get_config | grep ^wpa_passphrase= | cut -f2- -d=)
		local wps_state=$(hostapd_cli -i$IFNAME -p/var/run/hostapd-$parent wps_get_config | grep ^wps_state= | cut -f2- -d=)
		local section=$(config_foreach is_section_ifname wifi-iface $IFNAME)

		case "$wpa" in
			3)
				uci set wireless.${section}.encryption='mixed-psk'
				uci set wireless.${section}.key=$psk
				;;
			2)
				uci set wireless.${section}.encryption='psk2'
				uci set wireless.${section}.key=$psk
				;;
			1)
				uci set wireless.${section}.encryption='psk'
				uci set wireless.${section}.key=$psk
				;;
			*)
				uci set wireless.${section}.encryption='none'
				uci set wireless.${section}.key=''
				;;
		esac

		uci set wireless.${section}.ssid="$ssid"
		uci set wireless.${section}.wps_state=$wps_state
		uci commit
		kill "$(cat "/var/run/hostapd-cli-$IFNAME.pid")"
		#post hotplug event to whom take care of
		env -i ACTION="wps-configured" INTERFACE=$IFNAME /sbin/hotplug-call iface
		;;
	WPS-TIMEOUT)
		kill "$(cat "/var/run/hostapd-cli-$IFNAME.pid")"
		env -i ACTION="wps-timeout" INTERFACE=$IFNAME /sbin/hotplug-call iface
		;;
	DISCONNECTED)
		kill "$(cat "/var/run/hostapd_cli-$IFNAME.pid")"
                ;;
	WPS-AP-SETUP-LOCKED)
		local section=$(config_foreach is_section_ifname wifi-iface $IFNAME)
		uci set wireless.${section}.ap_setup_locked=1
		uci commit

		[ -f "$qca_hostapd_config_file" ] || return
		sed -i '/^ap_setup_locked/d' $qca_hostapd_config_file
		echo "ap_setup_locked=1" >> $qca_hostapd_config_file
		;;
	WPS-AP-SETUP-UNLOCKED)
		#TODO: update DNI and UCI config
		[ -f "$qca_hostapd_config_file" ] || return
		sed -i '/^ap_setup_locked/d' $qca_hostapd_config_file
		echo "ap_setup_locked=0" >> $qca_hostapd_config_file
		;;
	WPS-AP-PIN-FAILURE)
		EVENT=ADD_FAILURE_NUM /lib/wifi/ap-pin-counter
		;;
esac
