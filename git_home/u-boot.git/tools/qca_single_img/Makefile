#
# Fetch single image tool from $(git_home)/qsdk-chipcode.git
#

-include $(TOPDIR)/config.mk


# Default value for required variables
GIT_HOME ?= $(HOME)/git-home
CONFIG_QCA_SINGLE_IMG_GIT ?= $(GIT_HOME)/qsdk-chipcode.git
CONFIG_QCA_SINGLE_IMG_TREEISH ?= 4b360057b82b41aefc2f8afb928aa2c227147c0b


prepared = $(addprefix qsdk-chipcode/,.prepared_$(CONFIG_QCA_SINGLE_IMG_TREEISH))

all: prepare pack

prepare: $(obj)$(prepared)
	@echo -e "\n###### QCA single image tool is prepared [$(notdir $<)]. ######"
	@echo -e "\n###### Now copy u-boot.mbn to $(obj)./ or $(obj)qsdk-chipcode/common/build/ipq/ and then issue \"make pack\" ######\n"

pack: $(obj)$(prepared)
	-cp $(obj)u-boot.mbn $(obj)qsdk-chipcode/common/build/ipq/
	cp $(obj)pack.py $(obj)qsdk-chipcode/apss_proc/out/
	cd $(obj)qsdk-chipcode/common/build && python update_common_info.py
	cp $(obj)qsdk-chipcode/common/build/bin/nand-single.img $(obj)u-boot-single.img
	@echo -e "\n###### Finishing packing single image!! ######"
	@echo -e "\n###### Single image is $(obj)u-boot-single.img ######\n"

$(obj)$(prepared):
	@echo -e "\n###### Preparing QCA single image tool ######"
	@echo -e "\n###### Fetching QCA single image tool"
	rm -rf $(obj)qsdk-chipcode/
	git archive --format=tar --prefix=qsdk-chipcode/ --remote=$(CONFIG_QCA_SINGLE_IMG_GIT)/ $(CONFIG_QCA_SINGLE_IMG_TREEISH) | tar -C $(obj)./ -xf -

	@echo -e "\n###### Replacing files with local ones"
	if [ -d "files" ]; then \
		cp -a files/./ $(obj)qsdk-chipcode/; \
	fi

	@echo -e "\n###### Applying local patches"
	if [ -d "patches" ]; then \
		for patch in $$(ls patches/*.patch); do \
			patch -p1 --directory=$(obj)qsdk-chipcode/ < "$$patch"; \
		done \
	fi
	@> $@

clean:
	rm -rf qsdk-chipcode u-boot.mbn u-boot-single.img

.PHONY: all prepare pack clean
