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
## ARM specific configuration parser.
##

package XRamConf::Parser::arm;

use strict;
use vars qw(@ISA @ARM_HEADERS);

use File::Basename;
use File::Path;
use POSIX qw(:errno_h);

use XRamConf::Parser;
use XRamConf::Constants;
use XRamConf::Constants::arm;

@ISA = qw(XRamConf::Parser);
@ARM_HEADERS = qw(sys/xramdev_impl.h sys/nvdram_impl.h sys/param.h vm/hat.h
		  vm/hat_arm.h);

use constant	XRAMDEV_BUS_TYPE	=> 0;
use constant	NVDRAM_BUS_TYPE		=> 0;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	my $me = $class->SUPER::new(@_);
	$me = bless $me, $class;
	$me->parse();

	return $me;
}

sub getMapAttr
{
	my $me = shift;
	my ($dev, $userdata) = @_;

	my (@list) = ('PROT_READ');
	my $prot = $dev->getParameter('prot') || 'readonly';
	push(@list, 'PROT_WRITE') if ($prot eq 'writable');

	my $cache = $dev->getParameter('cache');
	if (defined($cache)) {
		if ($cache eq 'off') {
			push(@list, 'HAT_PLAT_NOCACHE');
		}
		else {
			push(@list, 'HAT_STORECACHING_OK');
			push(@list, 'HAT_PLAT_NOEXTCACHE')
				if ($cache eq 'l1only');
		}
	}
	else {
		push(@list, 'HAT_STORECACHING_OK');

		# Default cache attribute for user data is L1 only.
		push(@list, 'HAT_PLAT_NOEXTCACHE') if ($userdata);
	}

	return join('|', @list);
}

sub getHeaders
{
	return (wantarray) ? @ARM_HEADERS : \@ARM_HEADERS;
}

sub getDevicePath
{
	my $me = shift;
	my ($index) = @_;

	my $dev = $me->getDevice($index) or return undef;

	# Currently, ARM kernel creates xramdev node under root.
	my $name = $dev->getName();
	my $entity = sprintf("%s/%s@%x,%x:%s", DEVFS_PATH, XRAMDEV_DRVNAME,
			     XRAMDEV_BUS_TYPE, $index, $name);
	my $link = sprintf("%s/%s", XRAMDEV_DEVDIR, $name);

	my $e = $entity;
	my @comp;
	while ($e ne '/') {
		push(@comp, '..');
		$e = dirname($e);
	}

	my (@list);
	$entity = join('/', @comp) . $entity;
	push(@list,
	     {entity => $entity, link => $link},
	     {entity => $entity . XRAMDEV_CHR_SUFFIX,
	      link => $link . XRAMDEV_CHR_SUFFIX});

	return (wantarray) ? @list : \@list;
}

sub getUserDataPath
{
	my $me = shift;
	my ($index) = @_;

	my $ud = $me->getUserData($index) or return undef;

	# Currently, ARM kernel creates nvdram node under root.
	my $name = $ud->getName();
	my $entity = sprintf("%s/%s@%x,%x:%s", DEVFS_PATH, NVDRAM_DRVNAME,
			     NVDRAM_BUS_TYPE, $index, $name);
	my $link = sprintf("%s/%s", NVDRAM_DEVDIR, $name);

	my $e = $entity;
	my @comp;
	while ($e ne '/') {
		push(@comp, '..');
		$e = dirname($e);
	}

	my (@list);
	$entity = join('/', @comp) . $entity;
	push(@list, {entity => $entity, link => $link});

	return (wantarray) ? @list : \@list;
}

sub outputImpl
{
	my $me = shift;
	my ($fh) = @_;

	my ($nvsegsym, $nvcntsym) = (SYM_NVDRAM_SEGS, SYM_NVDRAM_SEGCNT);

	# Declare user data device using nvdram.
	$fh->print(<<OUT);
/* User data device segments. */
nvdseg_t	$nvsegsym\[\] = {
OUT

	my (@userdata) = map {$_->declare($me)} (@{$me->{USERDATA}});
	$fh->print(join(",\n", @userdata));
	$fh->print(<<OUT);

};

/* Number of NVDRAM devices. */
const uint_t	$nvcntsym = sizeof($nvsegsym) / sizeof(nvdseg_t);
OUT
}

sub devlinkImpl
{
	my $me = shift;
	my ($root) = @_;

	# At first, cleanup nvdram links.
	my $devdir = $root . NVDRAM_DEVDIR;
	die "Failed to remove directory: $devdir: $!\n"
		if (!rmtree($devdir) && $! != ENOENT);

	# Create nvdram directory.
	mkdir($devdir) or die "mkdir($devdir) failed: $!\n";

	# Create nvdram device links.
	my $count = $me->getUserDataCount();
	for (my $index = 0; $index < $count; $index++) {
		my $list = $me->getUserDataPath($index);
		foreach my $l (@$list) {
			my $entity = $l->{entity};
			my $link = $l->{link};
			my $ln = $root . $link;
			symlink($entity, $ln) or
				die "symlink($entity, $ln) failed: $!\n";
		}
	}
}

1;
