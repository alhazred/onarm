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

#ifndef	_VSCAN_H
#define	_VSCAN_H

#pragma ident	"@(#)vscan.h	1.2	08/01/28 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <sys/vnode.h>

/*
 * vscan.h provides definitions for vscan kernel module
 */

#define	VS_DRV_MAX_FILES	1024	/* max concurent file scans */
#define	VS_DRV_PATH		"/devices/pseudo/vscan@0:vscan"
#define	VS_DRV_IOCTL_ENABLE	0x0001	/* door rendezvous */
#define	VS_DRV_IOCTL_DISABLE	0x0002	/* vscand shutting down */
#define	VS_DRV_IOCTL_CONFIG	0x0004	/* vscand config data update */

/* Scan Result - vsr_result */
#define	VS_STATUS_UNDEFINED	0
#define	VS_STATUS_NO_SCAN	1 /* scan not required */
#define	VS_STATUS_ERROR		2 /* scan failed */
#define	VS_STATUS_CLEAN		3 /* scan successful, file clean */
#define	VS_STATUS_INFECTED	4 /* scan successful, file infected */

#define	VS_TYPES_LEN		4096	/* vs_config_t - types buffer */

/*
 * AV_SCANSTAMP_SZ is the size of the scanstamp stored in the
 * filesystem. vs_scanstamp_t is 1 character longer to allow
 * a null terminated string to be used within vscan
 */
typedef char vs_scanstamp_t[AV_SCANSTAMP_SZ + 1];

/* used for both request to and response from vscand */
typedef struct vs_scan_req {
	uint32_t vsr_id;
	uint32_t vsr_flags;
	uint64_t vsr_size;
	uint8_t vsr_modified;
	uint8_t vsr_quarantined;
	char vsr_path[MAXPATHLEN];
	vs_scanstamp_t vsr_scanstamp;
	uint32_t vsr_result;
} vs_scan_req_t;


/* passed in VS_DRV_IOCTL_CONFIG */
typedef struct vs_config {
	char vsc_types[VS_TYPES_LEN];
	uint64_t vsc_types_len;
	uint64_t vsc_max_size;	/* files > max size (bytes) not scan */
	uint64_t vsc_allow;	/* allow access to file exceeding max_size? */
} vs_config_t;


#ifdef _KERNEL

/*
 * max no of types in vs_config_t.vsc_types
 * used as dimention for array of pointers to types
 */
#define	VS_TYPES_MAX		VS_TYPES_LEN / 2

/*
 * seconds to wait for daemon to reconnect before unregistering from VFS
 * during this time, the kernel will:
 * - allow access to files that have not been modified since last scanned
 * - deny access to files which have been modified since last scanned
 */
#define	VS_DAEMON_WAIT_SEC	60

/* access derived from scan result (VS_STATUS_XXX) and file attributes */
#define	VS_ACCESS_UNDEFINED	0
#define	VS_ACCESS_ALLOW		1
#define	VS_ACCESS_DENY		2

int vscan_svc_init(void);
void vscan_svc_fini(void);
void vscan_svc_enable(void);
void vscan_svc_disable(void);
int vscan_svc_configure(vs_config_t *);
boolean_t vscan_svc_is_enabled(void);
boolean_t vscan_svc_in_use(void);
vnode_t *vscan_svc_get_vnode(int);

int vscan_door_init(void);
void vscan_door_fini(void);
int vscan_door_open(int);
void vscan_door_close(void);
int vscan_door_scan_file(vs_scan_req_t *);

boolean_t vscan_drv_create_node(int);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif


#endif /* _VSCAN_H */
