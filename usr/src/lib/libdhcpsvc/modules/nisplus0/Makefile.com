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
# Copyright (c) 2001 by Sun Microsystems, Inc.
# All rights reserved.
#

#
# Copyright (c) 2007 NEC Corporation
#

#ident	"%Z%%M%	%I%	%E% SMI"
#
# lib/libdhcpsvc/modules/nisplus0/Makefile.com

LIBRARY = ds_SUNWnisplus.a
VERS    = .0
LOCOBJS = nisplus_impl.o dhcp_network.o
OBJECTS = $(LOCOBJS) common.o 

# include library definitions
include $(SRC)/lib/libdhcpsvc/modules/Makefile.com

COMMON_DIR = ../../nisplus_common
SRCS    = $(LOCOBJS:%.o=../%.c) $(COMMON_DIR)/common.c
LDLIBS += -lgen -lnsl -ldhcpsvc -linetutil -lc
CPPFLAGS += -I.. -I$(COMMON_DIR)

.KEEP_STATE:

all:	$(LIBS)

# include library targets
include $(SRC)/lib/libdhcpsvc/modules/Makefile.targ

objs/%.o pics/%.o: $(COMMON_DIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
