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

PROG = devfspolicy 

include ../../Makefile.cmd

COMMON = ..

FILEMODE = 0744

ETCINITD= $(ROOTETC)/init.d

INITDEVFSPOLICY = initdevfspolicy
ETCINITDDEVFSPOLICY = $(ETCINITD)/$(INITDEVFSPOLICY)
INITDEVFSPOLICYMODE = 0744

RCDIR = $(ROOTETC)/rc2.d
INITDEVFSPOLICYS = S22devfspolicy

PLCYSRC = devpolicy.c plcysubr.c devfsadm.c

MODLOADDIR = $(COMMON)/../modload

CPPFLAGS += -D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT \
	-I.. -I$(MODLOADDIR) -D_DEVFSPOLICY
CFLAGS += $(CCVERBOSE) $(C_PICFLAGS) -I.. -I$(MODLOADDIR)

LDLIBS += -ldevinfo -lgen

SRCS = $(PLCYSRC:%=$(COMMON)/%)
OBJS = $(PLCYSRC:%.c=%.o)

POFILES = $(SRCS:.c=.po)
POFILE = devfspolicy.po

all: $(PROG) 

install: all $(ROOTUSRSBINPROG) ins_INITDEVFSPOLICY  ln_DEVFSPOLICYS

ins_INITDEVFSPOLICY	: $(INITDEVFSPOLICY)
	-$(RM) $(ETCINITD)/$(INITDEVFSPOLICY)
	$(INS) -s -m $(INITDEVFSPOLICYMODE) -f  $(ETCINITD) $(INITDEVFSPOLICY)

ln_DEVFSPOLICYS	: 
	-$(RM) $(RCDIR)/$(INITDEVFSPOLICYS)
	$(LN) $(ETCINITD)/$(INITDEVFSPOLICY) $(RCDIR)/$(INITDEVFSPOLICYS)

clean:
	$(RM) $(OBJS) $(POFILE) $(POFILES)

lint: $(PLCYSC:%.c=%.ln)

devpolicy.ln: FRC
	$(LINT.c) devpolicy.c $(LDLIBS)

plcysubr.ln: FRC
	$(LINT.c) plcysubr.c $(LDLIBS)

devfsadm.ln: FRC
	$(LINT.c) devfsadm.c $(LDLIBS)

%.ln: FRC
	$(LINT.c) $(PLCYSRC) $(@:.ln=.c) $(LDLIBS)

FRC:

include ../../Makefile.targ

$(PROG):	$(OBJS)
	$(LINK.c) -o $@ $< $(OBJS) $(LDLIBS)

$(POFILE): $(POFILES)
	$(RM) $@; cat $(POFILES) > $@

%.o: $(COMMON)/%.c
	$(COMPILE.c) -o $@ $<

#
# Source shared with add_drv/update_drv
#
../plcysubr.c:
	rm -rf $@
	ln -s ../modload/plcysubr.c ..

