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
## Static module configuration script.
##

package UtsTune::Script::Static;

use strict;
use vars qw(@ISA);

use UtsTune::Script;
use UtsTune::DB::Static;

@ISA = qw(UtsTune::Script);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($db, $objdir, $loader, $plat, $architectures) = @_;

	my $me = $class->SUPER::new($db, $objdir, $loader);
	$me->{STATIC_PLAT} = $plat;
	$me->{STATIC_ARCH} = $architectures;
	$me->{STATIC_DB} = {};

	return bless $me, $class;
}

sub getDB
{
	my $me = shift;
	my ($arch) = @_;

	my $db = $me->{STATIC_DB}->{$arch};
	unless ($db) {
		my $architectures = $me->{STATIC_ARCH};
		die "Unknown architecture: $arch\n"
			unless (grep({$arch eq $_} @$architectures));
		my $objdir = $me->getObjDir();
		my $plat = $me->{STATIC_PLAT};
		$db = UtsTune::DB::Static->new($plat, $arch, $objdir, 1);
		$me->{STATIC_DB}->{$arch} = $db;
	}

	return $db;
}

sub defineFunc
{
	my $me = shift;
	my ($loader_val) = @_;

	my $def = $me->SUPER::defineFunc($loader_val);

	$def .= <<'OUT';
sub static_link
{
	my ($arch, $module, $flag) = @_;

	die "static_link: architecture directory name is not specified.\n"
		unless ($arch);
	die "static_link: module name is not specified.\n" unless ($module);

	my $db = $me->getDB($arch);
	$db->put($module, $flag);
}

sub static_link_usrmod
{
	my ($arch, $module, $flag) = @_;

	die "static_link_usrmod: architecture directory name is not " .
		"specified.\n" unless ($arch);
	die "static_link_usrmod: module name is not specified.\n"
		unless ($module);

	my $db = $me->getDB($arch);
	$db->putUsrMod($module, $flag);
}

sub make_option($)
{
	die "make_option: Undefined function.\n";
}
OUT
}

sub syncDB
{
	my $me = shift;

	foreach my $arch (@{$me->{STATIC_ARCH}}) {
		my $db = $me->getDB($arch);
		$db->sync();
	}
}

sub run
{
	my $me = shift;

	$me->SUPER::run(@_);

	# Sync changes.
	$me->syncDB();
}

1;
