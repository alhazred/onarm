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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

/*
 * Serial I/O driver for 8250/16450/16550A/16650/16750 chips.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/termio.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/strtty.h>
#include <sys/debug.h>
#include <sys/kbio.h>
#include <sys/cred.h>
#include <sys/stat.h>
#include <sys/consdev.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/strsun.h>
#ifdef DEBUG
#include <sys/promif.h>
#endif
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ne_uart.h>
#include <sys/ne_uart_impl.h>
#include <sys/policy.h>

/*
 * set the RX FIFO trigger_level to half the RX FIFO size for now
 * we may want to make this configurable later.
 */
static	int neuart_trig_level = FIFO_TRIG_8;

int neuart_drain_check = 15000000;	/* tunable: exit drain check time */
int neuart_min_dtr_low = 500000;	/* tunable: minimum DTR down time */
int neuart_min_utbrk = 100000;		/* tunable: minumum untimed brk time */

int neuart_maxchip = NEUART16750;  /* tunable: limit chip support we look for */

/*
 * Just in case someone has a chip with broken loopback mode, we provide a
 * means to disable the loopback test. By default, we only loopback test
 * UARTs which look like they have FIFOs bigger than 16 bytes.
 * Set to 0 to suppress test, or to 2 to enable test on any size FIFO.
 */
int neuart_fifo_test = 1;		/* tunable: set to 0, 1, or 2 */

/*
 * Allow ability to switch off testing of the scratch register.
 * Some UART emulators might not have it. This will also disable the test
 * for Exar/Startech ST16C650, as that requires use of the SCR register.
 */
int neuart_scr_test = 1;	/* tunable: set to 0 to disable SCR reg test */

/*
 * As we don't yet support on-chip flow control, it's a bad idea to put a
 * large number of characters in the TX FIFO, since if other end tells us
 * to stop transmitting, we can only stop filling the TX FIFO, but it will
 * still carry on draining by itself, so remote end still gets what's left
 * in the FIFO.
 */
int neuart_max_tx_fifo = 16;	/* tunable: max fill of TX FIFO */

int neuart_default_baudrate;	/* default baud rate */

/*
 * Flag to switch DLAB bit of LCR in neuart_reset_fifo().
 */
int neuart_fiforeset_dlab = NEUART_FIFORESET_DLABON;

#define	async_stopc	async_ttycommon.t_stopc
#define	async_startc	async_ttycommon.t_startc

#define	NEUART_INIT	1
#define	NEUART_NOINIT	0

/* enum value for sw and hw flow control action */
typedef enum {
	FLOW_CHECK,
	FLOW_STOP,
	FLOW_START
} neuart_flowc_action;

#ifdef DEBUG
/* Output msgs during driver initialization. */
#define	NEUART_DEBUG_INIT	0x0001
/* Report characters received during int. */
#define	NEUART_DEBUG_INPUT	0x0002
/* Output msgs when wait for xmit to finish. */
#define	NEUART_DEBUG_EOT	0x0004
/* Output msgs when driver open/close called */
#define	NEUART_DEBUG_CLOSE	0x0008
/* Output msgs when H/W flowcontrol is active */
#define	NEUART_DEBUG_HFLOW	0x0010
/* Output each proc name as it is entered. */
#define	NEUART_DEBUG_PROCS	0x0020
/* Output value of Interrupt Service Reg. */
#define	NEUART_DEBUG_STATE	0x0040
/* Output value of Interrupt Service Reg. */
#define	NEUART_DEBUG_INTR	0x0080
/* Output msgs about output events. */
#define	NEUART_DEBUG_OUT	0x0100
/* Output msgs when xmit is enabled/disabled */
#define	NEUART_DEBUG_BUSY	0x0200
/* Output msgs about modem status & control. */
#define	NEUART_DEBUG_MODEM	0x0400
/* Output msgs about modem status & control. */
#define	NEUART_DEBUG_MODM2	0x0800
/* Output msgs about ioctl messages. */
#define	NEUART_DEBUG_IOCTL	0x1000
/* Output msgs about chip identification. */
#define	NEUART_DEBUG_CHIP	0x2000
/* Output msgs when S/W flowcontrol is active */
#define	NEUART_DEBUG_SFLOW	0x4000
#define	NEUART_DEBUG(x) (debug & (x))
static	int debug  = 0;
#else
#define	NEUART_DEBUG(x) B_FALSE
#endif

/* pnpISA compressed device ids */
#define	pnpMTS0219 0xb6930219	/* Multitech MT5634ZTX modem */

#define	NEUART_DRAINCHK_DIV	10
#define	NEUART_XHREWAIT		260
/*
 * PPS (Pulse Per Second) support.
 */
void ddi_hardpps();
/*
 * This is protected by the neuart_excl_hi of the port on which PPS event
 * handling is enabled.  Note that only one port should have this enabled at
 * any one time.  Enabling PPS handling on multiple ports will result in
 * unpredictable (but benign) results.
 */
static struct ppsclockev neuart_ppsev;

#ifdef PPSCLOCKLED
/* XXX Use these to observe PPS latencies and jitter on a scope */
#define	LED_ON
#define	LED_OFF
#else
#define	LED_ON
#define	LED_OFF
#endif

static	int max_neuart_instance = -1;

static	uint_t	neuart_softintr(caddr_t intarg);
static	uint_t	neuart_intr(caddr_t argneuart);

static boolean_t abort_charseq_recognize(uchar_t ch);

/* The neuart interrupt entry points */
static void	neuart_txint(struct neuart_com *neuart);
static void	neuart_rxint(struct neuart_com *neuart, uchar_t lsr);
static void	neuart_msint(struct neuart_com *neuart);
static void	neuart_softint(struct neuart_com *neuart);

static void	neuart_ioctl(struct neuart_asyncline *neuartas,
		    queue_t *q, mblk_t *mp);
static void	neuart_reioctl(void *unit);
static void	neuart_iocdata(queue_t *q, mblk_t *mp);
static void	neuart_restart(void *arg);
static void	neuart_start(struct neuart_asyncline *neuartas);
static void	neuart_nstart(struct neuart_asyncline *neuartas, int mode);
static void	neuart_resume(struct neuart_asyncline *neuartas);
static void	neuart_program(struct neuart_com *neuart, int mode);
static void	neuart_init(struct neuart_com *neuart);
static void	neuart_waiteot(struct neuart_com *neuart);
static void	neuart_putchar(cons_polledio_arg_t, uchar_t c);
static int	neuart_getchar(cons_polledio_arg_t);
static boolean_t	neuart_ischar(cons_polledio_arg_t);

static int	neuart_mctl(struct neuart_com *, int, int);
static int	neuart_todm(int, int);
static int	dmto_neuart(int);
/*PRINTFLIKE2*/
static void	neuart_error(int level, const char *fmt, ...) __KPRINTFLIKE(2);
static void	neuart_parse_mode(dev_info_t *devi, struct neuart_com *neuart);
static void	neuart_soft_state_free(struct neuart_com *);
static char	*neuart_hw_name(struct neuart_com *neuart);
static void	neuart_hold_utbrk(void *arg);
static void	neuart_resume_utbrk(struct neuart_asyncline *neuartas);
static void	neuart_dtr_free(struct neuart_asyncline *neuartas);
static int	neuart_identify_chip(dev_info_t *devi,
		    struct neuart_com *neuart);
static void	neuart_reset_fifo(struct neuart_com *neuart, uchar_t flags);
static int	neuart_getproperty(dev_info_t *devi, struct neuart_com *neuart,
		    const char *property);
static boolean_t	neuart_flowcontrol_sw_input(struct neuart_com *neuart,
			    neuart_flowc_action onoff, int type);
static void	neuart_flowcontrol_sw_output(struct neuart_com *neuart,
		    neuart_flowc_action onoff);
static void	neuart_flowcontrol_hw_input(struct neuart_com *neuart,
		    neuart_flowc_action onoff, int type);
static void	neuart_flowcontrol_hw_output(struct neuart_com *neuart,
		    neuart_flowc_action onoff);

#define	GET_PROP(devi, pname, pflag, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
		(pflag), (pname), (caddr_t)(pval), (plen)))

static ddi_iblock_cookie_t neuart_soft_iblock;
ddi_softintr_t neuart_softintr_id;
static	int neuart_addedsoft = 0;
int	neuart_softpend;	/* soft interrupt pending */
kmutex_t neuart_soft_lock;	/* lock protecting neuart_softpend */
kmutex_t neuart_glob_lock; /* lock protecting global data manipulation */
void *neuart_soft_state;

static int *com_ports;
static uint_t num_com_ports;

/*
 * Baud rate table. Indexed by #defines found in sys/termios.h
 */
ushort_t neuart_spdtab[] = {
	NEUART_B0,	/* 0 baud rate */
	NEUART_B50,	/* 50 baud rate */
	NEUART_B75,	/* 75 baud rate */
	NEUART_B110,	/* 110 baud rate (%0.026) */
	NEUART_B134,	/* 134 baud rate (%0.058) */
	NEUART_B150,	/* 150 baud rate */
	NEUART_B200,	/* 200 baud rate */
	NEUART_B300,	/* 300 baud rate */
	NEUART_B600,	/* 600 baud rate */
	NEUART_B1200,	/* 1200 baud rate */
	NEUART_B1800,	/* 1800 baud rate */
	NEUART_B2400,	/* 2400 baud rate */
	NEUART_B4800,	/* 4800 baud rate */
	NEUART_B9600,	/* 9600 baud rate */
	NEUART_B19200,	/* 19200 baud rate */
	NEUART_B38400,	/* 38400 baud rate */

	NEUART_B57600,	/* 57600 baud rate */
	NEUART_B76800,	/* 76800 baud rate not supported */
	NEUART_B115200,	/* 115200 baud rate */
	NEUART_B153600,	/* 153600 baud rate not supported */
	NEUART_B230400,	/* 0x8002 (SMC chip) 230400 baud rate not supported */
	NEUART_B307200,	/* 307200 baud rate not supported */
	NEUART_B460800,	/* 0x8001 (SMC chip) 460800 baud rate not supported */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
	0x0,	/* unused */
};

static int neuart_rsrv(queue_t *q);
static int neuart_open(queue_t *rq, dev_t *dev, int flag,
	int sflag, cred_t *cr);
static int neuart_close(queue_t *q, int flag, cred_t *credp);
static int neuart_wput(queue_t *q, mblk_t *mp);

struct module_info neuart_info = {
	0,
	"ne_uart",
	0,
	INFPSZ,
	4096,
	128
};

static struct qinit neuart_rint = {
	putq,
	neuart_rsrv,
	neuart_open,
	neuart_close,
	NULL,
	&neuart_info,
	NULL
};

static struct qinit neuart_wint = {
	neuart_wput,
	NULL,
	NULL,
	NULL,
	NULL,
	&neuart_info,
	NULL
};

struct streamtab neuart_str_info = {
	&neuart_rint,
	&neuart_wint,
	NULL,
	NULL
};

static int neuartinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int neuart_probe(dev_info_t *);
static int neuart_attach(dev_info_t *, ddi_attach_cmd_t);
static int neuart_detach(dev_info_t *, ddi_detach_cmd_t);

static 	struct cb_ops cb_neuart_ops = {
	nodev,			/* cb_open */
	nodev,			/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&neuart_str_info,	/* cb_stream */
	D_MP			/* cb_flag */
};

struct dev_ops neuart_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	neuartinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	neuart_probe,		/* devo_probe */
	neuart_attach,		/* devo_attach */
	neuart_detach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_neuart_ops,		/* devo_cb_ops */
};

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"Navi Engine UART driver",
	&neuart_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
MODDRV_ENTRY_INIT(void)
{
	int i;

	i = ddi_soft_state_init(&neuart_soft_state,
				sizeof (struct neuart_com), 2);
	if (i == 0) {
		mutex_init(&neuart_glob_lock, NULL, MUTEX_DRIVER, NULL);
		com_ports = NULL;
		if ((i = mod_install(&modlinkage)) != 0) {
			mutex_destroy(&neuart_glob_lock);
			ddi_soft_state_fini(&neuart_soft_state);
		} else {
			DEBUGCONT2(NEUART_DEBUG_INIT, "%s, debug = %x\n",
			    modldrv.drv_linkinfo, debug);
		}
	}
	return (i);
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	int i;

	if ((i = mod_remove(&modlinkage)) == 0) {
		DEBUGCONT1(NEUART_DEBUG_INIT, "%s unloading\n",
		    modldrv.drv_linkinfo);
		ASSERT(max_neuart_instance == -1);
		mutex_destroy(&neuart_glob_lock);
		if (neuart_addedsoft)
			ddi_remove_softintr(neuart_softintr_id);
		neuart_addedsoft = 0;
		/* free "motherboard-serial-ports" property if allocated */
		if (com_ports != NULL)
		    ddi_prop_free(com_ports);
		com_ports = NULL;
		mutex_destroy(&neuart_soft_lock);
		ddi_soft_state_fini(&neuart_soft_state);
	}
	return (i);
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
neuart_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct neuart_com *neuart;
	struct neuart_asyncline *neuartas;

	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(devi);	/* find out which unit */

	neuart = ddi_get_soft_state(neuart_soft_state, instance);
	if (neuart == NULL)
		return (DDI_FAILURE);
	neuartas = neuart->neuart_priv;

	DEBUGNOTE2(NEUART_DEBUG_INIT, "ne_uart%d: %s shutdown.",
	    instance, neuart_hw_name(neuart));

	/* cancel DTR hold timeout */
	if (neuartas->async_dtrtid != 0) {
		(void) untimeout(neuartas->async_dtrtid);
		neuartas->async_dtrtid = 0;
	}

	/* remove all minor device node(s) for this device */
	ddi_remove_minor_node(devi, NULL);

	ddi_remove_intr(devi, 0, neuart->neuart_iblock);

	mutex_enter(&neuart_glob_lock);
	mutex_destroy(&neuart->neuart_excl);
	mutex_destroy(&neuart->neuart_excl_hi);
	cv_destroy(&neuartas->async_flags_cv);
	ddi_regs_map_free(&neuart->neuart_iohandle);
	neuart_soft_state_free(neuart);
	mutex_exit(&neuart_glob_lock);
	DEBUGNOTE1(NEUART_DEBUG_INIT, "ne_uart%d: shutdown complete", instance);
	return (DDI_SUCCESS);
}

/*
 * neuart_probe
 * We don't bother probing for the hardware, as since Solaris 2.6, device
 * nodes are only created for auto-detected hardware or nodes explicitly
 * created by the user, e.g. via the DCA. However, we should check the
 * device node is at least vaguely usable, i.e. we have a block of 8 i/o
 * ports. This prevents attempting to attach to bogus serial ports which
 * some BIOSs still partially report when they are disabled in the BIOS.
 */
static int
neuart_probe(dev_info_t *devi)
{
	int instance;
	int ret = DDI_PROBE_FAILURE;
	int regnum;
	int reglen, nregs;
	struct reglist {
		uint_t bustype;
		int base;
		int size;
	} *reglist = NULL;

	instance = ddi_get_instance(devi);

	/* Retrieve "reg" property */

	if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
	    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "neuart_probe: \"reg\" property not found "
		    "in devices property list");
		goto probedone;
	}

	/* find I/O bus register property */

	nregs = reglen / sizeof (struct reglist);
	for (regnum = 0; regnum < nregs; regnum++) {
		if (reglist[regnum].bustype == 1)
			break;
	}
	if (regnum >= nregs) {
		DEBUGCONT1(NEUART_DEBUG_INIT,
		    "ne_uart%dprobe: No I/O register property", instance);
		goto probedone;
	}

	if (reglist[regnum].size < UART_REGSPAC) {
		/* not enough registers for a UART */
		DEBUGCONT1(NEUART_DEBUG_INIT,
		    "ne_uart%dprobe: Invalid I/O register property", instance);
		goto probedone;
	}

	ret = DDI_PROBE_DONTCARE;	/* OK, looks like it might be usable */

probedone:
	if (reglist != NULL)
		kmem_free(reglist, reglen);

	DEBUGCONT2(NEUART_DEBUG_INIT, "ne_uart%dprobe: ret=%s\n", instance,
	    ret == DDI_PROBE_DONTCARE ? "DDI_PROBE_DONTCARE" :
	    "DDI_PROBE_FAILURE");

	return (ret);
}

static int
neuart_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	int mcr;
	int ret;
	int regnum = 0;
	int i;
	int indx_cnt;
	struct neuart_com *neuart;
	char name[40];
	int status;
	uintptr_t	regaddr = 0;
	static ddi_device_acc_attr_t ioattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	instance = ddi_get_instance(devi);	/* find out which unit */
	ret = ddi_soft_state_zalloc(neuart_soft_state, instance);
	if (ret != DDI_SUCCESS)
		return (DDI_FAILURE);
	neuart = ddi_get_soft_state(neuart_soft_state, instance);
	ASSERT(neuart != NULL);	/* can't fail - we only just allocated it */
	neuart->neuart_unit = instance;
	mutex_enter(&neuart_glob_lock);
	if (instance > max_neuart_instance)
		max_neuart_instance = instance;
	mutex_exit(&neuart_glob_lock);

	/*CSTYLED*/
	{
		int reglen, nregs;
		int i;
		struct {
			uint_t bustype;
			int base;
			int size;
		} *reglist;

		/* new probe */
		if (ddi_getlongprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
		    "reg", (caddr_t)&reglist, &reglen) != DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
				"neuart_attach: reg property not found "
				"in devices property list");
			mutex_enter(&neuart_glob_lock);
			neuart_soft_state_free(neuart);
			mutex_exit(&neuart_glob_lock);
			return (DDI_PROBE_FAILURE);
		}
		regnum = -1;
		nregs = reglen / sizeof (*reglist);
		for (i = 0; i < nregs; i++) {
			switch (reglist[i].bustype) {
			case 1:			/* I/O bus reg property */
				if (regnum == -1) { /* only use the first one */
					regnum = i;
					regaddr = reglist[i].base;
				}
				break;

			case pnpMTS0219:	/* Multitech MT5634ZTX modem */
				/* Venus chipset can't do loopback test */
				neuart->neuart_flags2 |= NEUART2_NO_LOOPBACK;
				break;

			default:
				break;
			}
		}
		kmem_free(reglist, reglen);
	}

	if (regnum < 0 ||
	    ddi_regs_map_setup(devi, regnum, (caddr_t *)&neuart->neuart_ioaddr,
	    (offset_t)0, (offset_t)0, &ioattr, &neuart->neuart_iohandle)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "ne_uart%d: could not map UART registers @ %p",
		    instance, (void *)neuart->neuart_ioaddr);

		mutex_enter(&neuart_glob_lock);
		neuart_soft_state_free(neuart);
		mutex_exit(&neuart_glob_lock);
		return (DDI_FAILURE);
	}

	DEBUGCONT2(NEUART_DEBUG_INIT, "ne_uart%dattach: UART @ %p\n",
	    instance, (void *)neuart->neuart_ioaddr);

	mutex_enter(&neuart_glob_lock);
	if (com_ports == NULL) {	/* need to initialize com_ports */
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, devi, 0,
		    "motherboard-serial-ports", &com_ports, &num_com_ports) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN,
				"ne_uart%d: No motherboard-serial-ports.",
				instance);
			neuart_soft_state_free(neuart);
			mutex_exit(&neuart_glob_lock);
			return DDI_FAILURE;
		}
		if (num_com_ports > 10) {
			/* We run out of single digits for device properties */
			num_com_ports = 10;
			cmn_err(CE_WARN,
			    "More than %d motherboard-serial-ports",
			    num_com_ports);
		}
	}
	mutex_exit(&neuart_glob_lock);

	/*
	 * Lookup the i/o address to see if this is a standard COM port
	 * in which case we assign it the correct tty[a-d] to match the
	 * COM port number, or some other i/o address in which case it
	 * will be assigned /dev/term/[0123...] in some rather arbitrary
	 * fashion.
	 */

	for (i = 0; i < num_com_ports; i++) {
		if (regaddr == (uintptr_t)com_ports[i]) {
			neuart->neuart_com_port = i + 1;
			break;
		}
	}

	/*
	 * It appears that there was async hardware that on reset
	 * did not clear ICR.  Hence when we get to
	 * ddi_get_iblock_cookie below, this hardware would cause
	 * the system to hang if there was input available.
	 */

	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR, 0x00);

	/* establish default usage */
	neuart->neuart_mcr |= RTS|DTR;		/* do use RTS/DTR after open */
	neuart->neuart_lcr = STOP1|BITS8;	/* default to 1 stop 8 bits */

	neuart_default_baudrate = B9600;
	indx_cnt = sizeof (neuart_spdtab)/sizeof (*neuart_spdtab);
	for (i = 0; i < indx_cnt; i++){
		if ((UART_DEFAULT_BAUDRATE_DLL == (neuart_spdtab[i] & 0xff)) &&
		    (UART_DEFAULT_BAUDRATE_DLH ==
				((neuart_spdtab[i] >> 8) & 0xff)))
			neuart_default_baudrate = i;
	}

	/* set to default baud rate */
	neuart->neuart_bidx = neuart_default_baudrate;
#ifdef DEBUG
	neuart->neuart_msint_cnt = 0;		/* # of times in neuart_msint */
#endif
	mcr = 0;				/* don't enable until open */

	if (neuart->neuart_com_port != 0) {
		/*
		 * For motherboard ports, emulate tty eeprom properties.
		 * Actually, we can't tell if a port is motherboard or not,
		 * so for "motherboard ports", read standard DOS COM ports.
		 */
		switch (neuart_getproperty(devi, neuart, "ignore-cd")) {
		case 0:				/* *-ignore-cd=False */
			DEBUGCONT1(NEUART_DEBUG_MODEM,
			    "ne_uart%dattach: clear NEUART_IGNORE_CD\n",
			    instance);
			/* wait for cd */
			neuart->neuart_flags &= ~NEUART_IGNORE_CD;
			break;
		case 1:				/* *-ignore-cd=True */
			/*FALLTHRU*/
		default:			/* *-ignore-cd not defined */
			/*
			 * We set rather silly defaults of soft carrier on
			 * and DTR/RTS raised here because it might be that
			 * one of the motherboard ports is the system console.
			 */
			DEBUGCONT1(NEUART_DEBUG_MODEM,
			    "ne_uart%dattach: set NEUART_IGNORE_CD, "
			    "set RTS & DTR\n",
			    instance);
			mcr = neuart->neuart_mcr;	/* rts/dtr on */
			neuart->neuart_flags |= NEUART_IGNORE_CD;/* ignore cd */
			break;
		}

		/* Property for not raising DTR/RTS */
		switch (neuart_getproperty(devi, neuart, "rts-dtr-off")) {
		case 0:				/* *-rts-dtr-off=False */
			neuart->neuart_flags |= NEUART_RTS_DTR_OFF;    /* OFF */
			mcr = neuart->neuart_mcr;		/* rts/dtr on */
			DEBUGCONT1(NEUART_DEBUG_MODEM, "ne_uart%dattach: "
			    "NEUART_RTS_DTR_OFF set and DTR & RTS set\n",
			    instance);
			break;
		case 1:				/* *-rts-dtr-off=True */
			/*FALLTHRU*/
		default:			/* *-rts-dtr-off undefined */
			break;
		}

		/* Parse property for tty modes */
		neuart_parse_mode(devi, neuart);
	} else {
		DEBUGCONT1(NEUART_DEBUG_MODEM,
		    "ne_uart%dattach: clear NEUART_IGNORE_CD, "
		    "clear RTS & DTR\n", instance);
		neuart->neuart_flags &= ~NEUART_IGNORE_CD;    /* wait for cd */
	}

	/*
	 * Initialize the port with default settings.
	 */

	neuart->neuart_fifo_buf = 1;
	neuart->neuart_use_fifo = FIFO_OFF;

	/*
	 * Get icookie for mutexes initialization
	 */
	if ((ddi_get_iblock_cookie(devi, 0, &neuart->neuart_iblock) !=
	    DDI_SUCCESS) ||
	    (ddi_get_soft_iblock_cookie(devi, DDI_SOFTINT_MED,
	    &neuart_soft_iblock) != DDI_SUCCESS)) {
		ddi_regs_map_free(&neuart->neuart_iohandle);
		cmn_err(CE_CONT,
		    "ne_uart%d: could not hook interrupt for UART @ %p\n",
		    instance, (void *)neuart->neuart_ioaddr);
		mutex_enter(&neuart_glob_lock);
		neuart_soft_state_free(neuart);
		mutex_exit(&neuart_glob_lock);
		return (DDI_FAILURE);
	}

	/*
	 * Initialize mutexes before accessing the hardware
	 */
	mutex_init(&neuart->neuart_excl, NULL, MUTEX_DRIVER,
			neuart_soft_iblock);
	mutex_init(&neuart->neuart_excl_hi, NULL, MUTEX_DRIVER,
		(void *)neuart->neuart_iblock);

	mutex_enter(&neuart->neuart_excl);
	mutex_enter(&neuart->neuart_excl_hi);

	if (neuart_identify_chip(devi, neuart) != DDI_SUCCESS) {
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		mutex_destroy(&neuart->neuart_excl);
		mutex_destroy(&neuart->neuart_excl_hi);
		ddi_regs_map_free(&neuart->neuart_iohandle);
		cmn_err(CE_CONT, "Cannot identify UART chip at %p\n",
		    (void *)neuart->neuart_ioaddr);
		mutex_enter(&neuart_glob_lock);
		neuart_soft_state_free(neuart);
		mutex_exit(&neuart_glob_lock);
		return (DDI_FAILURE);
	}

	/* disable all interrupts */
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR, 0);
	/* select baud rate generator */
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR, DLAB);
	/* Set the baud rate to default */
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + (DAT+DLL),
		neuart_spdtab[neuart->neuart_bidx] & 0xff);
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + (DAT+DLH),
		(neuart_spdtab[neuart->neuart_bidx] >> 8) & 0xff);
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		neuart->neuart_lcr);
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR, mcr);

	mutex_exit(&neuart->neuart_excl_hi);
	mutex_exit(&neuart->neuart_excl);

	/*
	 * Set up the other components of the neuart_com structure
	 * for this port.
	 */
	neuart->neuart_dip = devi;

	mutex_enter(&neuart_glob_lock);
	if (neuart_addedsoft == 0) { /* install the soft interrupt handler */
		if (ddi_add_softintr(devi, DDI_SOFTINT_MED,
		    &neuart_softintr_id, NULL, 0, neuart_softintr,
		    (caddr_t)0) != DDI_SUCCESS) {
			mutex_destroy(&neuart->neuart_excl);
			mutex_destroy(&neuart->neuart_excl_hi);
			ddi_regs_map_free(&neuart->neuart_iohandle);
			mutex_exit(&neuart_glob_lock);
			cmn_err(CE_CONT,
				"Can not set soft interrupt "
				"for NE_UART driver\n");
			mutex_enter(&neuart_glob_lock);
			neuart_soft_state_free(neuart);
			mutex_exit(&neuart_glob_lock);
			return (DDI_FAILURE);
		}
		mutex_init(&neuart_soft_lock, NULL, MUTEX_DRIVER,
			(void *)neuart->neuart_iblock);
		neuart_addedsoft++;
	}
	mutex_exit(&neuart_glob_lock);

	neuart_init(neuart);	/* initialize the neuart_asyncline structure */

	/*
	 * Install interrupt handler for this device.
	 */
	if (ddi_add_intr(devi, 0, NULL, 0, neuart_intr,
	    (caddr_t)neuart) != DDI_SUCCESS) {
		mutex_destroy(&neuart->neuart_excl);
		mutex_destroy(&neuart->neuart_excl_hi);
		ddi_regs_map_free(&neuart->neuart_iohandle);
		cmn_err(CE_CONT,
			"Can not set device interrupt for NE_UART driver\n");
		mutex_enter(&neuart_glob_lock);
		neuart_soft_state_free(neuart);
		mutex_exit(&neuart_glob_lock);
		return (DDI_FAILURE);
	}

	/* create minor device nodes for this device */
	if (neuart->neuart_com_port != 0) {
		/*
		 * For DOS COM ports, add letter suffix so
		 * devfsadm can create correct link names.
		 */
		name[0] = neuart->neuart_com_port + 'a' - 1;
		name[1] = '\0';
	} else {
		/*
		 * ISA port which isn't a standard DOS COM
		 * port needs no further qualification.
		 */
		name[0] = '\0';
	}
	status = ddi_create_minor_node(devi, name, S_IFCHR, instance,
	    neuart->neuart_com_port != 0 ? DDI_NT_SERIAL_MB :
					   DDI_NT_SERIAL, NULL);
	if (status == DDI_SUCCESS) {
		(void) strcat(name, ",cu");
		status = ddi_create_minor_node(devi, name, S_IFCHR,
		    OUTLINE | instance,
		    neuart->neuart_com_port != 0 ? DDI_NT_SERIAL_MB_DO :
		    DDI_NT_SERIAL_DO, NULL);
	}

	if (status != DDI_SUCCESS) {
		struct neuart_asyncline *neuartas = neuart->neuart_priv;

		ddi_remove_minor_node(devi, NULL);
		ddi_remove_intr(devi, 0, neuart->neuart_iblock);
		mutex_destroy(&neuart->neuart_excl);
		mutex_destroy(&neuart->neuart_excl_hi);
		cv_destroy(&neuartas->async_flags_cv);
		ddi_regs_map_free(&neuart->neuart_iohandle);
		mutex_enter(&neuart_glob_lock);
		neuart_soft_state_free(neuart);
		mutex_exit(&neuart_glob_lock);
		return (DDI_FAILURE);
	}

	/*
	 * Fill in the polled I/O structure.
	 */
	neuart->polledio.cons_polledio_version = CONSPOLLEDIO_V0;
	neuart->polledio.cons_polledio_argument = (cons_polledio_arg_t)neuart;
	neuart->polledio.cons_polledio_putchar = neuart_putchar;
	neuart->polledio.cons_polledio_getchar = neuart_getchar;
	neuart->polledio.cons_polledio_ischar = neuart_ischar;
	neuart->polledio.cons_polledio_enter = NULL;
	neuart->polledio.cons_polledio_exit = NULL;

	ddi_report_dev(devi);
	DEBUGCONT1(NEUART_DEBUG_INIT, "ne_uart%dattach: done\n", instance);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
neuartinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	dev_t dev = (dev_t)arg;
	int instance, error;
	struct neuart_com *neuart;

	instance = UNIT(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		neuart = ddi_get_soft_state(neuart_soft_state, instance);
		if ((neuart == NULL) || (neuart->neuart_dip == NULL))
			error = DDI_FAILURE;
		else {
			*result = (void *) neuart->neuart_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(intptr_t)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/* neuart_getproperty -- walk through all name variants until we find a match */

static int
neuart_getproperty(dev_info_t *devi,
	struct neuart_com *neuart, const char *property)
{
	int len;
	int ret;
	char letter = neuart->neuart_com_port + 'a' - 1;	/* for ttya */
	char number = neuart->neuart_com_port + '0';		/* for COM1 */
	char val[40];
	char name[40];

	/* Property for ignoring DCD */
	(void) sprintf(name, "tty%c-%s", letter, property);
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-%s", number, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "tty0%c-%s", number, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "port-%c-%s", letter, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val,
				&len);
	}
	if (ret != DDI_PROP_SUCCESS)
		return (-1);		/* property non-existant */
	if (val[0] == 'f' || val[0] == 'F' || val[0] == '0')
		return (0);		/* property false/0 */
	return (1);			/* property true/!0 */
}

/* neuart_soft_state_free - local wrapper for ddi_soft_state_free(9F) */

static void
neuart_soft_state_free(struct neuart_com *neuart)
{
	/* If we were the max_neuart_instance, work out new value */
	if (neuart->neuart_unit == max_neuart_instance) {
		while (--max_neuart_instance >= 0) {
			if (ddi_get_soft_state(neuart_soft_state,
			    max_neuart_instance) != NULL)
				break;
		}
	}

	if (neuart->neuart_priv != NULL) {
		kmem_free(neuart->neuart_priv,
			sizeof (struct neuart_asyncline));
		neuart->neuart_priv = NULL;
	}
	ddi_soft_state_free(neuart_soft_state, neuart->neuart_unit);
}

static char *
neuart_hw_name(struct neuart_com *neuart)
{
	switch (neuart->neuart_hwtype) {
	case NEUART16C2552:
		return ("NS16C2552");
	case NEUART8250A:
		return ("8250A/16450");
	case NEUART16550:
		return ("16550");
	case NEUART16550A:
		return ("16550A");
	case NEUART16650:
		return ("16650");
	case NEUART16750:
		return ("16750");
	default:
		DEBUGNOTE2(NEUART_DEBUG_INIT,
		    "ne_uart%d: neuart_hw_name: unknown neuart_hwtype: %d",
		    neuart->neuart_unit, neuart->neuart_hwtype);
		return ("?");
	}
}

static int
neuart_identify_chip(dev_info_t *devi, struct neuart_com *neuart)
{
	int ret;
	int mcr;
	dev_t dev;
	uint_t hwtype;
	int lcr;
	int drev;
	int dvid;

	if (neuart_scr_test) {
		/* Check scratch register works. */

		/* write to scratch register */
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + SCR, SCRTEST);
		/* make sure that pattern doesn't just linger on the bus */
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + FIFOR, 0x00);
		/* read data back from scratch register */
		ret = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + SCR);
		if (ret != SCRTEST) {
			/*
			 * Scratch register not working.
			 * Probably not an async chip.
			 * 8250 and 8250B don't have scratch registers,
			 * but only worked in ancient PC XT's anyway.
			 */
			cmn_err(CE_CONT, "ne_uart%d: UART @ %p "
			    "scratch register: expected 0x5a, got 0x%02x\n",
			    neuart->neuart_unit,
			    (void *)neuart->neuart_ioaddr, ret);
			return (DDI_FAILURE);
		}
	}
	/*
	 * Use 16550 fifo reset sequence specified in NS application
	 * note. Disable fifos until chip is initialized.
	 */
	ddi_put8(neuart->neuart_iohandle,
	    neuart->neuart_ioaddr + FIFOR, 0x00);	/* clear */
	ddi_put8(neuart->neuart_iohandle,
	    neuart->neuart_ioaddr + FIFOR, FIFO_ON);	/* enable */
	ddi_put8(neuart->neuart_iohandle,
	    neuart->neuart_ioaddr + FIFOR, FIFO_ON | FIFORXFLSH);
						/* reset */
	if (neuart_maxchip >= NEUART16650 && neuart_scr_test) {
		/*
		 * Reset 16650 enhanced regs also, in case we have one of these
		 */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    EFRACCESS);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + EFR,
		    0);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    STOP1|BITS8);
	}

	/*
	 * See what sort of FIFO we have.
	 * Try enabling it and see what chip makes of this.
	 */

	/*
	 * Get "neuart-fiforeset-dlaboff" property from neuart.conf. 
	 */
	neuart_fiforeset_dlab = ddi_prop_get_int(DDI_DEV_T_ANY, devi, 0, 
				"neuart-fiforeset-dlaboff",
				NEUART_FIFORESET_DLABON);
	if (neuart_fiforeset_dlab != NEUART_FIFORESET_DLABOFF)
		neuart_fiforeset_dlab = NEUART_FIFORESET_DLABON;

	neuart->neuart_fifor = 0;
	/* just for neuart_reset_fifo() */
	neuart->neuart_hwtype = neuart_maxchip;
	if (neuart_maxchip >= NEUART16550A)
		neuart->neuart_fifor |=
		    FIFO_ON | FIFODMA | (neuart_trig_level & 0xff);
	if (neuart_maxchip >= NEUART16650)
		neuart->neuart_fifor |= FIFOEXTRA1 | FIFOEXTRA2;

	neuart_reset_fifo(neuart, FIFOTXFLSH | FIFORXFLSH);

	mcr = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR);
	ret = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ISR);
	DEBUGCONT4(NEUART_DEBUG_CHIP,
	    "ne_uart%d: probe fifo FIFOR=0x%02x ISR=0x%02x MCR=0x%02x\n",
	    neuart->neuart_unit, neuart->neuart_fifor | FIFOTXFLSH | FIFORXFLSH,
	    ret, mcr);
	switch (ret & 0xf0) {
	case 0x40:
		hwtype = NEUART16550; /* 16550 with broken FIFO */
		neuart->neuart_fifor = 0;
		break;
	case 0xc0:
		hwtype = NEUART16550A;
		neuart->neuart_fifo_buf = 16;
		neuart->neuart_use_fifo = FIFO_ON;
		neuart->neuart_fifor &= ~(FIFOEXTRA1 | FIFOEXTRA2);

		/*
		 * Identify for NS16C2552 chip.
		 */
		lcr = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR);
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR, DLAB);
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + DAT, 0x0);
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + ICR, 0x0);
		drev = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + DREV);
		dvid = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + DVID);
		if (((drev & 0xf0) == 0x30) || ((dvid & 0x0f) == 0x02)) {
			hwtype = NEUART16C2552;
			neuart->neuart_fifo_buf = 16;
			neuart->neuart_use_fifo = FIFO_ON;
			neuart->neuart_fifor &= ~(FIFOEXTRA1 | FIFOEXTRA2);
			ddi_put8(neuart->neuart_iohandle,
				 neuart->neuart_ioaddr + AFR, 0x0);
		}
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR, lcr);

		break;
	case 0xe0:
		hwtype = NEUART16650;
		neuart->neuart_fifo_buf = 32;
		neuart->neuart_use_fifo = FIFO_ON;
		neuart->neuart_fifor &= ~(FIFOEXTRA1);
		break;
	case 0xf0:
		/*
		 * Note we get 0xff if chip didn't return us anything,
		 * e.g. if there's no chip there.
		 */
		if (ret == 0xff) {
			cmn_err(CE_CONT, "ne_uart%d: UART @ %p "
			    "interrupt register: got 0xff\n",
			    neuart->neuart_unit, (void *)neuart->neuart_ioaddr);
			return (DDI_FAILURE);
		}
		/*FALLTHRU*/
	case 0xd0:
		hwtype = NEUART16750;
		neuart->neuart_fifo_buf = 64;
		neuart->neuart_use_fifo = FIFO_ON;
		break;
	default:
		hwtype = NEUART8250A; /* No FIFO */
		neuart->neuart_fifor = 0;
	}

	if (hwtype > neuart_maxchip) {
		cmn_err(CE_CONT, "ne_uart%d: UART @ %p "
		    "unexpected probe result: "
		    "FIFOR=0x%02x ISR=0x%02x MCR=0x%02x\n",
		    neuart->neuart_unit, (void *)neuart->neuart_ioaddr,
		    neuart->neuart_fifor | FIFOTXFLSH | FIFORXFLSH, ret, mcr);
		return (DDI_FAILURE);
	}

	/*
	 * Now reset the FIFO operation appropriate for the chip type.
	 * Note we must call neuart_reset_fifo() before any possible
	 * downgrade of the neuart->neuart_hwtype, or it may not disable
	 * the more advanced features we specifically want downgraded.
	 */
	neuart_reset_fifo(neuart, 0);
	neuart->neuart_hwtype = hwtype;

	/*
	 * Check for Exar/Startech ST16C650, which will still look like a
	 * 16550A until we enable its enhanced mode.
	 */
	if (neuart->neuart_hwtype == NEUART16550A &&
	    neuart_maxchip >= NEUART16650 &&
	    neuart_scr_test) {
		/* Enable enhanced mode register access */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    EFRACCESS);
		/* zero scratch register (not scratch register if enhanced) */
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + SCR, 0);
		/* Disable enhanced mode register access */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    STOP1|BITS8);
		/* read back scratch register */
		ret = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + SCR);
		if (ret == SCRTEST) {
			/* looks like we have an ST16650 -- enable it */
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR,
			    EFRACCESS);
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + EFR,
			    ENHENABLE);
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR,
			    STOP1|BITS8);
			neuart->neuart_hwtype = NEUART16650;
			neuart->neuart_fifo_buf = 32;
			/* 24 byte txfifo trigger */
			neuart->neuart_fifor |= 0x10;
			neuart_reset_fifo(neuart, 0);
		}
	}

	/*
	 * If we think we might have a FIFO larger than 16 characters,
	 * measure FIFO size and check it against expected.
	 */
	if (neuart_fifo_test > 0 &&
	    !(neuart->neuart_flags2 & NEUART2_NO_LOOPBACK) &&
	    (neuart->neuart_fifo_buf > 16 ||
	    (neuart_fifo_test > 1 && neuart->neuart_use_fifo == FIFO_ON) ||
	    NEUART_DEBUG(NEUART_DEBUG_CHIP))) {
		int i;

		/* Set baud rate to 57600 (fairly arbitrary choice) */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    DLAB);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + DAT,
		    neuart_spdtab[B57600] & 0xff);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR,
		    (neuart_spdtab[B57600] >> 8) & 0xff);
		/* Set 8 bits, 1 stop bit */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    STOP1|BITS8);
		/* Set loopback mode */
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR,
		    DTR | RTS | NEUART_LOOP | OUT1 | OUT2);

		/* Overfill fifo */
		for (i = 0; i < neuart->neuart_fifo_buf * 2; i++) {
			ddi_put8(neuart->neuart_iohandle,
			    neuart->neuart_ioaddr + DAT, i);
		}
		/*
		 * Now there's an interesting question here about which
		 * FIFO we're testing the size of, RX or TX. We just
		 * filled the TX FIFO much faster than it can empty,
		 * although it is possible one or two characters may
		 * have gone from it to the TX shift register.
		 * We wait for enough time for all the characters to
		 * move into the RX FIFO and any excess characters to
		 * have been lost, and then read all the RX FIFO. So
		 * the answer we finally get will be the size which is
		 * the MIN(RX FIFO,(TX FIFO + 1 or 2)). The critical
		 * one is actually the TX FIFO, because if we overfill
		 * it in normal operation, the excess characters are
		 * lost with no warning.
		 */
		/*
		 * Wait for characters to move into RX FIFO.
		 * In theory, 200 * neuart->neuart_fifo_buf * 2 should be
		 * enough. However, in practice it isn't always, so we
		 * increase to 400 so some slow 16550A's finish, and we
		 * increase to 3 so we spot more characters coming back
		 * than we sent, in case that should ever happen.
		 */
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		delay(drv_usectohz(400 * neuart->neuart_fifo_buf * 3));
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);

		/* Now see how many characters we can read back */
		for (i = 0; i < neuart->neuart_fifo_buf * 3; i++) {
			ret = ddi_get8(neuart->neuart_iohandle,
			    neuart->neuart_ioaddr + LSR);
			if (!(ret & RCA))
				break;	/* FIFO emptied */
			(void) ddi_get8(neuart->neuart_iohandle,
			    neuart->neuart_ioaddr + DAT); /* lose another */
		}

		DEBUGCONT3(NEUART_DEBUG_CHIP,
		    "ne_uart%d FIFO size: expected=%d, measured=%d\n",
		    neuart->neuart_unit, neuart->neuart_fifo_buf, i);

		hwtype = neuart->neuart_hwtype;
		if (i < neuart->neuart_fifo_buf) {
			/*
			 * FIFO is somewhat smaller than we anticipated.
			 * If we have 16 characters usable, then this
			 * UART will probably work well enough in
			 * 16550A mode. If less than 16 characters,
			 * then we'd better not use it at all.
			 * UARTs with busted FIFOs do crop up.
			 */
			if (i >= 16 && neuart->neuart_fifo_buf >= 16) {
				/* fall back to a 16550A */
				hwtype = NEUART16550A;
				neuart->neuart_fifo_buf = 16;
				neuart->neuart_fifor &=
						~(FIFOEXTRA1 | FIFOEXTRA2);
			} else {
				/* fall back to no FIFO at all */
				hwtype = NEUART16550;
				neuart->neuart_fifo_buf = 1;
				neuart->neuart_use_fifo = FIFO_OFF;
				neuart->neuart_fifor &=
				    ~(FIFO_ON | FIFOEXTRA1 | FIFOEXTRA2);
			}
		}
		/*
		 * We will need to reprogram the FIFO if we changed
		 * our mind about how to drive it above, and in any
		 * case, it would be a good idea to flush any garbage
		 * out incase the loopback test left anything behind.
		 * Again as earlier above, we must call neuart_reset_fifo()
		 * before any possible downgrade of neuart->neuart_hwtype.
		 */
		if (neuart->neuart_hwtype >= NEUART16650 &&
		    hwtype < NEUART16650) {
			/* Disable 16650 enhanced mode */
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR,
			    EFRACCESS);
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + EFR,
			    0);
			ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR,
			    STOP1|BITS8);
		}
		neuart_reset_fifo(neuart, FIFOTXFLSH | FIFORXFLSH);
		neuart->neuart_hwtype = hwtype;

		/* Clear loopback mode and restore DTR/RTS */
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + MCR, mcr);
	}

	DEBUGNOTE3(NEUART_DEBUG_CHIP, "ne_uart%d %s @ %p",
	    neuart->neuart_unit, neuart_hw_name(neuart),
	    (void *)neuart->neuart_ioaddr);

	/* Make UART type visible in device tree for prtconf, etc */
	dev = makedevice(DDI_MAJOR_T_UNKNOWN, neuart->neuart_unit);
	mutex_exit(&neuart->neuart_excl_hi);
	(void) ddi_prop_update_string(dev, devi, "uart",
					neuart_hw_name(neuart));
	mutex_enter(&neuart->neuart_excl_hi);

	if (neuart->neuart_hwtype == NEUART16550)    /* for broken 16550's, */
		neuart->neuart_hwtype = NEUART8250A; /* drive them as 8250A */

	return (DDI_SUCCESS);
}

/*
 * neuart_init() initializes the TTY protocol-private data for this channel
 * before enabling the interrupts.
 */
static void
neuart_init(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas;

	neuart->neuart_priv = kmem_zalloc(sizeof (struct neuart_asyncline),
								KM_SLEEP);
	neuartas = neuart->neuart_priv;
	mutex_enter(&neuart->neuart_excl);
	neuartas->async_common = neuart;
	cv_init(&neuartas->async_flags_cv, NULL, CV_DRIVER, NULL);
	mutex_exit(&neuart->neuart_excl);
}

/*ARGSUSED3*/
static int
neuart_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	struct neuart_com *neuart;
	struct neuart_asyncline *neuartas;
	int		mcr;
	int		unit;
	int 		len;
	struct termios 	*termiosp;

	unit = UNIT(*dev);
	DEBUGCONT1(NEUART_DEBUG_CLOSE, "ne_uart%dopen\n", unit);
	neuart = ddi_get_soft_state(neuart_soft_state, unit);
	if (neuart == NULL)
		return (ENXIO);		/* unit not configured */
	neuartas = neuart->neuart_priv;
	mutex_enter(&neuart->neuart_excl);

again:
	mutex_enter(&neuart->neuart_excl_hi);

	/*
	 * Block waiting for carrier to come up, unless this is a no-delay open.
	 */
	if (!(neuartas->async_flags & NEUARTAS_ISOPEN)) {
		/*
		 * Set the default termios settings (cflag).
		 * Others are set in ldterm.
		 */
		mutex_exit(&neuart->neuart_excl_hi);

		if (ddi_getlongprop(DDI_DEV_T_ANY, ddi_root_node(),
		    0, "ttymodes",
		    (caddr_t)&termiosp, &len) == DDI_PROP_SUCCESS &&
		    len == sizeof (struct termios)) {
			neuartas->async_ttycommon.t_cflag =
							termiosp->c_cflag;
			kmem_free(termiosp, len);
		} else
			cmn_err(CE_WARN,
				"neuart: couldn't get ttymodes property!");
		mutex_enter(&neuart->neuart_excl_hi);

		/* eeprom mode support - respect properties */
		if (neuart->neuart_cflag)
			neuartas->async_ttycommon.t_cflag =
							neuart->neuart_cflag;

		neuartas->async_ttycommon.t_iflag = 0;
		neuartas->async_ttycommon.t_iocpending = NULL;
		neuartas->async_ttycommon.t_size.ws_row = 0;
		neuartas->async_ttycommon.t_size.ws_col = 0;
		neuartas->async_ttycommon.t_size.ws_xpixel = 0;
		neuartas->async_ttycommon.t_size.ws_ypixel = 0;
		neuartas->async_dev = *dev;
		neuartas->async_wbufcid = 0;

		neuartas->async_startc = CSTART;
		neuartas->async_stopc = CSTOP;
		neuart_program(neuart, NEUART_INIT);
	} else if ((neuartas->async_ttycommon.t_flags & TS_XCLUDE)) {
		mutex_exit(&neuart->neuart_excl_hi);
		if (secpolicy_excl_open(cr) != 0) {
			mutex_exit(&neuart->neuart_excl);
			return (EBUSY);
		}
		mutex_enter(&neuart->neuart_excl_hi);
	} else if ((*dev & OUTLINE) &&
		   !(neuartas->async_flags & NEUARTAS_OUT)) {
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		return (EBUSY);
	}

	if (*dev & OUTLINE)
		neuartas->async_flags |= NEUARTAS_OUT;

	/* Raise DTR on every open, but delay if it was just lowered. */
	while (neuartas->async_flags & NEUARTAS_DTR_DELAY) {
		DEBUGCONT1(NEUART_DEBUG_MODEM,
		    "ne_uart%dopen: waiting for the NEUARTAS_DTR_DELAY "
		    "to be clear\n", unit);
		mutex_exit(&neuart->neuart_excl_hi);
		if (cv_wait_sig(&neuartas->async_flags_cv,
		    &neuart->neuart_excl) == 0) {
			DEBUGCONT1(NEUART_DEBUG_MODEM,
			    "ne_uart%dopen: interrupted by signal, exiting\n",
			    unit);
			mutex_exit(&neuart->neuart_excl);
			return (EINTR);
		}
		mutex_enter(&neuart->neuart_excl_hi);
	}

	mcr = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR);
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR,
		mcr|(neuart->neuart_mcr&DTR));

	DEBUGCONT3(NEUART_DEBUG_INIT,
		"ne_uart%dopen: \"Raise DTR on every open\": make mcr = %x, "
		"make TS_SOFTCAR = %s\n",
		unit, mcr|(neuart->neuart_mcr&DTR),
		(neuart->neuart_flags & NEUART_IGNORE_CD) ? "ON" : "OFF");
	if (neuart->neuart_flags & NEUART_IGNORE_CD) {
		DEBUGCONT1(NEUART_DEBUG_MODEM,
			"ne_uart%dopen: NEUART_IGNORE_CD set, set TS_SOFTCAR\n",
			unit);
		neuartas->async_ttycommon.t_flags |= TS_SOFTCAR;
	}
	else
		neuartas->async_ttycommon.t_flags &= ~TS_SOFTCAR;

	/*
	 * Check carrier.
	 */
	neuart->neuart_msr = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + MSR);
	DEBUGCONT3(NEUART_DEBUG_INIT, "ne_uart%dopen: TS_SOFTCAR is %s, "
		"MSR & DCD is %s\n",
		unit,
		(neuartas->async_ttycommon.t_flags & TS_SOFTCAR) ?
							"set" : "clear",
		(neuart->neuart_msr & DCD) ? "set" : "clear");
	if (neuart->neuart_msr & DCD)
		neuartas->async_flags |= NEUARTAS_CARR_ON;
	else
		neuartas->async_flags &= ~NEUARTAS_CARR_ON;
	mutex_exit(&neuart->neuart_excl_hi);

	/*
	 * If FNDELAY and FNONBLOCK are clear, block until carrier up.
	 * Quit on interrupt.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK)) &&
	    !(neuartas->async_ttycommon.t_cflag & CLOCAL)) {
		if ((!(neuartas->async_flags &
			(NEUARTAS_CARR_ON|NEUARTAS_OUT)) &&
		    !(neuartas->async_ttycommon.t_flags & TS_SOFTCAR)) ||
		    ((neuartas->async_flags & NEUARTAS_OUT) &&
		    !(*dev & OUTLINE))) {
			mutex_enter(&neuart->neuart_excl_hi);
			neuartas->async_flags |= NEUARTAS_WOPEN;
			mutex_exit(&neuart->neuart_excl_hi);
			if (cv_wait_sig(&neuartas->async_flags_cv,
			    &neuart->neuart_excl) == B_FALSE) {
				mutex_enter(&neuart->neuart_excl_hi);
				neuartas->async_flags &= ~NEUARTAS_WOPEN;
				mutex_exit(&neuart->neuart_excl_hi);
				mutex_exit(&neuart->neuart_excl);
				return (EINTR);
			}
			mutex_enter(&neuart->neuart_excl_hi);
			neuartas->async_flags &= ~NEUARTAS_WOPEN;
			mutex_exit(&neuart->neuart_excl_hi);
			goto again;
		}
	} else if ((neuartas->async_flags & NEUARTAS_OUT) &&
		   !(*dev & OUTLINE)) {
		mutex_exit(&neuart->neuart_excl);
		return (EBUSY);
	}

	neuartas->async_ttycommon.t_readq = rq;
	neuartas->async_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)neuartas;
	mutex_exit(&neuart->neuart_excl);
	/*
	 * Caution here -- qprocson sets the pointers that are used by canput
	 * called by neuart_softint. 
	 * NEUARTAS_ISOPEN must *not* be set until those pointers are valid.
	 */
	qprocson(rq);
	mutex_enter(&neuart->neuart_excl);
	mutex_enter(&neuart->neuart_excl_hi);
	neuartas->async_flags |= NEUARTAS_ISOPEN;
	neuartas->async_polltid = 0;
	mutex_exit(&neuart->neuart_excl_hi);
	mutex_exit(&neuart->neuart_excl);
	DEBUGCONT1(NEUART_DEBUG_INIT, "ne_uart%dopen: done\n", unit);
	return (0);
}

static void
neuart_progress_check(void *arg)
{
	struct neuart_asyncline *neuartas = arg;
	struct neuart_com *neuart = neuartas->async_common;
	mblk_t *bp;

	/*
	 * We define "progress" as either waiting on a timed break or delay, or
	 * having had at least one transmitter interrupt.  If none of these are
	 * true, then just terminate the output and wake up that close thread.
	 */
	mutex_enter(&neuart->neuart_excl);
	mutex_enter(&neuart->neuart_excl_hi);
	if (!(neuartas->async_flags &
	    (NEUARTAS_BREAK|NEUARTAS_DELAY|NEUARTAS_PROGRESS))) {
		neuartas->async_ocnt = 0;
		neuartas->async_flags &= ~NEUARTAS_BUSY;
		neuartas->async_timer = 0;
		bp = neuartas->async_xmitblk;
		neuartas->async_xmitblk = NULL;
		mutex_exit(&neuart->neuart_excl_hi);
		if (bp != NULL)
			freeb(bp);
		/*
		 * Since this timer is running, we know that we're in exit(2).
		 * That means that the user can't possibly be waiting on any
		 * valid ioctl(2) completion anymore, and we should just flush
		 * everything.
		 */
		flushq(neuartas->async_ttycommon.t_writeq, FLUSHALL);
		cv_broadcast(&neuartas->async_flags_cv);
	} else {
		neuartas->async_flags &= ~NEUARTAS_PROGRESS;
		mutex_exit(&neuart->neuart_excl_hi);
		neuartas->async_timer =
			timeout(neuart_progress_check,
		    		neuartas, drv_usectohz(neuart_drain_check));
	}
	mutex_exit(&neuart->neuart_excl);
}

/*
 * Release DTR so that neuart_open() can raise it.
 */
static void
neuart_dtr_free(struct neuart_asyncline *neuartas)
{
	struct neuart_com *neuart = neuartas->async_common;

	DEBUGCONT0(NEUART_DEBUG_MODEM,
	    "neuart_dtr_free, clearing NEUARTAS_DTR_DELAY\n");
	mutex_enter(&neuart->neuart_excl);
	neuartas->async_flags &= ~NEUARTAS_DTR_DELAY;
	neuartas->async_dtrtid = 0;
	cv_broadcast(&neuartas->async_flags_cv);
	mutex_exit(&neuart->neuart_excl);
}

/*
 * Close routine.
 */
/*ARGSUSED2*/
static int
neuart_close(queue_t *q, int flag, cred_t *credp)
{
	struct neuart_asyncline *neuartas;
	struct neuart_com *neuart;
	int icr, lcr;
	timeout_id_t tid;
	clock_t lbolt;
#ifdef DEBUG
	int instance;
#endif

	neuartas = (struct neuart_asyncline *)q->q_ptr;
	ASSERT(neuartas != NULL);
#ifdef DEBUG
	instance = UNIT(neuartas->async_dev);
	DEBUGCONT1(NEUART_DEBUG_CLOSE, "ne_uart%dclose\n", instance);
#endif
	neuart = neuartas->async_common;

	mutex_enter(&neuart->neuart_excl);
	neuartas->async_flags |= NEUARTAS_CLOSING;

	/*
	 * Turn off PPS handling early to avoid events occuring during
	 * close.  Also reset the DCD edge monitoring bit.
	 */
	mutex_enter(&neuart->neuart_excl_hi);
	neuart->neuart_flags &= ~(NEUART_PPS | NEUART_PPS_EDGE);
	mutex_exit(&neuart->neuart_excl_hi);

	/*
	 * There are two flavors of break -- timed (M_BREAK or TCSBRK) and
	 * untimed (TIOCSBRK).  For the timed case, these are enqueued on our
	 * write queue and there's a timer running, so we don't have to worry
	 * about them.  For the untimed case, though, the user obviously made a
	 * mistake, because these are handled immediately.  We'll terminate the
	 * break now and honor his implicit request by discarding the rest of
	 * the data.
	 */
	if (neuartas->async_flags & NEUARTAS_OUT_SUSPEND) {
		tid = neuartas->async_utbrktid;
		neuartas->async_utbrktid = 0;
		if (tid != 0) {
			mutex_exit(&neuart->neuart_excl);
			(void) untimeout(tid);
			mutex_enter(&neuart->neuart_excl);
		}
		mutex_enter(&neuart->neuart_excl_hi);
		lcr = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR);
		ddi_put8(neuart->neuart_iohandle,
		    neuart->neuart_ioaddr + LCR, (lcr & ~SETBREAK));
		neuartas->async_flags &= ~NEUARTAS_OUT_SUSPEND;
		mutex_exit(&neuart->neuart_excl_hi);
		goto nodrain;
	}

	/*
	 * If the user told us not to delay the close ("non-blocking"), then
	 * don't bother trying to drain.
	 *
	 * If the user did M_STOP (NEUARTAS_STOPPED), there's no hope of ever
	 * getting an M_START (since these messages aren't enqueued), and the
	 * only other way to clear the stop condition is by loss of DCD, which
	 * would discard the queue data.  Thus, we drop the output data if
	 * NEUARTAS_STOPPED is set.
	 */
	if ((flag & (FNDELAY|FNONBLOCK)) ||
	    (neuartas->async_flags & NEUARTAS_STOPPED)) {
		goto nodrain;
	}

	/*
	 * If there's any pending output, then we have to try to drain it.
	 * There are two main cases to be handled:
	 *	- called by close(2): need to drain until done or until
	 *	  a signal is received.  No timeout.
	 *	- called by exit(2): need to drain while making progress
	 *	  or until a timeout occurs.  No signals.
	 *
	 * If we can't rely on receiving a signal to get us out of a hung
	 * session, then we have to use a timer.  In this case, we set a timer
	 * to check for progress in sending the output data -- all that we ask
	 * (at each interval) is that there's been some progress made.  Since
	 * the interrupt routine grabs buffers from the write queue, we can't
	 * trust changes in async_ocnt.  Instead, we use a progress flag.
	 *
	 * Note that loss of carrier will cause the output queue to be flushed,
	 * and we'll wake up again and finish normally.
	 */
	if (!ddi_can_receive_sig() && neuart_drain_check != 0) {
		mutex_enter(&neuart->neuart_excl_hi);
		neuartas->async_flags &= ~NEUARTAS_PROGRESS;
		mutex_exit(&neuart->neuart_excl_hi);
		neuartas->async_timer =
				timeout(neuart_progress_check, neuartas,
		    drv_usectohz(neuart_drain_check));
	}
	lbolt = ddi_get_lbolt() +
		(drv_usectohz(neuart_drain_check) / NEUART_DRAINCHK_DIV);

	mutex_enter(&neuart->neuart_excl_hi);
	while (neuartas->async_ocnt > 0 ||
	    neuartas->async_ttycommon.t_writeq->q_first != NULL ||
	    (neuartas->async_flags &
		(NEUARTAS_BUSY|NEUARTAS_BREAK|NEUARTAS_DELAY)) ||
	    (neuart->neuart_flags & NEUART_DOINGSOFT)) {
		mutex_exit(&neuart->neuart_excl_hi);
		if (cv_timedwait_sig(&neuartas->async_flags_cv,
		    &neuart->neuart_excl, lbolt) == 0) {
			mutex_enter(&neuart->neuart_excl_hi);
			break;
		}
		mutex_enter(&neuart->neuart_excl_hi);
		lbolt = ddi_get_lbolt() +
			(drv_usectohz(neuart_drain_check) /
					NEUART_DRAINCHK_DIV);
	}

	tid = neuartas->async_timer;
	neuartas->async_timer = 0;
	mutex_exit(&neuart->neuart_excl_hi);
	if (tid != 0) {
		mutex_exit(&neuart->neuart_excl);
		(void) untimeout(tid);
		mutex_enter(&neuart->neuart_excl);
	}

nodrain:
	mutex_enter(&neuart->neuart_excl_hi);
	neuartas->async_ocnt = 0;
	if (neuartas->async_xmitblk != NULL) {
		mutex_exit(&neuart->neuart_excl_hi);
		freeb(neuartas->async_xmitblk);
		mutex_enter(&neuart->neuart_excl_hi);
	}
	neuartas->async_xmitblk = NULL;

	/*
	 * If line has HUPCL set or is incompletely opened fix up the modem
	 * lines.
	 */
	DEBUGCONT1(NEUART_DEBUG_MODEM,
		"ne_uart%dclose: next check HUPCL flag\n", instance);
	if ((neuartas->async_ttycommon.t_cflag & HUPCL) ||
	    (neuartas->async_flags & NEUARTAS_WOPEN)) {
		DEBUGCONT3(NEUART_DEBUG_MODEM,
			"ne_uart%dclose: HUPCL flag = %x, "
			"NEUARTAS_WOPEN flag = %x\n",
			instance,
			neuartas->async_ttycommon.t_cflag & HUPCL,
			neuartas->async_ttycommon.t_cflag & NEUARTAS_WOPEN);
		neuartas->async_flags |= NEUARTAS_DTR_DELAY;

		/* turn off DTR, RTS but NOT interrupt to 386 */
		if (neuart->neuart_flags &
		    (NEUART_IGNORE_CD|NEUART_RTS_DTR_OFF)) {
			DEBUGCONT3(NEUART_DEBUG_MODEM,
				"ne_uart%dclose: NEUART_IGNORE_CD flag = %x, "
				"NEUART_RTS_DTR_OFF flag = %x\n",
				instance,
				neuart->neuart_flags & NEUART_IGNORE_CD,
				neuart->neuart_flags & NEUART_RTS_DTR_OFF);
			ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + MCR,
				neuart->neuart_mcr|OUT2);
		} else {
			DEBUGCONT1(NEUART_DEBUG_MODEM,
			    "ne_uart%dclose: Dropping DTR and RTS\n", instance);
			ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + MCR, OUT2);
		}
		mutex_exit(&neuart->neuart_excl_hi);
		neuartas->async_dtrtid =
		    timeout((void (*)())neuart_dtr_free,
		    (caddr_t)neuartas, drv_usectohz(neuart_min_dtr_low));
		mutex_enter(&neuart->neuart_excl_hi);
	}
	/*
	 * If nobody's using it now, turn off receiver interrupts.
	 */
	if ((neuartas->async_flags &
		(NEUARTAS_WOPEN|NEUARTAS_ISOPEN)) == 0) {
		icr = ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + ICR);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR,
			(icr & ~RIEN));
	}
	mutex_exit(&neuart->neuart_excl_hi);
out:
	ttycommon_close(&neuartas->async_ttycommon);

	/*
	 * Cancel outstanding "bufcall" request.
	 */
	if (neuartas->async_wbufcid != 0) {
		unbufcall(neuartas->async_wbufcid);
		neuartas->async_wbufcid = 0;
	}

	/* Note that qprocsoff can't be done until after interrupts are off */
	qprocsoff(q);
	q->q_ptr = WR(q)->q_ptr = NULL;
	neuartas->async_ttycommon.t_readq = NULL;
	neuartas->async_ttycommon.t_writeq = NULL;

	/*
	 * Clear out device state, except persistant device property flags.
	 */
	neuartas->async_flags &= (NEUARTAS_DTR_DELAY|NEUART_RTS_DTR_OFF);
	cv_broadcast(&neuartas->async_flags_cv);
	mutex_exit(&neuart->neuart_excl);

	DEBUGCONT1(NEUART_DEBUG_CLOSE, "ne_uart%dclose: done\n", instance);
	return (0);
}

static boolean_t
neuart_isbusy(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas;

	DEBUGCONT0(NEUART_DEBUG_EOT, "neuart_isbusy\n");
	neuartas = neuart->neuart_priv;
	ASSERT(mutex_owned(&neuart->neuart_excl));
	ASSERT(mutex_owned(&neuart->neuart_excl_hi));
	return ((neuartas->async_ocnt > 0) ||
		((ddi_get8(neuart->neuart_iohandle,
		    neuart->neuart_ioaddr + LSR) & (XSRE|XHRE)) == 0));
}

static void
neuart_waiteot(struct neuart_com *neuart)
{
	/*
	 * Wait for the current transmission block and the
	 * current fifo data to transmit. Once this is done
	 * we may go on.
	 */
	DEBUGCONT0(NEUART_DEBUG_EOT, "neuart_waiteot\n");
	ASSERT(mutex_owned(&neuart->neuart_excl));
	ASSERT(mutex_owned(&neuart->neuart_excl_hi));
	while (neuart_isbusy(neuart)) {
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		drv_usecwait(10000);		/* wait .01 */
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
	}
}

/* neuart_reset_fifo -- flush fifos and [re]program fifo control register */
static void
neuart_reset_fifo(struct neuart_com *neuart, uchar_t flush)
{
	uchar_t lcr;

	/* On a 16750, we have to set DLAB in order to set FIFOEXTRA. */

	if ((neuart->neuart_hwtype >= NEUART16750) &&
	    (neuart_fiforeset_dlab != NEUART_FIFORESET_DLABOFF)) {
		lcr = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    lcr | DLAB);
	}

	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + FIFOR,
	    neuart->neuart_fifor | flush);

	/* Clear DLAB */

	if ((neuart->neuart_hwtype >= NEUART16750) &&
	    (neuart_fiforeset_dlab != NEUART_FIFORESET_DLABOFF)) {
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR, lcr);
	}
}

/*
 * Program the NE_UART port. Most of the neuart operation is based
 * on the value of 'c_iflag' and 'c_cflag'.
 */

#define	BAUDINDEX(cflg)	(((cflg) & CBAUDEXT) ? \
			(((cflg) & CBAUD) + CBAUD + 1) : ((cflg) & CBAUD))

static void
neuart_program(struct neuart_com *neuart, int mode)
{
	struct neuart_asyncline *neuartas;
	int baudrate, c_flag;
	int icr, lcr;
	int flush_reg;
	int ocflags;
#ifdef DEBUG
	int instance;
#endif

	ASSERT(mutex_owned(&neuart->neuart_excl));
	ASSERT(mutex_owned(&neuart->neuart_excl_hi));

	neuartas = neuart->neuart_priv;
#ifdef DEBUG
	instance = UNIT(neuartas->async_dev);
	DEBUGCONT2(NEUART_DEBUG_PROCS,
		"ne_uart%d_program: mode = 0x%08X, enter\n", instance, mode);
#endif

	baudrate = BAUDINDEX(neuartas->async_ttycommon.t_cflag);

	neuartas->async_ttycommon.t_cflag &= ~(CIBAUD);

	if (baudrate > CBAUD) {
		neuartas->async_ttycommon.t_cflag |= CIBAUDEXT;
		neuartas->async_ttycommon.t_cflag |=
			(((baudrate - CBAUD - 1) << IBSHIFT) & CIBAUD);
	} else {
		neuartas->async_ttycommon.t_cflag &= ~CIBAUDEXT;
		neuartas->async_ttycommon.t_cflag |=
			((baudrate << IBSHIFT) & CIBAUD);
	}

	c_flag = neuartas->async_ttycommon.t_cflag &
		(CLOCAL|CREAD|CSTOPB|CSIZE|PARENB|PARODD|CBAUD|CBAUDEXT);

	/* disable interrupts */
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR, 0);

	ocflags = neuart->neuart_ocflag;

	/* flush/reset the status registers */
	(void) ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ISR);
	(void) ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LSR);
	neuart->neuart_msr = flush_reg = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + MSR);
	/*
	 * The device is programmed in the open sequence, if we
	 * have to hardware handshake, then this is a good time
	 * to check if the device can receive any data.
	 */

	if ((CRTSCTS & neuartas->async_ttycommon.t_cflag) &&
	    !(flush_reg & CTS)) {
		neuart_flowcontrol_hw_output(neuart, FLOW_STOP);
	} else {
		/*
		 * We can not use
		 * neuart_flowcontrol_hw_output(neuart, FLOW_START) here,
		 * because if CRTSCTS is clear, we need clear
		 * NEUARTAS_HW_OUT_FLW bit.
		 */
		neuartas->async_flags &= ~NEUARTAS_HW_OUT_FLW;
	}

	/*
	 * If IXON is not set, clear NEUARTAS_SW_OUT_FLW;
	 * If IXON is set, no matter what IXON flag is before this
	 * function call to neuart_program,
	 * we will use the old NEUARTAS_SW_OUT_FLW status.
	 * Because of handling IXON in the driver, we also should re-calculate
	 * the value of NEUARTAS_OUT_FLW_RESUME bit, but in fact,
	 * the TCSET* commands which call neuart_program
	 * are put into the write queue, so there is no output needed to
	 * be resumed at this point.
	 */
	if (!(IXON & neuartas->async_ttycommon.t_iflag))
		neuartas->async_flags &= ~NEUARTAS_SW_OUT_FLW;

	/* manually flush receive buffer or fifo (workaround for buggy fifos) */
	if (mode == NEUART_INIT)
		if (neuart->neuart_use_fifo == FIFO_ON) {
			for (flush_reg = neuart->neuart_fifo_buf;
			     flush_reg-- > 0; ) {
				(void) ddi_get8(neuart->neuart_iohandle,
						neuart->neuart_ioaddr + DAT);
			}
		} else {
			flush_reg = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + DAT);
		}

	if (ocflags != (c_flag & ~CLOCAL) || mode == NEUART_INIT) {
		/* Set line control */
		lcr = ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LCR);
		lcr &= ~(WLS0|WLS1|STB|PEN|EPS);

		if (c_flag & CSTOPB)
			lcr |= STB;	/* 2 stop bits */

		if (c_flag & PARENB)
			lcr |= PEN;

		if ((c_flag & PARODD) == 0)
			lcr |= EPS;

		switch (c_flag & CSIZE) {
		case CS5:
			lcr |= BITS5;
			break;
		case CS6:
			lcr |= BITS6;
			break;
		case CS7:
			lcr |= BITS7;
			break;
		case CS8:
			lcr |= BITS8;
			break;
		}

		/* set the baud rate, unless it is "0" */
		ddi_put8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LCR, DLAB);
		if ((baudrate != 0) && (neuart_spdtab[baudrate] != 0)) {
			ddi_put8(neuart->neuart_iohandle,
				 neuart->neuart_ioaddr + DAT,
				 neuart_spdtab[baudrate] & 0xff);
			ddi_put8(neuart->neuart_iohandle,
				 neuart->neuart_ioaddr + ICR,
				 (neuart_spdtab[baudrate] >> 8) & 0xff);
		}
		/* set the line control modes */
		ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR, lcr);
		/*
		 * If we have a FIFO buffer, enable/flush
		 * at intialize time, flush if transitioning from
		 * CREAD off to CREAD on.
		 */
		if ((ocflags & CREAD) == 0 && (c_flag & CREAD) ||
		    mode == NEUART_INIT)
			if (neuart->neuart_use_fifo == FIFO_ON)
				neuart_reset_fifo(neuart, FIFORXFLSH);

		/* remember the new cflags */
		neuart->neuart_ocflag = c_flag & ~CLOCAL;
	}

	if (baudrate == 0)
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR,
			(neuart->neuart_mcr & RTS) | OUT2);
	else
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR,
			neuart->neuart_mcr | OUT2);

	/*
	 * Call the modem status interrupt handler to check for the carrier
	 * in case CLOCAL was turned off after the carrier came on.
	 * (Note: Modem status interrupt is not enabled if CLOCAL is ON.)
	 */
	neuart_msint(neuart);

	/* Set interrupt control */
	DEBUGCONT3(NEUART_DEBUG_MODM2,
		"ne_uart%d_program: c_flag & CLOCAL = %x "
		"t_cflag & CRTSCTS = %x\n",
		instance,
		c_flag & CLOCAL,
		neuartas->async_ttycommon.t_cflag & CRTSCTS);
	if ((c_flag & CLOCAL) &&
	    !(neuartas->async_ttycommon.t_cflag & CRTSCTS))
		/*
		 * direct-wired line ignores DCD, so we don't enable modem
		 * status interrupts.
		 */
		icr = (TIEN | SIEN);
	else
		icr = (TIEN | SIEN | MIEN);

	if (c_flag & CREAD)
		icr |= RIEN;

	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + ICR, icr);
	DEBUGCONT1(NEUART_DEBUG_PROCS, "ne_uart%d_program: done\n", instance);
}

static boolean_t
neuart_baudok(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int baudrate;


	baudrate = BAUDINDEX(neuartas->async_ttycommon.t_cflag);

	if (baudrate >= sizeof (neuart_spdtab)/sizeof (*neuart_spdtab))
		return (0);

	return (baudrate == 0 || neuart_spdtab[baudrate]);
}

/*
 * neuart_intr() is the High Level Interrupt Handler.
 *
 * There are four different interrupt types indexed by ISR register values:
 *		0: modem
 *		1: Tx holding register is empty, ready for next char
 *		2: Rx register now holds a char to be picked up
 *		3: error or break on line
 * This routine checks the Bit 0 (interrupt-not-pending) to determine if
 * the interrupt is from this port.
 */
uint_t
neuart_intr(caddr_t argneuart)
{
	struct neuart_com	*neuart = (struct neuart_com *)argneuart;
	struct neuart_asyncline	*neuartas;
	int			ret_status = DDI_INTR_UNCLAIMED;
	uchar_t			interrupt_id, lsr;

	neuartas = neuart->neuart_priv;
	if (neuartas == NULL) {
		    return (DDI_INTR_UNCLAIMED);
	}

	mutex_enter(&neuart->neuart_excl_hi);

	interrupt_id = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + ISR) & 0x0F;
	if (neuart_addedsoft == 0 ||
	    !(neuartas->async_flags & (NEUARTAS_ISOPEN|NEUARTAS_WOPEN))) {
		if (interrupt_id & NOINTERRUPT) {
			mutex_exit(&neuart->neuart_excl_hi);
			return (DDI_INTR_UNCLAIMED);
		} else {
			/*
			 * reset the device by:
			 *	reading line status
			 *	reading any data from data status register
			 *	reading modem status
			 */
			(void) ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LSR);
			(void) ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + DAT);
			neuart->neuart_msr = ddi_get8(neuart->neuart_iohandle,
						neuart->neuart_ioaddr + MSR);
			mutex_exit(&neuart->neuart_excl_hi);
			return (DDI_INTR_CLAIMED);
		}
	}

	/*
	 * We will loop until the interrupt line is pulled low. neuart
	 * interrupt is edge triggered.
	 */
	/* CSTYLED */
	for (;; interrupt_id = (ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + ISR) & 0x0F)) {
		if (interrupt_id & NOINTERRUPT)
			break;
		ret_status = DDI_INTR_CLAIMED;

		DEBUGCONT1(NEUART_DEBUG_INTR,
			"neuart_intr: interrupt_id = 0x%d\n", interrupt_id);
		lsr = ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LSR);
		switch (interrupt_id) {
		case RxRDY:
		case RSTATUS:
		case FFTMOUT:
			/* receiver interrupt or receiver errors */
			neuart_rxint(neuart, lsr);
			break;
		case TxRDY:

#ifndef	NS16C2552_NO_TXINTR_DEFECT
			/*
			 * Workaround for NS16C2552 on ether board
			 * to avoid the absence of interrupt
			 * when the Tx FIFO is empty.
			 */
			if ((neuart->neuart_hwtype == NEUART16C2552) &&
			    (!(lsr & XHRE))) {
				/* flush LSR */
				ddi_get8(neuart->neuart_iohandle,
						neuart->neuart_ioaddr + LSR);
			}
#endif	/* NS16C2552_NO_TXINTR_DEFECT */

			/* transmit interrupt */
			neuart_txint(neuart);
			continue;
		case MSTATUS:
			/* modem status interrupt */
			neuart_msint(neuart);
			break;
		}
		if ((lsr & XHRE) &&
		    (neuartas->async_flags & NEUARTAS_BUSY) &&
		    (neuartas->async_ocnt > 0))
			neuart_txint(neuart);
	}
	mutex_exit(&neuart->neuart_excl_hi);
	return (ret_status);
}

/*
 * Transmitter interrupt service routine.
 * If there is more data to transmit in the current pseudo-DMA block,
 * send the next character if output is not stopped or draining.
 * Otherwise, queue up a soft interrupt.
 *
 * XXX -  Needs review for HW FIFOs.
 */
static void
neuart_txint(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int		fifo_len;

	/*
	 * If NEUARTAS_BREAK or NEUARTAS_OUT_SUSPEND has been set, return to
	 * neuart_intr()'s context to claim the interrupt without performing
	 * any action. No character will be loaded into FIFO/THR until
	 * timed or untimed break is removed
	 */
	if (neuartas->async_flags & (NEUARTAS_BREAK|NEUARTAS_OUT_SUSPEND))
		return;

	fifo_len = neuart->neuart_fifo_buf; /* with FIFO buffers */
	if (fifo_len > neuart_max_tx_fifo)
		fifo_len = neuart_max_tx_fifo;

	if (neuart_flowcontrol_sw_input(neuart, FLOW_CHECK, IN_FLOW_NULL))
		fifo_len--;

	if (neuartas->async_ocnt > 0 && fifo_len > 0 &&
	    !(neuartas->async_flags &
	    (NEUARTAS_HW_OUT_FLW|NEUARTAS_SW_OUT_FLW|NEUARTAS_STOPPED))) {
		while (fifo_len-- > 0 && neuartas->async_ocnt-- > 0) {
			ddi_put8(neuart->neuart_iohandle,
			    	 neuart->neuart_ioaddr + DAT,
				 *neuartas->async_optr++);
		}
		neuartas->async_flags |= NEUARTAS_PROGRESS;
	}

	if (fifo_len <= 0)
		return;

	NEUARTSETSOFT(neuart);
}

/*
 * Interrupt on port: handle PPS event.  This function is only called
 * for a port on which PPS event handling has been enabled.
 */
static void
neuart_ppsevent(struct neuart_com *neuart, int msr)
{
	if (neuart->neuart_flags & NEUART_PPS_EDGE) {
		/* Have seen leading edge, now look for and record drop */
		if ((msr & DCD) == 0)
			neuart->neuart_flags &= ~NEUART_PPS_EDGE;
		/*
		 * Waiting for leading edge, look for rise; stamp event and
		 * calibrate kernel clock.
		 */
	} else if (msr & DCD) {
			/*
			 * This code captures a timestamp at the designated
			 * transition of the PPS signal (DCD asserted).  The
			 * code provides a pointer to the timestamp, as well
			 * as the hardware counter value at the capture.
			 *
			 * Note: the kernel has nano based time values while
			 * NTP requires micro based, an in-line fast algorithm
			 * to convert nsec to usec is used here -- see hrt2ts()
			 * in common/os/timers.c for a full description.
			 */
			struct timeval *tvp = &neuart_ppsev.tv;
			timestruc_t ts;
			long nsec, usec;

			neuart->neuart_flags |= NEUART_PPS_EDGE;
			LED_OFF;
			gethrestime(&ts);
			LED_ON;
			nsec = ts.tv_nsec;
			usec = nsec + (nsec >> 2);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 4);
			usec = nsec - (usec >> 3);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 3);
			usec = nsec + (usec >> 4);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 6);
			tvp->tv_usec = usec >> 10;
			tvp->tv_sec = ts.tv_sec;

			++neuart_ppsev.serial;

			/*
			 * Because the kernel keeps a high-resolution time,
			 * pass the current highres timestamp in tvp and zero
			 * in usec.
			 */
			ddi_hardpps(tvp, 0);
	}
}

/*
 * Receiver interrupt: RxRDY interrupt, FIFO timeout interrupt or receive
 * error interrupt.
 * Try to put the character into the circular buffer for this line; if it
 * overflows, indicate a circular buffer overrun. If this port is always
 * to be serviced immediately, or the character is a STOP character, or
 * more than 15 characters have arrived, queue up a soft interrupt to
 * drain the circular buffer.
 * XXX - needs review for hw FIFOs support.
 */

static void
neuart_rxint(struct neuart_com *neuart, uchar_t lsr)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	uchar_t c;
	uint_t s, needsoft = 0;
	tty_common_t *tp;
	int looplim = neuart->neuart_fifo_buf * 2;

	tp = &neuartas->async_ttycommon;
	if (!(tp->t_cflag & CREAD)) {
		while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
			(void) (ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + DAT) & 0xff);
			lsr = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LSR);
			if (looplim-- < 0)		/* limit loop */
				break;
		}
		return; /* line is not open for read? */
	}

	while (lsr & (RCA|PARERR|FRMERR|BRKDET|OVRRUN)) {
		c = 0;
		s = 0;				/* reset error status */
		if (lsr & RCA) {
			c = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + DAT) & 0xff;

			/*
			 * We handle XON/XOFF char if IXON is set,
			 * but if received char is _POSIX_VDISABLE,
			 * we left it to the up level module.
			 */
			if (tp->t_iflag & IXON) {
				if ((c == neuartas->async_stopc) &&
				    (c != _POSIX_VDISABLE)) {
					neuart_flowcontrol_sw_output(neuart,
					    FLOW_STOP);
					goto check_looplim;
				} else if ((c == neuartas->async_startc) &&
				    (c != _POSIX_VDISABLE)) {
					neuart_flowcontrol_sw_output(neuart,
					    FLOW_START);
					needsoft = 1;
					goto check_looplim;
				}
				if ((tp->t_iflag & IXANY) &&
				    (neuartas->async_flags &
						NEUARTAS_SW_OUT_FLW)) {
					neuart_flowcontrol_sw_output(neuart,
					    FLOW_START);
					needsoft = 1;
				}
			}
		}

		/*
		 * Check for character break sequence
		 */
		if ((abort_enable == KIOCABORTALTERNATE) &&
		    (neuart->neuart_flags & NEUART_CONSOLE)) {
			if (abort_charseq_recognize(c))
				abort_sequence_enter((char *)NULL);
		}

		/* Handle framing errors */
		if (lsr & (PARERR|FRMERR|BRKDET|OVRRUN)) {
			if (lsr & PARERR) {
				if (tp->t_iflag & INPCK) /* parity enabled */
					s |= PERROR;
			}

			if (lsr & (FRMERR|BRKDET))
				s |= FRERROR;
			if (lsr & OVRRUN) {
				neuartas->async_hw_overrun = 1;
				s |= OVERRUN;
			}
		}

		if (s == 0)
			if ((tp->t_iflag & PARMRK) &&
			    !(tp->t_iflag & (IGNPAR|ISTRIP)) &&
			    (c == 0377))
				if (RING_POK(neuartas, 2)) {
					RING_PUT(neuartas, 0377);
					RING_PUT(neuartas, c);
				} else
					neuartas->async_sw_overrun = 1;
			else
				if (RING_POK(neuartas, 1))
					RING_PUT(neuartas, c);
				else
					neuartas->async_sw_overrun = 1;
		else
			if (s & FRERROR) /* Handle framing errors */
				if (c == 0)
					if ((neuart->neuart_flags &
							NEUART_CONSOLE) &&
					    (abort_enable !=
					    KIOCABORTALTERNATE))
						abort_sequence_enter((char *)0);
					else
						neuartas->async_break++;
				else
					if (RING_POK(neuartas, 1))
					    RING_MARK(neuartas, c, s);
					else
					    neuartas->async_sw_overrun = 1;
			else /* Parity errors are handled by ldterm */
				if (RING_POK(neuartas, 1))
					RING_MARK(neuartas, c, s);
				else
					neuartas->async_sw_overrun = 1;
check_looplim:
		lsr = ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LSR);
		if (looplim-- < 0)		/* limit loop */
			break;
	}
	if ((RING_CNT(neuartas) > (RINGSIZE * 3)/4) &&
	    !(neuartas->async_inflow_source & IN_FLOW_RINGBUFF)) {
		neuart_flowcontrol_hw_input(neuart, FLOW_STOP,
					    IN_FLOW_RINGBUFF);
		(void) neuart_flowcontrol_sw_input(neuart, FLOW_STOP,
		    IN_FLOW_RINGBUFF);
	}

	if ((neuartas->async_flags & NEUARTAS_SERVICEIMM) || needsoft ||
	    (RING_FRAC(neuartas)) || (neuartas->async_polltid == 0))
		NEUARTSETSOFT(neuart);	/* need a soft interrupt */
}

/*
 * Modem status interrupt.
 *
 * (Note: It is assumed that the MSR hasn't been read by neuart_intr().)
 */

static void
neuart_msint(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int msr, t_cflag = neuartas->async_ttycommon.t_cflag;
#ifdef DEBUG
	int instance = UNIT(neuartas->async_dev);
#endif

neuart_msint_retry:
	/* this resets the interrupt */
	msr = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MSR);
	DEBUGCONT10(NEUART_DEBUG_STATE,
		"neuart%d_msint call #%d:\n"
		"   transition: %3s %3s %3s %3s\n"
		"current state: %3s %3s %3s %3s\n",
		instance,
		++(neuart->neuart_msint_cnt),
		(msr & DCTS) ? "DCTS" : "    ",
		(msr & DDSR) ? "DDSR" : "    ",
		(msr & DRI)  ? "DRI " : "    ",
		(msr & DDCD) ? "DDCD" : "    ",
		(msr & CTS)  ? "CTS " : "    ",
		(msr & DSR)  ? "DSR " : "    ",
		(msr & RI)   ? "RI  " : "    ",
		(msr & DCD)  ? "DCD " : "    ");

	/* If CTS status is changed, do H/W output flow control */
	if ((t_cflag & CRTSCTS) && (((neuart->neuart_msr ^ msr) & CTS) != 0))
		neuart_flowcontrol_hw_output(neuart,
		    msr & CTS ? FLOW_START : FLOW_STOP);
	/*
	 * Reading MSR resets the interrupt, we save the
	 * value of msr so that other functions could examine MSR by
	 * looking at neuart_msr.
	 */
	neuart->neuart_msr = (uchar_t)msr;

	/* Handle PPS event */
	if (neuart->neuart_flags & NEUART_PPS)
		neuart_ppsevent(neuart, msr);

	neuartas->async_ext++;
	NEUARTSETSOFT(neuart);
	/*
	 * We will make sure that the modem status presented to us
	 * during the previous read has not changed. If the chip samples
	 * the modem status on the falling edge of the interrupt line,
	 * and uses this state as the base for detecting change of modem
	 * status, we would miss a change of modem status event that occured
	 * after we initiated a read MSR operation.
	 */
	msr = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MSR);
	if (STATES(msr) != STATES(neuart->neuart_msr))
		goto	neuart_msint_retry;
}

/*
 * Handle a second-stage interrupt.
 */
/*ARGSUSED*/
uint_t
neuart_softintr(caddr_t intarg)
{
	struct neuart_com *neuart;
	int rv;
	int instance;

	/*
	 * Test and clear soft interrupt.
	 */
	mutex_enter(&neuart_soft_lock);
	DEBUGCONT0(NEUART_DEBUG_PROCS, "neuart_softintr: enter\n");
	rv = neuart_softpend;
	if (rv != 0)
		neuart_softpend = 0;
	mutex_exit(&neuart_soft_lock);

	if (rv) {
		/*
		 * Note - we can optimize the loop by remembering the last
		 * device that requested soft interrupt
		 */
		mutex_enter(&neuart_glob_lock);
		for (instance = 0;
		     instance <= max_neuart_instance;
		     instance++) {
			neuart = ddi_get_soft_state(neuart_soft_state,
								instance);
			if (neuart == NULL || neuart->neuart_priv == NULL) {
				continue;
			}
			mutex_enter(&neuart->neuart_excl);
			mutex_enter(&neuart->neuart_excl_hi);
			if (neuart->neuart_flags & NEUART_NEEDSOFT) {
				neuart->neuart_flags &= ~NEUART_NEEDSOFT;
				mutex_exit(&neuart->neuart_excl_hi);
				mutex_exit(&neuart->neuart_excl);
				neuart_softint(neuart);
			} else {
				mutex_exit(&neuart->neuart_excl_hi);
				mutex_exit(&neuart->neuart_excl);
			}
		}
		mutex_exit(&neuart_glob_lock);
	}
	return (rv ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}

/*
 * Handle a software interrupt.
 */
static void
neuart_softint(struct neuart_com *neuart)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	short	cc;
	mblk_t	*bp;
	queue_t	*q;
	uchar_t	val;
	uchar_t	c;
	tty_common_t	*tp;
	int nb;
	int instance = UNIT(neuartas->async_dev);

	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_softint\n", instance);
	mutex_enter(&neuart->neuart_excl);
	mutex_enter(&neuart->neuart_excl_hi);
	if (neuart->neuart_flags & NEUART_DOINGSOFT) {
		neuart->neuart_flags |= NEUART_DOINGSOFT_RETRY;
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		return;
	}
	neuart->neuart_flags |= NEUART_DOINGSOFT;

begin:
	neuart->neuart_flags &= ~NEUART_DOINGSOFT_RETRY;
	tp = &neuartas->async_ttycommon;
	q = tp->t_readq;
	if (neuartas->async_flags & NEUARTAS_OUT_FLW_RESUME) {
		if (neuartas->async_ocnt > 0) {
			neuart_resume(neuartas);
		} else {
			bp = neuartas->async_xmitblk;
			neuartas->async_xmitblk = NULL;
			mutex_exit(&neuart->neuart_excl_hi);
			if (bp != NULL)
				freeb(bp);
			neuart_start(neuartas);
			mutex_enter(&neuart->neuart_excl_hi);
		}
		neuartas->async_flags &= ~NEUARTAS_OUT_FLW_RESUME;
	}

	if (neuartas->async_ext) {
		neuartas->async_ext = 0;
		/* check for carrier up */
		DEBUGCONT3(NEUART_DEBUG_MODM2,
			"neuart%d_softint: neuart_msr & DCD = %x, "
			"tp->t_flags & TS_SOFTCAR = %x\n",
			instance,
			neuart->neuart_msr & DCD,
			tp->t_flags & TS_SOFTCAR);
		if (neuart->neuart_msr & DCD) {
			/* carrier present */
			if ((neuartas->async_flags &
					NEUARTAS_CARR_ON) == 0) {
				DEBUGCONT1(NEUART_DEBUG_MODM2,
					"neuart%d_softint: set "
					"NEUARTAS_CARR_ON\n",
					instance);
				neuartas->async_flags |= NEUARTAS_CARR_ON;
				if (neuartas->async_flags &
						NEUARTAS_ISOPEN) {
					mutex_exit(&neuart->neuart_excl_hi);
					mutex_exit(&neuart->neuart_excl);
					(void) putctl(q, M_UNHANGUP);
					mutex_enter(&neuart->neuart_excl);
					mutex_enter(&neuart->neuart_excl_hi);
				}
				cv_broadcast(&neuartas->async_flags_cv);
			}
		} else {
			if ((neuartas->async_flags & NEUARTAS_CARR_ON) &&
			    !(tp->t_cflag & CLOCAL) &&
			    !(tp->t_flags & TS_SOFTCAR)) {
				int flushflag;

				DEBUGCONT1(NEUART_DEBUG_MODEM,
					"neuart%d_softint: carrier dropped, "
					"so drop DTR\n",
					instance);
				/*
				 * Carrier went away.
				 * Drop DTR, abort any output in
				 * progress, indicate that output is
				 * not stopped, and send a hangup
				 * notification upstream.
				 */
				val = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + MCR);
				ddi_put8(neuart->neuart_iohandle,
				    neuart->neuart_ioaddr + MCR, (val & ~DTR));
				if (neuartas->async_flags & NEUARTAS_BUSY) {
				    DEBUGCONT0(NEUART_DEBUG_BUSY,
					    "neuart_softint: "
					    "Carrier dropped.  "
					    "Clearing async_ocnt\n");
				    neuartas->async_ocnt = 0;
				}	/* if */

				neuartas->async_flags &= ~NEUARTAS_STOPPED;
				if (neuartas->async_flags &
						NEUARTAS_ISOPEN) {
				    mutex_exit(&neuart->neuart_excl_hi);
				    mutex_exit(&neuart->neuart_excl);
				    (void) putctl(q, M_HANGUP);
				    mutex_enter(&neuart->neuart_excl);
				DEBUGCONT1(NEUART_DEBUG_MODEM,
					"neuart%d_softint: "
					"putctl(q, M_HANGUP)\n",
					instance);
				/*
				 * Flush FIFO buffers
				 * Any data left in there is invalid now
				 */
				if (neuart->neuart_use_fifo == FIFO_ON)
					neuart_reset_fifo(neuart, FIFOTXFLSH);
				/*
				 * Flush our write queue if we have one.
				 *
				 * If we're in the midst of close, then flush
				 * everything.  Don't leave stale ioctls lying
				 * about.
				 */
				flushflag = (neuartas->async_flags &
				    NEUARTAS_CLOSING) ? FLUSHALL : FLUSHDATA;
				flushq(tp->t_writeq, flushflag);

				mutex_enter(&neuart->neuart_excl_hi);
				/* active msg */
				bp = neuartas->async_xmitblk;
				neuartas->async_xmitblk = NULL;
				if (bp != NULL) {
					mutex_exit(&neuart->neuart_excl_hi);
					freeb(bp);
					mutex_enter(&neuart->neuart_excl_hi);
				}

				neuartas->async_flags &= ~NEUARTAS_BUSY;
				/*
				 * This message warns of Carrier loss
				 * with data left to transmit can hang the
				 * system.
				 */
				DEBUGCONT0(NEUART_DEBUG_MODEM,
					"neuart_softint: Flushing to "
					"prevent HUPCL hanging\n");
				}	/* if (NEUARTAS_ISOPEN) */
			}	/* if (NEUARTAS_CARR_ON && CLOCAL) */
			neuartas->async_flags &= ~NEUARTAS_CARR_ON;
			cv_broadcast(&neuartas->async_flags_cv);
		}	/* else */
	}	/* if (neuartas->async_ext) */

	mutex_exit(&neuart->neuart_excl_hi);

	/*
	 * If data has been added to the circular buffer, remove
	 * it from the buffer, and send it up the stream if there's
	 * somebody listening. Try to do it 16 bytes at a time. If we
	 * have more than 16 bytes to move, move 16 byte chunks and
	 * leave the rest for next time around (maybe it will grow).
	 */
	mutex_enter(&neuart->neuart_excl_hi);
	if (!(neuartas->async_flags & NEUARTAS_ISOPEN)) {
		RING_INIT(neuartas);
		goto rv;
	}
	if ((cc = RING_CNT(neuartas)) <= 0)
		goto rv;
	mutex_exit(&neuart->neuart_excl_hi);

	if (!canput(q)) {
		mutex_enter(&neuart->neuart_excl_hi);
		if (!(neuartas->async_inflow_source & IN_FLOW_STREAMS)) {
			neuart_flowcontrol_hw_input(neuart, FLOW_STOP,
			    IN_FLOW_STREAMS);
			(void) neuart_flowcontrol_sw_input(neuart, FLOW_STOP,
			    IN_FLOW_STREAMS);
		}
		goto rv;
	}
	if (neuartas->async_inflow_source & IN_FLOW_STREAMS) {
		mutex_enter(&neuart->neuart_excl_hi);
		neuart_flowcontrol_hw_input(neuart, FLOW_START,
		    IN_FLOW_STREAMS);
		(void) neuart_flowcontrol_sw_input(neuart, FLOW_START,
		    IN_FLOW_STREAMS);
		mutex_exit(&neuart->neuart_excl_hi);
	}
	DEBUGCONT2(NEUART_DEBUG_INPUT,
		"neuart%d_softint: %d char(s) in queue.\n", instance, cc);
	if (!(bp = allocb(cc, BPRI_MED))) {
		mutex_exit(&neuart->neuart_excl);
		ttycommon_qfull(&neuartas->async_ttycommon, q);
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
		goto rv;
	}
	mutex_enter(&neuart->neuart_excl_hi);
	do {
		if (RING_ERR(neuartas, S_ERRORS)) {
			RING_UNMARK(neuartas);
			c = RING_GET(neuartas);
			break;
		} else
			*bp->b_wptr++ = RING_GET(neuartas);
	} while (--cc);
	mutex_exit(&neuart->neuart_excl_hi);
	mutex_exit(&neuart->neuart_excl);
	if (bp->b_wptr > bp->b_rptr) {
			if (!canput(q)) {
				neuart_error(CE_NOTE,
					"ne_uart%d: local queue full",
					instance);
				freemsg(bp);
			} else
				(void) putq(q, bp);
	} else
		freemsg(bp);
	/*
	 * If we have a parity error, then send
	 * up an M_BREAK with the "bad"
	 * character as an argument. Let ldterm
	 * figure out what to do with the error.
	 */
	if (cc) {
		(void) putctl1(q, M_BREAK, c);
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
		/* finish cc chars */
		NEUARTSETSOFT(neuartas->async_common);
	} else {
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
	}
rv:
	if ((RING_CNT(neuartas) < (RINGSIZE/4)) &&
	    (neuartas->async_inflow_source & IN_FLOW_RINGBUFF)) {
		neuart_flowcontrol_hw_input(neuart,
					    FLOW_START, IN_FLOW_RINGBUFF);
		(void) neuart_flowcontrol_sw_input(neuart, FLOW_START,
		    IN_FLOW_RINGBUFF);
	}

	/*
	 * If a transmission has finished, indicate that it's finished,
	 * and start that line up again.
	 */
	if (neuartas->async_break > 0) {
		nb = neuartas->async_break;
		neuartas->async_break = 0;
		if (neuartas->async_flags & NEUARTAS_ISOPEN) {
			mutex_exit(&neuart->neuart_excl_hi);
			mutex_exit(&neuart->neuart_excl);
			for (; nb > 0; nb--)
				(void) putctl(q, M_BREAK);
			mutex_enter(&neuart->neuart_excl);
			mutex_enter(&neuart->neuart_excl_hi);
		}
	}
	if (neuartas->async_ocnt <= 0 &&
	    (neuartas->async_flags & NEUARTAS_BUSY)) {
		DEBUGCONT2(NEUART_DEBUG_BUSY,
		    "neuart%d_softint: Clearing NEUARTAS_BUSY.  "
		    "async_ocnt=%d\n",
		    instance,
		    neuartas->async_ocnt);
		neuartas->async_flags &= ~NEUARTAS_BUSY;
		bp = neuartas->async_xmitblk;
		neuartas->async_xmitblk = NULL;
		mutex_exit(&neuart->neuart_excl_hi);
		if (bp != NULL) {
			freeb(bp);
		}
		neuart_start(neuartas);
		/*
		 * If the flag isn't set after doing the neuart_start above, we
		 * may have finished all the queued output.  Signal any thread
		 * stuck in close.
		 */
		if (!(neuartas->async_flags & NEUARTAS_BUSY))
			cv_broadcast(&neuartas->async_flags_cv);
		mutex_enter(&neuart->neuart_excl_hi);
	}
	/*
	 * A note about these overrun bits: all they do is *tell* someone
	 * about an error- They do not track multiple errors. In fact,
	 * you could consider them latched register bits if you like.
	 * We are only interested in printing the error message once for
	 * any cluster of overrun errrors.
	 */
	if (neuartas->async_hw_overrun) {
		if (neuartas->async_flags & NEUARTAS_ISOPEN) {
			mutex_exit(&neuart->neuart_excl_hi);
			mutex_exit(&neuart->neuart_excl);
			neuart_error(CE_NOTE, "ne_uart%d: silo overflow",
				instance);
			mutex_enter(&neuart->neuart_excl);
			mutex_enter(&neuart->neuart_excl_hi);
		}
		neuartas->async_hw_overrun = 0;
	}
	if (neuartas->async_sw_overrun) {
		if (neuartas->async_flags & NEUARTAS_ISOPEN) {
			mutex_exit(&neuart->neuart_excl_hi);
			mutex_exit(&neuart->neuart_excl);
			neuart_error(CE_NOTE, "ne_uart%d: ring buffer overflow",
				instance);
			mutex_enter(&neuart->neuart_excl);
			mutex_enter(&neuart->neuart_excl_hi);
		}
		neuartas->async_sw_overrun = 0;
	}
	if (neuart->neuart_flags & NEUART_DOINGSOFT_RETRY) {
		goto begin;
	}
	neuart->neuart_flags &= ~NEUART_DOINGSOFT;
	if (neuartas->async_flags & NEUARTAS_CLOSING)
		cv_broadcast(&neuartas->async_flags_cv);

	mutex_exit(&neuart->neuart_excl_hi);
	mutex_exit(&neuart->neuart_excl);
	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_softint: done\n", instance);
}

/*
 * Restart output on a line after a delay or break timer expired.
 */
static void
neuart_restart(void *arg)
{
	struct neuart_asyncline *neuartas = (struct neuart_asyncline *)arg;
	struct neuart_com *neuart = neuartas->async_common;
	uchar_t lcr;
	mblk_t *bp;

	/*
	 * If break timer expired, turn off the break bit.
	 */
#ifdef DEBUG
	int instance = UNIT(neuartas->async_dev);

	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_restart\n", instance);
#endif
	mutex_enter(&neuart->neuart_excl);
	/*
	 * If NEUARTAS_OUT_SUSPEND is also set, we don't really
	 * clean the HW break, TIOCCBRK is responsible for this.
	 */
	if ((neuartas->async_flags & NEUARTAS_BREAK) &&
	    !(neuartas->async_flags & NEUARTAS_OUT_SUSPEND)) {
		mutex_enter(&neuart->neuart_excl_hi);
		lcr = ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LCR);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
			(lcr & ~SETBREAK));
		mutex_exit(&neuart->neuart_excl_hi);
	}
	neuartas->async_flags &= ~(NEUARTAS_DELAY|NEUARTAS_BREAK);
	cv_broadcast(&neuartas->async_flags_cv);

	mutex_enter(&neuart->neuart_excl_hi);
	if (neuartas->async_ocnt > 0) {
		neuart_resume(neuartas);
		mutex_exit(&neuart->neuart_excl_hi);
	} else {
		if (neuartas->async_flags & NEUARTAS_BUSY) {
			mutex_exit(&neuart->neuart_excl_hi);
			mutex_exit(&neuart->neuart_excl);
			return;
		}
		bp = neuartas->async_xmitblk;
		neuartas->async_xmitblk = NULL;
		mutex_exit(&neuart->neuart_excl_hi);
		if (bp != NULL) {
			freeb(bp);
		}
		neuart_start(neuartas);
	}

	mutex_exit(&neuart->neuart_excl);
}

static void
neuart_start(struct neuart_asyncline *neuartas)
{
	neuart_nstart(neuartas, 0);
}

/*
 * Start output on a line, unless it's busy, frozen, or otherwise.
 */
/*ARGSUSED*/
static void
neuart_nstart(struct neuart_asyncline *neuartas, int mode)
{
	struct neuart_com *neuart = neuartas->async_common;
	int cc;
	queue_t *q;
	mblk_t *bp;
	mblk_t *xmitbp;
	uchar_t *xmit_addr;
	uchar_t	val;
	int	fifo_len = 1;
	boolean_t didsome;
	mblk_t *nbp;

#ifdef DEBUG
	int instance = UNIT(neuartas->async_dev);

	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_nstart\n", instance);
#endif
	if (neuart->neuart_use_fifo == FIFO_ON) {
		fifo_len = neuart->neuart_fifo_buf; /* with FIFO buffers */
		if (fifo_len > neuart_max_tx_fifo)
			fifo_len = neuart_max_tx_fifo;
	}

	ASSERT(mutex_owned(&neuart->neuart_excl));

	/*
	 * If the chip is busy (i.e., we're waiting for a break timeout
	 * to expire, or for the current transmission to finish, or for
	 * output to finish draining from chip), don't grab anything new.
	 */
	if (neuartas->async_flags & (NEUARTAS_BREAK|NEUARTAS_BUSY)) {
		DEBUGCONT2((mode? NEUART_DEBUG_OUT : 0),
			"neuart%d_nstart: start %s.\n",
			instance,
			neuartas->async_flags & NEUARTAS_BREAK ?
			"break" : "busy");
		return;
	}

	/*
	 * Check only pended sw input flow control.
	 */
	mutex_enter(&neuart->neuart_excl_hi);
	if (neuart_flowcontrol_sw_input(neuart, FLOW_CHECK, IN_FLOW_NULL))
		fifo_len--;
	mutex_exit(&neuart->neuart_excl_hi);

	/*
	 * If we're waiting for a delay timeout to expire, don't grab
	 * anything new.
	 */
	if (neuartas->async_flags & NEUARTAS_DELAY) {
		DEBUGCONT1((mode? NEUART_DEBUG_OUT : 0),
			"neuart%d_nstart: start NEUARTAS_DELAY.\n", instance);
		return;
	}

	if ((q = neuartas->async_ttycommon.t_writeq) == NULL) {
		DEBUGCONT1((mode? NEUART_DEBUG_OUT : 0),
			"neuart%d_nstart: start writeq is null.\n", instance);
		return;	/* not attached to a stream */
	}

	for (;;) {
		DEBUGCONT1(NEUART_DEBUG_BUSY,
			"neuart%d_nstart: Set NEUARTAS_BUSY.\n",
			instance);
		mutex_enter(&neuart->neuart_excl_hi);
		neuartas->async_flags |= NEUARTAS_BUSY;
		mutex_exit(&neuart->neuart_excl_hi);

		if ((bp = getq(q)) == NULL) {
			DEBUGCONT1(NEUART_DEBUG_BUSY,
				"neuart%d_nstart: Clearing NEUARTAS_BUSY.\n",
				instance);
			mutex_enter(&neuart->neuart_excl_hi);
			neuartas->async_flags &= ~NEUARTAS_BUSY;
			mutex_exit(&neuart->neuart_excl_hi);
			return;	/* no data to transmit */
		}

		/*
		 * We have a message block to work on.
		 * Check whether it's a break, a delay, or an ioctl (the latter
		 * occurs if the ioctl in question was waiting for the output
		 * to drain).  If it's one of those, process it immediately.
		 */
		switch (bp->b_datap->db_type) {

		case M_BREAK:
			/*
			 * Set the break bit, and arrange for "neuart_restart"
			 * to be called in 1/4 second; it will turn the
			 * break bit off, and call "neuart_start" to grab
			 * the next message.
			 */
			mutex_enter(&neuart->neuart_excl_hi);
			val = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR);
			ddi_put8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR, (val | SETBREAK));
			DEBUGCONT1(NEUART_DEBUG_BUSY,
				"neuart%d_nstart: Clearing NEUARTAS_BUSY.\n",
				instance);
			neuartas->async_flags &= ~NEUARTAS_BUSY;
			neuartas->async_flags |= NEUARTAS_BREAK;
			mutex_exit(&neuart->neuart_excl_hi);
			(void) timeout(neuart_restart, (caddr_t)neuartas,
			    drv_usectohz(1000000)/4);
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_DELAY:
			/*
			 * Arrange for "neuart_restart" to be called when the
			 * delay expires; it will turn NEUARTAS_DELAY off,
			 * and call "neuart_start" to grab the next message.
			 */
			(void) timeout(neuart_restart, (caddr_t)neuartas,
			    (int)(*(unsigned char *)bp->b_rptr + 6));
			neuartas->async_flags |= NEUARTAS_DELAY;
			freemsg(bp);
			DEBUGCONT1(NEUART_DEBUG_BUSY,
				"neuart%d_nstart: Clearing NEUARTAS_BUSY.\n",
				instance);
			mutex_enter(&neuart->neuart_excl_hi);
			neuartas->async_flags &= ~NEUARTAS_BUSY;
			mutex_exit(&neuart->neuart_excl_hi);
			return;	/* wait for this to finish */

		case M_IOCTL:
			DEBUGCONT1(NEUART_DEBUG_BUSY,
				"neuart%d_nstart: Clearing NEUARTAS_BUSY.\n",
				instance);
			mutex_enter(&neuart->neuart_excl_hi);
			neuartas->async_flags &= ~NEUARTAS_BUSY;
			mutex_exit(&neuart->neuart_excl_hi);
			/*
			 * This ioctl was waiting for the output ahead of
			 * it to drain; obviously, it has.  Do it, and
			 * then grab the next message after it.
			 */
			mutex_exit(&neuart->neuart_excl);
			neuart_ioctl(neuartas, q, bp);
			mutex_enter(&neuart->neuart_excl);
			continue;
		}

		while (bp != NULL && (cc = bp->b_wptr - bp->b_rptr) == 0) {
			nbp = bp->b_cont;
			freeb(bp);
			bp = nbp;
		}
		if (bp != NULL)
			break;
	}

	/*
	 * We have data to transmit.  If output is stopped, put
	 * it back and try again later.
	 */
	if (neuartas->async_flags &
		(NEUARTAS_HW_OUT_FLW | NEUARTAS_SW_OUT_FLW |
	    NEUARTAS_STOPPED | NEUARTAS_OUT_SUSPEND)) {
		(void) putbq(q, bp);
		DEBUGCONT1(NEUART_DEBUG_BUSY,
			"neuart%d_nstart: Clearing NEUARTAS_BUSY.\n",
			instance);
		mutex_enter(&neuart->neuart_excl_hi);
		neuartas->async_flags &= ~NEUARTAS_BUSY;
		mutex_exit(&neuart->neuart_excl_hi);
		return;
	}

	xmitbp = bp;
	xmit_addr = bp->b_rptr;
	bp = bp->b_cont;
	if (bp != NULL)
		(void) putbq(q, bp);	/* not done with this message yet */

	/*
	 * In 5-bit mode, the high order bits are used
	 * to indicate character sizes less than five,
	 * so we need to explicitly mask before transmitting
	 */
	if ((neuartas->async_ttycommon.t_cflag & CSIZE) == CS5) {
		unsigned char *p = xmit_addr;
		int cnt = cc;

		while (cnt--)
			*p++ &= (unsigned char) 0x1f;
	}

	/*
	 * Set up this block for pseudo-DMA.
	 */
	mutex_enter(&neuart->neuart_excl_hi);
	/*
	 * If the transmitter is ready, shove the first
	 * character out.
	 */
	didsome = B_FALSE;
	neuartas->async_optr = xmit_addr;
	neuartas->async_xmitblk = xmitbp;
	neuartas->async_ocnt = cc;
	while (cc) {
		if ((neuartas->async_flags &
		    (NEUARTAS_OUT_SUSPEND|NEUARTAS_BREAK|NEUARTAS_STOPPED|
		    NEUARTAS_SW_OUT_FLW|NEUARTAS_HW_OUT_FLW)) ||
		    (neuartas->async_ocnt <= 0)) {
			break;
		}

		if ((ddi_get8(neuart->neuart_iohandle,
			neuart->neuart_ioaddr + LSR) & XHRE)) {
			ddi_put8(neuart->neuart_iohandle,
				 neuart->neuart_ioaddr + DAT,
		    		 *neuartas->async_optr++);
			neuartas->async_ocnt--;
			didsome = B_TRUE;
			break;
		}
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		delay(drv_usectohz(NEUART_XHREWAIT));
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
	}
	if (didsome)
		neuartas->async_flags |= NEUARTAS_PROGRESS;
	mutex_exit(&neuart->neuart_excl_hi);
}

/*
 * Resume output by poking the transmitter.
 */
static void
neuart_resume(struct neuart_asyncline *neuartas)
{
	struct neuart_com *neuart = neuartas->async_common;
#ifdef DEBUG
	int instance;
#endif

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));
#ifdef DEBUG
	instance = UNIT(neuartas->async_dev);
	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_resume\n", instance);
#endif

	if (ddi_get8(neuart->neuart_iohandle,
	    neuart->neuart_ioaddr + LSR) & XHRE) {
		if (neuart_flowcontrol_sw_input(neuart,
				FLOW_CHECK, IN_FLOW_NULL))
			return;
		if (neuartas->async_ocnt > 0 &&
		    !(neuartas->async_flags &
		    (NEUARTAS_HW_OUT_FLW|NEUARTAS_SW_OUT_FLW|
					NEUARTAS_OUT_SUSPEND))) {
			ddi_put8(neuart->neuart_iohandle,
			    	 neuart->neuart_ioaddr + DAT,
				 *neuartas->async_optr++);
			neuartas->async_ocnt--;
			neuartas->async_flags |= NEUARTAS_PROGRESS;
		}
	}
}

/*
 * Hold the untimed break to last the minimum time.
 */
static void
neuart_hold_utbrk(void *arg)
{
	struct neuart_asyncline *neuartas = arg;
	struct neuart_com *neuart = neuartas->async_common;

	mutex_enter(&neuart->neuart_excl);
	neuartas->async_flags &= ~NEUARTAS_HOLD_UTBRK;
	cv_broadcast(&neuartas->async_flags_cv);
	neuartas->async_utbrktid = 0;
	mutex_exit(&neuart->neuart_excl);
}

/*
 * Resume the untimed break.
 */
static void
neuart_resume_utbrk(struct neuart_asyncline *neuartas)
{
	uchar_t	val;
	mblk_t *bp;
	struct neuart_com *neuart = neuartas->async_common;
	ASSERT(mutex_owned(&neuart->neuart_excl));

	/*
	 * Because the wait time is very short,
	 * so we use uninterruptably wait.
	 */
	while (neuartas->async_flags & NEUARTAS_HOLD_UTBRK) {
		cv_wait(&neuartas->async_flags_cv, &neuart->neuart_excl);
	}
	mutex_enter(&neuart->neuart_excl_hi);
	/*
	 * Timed break and untimed break can exist simultaneously,
	 * if NEUARTAS_BREAK is also set at here, we don't
	 * really clean the HW break.
	 */
	if (!(neuartas->async_flags & NEUARTAS_BREAK)) {
		val = ddi_get8(neuart->neuart_iohandle,
				neuart->neuart_ioaddr + LCR);
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + LCR,
		    (val & ~SETBREAK));
	}
	neuartas->async_flags &= ~NEUARTAS_OUT_SUSPEND;
	cv_broadcast(&neuartas->async_flags_cv);
	if (neuartas->async_ocnt > 0) {
		neuartas->async_flags |= NEUARTAS_BUSY;
		neuart_resume(neuartas);
		mutex_exit(&neuart->neuart_excl_hi);
	} else {
		if (neuartas->async_flags & NEUARTAS_BUSY) {
			mutex_exit(&neuart->neuart_excl_hi);
			return;
		}
		bp = neuartas->async_xmitblk;
		neuartas->async_xmitblk = NULL;
		mutex_exit(&neuart->neuart_excl_hi);
		if (bp != NULL) {
			freeb(bp);
		}
		neuart_start(neuartas);
	}
}

/*
 * Process an "ioctl" message sent down to us.
 * Note that we don't need to get any locks until we are ready to access
 * the hardware.  Nothing we access until then is going to be altered
 * outside of the STREAMS framework, so we should be safe.
 */
int neuart_delay = 10000;
static void
neuart_ioctl(struct neuart_asyncline *neuartas, queue_t *wq, mblk_t *mp)
{
	struct neuart_com *neuart = neuartas->async_common;
	tty_common_t  *tp = &neuartas->async_ttycommon;
	struct iocblk *iocp;
	unsigned datasize;
	int error = 0;
	uchar_t val;
	mblk_t *datamp;
	unsigned int index;

#ifdef DEBUG
	int instance = UNIT(neuartas->async_dev);

	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_ioctl\n", instance);
#endif

	if (tp->t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(neuartas->async_ttycommon.t_iocpending);
		neuartas->async_ttycommon.t_iocpending = NULL;
	}

	iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * For TIOCMGET and the PPS ioctls, do NOT call ttycommon_ioctl()
	 * because this function frees up the message block (mp->b_cont) that
	 * contains the user location where we pass back the results.
	 *
	 * Similarly, CONSOPENPOLLEDIO needs ioc_count, which ttycommon_ioctl
	 * zaps.  We know that ttycommon_ioctl doesn't know any CONS*
	 * ioctls, so keep the others safe too.
	 */
	DEBUGCONT2(NEUART_DEBUG_IOCTL, "neuart%d_ioctl: %s\n",
		instance,
		iocp->ioc_cmd == TIOCMGET ? "TIOCMGET" :
		iocp->ioc_cmd == TIOCMSET ? "TIOCMSET" :
		iocp->ioc_cmd == TIOCMBIS ? "TIOCMBIS" :
		iocp->ioc_cmd == TIOCMBIC ? "TIOCMBIC" :
					    "other");
	switch (iocp->ioc_cmd) {
	case TIOCMGET:
	case TIOCGPPS:
	case TIOCSPPS:
	case TIOCGPPSEV:
	case CONSOPENPOLLEDIO:
	case CONSCLOSEPOLLEDIO:
	case CONSSETABORTENABLE:
	case CONSGETABORTENABLE:
		error = -1; /* Do Nothing */
		break;
	default:

		/*
		 * The only way in which "ttycommon_ioctl" can fail is if the
		 * "ioctl" requires a response containing data to be returned
		 * to the user, and no mblk could be allocated for the data.
		 * No such "ioctl" alters our state.  Thus, we always go ahead
		 * and do any state-changes the "ioctl" calls for.  If we
		 * couldn't allocate the data, "ttycommon_ioctl" has stashed
		 * the "ioctl" away safely, so we just call "bufcall" to
		 * request that we be called back when we stand a better
		 * chance of allocating the data.
		 */
		if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0) {
			if (neuartas->async_wbufcid)
				unbufcall(neuartas->async_wbufcid);
			neuartas->async_wbufcid =
			    bufcall(datasize, BPRI_HI,
			    (void (*)(void *)) neuart_reioctl,
			    (void *)
			      (intptr_t)neuartas->async_common->neuart_unit);
			return;
		}
	}

	mutex_enter(&neuart->neuart_excl);

	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETS:
			mutex_enter(&neuart->neuart_excl_hi);
			if (neuart_baudok(neuart))
				neuart_program(neuart, NEUART_NOINIT);
			else
				error = EINVAL;
			mutex_exit(&neuart->neuart_excl_hi);
			break;
		case TCSETSF:
		case TCSETSW:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			mutex_enter(&neuart->neuart_excl_hi);
			if (!neuart_baudok(neuart))
				error = EINVAL;
			else {
				if (neuart_isbusy(neuart))
					neuart_waiteot(neuart);
				neuart_program(neuart, NEUART_NOINIT);
			}
			mutex_exit(&neuart->neuart_excl_hi);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		error = 0;
		switch (iocp->ioc_cmd) {

		case TIOCGPPS:
			/*
			 * Get PPS on/off.
			 */
			if (mp->b_cont != NULL)
				freemsg(mp->b_cont);

			mp->b_cont = allocb(sizeof (int), BPRI_HI);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				break;
			}
			mutex_enter(&neuart->neuart_excl_hi);
			if (neuart->neuart_flags & NEUART_PPS)
				*(int *)mp->b_cont->b_wptr = 1;
			else
				*(int *)mp->b_cont->b_wptr = 0;
			mutex_exit(&neuart->neuart_excl_hi);
			mp->b_cont->b_wptr += sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = sizeof (int);
			break;

		case TIOCSPPS:
			/*
			 * Set PPS on/off.
			 */
			error = miocpullup(mp, sizeof (int));
			if (error != 0)
				break;

			mutex_enter(&neuart->neuart_excl_hi);
			if (*(int *)mp->b_cont->b_rptr)
				neuart->neuart_flags |= NEUART_PPS;
			else
				neuart->neuart_flags &= ~NEUART_PPS;
			/* Reset edge sense */
			neuart->neuart_flags &= ~NEUART_PPS_EDGE;
			mutex_exit(&neuart->neuart_excl_hi);
			mp->b_datap->db_type = M_IOCACK;
			break;

		case TIOCGPPSEV:
		{
			/*
			 * Get PPS event data.
			 */
			mblk_t *bp;
			void *buf;
#ifdef _SYSCALL32_IMPL
			struct ppsclockev32 p32;
#endif
			struct ppsclockev ppsclockev;

			if (mp->b_cont != NULL) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}

			mutex_enter(&neuart->neuart_excl_hi);
			if ((neuart->neuart_flags & NEUART_PPS) == 0) {
				error = ENXIO;
				mutex_exit(&neuart->neuart_excl_hi);
				break;
			}

			/* Protect from incomplete neuart_ppsev */
			ppsclockev = neuart_ppsev;
			mutex_exit(&neuart->neuart_excl_hi);

#ifdef _SYSCALL32_IMPL
			if ((iocp->ioc_flag & IOC_MODELS) != IOC_NATIVE) {
				TIMEVAL_TO_TIMEVAL32(&p32.tv, &ppsclockev.tv);
				p32.serial = ppsclockev.serial;
				buf = &p32;
				iocp->ioc_count = sizeof (struct ppsclockev32);
			} else
#endif
			{
				buf = &ppsclockev;
				iocp->ioc_count = sizeof (struct ppsclockev);
			}

			if ((bp = allocb(iocp->ioc_count, BPRI_HI)) == NULL) {
				error = ENOMEM;
				break;
			}
			mp->b_cont = bp;

			bcopy(buf, bp->b_wptr, iocp->ioc_count);
			bp->b_wptr += iocp->ioc_count;
			mp->b_datap->db_type = M_IOCACK;
			break;
		}

		case TCSBRK:
			error = miocpullup(mp, sizeof (int));
			if (error != 0)
				break;

			if (*(int *)mp->b_cont->b_rptr == 0) {

				/*
				 * XXX Arrangements to ensure that a break
				 * isn't in progress should be sufficient.
				 * This ugly delay() is the only thing
				 * that seems to work on the NCR Worldmark.
				 * It should be replaced. Note that an
				 * neuart_waiteot() also does not work.
				 */
				if (neuart_delay) {
					mutex_exit(&neuart->neuart_excl);
					delay(drv_usectohz(neuart_delay));
					mutex_enter(&neuart->neuart_excl);
				}
				while (neuartas->async_flags &
							NEUARTAS_BREAK) {
					cv_wait(&neuartas->async_flags_cv,
					    &neuart->neuart_excl);
				}
				mutex_enter(&neuart->neuart_excl_hi);
				/*
				 * We loop until the TSR is empty and then
				 * set the break.  NEUARTAS_BREAK has been set
				 * to ensure that no characters are
				 * transmitted while the TSR is being
				 * flushed and SOUT is being used for the
				 * break signal.
				 *
				 * The wait period is equal to
				 * clock / (baud * 16) * 16 * 2.
				 */
				index = BAUDINDEX(
					neuartas->async_ttycommon.t_cflag);
				neuartas->async_flags |= NEUARTAS_BREAK;
				while ((ddi_get8(neuart->neuart_iohandle,
				    neuart->neuart_ioaddr + LSR) & XSRE) == 0) {
					mutex_exit(&neuart->neuart_excl_hi);
					mutex_exit(&neuart->neuart_excl);
					drv_usecwait(
					    32*neuart_spdtab[index] & 0xfff);
					mutex_enter(&neuart->neuart_excl);
					mutex_enter(&neuart->neuart_excl_hi);
				}
				/*
				 * Arrange for "neuart_restart"
				 * to be called in 1/4 second;
				 * it will turn the break bit off, and call
				 * "neuart_start" to grab the next message.
				 */
				val = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR);
				ddi_put8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + LCR,
					(val | SETBREAK));
				mutex_exit(&neuart->neuart_excl_hi);
				(void) timeout(neuart_restart,
				(caddr_t)neuartas, drv_usectohz(1000000)/4);
			} else {
				DEBUGCONT1(NEUART_DEBUG_OUT,
					"neuart%d_ioctl: wait for flush.\n",
					instance);
				mutex_enter(&neuart->neuart_excl_hi);
				neuart_waiteot(neuart);
				mutex_exit(&neuart->neuart_excl_hi);
				DEBUGCONT1(NEUART_DEBUG_OUT,
					"neuart%d_ioctl: ldterm satisfied.\n",
					instance);
			}
			break;

		case TIOCSBRK:
			if (!(neuartas->async_flags &
					NEUARTAS_OUT_SUSPEND)) {
				mutex_enter(&neuart->neuart_excl_hi);
				neuartas->async_flags |=
						NEUARTAS_OUT_SUSPEND;
				neuartas->async_flags |=
						NEUARTAS_HOLD_UTBRK;
				index = BAUDINDEX(
				    neuartas->async_ttycommon.t_cflag);
				while ((ddi_get8(neuart->neuart_iohandle,
				    neuart->neuart_ioaddr + LSR) & XSRE) == 0) {
					mutex_exit(&neuart->neuart_excl_hi);
					mutex_exit(&neuart->neuart_excl);
					drv_usecwait(
					    32*neuart_spdtab[index] & 0xfff);
					mutex_enter(&neuart->neuart_excl);
					mutex_enter(&neuart->neuart_excl_hi);
				}
				val = ddi_get8(neuart->neuart_iohandle,
				    neuart->neuart_ioaddr + LCR);
				ddi_put8(neuart->neuart_iohandle,
				    neuart->neuart_ioaddr + LCR,
				    (val | SETBREAK));
				mutex_exit(&neuart->neuart_excl_hi);
				/* wait for 100ms to hold BREAK */
				neuartas->async_utbrktid =
				    timeout((void (*)())neuart_hold_utbrk,
				    (caddr_t)neuartas,
				    drv_usectohz(neuart_min_utbrk));
			}
			mioc2ack(mp, NULL, 0, 0);
			break;

		case TIOCCBRK:
			if (neuartas->async_flags & NEUARTAS_OUT_SUSPEND) {
				/*
				 * If NEUARTAS_OUT_SUSPEND is set, output has
				 * stopped completely. However, when TIOCSBRK
				 * ioctl is issued in delay, the output may
				 * stop with NEUARTAS_BUSY set.
				 * So clear NEUARTAS_BUSY to start output.
				 */
				mutex_enter(&neuart->neuart_excl_hi);
				neuartas->async_flags &= ~NEUARTAS_BUSY;
				mutex_exit(&neuart->neuart_excl_hi);
				neuart_resume_utbrk(neuartas);
			}
			mioc2ack(mp, NULL, 0, 0);
			break;

		case TIOCMSET:
		case TIOCMBIS:
		case TIOCMBIC:
			if (iocp->ioc_count != TRANSPARENT) {
				DEBUGCONT1(NEUART_DEBUG_IOCTL,
					"neuart%d_ioctl: "
					"non-transparent\n", instance);

				error = miocpullup(mp, sizeof (int));
				if (error != 0)
					break;

				mutex_enter(&neuart->neuart_excl_hi);
				(void) neuart_mctl(neuart,
					dmto_neuart(*(int *)mp->b_cont->b_rptr),
					iocp->ioc_cmd);
				mutex_exit(&neuart->neuart_excl_hi);
				iocp->ioc_error = 0;
				mp->b_datap->db_type = M_IOCACK;
			} else {
				DEBUGCONT1(NEUART_DEBUG_IOCTL,
					"neuart%d_ioctl: "
					"transparent\n", instance);
				mcopyin(mp, NULL, sizeof (int), NULL);
			}
			break;

		case TIOCMGET:
			datamp = allocb(sizeof (int), BPRI_MED);
			if (datamp == NULL) {
				error = EAGAIN;
				break;
			}

			mutex_enter(&neuart->neuart_excl_hi);
			*(int *)datamp->b_rptr =
					neuart_mctl(neuart, 0, TIOCMGET);
			mutex_exit(&neuart->neuart_excl_hi);

			if (iocp->ioc_count == TRANSPARENT) {
				DEBUGCONT1(NEUART_DEBUG_IOCTL,
					"neuart%d_ioctl: "
					"transparent\n", instance);
				mcopyout(mp, NULL, sizeof (int), NULL,
					datamp);
			} else {
				DEBUGCONT1(NEUART_DEBUG_IOCTL,
					"neuart%d_ioctl: "
					"non-transparent\n", instance);
				mioc2ack(mp, datamp, sizeof (int), 0);
			}
			break;

		case CONSOPENPOLLEDIO:
			error = miocpullup(mp, sizeof (struct cons_polledio *));
			if (error != 0)
				break;

			*(struct cons_polledio **)mp->b_cont->b_rptr =
				&neuart->polledio;

			mp->b_datap->db_type = M_IOCACK;
			break;

		case CONSCLOSEPOLLEDIO:
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSSETABORTENABLE:
			error = secpolicy_console(iocp->ioc_cr);
			if (error != 0)
				break;

			if (iocp->ioc_count != TRANSPARENT) {
				error = EINVAL;
				break;
			}

			mutex_enter(&neuart->neuart_excl_hi);
			if (*(intptr_t *)mp->b_cont->b_rptr)
				neuart->neuart_flags |= NEUART_CONSOLE;
			else
				neuart->neuart_flags &= ~NEUART_CONSOLE;
			mutex_exit(&neuart->neuart_excl_hi);

			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSGETABORTENABLE:
			/*CONSTANTCONDITION*/
			ASSERT(sizeof (boolean_t) <= sizeof (boolean_t *));
			/*
			 * Store the return value right in the payload
			 * we were passed.  Crude.
			 */
			mcopyout(mp, NULL, sizeof (boolean_t), NULL, NULL);
			mutex_enter(&neuart->neuart_excl_hi);
			*(boolean_t *)mp->b_cont->b_rptr =
				(neuart->neuart_flags & NEUART_CONSOLE) != 0;
			mutex_exit(&neuart->neuart_excl_hi);
			break;

		default:
			/*
			 * If we don't understand it, it's an error.  NAK it.
			 */
			error = EINVAL;
			break;
		}
	}
	if (error != 0) {
		iocp->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK;
	}
	mutex_exit(&neuart->neuart_excl);
	qreply(wq, mp);
	DEBUGCONT1(NEUART_DEBUG_PROCS, "neuart%d_ioctl: done\n", instance);
}

static int
neuart_rsrv(queue_t *q)
{
	mblk_t *bp;
	struct neuart_asyncline *neuartas;

	neuartas = (struct neuart_asyncline *)q->q_ptr;

	while (canputnext(q) && (bp = getq(q)))
		putnext(q, bp);
	mutex_enter(&neuartas->async_common->neuart_excl);
	mutex_enter(&neuartas->async_common->neuart_excl_hi);
	NEUARTSETSOFT(neuartas->async_common);
	mutex_exit(&neuartas->async_common->neuart_excl_hi);
	neuartas->async_polltid = 0;
	mutex_exit(&neuartas->async_common->neuart_excl);
	return (0);
}

/*
 * Put procedure for write queue.
 * Respond to M_STOP, M_START, M_IOCTL, and M_FLUSH messages here;
 * set the flow control character for M_STOPI and M_STARTI messages;
 * queue up M_BREAK, M_DELAY, and M_DATA messages for processing
 * by the start routine, and then call the start routine; discard
 * everything else.  Note that this driver does not incorporate any
 * mechanism to negotiate to handle the canonicalization process.
 * It expects that these functions are handled in upper module(s),
 * as we do in ldterm.
 */
static int
neuart_wput(queue_t *q, mblk_t *mp)
{
	struct neuart_asyncline *neuartas;
	struct neuart_com *neuart;
#ifdef DEBUG
	int instance;
#endif
	int error;

	neuartas = (struct neuart_asyncline *)q->q_ptr;
#ifdef DEBUG
	instance = UNIT(neuartas->async_dev);
#endif
	neuart = neuartas->async_common;

	switch (mp->b_datap->db_type) {

	case M_STOP:
		/*
		 * Since we don't do real DMA, we can just let the
		 * chip coast to a stop after applying the brakes.
		 */
		mutex_enter(&neuart->neuart_excl);
		neuartas->async_flags |= NEUARTAS_STOPPED;
		mutex_exit(&neuart->neuart_excl);
		freemsg(mp);
		break;

	case M_START:
		mutex_enter(&neuart->neuart_excl);
		if (neuartas->async_flags & NEUARTAS_STOPPED) {
			neuartas->async_flags &= ~NEUARTAS_STOPPED;
			/*
			 * If an output operation is in progress,
			 * resume it.  Otherwise, prod the start
			 * routine.
			 */
			if (neuartas->async_ocnt > 0) {
				mutex_enter(&neuart->neuart_excl_hi);
				neuart_resume(neuartas);
				mutex_exit(&neuart->neuart_excl_hi);
			} else {
				neuart_start(neuartas);
			}
		}
		mutex_exit(&neuart->neuart_excl);
		freemsg(mp);
		break;

	case M_IOCTL:
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {

		case TCSBRK:
			error = miocpullup(mp, sizeof (int));
			if (error != 0) {
				miocnak(q, mp, 0, error);
				return (0);
			}

			if (*(int *)mp->b_cont->b_rptr != 0) {
				DEBUGCONT1(NEUART_DEBUG_OUT,
					"neuart%d_ioctl: flush request.\n",
					instance);
				(void) putq(q, mp);
				mutex_enter(&neuart->neuart_excl);

				/*
				 * If an TIOCSBRK is in progress,
				 * clean it as TIOCCBRK does,
				 * then kick off output.
				 * If TIOCSBRK is not in progress,
				 * just kick off output.
				 */
				neuart_resume_utbrk(neuartas);
				mutex_exit(&neuart->neuart_excl);
				break;
			}
			/*FALLTHROUGH*/
		case TCSETSW:
		case TCSETSF:
		case TCSETAW:
		case TCSETAF:
			/*
			 * The changes do not take effect until all
			 * output queued before them is drained.
			 * Put this message on the queue, so that
			 * "neuart_start" will see it when it's done
			 * with the output before it.  Poke the
			 * start routine, just in case.
			 */
			(void) putq(q, mp);
			mutex_enter(&neuart->neuart_excl);

			/*
			 * If an TIOCSBRK is in progress,
			 * clean it as TIOCCBRK does.
			 * then kick off output.
			 * If TIOCSBRK is not in progress,
			 * just kick off output.
			 */
			neuart_resume_utbrk(neuartas);
			mutex_exit(&neuart->neuart_excl);
			break;

		default:
			/*
			 * Do it now.
			 */
			neuart_ioctl(neuartas, q, mp);
			break;
		}
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) {
			mutex_enter(&neuart->neuart_excl);

			/*
			 * Abort any output in progress.
			 */
			mutex_enter(&neuart->neuart_excl_hi);
			if (neuartas->async_flags & NEUARTAS_BUSY) {
			    DEBUGCONT1(NEUART_DEBUG_BUSY, "ne_uart%dwput: "
				    "Clearing async_ocnt, "
				    "leaving NEUARTAS_BUSY set\n",
				    instance);
				neuartas->async_ocnt = 0;
				neuartas->async_flags &= ~NEUARTAS_BUSY;
			} /* if */
			mutex_exit(&neuart->neuart_excl_hi);

			/* Flush FIFO buffers */
			if (neuart->neuart_use_fifo == FIFO_ON) {
				neuart_reset_fifo(neuart, FIFOTXFLSH);
			}

			/*
			 * Flush our write queue.
			 */
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			if (neuartas->async_xmitblk != NULL) {
				freeb(neuartas->async_xmitblk);
				neuartas->async_xmitblk = NULL;
			}
			mutex_exit(&neuart->neuart_excl);
			*mp->b_rptr &= ~FLUSHW;	/* it has been flushed */
		}
		if (*mp->b_rptr & FLUSHR) {
			/* Flush FIFO buffers */
			if (neuart->neuart_use_fifo == FIFO_ON) {
				neuart_reset_fifo(neuart, FIFORXFLSH);
			}
			flushq(RD(q), FLUSHDATA);
			qreply(q, mp);	/* give the read queues a crack at it */
		} else {
			freemsg(mp);
		}

		/*
		 * We must make sure we process messages that survive the
		 * write-side flush.
		 */
		mutex_enter(&neuart->neuart_excl);
		neuart_start(neuartas);
		mutex_exit(&neuart->neuart_excl);
		break;

	case M_BREAK:
	case M_DELAY:
	case M_DATA:
		/*
		 * Queue the message up to be transmitted,
		 * and poke the start routine.
		 */
		(void) putq(q, mp);
		mutex_enter(&neuart->neuart_excl);
		neuart_start(neuartas);
		mutex_exit(&neuart->neuart_excl);
		break;

	case M_STOPI:
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
		if (!(neuartas->async_inflow_source & IN_FLOW_USER)) {
			neuart_flowcontrol_hw_input(neuart, FLOW_STOP,
			    IN_FLOW_USER);
			(void) neuart_flowcontrol_sw_input(neuart, FLOW_STOP,
			    IN_FLOW_USER);
		}
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		freemsg(mp);
		break;

	case M_STARTI:
		mutex_enter(&neuart->neuart_excl);
		mutex_enter(&neuart->neuart_excl_hi);
		if (neuartas->async_inflow_source & IN_FLOW_USER) {
			neuart_flowcontrol_hw_input(neuart, FLOW_START,
			    IN_FLOW_USER);
			(void) neuart_flowcontrol_sw_input(neuart, FLOW_START,
			    IN_FLOW_USER);
		}
		mutex_exit(&neuart->neuart_excl_hi);
		mutex_exit(&neuart->neuart_excl);
		freemsg(mp);
		break;

	case M_CTL:
		if (MBLKL(mp) >= sizeof (struct iocblk) &&
		    ((struct iocblk *)mp->b_rptr)->ioc_cmd == MC_POSIXQUERY) {
			((struct iocblk *)mp->b_rptr)->ioc_cmd = MC_HAS_POSIX;
			qreply(q, mp);
		} else {
			/*
			 * These MC_SERVICE type messages are used by upper
			 * modules to tell this driver to send input up
			 * immediately, or that it can wait for normal
			 * processing that may or may not be done.  Sun
			 * requires these for the mouse module.
			 * (XXX - for x86?)
			 */
			mutex_enter(&neuart->neuart_excl);
			switch (*mp->b_rptr) {

			case MC_SERVICEIMM:
				neuartas->async_flags |= NEUARTAS_SERVICEIMM;
				break;

			case MC_SERVICEDEF:
				neuartas->async_flags &=
						~NEUARTAS_SERVICEIMM;
				break;
			}
			mutex_exit(&neuart->neuart_excl);
			freemsg(mp);
		}
		break;

	case M_IOCDATA:
		neuart_iocdata(q, mp);
		break;

	default:
		freemsg(mp);
		break;
	}
	return (0);
}

/*
 * Retry an "ioctl", now that "bufcall" claims we may be able to allocate
 * the buffer we need.
 */
static void
neuart_reioctl(void *unit)
{
	int instance = (uintptr_t)unit;
	struct neuart_asyncline *neuartas;
	struct neuart_com *neuart;
	queue_t	*q;
	mblk_t	*mp;

	neuart = ddi_get_soft_state(neuart_soft_state, instance);
	ASSERT(neuart != NULL);
	neuartas = neuart->neuart_priv;

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(&neuart->neuart_excl);
	neuartas->async_wbufcid = 0;
	if ((q = neuartas->async_ttycommon.t_writeq) == NULL) {
		mutex_exit(&neuart->neuart_excl);
		return;
	}
	if ((mp = neuartas->async_ttycommon.t_iocpending) != NULL) {
		/* not pending any more */
		neuartas->async_ttycommon.t_iocpending = NULL;
		mutex_exit(&neuart->neuart_excl);
		neuart_ioctl(neuartas, q, mp);
	} else
		mutex_exit(&neuart->neuart_excl);
}

static void
neuart_iocdata(queue_t *q, mblk_t *mp)
{
	struct neuart_asyncline	*neuartas = (struct neuart_asyncline *)q->q_ptr;
	struct neuart_com	*neuart;
	struct iocblk *ip;
	struct copyresp *csp;
#ifdef DEBUG
	int instance = UNIT(neuartas->async_dev);
#endif

	neuart = neuartas->async_common;
	ip = (struct iocblk *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	if (csp->cp_rval != 0) {
		if (csp->cp_private)
			freemsg(csp->cp_private);
		freemsg(mp);
		return;
	}

	mutex_enter(&neuart->neuart_excl);
	DEBUGCONT2(NEUART_DEBUG_MODEM, "neuart%d_iocdata: case %s\n",
		instance,
		csp->cp_cmd == TIOCMGET ? "TIOCMGET" :
		csp->cp_cmd == TIOCMSET ? "TIOCMSET" :
		csp->cp_cmd == TIOCMBIS ? "TIOCMBIS" :
		    "TIOCMBIC");
	switch (csp->cp_cmd) {

	case TIOCMGET:
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		mp->b_datap->db_type = M_IOCACK;
		ip->ioc_error = 0;
		ip->ioc_count = 0;
		ip->ioc_rval = 0;
		mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		mutex_enter(&neuart->neuart_excl_hi);
		(void) neuart_mctl(neuart,
			dmto_neuart(*(int *)mp->b_cont->b_rptr),
			csp->cp_cmd);
		mutex_exit(&neuart->neuart_excl_hi);
		mioc2ack(mp, NULL, 0, 0);
		break;

	default:
		mp->b_datap->db_type = M_IOCNAK;
		ip->ioc_error = EINVAL;
		break;
	}
	qreply(q, mp);
	mutex_exit(&neuart->neuart_excl);
}

/*
 * debugger/console support routines.
 */

/*
 * put a character out
 * Do not use interrupts.  If char is LF, put out CR, LF.
 */
static void
neuart_putchar(cons_polledio_arg_t arg, uchar_t c)
{
	struct neuart_com *neuart = (struct neuart_com *)arg;

	if (c == '\n')
		neuart_putchar(arg, '\r');

	while ((ddi_get8(neuart->neuart_iohandle,
	    neuart->neuart_ioaddr + LSR) & XHRE) == 0) {
		/* wait for xmit to finish */
		drv_usecwait(10);
	}

	/* put the character out */
	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + DAT, c);
}

/*
 * See if there's a character available. If no character is
 * available, return 0. Run in polled mode, no interrupts.
 */
static boolean_t
neuart_ischar(cons_polledio_arg_t arg)
{
	struct neuart_com *neuart = (struct neuart_com *)arg;

	return ((ddi_get8(neuart->neuart_iohandle,
		neuart->neuart_ioaddr + LSR) & RCA) != 0);
}

/*
 * Get a character. Run in polled mode, no interrupts.
 */
static int
neuart_getchar(cons_polledio_arg_t arg)
{
	struct neuart_com *neuart = (struct neuart_com *)arg;

	while (!neuart_ischar(arg))
		drv_usecwait(10);
	return (ddi_get8(neuart->neuart_iohandle,
		neuart->neuart_ioaddr + DAT));
}

/*
 * Set or get the modem control status.
 */
static int
neuart_mctl(struct neuart_com *neuart, int bits, int how)
{
	int mcr_r, msr_r;
	int instance = neuart->neuart_unit;

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));
	ASSERT(mutex_owned(&neuart->neuart_excl));

	/* Read Modem Control Registers */
	mcr_r = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR);

	switch (how) {

	case TIOCMSET:
		DEBUGCONT2(NEUART_DEBUG_MODEM,
			"ne_uart%dmctl: TIOCMSET, bits = %x\n", instance, bits);
		mcr_r = bits;		/* Set bits	*/
		break;

	case TIOCMBIS:
		DEBUGCONT2(NEUART_DEBUG_MODEM,
			"ne_uart%dmctl: TIOCMBIS, bits = %x\n",
			instance, bits);
		mcr_r |= bits;		/* Mask in bits	*/
		break;

	case TIOCMBIC:
		DEBUGCONT2(NEUART_DEBUG_MODEM,
			"ne_uart%dmctl: TIOCMBIC, bits = %x\n",
			instance, bits);
		mcr_r &= ~bits;		/* Mask out bits */
		break;

	case TIOCMGET:
		/* Read Modem Status Registers */
		/*
		 * If modem interrupts are enabled, we return the
		 * saved value of msr. We read MSR only in neuart_msint()
		 */
		if (ddi_get8(neuart->neuart_iohandle,
		    neuart->neuart_ioaddr + ICR) & MIEN) {
			msr_r = neuart->neuart_msr;
			DEBUGCONT2(NEUART_DEBUG_MODEM,
				"ne_uart%dmctl: TIOCMGET, read msr_r = %x\n",
				instance, msr_r);
		} else {
			msr_r = ddi_get8(neuart->neuart_iohandle,
					neuart->neuart_ioaddr + MSR);
			DEBUGCONT2(NEUART_DEBUG_MODEM,
				"ne_uart%dmctl: TIOCMGET, read MSR = %x\n",
				instance, msr_r);
		}
		DEBUGCONT2(NEUART_DEBUG_MODEM,
			"ne_uart%dtodm: modem_lines = %x\n",
			instance, neuart_todm(mcr_r, msr_r));
		return (neuart_todm(mcr_r, msr_r));
	}

	ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR, mcr_r);

	return (mcr_r);
}

static int
neuart_todm(int mcr_r, int msr_r)
{
	int b = 0;

	/* MCR registers */
	if (mcr_r & RTS)
		b |= TIOCM_RTS;

	if (mcr_r & DTR)
		b |= TIOCM_DTR;

	/* MSR registers */
	if (msr_r & DCD)
		b |= TIOCM_CAR;

	if (msr_r & CTS)
		b |= TIOCM_CTS;

	if (msr_r & DSR)
		b |= TIOCM_DSR;

	if (msr_r & RI)
		b |= TIOCM_RNG;
	return (b);
}

static int
dmto_neuart(int bits)
{
	int b = 0;

	DEBUGCONT1(NEUART_DEBUG_MODEM, "dmto_neuart: bits = %x\n", bits);
#ifdef	CAN_NOT_SET	/* only DTR and RTS can be set */
	if (bits & TIOCM_CAR)
		b |= DCD;
	if (bits & TIOCM_CTS)
		b |= CTS;
	if (bits & TIOCM_DSR)
		b |= DSR;
	if (bits & TIOCM_RNG)
		b |= RI;
#endif

	if (bits & TIOCM_RTS) {
		DEBUGCONT0(NEUART_DEBUG_MODEM, "dmto_neuart: set b & RTS\n");
		b |= RTS;
	}
	if (bits & TIOCM_DTR) {
		DEBUGCONT0(NEUART_DEBUG_MODEM, "dmto_neuart: set b & DTR\n");
		b |= DTR;
	}

	return (b);
}

static void
neuart_error(int level, const char *fmt, ...)
{
	va_list adx;
	static	time_t	last;
	static	const char *lastfmt;
	time_t	now;

	/*
	 * Don't print the same error message too often.
	 * Print the message only if we have not printed the
	 * message within the last second.
	 * Note: that fmt cannot be a pointer to a string
	 * stored on the stack. The fmt pointer
	 * must be in the data segment otherwise lastfmt would point
	 * to non-sense.
	 */
	now = gethrestime_sec();
	if (last == now && lastfmt == fmt)
		return;

	last = now;
	lastfmt = fmt;

	va_start(adx, fmt);
	vcmn_err(level, fmt, adx);
	va_end(adx);
}

/*
 * neuart_parse_mode(dev_info_t *devi, struct neuart_com *neuart)
 * The value of this property is in the form of "9600,8,n,1,-"
 * 1) speed: 9600, 4800, ...
 * 2) data bits
 * 3) parity: n(none), e(even), o(odd)
 * 4) stop bits
 * 5) handshake: -(none), h(hardware: rts/cts), s(software: xon/off)
 *
 * This parsing came from a SPARCstation eeprom.
 */
static void
neuart_parse_mode(dev_info_t *devi, struct neuart_com *neuart)
{
	char		name[40];
	char		val[40];
	int		len;
	int		ret;
	char		*p;
	char		*p1;

	ASSERT(neuart->neuart_com_port != 0);

	/*
	 * Parse the ttyx-mode property
	 */
	(void) sprintf(name, "tty%c-mode", neuart->neuart_com_port + 'a' - 1);
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-mode",
				neuart->neuart_com_port + '0');
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}

	/* no property to parse */
	neuart->neuart_cflag = 0;
	if (ret != DDI_PROP_SUCCESS)
		return;

	p = val;
	/* ---- baud rate ---- */

	/* initial default */
	neuart->neuart_cflag = CREAD|neuart_default_baudrate;
	if (p && (p1 = strchr(p, ',')) != 0) {
		*p1++ = '\0';
	} else {
		neuart->neuart_cflag |= BITS8;	/* add default bits */
		return;
	}

	if (strcmp(p, "110") == 0)
		neuart->neuart_bidx = B110;
	else if (strcmp(p, "150") == 0)
		neuart->neuart_bidx = B150;
	else if (strcmp(p, "300") == 0)
		neuart->neuart_bidx = B300;
	else if (strcmp(p, "600") == 0)
		neuart->neuart_bidx = B600;
	else if (strcmp(p, "1200") == 0)
		neuart->neuart_bidx = B1200;
	else if (strcmp(p, "2400") == 0)
		neuart->neuart_bidx = B2400;
	else if (strcmp(p, "4800") == 0)
		neuart->neuart_bidx = B4800;
	else if (strcmp(p, "9600") == 0)
		neuart->neuart_bidx = B9600;
	else if (strcmp(p, "19200") == 0)
		neuart->neuart_bidx = B19200;
	else if (strcmp(p, "38400") == 0)
		neuart->neuart_bidx = B38400;
	else if (strcmp(p, "57600") == 0)
		neuart->neuart_bidx = B57600;
	else if (strcmp(p, "115200") == 0)
		neuart->neuart_bidx = B115200;
	else
		neuart->neuart_bidx = B9600;

	neuart->neuart_cflag &= ~CBAUD;
	if (neuart->neuart_bidx > CBAUD) {
		/* > 38400 uses the CBAUDEXT bit */
		neuart->neuart_cflag |= CBAUDEXT;
		neuart->neuart_cflag |= neuart->neuart_bidx - CBAUD - 1;
	} else {
		neuart->neuart_cflag |= neuart->neuart_bidx;
	}

	ASSERT(neuart->neuart_bidx == BAUDINDEX(neuart->neuart_cflag));

	/* ---- Next item is data bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		neuart->neuart_cflag |= BITS8;	/* add default bits */
		return;
	}
	switch (*p) {
		default:
		case '8':
			neuart->neuart_cflag |= CS8;
			neuart->neuart_lcr = BITS8;
			break;
		case '7':
			neuart->neuart_cflag |= CS7;
			neuart->neuart_lcr = BITS7;
			break;
		case '6':
			neuart->neuart_cflag |= CS6;
			neuart->neuart_lcr = BITS6;
			break;
		case '5':
			/* LINTED: CS5 is currently zero (but might change) */
			neuart->neuart_cflag |= CS5;
			neuart->neuart_lcr = BITS5;
			break;
	}

	/* ---- Parity info ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	switch (*p)  {
		default:
		case 'n':
			break;
		case 'e':
			neuart->neuart_cflag |= PARENB;
			neuart->neuart_lcr |= PEN; break;
		case 'o':
			neuart->neuart_cflag |= PARENB|PARODD;
			neuart->neuart_lcr |= PEN|EPS;
			break;
	}

	/* ---- Find stop bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	if (*p == '2') {
		neuart->neuart_cflag |= CSTOPB;
		neuart->neuart_lcr |= STB;
	}

	/* ---- handshake is next ---- */
	p = p1;
	if (p) {
		if ((p1 = strchr(p, ',')) != 0)
			*p1++ = '\0';

		if (*p == 'h')
			neuart->neuart_cflag |= CRTSCTS;
		else if (*p == 's')
			neuart->neuart_cflag |= CRTSXOFF;
	}
}

/*
 * Check for abort character sequence
 */
static boolean_t
abort_charseq_recognize(uchar_t ch)
{
	static int state = 0;
#define	CNTRL(c) ((c)&037)
	static char sequence[] = { '\r', '~', CNTRL('b') };

	if (ch == sequence[state]) {
		if (++state >= sizeof (sequence)) {
			state = 0;
			return (B_TRUE);
		}
	} else {
		state = (ch == sequence[0]) ? 1 : 0;
	}
	return (B_FALSE);
}

/*
 * Flow control functions
 */
/*
 * Software input flow control
 * This function can execute software input flow control sucessfully
 * at most of situations except that the line is in BREAK status
 * (timed and untimed break).
 * INPUT VALUE of onoff:
 *               FLOW_START means to send out a XON char
 *                          and clear SW input flow control flag.
 *               FLOW_STOP means to send out a XOFF char
 *                          and set SW input flow control flag.
 *               FLOW_CHECK means to check whether there is pending XON/XOFF
 *                          if it is true, send it out.
 * INPUT VALUE of type:
 *		 IN_FLOW_RINGBUFF means flow control is due to RING BUFFER
 *		 IN_FLOW_STREAMS means flow control is due to STREAMS
 *		 IN_FLOW_USER means flow control is due to user's commands
 * RETURN VALUE: B_FALSE means no flow control char is sent
 *               B_TRUE means one flow control char is sent
 */
static boolean_t
neuart_flowcontrol_sw_input(struct neuart_com *neuart,
    neuart_flowc_action onoff, int type)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int instance = UNIT(neuartas->async_dev);
	int rval = B_FALSE;

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));

	if (!(neuartas->async_ttycommon.t_iflag & IXOFF))
		return (rval);

	/*
	 * If we get this far, then we know IXOFF is set.
	 */
	switch (onoff) {
	case FLOW_STOP:
		neuartas->async_inflow_source |= type;

		/*
		 * We'll send an XOFF character for each of up to
		 * three different input flow control attempts to stop input.
		 * If we already send out one XOFF, but FLOW_STOP comes again,
		 * it seems that input flow control becomes more serious,
		 * then send XOFF again.
		 */
		if (neuartas->async_inflow_source & (IN_FLOW_RINGBUFF |
		    IN_FLOW_STREAMS | IN_FLOW_USER))
			neuartas->async_flags |= NEUARTAS_SW_IN_FLOW |
			    NEUARTAS_SW_IN_NEEDED;
		DEBUGCONT2(NEUART_DEBUG_SFLOW, "neuart%d: input sflow stop, "
		    "type = %x\n", instance, neuartas->async_inflow_source);
		break;
	case FLOW_START:
		neuartas->async_inflow_source &= ~type;
		if (neuartas->async_inflow_source == 0) {
			neuartas->async_flags = (neuartas->async_flags &
			    ~NEUARTAS_SW_IN_FLOW) | NEUARTAS_SW_IN_NEEDED;
			DEBUGCONT1(NEUART_DEBUG_SFLOW, "neuart%d: "
			    "input sflow start\n", instance);
		}
		break;
	default:
		break;
	}

	if (((neuartas->async_flags &
	    (NEUARTAS_SW_IN_NEEDED | NEUARTAS_BREAK |
	    NEUARTAS_OUT_SUSPEND)) == NEUARTAS_SW_IN_NEEDED) &&
	    (ddi_get8(neuart->neuart_iohandle,
		neuart->neuart_ioaddr + LSR) & XHRE)) {
		/*
		 * If we get this far, then we know we need to send out
		 * XON or XOFF char.
		 */
		neuartas->async_flags = (neuartas->async_flags &
		    ~NEUARTAS_SW_IN_NEEDED) | NEUARTAS_BUSY;
		ddi_put8(neuart->neuart_iohandle, neuart->neuart_ioaddr + DAT,
		    neuartas->async_flags & NEUARTAS_SW_IN_FLOW ?
		    neuartas->async_stopc : neuartas->async_startc);
		rval = B_TRUE;
	}
	return (rval);
}

/*
 * Software output flow control
 * This function can be executed sucessfully at any situation.
 * It does not handle HW, and just change the SW output flow control flag.
 * INPUT VALUE of onoff:
 *                 FLOW_START means to clear SW output flow control flag,
 *			also combine with HW output flow control status to
 *			determine if we need to set NEUARTAS_OUT_FLW_RESUME.
 *                 FLOW_STOP means to set SW output flow control flag,
 *			also clear NEUARTAS_OUT_FLW_RESUME.
 */
static void
neuart_flowcontrol_sw_output(struct neuart_com *neuart,
    neuart_flowc_action onoff)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int instance = UNIT(neuartas->async_dev);

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));

	if (!(neuartas->async_ttycommon.t_iflag & IXON))
		return;

	switch (onoff) {
	case FLOW_STOP:
		neuartas->async_flags |= NEUARTAS_SW_OUT_FLW;
		neuartas->async_flags &= ~NEUARTAS_OUT_FLW_RESUME;
		DEBUGCONT1(NEUART_DEBUG_SFLOW,
		    "neuart%d: output sflow stop\n",
		    instance);
		break;
	case FLOW_START:
		neuartas->async_flags &= ~NEUARTAS_SW_OUT_FLW;
		if (!(neuartas->async_flags & NEUARTAS_HW_OUT_FLW))
			neuartas->async_flags |= NEUARTAS_OUT_FLW_RESUME;
		DEBUGCONT1(NEUART_DEBUG_SFLOW,
		    "neuart%d: output sflow start\n",
		    instance);
		break;
	default:
		break;
	}
}

/*
 * Hardware input flow control
 * This function can be executed sucessfully at any situation.
 * It directly changes RTS depending on input parameter onoff.
 * INPUT VALUE of onoff:
 *       FLOW_START means to clear HW input flow control flag,
 *                  and pull up RTS if it is low.
 *       FLOW_STOP means to set HW input flow control flag,
 *                  and low RTS if it is high.
 * INPUT VALUE of type:
 *		 IN_FLOW_RINGBUFF means flow control is due to RING BUFFER
 *		 IN_FLOW_STREAMS means flow control is due to STREAMS
 *		 IN_FLOW_USER means flow control is due to user's commands
 */
static void
neuart_flowcontrol_hw_input(struct neuart_com *neuart,
    neuart_flowc_action onoff, int type)
{
	uchar_t	mcr;
	uchar_t	flag;
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int instance = UNIT(neuartas->async_dev);

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));

	if (!(neuartas->async_ttycommon.t_cflag & CRTSXOFF))
		return;

	switch (onoff) {
	case FLOW_STOP:
		neuartas->async_inflow_source |= type;
		if (neuartas->async_inflow_source & (IN_FLOW_RINGBUFF |
		    IN_FLOW_STREAMS | IN_FLOW_USER))
			neuartas->async_flags |= NEUARTAS_HW_IN_FLOW;
		DEBUGCONT2(NEUART_DEBUG_HFLOW, "neuart%d: input hflow stop, "
		    "type = %x\n", instance, neuartas->async_inflow_source);
		break;
	case FLOW_START:
		neuartas->async_inflow_source &= ~type;
		if (neuartas->async_inflow_source == 0) {
			neuartas->async_flags &= ~NEUARTAS_HW_IN_FLOW;
			DEBUGCONT1(NEUART_DEBUG_HFLOW, "neuart%d: "
			    "input hflow start\n", instance);
		}
		break;
	default:
		break;
	}
	mcr = ddi_get8(neuart->neuart_iohandle, neuart->neuart_ioaddr + MCR);
	flag = (neuartas->async_flags & NEUARTAS_HW_IN_FLOW) ? 0 : RTS;

	if (((mcr ^ flag) & RTS) != 0) {
		ddi_put8(neuart->neuart_iohandle,
		    neuart->neuart_ioaddr + MCR, (mcr ^ RTS));
	}
}

/*
 * Hardware output flow control
 * This function can execute HW output flow control sucessfully
 * at any situation.
 * It doesn't really change RTS, and just change
 * HW output flow control flag depending on CTS status.
 * INPUT VALUE of onoff:
 *                FLOW_START means to clear HW output flow control flag.
 *			also combine with SW output flow control status to
 *			determine if we need to set NEUARTAS_OUT_FLW_RESUME.
 *                FLOW_STOP means to set HW output flow control flag.
 *			also clear NEUARTAS_OUT_FLW_RESUME.
 */
static void
neuart_flowcontrol_hw_output(struct neuart_com *neuart,
    neuart_flowc_action onoff)
{
	struct neuart_asyncline *neuartas = neuart->neuart_priv;
	int instance = UNIT(neuartas->async_dev);

	ASSERT(mutex_owned(&neuart->neuart_excl_hi));

	if (!(neuartas->async_ttycommon.t_cflag & CRTSCTS))
		return;

	switch (onoff) {
	case FLOW_STOP:
		neuartas->async_flags |= NEUARTAS_HW_OUT_FLW;
		neuartas->async_flags &= ~NEUARTAS_OUT_FLW_RESUME;
		DEBUGCONT1(NEUART_DEBUG_HFLOW,
		    "neuart%d: output hflow stop\n",
		    instance);
		break;
	case FLOW_START:
		neuartas->async_flags &= ~NEUARTAS_HW_OUT_FLW;
		if (!(neuartas->async_flags & NEUARTAS_SW_OUT_FLW))
			neuartas->async_flags |= NEUARTAS_OUT_FLW_RESUME;
		DEBUGCONT1(NEUART_DEBUG_HFLOW,
		    "neuart%d: output hflow start\n",
		    instance);
		break;
	default:
		break;
	}
}
