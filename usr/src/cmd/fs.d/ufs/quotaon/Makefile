#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
#ident	"%Z%%M%	%I%	%E% SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/quotaon/Makefile

FSTYPE=		ufs
LIBPROG=	quotaon
QUOTAOFF=	quotaoff
ATTMK=		$(LIBPROG)

#
# Note that no links are made from the rc[S0-3].d directories for this rc
# script.  The normal way to ship this is completely disabled.  The process
# for enabling this script is described in the "Routine System Administration
# Guide" (as of this time).
#
INIT_SCRIPT=	ufs_quota
ROOTINIT_D=	$(ROOTETC)/init.d
ROOTINIT_SCRIPT=$(ROOTINIT_D)/$(INIT_SCRIPT)

OTHERINSTALL=	$(ROOTLIBFSTYPE)/$(QUOTAOFF) \
		$(ROOTUSRSBIN)/$(LIBPROG) $(ROOTUSRSBIN)/$(QUOTAOFF) \
		$(ROOTINIT_SCRIPT)
LINKVALUEON=	../lib/fs/$(FSTYPE)/$(LIBPROG)
LINKVALUEOFF=	../lib/fs/$(FSTYPE)/$(QUOTAOFF)

include		../../Makefile.fstype

$(ROOTINIT_SCRIPT) :=	OWNER = root
$(ROOTINIT_SCRIPT) :=	GROUP = sys
$(ROOTINIT_SCRIPT) :=	FILEMODE = 0744

CPPFLAGS +=	-D_LARGEFILE64_SOURCE
lint:		lint_PROG

$(ROOTLIBFSTYPE)/$(QUOTAOFF):	$(ROOTLIBFSTYPE)/$(LIBPROG) 
		-$(RM) $@; $(LN) $(ROOTLIBFSTYPE)/$(LIBPROG) $@

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUEON) $@

$(ROOTUSRSBIN)/$(QUOTAOFF):
		-$(RM) $@; $(SYMLINK) $(LINKVALUEOFF) $@

$(ROOTINIT_D)/% :	%
		$(INS.file)
