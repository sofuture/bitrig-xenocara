# $OpenBSD: Makefile,v 1.12 2011/03/08 20:48:59 matthieu Exp $

.include <bsd.xconf.mk>

SUBDIR= bigreqsproto compositeproto dmxproto damageproto \
	fixesproto fontcacheproto fontsproto glproto inputproto \
	kbproto pmproto xineramaproto randrproto \
	recordproto renderproto resourceproto scrnsaverproto \
	videoproto x11proto xcb-proto xcmiscproto xextproto \
	xf86bigfontproto xf86dgaproto xf86miscproto \
	xf86vidmodeproto

.if ${XENOCARA_BUILD_DRI:L} == "yes"
SUBDIR+= xf86driproto dri2proto
.endif

.include <bsd.subdir.mk>

build: _SUBDIRUSE
