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

###
### Global definitions for utstune.
###

package UtsTune;

use strict;
use vars qw(@ISA @EXPORT $SRC $UTS $UTSSTAT $MACH $BUILD64 $INIT_STAMP);

use Exporter;
use Cwd qw(abs_path);
use File::Basename;
use File::stat;

@ISA = qw(Exporter);

# __CONCAT__(a, b) is defined in sys/int_const.h.
use constant	CONCAT	=> '__CONCAT__';

# Class names associated with option types.
use constant	TYPE_CLASS	=> {
	'string'	=> 'UtsTune::Type::String',
	'boolean'	=> 'UtsTune::Type::Bool',
	'int'		=> 'UtsTune::Type::Integer',
	'uint'		=> 'UtsTune::Type::UnsignedInteger',
	'long'		=> 'UtsTune::Type::Long',
	'ulong'		=> 'UtsTune::Type::UnsignedLong',
	'int64'		=> 'UtsTune::Type::Integer64',
	'uint64'	=> 'UtsTune::Type::UnsignedInteger64',
};

use constant	TUNE_EFILE	=> 'modtune.e';

# Kernel object directory names.
# The value is true if the key is 64-bit build directory.
use constant	OBJS_DIRS	 => {
	debug32 => 0, debug64 => 1, obj32 => 0, obj64 => 1
};

# Kernel build type.
# The value is true if the key is release build type.
use constant OBJS_TYPES	=> {
	debug32 => 0, debug64 => 0, obj32 => 1, obj64 => 1
};

##
## Initialize build environment.
##
{
	$SRC = $ENV{SRC};
	die "ERROR: \$SRC is not defined.\n" unless ($SRC);

	$SRC = abs_path($SRC);
	$UTS = "$SRC/uts";
	$UTSSTAT = stat($UTS);
	die "ERROR: Kernel source tree is not found: $UTS\n" unless ($UTSSTAT);
	die "ERROR: $SRC/uts is not directory.\n" unless (-d _);

	$MACH = $ENV{MACH};
	die "ERROR: \$MACH is not defined.\n" unless ($MACH);

	$BUILD64 = 1 unless ($ENV{BUILD64} eq '#');
	$INIT_STAMP = $UTS . '/.utstune_init_' . $MACH;
}

# Determine whether the target architecture supports static-linked kernel.
sub is_static_unix()
{
	return ($MACH eq 'arm');
}

# Die if the target architecture doesn't support static linked kernel.
sub check_static_unix()
{
	die "$MACH kernel does not support static-linked kernel.\n"
		unless (is_static_unix());
}

# Get name of kernel object directory.
sub get_objdir($)
{
	my ($env64) = @_;

	my $objdir = (defined($ENV{RELEASE_BUILD})) ? 'obj' : 'debug';
	if ($env64) {
		die "This build environment doesn't support 64-bit kernel.\n"
			unless ($BUILD64);
		$objdir .= '64';
	}
	else {
		$objdir .= '32';
	}

	return $objdir;
}

# Return object directories that will be built on this environment.
sub get_build_objdirs()
{
	my $obj = (defined($ENV{RELEASE_BUILD})) ? 'obj' : 'debug';

	my (@dirs);
	foreach my $d (keys %{OBJS_DIRS()}) {
		next if (!$BUILD64 and OBJS_DIRS->{$d});
		push(@dirs, $d) if ($d =~ /^$obj/);
	}

	return (wantarray) ? @dirs : \@dirs;
}

# Return object directories that is supported on this build tree.
sub get_all_objdirs()
{
	my (@dirs);

	foreach my $d (keys %{OBJS_DIRS()}) {
		push(@dirs, $d) unless (!$BUILD64 and OBJS_DIRS->{$d});
	}

	return (wantarray) ? @dirs : \@dirs;
}

# Return $SRC/uts relative path.
# Specified path must be absolute path, and be already canonicalized.
sub utspath($)
{
	my ($path) = @_;

	$path =~ s,^$UTS/*,,;
	return $path;
}

# Return $SRC relative path.
# Specified path must be absolute path, and be already canonicalized.
sub srcpath($)
{
	my ($path) = @_;

	$path =~ s,^$SRC/*,,;
	return $path;
}

# Return path to static module DB.
sub staticdb_path($$)
{
	my ($arch, $plat) = @_;

	my $fname = '.static';
	$fname .= '_' . $plat unless ($arch eq $plat);
	return $UTS . '/' . $fname;
}

# Check whether the specified path is located in uts source tree.
sub check_utstree($)
{
	my ($path) = @_;

	my $dir = dirname($path);
	do {
		my $sb = stat($dir);
		die "stat($path) failed: $!\n" unless ($sb);
		if ($sb->dev() == $UTSSTAT->dev() and
		    $sb->ino() == $UTSSTAT->ino()) {
			return 1;
		}
		$dir = dirname($dir);
	} while ($dir and $dir ne '/');

	return undef;
}

# Determine whether the specified scope is local scope.
sub is_local_scope($)
{
	my ($scope) = @_;
	
	return $scope =~ m,/,;
}

# Derive local scope name from file path.
sub local_scope($)
{
	my ($file) = @_;

	return dirname(utspath($file));
}

sub modtune_efile($$)
{
	my ($path, $objdir) = @_;

	my $dir = dirname($path) . '/' . $objdir . '/' . TUNE_EFILE;
}

sub default_platform($$)
{
	my ($db, $usefirst) = @_;

	if ($MACH eq 'arm' and defined($ENV{ARM_PLATFORM})) {
		return $ENV{ARM_PLATFORM};
	}

	return undef unless ($usefirst);

	my $pl = $db->getPlatforms();
	return ($pl) ? $pl->[0] : undef;
}

# Return true if the given build types are same.
sub is_same_build_type($$)
{
	my ($d1, $d2) = @_;

	return (OBJS_TYPES->{$d1} == OBJS_TYPES->{$d2});
}

@EXPORT = qw(CONCAT TYPE_CLASS TUNE_EFILE OBJS_DIRS OBJS_TYPES is_static_unix
	     check_static_unix get_objdir get_build_objdirs get_all_objdirs
	     srcpath utspath check_utstree staticdb_path is_local_scope
	     local_scope modtune_efile default_platform is_same_build_type);

1;
