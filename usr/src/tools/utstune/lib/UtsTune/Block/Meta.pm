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
## Class for meta data in tune file.
##

package	UtsTune::Block::Meta;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune::Block;
use UtsTune::Type::String;

@ISA = qw(UtsTune::Block);

# Valid meta data.
%PARAMETER = (objdir	=> {type => undef, mandatory => 1},
	      file	=> {type => undef, mandatory => 1},
	      platform	=> {type => 'UtsTune::Type::String'});

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	my $me = $class->SUPER::new(UtsTune::Type::String->new(), \%PARAMETER);

	return bless $me, $class;
}

sub setObjDir
{
	my $me = shift;
	my ($objdir) = @_;

	$me->setStringParameter('objdir', $objdir);
}

sub setFile
{
	my $me = shift;
	my ($file) = @_;

	$me->setStringParameter('file', $file);
}

sub getFile
{
	my $me = shift;

	return $me->getStringParameter('file');
}

sub getPlatform
{
	my $me = shift;

	return $me->getStringParameter('platform');
}

sub setPlatform
{
	my $me = shift;
	my ($plat) = @_;

	return $me->setStringParameter('platform', $plat);
}

sub getBlockDecl
{
	return 'meta';
}

1;
