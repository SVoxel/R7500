# Copyright (c) 2013 The Linux Foundation. All rights reserved.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

MODULE_NAME=R7500
HW_VERSION=R7500
FW_VERSION=V1.0.1.12SF
LG_VERSION=V1.0.0.252
HW_ID="29764841+0+128+256+3x3+4x4"
ifndef FW_REGION
	FW_REGION=""
endif

KERNEL_SIZE=2048k # kernel blocks * 128 = 16 * 128 = 2048
MAX_KERNEL_SIZE=2097152	# KERNEL_SIZE * 1024
MAX_IMAGE_SIZE=23461888	# (kernel blocks + rootfs blocks - reserved block for bad block) * 128 * 1024 = (16 + 192 - 29) * 128 * 1024 = 23461888

UBIFS_OPTS = -m 2048 -e 124KiB -c 4096 -U -F
UBINIZE_OPTS = -m 2048 -p 128KiB

E2SIZE=$(shell echo $$(($(CONFIG_TARGET_ROOTFS_PARTSIZE)*1024)))

define Image/Prepare
	echo "$(MODULE_NAME)" > $(TARGET_DIR)/module_name
	echo "$(HW_VERSION)" > $(TARGET_DIR)/hardware_version
	echo "$(FW_VERSION)" > $(TARGET_DIR)/firmware_version
	echo "$(FW_REGION)" > $(TARGET_DIR)/firmware_region
	echo "$(LG_VERSION)" > $(TARGET_DIR)/default_language_version
	echo $(HW_ID) > $(TARGET_DIR)/hw_id
	date > $(TARGET_DIR)/firmware_time
	echo "1" > $(TARGET_DIR)/etc/init.d/.hidden_new_features
	rm -f $(TARGET_DIR)/etc/modules.d/49-nf-conntrack-netlink
endef

define MkImageLzma
	mkimage -A arm -O linux -C lzma -T kernel -a 0x40908000 \
		-e 0x40908000 -n 'Linux-$(LINUX_VERSION)' \
		-d $(1) $(2)
endef

define Image/Build/R7500
	echo "Image/Build/R7500"
	dd if=$(LINUX_DIR)/arch/arm/boot/uImage of=$(KDIR)/vmlinux-$(2).uImage
	$(STAGING_DIR_HOST)/bin/R7500 \
		$(KDIR)/vmlinux-$(2).uImage \
		$(KDIR)/vmlinux-$(2).uImage.normal
	dd if=$(KDIR)/root.$(1) of=$(KDIR)/root.$(1).final bs=2k conv=sync
	$(call MkImageLzma,$(KDIR)/root.$(1).final,$(KDIR)/squashfs-$(2).uImage)
	$(STAGING_DIR_HOST)/bin/R7500 \
		$(KDIR)/squashfs-$(2).uImage \
		$(KDIR)/squashfs-$(2).uImage.tmp
	cat $(KDIR)/vmlinux-$(2).uImage.normal > $(KDIR)/vmlinux-$(2).uImage.tmp
	$(STAGING_DIR_HOST)/../../tools/checksize $(KDIR)/vmlinux-$(2).uImage.tmp $(shell expr $(MAX_KERNEL_SIZE) - 64)
	dd if=$(KDIR)/vmlinux-$(2).uImage.tmp bs=$(shell expr $(MAX_KERNEL_SIZE) - 64) conv=sync of=$(KDIR)/vmlinux-$(2).uImage.final
	dd if=$(KDIR)/squashfs-$(2).uImage.tmp bs=64 count=1 >> $(KDIR)/vmlinux-$(2).uImage.final
	( \
		dd if=$(KDIR)/vmlinux-$(2).uImage.final bs=$(KERNEL_SIZE) conv=sync; \
		dd if=$(KDIR)/root.$(1).final bs=64k; \
	) > $(BIN_DIR)/$(IMG_PREFIX)-sysupgrade.bin
	$(STAGING_DIR_HOST)/../../tools/checksize $(BIN_DIR)/$(IMG_PREFIX)-sysupgrade.bin $(MAX_IMAGE_SIZE)
	$(STAGING_DIR_HOST)/bin/mkdniimg \
		-B $(MODULE_NAME) -v $(FW_VERSION) -r $(FW_REGION) -H $(HW_ID) \
		-i $(BIN_DIR)/$(IMG_PREFIX)-sysupgrade.bin \
		-o $(BIN_DIR)/$(MODULE_NAME)-$(FW_VERSION)$(FW_REGION).img
endef

define Image/BuildKernel
	$(CP) $(LINUX_DIR)/vmlinux $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.elf
	$(CP) $(LINUX_DIR)/arch/arm/boot/Image $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.bin
	dd if=$(LINUX_DIR)/arch/arm/boot/uImage of=$(BIN_DIR)/$(IMG_PREFIX)-$(KERNEL)-uImage bs=2k conv=sync
endef

define Image/Build
	$(call Image/Build/$(1),$(1))
	mkdir -p $(BIN_DIR)
	dd if=$(KDIR)/root.$(1) of=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-root.img bs=2k conv=sync
	$(call Image/Build/R7500,$(1),r7500)
endef

define Sysupgrade
	cat $(BIN_DIR)/$(IMG_PREFIX)-$(KERNEL)-uImage \
	    $(BIN_DIR)/$(IMG_PREFIX)-$(1)-root.img > \
		$(BIN_DIR)/$(IMG_PREFIX)-sysupgrade.bin
endef

$(eval $(call BuildImage))
