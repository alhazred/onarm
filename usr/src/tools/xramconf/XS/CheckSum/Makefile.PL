# -*- mode: perl; -*-

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

use 5.008;
use ExtUtils::MakeMaker;

#
# Override MY::makefile to disable Makefile dependency.
# It breaks .KEEP_STATE mechanism.
#
sub MY::makefile
{
	my $me = shift;

	my @m;
	if ($me->{OBJECT}) {
		push(@m, <<'OUT');

$(OBJECT):	$(FIRST_MAKEFILE)

OUT
	}

	return join('', @m);
}

#
# Override MY::install to append "xs_install" target which is required
# for Solaris dmake.
#
sub MY::install
{
	my $me = shift;

	use ExtUtils::MM;
	my $ret = ExtUtils::MM->install(@_);

	$ret .= <<'OUT';

xs_install:	all .WAIT pure_install

OUT

	return $ret;
}

my (%args) = (NAME => 'XRamConf::Digest::CheckSum');
my $libdir = shift(@ARGV);
my $archlibdir = shift(@ARGV);
if ($libdir) {
	$args{INSTALLSITELIB} = $libdir;
	$args{INSTALLSITELIB} = $libdir;
	$args{INSTALLARCHLIB} = $archlibdir;
	$args{INSTALLSITEARCH} = $archlibdir;
}

foreach my $arg (@ARGV) {
	my ($key, $value) = split(/=/, $arg, 2);
	$args{$key} = $value;
}

# Create Makefile.
WriteMakefile(%args);
