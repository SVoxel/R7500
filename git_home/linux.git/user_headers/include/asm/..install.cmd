cmd_/home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm/.install := perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/arch/arm/include/asm /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm arm a.out.h byteorder.h fcntl.h hwcap.h ioctls.h ipcbuf.h mman.h msgbuf.h param.h posix_types.h ptrace.h sembuf.h setup.h shmbuf.h sigcontext.h signal.h socket.h sockios.h stat.h statfs.h swab.h termbits.h termios.h types.h unistd.h; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/arch/arm/include/asm /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm arm ; perl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/sourcecode/scripts/headers_install.pl /home/torby.tong/R8000/r7500-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/arch/arm/include/generated/asm /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm arm ; for F in auxvec.h bitsperlong.h errno.h ioctl.h poll.h resource.h siginfo.h; do echo "\#include <asm-generic/$$F>" > /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm/$$F; done; touch /home/torby.tong/R8000/r7500-v46-buildroot.git/git_home/auto-gpl.git/tmp/linux/linux-3.4.0/user_headers/include/asm/.install
