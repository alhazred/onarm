#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# sha1_block procedure for x86_64.
#
# It was brought to my attention that on EM64T compiler-generated code
# was far behind 32-bit assembler implementation. This is unlike on
# Opteron where compiler-generated code was only 15% behind 32-bit
# assembler, which originally made it hard to motivate the effort.
# There was suggestion to mechanically translate 32-bit code, but I
# dismissed it, reasoning that x86_64 offers enough register bank
# capacity to fully utilize SHA-1 parallelism. Therefore this fresh
# implementation:-) However! While 64-bit code does performs better
# on Opteron, I failed to beat 32-bit assembler on EM64T core. Well,
# x86_64 does offer larger *addressable* bank, but out-of-order core
# reaches for even more registers through dynamic aliasing, and EM64T
# core must have managed to run-time optimize even 32-bit code just as
# good as 64-bit one. Performance improvement is summarized in the
# following table:
#
#		gcc 3.4		32-bit asm	cycles/byte
# Opteron	+45%		+20%		6.8
# Xeon P4	+65%		+0%		9.9
# Core2		+60%		+10%		7.0

#
# OpenSolaris OS modifications
#
# Sun elects to use this software under the BSD license.
#
# This source originates from OpenSSL file sha1-x86_64.pl at
# ftp://ftp.openssl.org/snapshot/openssl-0.9.8-stable-SNAP-20080131.tar.gz
# (presumably for future OpenSSL release 0.9.8h), with these changes:
#
# 1. Added perl "use strict" and declared variables.
#
# 2. Added OpenSolaris ENTRY_NP/SET_SIZE macros from
# /usr/include/sys/asm_linkage.h, .ident keywords, and lint(1B) guards.
#
# 3. Added perl function &lea_offset_eax_register_register() to handle
#	Solaris as(1) bug.
#
# 4. Removed x86_64-xlate.pl script (not needed for as(1) or gas(1) assemblers).
#

use strict;
my ($code, $ctx, $inp, $num, $xi, $t0, $t1, $i, @V, $A, $B, $C, $D, $E, $T);
my $output = shift;
open STDOUT,">$output";


sub lea_offset_eax_register_register
# Workaround for a Solaris "gas" assembler bug where compiling the source
# errors out and does not generate a valid "lea" instruction.  Specifically,
#	&lea OFFSET(%eax, SOURCE_REGISTER),DESTINATION_REGISTER
#
# For Solaris as, "as -a32" must be used to compile this.
# For Solaris gas 2.15, this errors out with this message:
# Error: `0x5a827999(%eax,%r11d)' is not a valid 64 bit base/index expression
#
# This should be fixed in Solaris gas 2.16.
# It assembles with the Linux "as --64" gas 2.17 assembler and runs OK.
#
# For the ONBLD NV tools, the aw wrapper script fails when -a32 is used:
# /ws/onnv-tools/onbld/bin/i386/aw -xarch=amd64 -P -a32 -o lea.o lea.s
# aw: as->gas mapping failed at or near arg '-a32'
#
# For more information, see CRs 6644870 and 6628627.
{
	use Switch;
	my ($offset, $reg_src, $reg_dest) = @_;

	# Failed "lea" instruction.
	# This instruction errors out from the Solaris as assembler.
	# It assembles with the Linux "as --64" assembler and runs OK.
	$code .= "	/lea	$offset(%eax,$reg_src),$reg_dest\n";

	# Workaround
	# This workaround hand-generates hex machine code for lea.
	$code .= "	/ Solaris as assembly bug CR 6628627 errors out for\n";
	$code .= "	/ the above, so we specify the machine code in hex:\n";
	$code .= "	.byte	0x67	/ lea\n";

	switch ($reg_src) {
	case "%ebp"	{
			switch ($reg_dest) {
			case "%r11d" { $code .=
				"	.byte	0x44,0x8d,0x9c,0x28	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	case "%edi"	{
			switch ($reg_dest) {
			case "%ebp" { $code .=
				"	.byte	0x8d,0xac,0x38	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	case "%edx"	{
			switch ($reg_dest) {
			case "%esi" { $code .=
				"	.byte	0x8d,0xb4,0x10	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	case "%esi"	{
			switch ($reg_dest) {
			case "%edi" { $code .=
				"	.byte	0x8d,0xbc,0x30	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	case "%r11d"	{
			switch ($reg_dest) {
			case "%r12d" { $code .=
				"	.byte	0x46,0x8d,0xa4,0x18	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	case "%r12d"	{
			switch ($reg_dest) {
			case "%edx" { $code .=
				"	.byte	0x42,0x8d,0x94,0x20	"
				. "/ (%eax,$reg_src),$reg_dest\n"; }
			else	{ $code .= "Unknown register $reg_dest\n"; }
			}
	}
	else		{ $code .= "Unknown register $reg_src\n"; }
	}

	$code .= "	.long	$offset	/ offset\n";
}


#
# void sha1_block_data_order(SHA1_CTX *ctx, const void *inpp, size_t blocks);
#

# Arguments:
$ctx="%rdi";	# 1st arg
$inp="%rsi";	# 2nd arg
$num="%rdx";	# 3rd arg

# reassign arguments in order to produce more compact code
$ctx="%r8";
$inp="%r9";
$num="%r10";

# Temporaries:
$xi="%eax";
$t0="%ebx";
$t1="%ecx";
# State information from SHA-1 context:
$A="%edx";
$B="%esi";
$C="%edi";
$D="%ebp";
$E="%r11d";
# Temporary:
$T="%r12d";

@V=($A,$B,$C,$D,$E,$T);

sub PROLOGUE {
my $func=shift;
$code.=<<___;
ENTRY_NP($func)
	/* EXPORT DELETE START */
	push	%rbx
	push	%rbp
	push	%r12
	mov	%rsp,%rax
	mov	%rdi,$ctx	# reassigned argument
	sub	\$`8+16*4`,%rsp
	mov	%rsi,$inp	# reassigned argument
	and	\$-64,%rsp
	mov	%rdx,$num	# reassigned argument
	mov	%rax,`16*4`(%rsp)

	mov	0($ctx),$A
	mov	4($ctx),$B
	mov	8($ctx),$C
	mov	12($ctx),$D
	mov	16($ctx),$E
___
}

sub EPILOGUE {
my $func=shift;
$code.=<<___;
	mov	`16*4`(%rsp),%rsp
	pop	%r12
	pop	%rbp
	pop	%rbx
        /* EXPORT DELETE END */
	ret
SET_SIZE($func)
___
}

sub BODY_00_19 {
my ($i,$a,$b,$c,$d,$e,$f,$host)=@_;
my $j=$i+1;
$code.=<<___ if ($i==0);
	mov	`4*$i`($inp),$xi	
	`"bswap	$xi"	if(!defined($host))`
	mov	$xi,`4*$i`(%rsp)
___
	&lea_offset_eax_register_register("0x5a827999", $e, $f) if ($i < 15);
$code.=<<___ if ($i<15);
	/lea	0x5a827999($xi,$e),$f
	mov	$c,$t0
	mov	`4*$j`($inp),$xi
	mov	$a,$e
	xor	$d,$t0
	`"bswap	$xi"	if(!defined($host))`	
	rol	\$5,$e
	and	$b,$t0
	mov	$xi,`4*$j`(%rsp)
	add	$e,$f
	xor	$d,$t0
	rol	\$30,$b
	add	$t0,$f
___
	&lea_offset_eax_register_register("0x5a827999", $e, $f) if ($i >= 15);
$code.=<<___ if ($i>=15);
	/lea	0x5a827999($xi,$e),$f
	mov	`4*($j%16)`(%rsp),$xi
	mov	$c,$t0
	mov	$a,$e
	xor	`4*(($j+2)%16)`(%rsp),$xi
	xor	$d,$t0
	rol	\$5,$e
	xor	`4*(($j+8)%16)`(%rsp),$xi
	and	$b,$t0
	add	$e,$f
	xor	`4*(($j+13)%16)`(%rsp),$xi
	xor	$d,$t0
	rol	\$30,$b
	add	$t0,$f
	rol	\$1,$xi
	mov	$xi,`4*($j%16)`(%rsp)
___
}

sub BODY_20_39 {
my ($i,$a,$b,$c,$d,$e,$f)=@_;
my $j=$i+1;
my $K=($i<40)?0x6ed9eba1:0xca62c1d6;
	&lea_offset_eax_register_register($K, $e, $f) if ($i < 79);
$code.=<<___ if ($i<79);
	/lea	$K($xi,$e),$f
	mov	`4*($j%16)`(%rsp),$xi
	mov	$c,$t0
	mov	$a,$e
	xor	`4*(($j+2)%16)`(%rsp),$xi
	xor	$b,$t0
	rol	\$5,$e
	xor	`4*(($j+8)%16)`(%rsp),$xi
	xor	$d,$t0
	add	$e,$f
	xor	`4*(($j+13)%16)`(%rsp),$xi
	rol	\$30,$b
	add	$t0,$f
	rol	\$1,$xi
___
$code.=<<___ if ($i<76);
	mov	$xi,`4*($j%16)`(%rsp)
___
	&lea_offset_eax_register_register($K, $e, $f) if ($i == 79);
$code.=<<___ if ($i==79);
	/lea	$K($xi,$e),$f
	mov	$c,$t0
	mov	$a,$e
	xor	$b,$t0
	rol	\$5,$e
	xor	$d,$t0
	add	$e,$f
	rol	\$30,$b
	add	$t0,$f
___
}

sub BODY_40_59 {
my ($i,$a,$b,$c,$d,$e,$f)=@_;
my $j=$i+1;
	&lea_offset_eax_register_register("0x8f1bbcdc", $e, $f);
$code.=<<___;
	/lea	0x8f1bbcdc($xi,$e),$f
	mov	`4*($j%16)`(%rsp),$xi
	mov	$b,$t0
	mov	$b,$t1
	xor	`4*(($j+2)%16)`(%rsp),$xi
	mov	$a,$e
	and	$c,$t0
	xor	`4*(($j+8)%16)`(%rsp),$xi
	or	$c,$t1
	rol	\$5,$e
	xor	`4*(($j+13)%16)`(%rsp),$xi
	and	$d,$t1
	add	$e,$f
	rol	\$1,$xi
	or	$t1,$t0
	rol	\$30,$b
	mov	$xi,`4*($j%16)`(%rsp)
	add	$t0,$f
___
}

$code=<<___;
#if !defined(lint) && !defined(__lint)
	.ident	"@(#)sha1-x86_64.pl	1.1	08/03/02 SMI"
#include <sys/asm_linkage.h>
___


&PROLOGUE("sha1_block_data_order");
$code.=".align	4\n.Lloop:\n";
for($i=0;$i<20;$i++)	{ &BODY_00_19($i,@V); unshift(@V,pop(@V)); }
for(;$i<40;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
for(;$i<60;$i++)	{ &BODY_40_59($i,@V); unshift(@V,pop(@V)); }
for(;$i<80;$i++)	{ &BODY_20_39($i,@V); unshift(@V,pop(@V)); }
$code.=<<___;
	/ Update and save state information in SHA-1 context
	add	0($ctx),$E
	add	4($ctx),$T
	add	8($ctx),$A
	add	12($ctx),$B
	add	16($ctx),$C
	mov	$E,0($ctx)
	mov	$T,4($ctx)
	mov	$A,8($ctx)
	mov	$B,12($ctx)
	mov	$C,16($ctx)

	xchg	$E,$A	# mov	$E,$A
	xchg	$T,$B	# mov	$T,$B
	xchg	$E,$C	# mov	$A,$C
	xchg	$T,$D	# mov	$B,$D
			# mov	$C,$E
	lea	`16*4`($inp),$inp
	sub	\$1,$num
	jnz	.Lloop
___
&EPILOGUE("sha1_block_data_order");
$code.=<<___;
.asciz	"SHA1 block transform for x86_64, CRYPTOGAMS by <appro\@openssl.org>"

#else
	/* LINTED */
	/* Nothing to be linted in this file--it's pure assembly source. */
#endif /* !lint && !__lint */
___

####################################################################

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT;
