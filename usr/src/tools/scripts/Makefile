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

#
# Copyright (c) 2006-2008 NEC Corporation
#

SHFILES= \
	Install \
	acr \
	bfu \
	bfudrop \
	bindrop \
	bldenv \
	build_cscope \
	bringovercheck \
	checkpaths \
	checkproto \
	check_fnames \
	cstyle \
	elfcmp \
	flg.flp \
	genoffsets \
	keywords \
	makebfu \
	mkacr \
	mkbfu \
	mkclosed \
	nightly \
	onblddrop \
	protocmp.terse \
	sccscheck \
	sccscp \
	sccshist \
	sccsmv \
	sccsrm \
	sdrop \
	webrev \
	ws \
	wx \
	xref

PERLFILES= \
	cddlchk \
	check_rtime \
	genconst \
	hdrchk \
	jstyle \
	mkreadme_osol \
	mktpl \
	subdirfix \
	validate_flg \
	validate_paths \
	wdiff \
	xheader

PYFILES= \
	wsdiff

MAN1FILES= \
	Install.1 \
	acr.1 \
	bldenv.1 \
	bringovercheck.1 \
	cddlchk.1 \
	checkpaths.1 \
	check_rtime.1 \
	cstyle.1 \
	flg.flp.1 \
	genconst.1 \
	jstyle.1 \
	mkacr.1 \
	nightly.1 \
	sccscheck.1 \
	sccscp.1 \
	sccsmv.1 \
	sccsrm.1 \
	subdirfix.1 \
	webrev.1 \
	ws.1 \
	wsdiff.1 \
	wx.1 \
	xheader.1 \
	xref.1

MAKEFILES= \
	xref.mk

POD2MANFILES	= 	\
	cddlchk.1	\
	genconst.1	\
	subdirfix.1	\
	xheader.1

CLEANFILES = $(SHFILES) $(PERLFILES) $(PYFILES)
CLEANFILES = $(SHFILES) $(PERLFILES) $(PYFILES) $(POD2MANFILES)

include ../Makefile.tools

OWNER=		root
GROUP=		bin
$(ROOTONBLDMAN1FILES) := FILEMODE=	644
$(ROOTONBLDMAKEFILES) := FILEMODE=	644

.KEEP_STATE:

all:	$(SHFILES) $(PERLFILES) $(PYFILES) $(MAN1FILES) $(MAKEFILES)

install: all .WAIT $(ROOTONBLDSHFILES) $(ROOTONBLDPERLFILES) \
		$(ROOTONBLDPYFILES) $(ROOTONBLDMAN1FILES) $(ROOTONBLDMAKEFILES)

clean:
	$(RM) $(CLEANFILES)

bldenv: bldenv.sh stdenv.sh
	$(RM) $@
	sed -e '/# STDENV_START/ r stdenv.sh' bldenv.sh > bldenv
	$(CHMOD) +x $@ 

nightly: nightly.sh stdenv.sh
	$(RM) $@
	sed -e '/# STDENV_START/ r stdenv.sh' nightly.sh > nightly
	$(CHMOD) +x $@ 

cddlchk.1:	cddlchk.pl
	$(POD2MAN_BUILD)

genconst.1:	genconst.pl
	$(POD2MAN_BUILD)

subdirfix.1:	subdirfix.pl
	$(POD2MAN_BUILD)

xheader.1:	xheader.pl
	$(POD2MAN_BUILD)

include ../Makefile.targ
