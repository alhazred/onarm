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
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

PROG= who
XPG4PROG= who
DCFILE=$(PROG).dc
UTMPS= utmpx
ROOTADM= $(ROOT)/var/adm
ROOTUTMPS= $(UTMPS:%=$(ROOTADM)/%)
SYMSETC= $(UTMPS:%=$(ROOTETC)/%)
CLOBBERFILES += $(UTMPS)

include ../Makefile.cmd

SED= sed

$(ROOTUTMPS):= FILEMODE= 644
$(ROOTUTMPS):= OWNER= root
$(ROOTUTMPS):= GROUP= bin

$(ROOTADM)/%: %
	$(INS.file)

.KEEP_STATE:

CFLAGS += $(CCVERBOSE)
$(XPG4):= CPPFLAGS += -DXPG4

all: $(PROG) $(UTMPS) $(XPG4)

$(UTMPS):
	touch $@

install: all $(ROOTPROG) $(ROOTUTMPS) $(SYMSETC) $(ROOTXPG4PROG)

$(SYMSETC):
	-$(RM) $@; $(SYMLINK) ../var/adm/$(@F) $@
	$(CH)@-$(CHOWN) root $@
	$(CH)@-$(CHGRP) bin $@

clean:

lint:	lint_PROG

$(DCFILE): $(PROG).c
	$(RM) $(DCFILE)
	$(COMPILE.cpp) $(PROG).c | \
	$(XGETTEXT) $(XGETFLAGS) -t -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../Makefile.targ
