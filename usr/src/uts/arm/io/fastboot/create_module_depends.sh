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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

LISTOPT=
UTSTUNE=utstune

CAT=/usr/bin/cat
RM=/usr/bin/rm
MV=/usr/bin/mv
DIFF=/usr/bin/diff
AWK=/usr/bin/awk

usage()
{
	$CAT 1>&2 <<EOF
Usage: create_module_depends.sh [-o outfile][-p platform][-u utstune] srcdir
EOF
	exit 1
}

while getopts o:p:u: opt; do
	case $opt in
		o)	TARGET=$OPTARG;;
		p)	LISTOPT="-p $OPTARG";;
		u)	UTSTUNE=$OPTARG;;
		?)	usage;;
	esac
done

shift `expr $OPTIND - 1`

SRCDIR=$1
[ -z "$SRCDIR" ] && usage
if [ ! -d $SRCDIR ]; then
	echo "*** ERROR: directory not found: $SRCDIR" 1>&2
	exit 1
fi

tmpfile=
if [ -z "$TARGET" ]; then
	outfile=/dev/fd/1
elif [ -f $TARGET ]; then
	tmpfile="${TARGET}_tmp_$$"
	outfile=$tmpfile
else
	outfile=$TARGET
fi

for dir in `$UTSTUNE -L $LISTOPT`; do
	info="$SRCDIR/$dir/modinfo"
	module=`$AWK '/modname/ {printf("%s\n",$2);}' $info`
	if [ -n "$module" ]; then
		echo "M $module" >> $outfile
	fi

	list2=`$AWK '/depends/{for(i=2;i<=NF;i++)print $i;}' $info`
	for depend in $list2; do
		echo $depend | \
			$AWK 'BEGIN{FS = "/"}{print "n",$NF;}' >> $outfile
	done
done

if [ -n "$tmpfile" ]; then
	$DIFF $tmpfile $TARGET > /dev/null
	if [ $? -eq 0 ]; then
		echo "=== '$TARGET' is up to date."
		$RM -f $tmpfile
	else
		$MV -f $tmpfile $TARGET
	fi
fi
