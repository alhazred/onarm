/*
 * Copyright 1998 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)ch_malloc.h	1.3	07/03/16 SMI"

char * ch_malloc( unsigned long size );
char * ch_realloc( char *block, unsigned long size );
char * ch_calloc( unsigned long nelem, unsigned long size );
char * ch_strdup( char *s1 );
void ch_free(void *ptr);
