#!/bin/sh
#
# Quantenna Inc..
#

append DRIVERS "qtwifi"

QT_LOG_FILE=/tmp/qt-wireless.log
QT_SESSION_FILE=/tmp/qt-session
QT_SESSION_TIMEOUT=600
QT_STATUS_FILE=/tmp/WLAN_5G_status

QT_MAX_VAP=2

ready_to_set=0

wait_for_ready() {
    local check_times=0
    local op_mode

    while [ $check_times -le 20 ]; do
        echo "qcsapi_sockrpc get_mode wifi0" >> $QT_LOG_FILE
        op_mode=`qcsapi_sockrpc get_mode wifi0`
        if [ "$op_mode" = "Access point" -o "$op_mode" = "Station" ]; then
            ready_to_set=1
            break;
        fi
        sleep 1
        check_times=$(($check_times + 1))
        echo "Check times = $check_times" >> $QT_LOG_FILE
    done
}

check_session_alive() {
    [ "$ready_to_set" = "1" ] && return

    if [ -f $QT_SESSION_FILE ]; then
        local last_check_time=`cat $QT_SESSION_FILE`
        local current_time=`cat /proc/uptime | awk -F . '{print $1}'`

        time_delta=$(( $current_time - $last_check_time ))
        [ $time_delta -le $QT_SESSION_TIMEOUT ] && {
            ready_to_set=1
            echo "Session is not expired yet, [$last_check_time]-[$current_time]" >> $QT_LOG_FILE
        }
    fi

    if [ "$ready_to_set" = "0" ]; then
        wait_for_ready
        [ "$ready_to_set" = "1" ] && {
            cat /proc/uptime | awk -F . '{print $1}' > $QT_SESSION_FILE
            echo "New Session ID is set to "`cat $QT_SESSION_FILE` >> $QT_LOG_FILE
        }
    fi
}

qt_call() {
    check_session_alive
    if [ "$ready_to_set" = "0" ]; then
        echo "cannot communicate with qt chip, exit qt_call"
        return
    fi

    index=$1
    req=$2
    shift
    shift
    local val_num= val1= val2= val3= val4= val=
    val_num=$#
    [ "$#" != "0" ] && val1=$1 && shift
    [ "$#" != "0" ] && val2=$1 && shift
    [ "$#" != "0" ] && val3=$1 && shift
    [ "$#" != "0" ] && val4=$1 && shift
    val=$@
    local do_wait="&"

    [ "$ready_to_set" = "0" ] && return
    case "$req" in
        set_passphrase|set_PSK)
            qt_wifi_intf="wifi${index} 0"
            ;;
        rename_SSID)
            cur_ssid=`qcsapi_sockrpc get_SSID_list wifi0`
            if [ -z "$cur_ssid" ]; then
                req="create_SSID"
            else
                [ "$cur_ssid" = "$val1" ] && return
                val2="$val1"
                val1="$cur_ssid"
                val_num=2
            fi
            qt_wifi_intf="wifi${index}"
            ;;
        disable_wps|wifi_create_bss|save_wps_ap_pin|apply_security_config)
            do_wait=
            qt_wifi_intf="wifi${index}"
            ;;
        rfenable)
            [ "$val1" -eq "1" ] && { 
                qevt_client -h 1.1.1.2 &
                ledcontrol -n wifi5g -c green -s on
                echo "ON" > $QT_STATUS_FILE
            }
            [ "$val1" -eq "0" ] && {
                killall qevt_client
                ledcontrol -n wifi5g -c green -s off
                echo "OFF" > $QT_STATUS_FILE
            }
            do_wait=
            qt_wifi_intf="wifi${index}"
            ;;
        set_wifi_macaddr)
            do_wait=
            qt_wifi_intf=""
            ;;
        *)
            qt_wifi_intf="wifi${index}"
            ;;
    esac


    echo "qcsapi_sockrpc $req $qt_wifi_intf $val1 $val2 $val3 $val4 $val $do_wait" >> $QT_LOG_FILE
    case "$val_num" in
        0)
            qcsapi_sockrpc $req $qt_wifi_intf $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
        1)
            qcsapi_sockrpc $req $qt_wifi_intf "$val1" $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
        2)
            qcsapi_sockrpc $req $qt_wifi_intf "$val1" "$val2" $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
        3)
            qcsapi_sockrpc $req $qt_wifi_intf "$val1" "$val2" "$val3" $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
        4)
            qcsapi_sockrpc $req $qt_wifi_intf "$val1" "$val2" "$val3" "$val4" $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
        *)
            qcsapi_sockrpc $req $qt_wifi_intf "$val1" "$val2" "$val3" "$val4" "$val" $do_wait 2>&1 >> $QT_LOG_FILE
            ;;
    esac
}


# Quantenna use RPC to communicate with its PCIe module through interface host0
# The default IP on PCIe module is 1.1.1.2, so add 1.1.1.1 to br-lan:1 to config
# Quantenna PCIe module
set_quantenna_config_ip() {
    local device=$1
    local config_ip
    local device_ip
    local qt_pcie_ip_conf=/etc/qcsapi_target_ip.conf
    local in_normal_mode=0
    local bridge_idx=1

    config_get config_ip "$device" config_host_ip
    config_get device_ip "$device" config_device_ip

    if [ -n "$device_ip" ]; then
        echo "$device_ip" > $qt_pcie_ip_conf
    fi

    config_get config_interface "$device" config_interface

    config_get vifs "$device" vifs
    for vif in $vifs; do
        config_get mode "$vif" mode
        [ "$mode" = "sta" ] && {
            config_ip=1.1.1.3
            config_set "$device" config_host_ip "$config_ip"
        }
        br_if=`cat /proc/net/dev | grep -e "br[0-9]" | awk -F: '{print $1}' | sed 's/ //g'`
        if [ -z "$br_if" ]; then
            config_get network "$vif" network
            br_if="br-$network"
        fi

        echo "bridge = $br_if, config_ip = $config_ip" >> $QT_LOG_FILE
        if ! eval "brctl show $br_if | grep $config_interface" 2>/dev/null > /dev/null; then
            brctl addif "$br_if" $config_interface
            echo "brctl addif $br_if $config_interface" >> $QT_LOG_FILE
            ifconfig $config_interface up
        fi
        ifconfig ${br_if}:${bridge_idx} $config_ip netmask 255.255.255.248 broadcast 1.1.1.7
        echo "ifconfig ${br_if}:${bridge_idx} $config_ip netmask 255.255.255.248 broadcast 1.1.1.7" >> $QT_LOG_FILE
        [ -z "$config_ip" ] && {
            if ! eval "ifconfig ${br_if}:${bridge_idx} | grep $config_ip" 2>/dev/null > /dev/null; then
                ifconfig ${br_if}:${bridge_idx} $config_ip netmask 255.255.255.248 broadcast 1.1.1.7
                echo "ifconfig ${br_if}:${bridge_idx} $config_ip netmask 255.255.255.248 broadcast 1.1.1.7"  >> $QT_LOG_FILE
            fi
        }
        # Quantenna can create MBSSID on his board, but all of them are bridged 
        # together to the host0 on the host device, so only one control
        # interface, host0, is enough. Stop here.
        break;
    done
}

qtn_hw_init() {
    local device="$1"
    local mode="$2"
    local op_mode
    local qtn_mac_addr

    config_get country "$device" country
    set_region_str region "$country"
    qt_call 0 update_persistent_param region "$region"

    config_get bf "$device" bf 1
    qt_call 0 update_persistent_param bf "$bf"

    op_mode=`qcsapi_sockrpc get_mode wifi0`
    qtn_mac_addr=`echo $(qcsapi_sockrpc get_macaddr wifi0) | tr 'a-z' 'A-Z'`

    config_get macaddr "$device" macaddr
    macaddr=`echo $macaddr | tr 'a-z' 'A-Z'`
    [ "$macaddr" != "$qtn_mac_addr" ] && {
        qt_call 0 set_wifi_macaddr "$macaddr"
        op_mode="NONE"
    }

    [ -z "$mode" ] && mode="ap"

    [ "$mode" = "ap" -a "$op_mode" = "Access point" ] || [ "$mode" = "sta" -a "$op_mode" = "Station" ] || {
        qt_call 0 reload_in_mode $mode
    }
    qt_call 0 rfenable 1

    config_get channel "$device" channel
    config_get hwmode "$device" hwmode auto
    config_get htmode "$device" htmode auto

    case "$hwmode:$htmode" in
        *na:HT20)
            chanbw=20
            vht=0
            ;;
        *na:HT40*)
            chanbw=40
            vht=0
            ;;
        *ac:HT20)
            chanbw=20
            vht=1
            ;;
        *ac:HT40*)
            chanbw=40
            vht=1
            ;;
        *ac:HT80)
            chanbw=80
            vht=1
            ;;
        *ac:*)
            chanbw=80
            vht=1
            ;;
        *)
            chanbw=80
            vht=1
            ;;
    esac

    qt_call 0 update_persistent_param vht $vht
    qt_call 0 set_bw $chanbw
    qt_call 0 update_persistent_param bw $chanbw
    qt_call 0 set_bb_param 1

    [ "$mode" = "sta" ] && return

    [ auto = "$channel" ] && channel=0
    qt_call 0 set_channel $channel
    qt_call 0 update_persistent_param channel $channel
    qt_call 0 enable_scs 0
    qt_call 0 update_persistent_param scs 0

    region=`qcsapi_sockrpc get_regulatory_region wifi0`
    [ "$region" = "cn" -o "$bf" -eq "1" ] && return
    local reduce_db=
    config_get tpscale "$device" tpscale
    [ "$tpscale" -eq "0" ] && return
    [ "$tpscale" -eq "1" ] && reduce_db=1
    [ "$tpscale" -eq "3" ] && reduce_db=2
    [ "$tpscale" -eq "5" ] && reduce_db=3
    if [ "$region" != "none" ]; then
        channels=`qcsapi_sockrpc get_list_regulatory_channels $region`
        channels=$(echo $channels | sed  -e 's/,/ /g')
    else
        return
    fi
    for chan in $channels
    do
        max_pow=`qcsapi_sockrpc get_configured_tx_power wifi0 $chan $region`
        new_pow=$(($max_pow-$reduce_db))
        qt_call 0 set_tx_power $chan $new_pow
    done
}

qtn_clear_settings(){
    local i=1
    local op_mode

    op_mode=`qcsapi_sockrpc get_mode wifi0`
    if [ "$op_mode" = "Access point" ]; then
        while [ "$i" != "$QT_MAX_VAP" ]; do
            qt_call $i wifi_remove_bss
            i=$(($i+1))
        done
    fi
}

enable_qtwifi() {
    local device="$1"
    local inited=0

    check_session_alive
    if [ "$ready_to_set" = "0" ]; then
        echo "cannot communicate with qt chip, exit enable_qtwifi"
        return
    fi

    wait_for_ready
    config_get vifs "$device" vifs
    for vif in $vifs; do
        local authproto= authtype= crypto=
        config_get mode "$vif" mode
        [ "$inited" = "0" ] && {
            qtn_hw_init $device $mode
            inited=1;
        }
        config_get confidx "$vif" confidx 0
        [ "$mode" = "sta" ] || qt_call $confidx wifi_create_bss
        config_get wdspeerlist "$vif" wdspeerlist
        [ -n "$wdspeerlist" ] && {
            for peermac in $wdspeerlist; do
                qt_call $confidx wds_add_peer $peermac
            done
        }

        config_get enc "$vif" encryption "none"
        config_get eap_type "$vif" eap_type
        case "$enc" in
            none)
                authproto="Basic"
                [ "$mode" = "sta" ] && authtype="NONE"
                ;;
            wep)
                authproto="Basic"
                [ "$mode" = "sta" ] && authtype="NONE"
                ;;
            *wpa_mixed*)
                authproto="WPAand11i"
                authtype="EAPAuthentication"
                crypto="TKIPandAESEncryption"
                ;;
            *wpa2*)
                authproto="11i"
                authtype="EAPAuthentication"
                crypto="AESEncryption"
                ;;
            *wpa*)
                authproto="WPA"
                authtype="EAPAuthentication"
                crypto="TKIPEncryption"
                ;;
            *mixed*)
                authproto="WPAand11i"
                authtype="PSKAuthentication"
                crypto="TKIPandAESEncryption"
                ;;
            *psk2*)
                authproto="11i"
                authtype="PSKAuthentication"
                crypto="AESEncryption"
                ;;
            *psk*)
                authproto="WPA"
                authtype="PSKAuthentication"
                crypto="TKIPEncryption"
                ;;
        esac

        case "$enc" in
            *tkip+aes|*tkip+ccmp|*aes+tkip|*ccmp+tkip) crypto="TKIPandAESEncryption";;
            *aes|*ccmp) crypto="AESEncryption";;
            *tkip) crypto="TKIPEncryption";;
        esac

        # enforce CCMP for 11ng and 11na
        case "$hwmode:$crypto" in
            *ng:TKIP|*na:TKIP*|*ac:TKIP*) crypto="TKIPandAESEncryption";;
        esac

        config_get ssid "$vif" ssid
        case "$mode" in
            ap)
                [ -n "$authproto" ] && qt_call $confidx set_beacon "$authproto"
                [ -n "$authtype" ] && qt_call $confidx set_WPA_authentication_mode  "$authtype"
                [ -n "$crypto" ] && {
                    qt_call $confidx set_WPA_encryption_modes "$crypto"
                    config_get key "$vif" key
                    [ -n "$key" -a "$authtype" != "EAPAuthentication" ] && {
                        if [ ${#key} -eq 64 ]; then
                            qt_call $confidx set_PSK "$key"
                        else
                            qt_call $confidx set_passphrase "$key"
                        fi
                    }
                }
                [ -n "$ssid" ] && {
                    qt_call $confidx set_ssid "$ssid"
                }
                ;;
            sta*)
                [ -n "$ssid" ] && {
                    qt_call $confidx rename_SSID "$ssid"
                }
                [ -n "$authproto" -a "$authproto" != "Basic" ] && qt_call $confidx SSID_set_proto "$ssid" "$authproto"
                [ -n "$authtype" ] && qt_call $confidx SSID_set_authentication_mode "$ssid"  "$authtype"
                [ -n "$crypto" ] && {
                    qt_call $confidx SSID_set_encryption_modes "$ssid" "$crypto"
                    config_get key "$vif" key
                    qt_call $confidx SSID_set_passphrase "$ssid" 0 "$key"
                }
                ;;
        esac


        config_get_bool hidden "$vif" hidden 0
        [ "$mode" = "sta" ] || qt_call $confidx set_option SSID_broadcast "$((hidden^1))"
        echo "ssid = $ssid"

        [ "$mode" = "sta" ] || {
            config_get maclist "$vif" maclist
            config_get macfilter "$vif" macfilter
            case "$macfilter" in
                allow)
                    qt_call $confidx set_macaddr_filter 2
                    ;;
                deny)
                    qt_call $confidx set_macaddr_filter 1
                    ;;
                *)
                    # default deny policy if mac list exists
                    if [ -n "$maclist" ]; then
                        qt_call $confidx set_macaddr_filter 1
                    else
                        qt_call $confidx set_macaddr_filter 0
                    fi
                    ;;
            esac

            # clear maclist
            qt_call $confidx clear_mac_filters
            [ -n "$maclist" ] && {
                case "$macfilter" in
                    allow)
                        for mac in $maclist; do
                            qt_call $confidx authorize_macaddr "$mac"
                        done
                        ;;
                    *)
                        for mac in $maclist; do
                            qt_call $confidx deny_macaddr "$mac"
                        done
                        ;;
                esac
            }

            config_get_bool lan_restricted "$vif" lan_restricted
            if [ -n "$lan_restricted" ]; then
                config_get lan_ipaddr "$vif" lan_ipaddr
                wla_lan_restricted_access "$lan_ipaddr"
            fi

        }

        if [ "$mode" != "sta" ]; then
            [ "$confidx" = "0" ] && wps_setting $vif
        fi

        config_get_bool isolate "$vif" isolate 0
        [ "$isolate" = "1" ] && {
            qt_call $confidx set_intra_bss_isolate 1
            qt_call $confidx set_bss_isolate 1
        }

        [ "$mode" = "sta" ] && /lib/wifi/get-5g-rx-rate &

    done

    config_get vifs "$device" vifs
    vif=`echo $vifs | cut -d" " -f1`
    config_get_bool vap_only "$vif" vap_only
    [ -n "$vap_only" ] && qt_call 0 rfenable 0

    isup=`qcsapi_sockrpc get_status wifi0 | grep Up`
    if [ "$isup" != "" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
    fi

    qt_call 0 apply_security_config
    return 0
}

wps_setting()
{
    local vif="$1"

    qt_call 0 wps_upnp_enable 0

    config_get wps_state "$vif" wps_state
    qt_call 0 set_wps_configured_state "$wps_state"

    config_get config_methods "$vif" wps_config
    qt_call 0 set_wps_param config_methods "$config_methods"

    # Netgear requirement wps timeout=120
    qt_call 0 wps_set_timeout 120

    config_get device_name "$vif" wps_device_name "OpenWrt AP"
    qt_call 0 set_wps_param device_name "$device_name"

    config_get wps_pin "$vif" wps_pin "12345670"
    qt_call 0 set_wps_ap_pin "$wps_pin"
    qt_call 0 save_wps_ap_pin

    qt_call 0 set_wps_param wps_vendor_spec Netgear

    config_get ap_setup_locked "$vif" ap_setup_locked 0
    qt_call 0 set_wps_param ap_setup_locked "$ap_setup_locked"
    if [ "$ap_setup_locked" = "0" ]; then
        /lib/wifi/5g-ap-pin-process &
    fi
}

wifitoggle_qtwifi()
{
    local device="$1"
    local hw_btn_state="$2"
    ready_to_set=1
    config_get vifs "$device" vifs
    if [ "$vifs" != "" ]; then
        if [ "$hw_btn_state" = "on" ]; then
            qt_call 0 rfenable 0
        else
            qt_call 0 rfenable 1
        fi
    fi

    isup=`qcsapi_sockrpc get_status wifi0 | grep Up`
    if [ "$isup" != "" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
    elif [ "$isup" = "" ]; then
        rm /tmp/WLAN_uptime_5G
    fi
}

wifischedule_qtwifi()
{
    local device="$1"
    local hw_btn_state="$2"
    local band="$3"
    local newstate="$4"
    ready_to_set=1
    if [ "$newstate" = "on" -a "$hw_btn_state" = "on" ]; then
        qt_call 0 rfenable 1
        logger "[Wireless signal schedule] The wireless signal is ON,"
    else
        qt_call 0 rfenable 0
        logger "[Wireless signal schedule] The wireless signal is OFF,"
    fi

    isup=`qcsapi_sockrpc get_status wifi0 | grep Up`
    if [ "$isup" != "" ]; then
        cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
    elif [ "$isup" = "" ]; then
        rm /tmp/WLAN_uptime_5G
    fi
}

wifiradio_qtwifi()
{
    local device=$1
    ready_to_set=1

    shift
    while [ "$#" -gt "0" ]; do
        case $1 in
            -s|--status)
                isup=`qcsapi_sockrpc get_status wifi0 | grep Up`
                [ -n "$isup" ] && echo "ON" || echo "OFF"
                shift
                ;;
            -c|--channel)
                isap=`qcsapi_sockrpc get_mode wifi0 | grep "Access point"`
                [ -z "$isap" ] && continue
                chan=`qcsapi_sockrpc get_channel wifi0`
                echo "$chan"
                shift
                ;;
            *)
                shift
                ;;
        esac
    done
}

wps_qtwifi()
{
    local device=$1
    ready_to_set=1

    isup=`qcsapi_sockrpc get_status wifi0 | grep Up`
    [ -z "$isup" ] && return

    shift
    while [ "$#" -gt "0" ]; do
        case $1 in
            -c|--client_pin)
                    [ -f "/tmp/wps_start_by_2g" ] && {
                        rm /tmp/wps_start_by_2g
                        return
                    }
                    ACTION="STOP_WPS" /lib/wifi/5g-wps-process
                    qt_call 0 registrar_report_pin $2
                    ACTION="SET_STATE" /lib/wifi/5g-wps-process &
                shift 2
                ;;
            -p|--pbc_start)
                    [ -f "/tmp/wps_start_by_2g" ] && {
                        rm /tmp/wps_start_by_2g
                        return
                    }
                    ACTION="STOP_WPS" /lib/wifi/5g-wps-process
                    qt_call 0 registrar_report_button_press
                    ACTION="SET_STATE" /lib/wifi/5g-wps-process &
                shift
                ;;
            -s|--wps_stop)
                    [ -f "/tmp/wps_start_by_2g" ] && rm /tmp/wps_start_by_2g
                    ACTION="STOP_WPS" /lib/wifi/5g-wps-process
                shift
                ;;
            *)
                shift
                ;;
        esac
    done
}

statistic_qtwifi()
{
    local device=$1
    ready_to_set=1
    local tx_packets=0
    local rx_packets=0
    local collisions=0
    local tx_bytes=0
    local rx_bytes=0

    config_get vifs "$device" vifs

    for vif in $vifs; do
        config_get confidx "$vif" confidx

        # need capture statisitc from foreground, so use qcsapi_sockrpc directly
        tx_packets=$((tx_packets+`qcsapi_sockrpc get_interface_stats wifi$confidx | grep 'tx_pkts' | cut -f 2`))
        rx_packets=$((tx_packets+`qcsapi_sockrpc get_interface_stats wifi$confidx | grep 'rx_pkts' | cut -f 2`))
        tx_bytes=$((tx_packets+`qcsapi_sockrpc get_interface_stats wifi$confidx | grep 'tx_bytes' | cut -f 2`))
        rx_bytes=$((tx_packets+`qcsapi_sockrpc get_interface_stats wifi$confidx | grep 'rx_bytes' | cut -f 2`))
    done

    echo "###$1###"
    echo "TX packets:$tx_packets"
    echo "RX packets:$rx_packets"
    echo "collisons:$collisions"
    echo "TX bytes:$tx_bytes"
    echo "RX bytes:$rx_bytes"
    echo ""
}

disable_qtwifi() {
    local device="$1"

    killall get-5g-rx-rate
    killall 5g-ap-pin-process

    rm -f $QT_LOG_FILE
    check_session_alive
    if [ "$ready_to_set" = "0" ]; then
        echo "cannot communicate with qt chip, exit disable_qtwifi"
        return
    fi

    config_get lan_ipaddr "$vif" lan_ipaddr
    clear_wifi5g_ebtables "$lan_ipaddr"

    qtn_clear_settings
    qt_call 0 rfenable 0
    rm /tmp/WLAN_uptime_5G
    return 0
}

scan_qtwifi() {
    local device="$1"
    local adhoc sta ap monitor mesh disabled

    set_quantenna_config_ip $device
    config_get vifs "$device" vifs
    for vif in $vifs; do
        config_get_bool disabled "$vif" disabled 0
        [ $disabled = 0 ] || continue

        config_get mode "$vif" mode
        case "$mode" in
            adhoc|sta|ap|monitor|mesh)
                append $mode "$vif"
            ;;
            *) echo "$device($vif): Invalid mode, ignored."; continue;;
        esac
    done

    config_set "$device" vifs "${ap:+$ap }${adhoc:+$adhoc }${sta:+$sta }${monitor:+$monitor }${mesh:+$mesh}"
}

check_qtwifi_device() {
    # [ ${1%[0-9]} = "wifi" ] && config_set "$1" phy "$1"
    # config_get phy "$1" phy
    # [ -z "$phy" ] && {
    #     config_get phy "$1" phy
    # }
    # [ "$phy" = "$dev" ] && found=1
    [ ${1%[0-9]} = "wifi" ] && {
        config_get config_interface "$1" config_interface
        [ "$config_interface" = "$confdev" ] && found=1
    }
}

set_region_str()
{
    local region_num=$2
    local tmp_str

    case "$region_num" in
        76)
            tmp_str="br"
            ;;
        156)
            tmp_str="cn"
            ;;
        276)
            tmp_str="eu"
            ;;
        376)
            tmp_str="il"
            ;;
        392)
            tmp_str="jp"
            ;;
        484)
            tmp_str="mx"
            ;;
        643)
            tmp_str="ru"
            ;;
        682)
            tmp_str="sa"
            ;;
        702)
            tmp_str="sg"
            ;;
        710)
            tmp_str="za"
            ;;
        784)
            tmp_str="ae"
            ;;
        792)
            tmp_str="tr"
            ;;
        841)
            tmp_str="us"
            ;;
        5000)
            tmp_str="au"
            ;;
        5001)
            tmp_str="ca"
            ;;
        *)
            tmp_str="none"
            ;;
    esac

    eval export -- "${1}=\$tmp_str"
}

detect_qtwifi(){
    devidx=1
    config_load wireless

    while :; do
    config_get type "wifi$devidx" type
    [ -n "$type" ] || break
    devidx=$(($devidx + 1))
    done

    cd /sys/class/net
    [ -d host0 ] || return

    found=0
    confdev="host0"
    config_foreach check_qtwifi_device wifi-device
    [ "$found" -gt 0 ] && continue

    cat <<EOF
config wifi-device  wifi$devidx
        option type    qtwifi
        option channel    auto
        option macaddr    $(cat /sys/class/net/${confdev}/address)
        option hwmode   11ac
        option config_interface $confdev
        option config_host_ip 1.1.1.1
        option config_device_ip 1.1.1.2
        # REMOVE THIS LINE TO ENABLE WIFI:
        option disabled 0

config wifi-iface
        option device    wifi$devidx
        option network    lan
        option confidx    0
        option mode    ap
        option ssid    QTN-5G
        option encryption none

EOF

}

wla_lan_restricted_access()
{
    lan_ipaddr=$1
    br_if=br0
    br_subif=br0:1
    qt_ip=1.1.1.1
    common_if=host0
    main_tag=100
    guest_tag=200
    main_if=host0.100
    guest_if=host0.200
    ETH_P_ARP=0x0806
    ETH_P_RARP=0x8035
    ETH_P_IP=0x0800
    ETH_P_IPv6=0x86dd
    IPPROTO_ICMP=1
    IPPROTO_UDP=17
    IPPROTO_ICMPv6=58
    DHCPS_DHCPC=67:68
    PORT_DNS=53
    DHCP6S_DHCP6C=546:547
    lan_ipv6addr=$(ifconfig br0 | grep Scope:Link | awk '{print $3}' | awk -F '/' '{print $1}')

    qt_call 0 vlan_config enable 0
    qt_call 0 vlan_config bind "$main_tag"
    qt_call 1 vlan_config bind "$guest_tag"

    vconfig add "$common_if" "$main_tag"
    vconfig add "$common_if" "$guest_tag"
    ifconfig "$main_if" up
    ifconfig "$guest_if" up
    ifconfig "$br_subif" down
    brctl delif "$br_if" "$common_if"
    ifconfig "$common_if" "$qt_ip" netmask 255.255.255.248 broadcast 1.1.1.7
    brctl addif "$br_if" "$main_if"
    brctl addif "$br_if" "$guest_if"

    ebtables -D FORWARD -i "$guest_if" -j DROP
    ebtables -D FORWARD -o "$guest_if" -j DROP
    ebtables -D INPUT -p IPv4 --ip-proto udp --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p IPv4 --ip-proto udp --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p IPv4 -i "$guest_if" --ip-dst "$lan_ipaddr" -j DROP
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" -i "$guest_if" --ip6-dst "$lan_ipv6addr" -j DROP
    ebtables -L | grep  "host" > /tmp/wifi_rules
    while read loop
    do
            ebtables -D INPUT $loop;
            ebtables -D FORWARD $loop;
    done < /tmp/wifi_rules
    rm  /tmp/wifi_rules

    ebtables -I INPUT -p "$ETH_P_IP" -i "$guest_if" --ip-dst "$lan_ipaddr" -j DROP
    ebtables -I INPUT -p "$ETH_P_IP" --ip-proto udp --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -I INPUT -p "$ETH_P_IP" --ip-proto udp --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -I INPUT -p "$ETH_P_IPv6" -i "$guest_if" --ip6-dst "$lan_ipv6addr" -j DROP
    ebtables -I INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -I INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
    ebtables -I INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -I FORWARD -o "$guest_if" -j DROP
    ebtables -I FORWARD -i "$guest_if" -j DROP
}

clear_wifi5g_ebtables()
{
    lan_ipaddr=$1
    br_if=br0
    br_subif=br0:1
    qt_ip=1.1.1.1
    common_if=host0
    main_tag=100
    guest_tag=200
    main_if=host0.100
    guest_if=host0.200
    ETH_P_ARP=0x0806
    ETH_P_RARP=0x8035
    ETH_P_IP=0x0800
    ETH_P_IPv6=0x86dd
    IPPROTO_ICMP=1
    IPPROTO_UDP=17
    IPPROTO_ICMPv6=58
    DHCPS_DHCPC=67:68
    PORT_DNS=53
    DHCP6S_DHCP6C=546:547
    lan_ipv6addr=$(ifconfig br0 | grep Scope:Link | awk '{print $3}' | awk -F '/' '{print $1}')

    ebtables -D FORWARD -i "$guest_if" -j DROP
    ebtables -D FORWARD -o "$guest_if" -j DROP
    ebtables -D INPUT -p IPv4 --ip-proto udp --ip-dport "$DHCPS_DHCPC" -j ACCEPT
    ebtables -D INPUT -p IPv4 --ip-proto udp --ip-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p IPv4 -i "$guest_if" --ip-dst "$lan_ipaddr" -j DROP
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
    ebtables -D INPUT -p "$ETH_P_IPv6" -i "$guest_if" --ip6-dst "$lan_ipv6addr" -j DROP
    ebtables -L | grep  "host" > /tmp/wifi_rules
    while read loop
    do
            ebtables -D INPUT $loop;
            ebtables -D FORWARD $loop;
    done < /tmp/wifi_rules
    rm  /tmp/wifi_rules

    brctl delif "$br_if" "$guest_if"
    brctl delif "$br_if" "$main_if"
    ifconfig "$common_if" 0.0.0.0
    brctl addif "$br_if" "$common_if"
    ifconfig "$br_subif" "$qt_ip"
    ifconfig "$guest_if" down
    ifconfig "$main_if" down
    vconfig rem "$guest_if"
    vconfig rem "$$main_if"

    qt_call 1 vlan_config unbind "$main_tag"
    qt_call 0 vlan_config unbind "$guest_tag"
    qt_call 0 vlan_config disable 0
}
