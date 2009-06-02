#!/usr/bin/sh
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
# ident	"@(#)df.sh	1.7	06/11/01 SMI"
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Replace /usr/ucb/df
#

ARG=-k
FSSPEC=N
INODES=N
 
while [ $# -gt 0 ]
do
	flag=$1
	case $flag in
	'-F')
		ARG="$ARG -F"
		FSSPEC=Y
		;;
	'-t')
		ARG="$ARG -F"
		FSSPEC=Y
		shift
		if [ "$1" = "4.2" ]
		then
			ARG="$ARG ufs"
		else
			ARG="$ARG $1"
		fi
		;;
	'-i')
		ARG="$ARG -o i"
		INODES=Y
		;;
	*)
		ARG="$ARG $flag"
		;;
	esac
	if [ $# -gt 0 ]; then
		shift
	fi
done
if [ "$INODES" = "Y" ] && [ "$FSSPEC" = "N" ]; then
	ARG="-F ufs $ARG"
fi
exec /usr/sbin/df $ARG
