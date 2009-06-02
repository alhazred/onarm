#!/usr/bin/perl

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

use File::Basename;

sub do_symlink($$$);
sub liblink($);

foreach my $library (@ARGV) {
	liblink($library);
}

sub liblink($)
{
	my ($library) = @_;

	my ($fname, $path) = fileparse($library);

	my $sover = $fname;
	return unless ($sover =~ s,^(.*\.so)(\.(\d+))(\.([\d.]))*,$1$2,);
	do_symlink($path, $fname, $sover) unless ($sover eq $fname);

	my $soname = $sover;
	$soname =~ s,\.(\d+)$,,;
	do_symlink($path, $fname, $soname);
}

sub do_symlink($$$)
{
	my ($path, $src, $dst) = @_;

	my $dstpath = $path . '/' . $dst;
	unlink($dstpath);
	symlink($src, $dstpath) or die "symlink($src, $dstpath) failed: $!\n";
}
