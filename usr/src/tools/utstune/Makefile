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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/utstune/Makefile"

PLFILES		= utstune.pl utquery.pl statictool.pl
PLEXEFILES	= $(PLFILES:%.pl=%)
PERLFILES	= $(PLEXEFILES)

MAN1FILES	= $(PLFILES:%.pl=%.1)

CLEANFILES	= $(PERLFILES) $(MAN1FILES)

PERL_LIBDIR	= $(ROOTONBLDLIBPERL)

include ../Makefile.tools

UTSTUNE_LIBDIR	= $(ROOTONBLDLIBPERL)/UtsTune
UTSTUNE_INSDIR	=			\
	$(UTSTUNE_LIBDIR)		\
	$(UTSTUNE_LIBDIR)/Block		\
	$(UTSTUNE_LIBDIR)/DB		\
	$(UTSTUNE_LIBDIR)/Type		\
	$(UTSTUNE_LIBDIR)/Script

UTSTUNE_LIBS	=					\
	lib/UtsTune.pm					\
	lib/UtsTune/AbstractIntegerParser.pm		\
	lib/UtsTune/Block.pm				\
	lib/UtsTune/Block/Export.pm			\
	lib/UtsTune/Block/Meta.pm			\
	lib/UtsTune/Block/Option.pm			\
	lib/UtsTune/Block/Static.pm			\
	lib/UtsTune/DB.pm				\
	lib/UtsTune/DB/Parameter.pm			\
	lib/UtsTune/DB/Static.pm			\
	lib/UtsTune/IntegerParser.pm			\
	lib/UtsTune/Parser.pm				\
	lib/UtsTune/MakeOption.pm			\
	lib/UtsTune/OptionLoader.pm			\
	lib/UtsTune/PackageLoader.pm			\
	lib/UtsTune/ParameterParser.pm			\
	lib/UtsTune/QueryParser.pm			\
	lib/UtsTune/QueryProto.pm			\
	lib/UtsTune/QueryServer.pm			\
	lib/UtsTune/Script.pm				\
	lib/UtsTune/Script/Makeopt.pm			\
	lib/UtsTune/Script/Static.pm			\
	lib/UtsTune/TuneDefParser.pm			\
	lib/UtsTune/Type.pm				\
	lib/UtsTune/Type/Bool.pm			\
	lib/UtsTune/Type/Integer.pm			\
	lib/UtsTune/Type/Integer64.pm			\
	lib/UtsTune/Type/Long.pm			\
	lib/UtsTune/Type/String.pm			\
	lib/UtsTune/Type/Symbol.pm			\
	lib/UtsTune/Type/UnsignedInteger.pm		\
	lib/UtsTune/Type/UnsignedInteger64.pm		\
	lib/UtsTune/Type/UnsignedLong.pm		\
	lib/UtsTune/Util.pm

STATICTOOL_LIBS	=					\
	lib/StaticTool.pm				\
	lib/StaticTool/BindingLexer.pm			\
	lib/StaticTool/DacfConfParser.pm		\
	lib/StaticTool/DriverAliasesParser.pm		\
	lib/StaticTool/DriverClassesParser.pm		\
	lib/StaticTool/DriverConfParser.pm		\
	lib/StaticTool/IntBindParser.pm			\
	lib/StaticTool/ModInfoParser.pm			\
	lib/StaticTool/PackageLoader.pm			\
	lib/StaticTool/StructDumper.pm

STATICTOOL_LIBDIR	= $(ROOTONBLDLIBPERL)/StaticTool
STATICTOOL_INSDIR	=			\
	$(STATICTOOL_LIBDIR)

ROOTONBLDLIBPERLFILES	= $(UTSTUNE_LIBS:lib/%=$(ROOTONBLDLIBPERL)/%)	\
			  $(STATICTOOL_LIBS:lib/%=$(ROOTONBLDLIBPERL)/%)

OWNER	= root
GROUP	= bin
$(ROOTONBLDMAN1FILES)		:= FILEMODE = 644
$(ROOTONBLDLIBPERLFILES)	:= FILEMODE = 644

.KEEP_STATE:

all:	$(PERLFILES) $(MAN1FILES) $(MAKEFILES)

%:	%.pl
	$(PERL) $< $(PERL) $(PERL_LIBDIR) || ($(RM) $@; exit 1)

install:	all $(UTSTUNE_INSDIR) $(STATICTOOL_INSDIR) .WAIT	\
		$(ROOTONBLDPERLFILES) $(ROOTONBLDLIBPERLFILES)		\
		$(ROOTONBLDMAN1FILES)

clean:
	$(RM) $(CLEANFILES)

%.1:	%.pl
	$(POD2MAN_BUILD)

$(UTSTUNE_INSDIR):
	$(INS.dir)

$(STATICTOOL_INSDIR):
	$(INS.dir)

$(ROOTONBLDLIBPERL)/%:	lib/%
	$(INS.file)

include ../Makefile.targ
