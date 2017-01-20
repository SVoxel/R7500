#
# Copyright (c) 2013 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

if [ "$ACTION" = "released" -a "$BUTTON" = "wps" ]; then
	if [ "$SEEN" -gt 3 ]; then
		echo "" > /dev/console
		echo "RESET TO FACTORY SETTING EVENT DETECTED" > /dev/console
		echo "PLEASE WAIT WHILE REBOOTING THE DEVICE..." > /dev/console
		mtd -r erase rootfs_data
	fi
fi
