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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#ident	"@(#)fbt.c"

#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/strsubr.h>
#include <sys/dtrace.h>
#include <sys/kobj.h>
#include <sys/modctl.h>
#include <sys/atomic.h>
#include <vm/seg_kmem.h>
#include <sys/stack.h>
#include <sys/ctf_api.h>
#include <sys/sysmacros.h>

static dev_info_t		*fbt_devi;
static dtrace_provider_id_t	fbt_id;
static uintptr_t		fbt_trampoline;
static caddr_t			fbt_trampoline_window;
static size_t			fbt_trampoline_size;
static int			fbt_verbose = 0;

/*
 * Various interesting bean counters.
 */
static int			fbt_entry;
static int			fbt_ret;

extern char			stubs_base[];
extern char			stubs_end[];

#define	FBT_REG_R0		0
#define	FBT_REG_R1		1
#define	FBT_REG_R2		2
#define	FBT_REG_R3		3
#define	FBT_REG_IP		12
#define	FBT_REG_LR		14
#define	FBT_REG_PC		15
#define	FBT_RN_SHIFT		16
#define	FBT_RD_SHIFT		12

#define	FBT_COND_MASK		0xf0000000
#define	FBT_COND_AL		0xe0000000
#define	FBT_COND_NV		0xf0000000
#define	FBT_COND_IS_AL(instr)	(((instr) & FBT_COND_MASK) == FBT_COND_AL)
#define	FBT_COND_IS_NOT_NV(instr)	\
	(((instr) & FBT_COND_MASK) != FBT_COND_NV)

#define	FBT_INST_MASK		0x0fffffff
#define	FBT_INST_STR_XX_SP	0x052d0004	/* str{cond} XX, [sp, #-4]! */
#define	FBT_INST_STR_LR_SP	\
	(FBT_INST_STR_XX_SP | (FBT_REG_LR << FBT_RD_SHIFT))
#define	FBT_INST_LDR_XX_SP	0x049d0004	/* ldr{cond} XX, [sp], #4 */
#define	FBT_INST_LDR_PC_SP	\
	(FBT_INST_LDR_XX_SP | (FBT_REG_PC << FBT_RD_SHIFT))
#define	FBT_INST_LDR_LR_SP	\
	(FBT_INST_LDR_XX_SP | (FBT_REG_LR << FBT_RD_SHIFT))
#define	FBT_INST_LDR_XX_SP_OFF	0x059d0000	/* ldr{cond} XX, [sp, #OFF] */
#define	FBT_INST_LDR_IP_SP24	\
	(FBT_INST_LDR_XX_SP_OFF | (FBT_REG_IP << FBT_RD_SHIFT) | 24)
						/* ldr{cond} ip, [sp, #24] */

#define	FBT_INST_STMDB_SP	0x092d0000	/* stm{cond}db sp!, {...} */
#define FBT_INST_LDMIA_SP	0x08bd0000	/* ldm{cond}ia sp!, {...} */
#define	FBT_INST_STMLDM_MASK	0x0fff0000
#define	FBT_REGLIST_PC		0x00008000
#define	FBT_REGLIST_LR		0x00004000
#define	FBT_REGLIST_IP		0x00001000
#define	FBT_REGLIST_R3		0x00000008
#define	FBT_REGLIST_R0		0x00000001
#define	FBT_REGLIST_R0_3	0x0000000f

#define	FBT_INST_ADD_SP_IMM	0x028dd000	/* add{cond} sp, sp, #IMM */
#define	FBT_INST_MOV_XX_IMM	0x03a00000	/* mov{cond} XX, #IMM */
#define	FBT_INST_MOV_R0_IMM	\
	(FBT_INST_MOV_XX_IMM | (FBT_REG_R0 << FBT_RD_SHIFT))
#define	FBT_INST_MOV_R1_IMM	\
	(FBT_INST_MOV_XX_IMM | (FBT_REG_R1 << FBT_RD_SHIFT))
#define	FBT_INST_ORR_XX_YY_IMM	0x03800000	/* orr{cond} XX, YY, #IMM */
#define	FBT_INST_ORR_R0_IMM	(FBT_INST_ORR_XX_YY_IMM \
	| (FBT_REG_R0 << FBT_RN_SHIFT) | (FBT_REG_R0 << FBT_RD_SHIFT))
#define	FBT_INST_ORR_R1_IMM	(FBT_INST_ORR_XX_YY_IMM \
	| (FBT_REG_R1 << FBT_RN_SHIFT) | (FBT_REG_R1 << FBT_RD_SHIFT))
#define	FBT_IMM8_MASK		0x000000ff
#define	FBT_IMM8_MAX		0x000000ff
#define	FBT_IMM16_MAX		0x0000ffff
#define	FBT_IMM24_MAX		0x00ffffff
#define	FBT_ROTATE_IMM_SHIFT	8
#define	FBT_BYTE1_SHIFT		8
#define	FBT_BYTE2_SHIFT		16
#define	FBT_BYTE3_SHIFT		24

#define	FBT_INST_MOV_XX_YY	0x01a00000	/* mov{cond} XX, YY */
#define	FBT_INST_MOV_R3_R2	\
	(FBT_INST_MOV_XX_YY | (FBT_REG_R3 << FBT_RD_SHIFT) | FBT_REG_R2)
#define	FBT_INST_MOV_R2_R1	\
	(FBT_INST_MOV_XX_YY | (FBT_REG_R2 << FBT_RD_SHIFT) | FBT_REG_R1)
#define	FBT_INST_MOV_R1_R0	\
	(FBT_INST_MOV_XX_YY | (FBT_REG_R1 << FBT_RD_SHIFT) | FBT_REG_R0)
#define	FBT_INST_MOV_R3_LR	\
	(FBT_INST_MOV_XX_YY | (FBT_REG_R3 << FBT_RD_SHIFT) | FBT_REG_LR)
#define	FBT_INST_MOV_R2_R0	\
	(FBT_INST_MOV_XX_YY | (FBT_REG_R2 << FBT_RD_SHIFT) | FBT_REG_R0)

#define	FBT_INST_BL		0x0b000000
#define	FBT_INST_B		0x0a000000
#define	FBT_PC24_MASK		0x00ffffff
#define	FBT_PC24(from, to)	\
	((((uintptr_t)(to) - ((uintptr_t)(from) + 8)) >> 2) & FBT_PC24_MASK)

#define	FBT_STR_LR_SP		(FBT_COND_AL | FBT_INST_STR_LR_SP)
						/* str lr, [sp, #-4]! */
#define	FBT_STMDB_SP_R0_3_IP_LR	\
	(FBT_COND_AL | FBT_INST_STMDB_SP | FBT_REGLIST_R0_3 | FBT_REGLIST_IP \
	| FBT_REGLIST_LR)
						/* stmdb sp!, {r0-3, ip, lr} */
#define	FBT_LDR_IP_SP24		(FBT_COND_AL | FBT_INST_LDR_IP_SP24)
						/* ldr ip, [sp, #24] */
#define	FBT_STMDB_SP_R3_IP	\
	(FBT_COND_AL | FBT_INST_STMDB_SP | FBT_REGLIST_R3 | FBT_REGLIST_IP)
						/* stmdb sp!, {r3, ip} */
#define	FBT_ADD_SP_IMM(im)	\
	(FBT_COND_AL | FBT_INST_ADD_SP_IMM | ((im) & FBT_IMM8_MASK))
						/* add sp, sp, #IMM */
#define	FBT_LDMIA_SP_R0_3_IP_LR	\
	(FBT_COND_AL | FBT_INST_LDMIA_SP | FBT_REGLIST_R0_3 | FBT_REGLIST_IP \
	| FBT_REGLIST_LR)
						/* ldmia sp!, {r0-3, ip, lr} */
#define	FBT_LDMIA_SP_R0_3_IP_PC	\
	(FBT_COND_AL | FBT_INST_LDMIA_SP | FBT_REGLIST_R0_3 | FBT_REGLIST_IP \
	| FBT_REGLIST_PC)
						/* ldmia sp!, {r0-3, ip, pc} */

#define	FBT_MOV_R0_IMM(im)	\
	(FBT_COND_AL | FBT_INST_MOV_R0_IMM | ((im) & FBT_IMM8_MASK))
						/* mov r0, #IMM */
#define	FBT_MOV_R1_IMM(im)	\
	(FBT_COND_AL | FBT_INST_MOV_R1_IMM | ((im) & FBT_IMM8_MASK))
						/* mov r0, #IMM */
#define	FBT_ORR_R0_BYTE1(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R0_IMM | (12 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE1_SHIFT) & FBT_IMM8_MASK))
						/* orr r0, r0, #0x0000??00 */
#define	FBT_ORR_R0_BYTE2(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R0_IMM | (8 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE2_SHIFT) & FBT_IMM8_MASK))
						/* orr r0, r0, #0x00??0000 */
#define	FBT_ORR_R0_BYTE3(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R0_IMM | (4 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE3_SHIFT) & FBT_IMM8_MASK))
						/* orr r0, r0, #0x??000000 */
#define	FBT_ORR_R1_BYTE1(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R1_IMM | (12 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE1_SHIFT) & FBT_IMM8_MASK))
						/* orr r1, r1, #0x0000??00 */
#define	FBT_ORR_R1_BYTE2(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R1_IMM | (8 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE2_SHIFT) & FBT_IMM8_MASK))
						/* orr r1, r1, #0x00??0000 */
#define	FBT_ORR_R1_BYTE3(im)	\
	(FBT_COND_AL | FBT_INST_ORR_R1_IMM | (4 << FBT_ROTATE_IMM_SHIFT) \
	| (((im) >> FBT_BYTE3_SHIFT) & FBT_IMM8_MASK))
						/* orr r1, r1, #0x??000000 */

#define	FBT_MOV_R3_R2		(FBT_COND_AL | FBT_INST_MOV_R3_R2)
#define	FBT_MOV_R2_R1		(FBT_COND_AL | FBT_INST_MOV_R2_R1)
#define	FBT_MOV_R1_R0		(FBT_COND_AL | FBT_INST_MOV_R1_R0)
#define	FBT_MOV_R3_LR		(FBT_COND_AL | FBT_INST_MOV_R3_LR)
#define	FBT_MOV_R2_R0		(FBT_COND_AL | FBT_INST_MOV_R2_R0)

#define	FBT_BL(orig, dest)	\
	(FBT_COND_AL | FBT_INST_BL | FBT_PC24(orig, dest))
#define	FBT_B(orig, dest)	\
	(FBT_COND_AL | FBT_INST_B | FBT_PC24(orig, dest))
#define	FBT_B_COND(orig, dest)	\
	((*((uint32_t *)(orig)) & FBT_COND_MASK) \
	| FBT_INST_B | FBT_PC24(orig, dest))

#define	FBT_IS_STR_LR_SP(instr)	((instr) == FBT_STR_LR_SP)
						/* str lr, [sp, #-4]! */
#define	FBT_IS_STMDB_SP_LR(instr)	\
	(((instr) & (FBT_COND_MASK | FBT_INST_STMLDM_MASK | FBT_REGLIST_LR)) \
	== (FBT_COND_AL | FBT_INST_STMDB_SP | FBT_REGLIST_LR))
						/* stmdb sp!, {... lr} */
#define	FBT_IS_SAVE_LR(instr)	\
	(FBT_IS_STR_LR_SP(instr) || FBT_IS_STMDB_SP_LR(instr))

#define	FBT_IS_LDR_PC_SP(instr)	\
	((((instr) & FBT_INST_MASK) == FBT_INST_LDR_PC_SP) \
	&& FBT_COND_IS_NOT_NV(instr))		/* ldr{cond} pc, [sp], #4 */

#define	FBT_IS_LDMIA_SP_PC(instr)	\
	((((instr) & (FBT_INST_STMLDM_MASK | FBT_REGLIST_PC | FBT_REGLIST_LR)) \
	== (FBT_INST_LDMIA_SP | FBT_REGLIST_PC)) \
	&& FBT_COND_IS_NOT_NV(instr))		/* ldm{cond}ia sp!, {... pc} */

#define	FBT_IS_RETURN(instr)	\
	(FBT_IS_LDR_PC_SP(instr) || FBT_IS_LDMIA_SP_PC(instr))

#define	FBT_PROBENAME_ENTRY	"entry"
#define	FBT_PROBENAME_RETURN	"return"
#define	FBT_ESTIMATE_ID		(UINT32_MAX)
#define	FBT_COUNTER(id, count)	if ((id) != FBT_ESTIMATE_ID) (count)++

#define	FBT_ENTENT_MAXSIZE	(15 * sizeof (uint32_t))
#define	FBT_RETENT_MAXSIZE	(14 * sizeof (uint32_t))
#define	FBT_ENT_MAXSIZE		MAX(FBT_ENTENT_MAXSIZE, FBT_RETENT_MAXSIZE)

typedef struct fbt_probe {
	char		*fbtp_name;
	dtrace_id_t	fbtp_id;
	uintptr_t	fbtp_addr;
	struct modctl	*fbtp_ctl;
	int		fbtp_loadcnt;
	int		fbtp_symndx;
	int		fbtp_primary;
	int		fbtp_return;
	uint32_t	*fbtp_patchpoint;
	uint32_t	fbtp_patchval;
	uint32_t	fbtp_savedval;
	struct fbt_probe *fbtp_next;
} fbt_probe_t;

typedef struct fbt_trampoline {
	uintptr_t	fbtt_va;
	uintptr_t	fbtt_limit;
	uintptr_t	fbtt_next;
} fbt_trampoline_t;

static caddr_t
fbt_trampoline_map(uintptr_t tramp, size_t size)
{
	uintptr_t offs;
	page_t **ppl;

	ASSERT(fbt_trampoline_window == NULL);
	ASSERT(fbt_trampoline_size == 0);
	ASSERT(fbt_trampoline == NULL);

	size += tramp & PAGEOFFSET;
	fbt_trampoline = tramp & PAGEMASK;
	fbt_trampoline_size = (size + PAGESIZE - 1) & PAGEMASK;
	fbt_trampoline_window =
	    vmem_alloc(heap_arena, fbt_trampoline_size, VM_SLEEP);

	(void) as_pagelock(&kas, &ppl, (caddr_t)fbt_trampoline,
	    fbt_trampoline_size, S_WRITE);

	for (offs = 0; offs < fbt_trampoline_size; offs += PAGESIZE) {
		hat_devload(kas.a_hat, fbt_trampoline_window + offs, PAGESIZE,
		    hat_getpfnum(kas.a_hat, (caddr_t)fbt_trampoline + offs),
		    PROT_READ | PROT_WRITE,
		    HAT_LOAD_LOCK | HAT_LOAD_NOCONSIST);
	}

	as_pageunlock(&kas, ppl, (caddr_t)fbt_trampoline, fbt_trampoline_size,
	    S_WRITE);

	return (fbt_trampoline_window + (tramp & PAGEOFFSET));
}

static void
fbt_trampoline_unmap()
{
	ASSERT(fbt_trampoline_window != NULL);
	ASSERT(fbt_trampoline_size != 0);
	ASSERT(fbt_trampoline != NULL);

	membar_enter();
	sync_icache((caddr_t)fbt_trampoline, fbt_trampoline_size);
	sync_icache(fbt_trampoline_window, fbt_trampoline_size);

	hat_unload(kas.a_hat, fbt_trampoline_window, fbt_trampoline_size,
	    HAT_UNLOAD_UNLOCK);

	vmem_free(heap_arena, fbt_trampoline_window, fbt_trampoline_size);

	fbt_trampoline_window = NULL;
	fbt_trampoline = NULL;
	fbt_trampoline_size = 0;
}

static uint32_t
fbt_patch_entry(uint32_t *instr, uint32_t id, fbt_trampoline_t *tramp,
    int nargs)
{
	uint32_t *tinstr = (uint32_t *)tramp->fbtt_next;
	uintptr_t va = tramp->fbtt_va;
	uintptr_t base = tramp->fbtt_next;

	if (tramp->fbtt_next + FBT_ENTENT_MAXSIZE > tramp->fbtt_limit) {
		/*
		 * There isn't sufficient room for this entry; return failure.
		 */
		return (0);
	}

	FBT_COUNTER(id, fbt_entry);

	*tinstr++ = FBT_STMDB_SP_R0_3_IP_LR;	/* ip save for sp align */
	if (nargs >= 5)
		*tinstr++ = FBT_LDR_IP_SP24;
	if (nargs >= 4)
		*tinstr++ = FBT_STMDB_SP_R3_IP;
	if (nargs >= 3)
		*tinstr++ = FBT_MOV_R3_R2;
	if (nargs >= 2)
		*tinstr++ = FBT_MOV_R2_R1;
	if (nargs >= 1)
		*tinstr++ = FBT_MOV_R1_R0;
	*tinstr++ = FBT_MOV_R0_IMM(id);
	if (id > FBT_IMM8_MAX)  *tinstr++ = FBT_ORR_R0_BYTE1(id);
	if (id > FBT_IMM16_MAX) *tinstr++ = FBT_ORR_R0_BYTE2(id);
	if (id > FBT_IMM24_MAX) *tinstr++ = FBT_ORR_R0_BYTE3(id);
	*tinstr = FBT_BL((uintptr_t)tinstr - base + va, dtrace_probe);
	tinstr++;
	if (nargs >= 4)
		*tinstr++ = FBT_ADD_SP_IMM(8);
	*tinstr++ = FBT_LDMIA_SP_R0_3_IP_LR;
	*tinstr++ = *instr;
	*tinstr = FBT_B((uintptr_t)tinstr - base + va, instr + 1);
	tinstr++;

	tramp->fbtt_va += (uintptr_t)tinstr - tramp->fbtt_next;
	tramp->fbtt_next = (uintptr_t)tinstr;

	return (1);
}

static uint32_t
fbt_patch_return(uint32_t *instr, uint32_t offset, uint32_t id,
    fbt_trampoline_t *tramp)
{
	uint32_t *tinstr = (uint32_t *)tramp->fbtt_next;
	uint32_t restore;
	uintptr_t va = tramp->fbtt_va;
	uintptr_t base = tramp->fbtt_next;

	if (tramp->fbtt_next + FBT_RETENT_MAXSIZE > tramp->fbtt_limit) {
		/*
		 * There isn't sufficient room for this entry; return failure.
		 */
		return (0);
	}

	FBT_COUNTER(id, fbt_ret);

	restore = (*instr & FBT_INST_MASK);
	if (restore == FBT_INST_LDR_PC_SP) {
		/* ldr{cond} pc, [sp], #4 */
		restore = FBT_INST_LDR_LR_SP;
	} else {
		/* ldm{cond}ia sp!, {... pc} */
		restore = ((restore & ~FBT_REGLIST_PC) | FBT_REGLIST_LR);
	}
	*tinstr++ = (restore | FBT_COND_AL);
	*tinstr++ = FBT_STMDB_SP_R0_3_IP_LR;	/* ip save for sp align */
	*tinstr++ = FBT_MOV_R2_R0;
	*tinstr++ = FBT_MOV_R3_LR;
	*tinstr++ = FBT_MOV_R0_IMM(id);
	if (id > FBT_IMM8_MAX)  *tinstr++ = FBT_ORR_R0_BYTE1(id);
	if (id > FBT_IMM16_MAX) *tinstr++ = FBT_ORR_R0_BYTE2(id);
	if (id > FBT_IMM24_MAX) *tinstr++ = FBT_ORR_R0_BYTE3(id);
	*tinstr++ = FBT_MOV_R1_IMM(offset);
	if (offset > FBT_IMM8_MAX)  *tinstr++ = FBT_ORR_R1_BYTE1(offset);
	if (offset > FBT_IMM16_MAX) *tinstr++ = FBT_ORR_R1_BYTE2(offset);
	if (offset > FBT_IMM24_MAX) *tinstr++ = FBT_ORR_R1_BYTE3(offset);
	*tinstr = FBT_BL((uintptr_t)tinstr - base + va, dtrace_probe);
	tinstr++;
	*tinstr++ = FBT_LDMIA_SP_R0_3_IP_PC;

	tramp->fbtt_va += (uintptr_t)tinstr - tramp->fbtt_next;
	tramp->fbtt_next = (uintptr_t)tinstr;

	return (1);
}

static char *fbt_ntname[] = {
	"scucnt_",		/* dtrace_gethrtime() calls scucnt_*() */
	"__",			/* __divdi3(), __qdivrem(), ... */
	"stubs_common_code",	/* stubs_common_code() uses ip as parameter */
	"sync_icache",		/* sync_icache() */
	"hat_xcall",		/* sync_icache() calls */
	"xc_",			/* sync_icache() calls */
	NULL			/* END mark */
};

/*ARGSUSED*/
static void
fbt_provide_module(void *arg, struct modctl *ctl)
{
	struct module *mp = ctl->mod_mp;
	char *modname = ctl->mod_modname;
	char *str = mp->strings;
	int nsyms = mp->nsyms;
	Shdr *symhdr = mp->symhdr;
	size_t symsize;
	char *name, **ntname;
	int i;
	fbt_probe_t *fbt, *retfbt;
	fbt_trampoline_t tramp;
	uintptr_t offset;
	int primary = 0;
	ctf_file_t *fp = NULL;
	int error;
	int estimate = 1;
	uint32_t faketramp[50];
	size_t fbt_size = 0;

	/*
	 * Employees of dtrace and their families are ineligible.  Void
	 * where prohibited.
	 */
	if (strcmp(modname, "dtrace") == 0)
		return;

	if (ctl->mod_requisites != NULL) {
		struct modctl_list *list;

		list = (struct modctl_list *)ctl->mod_requisites;

		for (; list != NULL; list = list->modl_next) {
			if (strcmp(list->modl_modp->mod_modname, "dtrace") == 0)
				return;
		}
	}

	/*
	 * KMDB is ineligible for instrumentation -- it may execute in
	 * any context, including probe context.
	 */
	if (strcmp(modname, "kmdbmod") == 0)
		return;

	/*
	 * Only "unix" make probe in static module.
	 */
	if (mp->text == s_text && strcmp(modname, "unix") != 0)
		return;

	if (str == NULL || symhdr == NULL || symhdr->sh_addr == NULL) {
		/*
		 * If this module doesn't (yet) have its string or symbol
		 * table allocated, clear out.
		 */
		return;
	}

	symsize = symhdr->sh_entsize;

	if (mp->fbt_nentries) {
		/*
		 * This module has some FBT entries allocated; we're afraid
		 * to screw with it.
		 */
		return;
	}

	if (mp->fbt_tab != NULL)
		estimate = 0;

	/*
	 * This is a hack for unix/genunix/krtld.
	 */
	primary = vmem_contains(heap_arena, (void *)ctl,
	    sizeof (struct modctl)) == 0;
	kobj_textwin_alloc(mp);

	/*
	 * Open the CTF data for the module.  We'll use this to determine the
	 * functions that can be instrumented.  Note that this call can fail,
	 * in which case we'll use heuristics to determine the functions that
	 * can be instrumented.  (But in particular, leaf functions will not be
	 * instrumented.)
	 */
	fp = ctf_modopen(mp, &error);

forreal:
	if (!estimate) {
		tramp.fbtt_next =
		    (uintptr_t)fbt_trampoline_map((uintptr_t)mp->fbt_tab,
		    mp->fbt_size);
		tramp.fbtt_limit = tramp.fbtt_next + mp->fbt_size;
		tramp.fbtt_va = (uintptr_t)mp->fbt_tab;
	}

	for (i = 1; i < nsyms; i++) {
		ctf_funcinfo_t f;
		uint32_t *instr, *base, *limit;
		Sym *sym = (Sym *)(symhdr->sh_addr + i * symsize);
		int nargs;

		if (ELF_ST_TYPE(sym->st_info) != STT_FUNC)
			continue;

		/*
		 * Weak symbols are not candidates.  This could be made to
		 * work (where weak functions and their underlying function
		 * appear as two disjoint probes), but it's not simple.
		 */
		if (ELF_ST_BIND(sym->st_info) == STB_WEAK)
			continue;

		name = str + sym->st_name;

		if (strstr(name, "dtrace_") == name &&
		    strstr(name, "dtrace_safe_") != name) {
			/*
			 * Anything beginning with "dtrace_" may be called
			 * from probe context unless it explitly indicates
			 * that it won't be called from probe context by
			 * using the prefix "dtrace_safe_".
			 */
			continue;
		}

		if (strstr(name, "kdi_") == name ||
		    strstr(name, "_kdi_") != NULL) {
			/*
			 * Any function name beginning with "kdi_" or
			 * containing the string "_kdi_" is a part of the
			 * kernel debugger interface and may be called in
			 * arbitrary context -- including probe context.
			 */
			continue;
		}

		if (strcmp(name, "shl") == 0) {
			/* used in __qdivrem() :common/util/arm/qdivrem.c */
			continue;
		}

		for (ntname = &fbt_ntname[0]; *ntname != NULL; ntname++) {
			if (strstr(name, *ntname) == name) {
				break;
			}
		}
		if (*ntname != NULL) {
			continue;
		}

		/*
		 * We want to scan the function for one (and only one) save.
		 * Any more indicates that something fancy is going on.
		 */
		base = (uint32_t *)sym->st_value;
		limit = (uint32_t *)(sym->st_value + sym->st_size);

		/*
		 * We don't want to interpose on the module stubs.
		 */
		if (base >= (uint32_t *)stubs_base &&
		    base <= (uint32_t *)stubs_end)
			continue;

		/*
		 * We can't safely trace a zero-length function...
		 */
		if (base == limit)
			continue;

		/*
		 * Due to 4524008, _init and _fini may have a bloated st_size.
		 * While this bug was fixed quite some time ago, old drivers
		 * may be lurking.  We need to develop a better solution to
		 * this problem, such that correct _init and _fini functions
		 * (the vast majority) may be correctly traced.  One solution
		 * may be to scan through the entire symbol table to see if
		 * any symbol overlaps with _init.  If none does, set a bit in
		 * the module structure that this module has correct _init and
		 * _fini sizes.  This will cause some pain the first time a
		 * module is scanned, but at least it would be O(N) instead of
		 * O(N log N)...
		 */
		if (strcmp(name, "_init") == 0)
			continue;

		if (strcmp(name, "_fini") == 0)
			continue;

		instr = base;

		if (fp != NULL && ctf_func_info(fp, i, &f) != CTF_ERR) {
			nargs = f.ctc_argc;
		} else {
			nargs = 32;
		}

		if (!FBT_IS_SAVE_LR(*instr)) {
			/* cmn_err(CE_NOTE, "cannot instrument %s: "
			    "save lr not in first", name); */
			continue;
		}

		if (estimate) {
			tramp.fbtt_next = (uintptr_t)faketramp;
			tramp.fbtt_limit = tramp.fbtt_next + sizeof (faketramp);
			(void) fbt_patch_entry(instr, FBT_ESTIMATE_ID,
			    &tramp, nargs);
			fbt_size += tramp.fbtt_next - (uintptr_t)faketramp;
		} else {
			fbt = kmem_zalloc(sizeof (fbt_probe_t), KM_SLEEP);
			fbt->fbtp_name = name;
			fbt->fbtp_ctl = ctl;
			fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
			    name, FBT_PROBENAME_ENTRY, 1, fbt);
			fbt->fbtp_patchval = FBT_B(instr, tramp.fbtt_va);

			if (!fbt_patch_entry(instr, fbt->fbtp_id,
			    &tramp, nargs)) {
				cmn_err(CE_WARN, "unexpectedly short FBT table "
				    "in module %s (sym %d of %d)", modname,
				    i, nsyms);
				break;
			}

			fbt->fbtp_patchpoint =
			    (uint32_t *)((uintptr_t)mp->textwin +
			    ((uintptr_t)instr - (uintptr_t)mp->text));
			fbt->fbtp_savedval = *instr;

			fbt->fbtp_loadcnt = ctl->mod_loadcnt;
			fbt->fbtp_primary = primary;
			fbt->fbtp_symndx = i;
			mp->fbt_nentries++;
		}

		retfbt = NULL;
again:
		if (++instr >= limit)
			continue;

		if (!FBT_IS_RETURN(*instr))
			goto again;

		offset = (uintptr_t)instr - (uintptr_t)base;

		if (estimate) {
			tramp.fbtt_next = (uintptr_t)faketramp;
			tramp.fbtt_limit = tramp.fbtt_next + sizeof (faketramp);
			(void) fbt_patch_return(instr, offset, FBT_ESTIMATE_ID,
			    &tramp);
			fbt_size += tramp.fbtt_next - (uintptr_t)faketramp;

			goto again;
		}

		fbt = kmem_zalloc(sizeof (fbt_probe_t), KM_SLEEP);
		fbt->fbtp_name = name;
		fbt->fbtp_ctl = ctl;

		if (retfbt == NULL) {
			fbt->fbtp_id = dtrace_probe_create(fbt_id, modname,
			    name, FBT_PROBENAME_RETURN, 1, fbt);
		} else {
			retfbt->fbtp_next = fbt;
			fbt->fbtp_id = retfbt->fbtp_id;
		}

		fbt->fbtp_return = 1;
		retfbt = fbt;

		fbt->fbtp_patchval = FBT_B_COND(instr, tramp.fbtt_va);

		if (!fbt_patch_return(instr, offset, fbt->fbtp_id, &tramp)) {
			cmn_err(CE_WARN, "unexpectedly short FBT table "
			    "in module %s (sym %d of %d)", modname, i, nsyms);
			break;
		}

		fbt->fbtp_patchpoint = (uint32_t *)((uintptr_t)mp->textwin +
		    ((uintptr_t)instr - (uintptr_t)mp->text));
		fbt->fbtp_savedval = *instr;
		fbt->fbtp_loadcnt = ctl->mod_loadcnt;
		fbt->fbtp_primary = primary;
		fbt->fbtp_symndx = i;
		mp->fbt_nentries++;

		goto again;
	}

	if (estimate) {
		/*
		 * Slosh on another entry's worth...
		 */
		fbt_size += FBT_ENT_MAXSIZE;
		mp->fbt_size = fbt_size;
		mp->fbt_tab = kobj_texthole_alloc(mp->text, fbt_size);

		if (mp->fbt_tab == NULL) {
			cmn_err(CE_WARN, "couldn't allocate FBT table "
			    "for module %s", modname);
		} else {
			estimate = 0;
			goto forreal;
		}
	} else {
		fbt_trampoline_unmap();
	}

error:
	if (fp != NULL)
		ctf_close(fp);
}

/*ARGSUSED*/
static void
fbt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *next;
	struct modctl *ctl = fbt->fbtp_ctl;

	do {
		if (ctl != NULL && ctl->mod_loadcnt == fbt->fbtp_loadcnt) {
			if ((ctl->mod_loadcnt == fbt->fbtp_loadcnt &&
			    ctl->mod_loaded) || fbt->fbtp_primary) {
				((struct module *)
				    (ctl->mod_mp))->fbt_nentries--;
			}
		}

		next = fbt->fbtp_next;
		kmem_free(fbt, sizeof (fbt_probe_t));
		fbt = next;
	} while (fbt != NULL);
}

/*ARGSUSED*/
static void
fbt_enable(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *f;
	struct modctl *ctl = fbt->fbtp_ctl;

	ctl->mod_nenabled++;

	for (f = fbt; f != NULL; f = f->fbtp_next) {
		if (f->fbtp_patchpoint == NULL) {
			/*
			 * Due to a shortened FBT table, this entry was never
			 * completed; refuse to enable it.
			 */
			if (fbt_verbose) {
				cmn_err(CE_NOTE, "fbt is failing for probe %s "
				    "(short FBT table in %s)",
				    fbt->fbtp_name, ctl->mod_modname);
			}

			return;
		}
	}

	/*
	 * If this module has disappeared since we discovered its probes,
	 * refuse to enable it.
	 */
	if (!fbt->fbtp_primary && !ctl->mod_loaded) {
		if (fbt_verbose) {
			cmn_err(CE_NOTE, "fbt is failing for probe %s "
			    "(module %s unloaded)",
			    fbt->fbtp_name, ctl->mod_modname);
		}

		return;
	}

	/*
	 * Now check that our modctl has the expected load count.  If it
	 * doesn't, this module must have been unloaded and reloaded -- and
	 * we're not going to touch it.
	 */
	if (ctl->mod_loadcnt != fbt->fbtp_loadcnt) {
		if (fbt_verbose) {
			cmn_err(CE_NOTE, "fbt is failing for probe %s "
			    "(module %s reloaded)",
			    fbt->fbtp_name, ctl->mod_modname);
		}

		return;
	}

	for (; fbt != NULL; fbt = fbt->fbtp_next) {
		*fbt->fbtp_patchpoint = fbt->fbtp_patchval;
		sync_icache((caddr_t)fbt->fbtp_patchpoint, 4);
	}
}

/*ARGSUSED*/
static void
fbt_disable(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg, *f;
	struct modctl *ctl = fbt->fbtp_ctl;

	ASSERT(ctl->mod_nenabled > 0);
	ctl->mod_nenabled--;

	for (f = fbt; f != NULL; f = f->fbtp_next) {
		if (f->fbtp_patchpoint == NULL)
			return;
	}

	if ((!fbt->fbtp_primary && !ctl->mod_loaded) ||
	    (ctl->mod_loadcnt != fbt->fbtp_loadcnt))
		return;

	for (; fbt != NULL; fbt = fbt->fbtp_next) {
		*fbt->fbtp_patchpoint = fbt->fbtp_savedval;
		sync_icache((caddr_t)fbt->fbtp_patchpoint, 4);
	}
}

/*ARGSUSED*/
static void
fbt_suspend(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	struct modctl *ctl = fbt->fbtp_ctl;

	if (!fbt->fbtp_primary && !ctl->mod_loaded)
		return;

	if (ctl->mod_loadcnt != fbt->fbtp_loadcnt)
		return;

	ASSERT(ctl->mod_nenabled > 0);

	for (; fbt != NULL; fbt = fbt->fbtp_next) {
		*fbt->fbtp_patchpoint = fbt->fbtp_savedval;
		sync_icache((caddr_t)fbt->fbtp_patchpoint, 4);
	}
}

/*ARGSUSED*/
static void
fbt_resume(void *arg, dtrace_id_t id, void *parg)
{
	fbt_probe_t *fbt = parg;
	struct modctl *ctl = fbt->fbtp_ctl;

	if (!fbt->fbtp_primary && !ctl->mod_loaded)
		return;

	if (ctl->mod_loadcnt != fbt->fbtp_loadcnt)
		return;

	ASSERT(ctl->mod_nenabled > 0);

	for (; fbt != NULL; fbt = fbt->fbtp_next) {
		*fbt->fbtp_patchpoint = fbt->fbtp_patchval;
		sync_icache((caddr_t)fbt->fbtp_patchpoint, 4);
	}
}

/*ARGSUSED*/
static void
fbt_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	fbt_probe_t *fbt = parg;
	struct modctl *ctl = fbt->fbtp_ctl;
	struct module *mp = ctl->mod_mp;
	ctf_file_t *fp = NULL, *pfp;
	ctf_funcinfo_t f;
	int error;
	ctf_id_t argv[32], type;
	int argc = sizeof (argv) / sizeof (ctf_id_t);
	const char *parent;

	if (!ctl->mod_loaded || (ctl->mod_loadcnt != fbt->fbtp_loadcnt))
		goto err;

	if (fbt->fbtp_return && desc->dtargd_ndx == 0) {
		(void) strcpy(desc->dtargd_native, "int");
		return;
	}

	if ((fp = ctf_modopen(mp, &error)) == NULL) {
		/*
		 * We have no CTF information for this module -- and therefore
		 * no args[] information.
		 */
		goto err;
	}

	/*
	 * If we have a parent container, we must manually import it.
	 */
	if ((parent = ctf_parent_name(fp)) != NULL) {
		struct modctl *mod;
		int found = 0;

		/*
		 * We must iterate over all modules to find the module that
		 * is our parent.
		 */
		mod = &modules;
		do {
			if (strcmp(mod->mod_modname, parent) == 0) {
				found = 1;
				break;
			}
		} while ((mod = mod->mod_next) != &modules);

		if (found == 0)
			goto err;

		if ((pfp = ctf_modopen(mod->mod_mp, &error)) == NULL)
			goto err;

		if (ctf_import(fp, pfp) != 0) {
			ctf_close(pfp);
			goto err;
		}

		ctf_close(pfp);
	}

	if (ctf_func_info(fp, fbt->fbtp_symndx, &f) == CTF_ERR)
		goto err;

	if (fbt->fbtp_return) {
		if (desc->dtargd_ndx > 1)
			goto err;

		ASSERT(desc->dtargd_ndx == 1);
		type = f.ctc_return;
	} else {
		if (desc->dtargd_ndx + 1 > f.ctc_argc)
			goto err;

		if (ctf_func_args(fp, fbt->fbtp_symndx, argc, argv) == CTF_ERR)
			goto err;

		type = argv[desc->dtargd_ndx];
	}

	if (ctf_type_name(fp, type, desc->dtargd_native,
	    DTRACE_ARGTYPELEN) != NULL) {
		ctf_close(fp);
		return;
	}
err:
	if (fp != NULL)
		ctf_close(fp);

	desc->dtargd_ndx = DTRACE_ARGNONE;
}

static dtrace_pattr_t fbt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t fbt_pops = {
	NULL,
	fbt_provide_module,
	fbt_enable,
	fbt_disable,
	fbt_suspend,
	fbt_resume,
	fbt_getargdesc,
	NULL,
	NULL,
	fbt_destroy
};

static int
fbt_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "fbt", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE ||
	    dtrace_register("fbt", &fbt_attr, DTRACE_PRIV_KERNEL, NULL,
	    &fbt_pops, NULL, &fbt_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	fbt_devi = devi;
	return (DDI_SUCCESS);
}

static int
fbt_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (dtrace_unregister(fbt_id) != 0)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
fbt_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)fbt_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED*/
static int
fbt_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

static struct cb_ops fbt_cb_ops = {
	fbt_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops fbt_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	fbt_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	fbt_attach,		/* attach */
	fbt_detach,		/* detach */
	nodev,			/* reset */
	&fbt_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	nodev			/* dev power */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type (this is a pseudo driver) */
	"Function Boundary Tracing",	/* name of module */
	&fbt_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
MODDRV_ENTRY_INIT(void)
{
	return (mod_install(&modlinkage));
}

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	return (mod_remove(&modlinkage));
}
#endif	/* !STATIC_DRIVER */
