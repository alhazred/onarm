/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)import_err.h	1.2	06/09/27 SMI"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 *	Openvision retains the copyright to derivative works of
 *	this source code.  Do *NOT* create a derivative of this
 *	source code before consulting with your legal department.
 *	Do *NOT* integrate *ANY* of this source code into another
 *	product before consulting with your legal department.
 *
 *	For further information, read the top-level Openvision
 *	copyright which is contained in the top-level MIT Kerberos
 *	copyright.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 */

/*
 * import_err.h:
 * This file is automatically generated; please do not edit it.
 */

#include <com_err.h>

#define IMPORT_NO_ERR                            (37349888L)
#define IMPORT_BAD_FILE                          (37349889L)
#define IMPORT_BAD_TOKEN                         (37349890L)
#define IMPORT_BAD_VERSION                       (37349891L)
#define IMPORT_BAD_RECORD                        (37349892L)
#define IMPORT_BAD_FOOTER                        (37349893L)
#define IMPORT_FAILED                            (37349894L)
#define IMPORT_COUNT_MESSAGE                     (37349895L)
#define IMPORT_MISMATCH_COUNT                    (37349896L)
#define IMPORT_UNK_OPTION                        (37349897L)
#define IMPORT_WARN_DB                           (37349898L)
#define IMPORT_RENAME_FAILED                     (37349899L)
#define IMPORT_EXTRA_DATA                        (37349900L)
#define IMPORT_CONFIRM                           (37349901L)
#define IMPORT_OPEN_DUMP                         (37349902L)
#define IMPORT_IMPORT                            (37349903L)
#define IMPORT_TTY                               (37349904L)
#define IMPORT_RENAME_OPEN                       (37349905L)
#define IMPORT_RENAME_LOCK                       (37349906L)
#define IMPORT_RENAME_UNLOCK                     (37349907L)
#define IMPORT_RENAME_CLOSE                      (37349908L)
#define IMPORT_SINGLE_RECORD                     (37349909L)
#define IMPORT_PLURAL_RECORDS                    (37349910L)
#define IMPORT_GET_PARAMS                        (37349911L)
#define ERROR_TABLE_BASE_imp (37349888L)

extern const struct error_table et_imp_error_table;

#if !defined(_WIN32)
/* for compatibility with older versions... */
extern void initialize_imp_error_table (void) /*@modifies internalState@*/;
#else
#define initialize_imp_error_table()
#endif

#if !defined(_WIN32)
#define init_imp_err_tbl initialize_imp_error_table
#define imp_err_base ERROR_TABLE_BASE_imp
#endif