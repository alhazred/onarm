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
## Template entry for a struct member.
##

package XRamConf::Struct::Member;

use strict;
use vars qw(%SWAPFUNC %TEMPLATES $UINT8 $UINT16 $UINT32 $ALIGN64);

use XRamConf::ByteOrder;
use XRamConf::Constants;

%SWAPFUNC = (2 => \&bswap_16, 4 => \&bswap_32, 8 => \&bswap_64);
%TEMPLATES = ('1' => 'c', '2' => 's', '4' => 'l', '8' => 'q',
	      '1U'=> 'C', '2U' => 'S', '4U' => 'L', '8U' => 'Q');

if ($ENV{MACH} eq 'arm') {
	$ALIGN64 = ($ENV{GNUC_ARM_EABI} eq '#') ? 4 : 8;
}
else {
	$ALIGN64 = 8;
}

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%args) = @_;

	my $size = $args{size};
	my $swapfunc = $SWAPFUNC{$size};
	die "Unsupported size of struct member: $size\n"
		unless ($size == 1 or $swapfunc);
	my $unsigned = $args{unsigned};
	my $key = ($unsigned) ? $size . 'U' : $size;
	my $template = $TEMPLATES{$key};
	my $nelems = $args{elements};
	$template .= $nelems if ($nelems > 1);
	my $align = $args{alignment};
	unless ($align) {
		$align = ($size == 8) ? $ALIGN64 : $size;
	}
	die "Bad alignment: $align\n" unless (ISP2($align));

	my $me = bless {SIZE => $size, UNSIGNED => $unsigned,
			SWAPFUNC => $swapfunc, NELEMS => $nelems,
			ALIGN => $align, TEMPLATE => $template}, $class;

	return $me;
}

sub getSize
{
	my $me = shift;

	my $size = $me->{SIZE};
	my $nelems = $me->{NELEMS};
	$size *= $nelems if ($nelems > 1);

	return $size;
}

sub getAlignment
{
	my $me = shift;

	return $me->{ALIGN};
}

sub getDefaultValue
{
	my $me = shift;

	my $nelems = $me->{NELEMS};
	if ($nelems >= 1) {
		my @ret;
		for (my $i = 0; $i < $nelems; $i++) {
			push(@ret, 0);
		}
		return (wantarray) ? @ret : \@ret;
	}

	return 0;
}

sub encode
{
	my $me = shift;
	my ($order, @values) = @_;

	my $template = $me->{TEMPLATE};
	my $swapfunc = $me->{SWAPFUNC};
	my $data;
	if (!$swapfunc or $order == get_host_byteorder()) {
		$data = \@values;
	}
	else {
		$data = [];
		foreach my $v (@values) {
			push(@$data, &$swapfunc($v));
		}
	}

	return pack($template, @$data);
}

sub decode
{
	my $me = shift;
	my ($order, $raw) = @_;

	my $template = $me->{TEMPLATE};
	my $swapfunc = $me->{SWAPFUNC};

	my (@data) = unpack($template, $raw);
	my $ret;
	if (!$swapfunc or $order == get_host_byteorder()) {
		$ret = \@data;
	}
	else {
		$ret = [];
		foreach my $v (@data) {
			push(@$ret, &$swapfunc($v));
		}
	}

	my $nelems = $me->{NELEMS};
	return $ret->[0] if (!$nelems or $me->{NOARRAY});

	return (wantarray) ? @$ret : $ret;
}

# Default types.
$UINT8 = XRamConf::Struct::Member->new(size => 1, unsigned => 1);
$UINT16 = XRamConf::Struct::Member->new(size => 2, unsigned => 1);
$UINT32 = XRamConf::Struct::Member->new(size => 4, unsigned => 1);

1;
