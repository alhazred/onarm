/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ident	"@(#)arm/boot/boot_common.c"

/*
 * ARM-specific kernel boot utilities.
 * The code in this file is used when the kernel is static-linked kernel.
 */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/bootprops.h>
#include <sys/varargs.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/boot_impl.h>

/*
 * PROM boot property emulation
 */

struct pseudoprop {
	char *pp_name;
	void (*pp_func)();
} *pp_list;

struct bootprop {
	struct bootprop *bp_next;
	char *bp_name;
	void *bp_val;
	int bp_len;
};

static struct bootprop *bp_list;

static int	find_pseudo(const char *);
static struct bootprop	*find_prop(const char *);
static struct bootprop	*alloc_prop(const char *);
static void	set_propval(struct bootprop *, void *, int);
static void	*bkmem_alloc(size_t size);
static void	*bkmem_zalloc(size_t size);
static void	setup_default_bootargs(void);


/*
 *  Return the length of the "name"d property's value.
 */
/*ARGSUSED*/
static int
bgetproplen(struct bootops *bop, const char *name)
{
	struct bootprop *bp;

	BOOT_DPRINTF("bgetproplen: name = %s\n", name);
	bp = find_prop(name);
	return (bp ? bp->bp_len : BOOT_FAILURE);
}

/*ARGSUSED*/
static int
bgetprop(struct bootops *bop, const char *name, void *value)
{
	struct bootprop *bp;

	BOOT_DPRINTF("bgetprop: name = %s\n", name);
	if (find_pseudo(name) == BOOT_SUCCESS)
		return (BOOT_SUCCESS);

	bp = find_prop(name);
	if (!bp)
		return (BOOT_FAILURE);

	/* Found the property in question; return its value */
	(void) bcopy(bp->bp_val, value, bp->bp_len);
	return (BOOT_SUCCESS);
}

/*ARGSUSED*/
static char *
bnextprop(struct bootops *bop, char *prev)
{
	struct bootprop *bp = find_prop(prev);

	if (bp == NULL || bp->bp_next == NULL)
		return (NULL);
	return (bp->bp_next->bp_name);
}

/*ARGSUSED*/
static int
bsetprop(struct bootops *bop, const char *name, void *value, int len)
{
	struct bootprop *bp;

	BOOT_DPRINTF("bsetprop: name = %s, len = %d\n", name, len);
	bp = find_prop(name);
	if (bp == NULL)
		bp = alloc_prop(name);

	set_propval(bp, value, len);
	return (BOOT_SUCCESS);
}

static int
find_pseudo(const char *name)
{
	struct pseudoprop *pp = pp_list;

	if (pp) {
		while (pp->pp_name) {
			if (strcmp(name, pp->pp_name) == 0) {
				(*pp->pp_func)();
				BOOT_DPRINTF("find_pseudo: prop = %s\n", name);
				return (BOOT_SUCCESS);
			}
			pp++;
		}
	}
	return (BOOT_FAILURE);
}

static struct bootprop *
find_prop(const char *name)
{
	struct bootprop *bp = bp_list;

	if (name == NULL || *name == '\0')
		return (bp);

	while (bp) {
		if (strcmp(name, bp->bp_name) == 0)
			break;
		bp = bp->bp_next;
	}
	return (bp);
}

static struct bootprop *
alloc_prop(const char *name)
{
	struct bootprop *bp = bkmem_zalloc(sizeof (*bp));

	BOOT_DPRINTF("alloc_prop: name = %s\n", name);
	bp->bp_name = bkmem_alloc(strlen(name) + 1);
	(void) strcpy(bp->bp_name, name);
	bp->bp_next = bp_list;
	bp_list = bp;

	return (bp);
}

static void
set_propval(struct bootprop *bp, void *value, int len)
{
	BOOT_DPRINTF("set_propval: name = %s\n", bp->bp_name);

	if (bp->bp_val) {
		prom_printf("WARNING: Ignore duplicated property: %s=%s\n",
			    bp->bp_name, (char *)value);
		return;
	}

	bp->bp_len = len;
	bp->bp_val = bkmem_alloc(len);
	bcopy(value, bp->bp_val, len);
}


/*
 * static void
 * parse_bootargs(void)
 *
 * Parse the boot arguments and place results in struct bootprop *bp.
 *
 * The format for the u-boot args is:
 *    prop1=value1[,prop2=value2,prop3=value3...]
 *
 */
static void
parse_bootargs(void)
{
	char *name, *val, *cp;
	char	bootargs[OBP_MAXPATHLEN];

	BOOT_DPRINTF("setup boot properties.\n");
	BOOT_DPRINTF("process command line bootargs: %s\n", kern_bootargs);

	/* We must copy boot argument because strtok() will destroy string. */
	snprintf(bootargs, sizeof(bootargs), "%s", kern_bootargs);

	cp = bootargs;
	while (cp && *cp) {
		name = (char *)strtok(cp, "=");
		val = (char *)strtok(NULL, "");
		if (val == NULL) {
			val = "true";
			cp = NULL;	/* terminate loop */
		} else if (*val != '\'' && *val != '\"') {
			if (*val == ',') {
				cp = val + 1;
				val = "";
			} else {
				cp = (char *)strtok(val, ",");
				cp = (char *)strtok(NULL, "");
			}
		} else {
			/* look for closing single or double quote */
			cp = val + 1;
			while (cp && *cp != *val)
				++cp;
			if (cp == NULL) {
				prom_printf("missing %c in property %s.\n",
					    *val, name);
			} else {
				*cp++ = '\0';
				if (*cp == ',')
					cp++;
				else  if (*cp != '\0') {
					prom_printf("syntax error in u-boot "
						    "option: ignore %s\n", cp);
					cp = NULL;	/* terminate */
				}
			}
			val++;
		}

		(void) bsetprop(NULL, name, val, strlen(val) + 1);
	}
}

/*ARGSUSED*/
static void
bprintf(struct bootops *bop, const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	prom_vprintf(fmt, adx);
	va_end(adx);
}

/*
 * Do nothing on ARM.
 */
/*ARGSUSED*/
static void
bdoint(struct bootops *bop, int intnum, struct bop_regs *rp)
{
}

/*
 * void
 * boot_init_commin(void)
 *	Platform-independt early bootstrap initialization.
 *
 * Remarks:
 *	The caller must initialize bsys_alloc in bootops before call.
 */
void
boot_init_common(void)
{
	/* Initialize function entries. */
	bootops->bsys_getproplen = bgetproplen;
	bootops->bsys_getprop = bgetprop;
	bootops->bsys_nextprop = bnextprop;
	bootops->bsys_printf = bprintf;
	bootops->bsys_doint = bdoint;

	/* Import boot properties. */
	parse_bootargs();

	/* Setup default boot arguments */
	setup_default_bootargs();
}

/*
 * static void *
 * bkmem_zalloc(size_t size)
 *	Allocate buffer for property use.
 *	bkmem_alloc() uses BOP_ALLOC() to allocate memory.
 */
static void *
bkmem_alloc(size_t size)
{
	return BOP_ALLOC(bootops, NULL, size, sizeof(char *));
}

/*
 * static void *
 * bkmem_zalloc(size_t size)
 *	Allocate zero-filled buffer.
 *	bkmem_zalloc() uses BOP_ALLOC() to allocate memory.
 */
static void *
bkmem_zalloc(size_t size)
{
	void	*p = bkmem_alloc(size);

	bzero(p, size);
	return p;
}

/*
 * static void
 * setup_default_bootargs()
 *	Setup default boot arguments.
 *	If the u-boot argument does not have mandatory argument,
 *	place the default value into bootprop list.
 */
static void
setup_default_bootargs()
{
	int i = 0;
	bootarg_t *bap;

	bap = &default_bootargs[i++];
	while (bap->name != NULL) {
		struct bootprop *bp = find_prop(bap->name);
		if (bp == NULL) {
			(void)bsetprop(NULL, bap->name,
				bap->value, strlen(bap->value) + 1);
		}
		bap = &default_bootargs[i++];
	}
}
