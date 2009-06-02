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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile.com	1.3	07/10/19 SMI"
#

LIBRARY= libkmf.a
VERS= .1

OBJECTS= \
	algoid.o \
	algorithm.o \
	certgetsetop.o \
	certop.o \
	client.o \
	csrcrlop.o \
	generalop.o \
	keyop.o \
	kmfoids.o \
	pem_encode.o \
	pk11tokens.o \
	policy.o \
	pk11keys.o \
	rdn_parser.o

BERDERLIB=      -lkmfberder
BERDERLIB64=    -lkmfberder

CRYPTOUTILLIB=	 -lcryptoutil
CRYPTOUTILLIB64= -lcryptoutil

include $(SRC)/lib/Makefile.lib

SRCDIR=	../common
INCDIR=	../../include

LIBS=	$(DYNLIB) $(LINTLIB)

$(LINTLIB) := SRCS = $(SRCDIR)/$(LINTSRC)

LAZYLIBS=	$(ZLAZYLOAD) -lpkcs11 $(ZNOLAZYLOAD)
lint		:=	LAZYLIBS =	-lpkcs11

LDLIBS		+=	$(BERDERLIB) $(CRYPTOUTILLIB) -lmd $(LAZYLIBS) -lnsl -lsocket -lc 
LDLIBS64	+=	$(BERDERLIB64) $(CRYPTOUTILLIB64) -lmd $(LAZYLIBS) -lnsl -lsocket -lc 

# DYNLIB libraries do not have lint libs and are not linted
$(DYNLIB) :=    LDLIBS += $(ZLAZYLOAD) -lxml2 $(ZNOLAZYLOAD)
$(DYNLIB64) :=  LDLIBS64 += $(ZLAZYLOAD) -lxml2 $(ZNOLAZYLOAD)

CPPFLAGS	+=	-I$(INCDIR) -I/usr/include/libxml2 -I../../ber_der/inc -I$(SRCDIR)

.KEEP_STATE:

all:    $(LIBS)

lint:	lintcheck

include $(SRC)/lib/Makefile.targ
