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

# ident	"@(#)Makefile	1.51	06/10/11 SMI"
#
# build two versions, su and su.static
# su.static is dynamically linked; the .static suffix is historical.

PROG =		su
ROOTFS_PROG =	su.static
DEFAULTFILES =	su.dfl

include		../Makefile.cmd

LDLIBS2			= -lbsm -lsecdb
LINT_PROG		= lint_PROG
_MACH			=
$(ARM_BLD)LDLIBS2	=
$(ARM_BLD)LINT_PROG	=
$(ARM_BLD)_MACH		= _$(MACH)

EMB_PROG =	embedded_su
ROOTEMB_PROG =	$(ROOTLIB)/$(EMB_PROG)

PROG_STATIC = $(ROOTSBIN)/$(ROOTFS_PROG)

# set /usr/bin/su to 4555; set /sbin/su.static to 0555
FILEMODE =	04555

GROUP =		sys

$(PROG_STATIC) := FILEMODE = 0555

# A reduced su.static is created, with just enough functionality
# to satisfy the needs of a single-user login with /usr not mounted.
# In particular, nss_files.so.1 may be dlopen()'ed at runtime.
$(ROOTFS_PROG) :=	LDLIBS += -lpam $(LDLIBS2)

# The standard su is fully functional.
$(PROG) :=	CPPFLAGS += -DDYNAMIC_SU
$(PROG) :=	LDLIBS += -lpam $(LDLIBS2)

LINTFLAGS += -DDYNAMIC_SU

CLOBBERFILES	+= $(ROOTFS_PROG) $(EMB_PROG)

lint :=		LDLIBS += -lpam $(LDLIBS2)

$(ARM_BLD)include Makefile.$(MACH)

.KEEP_STATE:

all:		$(PROG) $(ROOTFS_PROG) $(EMB_PROG)

# install rule for non-setuid /sbin/su.static
# (exec'd by /sbin/sulogin when booting single user)
$(PROG) $(ROOTFS_PROG):	$(PROG)$(_MACH).c
		$(LINK.c) $(PROG)$(_MACH).c -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(PROG_STATIC) $(ROOTPROG) $(ROOTSBINPROG) \
		$(ROOTETCDEFAULTFILES) $(ROOTEMB_PROG)

$(ROOTSBINPROG): $(ROOTPROG)
		$(RM) $(ROOTSBINPROG); \
		$(SYMLINK) ../usr/bin/$(PROG) $(ROOTSBINPROG)

$(ROOTEMB_PROG):
		$(RM) $(ROOTEMB_PROG); \
		$(SYMLINK) ../bin/$(PROG) $(ROOTEMB_PROG)

$(EMB_PROG):
		$(RM) $(EMB_PROG); \
		$(SYMLINK) $(PROG) $(EMB_PROG)

clean:

lint:		$(LINT_PROG)
$(ARM_BLD)	$(LINT.c) $(PROG)$(_MACH).c $(LDLIBS)

include		../Makefile.targ
