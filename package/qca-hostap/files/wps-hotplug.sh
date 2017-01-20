#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

if [ "$ACTION" = "pressed" -a "$BUTTON" = "wps" ]; then
	for dir in /var/run/hostapd-*; do
		[ -d "$dir" ] || continue
		for vap_dir in $dir/ath*; do
		[ -r "$vap_dir" ] || continue
		hostapd_cli -i "${vap_dir#"$dir/"}" -p "$dir" wps_pbc
		done
	done
fi
