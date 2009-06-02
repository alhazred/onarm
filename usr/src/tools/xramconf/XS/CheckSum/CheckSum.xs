/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <string.h>

#define	CKSUM_DEFAULT_VALUE	0
#define	CKSUM_DIGEST_SIZE	sizeof(uint32_t)

/* CheckSum context. */
typedef struct cksum_ctx {
	uint32_t	c_value;	/* current check sum */
} cksum_ctx_t;

static cksum_ctx_t *
cksum_get_context(SV* sv)
{
	if (sv_derived_from(sv, "XRamConf::Digest::CheckSum")) {
		return (cksum_ctx_t *)(uintptr_t)(SvIV(SvRV(sv)));
	}
	croak("A reference to XRamConf::Digest::CheckSum is required.");
	return (cksum_ctx_t *)NULL;
}

static void
cksum_init(cksum_ctx_t *ctx)
{
	ctx->c_value = CKSUM_DEFAULT_VALUE;
}

#define	UINT32_ALIGNED(a)				\
	(((uintptr_t)(a) & (sizeof(uint32_t) - 1)) == 0)

static void
cksum_update(cksum_ctx_t *ctx, uint8_t *addr, size_t size)
{
	uint32_t	sum = ctx->c_value;

	while (!UINT32_ALIGNED(addr) && size > 0) {
		sum += *addr;
		addr++;
		size--;
	}
	while (UINT32_ALIGNED(addr) && size >= sizeof(uint32_t)) {
		uint32_t	value, *ptr = (uint32_t *)addr;

		value = *ptr;
		sum += ((value >> 24) & 0xff) + ((value >> 16) & 0xff) +
		       ((value >> 8) & 0xff) + (value & 0xff);
		addr += sizeof(uint32_t);
		size -= sizeof(uint32_t);
	}
	while (size > 0) {
		sum += *addr;
		addr++;
		size--;
	}

	ctx->c_value = sum;
}

MODULE = XRamConf::Digest::CheckSum	PACKAGE = XRamConf::Digest::CheckSum

##
## Constructor.
##
void
new(xclass)
	SV	*xclass
    PREINIT:
	cksum_ctx_t	*ctx;
    PPCODE:
	if (!SvROK(xclass)) {
		STRLEN	len;
		char	*sclass;

		sclass = SvPV(xclass, len);
		New(55, ctx, 1, cksum_ctx_t);
		ST(0) = sv_newmortal();
		sv_setref_pv(ST(0), sclass, (void *)ctx);
		SvREADONLY_on(SvRV(ST(0)));
	}
	else {
		ctx = cksum_get_context(xclass);
	}

	cksum_init(ctx);
	XSRETURN(1);

##
## Destructor.
##
void
DESTROY(ctx)
	cksum_ctx_t	*ctx;
    CODE:
	Safefree(ctx);

##
## Reset check sum context.
##
void
reset(ctx)
	cksum_ctx_t	*ctx;
    PPCODE:
    	cksum_init(ctx);
	XSRETURN(1);

##
## Return digest size in bytes.
##
void
getDigestSize(void)
    PPCODE:
	sv_setnv(ST(0), (double)CKSUM_DIGEST_SIZE);
	XSRETURN(1);

##
## Add data and update check sum.
##
void
add(me, ...)
	SV	*me
    PREINIT:
	cksum_ctx_t	*ctx = cksum_get_context(me);
	int	i;
	uint8_t	*data;
	STRLEN	len;
    PPCODE:
	for (i = 1; i < items; i++) {
		data = (uint8_t *)(SvPVbyte(ST(i), len));
		cksum_update(ctx, data, len);
	}
	XSRETURN(1);

##
## Return check sum.
##
void
getValue(ctx)
	cksum_ctx_t	*ctx;
    PPCODE:
	sv_setnv(ST(0), (double)ctx->c_value);
	XSRETURN(1);
