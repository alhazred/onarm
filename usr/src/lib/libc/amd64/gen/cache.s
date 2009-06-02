/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2002 Advanced Micro Devices, Inc.
 * 
 * All rights reserved.
 * 
 * Redistribution and  use in source and binary  forms, with or
 * without  modification,  are   permitted  provided  that  the
 * following conditions are met:
 * 
 * + Redistributions  of source  code  must  retain  the  above
 *   copyright  notice,   this  list  of   conditions  and  the
 *   following disclaimer.
 * 
 * + Redistributions  in binary  form must reproduce  the above
 *   copyright  notice,   this  list  of   conditions  and  the
 *   following  disclaimer in  the  documentation and/or  other
 *   materials provided with the distribution.
 * 
 * + Neither the  name of Advanced Micro Devices,  Inc. nor the
 *   names  of  its contributors  may  be  used  to endorse  or
 *   promote  products  derived   from  this  software  without
 *   specific prior written permission.
 * 
 * THIS  SOFTWARE  IS PROVIDED  BY  THE  COPYRIGHT HOLDERS  AND
 * CONTRIBUTORS AS IS AND  ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING,  BUT NOT  LIMITED TO,  THE IMPLIED  WARRANTIES OF
 * MERCHANTABILITY  AND FITNESS  FOR A  PARTICULAR  PURPOSE ARE
 * DISCLAIMED.  IN  NO  EVENT  SHALL  ADVANCED  MICRO  DEVICES,
 * INC.  OR CONTRIBUTORS  BE LIABLE  FOR ANY  DIRECT, INDIRECT,
 * INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES
 * (INCLUDING,  BUT NOT LIMITED  TO, PROCUREMENT  OF SUBSTITUTE
 * GOODS  OR  SERVICES;  LOSS  OF  USE, DATA,  OR  PROFITS;  OR
 * BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON  ANY THEORY OF
 * LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY,  OR TORT
 * (INCLUDING NEGLIGENCE  OR OTHERWISE) ARISING IN  ANY WAY OUT
 * OF THE  USE  OF  THIS  SOFTWARE, EVEN  IF  ADVISED  OF  THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * It is  licensee's responsibility  to comply with  any export
 * regulations applicable in licensee's jurisdiction.
 */

	.ident	"@(#)cache.s	1.3	05/07/18 SMI"

	.file	"cache.s"

#include "SYS.h"
#include "cache.h"

        .global .amd64cache1, .amd64cache1half, .amd64cache2, .amd64cache2half

        .data

        .align  8

.amd64cache1:	.quad	1024 * 64	# defaults to SledgeHammer
.amd64cache1half: .quad	1024 * 32
.amd64cache2:	.quad	1024 * 1024
.amd64cache2half: .quad	1024 * 512

        .text

// AMD cache size determination

	ENTRY(__amd64id)

        push    %rbx

        mov     $CPUIDLARGESTFUNCTION, %eax		# get ID string
        cpuid

	cmp	$0x68747541, %ebx			# "Auth"
	jne	1f

	cmp	$0x69746e65, %edx			# "enti"
	jne	1f

	cmp	$0x444d4163, %ecx			# "cAMD"
	jne	1f

        mov     $CPUIDLARGESTFUNCTIONEX, %eax           # get highest level of support
        cpuid

        cmp     $AMDIDL2INFO, %eax                      # check for support of cache info
        jb      1f

        mov     $AMDIDL1INFO, %eax                      # get L1 info
        cpuid

        shr     $24, %ecx
        shl     $10, %ecx
        mov     %rcx, _sref_(.amd64cache1)

        shr     $1, %ecx
        mov     %rcx, _sref_(.amd64cache1half)

        mov     $AMDIDL2INFO, %eax                      # get L2 info
        cpuid

        shr     $16, %ecx
        shl     $10, %ecx
        mov     %rcx, _sref_(.amd64cache2)

        shr     $1, %ecx
        mov     %rcx, _sref_(.amd64cache2half)

1:
        pop	%rbx
        ret

	SET_SIZE(__amd64id)
