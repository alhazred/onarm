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
# Copyright (c) 2008-2009 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/devconf/Makefile"

PLFILES		= devconf.pl
PLEXEFILES	= $(PLFILES:%.pl=%)
PERLFILES	= $(PLEXEFILES)

MAN1FILES	= $(PLFILES:%.pl=%.1)

CLEANFILES	= $(PERLFILES) $(MAN1FILES)

PERL_LIBDIR	= $(ROOTONBLDLIBPERL)

include ../Makefile.tools

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber

DEVCONF_LIBDIR	= $(ROOTONBLDLIBPERL)/DevConf
DEVCONF_INSDIR	=			\
	$(DEVCONF_LIBDIR)		\
	$(DEVCONF_LIBDIR)/Block		\
	$(DEVCONF_LIBDIR)/Type

DEVCONF_LIBS	=			\
	lib/DevConf/Block/DefObj.pm	\
	lib/DevConf/Block/Device.pm	\
	lib/DevConf/Block/Register.pm	\
	lib/DevConf/Constants.pm	\
	lib/DevConf/Parser.pm		\
	lib/DevConf/Properties.pm	\
	lib/DevConf/Type/CppByte.pm	\
	lib/DevConf/Type/CppInt.pm	\
	lib/DevConf/Type/CppInt64.pm	\
	lib/DevConf/Type/CppString.pm	\
	lib/DevConf/Type/PropValue.pm

ROOTONBLDLIBPERLFILES	= $(DEVCONF_LIBS:lib/%=$(ROOTONBLDLIBPERL)/%)

OWNER	= root
GROUP	= bin
$(ROOTONBLDMAN1FILES)		:= FILEMODE = 644
$(ROOTONBLDLIBPERLFILES)	:= FILEMODE = 644

.KEEP_STATE:

.PARALLEL:	$(ROOTONBLDLIBPERLFILES)

all:	$(PERLFILES) $(SUBDIRS) $(MAN1FILES)

%:	%.pl
	$(PERL) $< $(PERL) $(PERL_LIBDIR) || ($(RM) $@; exit 1)

install:	$(DEVCONF_INSDIR) .WAIT			\
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

$(DEVCONF_INSDIR):
	$(INS.dir)

$(STATICTOOL_INSDIR):
	$(INS.dir)

$(ROOTONBLDLIBPERL)/%:	lib/%
	$(INS.file)

FRC:
