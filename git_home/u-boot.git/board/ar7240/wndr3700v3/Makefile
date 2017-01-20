include $(TOPDIR)/config.mk

LIB	= lib$(BOARD).o

OBJS	= $(BOARD).o flash.o ../common/ar7240_pci.o

OBJS	+= ../common/ar7240_flash.o

OBJS	+= ../common/athrs17_phy.o

SOBJS	= ../common/lowlevel_init_934x.o
SOBJS	+= ../common/lowlevel_init_934x-1.1.o

$(LIB):	.depend $(OBJS) $(SOBJS)
	$(call cmd_link_o_target, $(OBJS) $(SOBJS))

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude .depend

#########################################################################
