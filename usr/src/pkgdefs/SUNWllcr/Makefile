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
# Copyright 1998-1999, 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"
#

include ../Makefile.com

DATAFILES += depend i.initd
CHKINSTALLSRC=checkinstall.initd

.KEEP_STATE:

all: $(FILES) action
# action is a pseudotarget denoting completed work on the depend file
action: depend
	grep SUNWllc depend > /dev/null || \
	(chmod 666 depend; \
	echo "P SUNWllc	LLC2 core functionality" >> depend; \
	chmod 444 depend )
	touch $@

install: all pkg

include ../Makefile.targ
