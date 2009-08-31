#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#

#
# Copyright (c) 2007 NEC Corporation
#

LIBRARY=	librtld_db.a
VERS=		.1

COMOBJS=	rtld_db.o	rd_elf.o
BLTOBJ=		msg.o

OBJECTS =	$(BLTOBJ) $(COMOBJS) $(COMOBJS64) $(PLTOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

SRCDIR =	../common

CPPFLAGS +=	-I$(SRCBASE)/lib/libc/inc
DYNFLAGS +=	$(VERSREF)
LDLIBS +=	$(CONVLIBDIR) $(CONV_LIB) -lc

LINTFLAGS +=	-u -erroff=E_NAME_DECL_NOT_USED_DEF2
LINTFLAGS64 +=	-u -erroff=E_NAME_DECL_NOT_USED_DEF2

BLTDEFS=	msg.h
BLTDATA=	msg.c

BLTFILES=	$(BLTDEFS) $(BLTDATA)

SGSMSGCOM=	../common/librtld_db.msg
SGSMSGINTEL=	../common/librtld_db.intel.msg
SGSMSGSPARCV9=	../common/librtld_db.sparcv9.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA)

SRCS=		../common/llib-lrtld_db
LINTSRCS=	$(COMOBJS:%.o=../common/%.c) $(PLTSRCS) $(BLTDATA)

CLEANFILES +=	$(BLTFILES) $(LINTOUTS)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB)

ROOTFS_DYNLIB=	$(DYNLIB:%=$(ROOTFS_LIBDIR)/%)
ROOTFS_LINTLIB=	$(LINTLIB:%=$(ROOTFS_LIBDIR)/%)
ROOTFS_LINKS=	$(ROOTFS_LIBDIR)/$(LIBLINKS)

ROOTFS_DYNLIB64=	$(DYNLIB:%=$(ROOTFS_LIBDIR64)/%)
ROOTFS_LINTLIB64=	$(LINTLIB:%=$(ROOTFS_LIBDIR64)/%)
ROOTFS_LINKS64=		$(ROOTFS_LIBDIR64)/$(LIBLINKS)

$(__LINT)LINTLIB_TARGET		= $(ROOTFS_LIBDIR)/$(LINTLIB)
$(__LINT)LINTLIB_TARGET		+= $(ROOTFS_LIBDIR)/$(LINTLIBSRC)
$(__LINT)LINTLIB64_TARGET	= $(ROOTFS_LIBDIR64)/$(LINTLIB)

$(ROOTFS_DYNLIB) :=	FILEMODE= 755
$(ROOTFS_DYNLIB64) :=	FILEMODE= 755

$(VAR_POUND_1)$(ROOTFS_LIBDIR)/$(LINTLIBSRC): ../common/$(LINTLIBSRC)
	$(VAR_POUND_1)$(INS.file) ../common/$(LINTLIBSRC)
