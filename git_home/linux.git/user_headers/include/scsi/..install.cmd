cmd_/home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi/.install := perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/include/scsi /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi arm scsi_bsg_fc.h scsi_netlink.h scsi_netlink_fc.h; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/include/scsi /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi arm ; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/include/generated/scsi /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi arm ; for F in ; do echo "\#include <asm-generic/$$F>" > /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi/$$F; done; touch /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/scsi/.install