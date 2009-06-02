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

#ident	"@(#)sdt.c"

#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/dtrace.h>
#include <sys/kobj.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <vm/seg_kmem.h>
#include <sys/stack.h>
#include <sys/sdt_impl.h>

static dev_info_t		*sdt_devi;

int sdt_verbose = 0;

#define	SDT_REG_R0		0
#define	SDT_REG_R1		1
#define	SDT_REG_R2		2
#define	SDT_REG_R3		3
#define	SDT_RD_SHIFT		12

#define	SDT_COND_MASK		0xf0000000
#define	SDT_COND_AL		0xe0000000

#define	SDT_INST_MASK		0x0fffffff
#define	SDT_INST_MOV_XX_YY	0x01a00000	/* mov{cond} XX, YY */
#define	SDT_INST_MOV_R3_R2	\
        (SDT_INST_MOV_XX_YY | (SDT_REG_R3 << SDT_RD_SHIFT) | SDT_REG_R2)
#define	SDT_INST_MOV_R2_R1	\
	(SDT_INST_MOV_XX_YY | (SDT_REG_R2 << SDT_RD_SHIFT) | SDT_REG_R1)
#define	SDT_INST_MOV_R1_R0	\
	(SDT_INST_MOV_XX_YY | (SDT_REG_R1 << SDT_RD_SHIFT) | SDT_REG_R0)

#define	SDT_INST_MOV_R0_IMM	0x03a00000	/* mov{cond} r0, #IMM */
#define	SDT_INST_ORR_R0_IMM	0x03800000	/* orr{cond} r0, r0, #IMM */
#define	SDT_IMM8_MASK           0x000000ff
#define	SDT_IMM8_MAX            0x000000ff
#define	SDT_IMM16_MAX           0x0000ffff
#define	SDT_IMM24_MAX           0x00ffffff
#define	SDT_ROTATE_IMM_SHIFT    8
#define	SDT_BYTE1_SHIFT         8
#define	SDT_BYTE2_SHIFT         16
#define	SDT_BYTE3_SHIFT         24

#define	SDT_INST_BL		0x0b000000
#define	SDT_INST_B		0x0a000000
#define	SDT_PC24_MASK		0x00ffffff
#define	SDT_PC24(from, to)	\
	((((uintptr_t)(to) - ((uintptr_t)(from) + 8)) >> 2) & SDT_PC24_MASK)

#define	SDT_LDR_IP_SP		0xe59dc000	/* ldr ip, [sp] */
#define	SDT_STMDB_SP_R3IPSPLR	0xe92d7008	/* stmdb sp!, {r3,ip,sp,lr} */

#define	SDT_MOV_R3_R2           (SDT_COND_AL | SDT_INST_MOV_R3_R2)
#define	SDT_MOV_R2_R1           (SDT_COND_AL | SDT_INST_MOV_R2_R1)
#define	SDT_MOV_R1_R0           (SDT_COND_AL | SDT_INST_MOV_R1_R0)

#define	SDT_MOV_R0_IMM(im)	\
        (SDT_COND_AL | SDT_INST_MOV_R0_IMM | ((im) & SDT_IMM8_MASK))
	                                                /* mov r0, #IMM */
#define	SDT_ORR_R0_BYTE1(im)	\
	(SDT_COND_AL | SDT_INST_ORR_R0_IMM | (12 << SDT_ROTATE_IMM_SHIFT) \
	| (((im) >> SDT_BYTE1_SHIFT) & SDT_IMM8_MASK))
						/* orr r0, r0, #0x0000??00 */
#define	SDT_ORR_R0_BYTE2(im)	\
	(SDT_COND_AL | SDT_INST_ORR_R0_IMM | (8 << SDT_ROTATE_IMM_SHIFT) \
	| (((im) >> SDT_BYTE2_SHIFT) & SDT_IMM8_MASK))
						/* orr r0, r0, #0x00??0000 */
#define	SDT_ORR_R0_BYTE3(im)	\
	(SDT_COND_AL | SDT_INST_ORR_R0_IMM | (4 << SDT_ROTATE_IMM_SHIFT) \
	| (((im) >> SDT_BYTE3_SHIFT) & SDT_IMM8_MASK))
						/* orr r0, r0, #0x??000000 */

#define	SDT_BL(orig, dest)	\
	(SDT_COND_AL | SDT_INST_BL | SDT_PC24(orig, dest))
#define	SDT_B(orig, dest)	\
	(SDT_COND_AL | SDT_INST_B | SDT_PC24(orig, dest))
#define	SDT_BL_COND(orig, dest)	\
	((*((uint32_t *)(orig)) & SDT_COND_MASK) \
	| SDT_INST_BL | SDT_PC24(orig, dest))

#define	SDT_ADD_SP_IMM12	0xe28dd00c	/* add sp, sp, #12 */
#define	SDT_LDR_PC_SP		0xe49df004	/* ldr pc, [sp], #4 */

#define	SDT_BX_LR		0xe12fff1e	/* bx lr */

#define	SDT_ENTRY_SIZE	(12 * sizeof (uint32_t))

static void
sdt_initialize(sdt_probe_t *sdp, uint32_t **trampoline)
{
	uint32_t id = sdp->sdp_id;
	uint32_t *instr = *trampoline;

	*instr++ = SDT_LDR_IP_SP;
	*instr++ = SDT_STMDB_SP_R3IPSPLR;	/* sp dummy save */
	*instr++ = SDT_MOV_R3_R2;
	*instr++ = SDT_MOV_R2_R1;
	*instr++ = SDT_MOV_R1_R0;
	*instr++ = SDT_MOV_R0_IMM(id);
	if (id > SDT_IMM8_MAX)  *instr++ = SDT_ORR_R0_BYTE1(id);
	if (id > SDT_IMM16_MAX) *instr++ = SDT_ORR_R0_BYTE2(id);
	if (id > SDT_IMM24_MAX) *instr++ = SDT_ORR_R0_BYTE3(id);
	*instr = SDT_BL(instr, dtrace_probe);
	instr++;
	*instr++ = SDT_ADD_SP_IMM12;
	*instr++ = SDT_LDR_PC_SP;

	*trampoline = instr;
}

/*ARGSUSED*/
static void
sdt_provide_module(void *arg, struct modctl *ctl)
{
	struct module *mp = ctl->mod_mp;
	char *modname = ctl->mod_modname;
	int primary, nprobes = 0;
	sdt_probedesc_t *sdpd;
	sdt_probe_t *sdp, *old;
	uint32_t *tab;
	sdt_provider_t *prov;
	int len;

	/*
	 * One for all, and all for one:  if we haven't yet registered all of
	 * our providers, we'll refuse to provide anything.
	 */
	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (prov->sdtp_id == DTRACE_PROVNONE)
			return;
	}

	if (mp->sdt_nprobes != 0 || (sdpd = mp->sdt_probes) == NULL)
		return;

	kobj_textwin_alloc(mp);

	/*
	 * Hack to identify unix/genunix/krtld.
	 */
	primary = vmem_contains(heap_arena, (void *)ctl,
	    sizeof (struct modctl)) == 0;

	/*
	 * If there hasn't been an sdt table allocated, we'll do so now.
	 */
	if (mp->sdt_tab == NULL) {
		for (; sdpd != NULL; sdpd = sdpd->sdpd_next) {
			nprobes++;
		}

		/*
		 * We could (should?) determine precisely the size of the
		 * table -- but a reasonable maximum will suffice.
		 */
		mp->sdt_size = nprobes * SDT_ENTRY_SIZE;
		mp->sdt_tab = kobj_texthole_alloc(mp->text, mp->sdt_size);

		if (mp->sdt_tab == NULL) {
			cmn_err(CE_WARN, "couldn't allocate SDT table "
			    "for module %s", modname);
			return;
		}
	}

	tab = (uint32_t *)mp->sdt_tab;

	for (sdpd = mp->sdt_probes; sdpd != NULL; sdpd = sdpd->sdpd_next) {
		char *name = sdpd->sdpd_name, *func, *nname;
		int i, j;
		sdt_provider_t *prov;
		ulong_t offs;
		dtrace_id_t id;
		uintptr_t addr;

		for (prov = sdt_providers; prov->sdtp_prefix != NULL; prov++) {
			char *prefix = prov->sdtp_prefix;

			if (strncmp(name, prefix, strlen(prefix)) == 0) {
				name += strlen(prefix);
				break;
			}
		}

		nname = kmem_alloc(len = strlen(name) + 1, KM_SLEEP);

		for (i = 0, j = 0; name[j] != '\0'; i++) {
			if (name[j] == '_' && name[j + 1] == '_') {
				nname[i] = '-';
				j += 2;
			} else {
				nname[i] = name[j++];
			}
		}

		nname[i] = '\0';

		sdp = kmem_zalloc(sizeof (sdt_probe_t), KM_SLEEP);
		sdp->sdp_loadcnt = ctl->mod_loadcnt;
		sdp->sdp_primary = primary;
		sdp->sdp_ctl = ctl;
		sdp->sdp_name = nname;
		sdp->sdp_namelen = len;
		sdp->sdp_provider = prov;

		func = kobj_searchsym(mp, sdpd->sdpd_offset, &offs);

		if (func == NULL)
			func = "<unknown>";

		/*
		 * We have our provider.  Now create the probe.
		 */
		if ((id = dtrace_probe_lookup(prov->sdtp_id, modname,
		    func, nname)) != DTRACE_IDNONE) {
			old = dtrace_probe_arg(prov->sdtp_id, id);
			ASSERT(old != NULL);

			sdp->sdp_next = old->sdp_next;
			sdp->sdp_id = id;
			old->sdp_next = sdp;
		} else {
			sdp->sdp_id = dtrace_probe_create(prov->sdtp_id,
			    modname, func, nname, 1, sdp);

			mp->sdt_nprobes++;
		}

		addr = sdpd->sdpd_offset;
		if (*(uint32_t *)addr == SDT_BX_LR) {
			/* Stub of static module. Only return code. */
			sdp->sdp_patchval = SDT_B(addr, tab);
		} else {
			sdp->sdp_patchval = SDT_BL_COND(addr, tab);
		}
		sdp->sdp_patchpoint = (uint32_t *)((uintptr_t)mp->textwin +
		    (sdpd->sdpd_offset - (uintptr_t)mp->text));
		sdp->sdp_savedval = *sdp->sdp_patchpoint;
		sdt_initialize(sdp, &tab);
	}

	sync_icache(mp->sdt_tab, mp->sdt_size);
}

/*ARGSUSED*/
static void
sdt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	sdt_probe_t *sdp = parg, *old;
	struct modctl *ctl = sdp->sdp_ctl;

	if (ctl != NULL && ctl->mod_loadcnt == sdp->sdp_loadcnt) {
		if ((ctl->mod_loadcnt == sdp->sdp_loadcnt &&
		    ctl->mod_loaded) || sdp->sdp_primary) {
			((struct module *)(ctl->mod_mp))->sdt_nprobes--;
		}
	}

	while (sdp != NULL) {
		old = sdp;
		kmem_free(sdp->sdp_name, sdp->sdp_namelen);
		sdp = sdp->sdp_next;
		kmem_free(old, sizeof (sdt_probe_t));
	}
}

/*ARGSUSED*/
static void
sdt_enable(void *arg, dtrace_id_t id, void *parg)
{
	sdt_probe_t *sdp = parg;
	struct modctl *ctl = sdp->sdp_ctl;

	ctl->mod_nenabled++;

	/*
	 * If this module has disappeared since we discovered its probes,
	 * refuse to enable it.
	 */
	if (!sdp->sdp_primary && !ctl->mod_loaded) {
		if (sdt_verbose) {
			cmn_err(CE_NOTE, "sdt is failing for probe %s "
			    "(module %s unloaded)",
			    sdp->sdp_name, ctl->mod_modname);
		}
		goto err;
	}

	/*
	 * Now check that our modctl has the expected load count.  If it
	 * doesn't, this module must have been unloaded and reloaded -- and
	 * we're not going to touch it.
	 */
	if (ctl->mod_loadcnt != sdp->sdp_loadcnt) {
		if (sdt_verbose) {
			cmn_err(CE_NOTE, "sdt is failing for probe %s "
			    "(module %s reloaded)",
			    sdp->sdp_name, ctl->mod_modname);
		}
		goto err;
	}

	while (sdp != NULL) {
		*sdp->sdp_patchpoint = sdp->sdp_patchval;
		sync_icache((caddr_t)sdp->sdp_patchpoint, 4);
		sdp = sdp->sdp_next;
	}

err:
	;
}

/*ARGSUSED*/
static void
sdt_disable(void *arg, dtrace_id_t id, void *parg)
{
	sdt_probe_t *sdp = parg;
	struct modctl *ctl = sdp->sdp_ctl;

	ASSERT(ctl->mod_nenabled > 0);
	ctl->mod_nenabled--;

	if ((!sdp->sdp_primary && !ctl->mod_loaded) ||
	    (ctl->mod_loadcnt != sdp->sdp_loadcnt))
		goto err;

	while (sdp != NULL) {
		*sdp->sdp_patchpoint = sdp->sdp_savedval;
		sync_icache((caddr_t)sdp->sdp_patchpoint, 4);
		sdp = sdp->sdp_next;
	}

err:
	;
}

static dtrace_pops_t sdt_pops = {
	NULL,
	sdt_provide_module,
	sdt_enable,
	sdt_disable,
	NULL,
	NULL,
	sdt_getargdesc,
	NULL,
	NULL,
	sdt_destroy
};

static int
sdt_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	sdt_provider_t *prov;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "sdt", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	sdt_devi = devi;

	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (dtrace_register(prov->sdtp_name, prov->sdtp_attr,
		    DTRACE_PRIV_KERNEL, NULL,
		    &sdt_pops, prov, &prov->sdtp_id) != 0) {
			cmn_err(CE_WARN, "failed to register sdt provider %s",
			    prov->sdtp_name);
		}
	}

	return (DDI_SUCCESS);
}

static int
sdt_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	sdt_provider_t *prov;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (prov->sdtp_id != DTRACE_PROVNONE) {
			if (dtrace_unregister(prov->sdtp_id) != 0)
				return (DDI_FAILURE);
			prov->sdtp_id = DTRACE_PROVNONE;
		}
	}

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
sdt_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)sdt_devi;
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
sdt_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

static struct cb_ops sdt_cb_ops = {
	sdt_open,		/* open */
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

static struct dev_ops sdt_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	sdt_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	sdt_attach,		/* attach */
	sdt_detach,		/* detach */
	nodev,			/* reset */
	&sdt_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	nodev			/* dev power */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type (this is a pseudo driver) */
	"Statically Defined Tracing",	/* name of module */
	&sdt_ops,		/* driver ops */
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
