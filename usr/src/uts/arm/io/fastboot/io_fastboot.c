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
 * Copyright (c) 2007-2008 NEC Corporation
 */

#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kobj.h>
#include <sys/cpuvar.h>
#include <sys/types.h>
#include "io_fastboot.h"

#ifdef IO_FASTBOOT_ENABLE
#include "io_module_queue.h"


/* queue control structure */
typedef struct io_waitq_ctrl {
	kmutex_t mutex;                 /* mutex */
	io_modinfo_t *head;             /* head of queue */
	io_modinfo_t *tail;             /* tail of queue */
} io_waitqctrl_t;

typedef struct io_initq_ctrl {
	kmutex_t mutex;
	int head;
	int tail;
	int bound;
}io_initq_ctrl_t;

static int io_modinfo_init(void);
static void io_modinfo_fini(void);
static int io_modqueue_empty(void);
static io_modinfo_t *io_modqueue_initdeq(void);
static io_modinfo_t *io_modqueue_waitdeq(void);
static void io_modqueue_waitenq(io_modinfo_t *);
static struct modctl* io_modctl_search(const char*);
static int io_modinfo_check_status(io_modinfo_t *);
static int io_modinfo_check_dependence(io_modinfo_t *);
static void io_attach_drivers(void);

static void _attach_drivers();
static void _io_modinfo_set_status(io_modinfo_t*, char);
static void _io_modqueue_enq(io_waitqctrl_t*, io_modinfo_t*);
static io_modinfo_t *_io_modqueue_deq(io_waitqctrl_t*);
static int io_atoi(const char*);
static void io_convert_string2number(io_modinfo_t*, int*);


static uint_t io_ncpu;                 /* CPU number */
static int _io_assigned_cpu = 0;       /* assigned CPU id */
static kmutex_t _io_assigned_cpu_mutex;/* mutex fot _io_assigned_cpu */
static int _io_thread_finish = 0;      /* finish flag */
static kmutex_t _io_thread_mutex;
static int _io_init_check = 0;         /* The flag which raises,   */
                                       /* when initialization end. */
static io_initq_ctrl_t io_initq;       /* initialaization queue controller */
static io_waitqctrl_t io_waitq;            /* wait queue controller */


/*
 * static int
 * io_modinfo_init()
 *      The io_modinfo_init() function set defaults as a variable, 
 *      and initialize mutex.
 *
 * Calling/Exit State:
 *      - return value
 *           0                  The initialization was successful.
 *          IO_ERR_DUPLICATION  The initialization was already carried out.
 */
static int
io_modinfo_init()
{
	int i;

	/* When initialization is already carried out, */
	/* it returned IO_ERR_DUPLICATION.             */
	if (_io_init_check == 0) {
		_io_init_check = 1;
	} else {
		return IO_ERR_DUPLICATION;
	}

	_io_thread_finish = 0;
	_io_assigned_cpu = 0;
	io_initq.head = 0;
	io_initq.tail = IO_TAIL;
	io_initq.bound = IO_BOUND;

	io_waitq.head = NULL;
	io_waitq.tail = NULL;

	mutex_init(&io_initq.mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&io_waitq.mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&_io_thread_mutex, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&_io_assigned_cpu_mutex, NULL, MUTEX_DRIVER, NULL);

	for (i = 0; i < IO_TAIL; i++) {
		mutex_init(&(modinfo_array[i].mutex),
		                             NULL, MUTEX_DRIVER, NULL);
	}

	return(0);
}


/*
 * static void
 * io_modinfo_fini()
 *      The io_modinfo_init() function free mutex 
 *      and set defaults as a variable.
 */
static void
io_modinfo_fini()
{
	int i;

	mutex_destroy(&io_initq.mutex);
	mutex_destroy(&io_waitq.mutex);
	mutex_destroy(&_io_thread_mutex);
	mutex_destroy(&_io_assigned_cpu_mutex);

	for (i = 0; i < IO_TAIL; i++) {
		mutex_destroy(&(modinfo_array[i].mutex));
	}

	io_initq.head = 0;
	io_initq.tail = 0;
	io_initq.bound = 0;
	io_waitq.head = NULL;
	io_waitq.tail = NULL;
	_io_thread_finish = 0;
	_io_assigned_cpu = 0;
}


/*
 * static int
 * io_modqueue_empty()
 *      The io_modqueue_empty() function confirmed that 
 *      an initialization queue is empty.
 *
 * Calling/Exit State:
 *      - return value
 *          0 : The initialization queue is not empty.
 *          1 : The initialization queue is empty.
 *
 */
static int
io_modqueue_empty()
{
	if (io_initq.head >= io_initq.bound) {
		return 1;
	} else {
		return 0;
	}
}


/*
 * static io_modinfo_t *
 * io_modqueue_initdeq()
 *       Get the module from the initialaization queue.
 *
 * Calling/Exit State:
 *      - return value
 *          pointer  The pointer to the io_modinfo structure.
 *          NULL     The initialization queue was empty.
 */
static io_modinfo_t *
io_modqueue_initdeq()
{
	io_modinfo_t *rtn = NULL;

	mutex_enter(&io_initq.mutex);
retry:
	if (io_initq.head >= io_initq.bound) {
		goto end;
	}
	rtn = &modinfo_array[io_initq.head];
	io_initq.head++;

#if USB_INIT_ENABLE
	if (strcmp("ehci",rtn->name) == 0 ||
	    strcmp("uhci",rtn->name) == 0|| strcmp("ohci",rtn->name) == 0) {
		rtn = NULL;
		goto retry;
	}
#endif
end:
	mutex_exit(&io_initq.mutex);
	return rtn;
}


/*
 * static void
 * _io_modinfo_set_status(io_modinfo_t *modp, char flag)
 *      The _io_modinfo_set_status() function set status of module.
 *
 * Calling/Exit State:
 *      - argument
 *           modp  The pointer to the io_modinfo structure.
 *           flag  The status of module.
 */
static void
_io_modinfo_set_status(io_modinfo_t *modp, char flag)
{

	if (modp->status == IO_STS_COMP) {
		return;
	}

	mutex_enter(&modp->mutex);
	modp->status = flag;
	mutex_exit(&modp->mutex);

	return;
}


/*
 * static void
 * _io_modqueue_enq(io_waitqctrl_t *qctrl, io_modinfo_t *mp)
 *      The _io_modqueue_enq() function inserts io_modinfo structure in queue.
 *
 * Calling/Exit State:
 *      - argument
 *           qctrl  The pointer to the queue controller structure.
 *           mp     The pointer to the io_modinfo structure.
 *
 * Remarks:
 *      When function access queue, it must lock queue.
 */
static void
_io_modqueue_enq(io_waitqctrl_t *qctrl, io_modinfo_t *mp)
{
	mutex_enter(&qctrl->mutex);

	mp->qnext = NULL;

	if (qctrl->head == NULL) {
		qctrl->head = mp;
		qctrl->tail = mp;
	} else {
		qctrl->tail->qnext = mp;
		qctrl->tail = mp;
	}

	mutex_exit(&qctrl->mutex);
}


/*
 * static io_modinfo_t *
 * _io_modqueue_deq(io_waitqctrl_t *qctrl)
 *      The _io_modqueue_deq() function dequeue module
 *      from the module queue.
 *
 * Calling/Exit State:
 *      - return value
 *          pointer  The pointer to the io_modinfo structure.
 *          NULL     The queue was empty.
 *
 *      - argument
 *           qctrl  The pointer to the queue controller structure.
 *
 * Remarks:
 *      When function access queue, it must lock queue.
 */
static io_modinfo_t *
_io_modqueue_deq(io_waitqctrl_t *qctrl)
{
	io_modinfo_t *ret = NULL;

	mutex_enter(&qctrl->mutex);

	if (qctrl->head == NULL) {
		goto end;
	}

	ret = qctrl->head;
	qctrl->head = ret->qnext;
	ret->qnext = NULL;
	if (qctrl->head == NULL) {
		qctrl->tail = NULL;
	}

end:
	mutex_exit(&qctrl->mutex);
	return(ret);
}


/*
 * static void
 * io_modqueue_waitenq(io_modinfo_t *module)
 *      The io_modqueue_waitenq() function enqueue a module in the wait queue.
 *
 * Calling/Exit State:
 *      - argument
 *           module  The pointer to the io_modinfo structure.
 */
static void
io_modqueue_waitenq(io_modinfo_t *module)
{
	_io_modqueue_enq(&io_waitq, module);
}


/*
 * static io_modinfo_t *
 * io_modqueue_waitdeq()
 *      The io_modqueue_waitdeq() function dequeue the module 
 *      from the wait queue. Dependence of a module is settled,
 *      and it's possible to initialize.
 *
 * Calling/Exit State:
 *      - return value
 *          pointer  The pointer to the io_modinfo structure.
 *          NULL     The wait queue was empty or the module,
 *                   it's possible to initialize, doesn't exist.
 *
 * Description:
 *      When function access queue, it must lock queue.
 */
static io_modinfo_t *
io_modqueue_waitdeq()
{
	io_modinfo_t *ret = NULL;
	io_modinfo_t *tmp = NULL;
	io_modinfo_t *old = NULL;    /* set the pointer of previous module */
	int i, rtn;

	mutex_enter(&io_waitq.mutex);

	/* if queue is empty , goto end */
	if ((&io_waitq)->head == NULL) {
		goto end;
	}

	/* search from top */
	tmp = (&io_waitq)->head;
	while (tmp != NULL) {
		rtn = io_modinfo_check_dependence(tmp);
		if (rtn == 0) {
			/* find the target */
			ret = tmp;
			/* Target is head of queue. */
			if (old == NULL) {
				(&io_waitq)->head = ret->qnext;
				if ((&io_waitq)->head == NULL) {
					(&io_waitq)->tail = NULL;
				}
			} else {
				old->qnext = tmp->qnext;
				if (old->qnext == NULL) {
					(&io_waitq)->tail = old;
				}
			}
			ret->qnext = NULL;
			goto end;
		}

		old = tmp;
		tmp = tmp->qnext;
	}

end:
	mutex_exit(&io_waitq.mutex);
	return(ret);

}


/*
 * static struct modctl *
 * io_modctl_search(const char *modname)
 *      The io_modctl_search function search a module from the list by modname.
 *
 * Calling/Exit State:
 *      - return value
 *          pointer  The pointer to the modctl structure.
 *          NULL     The modctl structure is not found.
 *
 *      - argument
 *           modname  module name
 */
static struct modctl *
io_modctl_search(const char *modname)
{
	const char	*target;
	struct modctl	*mp;
	char		*curname;
	int		found = 0;

	if(modname == NULL) {
		return NULL;
	}
	mutex_enter(&mod_lock);

	if ((target = strrchr(modname, '/')) == NULL) {
		target = modname;
	} else {
		modname++;
	}

	mp = &modules;
	do {
		if (strcmp(target, mp->mod_modname) == 0) {
			found = 1;
			break;
		}
	} while ((mp = mp->mod_next) != &modules);

	mutex_exit(&mod_lock);

	if (found ==1) {
		return mp;
	} else {
		return NULL;
	}
}


/* internal function */
#define ISDIGIT(_c) \
	((_c) >= '0' && (_c) <= '9')

/*
 * static int
 * io_atoi(const char *str)
 *      The io_atoi() function converte character to value.
 *
 * Calling/Exit State:
 *      - return value
 *          Thae atoi() function return the converted value.
 *
 *      - argument
 *           str  number character
 *
 * Remarks:
 *      Thae atoi() function expect number character.
 */
static int
io_atoi(const char *str)
{
	int ret = 0;

	while (ISDIGIT(*str)) {
		ret = ret * 10 + (*str++ - '0');
	}
	return  ret;
}


/*
 * static void
 * io_convert_string2number(io_modinfo_t *module, int *parents_index)
 *      The io_convert_string2number() function convert string
 *      to index of array.
 *
 * Calling/Exit State:
 *      - argument
 *           module         The pointer to the io_modinfo structure.
 *           parents_index  The array for dependence module stocks.
 */
static void
io_convert_string2number(io_modinfo_t *module, int *parents_index)
{
	char *buff, *start;
	char data[IO_MAX_STRING];
	int count = 0;

	memset(data, '\0', IO_MAX_STRING);
	memcpy(data, module->parents, strlen(module->parents) + 1);

	start = data;
	buff = data;

	for (; *buff != '\0'; buff++) {
		if (*buff == ',') {
			*buff = '\0';
			parents_index[count++] = io_atoi(start);
			start = buff + 1;
		}

		if (*buff == ';') {
			*buff = '\0';
			parents_index[count++] = io_atoi(start);
			break;
		}
	}
}


/*
 * static int
 * io_modinfo_check_dependence(io_modinfo_t *module)
 *      The io_modinfo_check_dependence() function check the state of a module
 *      which depends.
 *
 * Calling/Exit State:
 *      - return value
 *           0  Initialization of a module is possible.
 *          -1  Initialization of a module is impossible.
 *
 *      - argument
 *           module  The pointer to the io_modinfo structure.
 */
static int
io_modinfo_check_dependence(io_modinfo_t *module)
{
	int i, rtn;
	int parents_index[IO_MAX_PARENT];

	io_convert_string2number(module, parents_index);

	for (i = 0; i < module->pcnt; i++) {
		rtn =
		    io_modinfo_check_status(&modinfo_array[parents_index[i]]);
		if (rtn == -1) {
			return -1;
		}
		else if (rtn == 1) {
			_io_modinfo_set_status(&modinfo_array[parents_index[i]]
			                       ,IO_STS_COMP);
		}
		rtn = io_modinfo_check_dependence(
		                            &modinfo_array[parents_index[i]]);
		if (rtn < 0) {
			return -1;
		}
	}
	return 0;
}


/*
 * static int
 * io_modinfo_check_status(io_modinfo_t *module)
 *      Check the status of module, and decide the next action.
 *
 * Calling/Exit State:
 *
 *      - return value
 *           1  Initialization of a module is already complete.
 *           0  Initialization of a module is possible.
 *          -1  Initialization of a module is impossible.
 *
 *      - argument
 *           module  The pointer to the io_modinfo structure.
 *
 * Remarks:
 *      The dtrace modlue is specal. When modctl structure of dtrace exist,
 *      return 0.
 */
static int
io_modinfo_check_status(io_modinfo_t *module)
{
	int rtn = 0;
	struct modctl * mop;

	/* check status flag */
	if ( module->status == IO_STS_COMP ) {
		rtn = 1;
	} else if (module->status == IO_STS_BUSY) {
		rtn = -1;
	} else {
		mop = io_modctl_search(module->name);
		if (mop == NULL) {
			rtn = 0;
		} else {
			if(mop->mod_installed == 1) {
				rtn = 1;
			} else {
				/* "dtrace" and "ib" is special module.      */
				/* "dtrace" is initialized another process.  */
				/* "ib"  is already initialized              */
				if (strcmp(mop->mod_modname, "dtrace") == 0) {
					rtn = 0;
				} else if (strcmp(mop->mod_modname, "ib") \
				                                       == 0) {
					rtn = 0;
				} else {
					rtn = -1;
				}
			}
		}
	}

	return rtn;
}


/*
 * static void 
 * io_attach_drivers()
 *      The function which controls the initialization of module.
 *      
 * Description:
 *      When set finish flag, it must lock.
 */
static void
io_attach_drivers()
{
	int flag, fini_flag = 0, cpuid;

	/* set thread cpuX */
	mutex_enter(&_io_assigned_cpu_mutex);
	cpuid = _io_assigned_cpu;
	if(cpu[cpuid] != NULL) {
		_io_assigned_cpu++;
	} else {
		cpuid = -1;
	}
	mutex_exit(&_io_assigned_cpu_mutex);
	if(cpuid != -1) {
		thread_affinity_set(curthread, cpuid);
	}

	while (!io_modqueue_empty()) {
		flag = 0;

		io_modinfo_t *modp = io_modqueue_waitdeq();

		if (modp == NULL) {
			modp = io_modqueue_initdeq();
			if (modp == NULL) {
				delay(1);
				continue;
			}
			flag = 1;
		}

		/* check status */
		int sts = io_modinfo_check_status(modp);
		if (sts == 1) {
			/* In terms of reducing bootup time,           */
			/* initializing a module which was initialized */
			/* already should be skipped.                  */
			/* But it's initialized just in case.          */
			goto initialize;
		} else if (sts == -1) {
			io_modqueue_waitenq(modp);
			continue;
		}

		if (flag == 1) {
			/* check dependence */
			if (io_modinfo_check_dependence(modp) != 0) {
				io_modqueue_waitenq(modp);
				continue;
			}
		}

initialize:
		/* set status busy */
		_io_modinfo_set_status(modp, IO_STS_BUSY);

		/* do init */
		struct devnames *dnp = &devnamesp[modp->major];
		if ((dnp->dn_flags & DN_FORCE_ATTACH) && 
		 (ddi_hold_installed_driver((major_t)(modp->major)) != NULL)) {
			ddi_rele_driver((major_t)modp->major);
		}
		/* set status end */
		_io_modinfo_set_status(modp, IO_STS_COMP);
	}

	/* finish flag on */
	mutex_enter(&_io_thread_mutex);
	fini_flag = ++_io_thread_finish;
	mutex_exit(&_io_thread_mutex);

	/* if all finish flag on, do end processing. */
	if (fini_flag == io_ncpu) {
		/* Initialize module in wait queue, sequentially */
		while(io_waitq.head != NULL){
			io_modinfo_t *modp = _io_modqueue_deq(&io_waitq);
			struct devnames *dnp = &devnamesp[modp->major];
			if ((dnp->dn_flags & DN_FORCE_ATTACH) && 
			    (ddi_hold_installed_driver((major_t)(modp->major))
			    != NULL)) {
				ddi_rele_driver((major_t)modp->major);
			}
		}

		io_modinfo_fini();
	}

}
#endif /* IO_FASTBOOT_ENABLE */

#if USB_INIT_ENABLE
#define FASTBOOT_CALLED_COUNT	2	/* Called count while booting */
static kmutex_t usb_init_mutex;        /* mutex for initialization of USB */
static int fastboot_timer = 0;

/*
 * int
 * fastboot_usb_init()
 *      The fastboot_usb_init() function initialize USB host controller.
 *
 * Calling/Exit State:
 *      - return values
 *           0  The initialization wsa successful.
 *          -1  The initialization wsa failed.
 */
int
fastboot_usb_init()
{
	int rtn = 0;

	mutex_enter(&usb_init_mutex);

	if(ddi_hold_installed_driver(ddi_name_to_major("ehci")) == NULL) {
		rtn = -1;
	}

	if(ddi_hold_installed_driver(ddi_name_to_major("uhci")) == NULL) {
		rtn = -1;
	}

	if(ddi_hold_installed_driver(ddi_name_to_major("ohci")) == NULL) {
		rtn = -1;
	}
	mutex_exit(&usb_init_mutex);

	return rtn;
}
#endif /* USB_INIT_ENABLE */

#if defined(USB_INIT_ENABLE) || defined(IO_FASTBOOT_ENABLE)
/*
 * static void 
 * _attach_drivers()
 *      The _attach_drivers() function initialize drivers.
 *      This function is a copy of attach_drivers()[uts/common/os/devcfg.c].
 */
static void
_attach_drivers()
{
	int i;

	for (i = 0; i < devcnt; i++) {
		struct devnames *dnp = &devnamesp[i];

#if USB_INIT_ENABLE
		if (dnp->dn_name != NULL && 
		    (strcmp(dnp->dn_name, "ehci") == 0 ||
		    strcmp(dnp->dn_name, "uhci") == 0 ||
		    strcmp(dnp->dn_name, "ohci") == 0)) {
			continue;
		}
#endif
		if ((dnp->dn_flags & DN_FORCE_ATTACH) &&
		    (ddi_hold_installed_driver((major_t)i) != NULL))
			ddi_rele_driver((major_t)i);
	}
}

/*
 * void 
 * fastboot_thread_create_initdrv_parallel()
 *      The fastboot_thread_create_initdrv_parallel() function initialize 
 *      resource, set and create thread.
 */
void
fastboot_thread_create_initdrv_parallel()
{
	int i, rtn;

#if USB_INIT_ENABLE
	if (fastboot_timer == 0) {
		mutex_init(&usb_init_mutex, NULL, MUTEX_DRIVER, NULL);
		timeout((void *)fastboot_usb_init, NULL,
		    (USB_INIT_TIMER * drv_usectohz(1000000)));
	}
	mutex_enter(&usb_init_mutex);
	fastboot_timer++;
	mutex_exit(&usb_init_mutex);
#endif

#ifdef IO_FASTBOOT_ENABLE
	rtn = io_modinfo_init();
	if (rtn == 0) {
		io_ncpu = ((boot_max_ncpus == -1) ? 
			   max_ncpus : boot_max_ncpus);
		for (i = 0; i < io_ncpu; i++) {
			(void) thread_create(NULL, 0,
			    (void (*)())io_attach_drivers,
			    NULL, 0, &p0, TS_RUN, minclsyspri);
		}
	} else
#endif
	{
		/* original process */
		(void) thread_create(NULL, 0,
		    (void (*)())_attach_drivers,
		    NULL, 0, &p0, TS_RUN, minclsyspri);
	}

}
#endif /* defined(USB_INIT_ENABLE) || defined(IO_FASTBOOT_ENABLE) */
