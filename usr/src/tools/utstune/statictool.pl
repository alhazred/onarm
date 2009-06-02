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
# Copyright (c) 2006-2008 NEC Corporation
# All rights reserved.
#

use strict;
use Config;
use File::Basename;
use FileHandle;
use POSIX;

my ($perl, @libs) = (@ARGV);
my $file = basename($0);
$file =~ s/\.pl$//;

my $out = FileHandle->new($file, O_WRONLY|O_CREAT|O_TRUNC, 0755) or
	die "Can't open ($file): $!\n";

print $out <<EOF;
#!$perl

use lib qw(@libs);

EOF

print $out <<'EOF';
#
# Copyright (c) 2006-2008 NEC Corporation
# All rights reserved.
#

=head1 NAME

I<statictool> - Build utility to create static unix kernel

=head1 SYNOPSIS

B<statictool> [global-options] I<mode> [local-options] ...

=head1 DESCRIPTION

I<statictool> is build utility used to create static unix kernel.
It switches its work as the specified I<mode>.

=head1 GLOBAL OPTIONS

The following options are available as global option.
They are applied to all mode.

=over 4

=item B<-v>|B<--verbose>

Print verbose messages to stderr.

=item B<-h>|B<--help>

Print help message.

=item B<-O>|B<--objdir> I<objdir>

Specify build directory name to store kernel object files.

=item B<-P>|B<--platform> I<platform>

Specify target platform name.

=back

=head1 MODES

I<statictool> has the following modes:

=head2 B<cppflags>

This mode prints CPPFLAGS that is used to compile static kernel module
If the module is not configured as static link module, no output is printed.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

If this option is not specified, no output is printed because that means
no module is configured as static link module.

=item B<-m>|B<--modinfo> I<modinfo>

Specify path to modinfo that contains module information.
This is mandatory option.

See B<MODINFO FORMAT> section for the format of modinfo file.

=item B<-b>|B<--build-directory> B<directory>

Specify path to module build directory.
If omitted, the current directory is used as build directory.

=back

=head2 B<modstubs_cppflags>

This mode prints CPPFLAGS that is used to compile modstubs.s,
that contains stub entries for modules. Typically this flags is used to
compile platform-specific unix binary.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

If this option is not specified, no output is printed because that means
no module is configured as static link module.

=back

=head2 B<modinfo>

If the module is configured as static link module, this mode prints
C source code that contains module definition for static linking.
And if driver configuration file is specified, its configuration is also
printed.

If not configured as static link module, this prints only dependency
definition using "_depends_on" string.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

If this option is not specified, no output is printed because that means
no module is configured as static link module.

=item B<-d>|B<--driver-conf> I<driver.conf>

Specify path to driver.conf file, that contains driver configuration.
This options will be simply ignored if the specified file doesn't exist.

=item B<-b>|B<--build-directory> B<directory>

Specify path to module build directory.
If omitted, the current directory is used as build directory.

=item B<-m>|B<--modinfo> I<modinfo>

Specify path to modinfo that contains module information.
This is mandatory option.

See B<MODINFO FORMAT> section for the format of modinfo file.

=back

=head2 B<directory>

List all build directory paths for static link modules.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

If this option is not specified, no output is printed because that means
no module is configured as static link module.

=back

=head2 B<modpath>

List all binary paths to be linked to unix statically.
Object directory name, such as dbg32, obj32, must be specified as argument.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

If this option is not specified, no output is printed because that means
no module is configured as static link module.

=item B<-r>|B<--relocatable>

Show only relocatable object path.

=item B<-l>|B<--loadable>

Inverse of B<-r>. Show only loadable module object path.

=back

=head2 B<drvinfo>

This mode prints C source code that contains all static link module
information. And this command also used to embed some system files into kernel.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

=item B<-n>|B<--name-to-major> I<name_to_major>

Specify path to file name_to_major file, that will be installed as
/etc/name_to_major. This is mandatory option.

=item B<-s>|B<--name-to-sysnum> I<name_to_sysnum>

Specify path to file name_to_sysnum file, that will be installed as
/etc/name_to_sysnum. This is mandatory option.

=item B<-D>|B<--dacf-conf> I<dacf.conf>

Specify path to file dacf.conf file, that will be installed as
/etc/dacf.conf. This is mandatory option.

=item B<-A>|B<--driver-aliases> I<driver_aliases>

Specify path to file driver_aliases file, that will be installed as
/etc/driver_aliases. This is mandatory option.

=item B<-A>|B<--driver-classes> I<driver_classes>

Specify path to file driver_classes file, that will be installed as
/etc/driver_classes. This is mandatory option.

=back

=head2 B<dtracestubs>

This mode prints C source code that contains stub information of dtrace
probe entry point. You must specify path to kernel object file as argument.

The followings are local options:

=over 4

=item B<-n>|B<--nm> I<nm>

Specify path to I<nm>(1) command.

If omitted, it will be searched using B<PATH> environment variable.

=head2 B<symbind>

This mode changes symbol binding in ELF object.
You must specify ELF file path as argument.

Solaris ld can change binding of symbols in  ELF relocatable
specifying mapfile to -M option. But GNU ld can't change even though
it can change it in ELF shared object. So you may want to use B<symbind> tool
to change symbol bindings in loadable module binary, that is ELF
relocatable file. Currently, Only thing that B<symbind> can do is to localize
global symbol.

This mode requires I<objcopy>(1) command bundled to GNU binutils,
and I<nm>(1) command bundled to Solaris.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

=item B<-m>|B<--modinfo> I<modinfo>

Specify path to modinfo that contains module information.

See B<MODINFO FORMAT> section for the format of modinfo file.

=item B<-M>|B<--mapfile> mapfile

This is mandatory option to specify path to mapfile.

The format of mapfile is same as Solaris ld's mapfile.
Note that B<symbind> will ignore all description in mapfile except for
description to specify symbol binding.

=item B<-b>|B<--build-directory> B<directory>

Specify path to module build directory.
If omitted, the current directory is used as build directory.

=item B<-n>|B<--nm> I<nm>

Specify path to I<nm>(1) command.

If omitted, it will be searched using B<PATH> environment variable.

=item B<-O>|B<--objcopy> I<objcopy>

Specify path to I<objcopy>(1) command.

If omitted, it will be searched using B<PATH> environment variable.

=item B<-o>|B<--output> output

Specify output file path.

If omitted, input file will be modified.

=back

=head2 B<test>

This mode tests whether object file should be built as static-linked object.

If objects under the specified directory should be statically linked,
I<statictool> prints strings specified to command arguments to standard output.
No output is printed if objects should not be build as static-linked object.

If no argument is specified, "static" is printed if object files should be
build as static-linked object. Otherwise "dynamic" is printed.

The followings are local options:

=over 4

=item B<-c>|B<--static-conf> I<static.conf>

Specify path to static.conf.
See B<STATIC.CONF FORMAT> section for the format of static.conf.

=item B<-b>|B<--build-directory> B<directory>

Specify path to module build directory.
If omitted, the current directory is used as build directory.

=item B<-N>|B<--not>

Inverse condition. If this option is specified, I<statictools> prints
command arguments if objects should NOT be built as static-linked object.
No output is printed if not.

This option has no effect if no command argument is given.

=back

=back

=head1 STATIC.CONF FORMAT

I<static.conf> defines modules to be linked to unix statically.
Text from '#' character until the end of line will be ignored as comment.

Each valid line represents a module build environment.
Each line may have 1 or 2 columns separated by whitespace character.

=over 3

=item -

The first column represents the name of build directory.
It must be a relative path to the specified static.conf path.

If the following lines are contained in $(WS)/uts/mach/static.conf:

  module1
  module2

modules under the following build directory will be linked to unix binary
statically.

  $(WS)/uts/mach/module1
  $(WS)/uts/mach/module2

=item -

The second column is used to define macro to avoid symbol confliction
with stub entries. It means that the macro at the second column must be
defined when modstubs.s is compiled.

If the following lines are contained in static.conf:

  module1      MODULE1_MODULES
  module2
  module3      MODULE3_MODULES

the following flags will be passed to CPPFLAGS when modstubs.s is compiled.

  -DMODULE1_MODULES -DMODULE3_MODULES

=back

=head1 MODINFO FORMAT

I<modinfo> file defines the module information.
Text from '#' character until the end of line will be ignored as comment.

Each valid line must have 2 columns separated by whitespace character.
The first colums is the name of property, and the second is its value.

The following property name can be specified:

=over 4

=item B<namespace>

Define namespace of module, that is the name of directory where module file
is installed. If the module file is installed into 2 or more directories,
they all can be specified using whitespace as a separator.

If the module will be installed into /kernel/drv and /kernel/strmod,
namespace parameter must be "drv strmod".

If the module is not configured as static link module, this parameter is
ignored. But if configured as static link module, this parameter is mandatory.

=item B<modname>

Define module name, that is the filename of module binary.

If the module is not configured as static link module, this parameter is
ignored. But if configured as static link module, this parameter is mandatory.

=item B<depends>

Define module dependency.

The format is the same as "_depends_on" string.
If "misc/module1 drv/module2" is specified, it means that the module depends
on module "misc/module1" and "drv/module2".

=back

=cut

use strict;
use Getopt::Long;
use File::Basename;
use FileHandle;
use File::Basename;
use Cwd qw(abs_path);
use POSIX;
use POSIX qw(:sys_wait_h :errno_h);

use StaticTool;
use StaticTool::PackageLoader;
use UtsTune;

use vars qw($PROGNAME $VERBOSE $MODE %MODE_HELP $SDT_PREFIX @EXIT_HOOK
	    $OBJDIR $PLATFORM);

$PROGNAME = basename($0);
$SDT_PREFIX = '__dtrace_probe_';

END {
	foreach my $hook (@EXIT_HOOK) {
		&$hook();
	}
}

sub setup_mode_help();
sub usage($);
sub verbose(@);
sub parse_local_option(@);
sub is_static_build($$);
sub static_info_name($);
sub eval_string($);
sub find_program($);
sub check_status($$);
sub read_from_process($@);
sub run(@);

# Mode subroutines.
sub do_cppflags();
sub do_modstubs_cppflags();
sub do_modinfo();
sub do_directory();
sub do_modpath();
sub do_drvinfo();
sub do_dtracestubs();
sub do_symbind();
sub do_test();

# static.conf parser
{
	package StaticConfParser;

	use FileHandle;
	use File::Basename;
	use StaticTool;
	use UtsTune;

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($file) = @_;

		&main::verbose("static.conf = $file\n");

		my $me = {FILE => $file};
		return bless $me, $class;
	}

	sub loadDB
	{
		my $me = shift;
		my ($arch, $plat) = @_;

		die "Duplicated \"%import\" line.\n" if (exists($me->{DB}));

		# Load static module database.
		require UtsTune::DB::Static;
		import UtsTune::DB::Static;
		my $objdir = $main::OBJDIR;
		if (&UtsTune::DB::Static::exists($plat, $arch, $objdir)) {
			# Check whether the script file has been modified.
			require UtsTune::DB::Parameter;
			import UtsTune::DB::Parameter;
			my $db = UtsTune::DB::Parameter->new();
			$db->getStaticScript($plat);
			$me->{DB} = UtsTune::DB::Static->
				new($plat, $arch, $objdir);
		}
		else {
			$me->{DB} = undef;
		}
		$me->{DB_ARCH} = $arch;
	}

	sub parse
	{
		my $me = shift;
		my ($check) = @_;

		return undef if (!defined($check) and $me->{CONF});

		my $file = $me->{FILE};
		my $cf = FileHandle->new($file) or
			die "open($file) failed: $!";
		my (@conf, %dhash);
		while (<$cf>) {
			next if (/^\s*\x23/);
			chomp;
			my (@token) = split(/\s+/, $_);
			next unless (@token);

			if ($token[0] eq '%import') {
				shift(@token);
				$me->loadDB(@token);
				next;
			}

			my ($dir, $def) = (@token);
			die "\"$dir\" is duplicated in $file.\n"
				if (exists($dhash{$dir}));
			$dhash{$dir} = $def;

			if (defined($check)) {
				# Check whether the specified directory
				# is a build directory for static module.
				if ($check =~ m,\Q/$dir\E$,) {
					# This module must be linked statically
					# to unix.
					return ($def) ? $def : '1';
				}
				next;
			}

			my (%map) = (DIR => $dir, DEF => $def);
			push(@conf, \%map);
		}
		$me->{CONF} = \@conf;
		$me->{DEFS} = \%dhash;
		return undef;
	}

	# Returns CPPFLAGS for stub entry if static && stub.
	# Returns 1 if static.
	# Returns undef if not static.
	sub isStatic
	{
		my $me = shift;
		my ($dir) = @_;

		my $def = $me->parse($dir);
		return $def if ($def);

		# Check dynamic configurations.
		my $db = $me->{DB};
		return undef unless ($db);
		$dir = utspath($dir);
		$dir =~ s,^[^/]+/,,o;
		return $db->isStatic($dir);
	}

	sub modstubCppflags
	{
		my $me = shift;

		$me->parse();
		my @defs;
		foreach my $c (@{$me->{CONF}}) {
			my $def = $c->{DEF};
			push(@defs, "-D$def") if ($def);
		}

		# Check dynamic configurations.
		my $db = $me->{DB};
		push(@defs, map {"-D$_"} @{$db->getAllFlags()}) if ($db);

		return (wantarray) ? @defs : \@defs;
	}

	sub directories
	{
		my $me = shift;

		$me->parse();
		my $parent = dirname($me->{FILE});
		my @dirs;
		foreach my $c (@{$me->{CONF}}) {
			my $dir = $c->{DIR};
			push(@dirs, "$parent/$dir");
		}

		# Check dynamic configurations.
		my $db = $me->{DB};
		push(@dirs, map { "$parent/$_"; } @{$db->getAllDirectories()})
			if ($db);

		return (wantarray) ? @dirs : \@dirs;
	}

	sub modinfo
	{
		my $me = shift;
		my $modinfo = shift || DEFAULT_MODINFO;

		require StaticTool::PackageLoader;
		import StaticTool::PackageLoader;
		my $loader = StaticTool::PackageLoader->new();

		my @info;
		foreach my $dir (@{$me->directories()}) {
			my $path = $dir . '/' . $modinfo;
			push(@info, $loader->modInfoInstance($path));
		}

		return (wantarray) ? @info : \@info;
	}
}

# Solaris ld mapfile parser
{
	package MapfileParser;

	use vars qw(@ISA);

	use StaticTool::BindingLexer;

	@ISA = qw(StaticTool::BindingLexer);

	use FileHandle;
	use File::Basename;

	use constant	STAR		=> 10;
	use constant	LBRACE		=> 11;
	use constant	RBRACE		=> 12;
	use constant	GLOBAL		=> 13;
	use constant	LOCAL		=> 14;
	use constant	HIDDEN		=> 15;
	use constant	PROTECTED	=> 16;
	use constant	SYMBOLIC	=> 17;
	use constant	ELIMINATE	=> 18;

	use constant	S_BEGIN		=> 1;
	use constant	S_BRACE		=> 2;
	use constant	S_GLOBAL_BEGIN	=> 3;
	use constant	S_GLOBAL_SYMBOL	=> 4;
	use constant	S_GLOBAL_END	=> 5;
	use constant	S_LOCAL_BEGIN	=> 6;
	use constant	S_LOCAL_SYMBOL	=> 7;
	use constant	S_LOCAL_END	=> 8;
	use constant	S_SCOPE		=> 9;
	use constant	S_SCOPE_END	=> 10;
	use constant	S_BRACE_END	=> 11;

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;

		my $me = $class->SUPER::new(@_);
		$me->parse();

		return $me;
	}

	sub tokenize
	{
		my $me = shift;
		my ($line, $len) = @_;

		my $c = substr($line, 0, 1);
		my ($token, $type);
		if ($c eq ';') {
			$type = $me->SEMI;
			$line = substr($line, 1);
			$token = $c;
		}
		elsif ($c eq "\n") {
			$type = $me->NEWLINE;
			$line = substr($line, 1);
			$token = $c;
		}
		elsif ($c eq '*') {
			$type = STAR;
			$line = substr($line, 1);
			$token = $c;
		}
		elsif ($c eq '{') {
			$type = LBRACE;
			$line = substr($line, 1);
			$token = $c;
		}
		elsif ($c eq '}') {
			$type = RBRACE;
			$line = substr($line, 1);
			$token = $c;
		}
		elsif ($line =~ /^(global:)/o) {
			$type = GLOBAL;
			$token = $1;
			$line = substr($line, length($token));
		}
		elsif ($line =~ /^(local:)/o) {
			$type = LOCAL;
			$token = $1;
			$line = substr($line, length($token));
		}
		elsif ($line =~ /^(hidden:)/o) {
			$type = HIDDEN;
			$token = $1;
			$line = substr($line, length($token));
		}
		elsif ($line =~ /^(symbolic:)/o) {
			$type = SYMBOLIC;
			$token = $1;
			$line = substr($line, length($token));
		}
		elsif ($line =~ /^(protected:)/o) {
			$type = PROTECTED;
			$token = $1;
			$line = substr($line, length($token));
		}
		elsif ($line =~ /^(eliminate:)/o) {
			$type = ELIMINATE;
			$token = $1;
			$line = substr($line, length($token));
		}
		else {
			($token, $line) = $me->parseSymbol($line, $len);
			$type = $me->SYMBOL;
		}
		return ($token, $type, $line);
	}

	sub parse
	{
		my $me = shift;

		return () if ($me->{EOF});
		my (%globalsym, %localsym);
		my $state = S_BEGIN;

		while (1) {
			my ($token, $type) = $me->nextToken();
			unless (defined($token)) {
				$me->{EOF} = 1;
				last;
			}

			if ($type == $me->LBRACE) {
				if ($state == S_BEGIN) {
					$state = S_BRACE;
				}
				else {
					$me->syntaxError("Unexpected '{'.");
				}
			}
			elsif ($type == $me->RBRACE) {
				if ($state == S_GLOBAL_END or
				    $state == S_LOCAL_END or
				    $state == S_SCOPE_END or
				    $state == S_BEGIN) {
					$state = S_BRACE_END;
				}
				else {
					$me->syntaxError("Unexpected '}'.");
				}
			}
			elsif ($type == $me->SEMI) {
				if ($state == S_GLOBAL_SYMBOL) {
					$state = S_GLOBAL_END
				}
				elsif ($state == S_LOCAL_SYMBOL) {
					$state = S_LOCAL_END;
				}
				elsif ($state == S_SCOPE) {
					$state = S_SCOPE_END;
				}
				elsif ($state == S_BRACE_END) {
					$state = S_BEGIN;
				}
				else {
					$me->syntaxError
						("Unexpected semicolon.");
				}
			}
			elsif ($type == $me->GLOBAL) {
				if ($state == S_BRACE or
				    $state == S_GLOBAL_END or
				    $state == S_LOCAL_END) {
					$state = S_GLOBAL_BEGIN;
				}
				else {
					$me->parseError
						("Unexpeced global scope.");
				}
			}
			elsif ($type == $me->LOCAL) {
				if ($state == S_BRACE or
				    $state == S_GLOBAL_END or
				    $state == S_LOCAL_END) {
					$state = S_LOCAL_BEGIN;
				}
				else {
					$me->parseError
						("Unexpeced global scope.");
				}
			}
			elsif ($type == $me->PROTECTED or
			       $type == $me->HIDDEN or
			       $type == $me->SYMBOLIC or
			       $type == $me->ELIMINATE) {
				# Unsupported scopes.
				$state = S_SCOPE;
			}
			elsif ($type == $me->SYMBOL) {
				if ($state == S_GLOBAL_BEGIN or
				    $state == S_GLOBAL_END) {
					if ($localsym{$token}) {
						$me->parseError
							("Symbol \"", $token ,
							 "\" is defined in ",
							 "both global and ",
							 "local scope.");
					}
					$globalsym{$token} = 1;
					$state = S_GLOBAL_SYMBOL;
				}
				elsif ($state == S_LOCAL_BEGIN or
				       $state == S_LOCAL_END) {
					if ($globalsym{$token}) {
						$me->parseError
							("Symbol \"", $token ,
							 "\" is defined in ",
							 "both global and ",
							 "local scope.");
					}
					$localsym{$token} = 1;
					$state = S_LOCAL_SYMBOL;
				}
				elsif ($state == S_GLOBAL_SYMBOL or
				       $state == S_LOCAL_SYMBOL) {
					# Ignore symbol attributes.
					next;
				}
				elsif ($state != S_SCOPE and
				       $state != S_SCOPE_END) {
					$me->syntaxError("Unexpected token: ",
							 $token);
				}
			}
			elsif ($type == $me->STAR) {
				if ($state == S_LOCAL_BEGIN) {
					if ($globalsym{$token}) {
						$me->parseError
							("Symbol \"", $token ,
							 "\" is defined in ",
							 "both global and ",
							 "local scope.");
					}
					$localsym{$token} = 1;
					$state = S_LOCAL_SYMBOL;
				}
				else {
					$me->syntaxError("Unexpected token: ",
							 $token);
				}
			}
			elsif ($type != $me->NEWLINE) {
				$me->syntaxError("Unexpected token type: [",
						 $token, "]: ", $type);
			}
		}

		$me->{DEFS} = {GLOBAL => \%globalsym, LOCAL => \%localsym};
	}

	sub isLocalized
	{
		my $me = shift;
		my ($sym) = @_;

		my $globalsyms = $me->{DEFS}->{GLOBAL};
		my $localsyms = $me->{DEFS}->{LOCAL};

		if ($localsyms->{'*'}) {
			# All global symbols but in $globalsyms should be
			# localised.
			return ($globalsyms->{$sym}) ? undef : 1;
		}
		return ($localsyms->{$sym});
	}
}

MAIN:
{
	Getopt::Long::Configure(qw(no_ignore_case bundling require_order));
	setup_mode_help();

	my ($help, $switch);
	usage(2) unless (GetOptions
			 ('v|verbose'		=> \$VERBOSE,
			  'h|help'		=> \$help,
			  'O|objdir=s'		=> \$OBJDIR,
			  'switch=s'		=> \$switch,
			  'P|platform=s'	=> \$PLATFORM));
	usage(0) if ($help);
	$MODE = shift(@ARGV);
	usage(2) unless ($MODE);
	unless ($MODE_HELP{$MODE}) {
		undef $MODE;
		usage(2);
	}

	if ($OBJDIR) {
		# $OBJDIR may not yet be determined in multiple targeted
		# makefile. So we must check whether $OBJDIR is valid or not.
		unless (grep {$_ eq $OBJDIR} (get_build_objdirs())) {
			# Ignore invalid OBJDIR.
			$OBJDIR = get_objdir(0);
		}
	}

	my $func = "do_$MODE" . '()';
	eval $func;
	if ($@) {
		my $err = "$@";
		chomp($err);
		die "\n*** $PROGNAME $MODE failed:\n$err\n\n";
	}
}

sub setup_mode_help()
{
	$MODE_HELP{cppflags} = <<OUT;
   $PROGNAME cppflags -c <static.conf> [-m <modinfo_file>] -b <build_dir>
      Print CPPFLAGS to STDOUT for compilation of the specified directory.
      build_dir must be under uts directory.
OUT
	$MODE_HELP{modstubs_cppflags} = <<OUT;
   $PROGNAME modstubs_cppflags [-c <static.conf> -c ...]
      Print CPPFLAGS to STDOUT for compilation of modstubs.o.
OUT
	$MODE_HELP{modinfo} = <<OUT;
   $PROGNAME modinfo [-d <driver.conf>][-b <build_dir>][-m <modinfo_file>]
               [-c <static.conf>]
      Print C source code that contains module definition.
OUT
	$MODE_HELP{directory} = <<OUT;
   $PROGNAME directory [-c static.conf -c ...]
      Print build directory path defined in the specified static.conf.
OUT
	$MODE_HELP{modpath} = <<OUT;
   $PROGNAME modpath [-c static.conf -c ...] [-r|-l]
      Print module binary paths to be linked statically.
OUT
	$MODE_HELP{drvinfo} = <<OUT;
   $PROGNAME drvinfo [-c <static.conf> -c ...] -n name_to_major
               -s name_to_sysnum -D dacf.conf -A driver_aliases
               -C driver_classes
      Print C source code that contains all static-linked module information.
      This command also used to embed some system files into kernel.
OUT
	$MODE_HELP{dtracestubs} = <<OUT;
   $PROGNAME dtracestubs -i vmunix.o [-n nm_command]
      Print C source code that contains stub information of dtrace probe
      entry point.
OUT

	$MODE_HELP{symbind} = <<OUT;
   $PROGNAME symbind [-o <output>][-c <static.conf>][-m <modinfo_file>]
                [-b <build_dir>] -M <mapfile> <input>
      Change symbol binding in <input> according to descriptions in <mapfile>.
OUT

	$MODE_HELP{test} = <<OUT;
   $PROGNAME test [-N][-c <static.conf>][-b <build_dir>] ...
      Test whether objects under the specified directory should be
      static-linked objects.
OUT
}

sub usage($)
{
	my ($status) = @_;

	my $out = ($status) ? \*STDERR : \*STDOUT;
	print $out <<OUT;
Usage: $PROGNAME [options] mode ...

OUT

	if ($MODE) {
		my $msg = $MODE_HELP{$MODE};
		print $out <<OUT;
$msg
OUT
	}
	if ($status) {
		print $out <<OUT;
Try "$PROGNAME --help".
OUT
		exit $status;
	}

	unless ($MODE) {
	print $out <<OUT;
Mode:
OUT
		foreach my $mode (sort(keys %MODE_HELP)) {
			my $msg = $MODE_HELP{$mode};
			print $out <<OUT;
$msg
OUT
		}
	}
	exit $status;
}

sub verbose(@)
{
	return unless ($VERBOSE);
	print STDERR @_;
}

sub parse_local_option(@)
{
	my $localdefs = shift;
	my (@staticcf, @dir, @modinfo, @drivercf, @majbind, @sysbind,
	    @drvalias, @drvclass, @dacf);
	my (%defs) = ('c|static-conf=s'		=> \@staticcf,
		      'b|build-directory=s'	=> \@dir,
		      'm|modinfo=s'		=> \@modinfo,
		      'd|driver-conf=s'		=> \@drivercf,
		      'n|name-to-major=s'	=> \@majbind,
		      's|name-to-sysnum=s'	=> \@sysbind,
		      'D|dacf-conf=s'		=> \@dacf,
		      'A|driver-aliases=s'	=> \@drvalias,
		      'C|driver-classes=s'	=> \@drvclass);
	if ($localdefs and ref($localdefs) eq 'HASH') {
		while (my ($k, $v) = each(%$localdefs)) {
			$defs{$k} = $localdefs->{$k};
		}
	}
	usage(2) unless (GetOptions(%defs));
	usage(2) if (@dir > 1 or @drivercf > 1 or @modinfo > 1 or
		     @majbind > 1 or @sysbind > 1 or @dacf > 1 or
		     @drvalias > 1 or @drvclass > 1);
	my $d = $dir[0];
	$d =~ s,//+,/,g;
	$d =~ s,/$,,g;
	my $mi = $modinfo[0] || DEFAULT_MODINFO;
	my %opts = (static_conf	=> \@staticcf,
		    builddir	=> $d,
		    modinfo	=> $mi,
		    driver_conf	=> $drivercf[0],
		    major_bind	=> $majbind[0],
		    sys_bind	=> $sysbind[0],
		    drvalias	=> $drvalias[0],
		    drvclass	=> $drvclass[0],
		    dacf_conf	=> $dacf[0]);
	return \%opts;
}

sub is_static_build($$)
{
	my ($conf, $dir) = @_;

	my $cf = StaticConfParser->new($conf);
	return $cf->isStatic($dir);
}

sub static_info_name($)
{
	my ($modname) = @_;

	$modname = escape_symbol($modname);
	return $modname . '_static_info';
}

sub find_program($)
{
	my ($prog) = @_;

	my $name = uc($prog);
	return $ENV{$name} if (defined($ENV{$name}));

	foreach my $dir (split(/:/, $ENV{PATH})) {
		my $p = "$dir/$prog";
		return $p if (-x $p);
	}

	return $prog;
}

sub check_status($$)
{
	my ($cmd, $st) = @_;

	if (WIFEXITED($st)) {
		my $status = WEXITSTATUS($st);
		die "$cmd failed with status $status\n" unless ($status == 0);
	}
	elsif (WSIGNALLED($st)) {
		my $sig = WTERMSIG($st);

		# WCOREDUMP isn't a POSIX macro, do it the non-portable way.
		if ($st & 0x80) {
			die "$cmd failed with signal $sig (core dumped)\n";
		}
		else {
			die "$cmd failed with signal $sig\n";
		}
	}
	else {
		my $msg = sprintf("%s failed: status = 0x%08x", $cmd, $st);
		die "$msg\n";
	}
}

sub read_from_process($@)
{
	my ($handler, @argv) = @_;

	verbose("+ @argv\n");

	my ($rh, $wh) = FileHandle::pipe;
	my $pid = fork();
	if (!defined($pid)) {
		die "fork() failed: $!\n";
	}
	elsif ($pid == 0) {
		undef $rh;
		$wh->autoflush(1);
		POSIX::dup2($wh->fileno(), STDOUT->fileno());

		exec(@argv);
		die "exec(@argv) failed: $!\n";
	}

	undef $wh;
	while (<$rh>) {
		&$handler($_);
	}

	my $ret;
	do {
		$ret = waitpid($pid, 0);
	} while ($ret == -1 and $! == EINTR);
	die "waitpid() failed: $!\n" if ($ret == -1);

	check_status($argv[0], $?);
}

sub run(@)
{
	my (@argv) = @_;
	my $st;

	verbose("+ @argv\n");

	my $status = system(@argv);
	die "Failed to execute $argv[0]: $!\n" if ($status == -1);
	check_status($argv[0], $status);
}



##
## Dump CPPFLAGS for static linking.
##
sub do_cppflags()
{
	my $opts = parse_local_option();
	my $sconfs = $opts->{static_conf};
	return unless (@$sconfs);
	my $conf = $sconfs->[0];

	my $modinfo = $opts->{modinfo};
	my $dir = $opts->{builddir} || getcwd();
	usage(2) unless (-d $dir);
	usage(2) unless ($OBJDIR);

	$dir = abs_path($dir);
	verbose("static.conf = $conf\n");
	verbose("modinfo = $modinfo\n");
	verbose("build_directory = $dir\n");

	my $loader = StaticTool::PackageLoader->new();
	my $minfo = $loader->modInfoInstance($modinfo);

	my $static = is_static_build($conf, $dir);
	my $defs = $minfo->cppflags($static, $OBJDIR, $PLATFORM);

	print join(" ", @$defs), "\n";
}

##
## Dump CPPFLAGS to compile modstubs.o
##
sub do_modstubs_cppflags()
{
	my $opts = parse_local_option();
	my $confs = $opts->{static_conf};
	return unless (@$confs);

	my @defs;
	foreach my $conf (@$confs) {
		verbose("static.conf = $conf\n");
		next unless ($conf and -f $conf);

		my $cf = StaticConfParser->new($conf);
		push(@defs, @{$cf->modstubCppflags()});
	}
	print join(" ", @defs);
}

##
## Create C source code to define module information.
##
sub do_modinfo()
{
	my $opts = parse_local_option();
	my $modinfo = $opts->{modinfo};
	my $bdir = $opts->{builddir} || getcwd();
	my $sconfs = $opts->{static_conf};
	return unless (@$sconfs);
	usage(2) unless ($OBJDIR);
	my $conf = $sconfs->[0];
	my $drvconf = $opts->{driver_conf};

	$bdir = abs_path($bdir);
	verbose("static.conf = $conf\n");
	verbose("build_directory = $bdir\n");
	verbose("modinfo = $modinfo\n");
	verbose("driver.conf = $drvconf\n");
	unless (-f $modinfo) {
		# This module is not wanted to be build using this tool.
		return;
	}

	# Parse module information.
	my $loader = StaticTool::PackageLoader->new();
	my $minfo = $loader->modInfoInstance($modinfo);
	my $depends = $minfo->depends();
	my $modname = $minfo->name();
	my $namespace = $minfo->namespace();

	my $stubdef = is_static_build($conf, $bdir);
	unless (defined($stubdef)) {
		# Build as loadable module.
		print <<OUT;
/*
 * This file is auto-generated. Do NOT edit!!
 */

OUT
		if (@$depends) {
			my $dp = join(' ', @$depends);
			print <<OUT;
const char _depends_on[] = "$dp";
OUT
		}

		$minfo->dumpAdditional($OBJDIR, $PLATFORM);
		return;
	}

	die "\"namespace\" is not defined in $modinfo.\n" unless (@$namespace);
	die "\"modname\" is not defined in $modinfo.\n" unless ($modname);
	verbose("namespace = [@$namespace]\n");
	verbose("modname   = [$modname]\n");
	verbose("depends   = [@$depends]\n");

	my @property;
	if (-f $drvconf) {
		# Parse driver.conf.
		require StaticTool::DriverConfParser;
		import StaticTool::DriverConfParser;
		my $parser = StaticTool::DriverConfParser->new($drvconf);
		while (my $prop = $parser->parse()) {
			push(@property, $prop);
		}
	}

	# Dump namespace array.
	print <<OUT;
/*
 * This file is auto-generated. Do NOT edit!!
 */

#include <sys/types.h>
#include <sys/modctl.h>

/* Namespace */
static const char *namespace_array[] = {
OUT
	foreach my $ns (@$namespace) {
		print "\t", stringfy($ns), ",\n"
	}
	print <<OUT;
	NULL
};
OUT

	# Dump dependencies.
	my $dp;
	if (@$depends) {
		my @dinfo;
		print <<OUT;

/* Module dependencies */
OUT
		foreach my $d (@$depends) {
			my $di = static_info_name(basename($d));
			push(@dinfo, $di);
			print <<OUT;
extern mod_static_t $di;
OUT
		}
		my $dependname = $modname . '_depends_on';
		print <<OUT;

static const mod_static_t *${dependname}[] = {
OUT
		foreach my $di (@dinfo) {
			print <<OUT;
	\&$di,
OUT
		}
		print <<OUT;
	NULL
};
OUT
		$dp = $dependname;
	}
	else {
		$dp = C_NULL;
	}

	require StaticTool::StructDumper;
	import StaticTool::StructDumper;
	my $dumper = StaticTool::StructDumper->new();

	# Dump contents of driver.conf.
	my ($hwcptr, $hwccnt) = (C_NULL, 0);
	if (@property) {
		my $dcfname = basename($drvconf);
		print <<OUT;

/* Driver configuration derived from $dcfname. */
OUT
		my $hwcname = "drvconf_hwc_spec";
		$hwcptr = $hwcname;
		$hwccnt = scalar(@property);
		my (@hwc, @ddiprop);
		for (my $i = 0; $i < @property; $i++) {
			my $prop = $property[$i];

			$dumper->dumpHwcSpec(\@hwc, \@ddiprop, $i, $prop);
		}
		my $pp = join('', @ddiprop);
		my $hwcp = join('', @hwc);
		print <<OUT;
$pp
static const struct hwc_spec ${hwcname}[] = {
$hwcp
};
OUT
	}

	# Dump mod_static structure.
	my $infoname = static_info_name($modname);
	my $strmodname = stringfy($modname);
	my $prefix = $minfo->entryPointPrefix();
	my $init = $prefix . '_init';
	my $info = $prefix . '_info';
	my $flags = ($stubdef eq '1') ? '0' : 'MS_STUB';
	print <<OUT;

extern int $init(void);
extern int $info(struct modinfo *);

/* Statically linked module information */
mod_static_t $infoname = {
OUT
	$dumper->dumpMember("namespace_array", "ms_namespace");
	$dumper->dumpMember($strmodname, "ms_modname");
	$dumper->dumpMember($dp, "ms_depends");
	$dumper->dumpMember($init, "ms_init");
	$dumper->dumpMember($info, "ms_info");
	$dumper->dumpMember($flags, "ms_flags");
	$dumper->dumpMember($hwccnt, "ms_nhwc");
	$dumper->dumpMember($hwcptr, "ms_hwc");
	print <<OUT;
};

OUT

	$minfo->dumpAdditional($OBJDIR, $PLATFORM);
}

##
## Print build directory.
##
sub do_directory()
{
	my ($reloc, $loadable);
	my (%mydef) = ('r|relocatable' => \$reloc, 'l|loadable' => \$loadable);
	my $opts = parse_local_option(\%mydef);
	die "-r and -l are exclusive.\n" if ($reloc and $loadable);
	my $confs = $opts->{static_conf};
	my $modinfo = $opts->{modinfo};
	verbose("modinfo = $modinfo\n");
	return unless (@$confs);

	foreach my $conf (@$confs) {
		my $cf = StaticConfParser->new($conf);
		if ($reloc or $loadable) {
			foreach my $mi (@{$cf->modinfo($modinfo)}) {
				my $isreloc = $mi->isRelocatable();
				next if (($reloc and !$isreloc) or
					 ($loadable and $isreloc));
				my $path = dirname($mi->path());
				print $path, "\n";
			}
		}
		else {
			print join("\n", @{$cf->directories()}), "\n";
		}
	}
}

##
## Print static module binary path.
##
sub do_modpath()
{
	my ($reloc, $loadable);
	my (%mydef) = ('r|relocatable' => \$reloc, 'l|loadable' => \$loadable);
	my $opts = parse_local_option(\%mydef);
	die "-r and -l are exclusive.\n" if ($reloc and $loadable);
	my $confs = $opts->{static_conf};
	return unless (@$confs);
	my $modinfo = $opts->{modinfo};
	usage(2) unless ($OBJDIR);
	verbose("objdir = $OBJDIR\n");
	verbose("modinfo = $modinfo\n");

	foreach my $conf (@$confs) {
		verbose("static.conf = $conf\n");
		my $cf = StaticConfParser->new($conf);
		foreach my $mi (@{$cf->modinfo($modinfo)}) {
			my $isreloc = $mi->isRelocatable();
			next if (($reloc and !$isreloc) or
				 ($loadable and $isreloc));
			my $path = $mi->path();
			my $name = $mi->name();
			my $dir = dirname($path);
			print "$dir/$OBJDIR/$name\n";
		}
	}
}

##
## Create C source code to define all static-linked module information.
##
sub do_drvinfo()
{
	my $opts = parse_local_option();
	my $sconfs = $opts->{static_conf};
	my $namebind = $opts->{major_bind};
	my $sysbind = $opts->{sys_bind};
	my $drvalias = $opts->{drvalias};
	my $drvclass = $opts->{drvclass};
	my $dacf = $opts->{dacf_conf};

	my $modinfo = $opts->{modinfo};
	verbose("modinfo = $modinfo\n");
	verbose("name_to_major = $namebind\n");
	verbose("name_to_sysnum = $sysbind\n");
	verbose("driver_aliases = $drvalias\n");
	verbose("driver_classes = $drvclass\n");
	verbose("dacf.conf = $dacf\n");
	usage(2) unless ($namebind and -f $namebind);
	usage(2) unless ($sysbind and -f $sysbind);
	usage(2) unless ($drvalias and -f $drvalias);
	usage(2) unless ($drvclass and -f $drvclass);
	usage(2) unless ($dacf and -f $dacf);

	# Collect all modules to be static-linked.
	my (@info, %infomap);
	foreach my $conf (@$sconfs) {
		verbose("static.conf = $conf\n");
		my $cf = StaticConfParser->new($conf);
		foreach my $mi (@{$cf->modinfo($modinfo)}) {
			my $name = $mi->name();
			my $m = $infomap{$name};
			if ($m) {
				my $mf = $m->path();
				my $mfile = $mi->path();
				die "Module name \"$name\" is duplicated: " .
					"$mf, $mfile\n";
			}
			next if ($mi->isRelocatable());
			$infomap{$name} = $mi;
			push(@info, $mi);
		}
	}

	# Check whether all dependencies are configured as static link module.
	my %fatal;
	foreach my $mi (@info) {
		my $name = $mi->name();
		foreach my $dp (@{$mi->depends()}) {
			my $modname = basename($dp);
			my $dmi = $infomap{$modname};
			unless ($dmi) {
				$fatal{$dp} = $name;
				next;
			}
			next if (%fatal);
			$dmi->depended($mi);
			my $found;
			foreach my $ns (@{$dmi->namespace()}) {
				if ($dp eq "$ns/$modname") {
					$found = 1;
					last;
				}
			}
			unless ($found) {
				my $nsp = '"' .
					join('", "', @{$dmi->namespace()}) .
					'"';
				die "Although module \"$name\" requires " .
					"\"$dp\", module \"$modname\" " .
					"belongs to\nnamespace $nsp.\n";
			}
		}
	}
	if (%fatal) {
		my $msg = <<OUT;
The following modules must be configured as static link module because
static module depends on.

OUT
		$msg .= sprintf("  %-20s  %s\n", "Module", "Referenced From");
		foreach my $dp (sort (keys %fatal)) {
			$msg .= sprintf("  %-20s  %s\n", $dp, $fatal{$dp});
		}
		die "$msg\n";
	}

	# Dependency loop check.
	foreach my $mi (@info) {
		$mi->checkDependency();
	}

	# Dump module definitions.
	my $nmod = scalar(@info);
	print <<OUT;
/*
 * This file is auto-generated. Do NOT edit!!
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/dacf.h>
#include <sys/dacf_impl.h>

/* Number of static modules */
const int  static_modules_count = $nmod;

OUT
	foreach my $mi (@info) {
		my $infoname = static_info_name($mi->name());
		print <<OUT;
extern mod_static_t  $infoname;
OUT
	}

	print <<OUT;

const mod_static_t  *static_modules[] = {
OUT
	foreach my $mi (@info) {
		my $infoname = static_info_name($mi->name());
		print <<OUT;
	&$infoname,
OUT
	}
	print <<OUT;
};
OUT

	# Dump name_to_major file.
	require StaticTool::StructDumper;
	import StaticTool::StructDumper;
	my $dumper = StaticTool::StructDumper->new();
	my $majmap = $dumper->dumpBindingFile($namebind, "static_majbind");

	# Dump name_to_sysnum file.
	$dumper->dumpBindingFile($sysbind, "static_sysbind");

	# Dump driver_aliases file.
	require StaticTool::DriverAliasesParser;
	import StaticTool::DriverAliasesParser;	
	my $drva = StaticTool::DriverAliasesParser->new($drvalias);
	$drva->dumpStruct($majmap);

	# Dump driver_classes file.
	require StaticTool::DriverClassesParser;
	import StaticTool::DriverClassesParser;
	my $drvc = StaticTool::DriverClassesParser->new($drvclass);
	$drvc->dumpStruct($majmap);

	# Dump dacf.conf file.
	require StaticTool::DacfConfParser;
	import StaticTool::DacfConfParser;
	my $da = StaticTool::DacfConfParser->new($dacf);
	$da->dumpStruct();
}

##
## Create dtrace probe entry point.
##
sub do_dtracestubs()
{
	my ($nm);
	usage(2) unless (GetOptions('n|nm=s' => \$nm));
	my $vmunix = shift(@ARGV);
	usage(2) unless ($vmunix and -f $vmunix);
	$nm = find_program("nm") unless ($nm);

	my (%stubs);
	my $callback = sub {
		my ($line) = @_;

		chomp($line);
		my (@col) = split(/\s*\|\s*/, $line);
		return unless (@col == 8 and $col[6] eq 'UNDEF');

		my $sym = $col[7];
		if ($sym =~ /^$SDT_PREFIX(.*)$/) {
			# This is a probe point.
			$stubs{$sym} = $1;
			return;
		}
	};

	# Invoke nm and collect dtrace probe point.
	read_from_process($callback, $nm, '-x', $vmunix);

	print <<OUT;
/* This file is auto-generated. Do NOT edit!! */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sdt.h>
#include <sys/tnf_probe.h>

/* Dtrace probe points */
OUT
	# Create dummy entry point function.
	my (@entries) = (sort keys(%stubs));
	my (@sdt, @sdtproto, $head);
	require StaticTool::StructDumper;
	import StaticTool::StructDumper;
	my $dumper = StaticTool::StructDumper->new();
	for (my $i = 0; $i < @entries; $i++) {
		my $ent = $entries[$i];
		my $sdtname = $stubs{$ent};
		print <<OUT;
void $ent(void) {}
OUT
		my $name = sprintf("static_sdt_probe_%d", $i);
		$head = '&' . $name unless ($head);
		my $next = ($entries[$i + 1])
			? sprintf("&static_sdt_probe_%d", $i + 1)
			: C_NULL;
		push(@sdtproto, "const static sdt_probedesc_t  $name;\n");

		push(@sdt, "const static sdt_probedesc_t $name = {\n");
		$dumper->dumpMember(stringfy($sdtname), "sdpd_name",
				    undef, \@sdt);
		$dumper->dumpMember('(unsigned long)' . $ent, "sdpd_offset",
				    undef, \@sdt);
		$dumper->dumpMember('(sdt_probedesc_t *)' . $next, "sdpd_next",
				    undef, \@sdt);
		push(@sdt, "};\n");
	}

	$head = C_NULL unless ($head);
	my $proto = join('', @sdtproto);
	my $def = join('', @sdt);
	print <<OUT;

/* SDT entries */
$proto

$def

const sdt_probedesc_t *static_sdt_probedesc = $head;

/*
 * The head of TNF weak symbol list.
 * We set 1 as value just to put them into data section.
 * The real value will be patched later.
 */
const tnf_probe_control_t *static_tnf_probelist = (tnf_probe_control_t *)1;
const tnf_tag_data_t *static_tnf_taglist = (tnf_tag_data_t *)1;
OUT
}

sub do_symbind()
{
	my ($output, $mapfile, $objcopy, $nm);
	my (%mydef) = ('o|output=s' => \$output, 'M|mapfile=s' => \$mapfile,
		       'O|objcopy=s' => \$objcopy, 'n|nm=s' => \$nm);
	my $opts = parse_local_option(\%mydef);
	my $input = shift(@ARGV);
	$objcopy = find_program("objcopy") unless ($objcopy);
	$nm = find_program("nm") unless ($nm);

	my $sconf = $opts->{static_conf};
	my $conf = $sconf->[0] if (@$sconf);
	my $modinfo = $opts->{modinfo};
	my $dir = $opts->{builddir} || getcwd();
	verbose("static.conf = $conf\n");
	verbose("modinfo = $modinfo\n");
	verbose("build_directory = $dir\n");
	verbose("input file = $input\n");
	verbose("output file = $output\n");
	verbose("mapfile = $mapfile\n");
	verbose("objcopy = $objcopy\n");
	verbose("nm = $nm\n");
	usage(2) unless ($input and -f $input);
	usage(2) unless ($mapfile and -f $mapfile);

	my %forceglobal;
	my $loader = StaticTool::PackageLoader->new();
	if ($conf and -f $conf and $modinfo and -f $modinfo) {
		my $cf = StaticConfParser->new($conf);
		if ($cf->isStatic($dir)) {
			my $mi = $loader->modInfoInstance($modinfo);
			next if ($mi->isRelocatable());
			my $prefix = $mi->entryPointPrefix();

			# Module entry points must be global.
			foreach my $func (qw(_init _fini _info)) {
				my $f = $prefix . $func;
				$forceglobal{$f} = 1;
			}

			# Module information must be global.
			my $infoname = static_info_name($mi->name());
			$forceglobal{$infoname} = 1;
		}
	}

	# Parse mapfile.
	my $parser = MapfileParser->new($mapfile);

	# Invoke nm and collect symbols to be localized.
	my (%localize, @args);
	my $callback = sub {
		my ($line) = @_;

		chomp($line);
		my (@col) = split(/\s*\|\s*/, $line);
		if (@col == 8 and $col[4] eq 'GLOB' and $col[6] ne 'UNDEF') {
			my $sym = $col[7];
			verbose("Global symbol[$sym]\n");
			if ($parser->isLocalized($sym) and
			    !$forceglobal{$sym} and !$localize{$sym}) {
				$localize{$sym} = 1;
				verbose("Localize symbol[$sym]\n");
				push(@args, '-L', $sym);
			}
		}
	};
	read_from_process($callback, $nm, '-x', $input);
	return unless (@args);

	# Localize symbol using objcopy.
	my $dst = $input;
	push(@args, $input);
	if ($output) {
		push(@args, $output);
		$dst = $output;
	}

	my $hook = sub {
		unlink($dst) unless ($dst);
	};
	push(@EXIT_HOOK, $hook);
	run($objcopy, @args);
	undef $dst;
}

sub do_test()
{
	my $inverse;
	my (%mydef) = ('N|not' => \$inverse);
	my $opts = parse_local_option(\%mydef);
	my $sconf = $opts->{static_conf};
	my $conf = $sconf->[0] if (@$sconf);
	my $dir = $opts->{builddir} || getcwd();
	my $static;

	verbose("static.conf = $conf\n");
	verbose("build_directory = $dir\n");
	verbose("inverse = " . (($inverse) ? "yes" : "no") . "\n");

	if ($conf and -f $conf) {
		my $cf = StaticConfParser->new($conf);
		$static = $cf->isStatic($dir);
	}

	if (@ARGV) {
		$static = !$static if ($inverse);
		if ($static) {
			print join(' ', @ARGV), "\n";
		}
	}
	else {
		if ($static) {
			print "static\n";
		}
		else {
			print "dynamic\n";
		}
	}
}
EOF
