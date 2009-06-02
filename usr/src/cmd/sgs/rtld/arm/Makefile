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

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"@(#)Makefile	1.45	06/04/14 SMI"
#

BASEPLAT =	arm

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. profiling support).

P_COMOBJS=	debugdata.o \
		analyze.o	elf.o		external.o	globals.o \
		malloc.o	paths.o		setup.o		util.o \
		dlfcns.o	config_elf.o	locale.o	tsort.o \
		getcwd.o	remove.o	move.o		tls.o \
		cap.o

S_COMOBJS=	debug.o		audit.o		object.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	arm_elf.o	_setup.o	zero.o

CP_MACHOBJS=

S_MACHOBJS=

P_ASOBJS=	boot.o		boot_elf.o	caller.o	dlfcn1.o

S_ASOBJS=

CRTSRCS=
CRTI=
CRTN=
CRTS=		$(CRTI)		$(CRTN)

include		$(SRC)/Makefile.master

# This is safe because linking is done only with the primary compiler.
MAPFILE-ORDER =		../common/mapfile-order-devpro
$(__GNUC)MAPFILE-ORDER =	../common/mapfile-order-gcc

include		$(SRC)/cmd/sgs/rtld/Makefile.com

# Add any machine specific flags.

ADBGENCFLAGS +=	-erroff=%all
ADBGENFLAGS +=	-milp32
ADBSUB=		$(ADBSUB32)
SONAME=		$(VAR_RTLD_SONAME)
CPPFLAGS +=	-DRTLD_REDUCE_DATA
CPPFLAGS +=	-DRTLD_REDUCE_DEBUG_CALL
$(__GNULD)CPPFLAGS	+= -DRTLD_USE_GNULD

# If you want to use dynamic PLT tracing feature, you need to comment out
# -DRTLD_MALLOC_NO_PROTEXEC option below.
CPPFLAGS +=	-DRTLD_MALLOC_NO_PROTEXEC

# Comment out below line if you want to support DTrace fasttrap module.
ZDTRACE_DATA	=

SGSMSGTARG +=	$(SGSMSGARM) $(SGSMSG32)

MAPFILES +=	../common/mapfile-32-vers

ASFLAGS +=   $($(MACH)_AS_XARCH) $(AS_PICFLAGS)

VERS_SCRIPT	= vers-script

# GNU ld can't handle Solaris ld's mapfile. So the following definitions
# are required.
$(__GNULD)DYNFLAGS	+= -Bsymbolic
$(__GNULD)CFLAGS	+= -_gcc=-fvisibility=protected

$(__GNULD)$(RTLD):	$$(VERS_SCRIPT)

.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(VAR_RTLD_ETCDYNLIB)

lint:		$(LINTOUT32)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
