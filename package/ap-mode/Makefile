# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile 6501 2007-03-04 04:41:46Z pavlov $

include $(TOPDIR)/rules.mk

PKG_NAME:=ap-mode
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
PKG_GIT_DIR:=ap-mode-script.git
PKG_GIT_TREEISH=ap-mode-script-dni7

include $(INCLUDE_DIR)/package.mk

define Package/ap-mode
  SECTION:=utils
  CATEGORY:=Base system
  TITLE:=Shell script used to enable/disable AP Mode
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/ap-mode-script.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/ap-mode/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ap-mode $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/restart_ap_udhcpc $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/share/zcip/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/zcip.script $(1)/usr/share/zcip/
endef

$(eval $(call BuildPackage,ap-mode))
