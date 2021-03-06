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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"%Z%%M%	%I%	%E% SMI"
#

LIBRARY= librpcsvc.a
VERS = .1

OBJECTS= rstat_simple.o rstat_xdr.o rusers_simple.o rusersxdr.o rusers_xdr.o \
	 rwallxdr.o spray_xdr.o nlm_prot.o sm_inter_xdr.o nsm_addr_xdr.o \
	 bootparam_prot_xdr.o mount_xdr.o mountlist_xdr.o rpc_sztypes.o \
	 bindresvport.o

# include library definitions
include ../../Makefile.lib

# install this library in the root filesystem
include ../../Makefile.rootfs

SRCDIR =	../common

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

LIBS = $(ARLIB) $(DYNLIB) $(LINTLIB)

CPPFLAGS += -DYP

$(LINTLIB):= SRCS = $(SRCDIR)/$(LINTSRC)

LDLIBS += -lnsl -lc

.KEEP_STATE:

lint:	lintcheck

# include library targets
include ../../Makefile.targ
