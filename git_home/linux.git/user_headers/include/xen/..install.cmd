cmd_/home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen/.install := perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/include/xen /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen arm evtchn.h privcmd.h; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/include/xen /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen arm ; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/include/generated/xen /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen arm ; for F in ; do echo "\#include <asm-generic/$$F>" > /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen/$$F; done; touch /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/xen/.install
