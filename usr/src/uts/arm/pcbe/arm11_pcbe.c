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

#ident  "@(#)uts/arm/pcbe/arm11_pcbe.c"

/*
 * Performance Counter Back-End for ARM11 MPCore.
 */

#include <sys/cpc_impl.h>
#include <sys/cpc_pcbe.h>
#include <sys/modctl.h>
#include <sys/inttypes.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/sdt.h>
#include <sys/archsystm.h>
#include <asm/cpufunc.h>
#include <sys/controlregs.h>
#include <sys/kmem.h>

/*
 * "Well known" bit fields in the ARM11 MPCore PMNC register.
 * The PMNC register controls the operation of the Count Register0(PMN0), 
 * Count Register1(PMN1), and Cycle Counter Register(CCNT).
 * The interfaces in libcpc should make these #defines uninteresting.
 */
#define PMNC_E 0x1		/* Enable */
#define PMNC_P 0x2		/* Count Register Reset on Write, UNP on Read */
#define PMNC_INTEN_PMN0 0x10	/* Interrupt Enable for the PMN0 */
#define PMNC_INTEN_PMN1 0x20	/* Interrupt Enable for the PMN1 */
#define PMNC_FLAG_PMN0 0x100	/* Overflow/Interrupt Flag for the PMN0 */
#define PMNC_FLAG_PMN1 0x200	/* Overflow/Interrupt Flag for the PMN1 */

#define PMNC_INTEN_MASK (PMNC_INTEN_PMN0 | PMNC_INTEN_PMN1)
#define PMNC_FLAG_MASK  (PMNC_FLAG_PMN0 | PMNC_FLAG_PMN1)
#define PMNC_EVTCOUNT0_MASK 0xff000
#define PMNC_EVTCOUNT1_MASK 0xff00000

#define PMNC_FLAG_SHIFT 8
#define PMNC_EVTCOUNT1_SHIFT 12
#define PMNC_EVTCOUNT0_SHIFT 20

#define PMNC_INIT       (PMNC_FLAG_MASK & \
			 ~(PMNC_EVTCOUNT0_MASK | PMNC_EVTCOUNT1_MASK | \
			   PMNC_INTEN_MASK))

#define MASK32  UINT64_C(0xffffffff)

#define MPCORE_NPICS     2
#define MPCORE_IMPLNAME_LEN 128   /* should be smaller than CPC_MAX_IMPL_NAME */
#define MPCORE_CPUREF   "See 3.4.27. c15, Performance Monitor Control "  \
			"Register(PMNC) of the \"ARM11 MPCore Processor "  \
			"Technical Reference Manual\""
#define MPCORE_ATTR_INT "int"
#define MPCORE_ATTRS    MPCORE_ATTR_INT /* specified as "attr1,attr3,..." */

static int arm11_pcbe_init(void);
static uint_t arm11_pcbe_ncounters(void);
static const char *arm11_pcbe_impl_name(void);
static const char *arm11_pcbe_cpuref(void);
static char *arm11_pcbe_list_events(uint_t picnum);
static char *arm11_pcbe_list_attrs(void);
static uint64_t arm11_pcbe_event_coverage(char *event);
static int arm11_pcbe_pic_index(char *picname);
static uint64_t	arm11_pcbe_overflow_bitmap(void);
static int arm11_pcbe_configure(uint_t picnum, char *event, uint64_t preset,
	uint32_t flags, uint_t nattrs, kcpc_attr_t *attrs, void **data,
	void *token);
static void arm11_pcbe_program(void *token);
static void arm11_pcbe_allstop(void);
static void arm11_pcbe_sample(void *token);
static void arm11_pcbe_free(void *config);

static pcbe_ops_t arm11_pcbe_ops = {
	PCBE_VER_1,
	CPC_CAP_OVERFLOW_INTERRUPT | CPC_CAP_OVERFLOW_PRECISE,
	arm11_pcbe_ncounters,
	arm11_pcbe_impl_name,
	arm11_pcbe_cpuref,
	arm11_pcbe_list_events,
	arm11_pcbe_list_attrs,
	arm11_pcbe_event_coverage,
	arm11_pcbe_overflow_bitmap,
	arm11_pcbe_configure,
	arm11_pcbe_program,
	arm11_pcbe_allstop,
	arm11_pcbe_sample,
	arm11_pcbe_free
};

typedef struct _arm11_pcbe_config {
	uint8_t		arm11_picno;   /* 0 for pic0 or 1 for pic1 */
	uint32_t	arm11_ctl;     /* Value to program in PMNC */
	uint64_t	arm11_rawpic;  /* Value of the each pic */
} arm11_pcbe_config_t;

typedef struct _arm11_event {
	const uint8_t	number;	     /* PMNC event select value */
	const char	*name;
} arm11_event_t;

/*
 * ARM11 MPCore events
 */ 
static arm11_event_t arm11_events[] = {
	{0x0,  "L1_ICACHE_MISS"},
	{0x1,  "IBUF_STALL"},
	{0x2,  "STALL_DATA_DEPENDENCY"},
	{0x3,  "IMICRO_TLB_MISS"},
	{0x4,  "DMICRO_TLB_MISS"},
	{0x5,  "BR_INSTR_EXEC"},
	{0x6,  "BR_INSTR_RETIRED"},
	{0x7,  "BR_MISS_PRED"},
	{0x8,  "INSTR_EXEC"},
	{0x9,  "INSTR_FOLDING_EXEC"},
	{0xa,  "L1_DCACHE_READ"},
	{0xb,  "L1_DCACHE_MISS"},
	{0xc,  "L1_DCACHE_WRITE"},
	{0xd,  "L1_DCACHE_WRITE_MISS"},
	{0xe,  "L1_DCACHE_LINE_EVICTION"},
	{0xf,  "PC_CHANGE"},
	{0x10,  "MAIN_TLB_MISS"},
	{0x11,  "EXTERNAL_MEM_REQUEST"},
	{0x12,  "LSU_QUEUE_FULL"},
	{0x13,  "SB_DRAINS"},
	{0x14,  "WRITE_MERGED_STOREBUF"},
	{0x15,  "LSU_SAFE"},
#ifdef MPCORE_R0P3
	{0x16,  "STB_DEADLOCK"},
#endif /* MPCORE_R0P3 */
	{0xff,  "CYCLE_CNT"},
};

static char *pic_events;
static size_t pic_events_sz;
static char implname[MPCORE_IMPLNAME_LEN];

static int
arm11_pcbe_init(void)
{
	int                     i, id, entry_num;
	arm11_event_t           *ev;

	/*
	 * Check if this system has an ARM11 MPCore processor.
	 */
	id = cpuid_getidcode(CPU);
	if ((id | CP15_ID_REVISION) != CP15_ID_ARM11MPCORE) {
		return 	(-1);
	}

	/*
	 * Initialize the return value of the arm11_pcbe_impl_name().
	 */
	(void) cpuid_getbrandstr(CPU, implname, sizeof (implname));

	/*
	 * Initialize the list of events for each PIC.
	 * Do two passes: one to compute the size necessary and another
	 * to copy the strings. Need room for event, comma, and NULL terminator.
	 */
	entry_num = sizeof (arm11_events) / sizeof (arm11_event_t);
	for (i = 0; i < entry_num; i++) {
		pic_events_sz += strlen(arm11_events[i].name) + 1;
	}

	pic_events = kmem_alloc(pic_events_sz + 1, KM_SLEEP);
	*pic_events = '\0';
	for (i = 0; i < entry_num; i++) {
		(void) strcat(pic_events, arm11_events[i].name);
		(void) strcat(pic_events, ",");
	}

	/*
	 * Remove trailing comma.
	 */
	pic_events[pic_events_sz - 1] = '\0';

	return (0);
}

static uint_t
arm11_pcbe_ncounters(void)
{
	return (MPCORE_NPICS);
}

static const char *
arm11_pcbe_impl_name(void)
{
	return (implname);
}

static const char *
arm11_pcbe_cpuref(void)
{
	return (MPCORE_CPUREF);
}

static char *
arm11_pcbe_list_events(uint_t picnum)
{
	ASSERT(picnum == 0 || picnum == 1);
	return (pic_events);
}

static char *
arm11_pcbe_list_attrs(void)
{
	return (MPCORE_ATTRS);
}

static uint64_t
arm11_pcbe_event_coverage(char *event)
{
	int i, entry_num;
	uint64_t bitmap = 0;

	entry_num = sizeof (arm11_events) / sizeof (arm11_event_t);
	for (i = 0; i < entry_num; i++) {
		if (strcmp(event, arm11_events[i].name) == 0) {
			bitmap = 0x3;
			break;
		}
	}
	return (bitmap);
}

/*
 * Check if counter overflow and clear it.
 */
static uint64_t
arm11_pcbe_overflow_bitmap(void)
{
	uint64_t	overflow;
	uint32_t	pmnc;

	pmnc = READ_CP15(0, c15, c12, 0);
	overflow = (pmnc & PMNC_FLAG_MASK) >> PMNC_FLAG_SHIFT;

	/* 
	 * Turn the overflow flag back by writing 1 on Flag of the PMNC 
	 * for the overflowed PMN. 
	 */
	if (overflow) {
		WRITE_CP15(0, c15, c12, 0, pmnc);
	} 
	return (overflow);
}

/*ARGSUSED*/
static int
arm11_pcbe_configure(uint_t picnum, char *eventname, uint64_t preset,
	uint32_t flags, uint_t nattrs, kcpc_attr_t *attrs, void **data,
	void *token)
{
	arm11_pcbe_config_t	*conf;
	uint32_t		pmnc = PMNC_INIT, inten;
	int			i, entry_num, picshift;

	/*
	 * If we've been handed an existing configuration, we need only preset
	 * the counter value.
	 */
	if (*data != NULL) {
		conf = (arm11_pcbe_config_t *) *data;
		conf->arm11_rawpic = preset & MASK32;
		return (0);
	}

	if (picnum == 0) {
		inten = PMNC_INTEN_PMN0;
		picshift = PMNC_EVTCOUNT0_SHIFT;
	} else if (picnum == 1) {
		inten = PMNC_INTEN_PMN1;
		picshift = PMNC_EVTCOUNT1_SHIFT;
	} else {
		return (CPC_INVALID_PICNUM);
	}	

	/* Decode the specified events */
	entry_num = sizeof (arm11_events) / sizeof (arm11_event_t);
	for (i = 0; i < entry_num; i++) {
		if (strcmp(eventname, arm11_events[i].name) == 0) {
			pmnc |= arm11_events[i].number << picshift;
			break;
		}
	}
	if (i == entry_num) {
		return (CPC_INVALID_EVENT);
	}

	/* Decode the specified attributes */
	for (i = 0; i < nattrs; i++) {
		if (strcmp(MPCORE_ATTR_INT, attrs[i].ka_name) == 0) {
			if (attrs[i].ka_val == 1) {
				pmnc |= inten;
			} else if (attrs[i].ka_val != 0) {
				return (CPC_ATTRIBUTE_OUT_OF_RANGE);
			}
		} else {
			return (CPC_INVALID_ATTRIBUTE);
		}		
	}

	/* Decode the specified flags */
	if (flags & CPC_OVF_NOTIFY_EMT) {
		pmnc |= inten;
	}

	conf = kmem_alloc(sizeof (arm11_pcbe_config_t), KM_SLEEP);
	conf->arm11_picno = picnum;
	conf->arm11_rawpic = preset & MASK32;
	conf->arm11_ctl = pmnc;

	*data = conf;

	return (0);
}

static void
arm11_pcbe_program(void *token)
{
	arm11_pcbe_config_t	*pic0;
	arm11_pcbe_config_t	*pic1;
	arm11_pcbe_config_t	*tmp;
	arm11_pcbe_config_t	empty = { 1, 0, 0 }; /* assume pic1 to start */
	uint32_t		pmnc = 0;

	if ((pic0 = kcpc_next_config(token, NULL, NULL)) == NULL) {
		panic("arm11_pcbe: token %p has no configs", token);
	}

	if ((pic1 = kcpc_next_config(token, pic0, NULL)) == NULL) {
		pic1 = &empty;
	}

	if (pic0->arm11_picno != 0) {
		empty.arm11_picno = 0;
		tmp = pic1;
		pic1 = pic0;
		pic0 = tmp;
	}

	ASSERT(pic0->arm11_picno == 0 && pic1->arm11_picno == 1);
	pmnc |= (pic0->arm11_ctl | pic1->arm11_ctl | PMNC_E);

	WRITE_CP15(0, c15, c12, 2, (uint32_t)(pic0->arm11_rawpic & MASK32));
	WRITE_CP15(0, c15, c12, 3, (uint32_t)(pic1->arm11_rawpic & MASK32));

	WRITE_CP15(0, c15, c12, 0, pmnc);
}

static void
arm11_pcbe_allstop(void)
{
	WRITE_CP15(0, c15, c12, 0, (READ_CP15(0, c15, c12, 0) & ~PMNC_E));
}

static void
arm11_pcbe_sample(void *token)
{
	arm11_pcbe_config_t	*pic0;
	arm11_pcbe_config_t	*pic1;
	arm11_pcbe_config_t	*swap;
	arm11_pcbe_config_t	empty = { 1, 0, 0 }; /* assume pic1 to start */
	uint64_t		tmp;
	uint64_t		*pic0_data;
	uint64_t		*pic1_data;
	uint64_t		*dtmp;
	uint64_t		curpic[2];
	int64_t			diff;

	if ((pic0 = kcpc_next_config(token, NULL, &pic0_data)) == NULL) {
		panic("arm11_pcbe: token %p has no configs", token);
	}

	if ((pic1 = kcpc_next_config(token, pic0, &pic1_data)) == NULL) {
		pic1 = &empty;
		pic1_data = &tmp;
	}

	if (pic0->arm11_picno != 0) {
		empty.arm11_picno = 0;
		swap = pic0;
		pic0 = pic1;
		pic1 = swap;
		dtmp = pic0_data;
		pic0_data = pic1_data;
		pic1_data = dtmp;
	}
	ASSERT(pic0->arm11_picno == 0 && pic1->arm11_picno == 1);

	curpic[0] = READ_CP15(0, c15, c12, 2);
	curpic[1] = READ_CP15(0, c15, c12, 3);

	DTRACE_PROBE1(arm11__curpic0, uint64_t, curpic[0]);
	DTRACE_PROBE1(arm11__curpic1, uint64_t, curpic[1]);

	diff = curpic[0] - pic0->arm11_rawpic;
	if (diff < 0) {
		diff += (1LL << 32);
	}
	*pic0_data += diff;
	pic0->arm11_rawpic = *pic0_data & MASK32;

	diff = curpic[1] - pic1->arm11_rawpic;
	if (diff < 0) {
		diff += (1LL << 32);
	}
	*pic1_data += diff;
	pic1->arm11_rawpic = *pic1_data & MASK32;
}

static void
arm11_pcbe_free(void *config)
{
	kmem_free(config, sizeof (arm11_pcbe_config_t));
}

static struct modlpcbe modlpcbe = {
	&mod_pcbeops,
	"ARM11 MPCore Performance Counters v1.0",
	&arm11_pcbe_ops
};

static struct modlinkage modl = {
	MODREV_1,
	&modlpcbe,
};

int
MODDRV_ENTRY_INIT(void)
{
	if (arm11_pcbe_init() != 0) {
		return (ENOTSUP);
	}
	return (mod_install(&modl));
}

int
MODDRV_ENTRY_FINI(void)
{
	int ret, i;

	if ((ret = mod_remove(&modl)) == 0) {
		kmem_free(pic_events, pic_events_sz + 1);
	}
	return (ret);
}

int
MODDRV_ENTRY_INFO(struct modinfo *mi)
{
	return (mod_info(&modl, mi));
}
