/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Displays plot files on an HP7221 plotter.
 * Cloned from bgplot.c and gigiplot.c by Jim Kleckner
 * Thu Jun 30 13:35:04 PDT 1983
 *  Requires a handshaking program such as hp7221cat to get
 *  the plotter open and ready.
 */

#include <signal.h>
#include "hp7221.h"

int currentx = 0;
int currenty = 0;
double lowx = 0.0;
double lowy = 0.0;
double scale = 1.0;

void
openpl(void)
{
	void closepl();

	/* catch interupts */
	signal(SIGINT, closepl);
	currentx = 0;
	currenty = 0;
	printf( "~VR~W" );
	putMBP( 800, 2000 );
	putMBP( 7600, 9600 );
	printf( "~S" );
	putMBP( XMAX, YMAX );
	printf( "vA~*z" );

	space(0,0,XMAX,YMAX);
}
