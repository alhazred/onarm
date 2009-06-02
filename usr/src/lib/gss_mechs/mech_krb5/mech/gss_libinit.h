#pragma ident	"@(#)gss_libinit.h	1.1	07/08/06 SMI"

#ifndef GSSAPI_LIBINIT_H
#define GSSAPI_LIBINIT_H

#include "gssapi.h"

OM_uint32 gssint_initialize_library (void);
void gssint_cleanup_library (void);

#endif /* GSSAPI_LIBINIT_H */
