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
# ident	"%Z%%M%	%I%	%E% SMI"
#

include		../../Makefile.cmd

PROG = zoneshare zoneunshare

ETCZONESFILES=	SUNWtsoldef.xml
ROOTLIBZONES= $(ROOTLIB)/zones
ROOTLIBZONESFILES =  $(PROG:%=$(ROOTLIBZONES)/%)

$(ROOTETCZONESFILES) := FILEMODE = 0444

$(ROOTLIBZONES) := DIRMODE = 0755
$(ROOTLIBZONESFILES) := FILEMODE = 0555
$(ROOTLIBZONESFILES) := OWNER = root
$(ROOTLIBZONESFILES) := GROUP = sys

all: $(PROG)
install: all $(ROOTETCZONESFILES) $(ROOTLIBZONES) $(ROOTLIBZONESFILES)

$(ROOTLIBZONES):
	$(INS.dir)

$(ROOTETCZONESFILES)/% $(ROOTLIBZONES)/%: %
	$(INS.file)

lint:

clean:

_msg:

_dc:

include ../Makefile.targ

.KEEP_STATE:
