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
# Copyright (c) 2008-2009 NEC Corporation
# All rights reserved.
#

##
## Type for string or cpp macro.
##

package DevConf::Type::CppString;

use strict;
use vars qw(@ISA);

use DevConf::Constants;
use DevConf::Type::PropValue;

@ISA = qw(DevConf::Type::PropValue);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($value, $type) = @_;

	my $me = $class->SUPER::new($value);
	$me->{TYPE} = $type;

	return $me;
}

sub getType
{
	return 'string';
}

sub checkValueImpl
{
	my $me = shift;
	my ($value, $type, $def) = @_;

	if ($type != $me->STRING) {
		my $name = $me->getType();
		return "Invalid value for $name: $value";
	}

        return undef;
}

sub dumpValue
{
	my $me = shift;
	my $value = shift || $me->getValue();

	return $value;
}

1;
