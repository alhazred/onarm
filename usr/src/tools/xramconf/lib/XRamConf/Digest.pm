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
## Base class for message digest generator.
##

package XRamConf::Digest;

use strict;
use vars qw(%ALGORITHM);

%ALGORITHM = ('crc32'	=> 'XRamConf::Digest::CRC32',
	      'cksum'	=> 'XRamConf::Digest::CheckSum');

# Create digest generator. (static)
sub getInstance
{
	my ($cl, $algo) = @_;

	my $class = $ALGORITHM{$algo};
	return undef unless ($class);

	my $load = "require $class; import $class;";
	eval $load;
	if ($@) {
		my $err = "$@";

		chomp($err);
		die "Can't find digest generator for \"$algo\": :\n$err\n";
	}

	return $class->new();
}

# Determine whether the specified algorithm is supporeted. (static)
sub isSupported
{
	my ($cl, $algo) = @_;

	return $ALGORITHM{$algo};
}

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($size) = @_;

	my $me = bless {SIZE => $size}, $class;
	return $me;
}

sub getDigestSize
{
	my $me = shift;

	return $me->{SIZE};
}

sub add;
sub digest;
sub hexdigest;
sub getDigestSize;
sub getDigestType;

1;
