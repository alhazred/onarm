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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#

CLASS		= 32
PLATFORM	= sun4u
MODULE		= libmd_psr.so.1

include	../Makefile.com

# Override OBJECTS here because each $MACH/$PLATFORM can have a different
# set of algortithm optimisations and thus different source and object files.
OBJECTS		= md5.o sha1.o sha1_asm.o

MAPFILES =	mapfile

ASFLAGS	+=	-xarch=v8plusa -warn

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).
 
BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS)

.KEEP_STATE:

all:		$(LIBS)

$(ROOT_PSM_LIB_DIR)/% := FILEMODE = 755

install:	all $(LIBMD_PSR_LINKS) $(ROOT_PSM_LIBS)

include $(SRC)/Makefile.psm.targ
