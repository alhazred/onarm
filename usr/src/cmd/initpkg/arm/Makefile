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

include ../../Makefile.cmd

SBINF=		rc0 rc1 rc2 rc5 rc6
ETCTABS=	inittab
ETCINITDF=	mountfs strplumb ipfilter inetinit cron net-physical \
		mountfs_ufs syslogd remountufs
ETCRC0DF=	K74syslogd
ETCRC1DF=	S05remountufs S10mountfsusrvar S15mountfsdata
ETCRC2DF=	S05remountufs S10mountfsusrvar S15mountfsdata \
		S30strplumb S31syslogd S32ipfilter S34inetinit S35net-physical

ROOTETCINITD=	$(ROOTETC)/init.d
ROOTETCRC0D=	$(ROOTETC)/rc0.d
ROOTETCRC1D=	$(ROOTETC)/rc1.d
ROOTETCRC2D=	$(ROOTETC)/rc2.d

FILEMODE=	0744

ROOTSBINF=	$(SBINF:%=$(ROOTSBIN)/%)
ROOTETCTABS=	$(ETCTABS:%=$(ROOTETC)/%)
ROOTETCINITDF=	$(ETCINITDF:%=$(ROOTETCINITD)/%)
ROOTETCRC0DF=	$(ETCRC0DF:%=$(ROOTETCRC0D)/%)
ROOTETCRC1DF=	$(ETCRC1DF:%=$(ROOTETCRC1D)/%)
ROOTETCRC2DF=	$(ETCRC2DF:%=$(ROOTETCRC2D)/%)

$(ROOTETCINITD)/% : %
	$(INS.file)

$(ROOTETCRC0D)/K74syslogd: $(ROOTETCINITD)/syslogd
	$(RM) $@; $(LN) $(ROOTETCINITD)/syslogd $@

$(ROOTETCRC1D)/S05remountufs: $(ROOTETCINITD)/remountufs
	$(RM) $@; $(LN) $(ROOTETCINITD)/remountufs $@

$(ROOTETCRC1D)/S10mountfsusrvar: $(ROOTETCINITD)/mountfs
	$(RM) $@; $(LN) $(ROOTETCINITD)/mountfs $@

$(ROOTETCRC1D)/S15mountfsdata: $(ROOTETCINITD)/mountfs
	$(RM) $@; $(LN) $(ROOTETCINITD)/mountfs $@

$(ROOTETCRC2D)/S05remountufs: $(ROOTETCINITD)/remountufs
	$(RM) $@; $(LN) $(ROOTETCINITD)/remountufs $@

$(ROOTETCRC2D)/S10mountfsusrvar: $(ROOTETCINITD)/mountfs
	$(RM) $@; $(LN) $(ROOTETCINITD)/mountfs $@

$(ROOTETCRC2D)/S15mountfsdata: $(ROOTETCINITD)/mountfs
	$(RM) $@; $(LN) $(ROOTETCINITD)/mountfs $@

$(ROOTETCRC2D)/S30strplumb: $(ROOTETCINITD)/strplumb
	$(RM) $@; $(LN) $(ROOTETCINITD)/strplumb $@

$(ROOTETCRC2D)/S31syslogd: $(ROOTETCINITD)/syslogd
	$(RM) $@; $(LN) $(ROOTETCINITD)/syslogd $@

$(ROOTETCRC2D)/S32ipfilter: $(ROOTETCINITD)/ipfilter
	$(RM) $@; $(LN) $(ROOTETCINITD)/ipfilter $@

$(ROOTETCRC2D)/S34inetinit: $(ROOTETCINITD)/inetinit
	$(RM) $@; $(LN) $(ROOTETCINITD)/inetinit $@

$(ROOTETCRC2D)/S35net-physical: $(ROOTETCINITD)/net-physical
	$(RM) $@; $(LN) $(ROOTETCINITD)/net-physical $@

$(ROOTSBIN)/rc5: $(ROOTSBIN)/rc0
	$(RM) $@; $(LN) $(ROOTSBIN)/rc0 $@

$(ROOTSBIN)/rc6: $(ROOTSBIN)/rc0
	$(RM) $@; $(LN) $(ROOTSBIN)/rc0 $@

$(ROOTETC)/inittab	:= FILEMODE =	0644

install: $(ROOTSBINF) $(ROOTETCTABS) $(ROOTETCINITDF) $(ROOTETCRC0DF) $(ROOTETCRC1DF) $(ROOTETCRC2DF)
