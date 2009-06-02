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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"@(#)Makefile.com	1.12	08/01/10 SMI"
#

LIBRARY= rpcsec.a
VERS = .1

OBJECTS=rpcsec_gss.o rpcsec_gss_misc.o rpcsec_gss_utils.o svc_rpcsec_gss.o

# include library definitions
include ../../Makefile.lib

CPPFLAGS +=     -D_REENTRANT -I$(SRC)/uts/common/gssapi/include  \
		-I$(SRC)/uts/common
 
CFLAGS +=	$(XFFLAG)
CFLAGS64 +=	$(XFFLAG)

DYNFLAGS +=	$(ZIGNORE)

LINTSRC=	$(LINTLIB:%.ln=%)

LIBS  = $(ARLIB) $(DYNLIB)

LDLIBS += -lgss -lnsl -lc

.KEEP_STATE:

lint: lintcheck

# include library targets
include ../../Makefile.targ

# librpcsec build rules

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)