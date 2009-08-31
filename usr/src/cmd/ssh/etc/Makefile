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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#

#
# Copyright (c) 2008 NEC Corporation
#

MANIFEST = ssh.xml
SVCMETHOD = sshd

include ../../Makefile.cmd
include ../Makefile.ssh-common

IPV6DISABLE=
$(USE_INET6)IPV6DISABLE=$(POUND_SIGN)

ETCSSHDIR=	$(ROOTETC)/ssh
DIRS=		$(ETCSSHDIR)

FILES= ssh_config moduli
FILE1= sshd_config

ETCSSHFILES=	$(FILES:%=$(ETCSSHDIR)/%)
ETCSSHFILES1=    $(FILE1:%=$(ETCSSHDIR)/%)

$(ETCSSHFILES) := FILEMODE=	644
$(ETCSSHFILES) := OWNER=		root
$(ETCSSHFILES) := GROUP=		sys

ROOTMANIFESTDIR = $(ROOTSVCNETWORK)

$(ETCSSHDIR)/% : %
	$(INS.file)

$(DIRS):
	$(INS.dir)

$(POFILE):

all lint clean clobber _msg:

install: all $(DIRS) $(ETCSSHFILES) $(ETCSSHFILES1) $(ROOTMANIFEST) \
		$(ROOTSVCMETHOD)
	$(IPV6DISABLE) $(CHMOD) 600 $(ETCSSHFILES1);
	$(IPV6DISABLE) $(MV) $(ETCSSHFILES1) $(ETCSSHFILES1).bak;
	$(IPV6DISABLE) $(SED) \
	$(IPV6DISABLE) -e 's/#ListenAddress 0.0.0.0/ListenAddress 0.0.0.0/g' \
	$(IPV6DISABLE) -e 's/ListenAddress ::/#ListenAddress ::/g' \
	$(IPV6DISABLE) -e 's/PermitRootLogin no/PermitRootLogin yes/g' \
	$(IPV6DISABLE) $(ETCSSHFILES1).bak > $(ETCSSHFILES1); 
	$(IPV6DISABLE) $(RM) $(ETCSSHFILES1).bak; 
	$(IPV6DISABLE) $(CHMOD) 644 $(ETCSSHFILES1);

check: $(CHKMANIFEST)

include ../../Makefile.targ
