#	$OpenBSD: Makefile,v 1.12 2011/10/23 13:57:36 matthieu Exp $

SUBDIR= swrast

.if ${MACHINE} == i386 || ${MACHINE} == amd64
SUBDIR+= i810 i915 i965 radeon r200 r300 r600
.endif

build depend all install clean cleandir: _SUBDIRUSE

MESA_INCLUDE=	${.CURDIR}/../../../dist/Mesa/include

INCS = \
	dri_interface.h

includes:
	cd ${MESA_INCLUDE}/GL/internal; for i in ${INCS}; do \
	    j="cmp -s $$i ${DESTDIR}${INCSDIR}/GL/internal/$$i || \
		${INSTALL_DATA} $$i ${DESTDIR}${INCSDIR}/GL/internal"; \
		echo "\tinstalling $$i"; \
		eval "$$j"; \
	done

.include <bsd.xorg.mk>


