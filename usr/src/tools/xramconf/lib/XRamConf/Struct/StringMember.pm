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
## Template entry for a struct member whose type is string. (character array)
##
package XRamConf::Struct::StringMember;

use strict;
use vars qw(@ISA);

use XRamConf::Constants;
use XRamConf::Struct::Member;

@ISA = qw(XRamConf::Struct::Member);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($nelems) = @_;

	die "Number of elements is not defined.\n" unless ($nelems);

	my $me = $class->SUPER::new(size => 1, alignment => 1,
				    elements => $nelems);
	$me = bless $me, $class;

	my $template = 'Z' . $nelems;
	$me->{TEMPLATE} = $template;
	$me->{NOARRAY} = 1;

	return $me;
}

1;
