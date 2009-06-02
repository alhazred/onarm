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
# ident	"@(#)Makefile.com	1.10	07/10/13 SMI"
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# cmd/sgs/messages/Makefile.com

#
# Copyright (c) 2007-2008 NEC Corporation
#

include		$(SRC)/Makefile.master
include		$(SRC)/cmd/sgs/Makefile.com


# Establish our own domain.

TEXT_DOMAIN=	SUNW_OST_SGS

POFILE=		sgs.po

MSGFMT=		msgfmt

# The following message files are generated as part of each utilites build via
# sgsmsg(1l).  By default each file is formatted as a portable object file
# (.po) - see msgfmt(1).  If the sgsmsg -C option has been employed, each file
# is formatted as a message text source file (.msg) - see gencat(1).

POFILES-common=	ld		ldd		libld		liblddbg \
		libldstab	librtld		rtld		libelf \
		ldprof		libcrle		crle		moe \
		pvs 		elfdump		elfedit		lari
POFILES	= $(POFILES-common)

# These message files are generated as a side effect of generating the
# elfedit messages. Otherwise they are the same thing as POFILES
POFILES_ELFEDIT_MODULES = \
		elfedit_cap 	elfedit_dyn	elfedit_ehdr	elfedit_phdr \
		elfedit_shdr 	elfedit_str	elfedit_sym	elfedit_syminfo\
		lari

# Eliminate catalogs that are not supported on ARM build.
$(ARM_BLD)SUBDIRFIX_SKIP	= $(SRC)/cmd/sgs/SUBDIRS-skip.arm
$(ARM_BLD)SUBDIRFIX_DIRS	= $(POFILES-common)
$(ARM_BLD)POFILES		= $(SUBDIR_FIXUP:sh)

# Define a local version of the message catalog.  Test using: LANG=piglatin

MSGDIR=		$(ROOT)/usr/lib/locale/piglatin/LC_MESSAGES
TEST_MSGID=	test-msgid.po
TEST_MSGSTR=	test-msgstr.po
TEST_POFILE=	test-msg.po
TEST_MOFILE=	$(TEXT_DOMAIN).mo


CLEANFILES=	$(POFILE) $(TEST_MSGID) $(TEST_MSGSTR) $(TEST_POFILE) \
		$(TEST_MOFILE)
CLOBBERFILES=	$(POFILES) $(POFILES_ELFEDIT_MODULES)
