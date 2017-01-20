#!/bin/sh

on_led_qcawifi () {
    local radio_num=$1
    local security_type=$2
    local radio_type=$3
    local led_option=$4
    local g_ap_setup_locked=0
    local a_ap_setup_locked=0

    WIFI_LED_ON='ledcontrol -n wifi -c green -s on'
    WIFI_LED_OFF='ledcontrol -n wifi -c green -s off'
    WPS_LED_ON='ledcontrol -n wps -c green -s on'
    WPS_LED_OFF='rm -f /var/run/wps_led.pid; wpsled wps_lock_down stop; /sbin/ledcontrol -n wps -c green -s off'
    WPS_AP_LOCK_ON='wpsled wps_lock_down on &'
    TRAFFIC_11G_ON='iwpriv wifi0 gpio_output 1 0'
    TRAFFIC_11G_OFF='iwpriv wifi0 gpio_output 1 1'
    BLINK_11G_ON='echo 1 > /proc/sys/qca_dni/blink_2g_led'
    BLINK_11G_OFF='echo 0 > /proc/sys/qca_dni/blink_2g_led'

    [ "$radio_num" -gt "0" ] && eval ${WIFI_LED_ON} || eval ${WIFI_LED_OFF}

    [ "$radio_num" -gt "0" -a "$security_type" -gt "1" ] && eval ${WPS_LED_ON} || eval ${WPS_LED_OFF}
    if [ "$radio_type" = "dual" ]; then
        config_get vifs wifi0 vifs
        g_vif=`echo $vifs | cut -d" " -f1`
        config_get vifs wifi1 vifs
        a_vif=`echo $vifs | cut -d" " -f1`
        config_get g_ap_setup_locked "$g_vif" ap_setup_locked 0
        config_get a_ap_setup_locked "$a_vif" ap_setup_locked 0
    elif [ "$radio_type" = "11g" ]; then
        config_get vifs wifi0 vifs
        g_vif=`echo $vifs | cut -d" " -f1`
        config_get g_ap_setup_locked "$g_vif" ap_setup_locked 0
    elif [ "$radio_type" = "11a" ]; then
        config_get vifs wifi1 vifs
        a_vif=`echo $vifs | cut -d" " -f1`
        config_get a_ap_setup_locked "$a_vif" ap_setup_locked 0
    fi
    [ "$g_ap_setup_locked" = "1" -o "$a_ap_setup_locked" = "1" ] && eval ${WPS_AP_LOCK_ON}

    if [ "$radio_type" = "dual" -o "$radio_type" = "11g" ]; then
        if [ "$led_option" = "0" ]; then
            eval ${BLINK_11G_ON}
        elif [ "$led_option" = "1" ]; then
            eval ${BLINK_11G_OFF}
        elif [ "$led_option" = "2" ]; then
            eval ${BLINK_11G_OFF}
            eval ${TRAFFIC_11G_OFF}
        fi
    fi
    
}

off_led_qcawifi () {
    local radio_num=$1
    local security_type=$2
    local radio_type=$3
    local led_option=$4
    local g_ap_setup_locked=0
    local a_ap_setup_locked=0

    WIFI_LED_ON='ledcontrol -n wifi -c green -s on'
    WIFI_LED_OFF='ledcontrol -n wifi -c green -s off'
    WPS_LED_ON='ledcontrol -n wps -c green -s on'
    WPS_LED_OFF='rm -f /var/run/wps_led.pid; wpsled wps_lock_down stop; /sbin/ledcontrol -n wps -c green -s off'
    WPS_AP_LOCK_ON='wpsled wps_lock_down on &'
    TRAFFIC_11G_ON='iwpriv wifi0 gpio_output 1 0'
    TRAFFIC_11G_OFF='iwpriv wifi0 gpio_output 1 1'
    BLINK_11G_ON='echo 1 > /proc/sys/qca_dni/blink_2g_led'
    BLINK_11G_OFF='echo 0 > /proc/sys/qca_dni/blink_2g_led'

    eval ${WIFI_LED_OFF}
    [ "$radio_num" -gt "0" ] && eval ${WIFI_LED_ON}

    eval ${WPS_LED_OFF}
    [ "$radio_num" -gt "0" -a "$security_type" -gt "1" ] && eval ${WPS_LED_ON}
    if [ "$radio_type" = "dual" ]; then
        config_get vifs wifi0 vifs
        g_vif=`echo $vifs | cut -d" " -f1`
        config_get vifs wifi1 vifs
        a_vif=`echo $vifs | cut -d" " -f1`
        config_get g_ap_setup_locked "$g_vif" ap_setup_locked 0
        config_get a_ap_setup_locked "$a_vif" ap_setup_locked 0
    elif [ "$radio_type" = "11g" ]; then
        config_get vifs wifi0 vifs
        g_vif=`echo $vifs | cut -d" " -f1`
        config_get g_ap_setup_locked "$g_vif" ap_setup_locked 0
    elif [ "$radio_type" = "11a" ]; then
        config_get vifs wifi1 vifs
        a_vif=`echo $vifs | cut -d" " -f1`
        config_get a_ap_setup_locked "$a_vif" ap_setup_locked 0
    fi
    [ "$g_ap_setup_locked" = "1" -o "$a_ap_setup_locked" = "1" ] && eval ${WPS_AP_LOCK_ON}

    if [ "$radio_type" = "dual" -o "$radio_type" = "11g" ]; then
        if [ "$led_option" = "0" ]; then
            eval ${BLINK_11G_ON}
        elif [ "$led_option" = "1" ]; then
            eval ${BLINK_11G_OFF}
        elif [ "$led_option" = "2" ]; then
            eval ${BLINK_11G_OFF}
            eval ${TRAFFIC_11G_OFF}
        fi
    fi
}

force_off_led_qcawifi () {
    WIFI_LED_OFF='ledcontrol -n wifi -c green -s off'
    WPS_LED_OFF='rm -f /var/run/wps_led.pid; wpsled wps_lock_down stop; /sbin/ledcontrol -n wps -c green -s off'

    eval ${WIFI_LED_OFF}

    eval ${WPS_LED_OFF}
}