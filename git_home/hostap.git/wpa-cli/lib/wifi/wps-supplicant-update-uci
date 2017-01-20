#!/bin/sh
#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

IFNAME=$1
CMD=$2
. /sbin/wifi detect
is_section_ifname() {
	local config=$1
	local ifname
	config_get ifname "$config" ifname
	[ "${ifname}" = "$2" ] && echo ${config}
}

get_psk() {
	local count=
	local conf=$1
	local index=
	count=$(awk 'BEGIN{FS="="} /psk=/ {print $0}' $conf |grep "psk=" |wc -l)
	index=$(($count*2))
	psk=$(awk 'BEGIN{ORS=""} /psk/ {print $0}' $conf | cut -f $index -d\")
}

local psk=
local ssid=
local wpa_version=

case "$CMD" in
	CONNECTED)
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME save_config
		ssid=$(wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME status | grep ^ssid= | cut -f2- -d =)
		wpa_version=$(wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME status | grep ^key_mgmt= | cut -f2- -d=)
		get_psk /var/run/wpa_supplicant-$IFNAME.conf
		local section=$(config_foreach is_section_ifname wifi-iface $IFNAME)
		case $wpa_version in
			WPA2-PSK)
				uci set wireless.${section}.encryption='psk2'
				uci set wireless.${section}.key=$psk
				;;
			WPA-PSK)
				uci set wireless.${section}.encryption='psk'
				uci set wireless.${section}.key=$psk
				;;
			NONE)
				uci set wireless.${section}.encryption='none'
				uci set wireless.${section}.key=''
				;;
		esac
		uci set wireless.${section}.ssid="$ssid"
		uci commit
		kill "$(cat "/var/run/wps-hotplug-$IFNAME.pid")"
		#post hotplug event to whom take care of
		env -i ACTION="wps-connected" INTERFACE=$IFNAME /sbin/hotplug-call iface
		;;
	WPS-TIMEOUT)
		kill "$(cat "/var/run/wps-hotplug-$IFNAME.pid")"
		env -i ACTION="wps-timeout" INTERFACE=$IFNAME /sbin/hotplug-call iface
		;;
	DISCONNECTED)
		kill "$(cat "/var/run/wps-hotplug-$IFNAME.pid")"
		;;
esac

