/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include <X11/X.h>
#include "xf86.h"
#include "xf86Priv.h"

#include "xf86_OSlib.h"
#include "xf86OSpriv.h"

#include "xf86Bus.h"

#include <dev/wscons/wsconsio.h>

sbusDevice sbusInfo;

void
xf86SbusProbe(void)
{
	int fd = xf86Info.screenFd;
	u_int gtype;

	if (ioctl(fd, WSDISPLAYIO_GTYPE, &gtype) == -1)
		return;

	switch (gtype) {
	case WSDISPLAY_TYPE_MGX:
		sbusInfo.devId = SBUS_DEVICE_MGX;
		break;
	case WSDISPLAY_TYPE_SUN24:
		sbusInfo.devId = SBUS_DEVICE_LEO;
		break;
	case WSDISPLAY_TYPE_SUNBW:
		sbusInfo.devId = SBUS_DEVICE_BW2;
		break;
	case WSDISPLAY_TYPE_SUNCG2:
		sbusInfo.devId = SBUS_DEVICE_CG2;
		break;
	case WSDISPLAY_TYPE_SUNCG3:
		sbusInfo.devId = SBUS_DEVICE_CG3;
		break;
	case WSDISPLAY_TYPE_SUNCG4:
		sbusInfo.devId = SBUS_DEVICE_CG4;
		break;
	case WSDISPLAY_TYPE_SUNCG6:
		sbusInfo.devId = SBUS_DEVICE_CG6;
		break;
	case WSDISPLAY_TYPE_SUNCG8:
		sbusInfo.devId = SBUS_DEVICE_CG8;
		break;
	case WSDISPLAY_TYPE_SUNCG12:
		sbusInfo.devId = SBUS_DEVICE_CG12;
		break;
	case WSDISPLAY_TYPE_SUNCG14:
		sbusInfo.devId = SBUS_DEVICE_CG14;
		break;
	case WSDISPLAY_TYPE_SUNFFB:
		sbusInfo.devId = SBUS_DEVICE_FFB;
		break;
	case WSDISPLAY_TYPE_SUNTCX:
		sbusInfo.devId = SBUS_DEVICE_TCX;
		break;
	}

	sbusInfo.fd = -1;
}

_X_EXPORT int
xf86MatchSbusInstances(const char *driverName, int sbusDevId,
		       GDevPtr *devList, int numDevs, DriverPtr drvp,
		       int **foundEntities)
{
	int *retEntities = NULL;
	EntityPtr p = NULL;
	int num;

	*foundEntities = NULL;
	if (sbusDevId == sbusInfo.devId) {
		sbusInfo.device = devList[0]->identifier;
		num = xf86AllocateEntity();
		p = xf86Entities[num];
		p->driver = drvp;
		p->chipset = -1;
		p->busType = BUS_SBUS;
		xf86AddDevToEntity(num, devList[0]);
		p->active = TRUE;
		p->inUse = FALSE;
		/* Here we initialize the access structure */
		p->access = xnfcalloc(1,sizeof(EntityAccessRec));
		p->access->fallback = &AccessNULL;
		p->access->pAccess = &AccessNULL;
		retEntities = xalloc(sizeof(int));
		retEntities[0] = num;
		*foundEntities = retEntities;
		return 1;
	}

	return 0;
}

_X_EXPORT sbusDevicePtr
xf86GetSbusInfoForEntity(int entityIndex)
{
	return &sbusInfo;
}

_X_EXPORT void
xf86SbusUseBuiltinMode(ScrnInfoPtr pScrn, sbusDevicePtr psdp)
{
	int fd = xf86Info.screenFd;
	struct wsdisplay_fbinfo wdf;
	DisplayModePtr mode;

	if (ioctl(fd, WSDISPLAYIO_GINFO, &wdf) == -1)
		FatalError("%s: could not get mode (%s)",
			   "xf86SbusUseBuiltinMode", strerror(errno));

	mode = xnfcalloc(sizeof(DisplayModeRec), 1);
	mode->name = "current";
	mode->next = mode;
	mode->prev = mode;
	mode->type = M_T_BUILTIN;
	mode->HDisplay = wdf.width;
	mode->VDisplay = wdf.height;
	pScrn->modes = mode;
	pScrn->virtualX = wdf.width;
	pScrn->virtualY = wdf.height;
}

_X_EXPORT pointer
xf86MapSbusMem(sbusDevicePtr psdp, unsigned long Base, unsigned long Size)
{
	return xf86MapVidMem(0, 0, Base, Size);
}

_X_EXPORT void
xf86UnmapSbusMem(sbusDevicePtr psdp, pointer Base, unsigned long Size)
{
	xf86UnMapVidMem(0, Base, Size);
}

_X_EXPORT void
xf86SbusHideOsHwCursor(sbusDevicePtr psdp)
{
	int fd = xf86Info.screenFd;
	struct wsdisplay_cursor curs;

	curs.which = WSDISPLAY_CURSOR_DOCUR;
	curs.enable = 0;
	if (ioctl(fd, WSDISPLAYIO_SCURSOR, &curs) == -1)
		FatalError("%s: could not disable cursor (%s)",
			   "xf86SbusHideOsHWCursor", strerror(errno));
}