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
# ident	"@(#)Makefile.com	1.8	06/08/03 SMI"
#
#
# Copyright (c) 2007 NEC Corporation
#

include	../../../Makefile.master

LIBRARY=	libadm.a
VERS=		.1

ALLOBJ= \
ckdate.o     ckgid.o      ckint.o      ckitem.o     ckkeywd.o    ckpath.o  \
ckrange.o    ckstr.o      cktime.o     ckuid.o      ckyorn.o     data.o  \
devattr.o    devreserv.o  devtab.o     dgrpent.o    getdev.o     getdgrp.o  \
getinput.o   getvol.o     listdev.o    listdgrp.o   pkginfo.o  \
pkgnmchk.o   pkgparam.o   putdev.o     putdgrp.o    puterror.o   puthelp.o  \
putprmpt.o   puttext.o    rdwr_vtoc.o  regexp.o     space.o      fulldevnm.o

COMPACTOBJ= \
rdwr_vtoc.o  fulldevnm.o

$(SPARC_BLD)OBJECTS =	$(ALLOBJ)
$(INTEL_BLD)OBJECTS =	$(ALLOBJ)
$(ARM_BLD)OBJECTS =	$(COMPACTOBJ)

include	../../Makefile.lib

# install this library in the root filesystem
include ../../Makefile.rootfs

LIBS=		$(ARLIB) $(DYNLIB) $(LINTLIB)
SRCDIR=		../common

MAPFILES +=	mapfile-vers

CPPFLAGS +=	-I ../inc

$(LINTLIB) :=	SRCS=$(SRCDIR)/$(LINTSRC)
LDLIBS +=	-lc

.KEEP_STATE:

all: $(LIBS)

lint:	lintcheck

include		../../Makefile.targ
