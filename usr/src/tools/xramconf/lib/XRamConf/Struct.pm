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
## Base abstract class for struct data for C language.
##
## Remarks:
##   - Currently, nested struct is not supported.
##

package XRamConf::Struct;

use strict;
use XRamConf::Constants;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%args) = @_;

	# %args must contains the following key:
	#
	#    NAME:      name of this struct
	#    MEMBERS:   reference to the list of struct member name
	#    TEMPLATES: hash reference that contains member name and
	#               XRamConf::Struct::Member instance pair.
	$args{VALUES} = {} unless (defined($args{VALUES}));
	my $me = bless \%args, $class;

	return $me;
}

sub getName
{
	my $me = shift;

	return $me->{NAME};
}

sub get
{
	my $me = shift;
	my ($name, $nodefault) = @_;

	my $ret = $me->{VALUES}->{$name};
	unless (defined($ret) or $nodefault) {
		my $tmpl = $me->{TEMPLATES}->{$name};
		die "Unknown struct member: $name\n" unless ($tmpl);
		$ret = $tmpl->getDefaultValue();
	}

	return $ret;
}

sub set
{
	my $me = shift;
	my ($name, $value) = @_;

	$me->{VALUES}->{$name} = $value;
}

sub getSize
{
	my $me = shift;

	my $size = $me->{SIZE};
	unless ($size) {
		my $off = 0;
		foreach my $member (@{$me->{MEMBERS}}) {
			my $tmpl = $me->{TEMPLATES}->{$member};
			my $al = $tmpl->getAlignment();
			my $mod = $off & ($al - 1);
			if ($mod != 0) {
				my $pad = $al - $mod;
				$off += $pad;
			}
			$off += $tmpl->getSize();
		}
		my $align = $me->getAlignment();
		$size = ROUNDUP($off, $align);

		$me->{SIZE} = $size;
	}

	return $size;
}

sub getAlignment
{
	my $me = shift;

	my $align = $me->{ALIGN};
	unless ($align) {
		$align = 0;
		foreach my $tmpl (values(%{$me->{TEMPLATES}})) {
			my $al = $tmpl->getAlignment();
			$align = $al if ($al > $align);
		}
		$me->{ALIGN} = $align;
	}

	return $align;
}

sub encode
{
	my $me = shift;
	my ($order) = @_;

	my $data;
	my $off = 0;
	my $size = $me->getSize();
	foreach my $member (@{$me->{MEMBERS}}) {
		my $tmpl = $me->{TEMPLATES}->{$member};
		my $align = $tmpl->getAlignment();
		my $mod = $off & ($align - 1);
		if ($mod != 0) {
			my $pad = $align - $mod;
			$data .= pack('C' . $pad, 0);
			$off += $pad;
		}
		my $value = $me->get($member);
		$value = [$value] unless (ref($value) eq 'ARRAY');

		$data .= $tmpl->encode($order, @$value);
		$off += $tmpl->getSize();
	}

	my $pad = $size - $off;
	$data .= pack('C' . $pad, 0) if ($pad);

	return $data;
}

sub decode
{
	my $me = shift;
	my ($order, $raw) = @_;

	my $off = 0;
	my $len = length($raw);
	foreach my $member (@{$me->{MEMBERS}}) {
		my $tmpl = $me->{TEMPLATES}->{$member};
		my $al = $tmpl->getAlignment();
		my $mod = $off & ($al - 1);
		if ($mod != 0) {
			my $pad = $al - $mod;
			$off += $pad;
		}
		my $size = $tmpl->getSize();
		if ($off + $size > $len) {
			my $name = $me->getName();
			die "Lack of struct $name data: length = $len\n";
		}
		my $bin = substr($raw, $off, $size);
		my $value = $tmpl->decode($order, $bin);
		$me->set($member, $value);
		$off += $size;
	}
}

1;
