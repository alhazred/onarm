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
### Static module database.
###

package UtsTune::DB::Static;

use strict;
use vars qw(@ISA $SEP $KMODS_IN_USR);

use FileHandle;
use POSIX;

use UtsTune;
use UtsTune::DB;

@ISA = qw(UtsTune::DB);

use constant	DB_FNAME	=> '.static';
use constant	STAMP_SUFFIX	=> '_stamp';

$SEP = ':';
$KMODS_IN_USR = 1 unless (exists($ENV{KMODS_INST_USR}) and
			  $ENV{KMODS_INST_USR} eq '#');

# Static methods.
sub exists
{
	my ($plat, $arch, $objdir) = @_;

	my $stamp = dbfile($plat, $arch, $objdir) . STAMP_SUFFIX;
	return (-f $stamp);
}

sub dbfile
{
	my ($plat, $arch, $objdir) = @_;

	my $dbfile = $UtsTune::UTS . '/' . $arch . '/' . DB_FNAME;
	$dbfile .= '_' . $plat unless ($arch eq $plat);
	$dbfile .= '_' . $objdir;

	return $dbfile;
}

# Instance methods.
sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($plat, $arch, $objdir, $write) = @_;

	check_static_unix();

	my $dbfile = dbfile($plat, $arch, $objdir);
	my $me = $class->SUPER::new($dbfile, $write);

	my $stamp = $dbfile . STAMP_SUFFIX;
	$me->{STAMP} = $stamp;
	$me->{STATIC_ARCH} = $arch;
	$me->{NEW_VALUE} = {};
	$me->{KMODS_USR} = {};

	unless (-f $stamp) {
		# Create timestamp file.
		my $fh = FileHandle->new($stamp, O_CREAT|O_WRONLY|O_TRUNC,
					 0644)
			or die "open($stamp) failed: $!\n";
	}

	return bless $me, $class;
}

sub updateStamp
{
	my $me = shift;

	my $stamp = $me->{STAMP};
	my $t = time();
	utime($t, $t, $stamp) or
		die "Failed to update timestamp($stamp): $!\n";
}

sub put
{
	my $me = shift;
	my ($key, $value) = @_;

	$value = 0 unless ($value);

	# Do NOT append value into DB.
	# It will be done by sync().
	$me->{NEW_VALUE}->{$key} = $value;

	delete($me->{KMODS_USR}->{$key});
}

sub putUsrMod
{
	my $me = shift;
	my ($key, $value) = @_;

	my $newmap = $me->{NEW_VALUE};
	unless (exists($newmap->{$key})) {
		$me->put($key, $value);
		$me->{KMODS_USR}->{$key} = 1;
	}
}

sub isStatic
{
	my $me = shift;
	my ($key) = @_;

	my $value = $me->get($key);
	return undef unless (defined($value));
	my ($flag, $usr) = split(/$SEP/, $value, 2);
	return undef if ($usr and !$KMODS_IN_USR);

	return ($flag) ? $flag : 1;
}

sub clobber
{
	my $me = shift;

	unlink($me->{STAMP});
	$me->SUPER::clobber(@_);
}

# Sync changes into DB.
sub sync
{
	my $me = shift;

	my $h = $me->open();
	my $nh = $me->{NEW_VALUE};
	my $kmods = $me->{KMODS_USR};
	my (@orgkey) = keys(%$h);
	my (@newkey) = keys(%$nh);

	my $changed;
	if (@orgkey == @newkey) {
		(@orgkey) = sort(@orgkey);
		(@newkey) = sort(@newkey);
		for (my $i = 0; $i < scalar(@orgkey); $i++) {
			my $okey = $orgkey[$i];
			my $nkey = $newkey[$i];
			unless ($okey eq $nkey) {
				$changed = 1;
				last;
			}
			my $oval = $h->{$okey};
			my ($ov, $usr) = split(/$SEP/, $oval, 2);
			my $nval = $nh->{$nkey};
			unless ($ov eq $nval) {
				$changed = 1;
				last;
			}
			my $ou = (defined($usr)) ? 1 : 0;
			my $nu = (defined($kmods->{$nkey})) ? 1 : 0;
			unless ($ou == $nu) {
				$changed = 1;
				last;
			}
		}
	}
	else {
		$changed = 1;
	}

	if ($changed) {
		undef %$h;
		foreach my $nkey (@newkey) {
			my $nv = $nh->{$nkey};
			$nv .= $SEP . '1' if ($kmods->{$nkey});
			$h->{$nkey} = $nv;
		}
		$me->updateStamp();
	}
}

sub getAll
{
	my $me = shift;

	my $h = $me->open();
	my (%ret);

	while (my ($key, $value) = each(%$h)) {
		my ($flag, $usr) = split(/$SEP/, $value, 2);
		$ret{$key} = $flag unless ($usr and !$KMODS_IN_USR);
	}

	return (wantarray) ? %ret : \%ret;
}

sub getAllDirectories
{
	my $me = shift;

	my $h = $me->open();
	my @dirs;

	while (my ($key, $value) = each(%$h)) {
		my ($flag, $usr) = split(/$SEP/, $value, 2);
		push(@dirs, $key) unless ($usr and !$KMODS_IN_USR);
	}
	return (wantarray) ? @dirs : \@dirs;
}

sub getAllFlags
{
	my $me = shift;

	my $h = $me->open();
	my @flags;
	while (my ($key, $value) = each(%$h)) {
		my ($flag, $usr) = split(/$SEP/, $value, 2);
		next if ($usr and !$KMODS_IN_USR);
		push(@flags, $flag) if ($flag);
	}

	return (wantarray) ? @flags : \@flags;
}

1;
