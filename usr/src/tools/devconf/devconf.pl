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
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

=head1 NAME

I<devconf> - Generate built-in device configuration

=head1 SYNOPSIS

B<devconf> [-r][-o outfile] config-file

=head1 DESCRIPTION

I<devconf> takes one device configuration file, and it generates C source
code that contains built-in device node configuration.

=head1 OPTIONS

I<devconf> takes the following options:

=over 4

=item B<-o> I<file>

Specify output filename.
If omitted, I<devconf> dumps source code to standard output.

=item B<-r>

Remove output file on error.

=back

=head1 FORMAT OF CONFIGURATION FILE

This section describes the format of built-in device configuration file.
At first, remember that text from a "#" character until the end of line is
treaded as comment and is ignored.

Each device must be declared by the following form:

  device <name> {
          ....;
  }

One B<device> block represents one device node.
<name> is a device node name, and it must be a quoted string.
Device node name must obey the following restrictions:

=over 2

=item -

It must start with alphabet.

=item -

It must consist of alphabet, digit, comma, and underscore.

=back

The following properties can be defined in the B<device> block.

=over 4

=item B<registers>

B<registers> block defines set of hardware register specifications for
this device. B<registers> block must contain at least one B<reg> block which
defines register address range.

The following is the format of B<registers> block:

  registers {
      reg {
          address:  <address>;
          size:     <size>;
      }
      ...
  }

<address> is a base address of register, and <size> is size of register
address range. All B<reg> block must contains a pair of <address> and
<size>.
Two or more B<reg> blocks can be defined in one B<registers> block.
The first B<reg> block is always used to create name of the device node.

In addition, boolean property <uart_port> can be specified in
B<reg> block. The following B<reg> block represents that the
given register is a on-board UART port.

  reg {
      address:    <address>;
      size:       <size>;
      uart_port:  true;
  }

=item B<interrupts>

Define interrupt resource identifier bound to this device node.
The following is the format of B<interrupts> property:

  interrupts: <number>[, ...];

<number> must be a number that represents interrupt resource.
Two or more numbers can be defined by using comma-separated values.

=item B<properties>

B<properties> block defines set of properties to be defined in this
device node.
The following is the format of B<properties> property:

  properties {
      int {
          <property_name>:  <int-value>[,...];
          ...
      }
      int64 {
           <property_name>:  <int64-value>[,...];
           ...
      }
      string {
           <property_name>:  <string-value>[,...];
           ...
      }
      byte {
           <property_name>:  <byte-value>[,...];
           ...
      }
      boolean {
           <property_name>;
           ...
      }
  }

Each block in B<properties> block represents type of property value.

=over 4

=item B<int>

Set of integer properties.

=item B<int64>

Set of 64-bit integer properties.

=item B<string>

Set of string properties.

=item B<byte>

Set of byte properties.

=item B<boolean>

Set of boolean properties.

=back

All properties except for boolean properties must have value.
Array can be defined as property value by using comma-separated values.

On the other hand, boolean properties must not have value.
An existing boolean property always means "true".

=item B<cpp_if>

If this property is defined, the device node is created only if
the specified cpp condition is satisfied.

The following is the format of B<cpp_if> property:

  cpp_if:  <cpp-condition>;

<cpp-condition> must be a quoted string.
<cpp-condition> is passed to cpp #if directive.

For example, the following device node is defined only if
"#if defined(FOO)" is true.

  device "foo" {
      ...
      cpp_if:   "defined(FOO)";
  }

=back

=head2 USING CPP MACROS

cpp macros can be used in the device configuration file.

=over 2

=item -

If you want to use cpp macros, you must include C header file.
If a line starts with backslash character, from the next character to
the end of line will be exported to C source file.
So you can include C header file as follows:

  \#include <sys/foo.h>

=item -

All strings that start with "cpp:" are treated cpp macro.

=back

For example, the following B<register> block in the configuration file
is valid as long as DEVICE_ADDR and DEVICE_SIZE is defined in
sys/device.h.

  \#include <sys/device.h>

   device "dev" {
       registers {
           reg {
               address:    cpp:DEVICE_ADDR;
               size:       cpp:DEVICE_SIZE;
           }
       }
   }

=head1 EXAMPLE

  # IRQ1 and IRQ2 must be defined in sys/foo.h.
  \#include <sys/foo.h>

  # This node is named as "foo".
  device "foo" {
      # This device has two registers:
      #    - [0xff000000, 0xff001000)
      #    - [0xff100000, 0xff100800)
      registers {
          reg {
              address:   0xff000000;
              size:      0x1000;
          }
          reg {
              address:   0xff100000;
              size:      0x800;
          }
      }

      # Two interrupt resources, IRQ1 and IRQ2  are assigned
      # to this device.
      interrupts:    cpp:IRQ1, cpp:IRQ2;

      properties {
          # This node has two integer property:
          #   int-prop-1:   1
          #   int-prop-2:   0x10, 0x20
          int {
              "int-prop-1":   1;
              "int-prop-2":   0x10, 0x20;
          }

          # This node has one string property:
          #   str-prop:     "value"
          string {
              "str-prop":     "value";
          }
      }
  }

=cut

use strict;
use Getopt::Long;
use FileHandle;
use File::Basename;
use POSIX;

use DevConf::Parser;

use vars qw($PROGNAME @UNLINK $DO_UNLINK);

$PROGNAME = basename($0);

sub usage($);

END {
	my $status = $?;
	if ($status) {
		# Unlink output files.
		foreach my $f (@UNLINK) {
			print STDERR "*** Remove $f\n";
			unlink($f);
		}
	}
}

MAIN:
{
	Getopt::Long::Configure(qw(no_ignore_case bundling require_order));

	my ($help, $outfile, $out);
	usage(2) unless (GetOptions
			 ('o=s'		=> \$outfile,
			  'r'		=> \$DO_UNLINK,
			  'help'	=> \$help));
	usage(0) if ($help);
	usage(2) unless (@ARGV == 1);

	my $conf = $ARGV[0];
	if ($outfile) {
		$out = FileHandle->new($outfile,
				       O_WRONLY|O_CREAT|O_TRUNC, 0644)
			or die "open($outfile) failed: $!\n";
		push(@UNLINK, $outfile) if ($DO_UNLINK);
	}
	else {
		$out = \*STDOUT;
	}

	my $parser = DevConf::Parser->new($conf);
	$parser->output($out);

	exit 0;
}

sub usage($)
{
	my ($status) = @_;

	my $out = ($status) ? \*STDERR : \*STDOUT;
	print $out <<OUT;
Usage: $PROGNAME [-r][-o <outfile>] config-file

OUT
	if ($status) {
		print $out <<OUT;
Try "$PROGNAME --help".
OUT
	}
	else {
		print $out <<OUT;
Options:

  -o <outfile>
      Specify output filename.

  -r
      Remove output file on error.

OUT
	}

	exit $status;
}

EOF
