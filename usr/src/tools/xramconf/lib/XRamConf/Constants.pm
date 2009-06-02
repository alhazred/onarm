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
## Common constants and simple functions.
##

package XRamConf::Constants;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;

@ISA = qw(Exporter);

##
## Kernel symbols
##

# Timestamp of configuration file
use constant	SYM_XRAMDEV_CONFTIME	=> 'xramdev_conftime';

# Array of xramfs device configuration
use constant	SYM_XRAMDEV_SEGS	=> 'xramdev_segs';

# Number of XRAMDEV_SEGS elements
use constant	SYM_XRAMDEV_SEGCNT	=> 'xramdev_segcnt';

# Common symbols
use constant	SYM_PAGESIZE		=> '_pagesize';
use constant	SYM_PAGESHIFT		=> '_pageshift';
use constant	SYM_PAGEOFFSET		=> '_pageoffset';
use constant	SYM_PAGEMASK		=> '_pagemask';
use constant	SYM_KERNELBASE		=> '_kernelbase';

##
## Misc.
##

# Max node name length.
use constant	XRAMDEV_MAX_NAMELEN		=> 32;

# Max image filename length.
use constant	XRAMDEV_MAX_FILE_NAMELEN	=> 32;

# Driver name of xramdev.
use constant	XRAMDEV_DRVNAME			=> 'xramdev';

# Suffix for character special file.
use constant	XRAMDEV_CHR_SUFFIX		=> ',raw';

# xramdev device directory.
use constant	XRAMDEV_DEVDIR			=> '/dev/xramdev';

# devfs mount point.
use constant	DEVFS_PATH			=> '/devices';

# Max size of warm boot information.
use constant	MAX_WBINFO_SIZE			=> 1024;

# Max privilege name length.
use constant	PRIVNAME_MAX			=> 32;

# Max number of xramdev devices.
use constant	XRAMDEV_MAXDEVS			=> 256;

# Max number of userdata devices.
use constant	USERDATA_MAXDEVS		=> 256;

# Round up.
sub ROUNDUP($$)
{
	my ($x, $y) = @_;

	use integer;

	return (($x + $y - 1) / $y) * $y
}

# Determine whether the value is power of 2.
sub ISP2($)
{
	my ($x) = @_;

	return (($x & ($x - 1)) == 0) ? 1 : 0;
}

@EXPORT = qw(SYM_XRAMDEV_CONFTIME SYM_XRAMDEV_SEGS SYM_XRAMDEV_SEGCNT
	     SYM_PAGESIZE SYM_PAGESHIFT SYM_PAGEOFFSET SYM_PAGEMASK
	     SYM_KERNELBASE XRAMDEV_MAX_NAMELEN XRAMDEV_MAX_FILE_NAMELEN
	     XRAMDEV_DRVNAME XRAMDEV_CHR_SUFFIX  XRAMDEV_DEVDIR DEVFS_PATH
	     MAX_WBINFO_SIZE PRIVNAME_MAX XRAMDEV_MAXDEVS USERDATA_MAXDEVS
	     ROUNDUP ISP2);

1;
