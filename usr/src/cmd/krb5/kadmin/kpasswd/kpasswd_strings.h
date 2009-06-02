#pragma ident	"@(#)kpasswd_strings.h	1.2	06/09/27 SMI"

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

#include <com_err.h>

/*
 * kpasswd_strings.h:
 * This file is automatically generated; please do not edit it.
 */
#define KPW_STR_USAGE                            (-1767084800L)
#define KPW_STR_PRIN_UNKNOWN                     (-1767084799L)
#define KPW_STR_WHILE_LOOKING_AT_CC              (-1767084798L)
#define KPW_STR_OLD_PASSWORD_INCORRECT           (-1767084797L)
#define KPW_STR_CANT_OPEN_ADMIN_SERVER           (-1767084796L)
#define KPW_STR_NEW_PASSWORD_MISMATCH            (-1767084795L)
#define KPW_STR_PASSWORD_CHANGED                 (-1767084794L)
#define KPW_STR_PASSWORD_NOT_CHANGED             (-1767084793L)
#define KPW_STR_PARSE_NAME                       (-1767084792L)
#define KPW_STR_UNPARSE_NAME                     (-1767084791L)
#define KPW_STR_NOT_IN_PASSWD_FILE               (-1767084790L)
#define KPW_STR_CHANGING_PW_FOR                  (-1767084789L)
#define KPW_STR_OLD_PASSWORD_PROMPT              (-1767084788L)
#define KPW_STR_WHILE_READING_PASSWORD           (-1767084787L)
#define KPW_STR_NO_PASSWORD_READ                 (-1767084786L)
#define KPW_STR_WHILE_TRYING_TO_CHANGE           (-1767084785L)
#define KPW_STR_WHILE_DESTROYING_ADMIN_SESSION   (-1767084784L)
#define KPW_STR_WHILE_FREEING_PRINCIPAL          (-1767084783L)
#define KPW_STR_WHILE_FREEING_POLICY             (-1767084782L)
#define KPW_STR_CANT_GET_POLICY_INFO             (-1767084781L)
#define KPW_STR_POLICY_EXPLANATION               (-1767084780L)
#define ERROR_TABLE_BASE_kpws (-1767084800L)

extern const struct error_table et_kpws_error_table;

#if !defined(_WIN32)
/* for compatibility with older versions... */
extern void initialize_kpws_error_table (void) /*@modifies internalState@*/;
#else
#define initialize_kpws_error_table()
#endif

#if !defined(_WIN32)
#define init_kpws_err_tbl initialize_kpws_error_table
#define kpws_err_base ERROR_TABLE_BASE_kpws
#endif
