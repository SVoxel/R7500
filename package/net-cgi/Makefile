# 
# Copyright (C) 2009 OpenWrt.org
#

include $(TOPDIR)/rules.mk

PKG_NAME:=net-cgi
PKG_RELEASE:=1
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

WEB_PATH:=www

include $(INCLUDE_DIR)/package.mk
include ./files/net-cgi.config

PKG_GIT_TREEISH:=net-cgi-dni191

define Package/net-cgi
  SECTION:=utils
  CATEGORY:=Base system
  DEFAULT:=y
  TITLE:=The mini_httpd CGI interface
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/net-cgi.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/net-cgi/install
	install -d -m0755 $(1)/usr/sbin
	install -m0755 $(PKG_BUILD_DIR)/net-cgi $(1)/usr/sbin
	install -m0755 $(PKG_BUILD_DIR)/inetd $(1)/usr/sbin/
	install -m0755 $(PKG_BUILD_DIR)/files/wget_netgear $(1)/usr/sbin/
ifeq ($(HAVE_USB_STORAGE), 1)
	install -m0755 $(PKG_BUILD_DIR)/usb_cfg $(1)/usr/sbin/
	install -m0755 $(PKG_BUILD_DIR)/files/select_partition $(1)/usr/sbin/
endif
ifeq ($(HAVE_PLC), 1)
	install -m0755 $(PKG_BUILD_DIR)/detplc $(1)/usr/sbin/
endif
	install -d -m0755 $(1)/$(WEB_PATH)
	install -d -m0755 $(1)/$(WEB_PATH)/image
	install -d -m0755 $(1)/$(WEB_PATH)/style
	install -d -m0755 $(1)/$(WEB_PATH)/help
	install -d -m0755 $(1)/$(WEB_PATH)/cgi-bin
	ln -sf /tmp/openvpn/windows.zip $(1)/$(WEB_PATH)/windows.zip
	ln -sf /tmp/openvpn/nonwindows.zip $(1)/$(WEB_PATH)/nonwindows.zip
	install -d -m0755 $(1)/$(WEB_PATH)/language
	install -m0644 $(PKG_BUILD_DIR)/$(WEB_PATH)/*.* $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/image/* $(1)/$(WEB_PATH)/image
	install -m0644 $(PKG_BUILD_DIR)/$(WEB_PATH)/style/*.* $(1)/$(WEB_PATH)/style
	install -m0644 $(PKG_BUILD_DIR)/$(WEB_PATH)/help/*.* $(1)/$(WEB_PATH)/help
	install -m0755 $(PKG_BUILD_DIR)/$(WEB_PATH)/cgi-bin/*.* $(1)/$(WEB_PATH)/cgi-bin
	install -m0755 $(PKG_BUILD_DIR)/$(WEB_PATH)/language/*.* $(1)/$(WEB_PATH)/language

ifeq ($(CD_LESS),1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/browser_hijack/* $(1)/$(WEB_PATH)
ifeq ($(CD_LESS_DOWNLOAD),1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/BRS_hdd_download.htm $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/BRS_hdd_download_href.htm $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/BRS_hdd_success.htm $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/BRS_netgear_success.html $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/hdd_is_Mac.js $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/image/centria_cd_less.gif $(1)/$(WEB_PATH)/image
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/centria_cd_less/image/hdd_icons.jpg $(1)/$(WEB_PATH)/image
endif

endif
ifeq ($(HAVE_USB_STORAGE), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/usb_storage/* $(1)/$(WEB_PATH)
ifeq ($(HDD_MULTI_DEVICE), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/user_hdd_storage/USB_disk_rm_part.htm $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/user_hdd_storage/USB_device_refresh.htm $(1)/$(WEB_PATH)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/user_hdd_storage/USB_disk_rm.htm $(1)/$(WEB_PATH)
endif
endif
ifeq ($(READYSHARE_REMOTE), 1)
	install -d -m0755 $(1)/$(WEB_PATH)/pluging
	install -m0755 $(PKG_BUILD_DIR)/$(WEB_PATH)/readyshare_remote/RemoteShare.htm $(1)/$(WEB_PATH)/pluging
	install -m0755 $(PKG_BUILD_DIR)/$(WEB_PATH)/readyshare_remote/*.* $(1)/$(WEB_PATH)/cgi-bin/
	install -m0755 $(PKG_BUILD_DIR)/remote_smb_conf $(1)/usr/sbin/
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/readyshare_remote/help/* $(1)/$(WEB_PATH)/help

endif
ifeq ($(READYSHARE_PRINT), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/readyshare_print/* $(1)/$(WEB_PATH)/
endif
ifeq ($(HAVE_VAULT), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/readyshare_vault/* $(1)/$(WEB_PATH)/
endif
ifeq ($(HAVE_L2TP), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/l2tp/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_BIGPOND), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/bigpond/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_GUEST_NETWORK), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/guest/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_QOS), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/qos/* $(1)/$(WEB_PATH)
endif
ifeq ($(STREAM_BOOST), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/streamboost/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_WDS), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/wds/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_IPv6), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/ipv6/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_VPN), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/vpn/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_TRAFFIC_METER), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/traffic/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_BLOCK_SITES), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/block_sites/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_ACCESS_CONTROL), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/access_control/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_TR069), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/tr069/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_PLC), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/plc/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_AP_MODE), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/apmode/* $(1)/$(WEB_PATH)
endif
ifeq ($(HAVE_JP), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/multipppoe/* $(1)/$(WEB_PATH)
endif
ifeq ($(NETGEAR_DDNS), 1)
	$(CP) $(PKG_BUILD_DIR)/$(WEB_PATH)/netgear_ddns/* $(1)/$(WEB_PATH)
endif
endef

$(eval $(call BuildPackage,net-cgi))
