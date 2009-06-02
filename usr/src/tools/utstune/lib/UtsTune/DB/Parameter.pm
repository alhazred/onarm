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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

###
### Parameter database access methods
###

package UtsTune::DB::Parameter;

use strict;
use vars qw(@ISA);

use File::Basename;
use File::stat;

use UtsTune;
use UtsTune::DB;

@ISA = qw(UtsTune::DB);

use constant	DB_FNAME	=> '.utstune';
use constant	KEY_ARCH	=> '_arch_';
use constant	KEY_PLATFORM	=> '_plat_';
use constant	KEY_CURFILE	=> '_cur_';
use constant	KEY_MODTUNE	=> '_modtune_';
use constant	KEY_STATIC	=> '_static_';
use constant	KEY_STATIC_ARCH	=> '_static_arch_';
use constant	KEY_SCOPE	=> '_scope_';
use constant	KEY_IMPORT	=> '_import_';
use constant	KEY_MAKEOPT	=> '_makeopt_';
use constant	KEY_INIT	=> '_init_';
use constant	KEY_PLATOBJDIR	=> '_platobjdir_';
use constant	KEY_MODINFO	=> '_modinfo_';

use constant	SEP_SPEC	=> ':';

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($write) = @_;

	my $fname = DB_FNAME . '_' . $UtsTune::MACH;
	my $dbfile = $UtsTune::UTS . '/' . $fname;
	my $me = $class->SUPER::new($dbfile, $write);

	return bless $me, $class;
}

sub specificKey
{
	my $me = shift;
	my ($dbkey, $key) = @_;

	return $dbkey . $key;
}

sub curfileKey
{
	my $me = shift;
	my ($name, $plat, $objdir) = @_;

	$name = $name . SEP_SPEC . $plat if ($plat);
	return KEY_CURFILE . $name . '_' . $objdir;
}

sub modtuneKey
{
	my $me = shift;
	my ($path, $utsrel) = @_;

	my $rel = ($utsrel) ? $path : utspath($path);
	return KEY_MODTUNE . $rel;
}

sub makeoptKey
{
	my $me = shift;
	my ($path, $utsrel) = @_;

	my $rel = ($utsrel) ? $path : utspath($path);
	return KEY_MAKEOPT . dirname($rel);
}

sub importKey
{
	my $me = shift;
	my ($path, $utsrel) = @_;

	my $rel = ($utsrel) ? $path : utspath($path);
	return KEY_IMPORT . $rel;
}

sub scopeKey
{
	my $me = shift;
	my ($arg) = @_;

	return KEY_SCOPE . $arg;
}

sub modinfoKey
{
	my $me = shift;
	my ($basekey, $index) = @_;

	return $basekey . SEP_SPEC . $index;
}

sub getSpecPath
{
	my $me = shift;
	my ($dbkey, $arg) = @_;

	my @list;
	my $h = $me->open();
	if ($arg) {
		my $key = $me->specificKey($dbkey, $arg);
		return $h->{$key};
	}
	my $v = $h->{$dbkey};
	if ($v) {
		my $sep = SEP_SPEC;
		foreach my $sp (split(/$sep/, $v)) {
			my $spkey = $me->specificKey($dbkey, $sp);
			my $path = $h->{$spkey};
			push(@list, [$sp, $path]);
		}
	}

	return (wantarray) ? @list : \@list;
}

sub addSpecific
{
	my $me = shift;
	my ($dbkey, $key, $path) = @_;

	my $pathkey = $me->specificKey($dbkey, $key);
	my $h = $me->open();
	my $oldpath = $h->{$pathkey};
	if ($oldpath) {
		die "$oldpath\n" if ($path ne $oldpath);

		# Already added.
		return;
	}

	# Append the specified key to specific list.
	my $sp = $h->{$dbkey};
	if ($sp) {
		$sp .= SEP_SPEC . $key;
	}
	else {
		$sp = $key;
	}
	$h->{$dbkey} = $sp;

	# Record definition file path.
	$h->{$pathkey} = $path;
}

sub getSpecific
{
	my $me = shift;
	my ($dbkey) = @_;

	my $h = $me->open();
	my $v = $h->{$dbkey};
	my @spec;
	if ($v) {
		my $sep = SEP_SPEC;
		(@spec) = split(/$sep/, $v);
	}

	return (wantarray) ? @spec : \@spec;
}

sub removeSpecific
{
	my $me = shift;
	my ($dbkey, $key) = @_;

	my $h = $me->open();
	my $pathkey = $me->specificKey($dbkey, $key);
	delete($h->{$pathkey});

	# Remove the specified key from specific list.
	my $sp = $h->{$dbkey};
	if ($sp) {
		my $sep = SEP_SPEC;
		my (@splist) = (grep {$_ ne $key }
				(split(/$sep/, $sp)));
		if (@splist) {
			$h->{$dbkey} = join(SEP_SPEC, @splist);
		}
		else {
			delete($h->{$dbkey});
		}
	}
}

sub addArchSpec
{
	my $me = shift;

	$me->addSpecific(KEY_ARCH, @_);
}

sub addPlatSpec
{
	my $me = shift;

	$me->addSpecific(KEY_PLATFORM, @_);
}

sub getArchPath
{
	my $me = shift;

	return $me->getSpecPath(KEY_ARCH, @_);
}

sub getPlatPath
{
	my $me = shift;

	return $me->getSpecPath(KEY_PLATFORM, @_);
}

sub getArchitectures
{
	my $me = shift;

	return $me->getSpecific(KEY_ARCH);
}

sub getPlatforms
{
	my $me = shift;

	return $me->getSpecific(KEY_PLATFORM);
}

sub isOptionKey
{
	my $me = shift;
	my ($key) = @_;

	return ($key =~ /^[A-Z]/o);
}

sub getOptionName
{
	my $me = shift;
	my ($key, $plat) = @_;

	return undef unless ($me->isOptionKey($key));

	my $sep = SEP_SPEC;
	my ($k, $pl) = split(/$sep/, $key, 2);
	return $k unless (defined($pl));
	return $k if (defined($plat) and $plat eq $pl);

	return undef;
}

sub getAllOptions
{
	my $me = shift;
	my ($plat, $dosort) = @_;

	my $h = $me->open();
	my (@names, %map);
	my $spat = SEP_SPEC;
	my $pat = SEP_SPEC . $plat;
	while (my ($key, $value) = each(%$h)) {
		next unless ($me->isOptionKey($key));
		if ($key =~ /$spat/o) {
			next unless ($key =~ s/\Q$pat\E$//);
		}
		unless ($map{$key}) {
			push(@names, $key);
			$map{$key} = 1;
		}
	}

	(@names) = (sort @names) if ($dosort);
	return (wantarray) ? @names : \@names;
}

sub getScope
{
	my $me = shift;
	my ($path) = @_;

	foreach my $pl (@{$me->getPlatforms()}) {
		my $p = $me->getPlatPath($pl);
		return $pl if ($p eq $path);
	}
	foreach my $al (@{$me->getArchitectures()}) {
		my $p = $me->getArchPath($al);
		return $al if ($p eq $path);
	}

	return local_scope($path);
}

sub addOption
{
	my $me = shift;
	my ($name, $plat, $path, $objdir, $opath) = @_;

	# Check whether this parameter name is unique.
	my $orgname = $name;
	my $h = $me->open();
	my $old = $h->{$name};
	if (defined($old)) {
		if ($old ne $path) {
			$old =~ s,^\Q$UtsTune::UTS\E/*,,;
			die "$name is already defined in $old\n";
		}
		undef $plat;
	}
	elsif ($plat) {
		my $platkey = $name . SEP_SPEC . $plat;
		$old = $h->{$platkey};
		if (defined($old)) {
			if ($old ne $path) {
				$old =~ s,^\Q$UtsTune::UTS\E/*,,;
				die "$name is already defined in $old\n";
			}
		}
		$name = $platkey;
	}

	unless (defined($old)) {
		$h->{$name} = $path;
		my $scope = $me->getScope($path);
		my $skey = $me->scopeKey($name);
		$h->{$skey} = $scope;
	}

	my $k = $me->curfileKey($orgname, $plat, $objdir);
	$h->{$k} = $opath;
}

sub removeOption
{
	my $me = shift;
	my ($name, $plat) = @_;

	my $orgname = $name;
	$name = $name . SEP_SPEC . $plat if ($plat);
	$me->remove($name);
	my $skey = $me->scopeKey($name);
	$me->remove($skey);
	foreach my $o (keys %{OBJS_DIRS()}) {
		my $k = $me->curfileKey($orgname, $plat, $o);
		$me->remove($k);
	}
}

sub getOptionScope
{
	my $me = shift;
	my ($name, $plat) = @_;

	if ($plat) {
		my $platkey = $me->scopeKey($name . SEP_SPEC . $plat);
		my $ret = $me->get($platkey);
		return $ret if ($ret);
	}

	my $key = $me->scopeKey($name);
	return $me->get($key);
}

sub addModTune
{
	my $me = shift;
	my ($path, $mtime, $scopelist) = @_;

	my $rel = utspath($path);
	my $key = $me->modtuneKey($rel, 1);
	my $h = $me->open();

	$h->{$key} = $mtime;
	if ($scopelist and @$scopelist) {
		# Keep imported scopes.
		my $ikey = $me->importKey($rel, 1);
		$h->{$ikey} = join(SEP_SPEC, @$scopelist);
	}
}

sub addMakeOpt
{
	my $me = shift;
	my ($script, $mtime, $scopelist) = @_;

	my $rel = utspath($script);
	my $key = $me->makeoptKey($rel, 1);
	my $h = $me->open();

	$h->{$key} = $script . SEP_SPEC . $mtime;
	if ($scopelist and @$scopelist) {
		# Keep imported scopes.
		my $ikey = $me->importKey($rel, 1);
		$h->{$ikey} = join(SEP_SPEC, @$scopelist);
	}
}

sub getMakeOpt
{
	my $me = shift;
	my ($file) = @_;

	my $key = $me->makeoptKey($file);
	my $h = $me->open();
	my $v = $h->{$key};
	my @list;
	if ($v) {
		my $sep = SEP_SPEC;
		(@list) = split(/$sep/, $v);
	}

	return (wantarray) ? @list : \@list;
}

sub removeMakeOpt
{
	my $me = shift;
	my ($file) = @_;

	my $key = $me->makeoptKey($file);
	my $h = $me->open();
	my $v = $h->{$key};

	my (@rmkey) = ($key);
	if ($v) {
		my $sep = SEP_SPEC;
		my ($path, $mtime) = split(/$sep/, $v);
		push(@rmkey, $me->importKey($path));
	}

	foreach my $k (@rmkey) {
		delete($h->{$k});
	}
}

sub getMakeOptMtime
{
	my $me = shift;

	my $l = $me->getMakeOpt(@_);
	return $l->[1];
}

sub getMakeOptScript
{
	my $me = shift;

	my $l = $me->getMakeOpt(@_);
	return $l->[0];
}

# Get all makeopt files that imports the specified scope.
sub getMakeOptByScope
{
	my $me = shift;
	my ($scopeMap) = @_;

	my $h = $me->open();
	my (%fmap, @list);
	my $importKey = KEY_IMPORT;
	my $sep = SEP_SPEC;

	LOOP:
	while (my ($key, $value) = each(%$h)) {
		next unless ($key =~ /^$importKey(.+)$/);
		my $f = $1;
		unless ($scopeMap) {
			push(@list, $UtsTune::UTS . '/' . $f);
			next;
		}
		foreach my $sc (split(/$sep/, $value)) {
			if ($scopeMap->{$sc}) {
				push(@list, $UtsTune::UTS . '/' . $f);
				next LOOP;
			}
		}
	}
	foreach my $scope (keys %$scopeMap) {
		push(@list, "$UtsTune::UTS/$scope/file")
			if (is_local_scope($scope));
	}
	foreach my $f (@list) {
		my $script = $me->getMakeOptScript($f);
		$fmap{$script} = 1 if ($script);
	}

	my (@files) = keys(%fmap);
	return (wantarray) ? @files : \@files;
}

# Get all scopes related to the specified modtune file.
sub getRelatedScope
{
	my $me = shift;
	my ($modtune) = @_;

	my @scopes;
	my $key = $me->importKey($modtune);
	my $val = $me->get($key);
	if ($val) {
		my $sep = SEP_SPEC;
		push(@scopes, split(/$sep/, $val));
	}

	return (wantarray) ? @scopes : \@scopes;
}

sub getModTuneMtime
{
	my $me = shift;
	my ($path) = @_;

	my $key = $me->modtuneKey($path);
	my $h = $me->open();
	return $h->{$key};
}

sub getModTune
{
	my $me = shift;
	my ($name, $plat) = @_;

	my $h = $me->open();
	my $path = $h->{$name};
	if ($plat and !$path) {
		my $k = $name . SEP_SPEC . $plat;
		$path = $h->{$k};
	}

	return $path;
}

sub removeModTune
{
	my $me = shift;
	my ($path) = @_;

	my $h = $me->open();
	my (@rmkey, @rmarch, @rmplat);
	my $archKey = KEY_ARCH;
	my $platKey = KEY_PLATFORM;
	while (my ($key, $value) = each(%$h)) {
		next unless ($value eq $path);
		if ($key =~ s/^$archKey//o) {
			push(@rmarch, $key);
		}
		elsif ($key =~ s/^$platKey//o) {
			push(@rmplat, $key);
		}
		else {
			push(@rmkey, $key);
		}
	}

	foreach my $arch (@rmarch) {
		$me->removeSpecific(KEY_ARCH, $arch);
	}
	foreach my $plat (@rmplat) {
		$me->removeSpecific(KEY_PLATFORM, $plat);
		$me->removeStaticScript($plat);
		foreach my $objdir (get_all_objdirs()) {
			my $key = join(SEP_SPEC, KEY_PLATOBJDIR, $plat,
				       $objdir);
			delete($h->{$key});
		}
	}
	foreach my $name (@rmkey) {
		my ($nm, $plat) = split(SEP_SPEC, $name);
		$me->removeOption($nm, $plat);
	}

	my $rel = utspath($path);
	foreach my $key ($me->modtuneKey($rel, 1),
			 $me->importKey($rel, 1)) {
		delete($h->{$key});
	}
}

sub getCurFile
{
	my $me = shift;
	my ($name, $plat, $objdir) = @_;

	my $h = $me->open();
	my $k = $me->curfileKey($name, undef, $objdir);
	my $opath = $h->{$k};
	if ($plat and !$opath) {
		$k = $me->curfileKey($name, $plat, $objdir);
		$opath = $h->{$k};
	}

	return $opath;
}

# Returns removed modtune path.
sub getRemovedModTune
{
	my $me = shift;
	my ($doremove) = @_;

	my $h = $me->open();
	my (%pathMap, @removed, @rmkey);
	my $modtuneKey = KEY_MODTUNE;
	while (my ($key, $value) = each(%$h)) {
		next unless ($key =~ /^$modtuneKey(.+)$/);
		my $path = $UtsTune::UTS . '/' . $1;
		unless (-f $path) {
			push(@removed, $path);
			push(@rmkey, $key) if ($doremove);
		}
	}
	foreach my $k (@rmkey) {
		delete($h->{$k});
	}
	return (wantarray) ? @removed : \@removed;
}

# Returns removed makeopt path.
sub getRemovedMakeOpt
{
	my $me = shift;
	my ($doremove) = @_;

	my $h = $me->open();
	my (%pathMap, @removed);
	my $makeoptKey = KEY_MAKEOPT;
	my $sep = SEP_SPEC;
	while (my ($key, $value) = each(%$h)) {
		next unless ($key =~ /^$makeoptKey/);
		my ($path, $mtime) = split(/$sep/, $value);
		push(@removed, $path) unless (-f $path);
	}

	if ($doremove) {
		foreach my $k (@removed) {
			$me->removeMakeOpt($k);
		}
	}

	return (wantarray) ? @removed : \@removed;
}

sub addStaticScript
{
	my $me = shift;
	my ($file, $plat, $arch) = @_;

	my $key = $me->specificKey(KEY_STATIC, $plat);
	my $h = $me->open();
	my $cur = $h->{$key};
	if (defined($cur)) {
		die "Static tune for $plat is already defined.\n"
			unless ($cur eq $file);
	}
	else {
		my $aval = join(SEP_SPEC, @$arch);
		my $akey = $me->specificKey(KEY_STATIC_ARCH, $plat);

		$h->{$key} = $file;
		$h->{$akey} = $aval;
	}
}

sub getStaticScript
{
	my $me = shift;
	my ($plat) = @_;

	my $key = $me->specificKey(KEY_STATIC, $plat);
	my $h = $me->open();
	my $tune = $h->{$key};
	if ($tune) {
		require UtsTune::PackageLoader;
		import UtsTune::PackageLoader;
		my $loader = UtsTune::PackageLoader->new();
		my $util = $loader->utilInstance();
		$util->checkTimeStamp($tune);
	}

	return $tune;
}

sub removeStaticScript
{
	my $me = shift;
	my ($plat) = @_;

	my $h = $me->open();

	my $key = $me->specificKey(KEY_STATIC, $plat);
	delete($h->{$key});
	$key = $me->specificKey(KEY_STATIC_ARCH, $plat);
	delete($h->{$key});
}

sub getStaticArch
{
	my $me = shift;
	my ($plat) = @_;

	my $h = $me->open();
	my $akey = $me->specificKey(KEY_STATIC_ARCH, $plat);
	my $sep = SEP_SPEC;
	my (@arch) = split(/$sep/, $h->{$akey});
	return \@arch;
}

sub initKey
{
	my $me = shift;
	my (%args) = @_;

	my $value = $args{value};
	my $plat = $args{platform};
	my $objdir = $args{objdir};

	my $h = $me->open();
	my $key = KEY_INIT;
	$key .= $plat . SEP_SPEC . $objdir if ($plat and $objdir);
	my $old = $h->{$key};

	if (defined($value)) {
		if ($value) {
			$h->{$key} = 1;
		}
		else {
			delete($h->{$key});
		}
	}

	return $old;
}

# Determine whether build environment for the specified platform is
# initialized or not.
sub checkPlatform
{
	my $me = shift;
	my ($plat, $objdir) = @_;

	# Do nothing if initialization is now running.
	return if ($me->initKey());

	return unless ($plat and $objdir);

	unless ($me->initKey(platform => $plat, objdir => $objdir)) {
		die <<OUT
Build environment for $plat ($objdir) is not yet initialized.
Try "make tune-init" under \$SRC/uts.
OUT
	}
}

sub addPlatformObjDir
{
	my $me = shift;
	my ($plat, $objdir) = @_;

	my $key = join(SEP_SPEC, KEY_PLATOBJDIR, $plat, $objdir);
	$me->put($key, 1);
}

sub isValidPlatform
{
	my $me = shift;
	my ($plat, $objdir) = @_;

	my $key = join(SEP_SPEC, KEY_PLATOBJDIR, $plat, $objdir);
	return $me->get($key);
}

sub setModuleDir
{
	my $me = shift;
	my ($plat, $dirs) = @_;

	my (@sorted) = sort(@$dirs);
	my $h = $me->open();
	my $basekey = $me->specificKey(KEY_MODINFO, $plat);

	# Remove old keys.
	my $count = scalar(@sorted);
	my $oldcnt = $h->{$basekey};
	if ($oldcnt) {
		for (my $i = $count; $i < $oldcnt; $i++) {
			my $key = $me->modinfoKey($basekey, $i);
			delete($h->{$key});
		}
	}

	for (my $i = 0; $i < $count; $i++) {
		my $key = $me->modinfoKey($basekey, $i);
		$h->{$key} = $sorted[$i];
	}

	$h->{$basekey} = $count;
}

sub getModuleDir
{
	my $me = shift;
	my ($plat) = @_;

	my $basekey = $me->specificKey(KEY_MODINFO, $plat);
	my $h = $me->open();
	my @list;

	my $count = $h->{$basekey};
	unless ($count) {
		return (wantarray) ? () : undef;
	}

	for (my $i = 0; $i < $count; $i++) {
		my $key = $me->modinfoKey($basekey, $i);
		push(@list, $h->{$key});
	}

	return (wantarray) ? @list : \@list;
}

1;
