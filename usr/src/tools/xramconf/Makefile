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

#ident	"@(#)tools/xramconf/Makefile"

PLFILES		= xramconf.pl
PLEXEFILES	= $(PLFILES:%.pl=%)
PERLFILES	= $(PLEXEFILES)

MAN1FILES	= $(PLFILES:%.pl=%.1)

CLEANFILES	= $(PERLFILES) $(MAN1FILES)

PERL_LIBDIR	= $(ROOTONBLDLIBPERLMACH) $(ROOTONBLDLIBPERL)

include ../Makefile.tools

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber

SUBDIRS	= XS

XRAMCONF_LIBDIR	= $(ROOTONBLDLIBPERL)/XRamConf
XRAMCONF_INSDIR	=			\
	$(XRAMCONF_LIBDIR)		\
	$(XRAMCONF_LIBDIR)/Block	\
	$(XRAMCONF_LIBDIR)/Constants	\
	$(XRAMCONF_LIBDIR)/Image	\
	$(XRAMCONF_LIBDIR)/Kernel	\
	$(XRAMCONF_LIBDIR)/Parser	\
	$(XRAMCONF_LIBDIR)/Struct	\
	$(XRAMCONF_LIBDIR)/Type

XRAMCONF_LIBS	=				\
	lib/XRamConf/Block/Kernel.pm		\
	lib/XRamConf/Block/UserData.pm		\
	lib/XRamConf/Block/XRamDev.pm		\
	lib/XRamConf/ByteOrder.pm		\
	lib/XRamConf/Constants.pm		\
	lib/XRamConf/Constants/arm.pm		\
	lib/XRamConf/Digest.pm			\
	lib/XRamConf/Elf.pm			\
	lib/XRamConf/Image/arm.pm		\
	lib/XRamConf/Kernel/arm.pm		\
	lib/XRamConf/Parser.pm			\
	lib/XRamConf/Parser/Group.pm		\
	lib/XRamConf/Parser/Passwd.pm		\
	lib/XRamConf/Parser/arm.pm		\
	lib/XRamConf/ProcUtil.pm		\
	lib/XRamConf/Struct.pm			\
	lib/XRamConf/Struct/Member.pm		\
	lib/XRamConf/Struct/StringMember.pm	\
	lib/XRamConf/Struct/WbCmd.pm		\
	lib/XRamConf/Struct/WbDigest.pm		\
	lib/XRamConf/Struct/WbFile.pm		\
	lib/XRamConf/Struct/WbInfo.pm		\
	lib/XRamConf/Type/Cache.pm		\
	lib/XRamConf/Type/Octal.pm		\
	lib/XRamConf/Type/Prot.pm		\
	lib/XRamConf/Type/Raw.pm

ROOTONBLDLIBPERLFILES	= $(XRAMCONF_LIBS:lib/%=$(ROOTONBLDLIBPERL)/%)

OWNER	= root
GROUP	= bin
$(ROOTONBLDMAN1FILES)		:= FILEMODE = 644
$(ROOTONBLDLIBPERLFILES)	:= FILEMODE = 644

.KEEP_STATE:

.PARALLEL:	$(ROOTONBLDLIBPERLFILES)

all:	$(PERLFILES) $(SUBDIRS) $(MAN1FILES)

%:	%.pl
	$(PERL) $< $(PERL) $(PERL_LIBDIR) || ($(RM) $@; exit 1)

install:	$(XRAMCONF_INSDIR) .WAIT			\
		$(ROOTONBLDPERLFILES) $(ROOTONBLDLIBPERLFILES)	\
		$(ROOTONBLDMAN1FILES) $(SUBDIRS)

$(ROOTONBLDPERLFILES):	$(PERLFILES)
$(ROOTONBLDMAN1FILES):	$(MAN1FILES)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

local-clean:
	$(RM) $(CLEANFILES)

clean clobber:	local-clean $(SUBDIRS)

%.1:	%.pl
	$(POD2MAN_BUILD)

$(XRAMCONF_INSDIR):
	$(INS.dir)

$(STATICTOOL_INSDIR):
	$(INS.dir)

$(ROOTONBLDLIBPERL)/%:	lib/%
	$(INS.file)

FRC:
