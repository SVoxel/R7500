#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qt-wireless
PKG_VERSION:=36.6.0.30
PKG_RELEASE:=1

PKG_GIT_DIR=quantenna-sdk.git
QT_WIRELESS_GIT_TREEISH=80e0516c10dacc9e26a6ef64f7eccc76a6c0702b

include $(INCLUDE_DIR)/package.mk

define Package/quantenna-wl/Default
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Quantenna WiFi driver
endef

define KernelPackage/qt-drivers/Default
  $(call Package/quantenna-wl/Default)
  SECTION:=kernel
  TITLE:=Kernel driver for Quantenna chipsets
  FILES:=$(PKG_BUILD_DIR)/drivers/pcie2/host/arm/qdpc-host.ko
  AUTOLOAD:=$(call AutoLoad,40, qdpc-host)
endef

define KernelPackage/qt-drivers/Default/description
 This package contains the proprietary wireless driver for the Quantenna 
 chipset.
endef

define KernelPackage/qt-drivers
$(call KernelPackage/qt-drivers/Default,)
  TITLE+= (normal version)
endef

define KernelPackage/qt-drivers/description
$(call KernelPackage/qt-drivers/Default/description)
endef

define Package/qcsapi
$(call Package/quantenna-wl/Default)
  TITLE:=quantenna driver setup utility, qcsapi
endef

define Package/qcsapi/description
 This package contains an utility for initializing the proprietary Quantenna
 wireless driver.
endef

MAKE_KMOD := $(MAKE) V=99 -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		PATH="$(TARGET_PATH)" \
		DNI_KMOD_CFLAGS="-DDNI_5G_LED -DDNI_EXTRA_FUNCTIONS -DSKIP_PCI_DMA_MASK -DQCA_NSS_PLATFORM -I$(STAGING_DIR)/usr/include/qca-nss-drv -DDISABLE_PCIE_UPDATA_HW_BAR -DRX_IP_HDR_REALIGN"

define Build/Prepare
	$(CP) $(GIT_HOME)/quantenna-sdk.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

#define Build/InstallDev
define KernelPackage/qt-drivers/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(CP) ./binary/topaz-linux.lzma.img $(1)/lib/firmware/topaz-linux.lzma.img
	$(CP) ./binary/u-boot.bin $(1)/lib/firmware
endef

define Package/qcsapi/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqcsapi_client_src/c_rpc_qcsapi_sample $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqcsapi_client_src/qcsapi_pcie $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqcsapi_client_src/qcsapi_pcie_static $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqcsapi_client_src/qcsapi_sockrpc $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libqcsapi_client_src/qcsapi_sockrpc_static $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/host/qevt-client/qevt_client $(1)/usr/sbin/
	$(CP) $(PKG_BUILD_DIR)/libqcsapi_client_src/libqcsapi_client.so* $(1)/lib/
	$(CP) ./files/* $(1)/
	$(INSTALL_DIR) $(1)/etc
	ln -sf /tmp/etc/qcsapi_target_ip.conf $(1)/etc/qcsapi_target_ip.conf
endef

$(eval $(call KernelPackage,qt-drivers))
$(eval $(call BuildPackage,qcsapi))
