#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=qca-hostap

# This value is custom to this package
# The first version will be used as a default and the other selectable
# in the menuconfig
PKG_VERSION:=10.2.2.39.1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2

include $(INCLUDE_DIR)/package-version-override.mk
include $(INCLUDE_DIR)/package.mk

#PKG_BRANCH:=$(basename $(PKG_VERSION))
#PKG_SOURCE_URL:=http://tritium.qca.qualcomm.com/software/$(PKG_BRANCH)/AP$(PKG_VERSION:999.999.0%=QCAMain%)/src/components
PKG_BUILD_DEPENDS:=qca-wifi

LOCAL_TYPE=$(strip \
		$(if $(findstring qca-wpa-supplicant,$(BUILD_VARIANT)),supplicant, \
		hostapd \
	)))

define Package/qca-hostap/Default
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
endef

define Package/qca-hostap
  $(call Package/qca-hostap/Default)
  TITLE:=Hostap authenticator
  DEPENDS:=kmod-qca-wifi @(!(PACKAGE_hostapd)) +qsdk-wifi-script
endef

define Package/qca-hostap/description
 This package contains the IEEE 802.1x/WPA/EAP/RADIUS Authenticator.
endef

define Package/qca-hostapd-cli
  $(call Package/qca-hostap/Default)
  DEPENDS:=qca-hostap
  TITLE:=Hostap authenticator command line interface
endef

define Package/qca-hostapd-cli/description
 This package contains a command line utility to control the
 IEEE 802.1x/WPA/EAP/RADIUS Authenticator.
endef

define Package/qca-wpa-supplicant
  $(call Package/qca-hostap/Default)
  TITLE:=WPA Supplicant
  DEPENDS:=kmod-qca-wifi @(!(PACKAGE_wpa-supplicant)) +qsdk-wifi-script
endef

define Package/qca-wpa-supplicant/Description
  WPA Supplicant
endef

define Package/qca-wpa-cli
  $(call Package/qca-hostap/Default)
  DEPENDS:=qca-wpa-supplicant
  TITLE:=WPA Supplicant command line interface
endef

define Package/qca-wpa-cli/Description
  WPA Supplicant control utility
endef


ifneq ($(wildcard $(PKG_BUILD_DIR)/.config_*),$(subst .configured_,.config_,$(STAMP_CONFIGURED)))
  define Build/Configure/rebuild
  endef
endif

define ConfigOptAdd
	config QCA_$(3)_$(1)
			bool "$(patsubst $(2)-%,%,$(1))"
endef

define ConfigAthrHostap
	choice
		prompt "$(1) configuration"
		depends PACKAGE_qca-$(1)
	$(foreach config,$(notdir $(basename $(wildcard files/$(1)-*.config))),
		$(call ConfigOptAdd,$(config),$(1),$(2)))
	endchoice

	config QCA_$(2)_CONFIG
		string
	$(foreach config,$(notdir $(basename $(wildcard files/$(1)-*.config))),
		default "$(config)" if QCA_$(2)_$(config))

endef

define ConfigDNIHostap
	config QCA_$(2)_USE_DNI_GIT
		bool "Fetch source from DNI git repository for package $(1)"
		default n
endef

ifneq ($(CONFIG_QCA_ATHR_HOSTAP_USE_DNI_GIT),)
PKG_GIT_DIR=hostap.git
QCA_HOSTAP_GIT_TREEISH=hostap-10.2.2.39.1-dni1

HOSTAPD_CFLAGS="-DDNI_WPS_LOCK_DOWN -DNETGEAR_RANDOM_SSID -DNETGEAR_ROUTER_PIN_HACK -DNETGEAR_ANYSSID -DNETGEAR_SINGLE_RADIO_SSID_PREFIX -DNETGEAR_WPS_UPNP_SETTING -DNTG_DUT_NOT_SUPPORT_WEP_OPEN_AUTH -DNTGR_WPS_SET_MIX_MODE_IF_UNCFG -DNO_OPTIONAL_OPTION_IN_WPS_IE -DNO_TRIM_TRAILING_WHITESPACE -DWPS_CONF_RELOAD_TIME -DWPS_DISABLE_IE -DDNI_HOTPLUG_CMD -DDNI_WPS_CANCEL -DWPS_GENERATE_RANDOM_PASSPHRASE -DWPS_GENERATE_UUID -DDNI_DETECT_DUALBAND_MODE"

define Build/Prepare
	$(CP) $(GIT_HOME)/hostap.git/* $(PKG_BUILD_DIR)/
endef
endif


define Package/qca-hostap/config
$(call Package/$(PKG_NAME)/override_source_path,$(PKG_NAME))
$(call Package/$(PKG_NAME)/override_version,$(PKG_NAME),$(PKG_SUPPORTED_VERSION))
$(call ConfigAthrHostap,hostap,ATHR_HOSTAP_HOSTAPD)
$(call ConfigDNIHostap,hostap,ATHR_HOSTAP)
endef

define Package/qca-wpa-supplicant/config
$(call ConfigAthrHostap,wpa-supplicant,ATHR_HOSTAP_SUPPLICANT)
endef

HOSTAPD_CONFIG_FILE:= \
	./files/$(strip $(CONFIG_QCA_ATHR_HOSTAP_HOSTAPD_CONFIG)).config
WPA_CONFIG_FILE= \
	./files/$(strip $(CONFIG_QCA_ATHR_HOSTAP_SUPPLICANT_CONFIG)).config

define Build/Configure
endef

TARGET_CPPFLAGS += \
	-I$(STAGING_DIR)/usr/include

define Build/RunMake
	CFLAGS="$(TARGET_CPPFLAGS) $(TARGET_CFLAGS)" \
	$(MAKE) -C $(PKG_BUILD_DIR)/$(1) \
	$(TARGET_CONFIGURE_OPTS) \
	EXTRA_CFLAGS=$(HOSTAPD_CFLAGS) \
	LIBS="$(TARGET_LDFLAGS)" \
	$(2)
endef

define Build/Compile/qca-hostap
endef

define Build/Compile/qca-wpa-supplicant
endef

define Build/InstallDev
endef


define Build/Compile
	$(Build/Compile/qca-wpa-supplicant)
	$(Build/Compile/qca-hostap)
endef

define Install/qca-hostap
	$(INSTALL_DIR) $(1)/lib/wifi
	$(INSTALL_DIR) $(1)/usr/sbin
endef

define Install/qca-wpa-supplicant
	$(INSTALL_DIR) $(1)/lib/wifi
	$(INSTALL_DIR) $(1)/usr/sbin
endef

define Package/qca-hostap/install
	$(call Install/qca-hostap,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd $(1)/usr/sbin/
endef

ifneq ($(LOCAL_TYPE),supplicant)
  define Package/qca-hostapd-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/hotplug.d/button
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hostapd/hostapd_cli $(1)/usr/sbin/
	$(INSTALL_DATA) ./files/wps-hotplug.sh $(1)/etc/hotplug.d/button/50-wps
	$(INSTALL_DATA) ./files/wps-reset-hotplug.sh $(1)/etc/hotplug.d/button/51-wps-reset

	$(INSTALL_DIR) $(1)/lib/wifi
	#$(INSTALL_BIN) ./files/wps-hostapd-update-uci.sh \
			$(1)/lib/wifi/wps-hostapd-update-uci
  endef
endif

define Package/qca-wpa-supplicant/install
	$(call Install/qca-wpa-supplicant,$(1))
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wpa-supplicant/wpa_supplicant $(1)/usr/sbin/
endef

ifneq ($(LOCAL_TYPE),hostapd)
  define Package/qca-wpa-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/wpa-supplicant/wpa_cli $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/hotplug.d/button
	$(INSTALL_DATA) ./files/wps-supplicant-hotplug.sh \
			$(1)/etc/hotplug.d/button/52-wps-supplicant

	$(INSTALL_DIR) $(1)/lib/wifi
	$(INSTALL_BIN) ./files/wps-supplicant-update-uci.sh \
			$(1)/lib/wifi/wps-supplicant-update-uci
  endef
endif

$(eval $(call BuildPackage,qca-hostap))
$(eval $(call BuildPackage,qca-wpa-supplicant))
$(eval $(call BuildPackage,qca-hostapd-cli))
$(eval $(call BuildPackage,qca-wpa-cli))
