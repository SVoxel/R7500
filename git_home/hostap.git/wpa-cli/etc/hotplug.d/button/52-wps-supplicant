#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

local pid
if [ "$ACTION" = "pressed" -a "$BUTTON" = "wps" ]; then
	for dir in /var/run/wpa_supplicant-*; do
		[ -d "$dir" ] || continue
		pid=/var/run/wps-hotplug-${dir#"/var/run/wpa_supplicant-"}.pid
		wpa_cli -p "$dir" wps_pbc
		[ -f $pid ] || {
			wpa_cli -p"$dir" -a/lib/wifi/wps-supplicant-update-uci -P$pid -B
		}
	done
fi
