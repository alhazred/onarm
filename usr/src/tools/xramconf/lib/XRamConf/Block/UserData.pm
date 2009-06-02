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
## Class for "userdata" block in xramfs device configuration file.
##

package XRamConf::Block::UserData;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune;
use UtsTune::Block;
use UtsTune::Type::Integer;
use XRamConf::Constants;

use constant	DEFAULT_PRIVMODE	=> '0666';
use constant	DEFAULT_MODE		=> 0600;
use constant	DEFAULT_UID		=> 0;
use constant	DEFAULT_GID		=> 3;

@ISA = qw(XRamConf::Block::XRamDev);

# Valid parameter names
%PARAMETER = (
	size		=> {type => 'UtsTune::Type::UnsignedInteger64'},
	mode		=> {type => 'XRamConf::Type::Octal'},
	cache		=> {type => 'XRamConf::Type::Cache'},
	owner		=> {type => 'XRamConf::Type::Raw'},
	group		=> {type => 'XRamConf::Type::Raw'},
	read_priv	=> {type => 'UtsTune::Type::String'},
	write_priv	=> {type => 'UtsTune::Type::String'},
	priv_only	=> {type => 'UtsTune::Type::Bool'},
	sysdump		=> {type => 'UtsTune::Type::Bool'},
);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my $name = shift;

	my $me = $class->SUPER::new($name, \%PARAMETER);

	return bless $me, $class;
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

	$me->checkId($parser, 'owner', 'user', $parser->getPasswd()); 
	$me->checkId($parser, 'group', 'group', $parser->getGroup()); 

	my $mode = $me->getParameter('mode');
	if (defined($mode)) {
		$parser->parseError("\"mode\" must be greater than zero.")
			unless ($mode);
		$parser->parseError("Invalid file access mode bits.")
			if ($mode & ~0777);
	}

	$me->checkPriv($parser, 'read_priv', 'read privilege');
	$me->checkPriv($parser, 'write_priv', 'write privilege');
}

sub checkId
{
	my $me = shift;
	my ($parser, $key, $label, $idmap) = @_;

	my $value = $me->getParameter($key);
	return unless (defined($value));
	$parser->parseError("\"$key\" requires $label as value")
		unless (length($value) > 0);
	my $id = $idmap->getId($value);
	unless (defined($id)) {
		$parser->parseError("Unknown $label: $value")
			unless ($value =~ /^\d+$/);
		$id = $value + 0;
		$parser->parseError("Too large $label ID: $value")
			if ($id > UtsTune::Type::Integer::INT_MAX);
	}
	$me->setParameter($key, $id);
}

sub checkPriv
{
	my $me = shift;
	my ($parser, $key, $label) = @_;

	my $spriv = $me->getParameter($key);
	if (defined($spriv)) {
		my $priv = $parser->evalString($spriv);
		if (!$priv or $priv =~ /^none$/i) {
			$me->setParameter($key, undef);
		}
		else {
			$parser->checkSymbol($priv, $label, PRIVNAME_MAX);
		}
	}
}

sub declarePriv
{
	my $me = shift;
	my ($key) = @_;

	my $spriv = $me->getParameter($key);
	return ($spriv) ? lc($spriv) : 'NULL';
}

#
# Declare memory segments for user data.
# Currently, user data is implemented by nvdram driver.
#
sub declare
{
	my $me = shift;
	my ($parser) = @_;

	my $stype = $me->getParamType('size');
	my $name = $me->getName();
	my $size = $me->getParameter('size') . $stype->getSuffix();

	# userdata device itself must be always writable.
	$me->setParameter('prot', 'writable');

	my $attr = $parser->getMapAttr($me, 1);

	my $flags = 0;

	my (@flg, $smode, $uid, $gid);
	push(@flg, 'NVDSEG_SYSDUMP') if ($me->getBool('sysdump'));
	if ($me->getBool('priv_only')) {
		push(@flg, 'NVDSEG_PRIVONLY');

		# Use default file permission.
		$smode = DEFAULT_PRIVMODE;
		$uid = DEFAULT_UID;
		$gid = DEFAULT_GID
	}
	else {
		my $mode = $me->getParameter('mode') || DEFAULT_MODE;
		$smode = sprintf("0%o", $mode);
		$uid = $me->getParameter('owner');
		$uid = DEFAULT_UID unless (defined($uid));
		$gid = $me->getParameter('group');
		$gid = DEFAULT_GID unless (defined($gid));
	}
	$flags = join('|', @flg) if (@flg);

	my $rpriv = $me->declarePriv('read_priv');
	my $wpriv = $me->declarePriv('write_priv');

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

		/* file access mode */
		$smode,

		/* flags */
		$flags,

		/* owner */
		$uid,

		/* group */
		$gid,

		/* node name */
		"$name",

		/* privilege for read */
		$rpriv,

		/* privilege for write */
		$wpriv
OUT
	$ret .= "\t}";

	return $ret;
}

1;
