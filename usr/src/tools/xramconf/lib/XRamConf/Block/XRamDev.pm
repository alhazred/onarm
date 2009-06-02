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
## Class for "xramdev" block in xramfs device configuration file.
##

package XRamConf::Block::XRamDev;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune;
use UtsTune::Block;
use XRamConf::Constants;
use XRamConf::Digest;

@ISA = qw(UtsTune::Block);

# Valid parameter names
%PARAMETER = (
	size		=> {type => 'UtsTune::Type::UnsignedInteger64'},
	prot		=> {type => 'XRamConf::Type::Prot'},
	cache		=> {type => 'XRamConf::Type::Cache'},
	rootfs		=> {type => 'UtsTune::Type::Bool'},
	sysdump		=> {type => 'UtsTune::Type::Bool'},
	image		=> {type => 'UtsTune::Type::String'},
	digest		=> {type => 'UtsTune::Type::Symbol'},
);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my $name = shift;
	my $param = shift || \%PARAMETER;

	my $me = $class->SUPER::new(undef, $param);
	$me->{NAME} = $name;

	return bless $me, $class;
}

sub getName
{
	my $me = shift;

	return $me->{NAME};
}

sub fixup
{
	my $me = shift;
	my ($parser) = @_;

	my $size = $me->getParameter('size');
	$parser->parseError("\"size\" is not defined.")
		unless (defined($size));
	$parser->parseError("\"size\" must be greater than zero.")
		unless ($size);

	my $algo = $me->getParameter('digest');
	$parser->parseError("Unsupported digest algorithm: \"$algo\"")
		if ($algo and !XRamConf::Digest->isSupported($algo));

	my $image = $me->getImageFile();
	if ($image) {
		$parser->parseError("Too long image filename: \"$image\"")
			if (length($image) >= XRAMDEV_MAX_FILE_NAMELEN);
		if ($image =~ /([^\w-.])/) {
			my $bad = $1;
			$parser->parseError("Bad character in image filename:",
					    " '$bad'");
		}
	}

	if ($me->getBool('rootfs')) {
		my $root = $parser->rootFs();
		if ($root) {
			my $rname = $root->getName();
			my $name = $me->getName();
			$parser->parseError("Duplicated root filesystem: ",
					    "$name, $rname");
		}
		$parser->rootFs($me);
	}
}

sub getDigestInstance
{
	my $me = shift;

	my $algo = $me->getParameter('digest');
	return undef unless ($algo);
	return XRamConf::Digest->getInstance($algo);
}

sub checkValueImpl
{
}

sub checkValue
{
	my $me = shift;
	my ($value, $objdir) = @_;

	my $type = $me->getType();

	eval {
		$me->checkValueImpl($value, $type, $objdir, "Value");
	};
	if ($@) {
		my $err = "$@";

		chomp($err);
		my $name = $me->getName();
		die "$name: $err\n";
	}
}

sub getImageFile
{
	my $me = shift;

	my $value = $me->getParameter('image');
	return $me->evalString($value);
}

sub declare
{
	my $me = shift;
	my ($parser) = @_;

	my $stype = $me->getParamType('size');
	my $name = $me->getName();
	my $size = $me->getParameter('size') . $stype->getSuffix();
	my $attr = $parser->getMapAttr($me);
	my $flags = 0;

	my @flg;
	push(@flg, 'XDSEG_ROOTFS') if ($me->getBool('rootfs'));
	push(@flg, 'XDSEG_SYSDUMP') if ($me->getBool('sysdump'));
	$flags = join('|', @flg) if (@flg);

	my $ret = <<OUT;
	{
		/* base PFN (updated later) */
		0,

		/* page count */
		(pgcnt_t)mmu_btopr($size),

		/* HAT attributes */
		$attr,

		/* Lower PFN index (updated later) */
		NULL,

		/* Higher PFN index (updated later) */
		NULL,

		/* flags */
		$flags,

		/* node name */
		"$name",

		/* kernel virtual address (no initial value) */
		0,
OUT
	$ret .= "\t}";

	return $ret;
}

1;
