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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

=head1 NAME

I<utstune> - Administration tool for kernel build-time tunable parameters.

=head1 SYNOPSIS

B<utstune> [-cmv6] [<param-name> ...]

B<utstune> [-f6] -d [<param-name> ...]

B<utstune> [-f6] -s <param-name> <param-value>

B<utstune> [-6] -S

B<utstune> [-6] -L

B<utstune> [-Q] [<query>]

=head1 DESCRIPTION

I<utstune> manages tunable parameters that are used when kernel
compilation. I<utstune> must be invoked under OpenSolaris build environment
activated by I<bldenv>.

If neither B<-d> nor B<-s> option is specified, B<utstune> prints parameter
information to stdout. If list of parameter name is specified to argument,
only information of them will be printed. If no argument is specified,
all parameters will be printed. The "*" mark will be printed if a value is
set to the parameter using B<-s> option.

=head1 OPTIONS

I<utstune> takes the following optionss:

=over 4

=item B<-6>|B<--64>

Choose 64-bit kernel environment.

If this option is specified, I<utstune> sees parameters for 64-bit kernel.
Otherwise 32-bit kernel environment.

=item B<-c>|B<--changed>

Show only parameters that have value set by B<-s> option.

This options is ignored if B<-d> or B<-s> option is specified.

=item B<-m>|B<--module>

Show parameters per module. If this option is omitted, all parameters are
sorted in dictionary order.

This options is ignored if B<-d> or B<-s> option is specified.

=item B<-v>|B<--verbose>

Show verbose information of parameters.

This options is ignored if B<-d> or B<-s> option is specified.

=item B<-s>|B<--set>

Set value of tunable parameter.

This option requires two argument, the first is a parameter name, and the
second is new value for the parameter.

=item B<-d>|B<--default>

Reset tunable parameter to the default value.

If parameter names are specified to argument, only the specified parameters
are changed, otherwise all parameters.

=item B<-f>|B<--force>

This option affects B<-s> and B<-d>.

This option suppresses all confirmation messages and forces the change of
tunable parameter.

=item B<-S>|B<--static-module>

Show kernel modules to be static-linked.

Note that this option only shows modules that are dynamically configured
as static link.

=item B<-L>|B<--list-module>

List all module directories.

=item B<-Q>|B<--query>

Tell I<utstune> to run as query mode, that evaluates the specified query.
This option is used to test the current value of parameters.

If this option is given, I<utstune> takes an query from argument.
If argument is not specified, it takes from standard input.
I<utstune> indicates the result of evaluation by its exit status.

=item B<-q>|B<--query-server> I<channel>

Run as query server mode.
Query server mode receives command from B<utquery> command,
and then it returns result of the given query.

I<channel> is a string used to distinguish connection.
Query server accepts queries from B<utquery> command with specifying
the same channel.

=item B<-T>|B<--timeout> I<timeout>

How long (in seconds) a query server can be idle.
If this option is specified with B<-q> option, B<utstune> query server
quits if it becomes idle in I<timeout> seconds.

Default value is zero, that means B<utstune> query server waits for
a query forever.

=item B<--help>

Print help message.

=item B<--manual>

Print this document.

=back

=head1 INTERNAL OPTIONS

The following options are used in kernel Makefile.
Do not invoke them from command-line.

=over 4

=item B<--compile>

Compile parameter definition.
This option is used from kernel Makefile.

This option requires three arguments. The first argument is directory name
that contains kernel object files, aka OBJS_DIR in kernel Makefile.
The second is a file path that contains definitions of tunable parameters.
The third is output filenaame to store current configuration. 

If output file already exists, I<utstune> tries to merge changes of
parameter definition to the current configuration.
The current value will be reset to the default value if I<utstune> failed
to merge changes.

Compilation checks timestamp (mtime) of definition and output file.
If output file is newer than definition file, B<--compile> option does nothing.
If definition file doesn't exist, B<--compile> option creates output file
that contains no option.

=item B<--platform> I<platform-spec>

Specify target platform.
I<platform-spec> is used as identifier of platform,
that will be used as parameter scope identifier.

B<utstune> shows parameters for the specified platform.
If B<--platform> is specified with B<--compile> option,
it means that all parameters in the file are platform-specific.

=item B<--import> I<scope-spec>

Specify outer parameter scope to be imported.

I<spec> is a scope identifier that determines scope of parameter.
Multiple B<--import> options are acceptable.

This option is ignored unless B<--compile> or B<--makeopt-add> option
is specified.

=item B<--header>

Generate header file that contains current configuration.
This option is used from kernel Makefile.

This option requires two arguments. The first argument is a file path
that contains current configuration generated by B<--compile> option.
The second argument is output filename to store current parameters
in cpp format.

If configuration file is not present, this option creates empty header file.

=item B<--remove>

Remove definitions of option.
This option is used from kernel Makefile.

This option requires definition file path as argument,
and removes all definitions related to the specified definition file.

=item B<--depend> I<deffile>

Print source file path for parameter definition.
I<deffile> should be a pathname to parameter definition file.
This option is used to control make depend for parameter build.

This option requires one argument, directory name that contains kernel
object files, aka OBJS_DIR in kernel Makefile.

=item B<--init>

Initialize environment for tunable parameters.

This option requires build directory that contains kernel sources as
argument.

=item B<--clobber>

Remove all files used by I<utstune>.

=item B<--makeopt-add> I<makeopt>

Set dynamic Makefile configuration script.

If this option is used, I<makeopt> is evaluated when tunable parameter
is changed, and configure additional Makefile for the build directory
where the I<makeopt> is located.

=item B<--makeopt-remove> I<makeopt>

Remove setting for dynamic Makefile configuration script.

=item B<--makeopt> I<makeopt>

Dump dynamic-configured Makefile configuration.
Object directory name must be specified to argument.

=item B<-R>|B<--reset>

Reset parameter DB.
This option affects only if B<--init> is specified.

=item B<-C>|B<--clean>

Clean up current configuration before initialization.
This option affects only if B<--init> is specified.

=item B<-D>|B<--deffile> I<deffile-name>

Specify filename that contains definitions of parameter.
This option is used to find parameter definitions by B<--init> option.

"modtune" and "makeopt" are used if omitted.

=item B<-M>|B<--make> I<make-path>

Specify path to make(1S) command.
This option is used to initialize parameter environment by B<--init> option.
Note that GNU make can't be used as make command.

"make" is used if omitted.

=back

=head1 FORMAT OF OPTION DEFINITION

This section describes the format of tunable parameter definition.

Each tunable parameter must be declared by the following form:

  option <type> <option-name> {
          <property>:  <value>;
  }

<option-name> is a name of tunable parameter. It must start with a capital
alphabet (A-Z), and must consist of capital alphabet, digit, and '_'.
option block can contain statement to define parameter. Each statement in
option block must be a <property> and <value> separated by colon, and
statement must be ended with semicolon.

Each tunable parameter has a data type specified by <type>.
<type> must be one of followings:

=over 4

=item B<string>

String constant. The value of option must be a string quoted by '"'.
Like C language, '"' character in a string must be escaped by '\',
such as "\"".

=item B<boolean>

Boolean value, The value of this option must be I<true> or I<false>.
This option is defined by cpp if I<true> is set, otherwise undefined.

Let's assume that the following definition.

  option boolean BOOL_OPT {
         ....
  }

If I<true> is set for this parameter, "#define BOOL_OPT  1" is applied
to kernel build processes. If I<false> is set, "#undef BOOL_OPT" is applied.

=item B<int>

32 bit integer value. This type can take a value from -2147483648 to
2147483647 as long as the value satisfies other constraints.

=item B<uint>

32 bit unsigned integer value. This type can take a value from 0 to
4294967295 as long as the value satisfies other constraints.

=item B<long>

32 bit long value. This type is similar to B<int> type, but the value of
this type is defined as long constant, such as '1024L'.

=item B<ulong>

32 bit unsigned long value. This type is similar to B<uint> type,
but the value of this type is defined as long constant, such as '1024UL'.

=item B<int64>

64 bit integer value. This type can take a value from -9223372036854775808 to
9223372036854775807 as long as the value satisfies other constraints.
The value of this type is defined as long long constant, such as '1024LL'.

=item B<uint64>

64 bit integer value. This type can take a value from 0 to
18446744073709551615 as long as the value satisfies other constraints.
The value of this type is defined as unsigned long long constant,
such as '1024ULL'.

=back

Numerical value can be specified by decimal, octal, or hexadecimal.
If a value starts with I<0>, it's treated as octal, I<0x> as hexadecimal,
otherwise decimal value.

Note that all numerical types treat a value that doesn't start with '-' as
positive value. For example, -1 and 0xffffffff is not identical.
0xffffffff is always treated as 4294967295.

Each option block must contain at least two properties, I<default> and
I<description>. I<default> declares the defalut value of the option.
And brief description of the option must be set to I<description>.
The value of I<description> must be a quoted string.

The following example declares tunable parameter INT_OPT, and the default
value is 1024.

  option int INT_OPT
  {
          default:      1024;
          description:  "Integer example.";
  }

Each option can have one or more constraints. Constraints can be defined
by the following properties. If more than two constrains are defined,
the value of option must satisfy all of them.

=over 4

=item B<max>

Defines the upper limit of the value. The value larger than this limit can't
set to this option.

Note that this constraint can't be defined for non-numerical type,
such as B<string>, B<boolean>.

=item B<min>

Defines the lower limit of the value. The value smaller than this limit can't
set to this option.

Note that this constraint can't be defined for non-numerical type,
such as B<string>, B<boolean>.

=item B<power>

Enforce a power of N value on the option value.
The value of B<power> property must be larger than zero.

Note that the followings:

=over 3

=item -

This constraint can't be defined for non-numerical type,
such as B<string>, B<boolean>.

=item -

Zero and negative value never satisfies this constraint.

=back

=item B<align>

Enforce a value that can be divided by the specified value without remainder
on the option value. The value of B<align> property must be larger than zero.

Note that this constraint can't be defined for non-numerical type,
such as B<string>, B<boolean>.

=item B<candidates>

Enforce a value listed by property on the option value.
This constraint can be defined to all option type, but B<boolean> type.

Candidates are defined by a list of value separated by comma.
For example, the following option can take one of "option1", "option2",
"option3", but can't take other value.

  option string STR_OPT {
          default:     "option1";
          description: "String parameter";
          candidates:  "option1", "option2", "option3";
  }

Unlike other constraints, other constraints will be ignored if a value
satisfies B<candidates> constraint. For example, the folliwing option can
take a value from 0x1000 to 0x2000, or zero.

  option int INT_OPT {
          default:     0;
          description: "Integer parameter";
          min          0x1000;
          max          0x2000;
          candidates:  0;
  }

=back

B<default> and all constraint parameter names can take kernel build type
as suffix. Assuming that the following option:

  option int INT_OPT {
          default:           0x1100;
          default_obj32:     0x1000;
          description:       "Integer parameter";
          min                0x1100;
          min_obj32:         0x1000;
          max                0x2000;
  }

As for 32-bit non-DEBUG kernel, the default value and the lower limit are
0x1000. For the other kernels, they are 0x1100.

In addition, some definition may have the following propertiy:

=over 4

=item B<rebuild>

Denotes that all kernel modules should be rebuilt if the option is changed.
The B<rebuild> property takes boolean value.
Default value is false.

For example, the following definition denotes that all kernel modules
should be rebuilt if the value of INT_OPTION is changed.

  option int INT_OPT {
          default:     0;
          description: "Integer parameter";
          rebuild:     true;
  }

=item B<also-define>

Denotes that also the specified option should be defined as true
if the option is defined true.
The B<also-define> property takes other option name as value like this:

  option boolean BOOL_OPT {
          default:       false;
          description:   "Boolean parameter";
          also-define:   BOOL_OPT2;
  }

Above example denotes that BOOL_OPT2 should be defined as true
if the value of BOOL_OPT is defined as true.

B<also-define> property have the following constraints:

=over 3

=item -

Only boolean option can have B<also-define> property.

=item -

The value of B<also-define> must be a name of boolean option.

=item -

The boolean option specified to the value of B<also-define> must be
defined in the same configuration file.

=back

Note that B<also-define> works when the value of boolean option is changed
to true. Even if the option that has B<also-define> is configured as false,
no other option is changed.

=back

=head1 EXIT STATUS

If B<-Q> option is specified, the following exit values are returned:

=over 3

=item 0

The query evaluated to true.

=item 1

The query evaluated to false.

=item >1

An error was detected.

=back

If B<-Q> option is not specified, the following exit values are returned:

=over 3

=item 0

Suceeded.

=item >0

An error was detected.

=back

=head1 SEE ALSO

B<bldenv>(1),
B<utquery>(1)

=cut

use strict;
use Cwd qw(abs_path);
use Getopt::Long;
use DirHandle;
use FileHandle;
use File::Basename;
use File::stat;
use POSIX qw(:DEFAULT :sys_wait_h);

use UtsTune;
use UtsTune::PackageLoader;
use UtsTune::DB::Parameter;

use vars qw($PROGNAME @EXIT_HOOK $OUT);

use constant	NORMAL_FORMAT		=> "  %-30s  %s\n";
use constant	VERBOSE_FORMAT		=> "  %16s  %s\n";
use constant	VERBOSE_FORMAT_NONL	=> "  %16s  %s";
use constant	MAX_WIDTH		=> 80;

use constant	MODTUNE		=> 'modtune';
use constant	MODINFO		=> 'modinfo';

use constant	INITTUNE_DEPTH	=> 2;

$PROGNAME = basename($0);

END {
	foreach my $hook (@EXIT_HOOK) {
		&$hook();
	}
}

sub usage($);
sub build_name($$);
sub stamp_update();
sub inittune($$$\@$$);
sub inittune_walk($$$);
sub check_status($$);
sub check_utstree($);
sub static_foreach($$@);
sub static_clobber($);
sub static_touch($);
sub clobber();
sub compile($\@$);
sub makeopt(@);
sub header();
sub removetune();
sub param_set($$$);
sub param_set_impl($$$$$$$$);
sub param_reset($$$);
sub param_reset_impl($$$$\@\%);
sub show_static($$);
sub list_module_dir($$);
sub apply_script(@);
sub apply_static_script($$$);
sub confirm($$);
sub print_option_header();
sub print_option_line($$$);
sub print_option($$$$$);
sub print_verbose($$$);
sub print_candidates($$$);
sub print_fold($$$);
sub print_tune($$$$$);
sub check_environment(%);

## Default object to output message.
{
	package DefaultOutput;

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my $fh = shift || \*STDOUT;

		my $me = {OUT => $fh, BUF => []};
		return bless $me, $class;
	}

	sub delayedPrint
	{
		my $me = shift;
		my (@args) = (@_);

		push(@{$me->{BUF}}, \@args);
	}

	sub flushBuffer
	{
		my $me = shift;

		my $fh = $me->{OUT};
		foreach my $args (@{$me->{BUF}}) {
			$fh->print(@$args);
		}
		$me->{BUF} = [];
	}

	sub flush
	{
		my $me = shift;

		my $fh = $me->{OUT};
		$fh->flush();
	}

	sub print
	{
		my $me = shift;

		$me->flushBuffer();
		my $fh = $me->{OUT};
		$fh->print(@_);
	}

	sub printf
	{
		my $me = shift;

		$me->flushBuffer();
		my $fh = $me->{OUT};
		$fh->printf(@_);
	}

	sub errprint
	{
		my $me = shift;

		print STDERR "\n*** ERROR: ", @_, "\n";
	}

	sub debug
	{
	}
}

## This object is used to output debug message.
{
	package DebugOutput;

	use vars qw(@ISA);
	@ISA = qw(DefaultOutput);

	sub debug
	{
		my $me = shift;

		$me->flushBuffer();
		my $fh = $me->{OUT};
		$fh->print("-- ", @_, "\n");
	}
}

##
## Run make command on background.
##
{
	package MakeRunner;

	use FileHandle;
	use POSIX qw(:DEFAULT :sys_wait_h :errno_h);

	use UtsTune;

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($make, $dir, $mask, @args) = @_;

		$dir = utspath($dir);
		my $me = {MAKE => $make, DIR => $dir, ARGS => \@args,
			  MASK => $mask, PARAM => []};
		$me = bless $me, $class;

		return $me;
	}

	sub setParameter
	{
		my $me = shift;
		my ($param) = @_;

		$me->{PARAM} = $param;
	}

	sub run
	{
		my $me = shift;
		my ($null) = @_;

		my ($erfh, $ewfh) = FileHandle->pipe() or
			die "pipe() failed: $!\n";

		my $pid = fork();
		if (!defined($pid)) {
			die "fork() failed: $!\n";
		}
		elsif ($pid != 0) {
			$me->{ERRFH} = $erfh;
			$me->{PID} = $pid;
			undef $ewfh;
			return;
		}

		my (@args) = (@{$me->{ARGS}}, @{$me->{PARAM}});

		my $dir = $me->getDirectory();
		chdir($dir) or die "chdir($dir) failed: $!\n";

		undef $erfh;
		POSIX::dup2($ewfh->fileno(), STDERR->fileno());
		POSIX::dup2($null->fileno(), STDOUT->fileno()) if ($null);

		exec($me->{MAKE}, @args);
		die "exec failed: $!\n";
	}

	sub getPid
	{
		my $me = shift;

		return $me->{PID};
	}

	sub setStatus
	{
		my $me = shift;
		my ($status) = @_;

		$me->{STATUS} = $status;
	}

	sub getDirectory
	{
		my $me = shift;

		return $me->{DIR};
	}

	sub sync
	{
		my $me = shift;

		my $errfh = $me->{ERRFH};
		return unless ($errfh);

		my $pid = $me->{PID};
		my (@err) = (<$errfh>);
		undef $errfh;
		delete($me->{ERRFH});

		my $mask = $me->{MASK};
		my $oldmask = POSIX::SigSet->new();
		POSIX::sigprocmask(SIG_BLOCK, $mask, $oldmask) or
			die "sigprocmask() failed: $!\n";

		my $status = $me->{STATUS};
		unless (defined($status)) {
			my $ret;
			do {
				$ret = waitpid($pid, 0);
			} while ($ret == -1 and $! == EINTR);
			die "waitpid($pid) failed: $!\n" if ($ret == -1);
			$status = $?;
		}
		POSIX::sigprocmask(SIG_SETMASK, $oldmask) or
			die "sigprocmask() failed: $!\n";

		eval {
			&main::check_status($me->{MAKE}, $status);
		};
		if ($@) {
			my $msg = "$@";
			chomp($msg);
			$me->{ERROR} = $msg;
			$me->{ERROUT} = \@err;
		}
	}

	sub getErrorMessage
	{
		my $me = shift;

		my $msg = $me->{ERROR};
		return undef unless ($msg);
		my $out = $me->{ERROUT};

		my $args = $me->{MAKE} . ' ' . join(' ', @{$me->{ARGS}});
		my $dir = $me->getDirectory();
		my $ret = <<OUT;
*** $dir: $args failed: $msg
OUT
		if (@$out) {
			my $eo = join('', @$out);
			$ret .= <<OUT;

--- Error output:
$eo
---
OUT
		}
		return $ret;
	}
}

##
## Manager class for MakeRunner.
##
{
	package MakeRunnerManager;

	use POSIX;
	use FileHandle;

	use constant	MAX_RUNNER	=> 8;

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($make, $verbose) = @_;

		my $chldmask = POSIX::SigSet->new(SIGCHLD);
		my $me = {MAKE => $make, RUNNER => [], ACTIVE => {},
			  ERROR => [], MASK => $chldmask, PARAM => []};

		unless ($verbose) {
			my $null = FileHandle->new('/dev/null') or
				die "open(/dev/null) failed: $!\n";
			$me->{NULL} = $null;
		}
		return bless $me, $class;
	}

	sub addParameter
	{
		my $me = shift;
		my ($param) = @_;

		push(@{$me->{PARAM}}, $param);
	}

	sub setActive
	{
		my $me = shift;
		my ($r) = @_;

		my $pid = $r->getPid();
		my $mask = $me->{MASK};
		my $oldmask = POSIX::SigSet->new();
		POSIX::sigprocmask(SIG_BLOCK, $mask, $oldmask) or
			die "sigprocmask() failed: $!\n";
		$me->{ACTIVE}->{$pid} = $r;
		POSIX::sigprocmask(SIG_SETMASK, $oldmask) or
			die "sigprocmask() failed: $!\n";
	}

	sub removeActive
	{
		my $me = shift;
		my ($pid, $status) = @_;

		my $mask = $me->{MASK};
		my $oldmask = POSIX::SigSet->new();
		POSIX::sigprocmask(SIG_BLOCK, $mask, $oldmask) or
			die "sigprocmask() failed: $!\n";
		my $r = $me->{ACTIVE}->{$pid};
		delete($me->{ACTIVE}->{$pid});
		$r->setStatus($status);
		POSIX::sigprocmask(SIG_SETMASK, $oldmask) or
			die "sigprocmask() failed: $!\n";
		return $r;
	}

	sub post
	{
		my $me = shift;
		my $dir = shift;

		my $active = $me->{ACTIVE};
		while (scalar(keys(%$active)) > MAX_RUNNER) {
			POSIX::pause();
		}

		my $r = MakeRunner->new($me->{MAKE}, $dir, $me->{MASK}, @_);
		$r->setParameter($me->{PARAM});
		my $null = $me->{NULL};
		push(@{$me->{RUNNER}}, $r);
		$r->run($null);
		if ($null) {
			$me->setActive($r);
		}
		else {
			$r->sync();
			my $err = $r->getErrorMessage();
			die "$err" if ($err);
		}
	}

	sub sync
	{
		my $me = shift;

		foreach my $r (@{$me->{RUNNER}}) {
			$r->sync();
			my $dir = $r->getDirectory();
			my $err = $r->getErrorMessage();
			push(@{$me->{ERROR}}, $err, "\n") if ($err);
		}

		my $err = $me->{ERROR};
		return unless (@$err);

		print STDERR @$err;
		die "Initialization failed.\n";
	}
}

MAIN:
{
	Getopt::Long::Configure(qw(no_ignore_case bundling require_order));

	my ($help, $debug, $compile, $header, $init, $reset, $manual, $env64);
	my (@defname, $make, $verbose, $default, $set, $changed, $plat);
	my ($remove, $force, $module, $clean, $clobber, $static, @scope);
	my ($mkoptadd, $mkoptrm, $mkopt, @switch, $depend, $query, $listmod);
	my ($qserver, $qtimeout);
	usage(2) unless (GetOptions
			 ('debug'	=> \$debug,
			  'help'	=> \$help,
			  'compile'	=> \$compile,
			  'header'	=> \$header,
			  'remove'	=> \$remove,
			  'init'	=> \$init,
			  'clobber'	=> \$clobber,
			  'makeopt-add=s'	=> \$mkoptadd,
			  'makeopt-remove=s'	=> \$mkoptrm,
			  'makeopt=s'	=> \$mkopt,
			  'switch=s'	=> \@switch,
			  'depend=s'	=> \$depend,
			  'q|query-server=s'	=> \$qserver,
			  'T|timeout=i'	=> \$qtimeout,
			  'Q|query'	=> \$query,
			  'D|deffile=s'	=> \@defname,
			  'M|make=s'	=> \$make,
			  'R|reset'	=> \$reset,
			  'C|clean'	=> \$clean,
			  '6|64'	=> \$env64,
			  's|set'	=> \$set,
			  'd|default'	=> \$default,
			  'v|verbose'	=> \$verbose,
			  'c|changed'	=> \$changed,
			  'f|force'	=> \$force,
			  'S|static-module'	=> \$static,
			  'L|list-module'	=> \$listmod,
			  'm|module'	=> \$module,
			  'p|platform=s'	=> \$plat,
			  'import=s'	=> \@scope,
			  'manual'	=> \$manual));

	usage(0) if ($help);

	if ($manual) {
		require Config;
		import Config;

		use vars qw(%Config);

		my $perldoc = $Config{installbin} . '/perldoc';
		exec($perldoc, $0);
		die "exec($perldoc, $0) failed: $!";
	}

	my $exc = 0;

	$exc++ if ($compile);
	$exc++ if ($header);
	$exc++ if ($remove);
	$exc++ if ($init);
	$exc++ if ($set);
	$exc++ if ($default);
	$exc++ if ($static);
	$exc++ if ($listmod);
	$exc++ if ($mkoptadd);
	$exc++ if ($mkoptrm);
	$exc++ if ($depend);
	$exc++ if ($query);
	$exc++ if ($qserver);
	usage(1) if ($exc > 1);

	if ($depend) {
		my $objdir = $ARGV[0];
		die "OBJS_DIR is required as argument.\n" unless ($objdir);

		my $loader = UtsTune::PackageLoader->new();
		my $util = $loader->utilInstance();
		$util->dependency($depend, $objdir);
		last MAIN;
	}

	$OUT = ($debug) ? DebugOutput->new() : DefaultOutput->new();

	my $objdir = get_objdir($env64);
	$OUT->debug("OBJS_DIR = \"$objdir\"");

	$SIG{INT} = sub { die "Interrupted.\n"; };
	$SIG{HUP} = sub { die "Hang up.\n"; };

	if ($query) {
		if (@ARGV) {
			$query = join(' ', @ARGV);
		}
		else {
			$query = join(' ', <STDIN>);
		}
		my $db = UtsTune::DB::Parameter->new();
		$plat = default_platform($db, 1) unless ($plat);

		my $status;
		eval {
			require UtsTune::QueryParser;
			import UtsTune::QueryParser;
			my $dout = $OUT if ($debug);
			my $parser = UtsTune::QueryParser->
				new(DB => $db, PLATFORM => $plat,
				    OBJDIR => $objdir, DEBUG => $dout);
			$status = $parser->evaluate($query);
		};
		if ($@) {
			my $err = "$@";
			chomp($err);
			print STDERR "*** ERROR: $err\n";
			exit 2;
		}
		exit $status;
	}
	if ($qserver) {
		require UtsTune::QueryServer;
		import UtsTune::QueryServer;

		my $db = UtsTune::DB::Parameter->new();
		$plat = default_platform($db, 1) unless ($plat);
		my $dout = $OUT if ($debug);
		eval {
			my $server = UtsTune::QueryServer->
				new(DB => $db, PLATFORM => $plat,
				    OBJDIR => $objdir, DEBUG => $dout,
				    TIMEOUT => $qtimeout, CHANNEL => $qserver);
			$server->run();
		};
		if ($@) {
			my $err = "$@";
			chomp($err);
			print STDERR "*** ERROR: $err\n";
			exit 2;
		}

		exit 0;
	}
	eval {
		if ($init) {
			inittune($objdir, $reset, $clean, @defname, $make,
				 $verbose);
			last MAIN;
		}
		if ($clobber) {
			clobber();
			last MAIN;
		}
		if ($compile) {
			compile($plat, @scope, $objdir);
			last MAIN;
		}
		if ($header) {
			header();
			last MAIN;
		}
		if ($remove) {
			removetune();
			last MAIN;
		}
		if ($set) {
			param_set($plat, $objdir, $force);
			last MAIN;
		}
		if ($default) {
			param_reset($plat, $objdir, $force);
			last MAIN;
		}
		if ($static) {
			show_static($plat, $objdir);
			last MAIN;
		}
		if ($listmod) {
			list_module_dir($plat, $objdir);
			last MAIN;
		}
		if ($mkoptadd) {
			makeopt($mkoptadd, 'add', @scope);
			last MAIN;
		}
		if ($mkoptrm) {
			makeopt($mkoptrm, 'remove');
			last MAIN;
		}
		if ($mkopt) {
			die "Object directory must be specified.\n"
				unless (@ARGV == 1);
			makeopt($mkopt, 'dump', @ARGV);
			last MAIN;
		}
	};
	if ($@) {
		my $err = "$@";

		chomp($err);
		$OUT->errprint($err, "\n");
		exit 1;
	}

	# Show parameters.
	my $db = UtsTune::DB::Parameter->new();
	$plat = default_platform($db, 1) unless ($plat);
	my $bldname = build_name($plat, $objdir);

	# Check whether utstune environment is initialized or not.
	my $loader = UtsTune::PackageLoader->new();
	check_environment(loader => $loader, db => $db, platform => $plat,
			  objdir => $objdir);

	if ($changed) {
		$OUT->delayedPrint(<<OUT);
Modified tunable parameters for $bldname
OUT
	}
	else {
		$OUT->delayedPrint(<<OUT);
Tunable parameters for $bldname
OUT
	}

	my $nameMap;
	if (@ARGV) {
		# Check whether the specified parameter name is valid.
		$nameMap = {};
		foreach my $name (@ARGV) {
			if ($db->getModTune($name, $plat)) {
				$nameMap->{$name} = 1;
			}
			else {
				$OUT->errprint("Unknown option name: ", $name);
			}
		}
	}

	unless ($module) {
		# Dump all parameters in dictionary order.
		$OUT->delayedPrint("\n") if ($verbose);
		my $names;
		if ($nameMap) {
			$names = [sort keys(%$nameMap)];
		}
		else {
			$names = $db->getAllOptions($plat, 1);
		}

		print_option_header() unless ($verbose);
		my $out;
		my %psmap;
		foreach my $name (@$names) {
			my $opath = $db->getCurFile($name, $plat, $objdir);
			unless ($opath) {
				$OUT->errprint("Unknown option name: ", $name);
				next;
			}
			unless (-f $opath) {
				my $rpath = dirname(dirname($opath));
				$rpath =~ s,^\Q$UtsTune::UTS\E/*,,;
				$OUT->errprint("Configuration for $rpath ",
					       "doesn't exist. ",
					       "Try \"make tune-init\".\n");
				exit 1;
			}
			my $parser = $psmap{$opath};
			unless ($parser) {
				$parser = $loader->parserInstance
					(FILE => $opath, PRINT => $OUT);
				$psmap{$opath} = $parser;
			}
			$out |= print_tune($parser, $objdir, $verbose,
					   $changed, $name);
		}
		$OUT->print("\n") if (!$verbose and $out);
		last MAIN;
	}

	# Dump parameters per module.
	$OUT->delayedPrint("\n");

	# Determine path to architecture-specific parameter configuration.
	my $archList = $db->getArchPath();

	# Determine path to platform-specific parameter configuration.
	my $platList = $db->getPlatPath();

	# Walk through parameter DB, and pick up configuration files.
	my %confMap;
	my $walker = sub {
		my ($key, $value) = @_;

		$key = $db->getOptionName($key, $plat) or return;
		my $map = $confMap{$value};
		unless ($map) {
			my $opath = $db->getCurFile($key, $plat, $objdir);
			unless (-f $opath) {
				my $rpath = dirname(dirname($opath));
				$rpath =~ s,^\Q$UtsTune::UTS\E/*,,;
				$OUT->errprint("Configuration for $rpath ",
					       "doesn't exist. ",
					       "Try \"make tune-init\".\n");
				exit 1;
			}
			my $rpath = $value;
			$rpath =~ s,^\Q$UtsTune::UTS\E/*,,;
			$map = {PARSER => $loader->parserInstance
					(FILE => $opath, PRINT => $OUT),
				RPATH => $rpath, OPTIONS => []};
			$confMap{$value} = $map;
		}

		my $opts = $map->{OPTIONS};
		my $parser = $map->{PARSER};
		unless ($changed and !$parser->hasValue($key)) {
			push(@{$opts}, $key);
		}
	};
	$db->walk($walker);

	my $func = sub {
		my ($map, $nameMap) = @_;

		print_option_header() unless ($verbose);
		my $parser = $map->{PARSER};
		return print_tune($parser, $objdir, $verbose, $changed,
				  $nameMap);
	};

	my $specFunc = sub {
		my ($list, $nameMap) = @_;

		foreach my $l (@$list) {
			my ($key, $path) = (@$l);

			my $map = $confMap{$path};
			my $rpath = $map->{RPATH};
			next unless ($map and $map->{OPTIONS} and
				     @{$map->{OPTIONS}});
			delete($confMap{$path});
			$OUT->print("$key-specific parameters ($rpath)\n");
			&$func($map, $nameMap);
			$OUT->print("\n") unless ($verbose);
		}
	};

	# At first, dump architecture-specific parameters.
	&$specFunc($archList, $nameMap);

	# Dump platform-specific parameters.
	&$specFunc($platList, $nameMap);

	# Dump module-specific parameters.
	while (my ($path, $map) = each(%confMap)) {
		my $rpath = $map->{RPATH};
		next unless (@{$map->{OPTIONS}});

		$OUT->print("Module specific parameters ($rpath)\n");
		&$func($map, $nameMap);
		$OUT->print("\n") unless ($verbose);
	}
}

sub usage($)
{
	my ($status) = @_;

	my $out = ($status) ? \*STDERR : \*STDOUT;
	print $out <<OUT;
Usage: $PROGNAME [-c][-m][-v][-6] [<param-name> ...]
Usage: $PROGNAME [-f][-6] -d [<param-name> ...]
Usage: $PROGNAME [-f][-6] -s <param-name> <param-value>
Usage: $PROGNAME -S
Usage: $PROGNAME -L
Usage: $PROGNAME -Q [<query>]
Usage: $PROGNAME -q channel

OUT

	if ($status) {
		print $out <<OUT;
Try "$PROGNAME --help".
OUT
	}
	else {
		print $out <<OUT;
Options:

  -c|--changed
      Show only parameters that have different value from the default.

  -m|--module
      Show parameters per module.

  -v|--verbose
      Show verbose information of parameters.

  -s|--set
      Set value of tunable parameter.

  -d|--default
      Reset tunable parameter to the default value.

  -S|--static-module
      Show kernel modules to be static-linked.

  -f|--force
      Supress all confirmation messages when the parameter is changed.

  -6|--64
      Choose 64-bit kernel environment.

  --manual
      Print online manual for utstune(1).

OUT
	}

	exit $status;
}

sub build_name($$)
{
	my ($plat, $objdir) = @_;

	my ($bits, $build);
	if ($objdir =~ /64$/) {
		$bits = 64;
	}
	else {
		$bits = 32;
	}
	if ($objdir =~ /^debug/) {
		$build = 'debug';
	}
	else {
		$build = 'release';
	}

	my $name = "$bits-bit $build kernel";
	$name .= sprintf(" (%s,%s)", $UtsTune::MACH, $plat) if ($plat);

	return $name;
}

sub stamp_update()
{
	my $t = time();
	utime($t, $t, $UtsTune::INIT_STAMP);
}

##
## Initialize utstune environment.
##
sub inittune($$$\@$$)
{
	my ($objdir, $reset, $clean, $deffiles, $make, $verbose) = @_;

	usage(1) if (@ARGV == 0);

	chdir($UtsTune::UTS) or die "chdir($UtsTune::UTS) failed: $!";

	require UtsTune::MakeOption;
	import UtsTune::MakeOption;
	$deffiles = [MODTUNE, UtsTune::MakeOption::MAKEOPT()]
		unless (@$deffiles);
	my (%defmap) = map {$_ => 1} @$deffiles;
	$make = 'make' if (!$make or $make =~ /dmake$/);

	my $loader = UtsTune::PackageLoader->new();
	my $util = $loader->utilInstance();
	my ($reason, $doreset) = $util->checkFormatVersion();
	if ($reason) {
		my $rmsg = ' Reset database.' if ($doreset);
		$OUT->print(<<OUT);
*** NOTICE: $reason.$rmsg
OUT
		# Create timestamp file.
		$util->putFormatVersion();
		$reset = $doreset;
	}

	# Update timestamp.
	stamp_update();

	my (@target) = qw(tune-init);
	if ($reset or $clean) {
		unshift(@target, 'tune-clean');

		# This may fail on initialization.
		eval {
			my $db = UtsTune::DB::Parameter->new();
			static_clobber($db);
		};
	}

	my $hook = sub {
		if ($? == 0) {
			# Update timestamp.
			stamp_update();
		}
		else {
			unlink($UtsTune::INIT_STAMP);
		}

		# Remove initialization key from DB.
		eval {
			my $db = UtsTune::DB::Parameter->new(1);
			$db->initKey(value => 0);
		};
	};
	push(@EXIT_HOOK, $hook);

	my $runner = MakeRunnerManager->new($make, $verbose);

	# makeopt feature must be disabled while initialization.
	$runner->addParameter('MAKEOPT_INIT=1');

	unless ($verbose) {
		$SIG{CHLD} = sub {
			my $pid;
			do {
				$pid = wait();
			} while ($pid == -1 and $! == EINTR);
			die "wait() failed: $!\n" if ($pid == -1);
			my $status = $?;

			$runner->removeActive($pid, $status);
		};
	}
	my $db = UtsTune::DB::Parameter->new(1);
	my $oldplat = $db->getPlatforms();
	my $removed_mkopts;
	if ($reset) {
		$db->reset();

		# Set initialization key into DB.
		$db->initKey(value => 1);

		undef $db;
	}
	else {
		# Set initialization key into DB.
		$db->initKey(value => 1);

		# Eliminates removed modtune.
		my @removed;
		push(@removed, @{$db->getRemovedModTune(1)});
		$removed_mkopts = $db->getRemovedMakeOpt(1);
		push(@removed, @$removed_mkopts);
		undef $db;

		my $cwd = getcwd();
		my %done;
		foreach my $path (@removed) {
			my $dir = dirname($path);
			next if ($done{$dir});
			$done{$dir} = 1;
			chdir($dir) or die "chdir($dir) failed: $!\n";
			my $rpath = utspath($path);
			my $rdir = dirname($rpath);
			$OUT->print("==> $rdir: $make ",
				    join(' ', @target), "\n");
			$runner->post($dir, @target);
		}
		chdir($cwd) or die "chdir($cwd) failed: $!\n";
	}

	my (%done, %modinfo);
	my $wanted = sub {
		my ($dir, $fname) = @_;

		$modinfo{$dir} = 1 if ($fname eq MODINFO);

		return unless ($defmap{$fname});
		return unless (-f "$dir/Makefile");
		return if ($done{$dir});
		$done{$dir} = 1;

		$OUT->print("==> $dir: $make ", join(' ', @target), "\n");
		$runner->post($dir, @target);
	};

	foreach my $dir (@ARGV) {
		inittune_walk($dir, 0, $wanted);
	}
	my (@moddirs) = keys(%modinfo);

	$runner->sync();

	foreach my $mkopt (@$removed_mkopts) {
		makeopt($mkopt, 'remove');
	}

	# Apply dynamic configuration script.
	apply_script();

	# Put initialization key into DB to denote the platform build
	# environment is initialized.
	$db = UtsTune::DB::Parameter->new(1);
	my $newpl = $db->getPlatforms();
	foreach my $objdir (get_build_objdirs()) {
		foreach my $plat (@$newpl) {
			next unless ($db->isValidPlatform($plat, $objdir));
			$db->initKey(platform => $plat, objdir => $objdir,
				     value => 1);
			$db->setModuleDir($plat, \@moddirs) if (@moddirs);
		}
	}
}

sub inittune_walk($$$)
{
	my ($dirpath, $depth, $wanted) = @_;

	return if ($depth >= INITTUNE_DEPTH);

	my $dirp = DirHandle->new($dirpath) or
		die "opendir($dirpath) failed: $!\n";

	# Collect files and directories under the given directory.
	my (@dirs, @files);
	while (my $dp = $dirp->read()) {
		next if ($dp eq '.' or $dp eq '..');

		my $path = "$dirpath/$dp";
		my $st = lstat($path) or die "lstat($path) failed: $!";
		my $mode = $st->mode();
		if (S_ISDIR($mode)) {
			push(@dirs, $path);
		}
		elsif (S_ISREG($mode)) {
			push(@files, {DIR => $dirpath, FILE => $dp});
		}
	}
	undef $dirp;

	# Call wanted function for each file.
	foreach my $map (@files) {
		&$wanted($map->{DIR}, $map->{FILE});
	}

	$depth++;
	foreach my $dir (@dirs) {
		inittune_walk($dir, $depth, $wanted);
	}
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

sub static_foreach($$@)
{
	my ($db, $method, @arg) = @_;

	if (is_static_unix()) {
		require UtsTune::DB::Static;
		import UtsTune::DB::Static;

		my $pl = $db->getPlatforms();
		foreach my $plat (@$pl) {
			my $architectures = $db->getStaticArch($plat) or next;
			foreach my $arch (@$architectures) {
				foreach my $objdir (get_all_objdirs()) {
					my $sdb = UtsTune::DB::Static->
						new($plat, $arch, $objdir);
					$sdb->$method(@arg);
				}
			}
		}
	}
}

sub static_clobber($)
{
	my ($db) = @_;

	static_foreach($db, 'clobber');
}

sub static_touch($)
{
	my ($db) = @_;

	static_foreach($db, 'updateStamp');
}

sub clobber()
{
	my $db = UtsTune::DB::Parameter->new();

	static_clobber($db);
	$db->clobber();

	unlink($UtsTune::INIT_STAMP);
}

sub compile($\@$)
{
	my ($plat, $scopelist, $envobjdir) = @_;

	die "Invalid character in platform identifier.\n"
		if ($plat and is_local_scope($plat));
	usage(1) unless (@ARGV == 3);
	my ($objdir, $input, $output) = @ARGV;

	$OUT->debug("Tunedef compile: plat=\"$plat\": $objdir: ",
		    "$input => $output");

	# Do nothing if build environment does not match.
	return unless (is_same_build_type($envobjdir, $objdir));

	# Check whether utstune environment is initialized or not.
	my $db = UtsTune::DB::Parameter->new(1);
	my $loader = UtsTune::PackageLoader->new();
	check_environment(loader => $loader, db => $db, platform => $plat,
			  objdir => $objdir);

	my $parser;
	if (-f $output) {
		# Compiled file exists.
		$parser = $loader->parserInstance
			(FILE => $input, ISDEF => 1, PRINT => $OUT);
		my $cur = $loader->parserInstance
			(FILE => $output, PRINT => $OUT);

		# Try to merge definitions.
		$OUT->debug("Merge $output");
		$parser->merge($cur, $plat, $objdir, $db);
	}
	else {
		$OUT->debug("Create new tune file: $output");
		$parser = $loader->parserInstance
			(FILE => $input, ISDEF => 1, PRINT => $OUT);
	}

	# Check whether input file has been modified after "tune-init".
	$parser->updateCheck($objdir);

	my $hook = sub {
		unlink($output) unless ($? == 0);
	};
	push(@EXIT_HOOK, $hook);

	my $modtune = $parser->getPath();
	my $inmtime = $parser->getMtime();
	$parser->output($output, $plat, $objdir, $db);
	$db->addModTune($modtune, $inmtime, $scopelist);

	my $explat = $parser->getPlatformScope();
	my $static = $parser->getStatic();
	if ($static) {
		die "\"static\" block must be defined in platform modtune.\n"
			unless ($explat);
		my $file = $static->getFile();
		my $arch = $static->getArchitectures();
		my $script = abs_path($file);
		die "Static tune file doesn't exist: $script\n"
			unless (-f $script);
		$db->addStaticScript($script, $explat, $arch);
	}
	elsif ($explat) {
		$db->removeStaticScript($explat);
	}

	$OUT->debug("Tunedef compile: done");
}

sub makeopt(@)
{
	my ($script, $func, @args) = @_;

	require UtsTune::MakeOption;
	import UtsTune::MakeOption;
	my $mopt = UtsTune::MakeOption->new();
	my $path = abs_path($script);
	$mopt->$func($path, @args);
}

sub header()
{
	usage(1) unless (@ARGV == 2);
	my ($input, $output) = @ARGV;

	unless (-f $input) {
		# No parameter is defined.
		# Create empty header.
		my $fh = FileHandle->new($output, O_WRONLY|O_CREAT|O_TRUNC,
					 0644)
			or die "open($output) failed: $!\n";
		return;
	}

	$OUT->debug("Header dump: $input => $output");
	my $loader = UtsTune::PackageLoader->new();
	my $parser = $loader->parserInstance(FILE => $input, PRINT => $OUT);

	my $hook = sub {
		unlink($output) unless ($? == 0);
	};
	push(@EXIT_HOOK, $hook);
	$parser->header($output);

	$OUT->debug("Header dump: done");
}

sub removetune()
{
	usage(1) unless (@ARGV == 1);
	my ($file) = @ARGV;

	my $path = abs_path($file);
	$OUT->debug("Remove modtune: $path");

	my $db = UtsTune::DB::Parameter->new(1);

	# Check whether modtune file has been removed after "tune-init".
	my $mtime = $db->getModTuneMtime($path);
	if (! -f $UtsTune::INIT_STAMP or defined($mtime)) {
		my $f = utspath($path);
		my $msg = <<OUT;
$f file has been removed. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}

	$db->removeModTune($path);
	$OUT->debug("Remove modtune: done");
}

sub param_set($$$)
{
	my ($plat, $objdir, $force) = @_;

	usage(1) unless (@ARGV == 2);
	my ($name, $value) = (@ARGV);
	my $db = UtsTune::DB::Parameter->new(1);
	$plat = default_platform($db, 1) unless ($plat);

	# Check whether utstune environment is initialized or not.
	check_environment(db => $db, platform => $plat, objdir => $objdir);

	$OUT->debug("Change parameter: $plat: $name => $value");

	require UtsTune::OptionLoader;
	import UtsTune::OptionLoader;
	my $optloader = UtsTune::OptionLoader->new($db, $plat, $objdir);

	# Change parameter.
	param_set_impl($name, $plat, $value, $objdir, $force, $db, $optloader,
		       undef);
}

sub param_set_impl($$$$$$$$)
{
	my ($name, $plat, $value, $objdir, $force, $db, $optloader, $also) = @_;

	$OUT->debug("Change parameter: $name => $value");

	my $opt = $optloader->loadOption($name) or
		die "Unknown option name: $name\n";
	my $otype = $opt->getType();
	$OUT->debug("option type: ", $otype->getType());
	my ($newvalue, $type) = $otype->valueFromArgument($value);
	$OUT->debug("newvalue:[$newvalue] type:[$type]");
	my $oldvalue = $opt->getValue($objdir);
	if ($opt->hasValue() and $otype->compare($oldvalue, $newvalue) == 0) {
		unless ($also) {
			$OUT->print(<<OUT);
Tunable parameter "$name" is already set to $newvalue.
OUT
		}
		return;
	}

	# Check whether new value is valid or not.
	my $err = $otype->checkValue($newvalue, $type);
	die "$err\n" if ($err);
	$opt->checkValue($newvalue, $objdir);

	unless ($force) {
		my $msg = <<OUT;

Tunable parameter "$name" will be changed from $oldvalue to $newvalue.
OUT
		my $ret = confirm($msg, 'y');
		unless ($ret) {
			$OUT->printf("Cancelled.\n");
			return;
		}
	}

	# Change parameter.
	$opt->setValue($newvalue);
	my $parser = $optloader->loadParser($name);

	my $adlist = $opt->getParameter('also-define');
	if ($adlist and @$adlist and $otype->isTrue($value)) {
		my $nl = 1;

		foreach my $ad (@$adlist) {
			unless ($force) {
				$OUT->print("\n") if ($nl);
				$OUT->print(<<OUT);
NOTICE: $ad is also defined as \"true\".
OUT
				undef $nl;
			}
			param_set_impl($ad, $plat, $value, $objdir, 1, $db,
				       $optloader, 1);
		}
	}

	$parser->checkAlsoDefineValue($objdir) unless ($also);

	my $curfile = $parser->getPath();
	$parser->output($curfile, $plat, $objdir, $db);
	static_touch($db) if ($opt->isRebuild());

	# Apply dynamic configuration script.
	my $scope = $db->getOptionScope($name, $plat);
	apply_script($objdir, $db, $plat, [$scope]);
}

sub param_reset($$$)
{
	my ($plat, $objdir, $force) = @_;

	my $db = UtsTune::DB::Parameter->new(1);
	$plat = default_platform($db, 1) unless ($plat);

	# Check whether utstune environment is initialized or not.
	check_environment(db => $db, platform => $plat, objdir => $objdir);

	require UtsTune::OptionLoader;
	import UtsTune::OptionLoader;
	my $optloader = UtsTune::OptionLoader->new($db, $plat, $objdir);

	my (@target, %newpsmap, $rebuild);
	my $walker = sub {
		my ($name, $modtune) = @_;

		$name = $db->getOptionName($name, $plat) or return;
		$rebuild |= param_reset_impl($name, $plat, $objdir, $optloader,
					     @target, %newpsmap);
		my $opt = $optloader->loadOption($name);
		my $parser = $optloader->loadParser($name, $plat);
		my $curfile = $parser->getPath();

		# Check whether this option is changed.
		my $type = $opt->getType();
		if ($opt->hasValue()) {
			my $oldvalue = $opt->getValue($objdir);
			my $default = $opt->getParameter('default', $objdir);
			push(@target, {NAME => $name, OLD => $oldvalue,
				       NEW => $default, OPT => $opt});
			$opt->reset();
			$rebuild = 1 if ($opt->isRebuild());
			$newpsmap{$curfile} = $parser;
		}
	};
	if (@ARGV) {
		foreach my $name (@ARGV) {
			my $modtune = $db->getModTune($name, $plat);
			die "Unknown option name: $name\n" unless ($modtune);
			&$walker($name, $modtune);
		}
	}
	else {
		$db->walk($walker);
	}

	unless (@target) {
		$OUT->print("No parameter to be reset.\n");
		return;
	}

	unless ($force) {
		my $msg;
		if (@target == 1) {
			my $map = $target[0];
			my $name = $map->{NAME};
			my $oldvalue = $map->{OLD};
			my $newvalue = $map->{NEW};
			$msg = <<OUT;
Tunable parameter "$name" will be reset from $oldvalue to default value ($newvalue).
OUT
		}
		else {
			$msg = <<OUT;

The following tunable parameters will be reset:

OUT
			foreach my $map (@target) {
				my $name = $map->{NAME};
				my $oldvalue = $map->{OLD};
				my $newvalue = $map->{NEW};
				$msg .= sprintf("    %-20s  (%s => %s)\n",
						$name, $oldvalue, $newvalue);
			}
		}

		my $ret = confirm($msg, 'y');
		unless ($ret) {
			$OUT->printf("Cancelled.\n");
			return;
		}
	}

	while (my ($curfile, $parser) = each(%newpsmap)) {
		$parser->checkAlsoDefineValue($objdir);
	}

	# Reset parameter.
	my (%scmap);
	while (my ($curfile, $parser) = each(%newpsmap)) {
		$parser->output($curfile, $plat, $objdir, $db);
		my $scope = $db->getScope($parser->getDefPath());
		$scmap{$scope} = 1;
	}

	static_touch($db) if ($rebuild);

	# Apply dynamic configuration script.
	my (@scope) = keys(%scmap);
	apply_script($objdir, $db, $plat, \@scope);
}

sub param_reset_impl($$$$\@\%)
{
	my ($name, $plat, $objdir, $optloader, $target, $newpsmap) = @_;

	my $opt = $optloader->loadOption($name);
	my $parser = $optloader->loadParser($name, $plat);
	my $curfile = $parser->getPath();

	# Check whether this option is changed.
	my $type = $opt->getType();
	my $rebuild = 0;
	if ($opt->hasValue()) {
		my $oldvalue = $opt->getValue($objdir);
		my $default = $opt->getParameter('default', $objdir);
		push(@$target, {NAME => $name, OLD => $oldvalue,
				NEW => $default});
		$opt->reset();
		$rebuild = 1 if ($opt->isRebuild());
		$newpsmap->{$curfile} = $parser;
		my $adlist = $opt->getParameter('also-define');
		my $otype = $opt->getType();
		if ($adlist and @$adlist and $otype->isTrue($default)) {
			foreach my $ad (@$adlist) {
				my $r = param_reset_impl($ad, $plat, $objdir,
							 $optloader, @$target,
							 %$newpsmap);
				$rebuild |= $r;
			}
		}
	}

	return $rebuild;
}

sub show_static($$)
{
	my ($platform, $objdir) = @_;

	check_static_unix();

	my $db = UtsTune::DB::Parameter->new();
	$platform = default_platform($db, undef) unless ($platform);

	# Check whether utstune environment is initialized or not.
	check_environment(db => $db, platform => $platform, objdir => $objdir);

	my $pl = ($platform) ? [$platform] : $db->getPlatforms();

	my $bldname = build_name($platform, $objdir);

	$OUT->print(<<OUT);
Dynamically configured static modules for $bldname:
OUT
	require UtsTune::DB::Static;
	import UtsTune::DB::Static;
	foreach my $plat (@$pl) {
		my $tune = $db->getStaticScript($plat) or next;
		my $architectures = $db->getStaticArch($plat) or next;
		if ($platform) {
			$OUT->print("\n");
		}
		else {
			$OUT->print(<<OUT);

  Platform: $plat
OUT
		}
		foreach my $arch (@$architectures) {
			unless (&UtsTune::DB::Static::exists($plat, $arch,
							     $objdir)) {
				next;
			}
			my $sdb = UtsTune::DB::Static->new($plat, $arch,
							   $objdir);
			my $map = $sdb->getAll();
			foreach my $k (sort keys(%$map)) {
				my $flag = $map->{$k};
				my $mod = $arch . '/' . $k;

				if ($flag) {
					$OUT->printf("    %-16s\t%s\n", $mod,
						     $flag);
				}
				else {
					$OUT->print("    $mod\n");
				}
			}
		}
	}
}

sub list_module_dir($$)
{
	my ($platform, $objdir) = @_;

	check_static_unix();

	my $db = UtsTune::DB::Parameter->new();
	$platform = default_platform($db, undef) unless ($platform);

	# Check whether utstune environment is initialized or not.
	check_environment(db => $db, platform => $platform, objdir => $objdir);

	my $list = $db->getModuleDir($platform);
	if ($list) {
		foreach my $dir (@$list) {
			$OUT->print($dir, "\n");
		}
	}
}

##
## Apply dynamic configuration script.
##
sub apply_script(@)
{
	my ($objdir, $db, $platform, $mkscope) = @_;

	$db = UtsTune::DB::Parameter->new() unless ($db);

	my $makedirs;
	if ($objdir) {
		$makedirs = [$objdir];
	}
	else {
		$makedirs = get_build_objdirs();
	}

	my $allpl = $db->getPlatforms();
	$platform = default_platform($db, undef) unless ($platform);
	my $pl = ($platform) ? [$platform] : $allpl;
	my (%plmap) = map {$_ => 1} @$allpl;

	require UtsTune::MakeOption;
	import UtsTune::MakeOption;
	my $mopt = UtsTune::MakeOption->new();

	foreach my $plat (@$pl) {
		my (%exclude) = (%plmap);

		delete($exclude{$plat});

		# Determine static-link modules.
		apply_static_script($makedirs, $plat, $db)
			if (is_static_unix());

		# Determine make options.
		$mopt->output($makedirs, $plat, \%exclude, $db, $mkscope,
			      $OUT);
	}
}

##
## Apply static module configuration script.
##
sub apply_static_script($$$)
{
	my ($makedirs, $plat, $db) = @_;

	require UtsTune::OptionLoader;
	import UtsTune::OptionLoader;

	require UtsTune::Script::Static;
	import UtsTune::Script::Static;

	my $tune = $db->getStaticScript($plat) or return;
	my $architectures = $db->getStaticArch($plat) or return;

	foreach my $objdir (@$makedirs) {
		my $optloader =
			UtsTune::OptionLoader->new($db, $plat, $objdir);
		$OUT->debug("Apply static tune for $plat.$objdir: $tune");

		# Load static tune script.
		my $script = UtsTune::Script::Static->new($db, $objdir,
							  $optloader, $plat,
							  $architectures);
		$script->run($tune);
	}
}

sub confirm($$)
{
	my ($msg, $default) = @_;

	my $yorn = ($default eq 'y') ? 'Y/n'
		: ($default eq 'n') ? 'y/N' : 'y/n';
	$OUT->print($msg, "\n");
	my $ret;
	while (1) {
		$OUT->print("==> OK? ($yorn): ");
		$OUT->flush();
		my $input = <STDIN>;
		last unless (defined($input));

		chomp($input);
		if ($input) {
			$input = lc($input);
			$input =~ s/^\s*//o;
			$input =~ s/\s*$//o;
		}
		else {
			$input = $default;
		}
		if ($input eq 'y') {
			$ret = 1;
			last;
		}
		if ($input eq 'n') {
			$ret = 0;
			last;
		}

		$OUT->print("Type 'y' or 'n'.\n\n");
	}

	return $ret;
}

sub print_option_header()
{
	$OUT->delayedPrint("\n");
	my $header = sprintf(NORMAL_FORMAT, "OPTION NAME", "CURRENT VALUE");
	$OUT->delayedPrint($header);
	$OUT->delayedPrint(<<OUT);
  ------------------------------------------------------------------------
OUT
}

sub print_option_line($$$)
{
	my ($name, $value, $changed) = @_;

	$name .= "(*)" if ($changed);
	$OUT->printf(NORMAL_FORMAT, $name, $value);
}

sub print_option($$$$$)
{
	my ($opt, $objdir, $verbose, $chgonly, $defpath) = @_;

	my $changed = $opt->hasValue();
	return 0 if ($chgonly and !$changed);

	if ($verbose) {
		print_verbose($opt, $objdir, $defpath);
		return 1;
	}

	my $name = $opt->getName();
	my $value = $opt->getValue($objdir);
	my $type = $opt->getType();
	print_option_line($name, $type->dumpValue($value, 1), $changed);
	return 1;
}

sub print_verbose($$$)
{
	my ($opt, $objdir, $defpath) = @_;

	my $type = $opt->getType();
	$OUT->print(<<OUT);
	------------------------------------------------------------------------
OUT
		my $name = $opt->getName();
	$OUT->printf(VERBOSE_FORMAT, "OPTION NAME:", $name);

	$OUT->printf(VERBOSE_FORMAT, "TYPE:", $type->getType());

	if ($defpath) {
		$OUT->printf(VERBOSE_FORMAT, "DEFINED:", $defpath);
	}

	my $value = $opt->getValue($objdir);
	$OUT->printf(VERBOSE_FORMAT, "CURRENT VALUE:",
		     $type->dumpValue($value, 1));

	my $default = $opt->getParameter('default', $objdir);
	$OUT->printf(VERBOSE_FORMAT, "DEFAULT VALUE:",
		     $type->dumpValue($default, 1));

	my $min = $opt->getParameter('min', $objdir);
	$OUT->printf(VERBOSE_FORMAT, "MIN:", $type->dumpValue($min, 1))
		if (defined($min));

	my $max = $opt->getParameter('max', $objdir);
	$OUT->printf(VERBOSE_FORMAT, "MAX:", $type->dumpValue($max, 1))
		if (defined($max));

	my $power = $opt->getParameter('power', $objdir);
	$OUT->printf(VERBOSE_FORMAT, "POWER:", $power)
		if (defined($power));

	my $align = $opt->getParameter('align', $objdir);
	$OUT->printf(VERBOSE_FORMAT, "ALIGN:", $align)
		if (defined($align));

	my $cds = $opt->getParameter('candidates', $objdir);
	print_candidates($opt, "CANDIDATES:", $cds) if (defined($cds));

	my $desc = $opt->getParameter('description');
	print_fold($opt, "DESCRIPTION:", $desc);

	print_fold($opt, "REBUILD:", '"true"') if ($opt->isRebuild());
	my $adlist = $opt->getParameter('also-define');
	print_candidates($opt, 'ALSO DEFINE:', $adlist) if (defined($adlist));

	$OUT->print("\n");
}

sub print_candidates($$$)
{
	my ($opt, $label, $list) = @_;

	my $type = $opt->getType();
	my $lbl = sprintf(VERBOSE_FORMAT_NONL, $label, "");
	my $col = length($lbl);
	my $startcol = $col;
	$OUT->print($lbl);
	my $prefix = ' ' x $startcol;
	foreach my $elem (@$list) {
		$elem = $type->dumpValue($elem, 1);
		if ($col == $startcol) {
			$OUT->print($elem);
			$col += length($elem);
			next;
		}
		my $len = length($elem);
		my $ncol = $col + $len + 2;
		if ($ncol > MAX_WIDTH) {
			$OUT->print(",\n", $prefix, $elem);
			$col = $startcol + $len;
			next;
		}
		$col = $ncol;
		$OUT->print(", ", $elem);
	}
	$OUT->print("\n");
}

sub print_fold($$$)
{
	my ($opt, $label, $str) = @_;

	$str = $opt->evalString($str);
	my $lbl = sprintf(VERBOSE_FORMAT_NONL, $label, "");
	my $col = length($lbl);
	my $startcol = $col;
	$OUT->print($lbl);
	my $prefix = ' ' x $startcol;
	my $width = MAX_WIDTH - $startcol;
	foreach my $line (split(/\n/, $str)) {
		unless ($col) {
			$col = $startcol;
			$OUT->print($prefix);
		}
		my $len = length($line);
		my $ncol = $col + $len;
		if ($ncol <= MAX_WIDTH) {
			$OUT->print($line, "\n");
			$col = 0;
			next;
		}

		my $break;
		for (my $i = $width - 1; $i > 0; $i--) {
			my $c = substr($line, $i, 1);
			if ($c eq ' ' or $c eq "\t") {
				$break = $i;
				last;
			}
		}
		unless (defined($break)) {
			for (my $i = $len - 1; $i > 0; $i--) {
				my $c = substr($line, $i, 1);
				if ($c eq ' ' or $c eq "\t") {
					$break = $i;
					last;
				}
			}
		}
		if  (defined($break)) {
			# Break line here.
			$OUT->print(substr($line, 0, $break), "\n");
			for (; $break < $len; $break++) {
				my $c = substr($line, $break, 1);
				last unless ($c eq ' ' or $c eq "\t");
			}
			if ($break == $len) {
				next;
			}
			$line = substr($line, $break);
			$col = 0;
			redo;
		}

		# We can't break this line.
		$OUT->print($line, "\n");
		$col = 0;
	}
}

sub print_tune($$$$$)
{
	my ($tune, $objdir, $verbose, $changed, $name) = @_;

	if ($name and !ref($name)) {
		my $opt = $tune->getOption($name);
		die "Unknown option name: $name\n" unless (defined($opt));
		my $defpath = $tune->getDefPath();
		$defpath =~ s,^$UtsTune::UTS/*,,go;
		return print_option($opt, $objdir, $verbose, $changed,
				    $defpath);
	}

	my $ret;
	my $omap = $tune->getOption();
	foreach my $nm (sort keys (%$omap)) {
		my $opt = $omap->{$nm};
		my $nm = $opt->getName();
		next if ($name and !$name->{$nm});
		$ret |= print_option($opt, $objdir, $verbose, $changed, undef);
	}

	return $ret;
}

sub check_environment(%)
{
	my (%args) = @_;

	my $loader = $args{loader} || UtsTune::PackageLoader->new();
	my $db = $args{db} || UtsTune::DB::Parameter->new();
	my $plat = $args{platform};
	my $objdir = $args{objdir};

	my $util = $loader->utilInstance();
	$util->checkFormatVersion(1);

	$db->checkPlatform($plat, $objdir);	
}

EOF
