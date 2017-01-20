#
# Copyright (c) 2014 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

local wps_possible config_methods

hostapd_set_bss_options() {
	local var="$1"
	local vif="$2"
	local enc wep_rekey wpa_group_rekey wpa_strict_rekey wpa_pair_rekey wpa_master_rekey pid
	local add_sha256_str ieee80211r_str

	config_get enc "$vif" encryption "none"
	config_get wep_rekey        "$vif" wep_rekey        # 300
	config_get wpa_group_rekey  "$vif" wpa_group_rekey 0 # 300
	config_get wpa_strict_rekey  "$vif" wpa_strict_rekey 0 # 300
	config_get wpa_pair_rekey   "$vif" wpa_pair_rekey   # 300
	config_get wpa_master_rekey "$vif" wpa_master_rekey # 640
	config_get_bool ap_isolate "$vif" isolate 0
	config_get_bool ieee80211r "$vif" ieee80211r 0
	config_get kh_key_hex "$vif" kh_key_hex "000102030405060708090a0b0c0d0e0f"

	config_get device "$vif" device
	config_get hwmode "$device" hwmode
	config_get phy "$device" phy

	append "$var" "ctrl_interface=/var/run/hostapd-$phy" "$N"

	if [ "$ap_isolate" -gt 0 ]; then
		append "$var" "ap_isolate=$ap_isolate" "$N"
	fi

	# Examples:
	# psk-mixed/tkip 	=> WPA1+2 PSK, TKIP
	# wpa-psk2/tkip+aes	=> WPA2 PSK, CCMP+TKIP
	# wpa2/tkip+aes 	=> WPA2 RADIUS, CCMP+TKIP
	# ...

	# TODO: move this parsing function somewhere generic, so that
	# later it can be reused by drivers that don't use hostapd

	# crypto defaults: WPA2 vs WPA1
	case "$enc" in
		none)
			wpa=0
			crypto=
		;;
		wpa2*|*psk2*)
			wpa=2
			crypto="CCMP"
		;;
		*)
			# TKIP alone is now prohibited by WFA so the only
			# combination left must be CCMP+TKIP (wpa=3)
			wpa=3
			crypto="CCMP TKIP"
		;;
	esac

	# explicit override for crypto setting
	case "$enc" in
		*tkip+aes|*tkip+ccmp|*aes+tkip|*ccmp+tkip) crypto="CCMP TKIP";;
		*aes|*ccmp) crypto="CCMP";;
		*tkip) crypto="TKIP";;
	esac

	# WPA TKIP alone is no longer allowed for certification
	case "$hwmode:$crypto" in
		*:TKIP) crypto="CCMP TKIP";;
	esac

	# use crypto/auth settings for building the hostapd config
	case "$enc" in
		none)
			wps_possible=1
			# Here we make the assumption that if we're in open mode
			# with WPS enabled, we got to be in unconfigured state.
			wps_configured_state=1
		;;
		*psk*)
			config_get psk "$vif" key
			if [ ${#psk} -eq 64 ]; then
				append "$var" "wpa_psk=$psk" "$N"
			else
				append "$var" "wpa_passphrase=$psk" "$N"
			fi
			wps_possible=1
			# By default we assume we are in configured state,
			# while the user has the provision to override this.
			wps_configured_state=2
			[ -n "$wpa_group_rekey"  ] && append "$var" "wpa_group_rekey=$wpa_group_rekey" "$N"
			[ -n "$wpa_strict_rekey"  ] && append "$var" "wpa_strict_rekey=$wpa_strict_rekey" "$N"
			[ -n "$wpa_pair_rekey"   ] && append "$var" "wpa_ptk_rekey=$wpa_pair_rekey"    "$N"
			[ -n "$wpa_master_rekey" ] && append "$var" "wpa_gmk_rekey=$wpa_master_rekey"  "$N"
		;;
		*wpa*)
			# required fields? formats?
			# hostapd is particular, maybe a default configuration for failures
			config_get auth_server "$vif" auth_server
			[ -z "$auth_server" ] && config_get auth_server "$vif" server
			append "$var" "auth_server_addr=$auth_server" "$N"
			config_get auth_port "$vif" auth_port
			[ -z "$auth_port" ] && config_get auth_port "$vif" port
			auth_port=${auth_port:-1812}
			append "$var" "auth_server_port=$auth_port" "$N"
			config_get auth_secret "$vif" auth_secret
			[ -z "$auth_secret" ] && config_get auth_secret "$vif" key
			append "$var" "auth_server_shared_secret=$auth_secret" "$N"
			config_get acct_server "$vif" acct_server
			[ -n "$acct_server" ] && append "$var" "acct_server_addr=$acct_server" "$N"
			config_get acct_port "$vif" acct_port
			[ -n "$acct_port" ] && acct_port=${acct_port:-1813}
			[ -n "$acct_port" ] && append "$var" "acct_server_port=$acct_port" "$N"
			config_get acct_secret "$vif" acct_secret
			[ -n "$acct_secret" ] && append "$var" "acct_server_shared_secret=$acct_secret" "$N"
			config_get eap_reauth_period "$vif" eap_reauth_period
			[ -n "$eap_reauth_period" ] && append "$var" "eap_reauth_period=$eap_reauth_period" "$N"
			config_get nasid "$vif" nasid
			config_get wep_key_len_broadcast "$vif" wep_key_len_broadcast
			config_get wep_key_len_unicast "$vif" wep_key_len_unicast
			append "$var" "nas_identifier=$nasid" "$N"
			append "$var" "eapol_key_index_workaround=1" "$N"
			append "$var" "ieee8021x=1" "$N"
			[ -n "$wpa_group_rekey"  ] && append "$var" "wpa_group_rekey=$wpa_group_rekey" "$N"
			[ -n "$wpa_strict_rekey"  ] && append "$var" "wpa_strict_rekey=$wpa_strict_rekey" "$N"
			[ -n "$wpa_pair_rekey"   ] && append "$var" "wpa_ptk_rekey=$wpa_pair_rekey"    "$N"
			[ -n "$wpa_master_rekey" ] && append "$var" "wpa_gmk_rekey=$wpa_master_rekey"  "$N"
			[ -n "$wep_key_len_broadcast" ] && append "$var" "wep_key_len_broadcast=$wep_key_len_broadcast" "$N"
			[ -n "$wep_key_len_unicast" ] && append "$var" "wep_key_len_unicast=$wep_key_len_unicast" "$N"
			[ -n "$wep_rekey" ] && append "$var" "wep_rekey_period=$wep_rekey" "$N"
		;;
		*wep*)
			config_get key "$vif" key
			key="${key:-1}"
			case "$key" in
				[1234])
					for idx in 1 2 3 4; do
						local zidx
						zidx=$(($idx - 1))
						config_get ckey "$vif" "key${idx}"
						[ -n "$ckey" ] && \
							append "$var" "wep_key${zidx}=$(prepare_key_wep "$ckey")" "$N"
					done
					append "$var" "wep_default_key=$((key - 1))"  "$N"
				;;
				*)
					append "$var" "wep_key0=$(prepare_key_wep "$key")" "$N"
					append "$var" "wep_default_key=0" "$N"
					[ -n "$wep_rekey" ] && append "$var" "wep_rekey_period=$wep_rekey" "$N"
				;;
			esac
			case "$enc" in
				*shared*)
					auth_algs=2
				;;
				*mixed*)
					auth_algs=3
				;;
			esac
			wpa=0
			crypto=
		;;
		8021x)
			# For Dynamic WEP 802.1x,maybe need more fields
			config_get auth_server "$vif" auth_server
			[ -z "$auth_server" ] && config_get auth_server "$vif" server
			append "$var" "auth_server_addr=$auth_server" "$N"
			config_get auth_port "$vif" auth_port
			[ -z "$auth_port" ] && config_get auth_port "$vif" port
			auth_port=${auth_port:-1812}
			append "$var" "auth_server_port=$auth_port" "$N"
			config_get auth_secret "$vif" auth_secret
			[ -z "$auth_secret" ] && config_get auth_secret "$vif" key
			config_get eap_reauth_period "$vif" eap_reauth_period
			[ -n "$eap_reauth_period" ] && append "$var" "eap_reauth_period=$eap_reauth_period" "$N"
			config_get wep_rekey "$vif" wep_rekey 300

			config_get nasid "$vif" nasid
			append "$var" "nas_identifier=$nasid" "$N"
			append "$var" "ieee8021x=1" "$N"
			append "$var" "auth_server_shared_secret=$auth_secret" "$N"
			append "$var" "wep_rekey_period=$wep_rekey" "$N"
			append "$var" "eap_server=0" "$N"
			append "$var" "eapol_version=2" "$N"
			append "$var" "eapol_key_index_workaround=0" "$N"
			append "$var" "wep_key_len_broadcast=13" "$N"
			append "$var" "wep_key_len_unicast=13" "$N"
			auth_algs=1
			wpa=0
			crypto=
		;;
		*)
			wpa=0
			crypto=
		;;
	esac
	append "$var" "auth_algs=${auth_algs:-1}" "$N"
	append "$var" "wpa=$wpa" "$N"
	[ -n "$crypto" ] && append "$var" "wpa_pairwise=$crypto" "$N"
	[ -n "$wpa_group_rekey" ] && append "$var" "wpa_group_rekey=$wpa_group_rekey" "$N"
	[ -n "$wpa_strict_rekey" ] && append "$var" "wpa_strict_rekey=$wpa_strict_rekey" "$N"

        append "$var" "dualband=1" "$N"

	config_get ssid "$vif" ssid
	config_get bridge "$vif" bridge
	config_get ieee80211d "$vif" ieee80211d
	config_get iapp_interface "$vif" iapp_interface

	config_get_bool wps_pbc "$vif" wps_pbc 0
	config_get_bool wps_label "$vif" wps_label 0

	config_get config_methods "$vif" wps_config
	[ "$wps_pbc" -gt 0 ] && append config_methods push_button

	# WPS 2.0 test case 4.1.7:
	# if we're configured to enable WPS and we hide our SSID, then
	# we have to require an "explicit user operation to continue"
	config_get_bool hidden "$vif" hidden 0
	[ -n "$wps_possible" -a -n "$config_methods" -a "$hidden" -gt 0 ] && {
		echo "Hidden SSID is enabled on \"$ifname\", WPS will be automatically disabled"
		echo "Please press any key to continue."
		read -s -n 1
		wps_possible=
	}

	[ -n "$wps_possible" -a -n "$config_methods" ] && {
		config_get device_type "$vif" wps_device_type "6-0050F204-1"
		config_get device_name "$vif" wps_device_name "R7500"
		config_get manufacturer "$vif" wps_manufacturer "NTGR"
		config_get model_name "$vif" model_name "R7500"
		config_get model_number "$vif" model_number "R7500"
		config_get serial_number "$vif" serial_number "12345"
		config_get wps_pin "$vif" wps_pin "12345670"
		config_get wps_state "$vif" wps_state $wps_configured_state
		config_get ap_setup_locked "$vif" ap_setup_locked 0

		config_get_bool pbc_in_m1 "$vif" pbc_in_m1 1
		[ -n "$pbc_in_m1" ] && append "$var" "pbc_in_m1=$pbc_in_m1" "$N"

		config_get_bool ext_registrar "$vif" ext_registrar 1
		[ "$ext_registrar" -gt 0 -a -n "$bridge" ] && append "$var" "upnp_iface=$bridge" "$N"

		append "$var" "eap_server=1" "$N"
		append "$var" "ap_pin=$wps_pin" "$N"
		append "$var" "wps_state=$wps_state" "$N"
		append "$var" "ap_setup_locked=$ap_setup_locked" "$N"
		append "$var" "device_type=$device_type" "$N"
		append "$var" "device_name=$device_name" "$N"
		append "$var" "manufacturer=$manufacturer" "$N"
		append "$var" "model_name=$model_name" "$N"
		append "$var" "model_number=$model_number" "$N"
		append "$var" "serial_number=$serial_number" "$N"
		append "$var" "wps_upnp_ad_period=1800" "$N"
		append "$var" "wps_upnp_ad_ttl=4" "$N"
		append "$var" "manufacturer_url=http://www.netgear.com" "$N"
		append "$var" "friendly_name=WPS Access Point" "$N"
		append "$var" "model_description=$device_name" "$N"
		append "$var" "model_url=http://www.routerlogin.com" "$N"
		append "$var" "wps_disable_ie=0" "$N"
		append "$var" "config_methods=$config_methods" "$N"

		# fix the overlap session of WPS PBC for dual band AP
		macaddr=$(cat /sys/class/net/${bridge}/address)
		uuid=$(echo "$macaddr" | sed 's/://g')
		[ -n "$uuid" ] && {
			append "$var" "uuid=87654321-9abc-def0-1234-$uuid" "$N"
		}

	}

	append "$var" "ssid=$ssid" "$N"
	[ -n "$bridge" ] && append "$var" "bridge=$bridge" "$N"
	[ -n "$ieee80211d" ] && append "$var" "ieee80211d=$ieee80211d" "$N"
	[ -n "$iapp_interface" ] && append "$var" iapp_interface=$(uci_get_state network "$iapp_interface" ifname "$iapp_interface") "$N"

	if [ "$wpa" -ge "2" ]
	then
		# RSN -> allow preauthentication
		config_get rsn_preauth "$vif" rsn_preauth
		if [ -n "$bridge" -a "$rsn_preauth" = 1 ]
		then
			append "$var" "rsn_preauth=1" "$N"
			append "$var" "rsn_preauth_interfaces=$bridge" "$N"
		fi

		# RSN -> allow management frame protection
		local ieee80211w=
		config_get ieee80211w "$vif" ieee80211w 0
		case "$ieee80211w" in
			[012])
				append "$var" "ieee80211w=$ieee80211w" "$N"
				[ "$ieee80211w" -gt "0" ] && {
					config_get ieee80211w_max_timeout "$vif" ieee80211w_max_timeout
					config_get ieee80211w_retry_timeout "$vif" ieee80211w_retry_timeout
					[ -n "$ieee80211w_max_timeout" ] && \
						append "$var" "assoc_sa_query_max_timeout=$ieee80211w_max_timeout" "$N"
					[ -n "$ieee80211w_retry_timeout" ] && \
						append "$var" "assoc_sa_query_retry_timeout=$ieee80211w_retry_timeout" "$N"
				}
			;;
		esac

		# Allow SHA256
		case "$enc" in
			*wpa*) keymgmt=EAP;;
			*psk*) keymgmt=PSK;;
		esac
		config_get_bool add_sha256 "$vif" add_sha256 0
		[ "${add_sha256}" -gt 0 ] && add_sha256_str="WPA-${keymgmt}-SHA256"

		if [ "${ieee80211r}" -gt 0 ]
		then
			ieee80211r_str="FT-${keymgmt}"
		fi

		case "$ieee80211w" in
			[01]) append "$var" "wpa_key_mgmt=WPA-${keymgmt} ${add_sha256_str} ${ieee80211r_str}" "$N";;
			2) append "$var" "wpa_key_mgmt=WPA-${keymgmt}-SHA256 ${ieee80211r_str}" "$N";;
		esac
	fi

	config_get_bool hs20 "$vif" hs20 0
	if [ "$hs20" -gt 0 ]
	then
		append "$var" "hs20=1" "$N"
		config_get disable_dgaf "$vif" disable_dgaf
		[ -n "$disable_dgaf" ] && append "$var" "disable_dgaf=$disable_dgaf" "$N"

		add_hs20_operator_friendly_name() {
			append "$var" "hs20_operator_friendly_name=${1}" "$N"
		}

		config_list_foreach "$vif" hs20_oper_friendly_name add_hs20_operator_friendly_name

		add_hs20_connection_capability() {
			append "$var" "hs20_connection_capability=${1}" "$N"
		}

		config_list_foreach "$vif" hs20_conn_capab add_hs20_connection_capability

		config_get hs20_wan_metrics "$vif" hs20_wan_metrics
		[ -n "$hs20_wan_metrics" ] && append "$var" "hs20_wan_metrics=$hs20_wan_metrics" "$N"
		config_get hs20_operating_class "$vif" hs20_operating_class
		[ -n "$hs20_operating_class" ] && append "$var" "hs20_operating_class=$hs20_operating_class" "$N"

		append "$var" "interworking=1" "$N"
		append "$var" "manage_p2p=1" "$N"
		append "$var" "tdls_prohibit=1" "$N"
		config_get hessid "$vif" hessid
		[ -n "$hessid" ] && append "$var" "hessid=$hessid" "$N"
		config_get access_network_type "$vif" access_network_type
		[ -n "$access_network_type" ] && append "$var" "access_network_type=$access_network_type" "$N"
		config_get internet "$vif" internet
		[ -n "$internet" ] && append "$var" "internet=$internet" "$N"
		config_get asra "$vif" asra
		[ -n "$asra" ] && append "$var" "asra=$asra" "$N"
		config_get esr "$vif" esr
		[ -n "$esr" ] && append "$var" "esr=$esr" "$N"
		config_get uesa "$vif" uesa
		[ -n "$uesa" ] && append "$var" "uesa=$uesa" "$N"
		config_get venue_group "$vif" venue_group
		[ -n "$venue_group" ] && append "$var" "venue_group=$venue_group" "$N"
		config_get venue_type "$vif" venue_type
		[ -n "$venue_type" ] && append "$var" "venue_type=$venue_type" "$N"
		add_roaming_consortium() {
			append "$var" "roaming_consortium=${1}" "$N"
		}
		config_list_foreach "$vif" roaming_consortium add_roaming_consortium

		# qca-hostapd.conf has a hs20 prefix for all the below params
		add_hs20_venue_name() {
			append "$var" "hs20_venue_name=${1}" "$N"
		}
		config_list_foreach "$vif" venue_name add_hs20_venue_name

		config_get network_auth_type "$vif" network_auth_type
		[ -n "$network_auth_type" ] && append "$var" "hs20_network_auth_type=$network_auth_type" "$N"
		config_get ipaddr_type_availability "$vif" ipaddr_type_availability
		[ -n "$ipaddr_type_availability" ] && append "$var" "hs20_ipaddr_type_availability=$ipaddr_type_availability" "$N"


		add_hs20_domain_name() {
			append "$var" "hs20_domain_name_list=${1}" "$N"
		}

		config_list_foreach "$vif" domain_name add_hs20_domain_name

		config_get anqp_3gpp_cell_net "$vif" anqp_3gpp_cell_net
		[ -n "$anqp_3gpp_cell_net" ] && append "$var" "hs20_3gpp_cellular_network=$anqp_3gpp_cell_net" "$N"

		add_hs20_nai_realm() {
			append "$var" "hs20_nai_realm_list=${1}" "$N"
		}
		config_list_foreach "$vif" nai_realm add_hs20_nai_realm

		config_get qos_map_set "$vif" qos_map_set
		[ -n "$qos_map_set" ] && append "$var" "qos_map_set=$qos_map_set" "$N"
		config_get gas_frag_limit "$vif" gas_frag_limit
		[ -n "$gas_frag_limit" ] && append "$var" "gas_frag_limit=$gas_frag_limit" "$N"
		config_get gas_comeback_delay "$vif" gas_comeback_delay
		[ -n "$gas_comeback_delay" ] && append "$var" "gas_comeback_delay=$gas_comeback_delay" "$N"

	fi

	if [ "$ieee80211r" -gt 0 ]
	then

		config_get mobility_domain "$vif" mobility_domain
		[ -n "$mobility_domain" ] && append "$var" "mobility_domain=$mobility_domain" "$N"
		config_get r0_key_lifetime "$vif" r0_key_lifetime 10000
		append "$var" "r0_key_lifetime=$r0_key_lifetime" "$N"
		config_get r1_key_holder "$vif" r1_key_holder
		[ -n "$r1_key_holder" ] && append "$var" "r1_key_holder=$r1_key_holder" "$N"
		config_get reassociation_deadline "$vif" reassociation_deadline 1000
		append "$var" "reassociation_deadline=$reassociation_deadline" "$N"
		config_get pmk_r1_push "$vif" pmk_r1_push 1
		append "$var" "pmk_r1_push=$pmk_r1_push" "$N"
		config_get ft_over_ds "$vif" ft_over_ds
		[ -n "$ft_over_ds" ] && append "$var" "ft_over_ds=$ft_over_ds" "$N"

		config_get nasid "$vif" nasid
		config_get nasid2 "$vif" nasid2
		config_get ap_macaddr "$vif" ap_macaddr
		config_get ap2_macaddr "$vif" ap2_macaddr
		config_get ap2_r1_key_holder "$vif" ap2_r1_key_holder

		append "$var" "r0kh=$ap_macaddr $nasid $kh_key_hex" "$N"
		append "$var" "r0kh=$ap2_macaddr $nasid2 $kh_key_hex" "$N"
		append "$var" "r1kh=$ap2_macaddr $ap2_r1_key_holder $kh_key_hex" "$N"
	fi
}

hostapd_set_log_options() {
	local var="$1"
	local cfg="$2"
	local log_level log_80211 log_8021x log_radius log_wpa log_driver log_iapp log_mlme

	config_get log_level "$cfg" log_level 2

	config_get_bool log_80211  "$cfg" log_80211  1
	config_get_bool log_8021x  "$cfg" log_8021x  1
	config_get_bool log_radius "$cfg" log_radius 1
	config_get_bool log_wpa    "$cfg" log_wpa    1
	config_get_bool log_driver "$cfg" log_driver 1
	config_get_bool log_iapp   "$cfg" log_iapp   1
	config_get_bool log_mlme   "$cfg" log_mlme   1

	local log_mask=$((       \
		($log_80211  << 0) | \
		($log_8021x  << 1) | \
		($log_radius << 2) | \
		($log_wpa    << 3) | \
		($log_driver << 4) | \
		($log_iapp   << 5) | \
		($log_mlme   << 6)   \
	))

	append "$var" "logger_syslog=$log_mask" "$N"
	append "$var" "logger_syslog_level=$log_level" "$N"
	append "$var" "logger_stdout=$log_mask" "$N"
	append "$var" "logger_stdout_level=$log_level" "$N"
}

hostapd_setup_vif() {
	local vif="$1" && shift
	local driver="$1" && shift
	local no_nconfig
	local ifname device channel hwmode

	hostapd_cfg=

	# These are flags that may or may not be used when calling
	# "hostapd_setup_vif()". These are not mandatory and may be called in
	# any order
	while [ $# -ne 0 ]; do
		local tmparg="$1" && shift
		case "$tmparg" in
		no_nconfig)
			no_nconfig=1
			;;
		esac
	done

	config_get ifname "$vif" ifname
	config_get device "$vif" device
	config_get channel "$device" channel
	config_get hwmode "$device" hwmode

	hostapd_set_log_options hostapd_cfg "$device"
	hostapd_set_bss_options hostapd_cfg "$vif"

	case "$hwmode" in
		*bg|*gdt|*gst|*fh) hwmode=g;;
		*adt|*ast) hwmode=a;;
	esac
	[ "$channel" = auto ] && channel=
	[ -n "$channel" -a -z "$hwmode" ] && wifi_fixup_hwmode "$device"
	cat > /var/run/hostapd-$ifname.conf <<EOF
driver=$driver
interface=$ifname
${channel:+channel=$channel}
$hostapd_cfg
EOF
	[ -z "${no_nconfig}" ] &&
		echo ${hwmode:+hw_mode=${hwmode#11}} >> /var/run/hostapd-$ifname.conf

	# Run a single hostapd instance for all the radio's
	# Enables WPS VAP TIE feature
	config_get_bool wps_vap_tie_dbdc qcawifi wps_vap_tie_dbdc 0

	if [ $wps_vap_tie_dbdc -ne 0 ]; then
		echo -e "/var/run/hostapd-$ifname.conf \c\h" >> /tmp/hostapd_conf_filename
	else
		hostapd -P /var/run/wifi-$ifname.pid -B /var/run/hostapd-$ifname.conf

		if [ -n "$wps_possible" -a -n "$config_methods" ]; then
			pid=/var/run/hostapd_cli-$ifname.pid
			hostapd_cli -i $ifname -P $pid -a /lib/wifi/wps-hostapd-update-uci -p /var/run/hostapd-$device -B
		fi
	fi

}

