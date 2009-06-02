#!/sbin/sh
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
# ident	"@(#)vfstab_ufs_sata.sh
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#
# Copyright (c) 2008 NEC Corporation
#

#

echo "#device		device		mount		FS	fsck	mount	mount
#to mount	to fsck		point		type	pass	at boot	options
#
/devices	-		/devices	devfs	-	no	-
/proc		-		/proc		proc	-	no	-
ctfs		-		/system/contract ctfs	-	no	-
objfs		-		/system/object	objfs	-	no	-
fd		-		/dev/fd		fd	-	no	-
swap		-		/tmp		tmpfs	-	yes	-
swap		-		/var/run	tmpfs	-	yes	size=1m
/dev/dsk/c0t0d0s4	-		/usr		ufs	-	yes	-
/dev/dsk/c0t0d0s5	-		/var		ufs	-	yes	-
/dev/dsk/c0t0d0s6	-		/data		ufs	-	yes	-
">vfstab_ufs_sata
