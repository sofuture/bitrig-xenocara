# $OpenBSD: Makefile,v 1.14 2011/10/23 13:59:38 matthieu Exp $

DEBUG?=

LIB= GLw

MESA_MAJOR=	7
MESA_MINOR=	10
MESA_TINY=	3

CPPFLAGS += -I${X11BASE}/include \
	-I${.CURDIR}

SRCS= \
	GLwDrawA.c \
	GLwDAUtil.c \
	GLwM1DrawA.c \
	GLwM2DrawA.c

INCS= \
	GLwDrawA.h \
	GLwDrawAP.h \
	GLwMDrawA.h \
	GLwMDrawAP.h

obj: _xenocara_obj

includes:
	cd ${.CURDIR}; for i in ${INCS}; do \
	    j="cmp -s $$i ${DESTDIR}${INCSDIR}/GL/$$i || \
		${INSTALL_DATA} $$i ${DESTDIR}${INCSDIR}/GL"; \
		echo "\tinstalling $$i"; \
		eval "$$j"; \
	done

beforeinstall: includes

afterinstall: glw.pc
	$(INSTALL) -c -m 644 -o root -g wheel glw.pc \
		${DESTDIR}${LIBDIR}/pkgconfig

glw.pc: glw.pc.in
	sed -e 's,@INSTALL_DIR@,$(X11BASE),' \
	    -e 's,@LIB_DIR@,lib,' \
	    -e 's,@VERSION@,$(MESA_MAJOR).$(MESA_MINOR).$(MESA_TINY),' \
	< ${.CURDIR}/glw.pc.in > glw.pc

NOPROFILE=

.include <bsd.lib.mk>
.include <bsd.xorg.mk>

