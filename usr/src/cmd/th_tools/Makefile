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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"

PROG = th_manage th_define
SHFILES = th_script

include ../Makefile.cmd

CFLAGS += -I$(SRC)/uts/common
LINTFLAGS += -I$(SRC)/uts/common
C99MODE= $(C99_ENABLE)
CFLAGS += $(CCVERBOSE)

SRCS = $(PROG:%=%.c)
OBJECTS = $(SRCS:%.c=%.o)

LDLIBS += -ldevinfo
th_manage := LDLIBS += -ldevice

ROOTPROG = $(PROG:%=$(ROOTUSRSBIN)/%)

CLOBBERFILES = $(PROG) $(SHFILE)

OWNER = root
$(ROOTPROG) := FILEMODE = 0555

.KEEP_STATE:

all: $(PROG) $(SHFILES)

$(SHFILES): $(SHFILES).sh
	-$(RM) $(SHFILES)
	$(CP) $(SHFILES).sh $(SHFILES)

install: all $(ROOTUSRSBINPROG) $(ROOTLIBSHFILES)

clean:
	$(RM) $(OBJECTS) core

lint:
	$(LINT.c) th_define.c $(LDLIBS)
	$(LINT.c) th_manage.c $(LDLIBS) -ldevice

include ../Makefile.targ
