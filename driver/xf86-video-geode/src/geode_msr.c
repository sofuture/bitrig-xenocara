#define _LARGEFILE64_SOURCE

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef __OpenBSD__
#include <machine/amdmsr.h>
#endif

#include <errno.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>

#include "os.h"
#include "geode.h"

#ifdef __OpenBSD__
#define _PATH_MSRDEV	"/dev/amdmsr"
//#define X_PRIVSEP
extern int priv_open_device(const char *);
#else
#define _PATH_MSRDEV	"/dev/cpu/0/msr"
#endif

static int
_msr_open(void)
{
    static int msrfd = 0;

    if (msrfd == 0) {
#ifdef X_PRIVSEP
	msrfd = priv_open_device(_PATH_MSRDEV);
#else
	msrfd = open(_PATH_MSRDEV, O_RDWR);
#endif
	if (msrfd == -1)
	    FatalError("Unable to open %s: %s\n", _PATH_MSRDEV,
		strerror(errno));
    }

    return msrfd;
}

int
GeodeReadMSR(unsigned long addr, unsigned long *lo, unsigned long *hi)
{
#ifdef __OpenBSD__
    struct amdmsr_req req;
#endif
    int fd = _msr_open();

#ifdef __OpenBSD__
    req.addr = addr;

    if (ioctl(fd, RDMSR, &req) == -1)
	FatalError("Unable to read MSR at address %0lx06x: %s\n", addr,
	    strerror(errno));

    *hi = req.val >> 32;
    *lo = req.val & 0xffffffff;
#else
    unsigned int data[2];
    int fd = _msr_open();
    int ret;

    if (fd == -1)
	return -1;

    ret = lseek(fd, (off_t) addr, SEEK_SET);

    if (ret == -1)
	return -1;

    ret = read(fd, (void *)data, sizeof(data));

    if (ret != 8)
	return -1;

    *hi = data[1];
    *lo = data[0];
#endif
    return 0;
}

int
GeodeWriteMSR(unsigned long addr, unsigned long lo, unsigned long hi)
{
#ifdef __OpenBSD__
    struct amdmsr_req req;
#endif
    int fd = _msr_open();

#ifdef __OpenBSD__
    req.addr = addr;
    req.val = (u_int64_t)hi << 32 | (u_int64_t)lo;

    if (ioctl(fd, WRMSR, &req) == -1)
	FatalError("Unable to write MSR at address 0x%06lx: %s\n", addr,
	    strerror(errno));
#else
    unsigned int data[2];
    int fd = _msr_open();

    if (fd == -1)
	return -1;

    if (lseek(fd, (off_t) addr, SEEK_SET) == -1)
	return -1;

    data[0] = lo;
    data[1] = hi;

    if (write(fd, (void *)data, 8) != 8)
	return -1;
#endif
    return 0;
}
