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

##
## Class for export block in tune file.
##

package	UtsTune::Block::Export;

use strict;
use vars qw(@ISA %PARAMETER %TYPES);

use UtsTune;
use UtsTune::Block;
use UtsTune::Type::String;

@ISA = qw(UtsTune::Block);

# Valid parameters.
%PARAMETER = (type	=> {type => 'UtsTune::Type::String',
			    mandatory => 1},
	      scope	=> {type => 'UtsTune::Type::String',
			    mandatory => 1});

# Valid types.
%TYPES = (architecture => 1, platform => 1);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	my $me = $class->SUPER::new(UtsTune::Type::String->new(), \%PARAMETER);

	return bless $me, $class;
}

sub getScopeType
{
	my $me = shift;

	return $me->getStringParameter('type');
}

sub getScope
{
	my $me = shift;

	return $me->getStringParameter('scope');
}

sub getArchitectureScope
{
	my $me = shift;

	my $type = $me->getScopeType();

	return ($type eq 'architecture') ? $me->getScope() : undef;
}

sub getPlatformScope
{
	my $me = shift;

	my $type = $me->getScopeType();

	return ($type eq 'platform') ? $me->getScope() : undef;
}

sub getBlockDecl
{
	return 'export';
}

sub fixup
{
	my $me = shift;
	my ($parser) = @_;

	$me->SUPER::fixup($parser);

	my $type = $me->getScopeType();
	$parser->parseError("Invalid value for \"type\": $type")
		unless ($TYPES{$type});

	my $scope = $me->getScope();
	$parser->parseError("Invalid character in \"scope\": $scope")
		if (is_local_scope($scope));
}

1;
