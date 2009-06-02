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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)device_stubs.c"

#include <sys/mutex.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/list.h>
#include <sys/contract.h>
#include <sys/contract_impl.h>
#include <sys/contract/device.h>
#include <sys/contract/device_impl.h>
#include <sys/cmn_err.h>
#include <sys/ddi_impldefs.h>
#include <sys/systm.h>
#include <sys/stat.h>

/*
 * Stub routines for Device Contracts
 */

/* barrier routines */
static void ct_barrier_acquire(dev_info_t *dip);
static void ct_barrier_release(dev_info_t *dip);
static void ct_barrier_wait_for_release(dev_info_t *dip);

/*
 * State transition table showing which transitions are synchronous and which
 * are not.
 */
struct ct_dev_negtable {
	uint_t	st_old;
	uint_t	st_new;
	uint_t	st_neg;
} ct_dev_negtable[] = {
	{CT_DEV_EV_ONLINE, CT_DEV_EV_OFFLINE,	1},
	{CT_DEV_EV_ONLINE, CT_DEV_EV_DEGRADED,	0},
	{CT_DEV_EV_DEGRADED, CT_DEV_EV_ONLINE,	0},
	{CT_DEV_EV_DEGRADED, CT_DEV_EV_OFFLINE,	1},
	{0}
};

/*
 * Routine that determines if a particular CT_DEV_EV_? event corresponds to a
 * synchronous state change or not.
 */
static int
is_sync_neg(uint_t old, uint_t new)
{
	int	i;

	ASSERT(old & CT_DEV_ALLEVENT);
	ASSERT(new & CT_DEV_ALLEVENT);

	if (old == new) {
		CT_DEBUG((CE_WARN, "is_sync_neg: transition to same state: %d",
		    new));
		return (-2);
	}

	for (i = 0; ct_dev_negtable[i].st_new != 0; i++) {
		if (old == ct_dev_negtable[i].st_old &&
		    new == ct_dev_negtable[i].st_new) {
			return (ct_dev_negtable[i].st_neg);
		}
	}

	CT_DEBUG((CE_WARN, "is_sync_neg: Unsupported state transition: "
	    "old = %d -> new = %d", old, new));

	return (-1);
}

/*
 * Called when a device is successfully opened to create an open-time contract
 * i.e. synchronously with a device open.
 */
int
contract_device_open(dev_t dev, int spec_type, contract_t **ctpp)
{
	if (ctpp)
		*ctpp = NULL;
	return (0);
}

/*
 * Determines the current state of a device (i.e a devinfo node
 */
static int
get_state(dev_info_t *dip)
{
	if (DEVI_IS_DEVICE_OFFLINE(dip) || DEVI_IS_DEVICE_DOWN(dip))
		return (CT_DEV_EV_OFFLINE);
	else if (DEVI_IS_DEVICE_DEGRADED(dip))
		return (CT_DEV_EV_DEGRADED);
	else
		return (CT_DEV_EV_ONLINE);
}

/*
 * Core routine called by event-specific routines when an event occurs.
 * Determines if an event should be be published, and if it is to be
 * published, whether a negotiation should take place. Also implements
 * NEGEND events which publish the final disposition of an event after
 * negotiations are complete.
 */
static uint_t
contract_device_publish(dev_info_t *dip, dev_t dev, int spec_type,
    uint_t evtype)
{
	uint_t result = CT_NONE;
	int sync = 0;

	ASSERT(dip);
	ASSERT(dev != NODEV && dev != DDI_DEV_T_NONE);
	ASSERT((dev == DDI_DEV_T_ANY && spec_type == 0) ||
	    (spec_type == S_IFBLK || spec_type == S_IFCHR));
	ASSERT(evtype == 0 || (evtype & CT_DEV_ALLEVENT));

	/* Is this a synchronous state change ? */
	if (evtype != CT_EV_NEGEND) {
		sync = is_sync_neg(get_state(dip), evtype);
		/* NOP if unsupported transition */
		if (sync == -2 || sync == -1) {
			DEVI(dip)->devi_flags |= DEVI_CT_NOP;
			result = (sync == -2) ? CT_ACK : CT_NONE;
			goto out;
		}
		CT_DEBUG((CE_NOTE, "publish: is%s sync state change",
		    sync ? "" : " not"));
	} else if (DEVI(dip)->devi_flags & DEVI_CT_NOP) {
		DEVI(dip)->devi_flags &= ~DEVI_CT_NOP;
		result = CT_ACK;
		goto out;
	}

	mutex_enter(&(DEVI(dip)->devi_ct_lock));

	/*
	 * If this device didn't go through negotiation, don't publish
	 * a NEGEND event - simply release the barrier to allow other
	 * device events in.
	 */
	if (evtype == CT_EV_NEGEND) {
		ASSERT(list_is_empty(&(DEVI(dip)->devi_ct)));
		ASSERT(DEVI(dip)->devi_ct_neg == 0);
		CT_DEBUG((CE_NOTE, "publish: no negend reqd. release barrier"));
		ct_barrier_release(dip);
		mutex_exit(&(DEVI(dip)->devi_ct_lock));
		result = CT_ACK;
		goto out;
	} else {
		/*
		 * This is a new event, not a NEGEND event. Wait for previous
		 * contract events to complete.
		 */
		ct_barrier_acquire(dip);
	}

	ASSERT(list_is_empty(&(DEVI(dip)->devi_ct)));
	ASSERT(DEVI(dip)->devi_ct_neg == 0);

	/* No matching contracts */
	CT_DEBUG((CE_NOTE, "publish: No matching contract"));
	result = CT_NONE;

	mutex_exit(&(DEVI(dip)->devi_ct_lock));

out:
	CT_DEBUG((CE_NOTE, "publish: result = %d", result));
	return (result);
}

/*
 * contract_device_offline
 *
 * Event publishing routine called by I/O framework when a device is offlined.
 */
ct_ack_t
contract_device_offline(dev_info_t *dip, dev_t dev, int spec_type)
{
	uint_t result;
	uint_t evtype;

	evtype = CT_DEV_EV_OFFLINE;
	result = contract_device_publish(dip, dev, spec_type, evtype);

	return (result);
}

/*
 * contract_device_degrade
 *
 * Event publishing routine called by I/O framework when a device
 * moves to degrade state.
 */
/*ARGSUSED*/
void
contract_device_degrade(dev_info_t *dip, dev_t dev, int spec_type)
{
	uint_t evtype;

	evtype = CT_DEV_EV_DEGRADED;
	(void) contract_device_publish(dip, dev, spec_type, evtype);
}

/*
 * contract_device_undegrade
 *
 * Event publishing routine called by I/O framework when a device
 * moves from degraded state to online state.
 */
/*ARGSUSED*/
void
contract_device_undegrade(dev_info_t *dip, dev_t dev, int spec_type)
{
	uint_t evtype;

	evtype = CT_DEV_EV_ONLINE;
	(void) contract_device_publish(dip, dev, spec_type, evtype);
}

/*
 * For all contracts which have undergone a negotiation (because the device
 * moved out of the acceptable state for that contract and the state
 * change is synchronous i.e. requires negotiation) this routine publishes
 * a CT_EV_NEGEND event with the final disposition of the event.
 *
 * This event is always a critical event.
 */
void
contract_device_negend(dev_info_t *dip, dev_t dev, int spec_type, int result)
{
	uint_t evtype;

	ASSERT(result == CT_EV_SUCCESS || result == CT_EV_FAILURE);

	CT_DEBUG((CE_NOTE, "contract_device_negend(): entered: result: %d, "
	    "dip: %p", result, (void *)dip));

	evtype = CT_EV_NEGEND;
	(void) contract_device_publish(dip, dev, spec_type, evtype);

	CT_DEBUG((CE_NOTE, "contract_device_negend(): exit dip: %p",
	    (void *)dip));
}

/*
 * Wrapper routine called by other subsystems (such as LDI) to start
 * negotiations when a synchronous device state change occurs.
 * Returns CT_ACK or CT_NACK.
 */
ct_ack_t
contract_device_negotiate(dev_info_t *dip, dev_t dev, int spec_type,
    uint_t evtype)
{
	int	result;

	ASSERT(dip);
	ASSERT(dev != NODEV);
	ASSERT(dev != DDI_DEV_T_ANY);
	ASSERT(dev != DDI_DEV_T_NONE);
	ASSERT(spec_type == S_IFBLK || spec_type == S_IFCHR);

	switch (evtype) {
	case CT_DEV_EV_OFFLINE:
		result = contract_device_offline(dip, dev, spec_type);
		break;
	default:
		cmn_err(CE_PANIC, "contract_device_negotiate(): Negotiation "
		    "not supported: event (%d) for dev_t (%lu) and spec (%d), "
		    "dip (%p)", evtype, dev, spec_type, (void *)dip);
		result = CT_NACK;
		break;
	}

	return (result);
}

/*
 * A wrapper routine called by other subsystems (such as the LDI) to
 * finalize event processing for a state change event. For synchronous
 * state changes, this publishes NEGEND events. For asynchronous i.e.
 * non-negotiable events this publishes the event.
 */
void
contract_device_finalize(dev_info_t *dip, dev_t dev, int spec_type,
    uint_t evtype, int ct_result)
{
	ASSERT(dip);
	ASSERT(dev != NODEV);
	ASSERT(dev != DDI_DEV_T_ANY);
	ASSERT(dev != DDI_DEV_T_NONE);
	ASSERT(spec_type == S_IFBLK || spec_type == S_IFCHR);

	switch (evtype) {
	case CT_DEV_EV_OFFLINE:
		contract_device_negend(dip, dev, spec_type, ct_result);
		break;
	case CT_DEV_EV_DEGRADED:
		contract_device_degrade(dip, dev, spec_type);
		contract_device_negend(dip, dev, spec_type, ct_result);
		break;
	case CT_DEV_EV_ONLINE:
		contract_device_undegrade(dip, dev, spec_type);
		contract_device_negend(dip, dev, spec_type, ct_result);
		break;
	default:
		cmn_err(CE_PANIC, "contract_device_finalize(): Unsupported "
		    "event (%d) for dev_t (%lu) and spec (%d), dip (%p)",
		    evtype, dev, spec_type, (void *)dip);
		break;
	}
}

/*
 * Called by I/O framework when a devinfo node is freed to remove the
 * association between a devinfo node and its contracts.
 */
void
contract_device_remove_dip(dev_info_t *dip)
{
	mutex_enter(&(DEVI(dip)->devi_ct_lock));
	ct_barrier_wait_for_release(dip);

	ASSERT(list_is_empty(&(DEVI(dip)->devi_ct)));
	mutex_exit(&(DEVI(dip)->devi_ct_lock));
}

/*
 * Barrier related routines
 */
static void
ct_barrier_acquire(dev_info_t *dip)
{
	ASSERT(MUTEX_HELD(&(DEVI(dip)->devi_ct_lock)));
	CT_DEBUG((CE_NOTE, "ct_barrier_acquire: waiting for barrier"));
	while (DEVI(dip)->devi_ct_count != -1)
		cv_wait(&(DEVI(dip)->devi_ct_cv), &(DEVI(dip)->devi_ct_lock));
	DEVI(dip)->devi_ct_count = 0;
	CT_DEBUG((CE_NOTE, "ct_barrier_acquire: thread owns barrier"));
}

static void
ct_barrier_release(dev_info_t *dip)
{
	ASSERT(MUTEX_HELD(&(DEVI(dip)->devi_ct_lock)));
	ASSERT(DEVI(dip)->devi_ct_count != -1);
	DEVI(dip)->devi_ct_count = -1;
	cv_broadcast(&(DEVI(dip)->devi_ct_cv));
	CT_DEBUG((CE_NOTE, "ct_barrier_release: Released barrier"));
}

static void
ct_barrier_wait_for_release(dev_info_t *dip)
{
	ASSERT(MUTEX_HELD(&(DEVI(dip)->devi_ct_lock)));
	while (DEVI(dip)->devi_ct_count != -1)
		cv_wait(&(DEVI(dip)->devi_ct_cv), &(DEVI(dip)->devi_ct_lock));
}
