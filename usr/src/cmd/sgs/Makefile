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
# Copyright (c) 2007-2008 NEC Corporation
#

include		$(SRC)/cmd/Makefile.cmd

SUBDIRS-common= libconv		\
		.WAIT		\
		libdl		\
		libelf		\
		liblddbg	\
		.WAIT		\
		libld		\
		libldmake	\
		libldstab	\
		librtld		\
		libcrle		\
		sgsdemangler	\
		.WAIT		\
		0@0		\
		ld		\
		ldd		\
		lddstub		\
		rtld		\
		link_audit	\
		.WAIT		\
		librtld_db	\
		ldprof		\
		pvs		\
		crle		\
		ar		\
		dump		\
		elfdump		\
		elfedit		\
		error		\
		gprof		\
		lari		\
		lex		\
		lorder		\
		m4		\
		mcs		\
		moe		\
		nm		\
		prof		\
		ranlib		\
		size		\
		symorder	\
		tsort		\
		unifdef		\
		yacc

SUBDIRS-i386=
SUBDIRS-sparc=	rtld.4.x

SUBDIRS=	$(SUBDIRS-common) $(SUBDIRS-$(MACH))

# On ARM build, some sub directories should not be built.
$(ARM_BLD)SUBDIRFIX_SKIP	= SUBDIRS-skip.arm
$(ARM_BLD)SUBDIRFIX_DIRS	= $(SUBDIRS-common) $(SUBDIRS-$(MACH))
$(ARM_BLD)SUBDIRS		= $(SUBDIR_FIXUP:sh)

# Messaging support
#
POSUBDIRS=	ar		m4		nm \
		tsort		yacc
POFILE=		sgs.po
POFILES=	$(POSUBDIRS:%=%/%.po)

MSGSUBDIRS=	ld		ldd		libld		liblddbg \
		libldstab	librtld		rtld		libelf \
		ldprof		libcrle		pvs		elfdump	\
		elfedit		crle		moe 		lari \
		librtld_db

MSGDIR=		messages


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
lint :=		TARGET= lint
_msg :=		TARGET= catalog
_msg_gettext :=	TARGET= catalog
_msg_sgsmsg :=	TARGET= catalog
chkmsg :=	TARGET= chkmsg

NATIVE_ADD		= native-add
$(ARM_BLD)NATIVE_ADD	=

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	$(NATIVE_ADD) .WAIT $(SUBDIRS)

include		$(SRC)/cmd/Makefile.targ

# Messaging support
#
_msg: _msg_gettext _msg_sgsmsg

_msg_gettext: $(MSGDOMAIN)/$(POFILE)

_msg_sgsmsg: $(MSGDIR)

$(MSGDOMAIN)/$(POFILE): \
		$(MSGDOMAIN) $(POFILE)

$(POFILE):	$(POSUBDIRS)
		$(RM) $(POFILE)
		cat $(POFILES) > $(POFILE)

$(MSGDIR):	$(MSGSUBDIRS) FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

chkmsg:		libconv $(MSGSUBDIRS) FRC

# built from lib/Makefile
install_lib:	FRC
		@ cd lex; pwd; $(MAKE) $@
		@ cd yacc; pwd; $(MAKE) $@

lint:		$(SUBDIRS)

delete \
clean clobber:	native-clobber cross-clobber .WAIT $(SUBDIRS) $(MSGDIR)

$(SUBDIRS):	FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)


# Integration of ld and ld.so.1 in some developement cycles requires that both
# of these modules be built using the new ld.  This `native' target allows us
# to build a local ld which will then be used to build the delivered version of
# itself and ld.so.1.  Once this new functionality appears in the standard ld
# this target can be disabled.

native-add:	native-proto FRC
		@ cd tools/$(MACH); pwd; $(MAKE) native
		@ cd libconv/$(MACH); pwd; $(MAKE)
		@ cd libelf/$(MACH); pwd; $(MAKE) native
		@ cd liblddbg/$(MACH); pwd; $(MAKE) native
		@ cd libldstab/$(MACH); pwd; $(MAKE) native
		@ cd libld/$(MACH); pwd; $(MAKE) native
		@ cd ld/$(MACH); pwd; $(MAKE) native

native-clobber:
		@ cd tools; pwd; $(MAKE) $(TARGET)
		$(RM) -r proto/$(MACH)

native-proto:
		-@mkdir -p proto/$(MACH)

FRC:

#
# Cross-reference customization: ignore the directories named by XRPRUNE,
# and tweak the file globs slightly.
#
XRPRUNE=	rtld.4.x packages abi
XRADD=		*.msg mapfile* llib-[a-z]*
XRDEL=		Makefile* kobj_*

#
# Establish a set of directories for xref to search.  As there are duplicates
# of things like headers, and only one file will be added to the xref database,
# we want xref to list the source file.
#
XRDIRS=		. \
		../../head \
		../../uts/common/krtld \
		../../uts/common/sys \
		../../uts/sparc/sys \
		../../uts/sparc/krtld \
		../../uts/intel/ia32/krtld \
		../../uts/intel/amd64/krtld \
		../../uts/arm/sys \
		../../uts/arm/krtld

xref:		FRC
		@ $(RM) cscope.*
		xref -p -x cscope

# Rules to build ld for cross build environment.
cross:	cross-proto FRC
	@cd tools/$(MACH); pwd; $(MAKE) cross
	@cd libconv/$(MACH); pwd; $(MAKE) cross
	@cd libelf/$(MACH); pwd; $(MAKE) cross
	@cd liblddbg/$(MACH); pwd; $(MAKE) cross
	@cd libldstab/$(MACH); pwd; $(MAKE) cross
	@cd libld/$(MACH); pwd; $(MAKE) cross
	@cd ld/$(MACH); pwd; $(MAKE) cross

cross-proto:
	-@mkdir -p proto/cross/$(MACH)/bin
	-@mkdir -p proto/cross/$(MACH)/lib

cross-clobber:
	@ cd tools; pwd; $(MAKE) $(TARGET)
	$(RM) -r proto/cross/$(MACH)
