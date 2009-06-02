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
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/xramconf/XS/Makefile.xs"

##
## Wrapper makefile for ExtUtils::MakeMaker.
##

MAKEFILE	= Makefile
MAKEFILE_PL	= $(MAKEFILE).PL
MAKEFILE_OLD	= $(MAKEFILE).old
TYPEMAP		= typemap

include ../../../Makefile.tools

all:	$(MAKEFILE)
	$(MAKE) -f $(MAKEFILE) all

install:	$(MAKEFILE)
	$(MAKE) -f $(MAKEFILE) xs_install

clean:
	@if [ -f $(MAKEFILE) ]; then			\
	    $(MAKE) -f $(MAKEFILE) $@;			\
	elif [ -f $(MAKEFILE_OLD) ]; then		\
	    $(MAKE) -f $(MAKEFILE_OLD) $@;		\
	fi

clobber:	clean
	$(RM) $(MAKEFILE) $(MAKEFILE_OLD)

$(MAKEFILE):	$(MAKEFILE_PL) $(TYPEMAP)
	$(PERL) $(MAKEFILE_PL) $(ROOTONBLDLIBPERL) 	\
		$(ROOTONBLDLIBPERLMACH) "CC=$(CC)" "LD=$(CC)"
