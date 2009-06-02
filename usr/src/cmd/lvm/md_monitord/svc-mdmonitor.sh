#!/bin/sh
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
# ident	"@(#)svc-mdmonitor.sh	1.11	06/06/30 SMI"
#
# Start mdmonitord.

MDMONITORD=/usr/sbin/mdmonitord

. /lib/svc/share/smf_include.sh

if [ ! -x $MDMONITORD ]; then
	echo "$MDMONITORD is missing or not executable."
	exit $SMF_EXIT_ERR_CONFIG
fi

$MDMONITORD 
error=$?
case $error in
0)	exit 0
	;;

*)	echo "Could not start $MDMONITORD. Error $error."
	exit $SMF_EXIT_ERR_FATAL
	;;
esac
