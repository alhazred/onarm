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
## Class for static module configuration script definition in tune file.
##

package	UtsTune::Block::Static;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune;
use UtsTune::Block;
use UtsTune::Type::String;

@ISA = qw(UtsTune::Block);

# Valid parameters.
%PARAMETER = (file		=> {type => 'UtsTune::Type::String',
				    mandatory => 1},
	      architectures	=> {type => 'UtsTune::Type::String',
				    list => 1, mandatory => 1});

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	check_static_unix();

	my $me = $class->SUPER::new(UtsTune::Type::String->new(), \%PARAMETER);

	return bless $me, $class;
}

sub getFile
{
	my $me = shift;

	return $me->getStringParameter('file');
}

sub getArchitectures
{
	my $me = shift;

	my $value = $me->getParameter('architectures');
	my @arch;
	if (defined($value)) {
		foreach my $a (@$value) {
			push(@arch, $me->evalString($a));
		}
	}

	return \@arch;
}

sub getBlockDecl
{
	return 'static';
}

1;
