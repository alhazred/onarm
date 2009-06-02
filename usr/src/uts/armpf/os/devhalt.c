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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)devhalt.c"

#include <sys/prom_debug.h>
#include <sys/debug.h>
#include <sys/devhalt.h>

typedef struct devhalt {
	struct devhalt	*dh_next; 	/* next on list */
	boolean_t	(*dh_func)();	/* devhalt function */
} devhalt_t;

typedef struct devhalt_table {
	kmutex_t        dht_lock;		/* protect all devhalt states */
	devhalt_t	*dht_first;	        /* ptr to 1st devhalt_t */
} devhalt_table_t;

static devhalt_table_t devhalt_table;

#if !defined(DEVHALT_ENTRY_MAX)
#define DEVHALT_ENTRY_MAX     5
#endif
static devhalt_t 	dh_entry[DEVHALT_ENTRY_MAX];


/*
 * Init all devhalt tables.
 */
void
devhalt_init()
{
	bzero(dh_entry, sizeof(dh_entry));
	bzero(&devhalt_table, sizeof(devhalt_table_t));
	mutex_init(&devhalt_table.dht_lock, NULL, MUTEX_DEFAULT, NULL);
	return;
}

/*
 * devhalt_add() is called to register func() be called later.
 */
devhalt_id_t
devhalt_add(boolean_t (*func)(void))
{
	int i;
	devhalt_t *p, *tmp;

	ASSERT(func >= (boolean_t (*)())KERNELBASE);

	if(func == (boolean_t (*)())NULL) {
		return((devhalt_id_t)0);
	}
    
	mutex_enter(&devhalt_table.dht_lock);
	for ( i = 0, p = dh_entry; i < DEVHALT_ENTRY_MAX; i++, p++ ) {
		if ( p->dh_func == (devhalt_id_t)func ) {
			mutex_exit(&devhalt_table.dht_lock);
			return((devhalt_id_t)0);
		}
	}

	for ( i = 0, p = dh_entry; i < DEVHALT_ENTRY_MAX; i++, p++ ) {
		if ( p->dh_func == NULL ) {
			p->dh_func = func;
 			tmp = devhalt_table.dht_first;
			devhalt_table.dht_first = p;
			p->dh_next = tmp;
			mutex_exit(&devhalt_table.dht_lock);
			return((devhalt_id_t)func);
		}
	}
	mutex_exit(&devhalt_table.dht_lock);
	return((devhalt_id_t)0);
}

/*
 * devhalt_remove() is called to remove registered func() from devhalt_table.
 */
devhalt_id_t
devhalt_remove(devhalt_id_t dh)
{
	devhalt_t *p, **pp;

	ASSERT(dh >= (devhalt_id_t)KERNELBASE);

	mutex_enter(&devhalt_table.dht_lock);
	pp = &devhalt_table.dht_first;
	p  =  devhalt_table.dht_first;

	while ( p != NULL ) {
		if ( (devhalt_id_t)p->dh_func == dh ) {
			*pp = p->dh_next;
			bzero(p, sizeof(devhalt_t));
			mutex_exit(&devhalt_table.dht_lock);
			return(dh);
		}
		pp = &p->dh_next;
		p = p->dh_next;
	}
	mutex_exit(&devhalt_table.dht_lock);
	return((devhalt_id_t)0);
}

/*
 * devhalt_execute() sequencialy runs registered devhalt functions.
 */
void
devhalt_execute(void)
{
	devhalt_t *p;

	/* we assume all other part of kernel had stopped. */
	p = devhalt_table.dht_first;
	while ( p != NULL && p->dh_func != NULL ) {
		(*p->dh_func)();
		p = p->dh_next;
	}
	return;
}
