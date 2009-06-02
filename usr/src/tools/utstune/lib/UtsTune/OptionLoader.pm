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
## Load current option configuration.
##

package UtsTune::OptionLoader;

use strict;

use File::Basename;
use UtsTune;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($db, $plat, $objdir) = @_;

	return bless {DB => $db, OBJDIR => $objdir, PARSER => {},
		      PLATFORM => $plat, OPTION => {}, SCOPE => {}}, $class;
}

sub setObjDir
{
	my $me = shift;
	my ($objdir) = @_;

	$me->{OBJDIR} = $objdir;

	# Purge cached data.
	# SCOPE doesn't need to be purged.
	$me->{PARSER} = {};
	$me->{OPTION} = {};
}

sub loadParser
{
	my $me = shift;
	my ($name) = @_;

	my $db = $me->{DB};
	my $objdir = $me->{OBJDIR};

	my $plat = $me->{PLATFORM};
	my $opath = $db->getCurFile($name, $plat, $objdir);
	die "Unknown option name: $name\n" unless ($opath);

	my $parser = $me->{PARSER}->{$opath};
	unless ($parser) {
		unless (-f $opath) {
			my $rpath = dirname(dirname($opath));
			$rpath =~ s,^\Q$UtsTune::UTS\E/*,,;
			my $msg = <<OUT;
Configuration for $rpath does not exist. Try "make tune-init".
OUT
use Carp;
			confess $msg;
			die $msg;
		}

		require UtsTune::PackageLoader;
		import UtsTune::PackageLoader;
		my $loader = UtsTune::PackageLoader->new();
		$parser = $loader->parserInstance(FILE => $opath);
		$me->{PARSER}->{$opath} = $parser;
	}

	return $parser;
}

sub loadOption
{
	my $me = shift;
	my ($name) = @_;

	my $opt = $me->{OPTION}->{$name};
	unless ($opt) {
		my $parser = $me->loadParser($name);
		$opt = $parser->getOption($name);
		$me->{OPTION}->{$name} = $opt;
	}

	return $opt;
}

sub getScope
{
	my $me = shift;
	my ($name) = @_;

	my $scope = $me->{SCOPE}->{$name};
	my $plat = $me->{PLATFORM};
	unless ($scope) {
		my $db = $me->{DB};
		my $modtune = $db->getModTune($name, $plat);
		die "Unknown option name: $name\n" unless ($modtune);

		$scope = $db->getScope($modtune);
		$me->{SCOPE}->{$name} = $scope;
	}

	return $scope;
}

sub getPlatform
{
	my $me = shift;

	return $me->{PLATFORM};
}

1;
