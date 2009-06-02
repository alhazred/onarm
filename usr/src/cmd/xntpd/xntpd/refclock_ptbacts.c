/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)refclock_ptbacts.c	1.2	99/08/11 SMI"

/*
 * crude hack to avoid hard links in distribution
 * and keep only one ACTS type source for different
 * ACTS refclocks
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(PTBACTS)
# define KEEPPTBACTS
# include "refclock_acts.c"
#else /* not (REFCLOCK && PTBACTS) */
int refclock_ptbacts_bs;
#endif /* not (REFCLOCK && PTBACTS) */
