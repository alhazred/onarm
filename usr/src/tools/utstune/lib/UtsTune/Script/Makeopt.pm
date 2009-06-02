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
# Copyright (c) 2007-2008 NEC Corporation
# All rights reserved.
#

##
## Makefile option configuration script.
##

package UtsTune::Script::Makeopt;

use strict;
use vars qw(@ISA);

use UtsTune::Script;

@ISA = qw(UtsTune::Script);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($db, $loader, $scopelist) = @_;

	my $me = $class->SUPER::new($db, undef, $loader);
	$me->{MAKEOPT} = [];
	$me->setScope($scopelist);

	return bless $me, $class;
}

sub defineFunc
{
	my $me = shift;
	my ($loader_val) = @_;

	my $def = $me->SUPER::defineFunc($loader_val);

	$def .= <<'OUT';
sub static_link
{
	die "static_link: Undefined function.\n";
}

sub static_link_usrmod
{
	die "static_link_usrmod: Undefined function.\n";
}

sub make_option($)
{
	push(@{$me->{MAKEOPT}}, $_[0]);
}
OUT
}

sub setObjDir
{
	my $me = shift;

	$me->SUPER::setObjDir(@_);
	$me->{MAKEOPT} = [];
}

sub getOptions
{
	my $me = shift;

	return (wantarray) ? @{$me->{MAKEOPT}} : $me->{MAKEOPT};
}

1;
