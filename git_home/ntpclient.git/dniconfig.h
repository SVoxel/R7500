/* dniconfig.h for dni's macro definition.
 * If you have your own dniconfig.h in project for ntpclient,
 * you can replace with yours.
 */
#define NET_IFNAME              "eth0"  /* The router's wan side interface name. */
#define CDC_IFNAME              "eth2"  /* The router's CDC modem side interface name. */
#define PPP_IFNAME              "ppp0"
#define MODEM			"/tmp/modem"

#define NETGEAR_DAYLIGHT_SAVING_TIME

#define FIX_BUG_28601
#define WLAN_COMMON_SUUPPORT 1 //fix bug 34896
