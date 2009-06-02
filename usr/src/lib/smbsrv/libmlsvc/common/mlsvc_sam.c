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

#pragma ident	"@(#)mlsvc_sam.c	1.4	08/03/03 SMI"

/*
 * Security Accounts Manager RPC (SAMR) interface definition.
 */

#include <strings.h>
#include <unistd.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>

#include <smbsrv/libsmb.h>
#include <smbsrv/libmlrpc.h>
#include <smbsrv/ntstatus.h>
#include <smbsrv/ntsid.h>
#include <smbsrv/smbinfo.h>
#include <smbsrv/nmpipes.h>
#include <smbsrv/libmlsvc.h>
#include <smbsrv/mlsvc_util.h>
#include <smbsrv/ndl/samrpc.ndl>
#include <smbsrv/samlib.h>

/*
 * The keys associated with the various handles dispensed by the SAMR
 * server.  These keys can be used to validate client activity.
 * These values are never passed over the wire so security shouldn't
 * be an issue.
 */
typedef enum {
	SAMR_KEY_NULL = 0,
	SAMR_KEY_CONNECT,
	SAMR_KEY_DOMAIN,
	SAMR_KEY_USER,
	SAMR_KEY_GROUP,
	SAMR_KEY_ALIAS
} samr_key_t;

typedef struct samr_keydata {
	samr_key_t kd_key;
	nt_domain_type_t kd_type;
	DWORD kd_rid;
} samr_keydata_t;

static ndr_hdid_t *samr_hdalloc(ndr_xa_t *, samr_key_t, nt_domain_type_t,
    DWORD);
static void samr_hdfree(ndr_xa_t *, ndr_hdid_t *);
static ndr_handle_t *samr_hdlookup(ndr_xa_t *, ndr_hdid_t *, samr_key_t);
static int samr_call_stub(struct mlrpc_xaction *mxa);
static DWORD samr_s_enum_local_domains(struct samr_EnumLocalDomain *,
    struct mlrpc_xaction *);

static mlrpc_stub_table_t samr_stub_table[];

static mlrpc_service_t samr_service = {
	"SAMR",				/* name */
	"Security Accounts Manager",	/* desc */
	"\\samr",			/* endpoint */
	PIPE_LSASS,			/* sec_addr_port */
	"12345778-1234-abcd-ef000123456789ac", 1,	/* abstract */
	"8a885d04-1ceb-11c9-9fe808002b104860", 2,	/* transfer */
	0,				/* no bind_instance_size */
	NULL,				/* no bind_req() */
	NULL,				/* no unbind_and_close() */
	samr_call_stub,			/* call_stub() */
	&TYPEINFO(samr_interface),	/* interface ti */
	samr_stub_table			/* stub_table */
};

/*
 * samr_initialize
 *
 * This function registers the SAM RPC interface with the RPC runtime
 * library. It must be called in order to use either the client side
 * or the server side functions.
 */
void
samr_initialize(void)
{
	(void) mlrpc_register_service(&samr_service);
}

/*
 * Custom call_stub to set the stream string policy.
 */
static int
samr_call_stub(struct mlrpc_xaction *mxa)
{
	MLNDS_SETF(&mxa->send_mlnds, MLNDS_F_NOTERM);
	MLNDS_SETF(&mxa->recv_mlnds, MLNDS_F_NOTERM);

	return (mlrpc_generic_call_stub(mxa));
}

/*
 * Handle allocation wrapper to setup the local context.
 */
static ndr_hdid_t *
samr_hdalloc(ndr_xa_t *mxa, samr_key_t key, nt_domain_type_t domain_type,
    DWORD rid)
{
	samr_keydata_t *data;

	if ((data = malloc(sizeof (samr_keydata_t))) == NULL)
		return (NULL);

	data->kd_key = key;
	data->kd_type = domain_type;
	data->kd_rid = rid;

	return (ndr_hdalloc(mxa, data));
}

/*
 * Handle deallocation wrapper to free the local context.
 */
static void
samr_hdfree(ndr_xa_t *mxa, ndr_hdid_t *id)
{
	ndr_handle_t *hd;

	if ((hd = ndr_hdlookup(mxa, id)) != NULL) {
		free(hd->nh_data);
		ndr_hdfree(mxa, id);
	}
}

/*
 * Handle lookup wrapper to validate the local context.
 */
static ndr_handle_t *
samr_hdlookup(ndr_xa_t *mxa, ndr_hdid_t *id, samr_key_t key)
{
	ndr_handle_t *hd;
	samr_keydata_t *data;

	if ((hd = ndr_hdlookup(mxa, id)) == NULL)
		return (NULL);

	if ((data = (samr_keydata_t *)hd->nh_data) == NULL)
		return (NULL);

	if (data->kd_key != key)
		return (NULL);

	return (hd);
}

/*
 * samr_s_ConnectAnon
 *
 * This is a request to connect to the local SAM database. We don't
 * support any form of update request and our database doesn't
 * contain any private information, so there is little point in
 * doing any access access checking here.
 *
 * Return a handle for use with subsequent SAM requests.
 */
static int
samr_s_ConnectAnon(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_ConnectAnon *param = arg;
	ndr_hdid_t *id;

	id = samr_hdalloc(mxa, SAMR_KEY_CONNECT, NT_DOMAIN_NULL, 0);
	if (id) {
		bcopy(id, &param->handle, sizeof (samr_handle_t));
		param->status = 0;
	} else {
		bzero(&param->handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_CloseHandle
 *
 * Close the SAM interface specified by the handle.
 * Free the handle and zero out the result handle for the client.
 */
static int
samr_s_CloseHandle(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_CloseHandle *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;

	samr_hdfree(mxa, id);

	bzero(&param->result_handle, sizeof (samr_handle_t));
	param->status = 0;
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_LookupDomain
 *
 * This is a request to map a domain name to a domain SID. We can map
 * the primary domain name, our local domain name (hostname) and the
 * builtin domain names to the appropriate SID. Anything else will be
 * rejected.
 */
static int
samr_s_LookupDomain(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_LookupDomain *param = arg;
	char resource_domain[SMB_PI_MAX_DOMAIN];
	char *domain_name;
	nt_sid_t *sid = NULL;

	if ((domain_name = (char *)param->domain_name.str) == NULL) {
		bzero(param, sizeof (struct samr_LookupDomain));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_PARAMETER);
		return (MLRPC_DRC_OK);
	}

	(void) smb_getdomainname(resource_domain, SMB_PI_MAX_DOMAIN);
	if (mlsvc_is_local_domain(domain_name) == 1) {
		sid = nt_sid_dup(nt_domain_local_sid());
	} else if (strcasecmp(resource_domain, domain_name) == 0) {
		/*
		 * We should not be asked to provide
		 * the domain SID for the primary domain.
		 */
		sid = NULL;
	} else {
		sid = nt_builtin_lookup_name(domain_name, 0);
	}

	if (sid) {
		param->sid = (struct samr_sid *)mlsvc_sid_save(sid, mxa);
		free(sid);

		if (param->sid == NULL) {
			bzero(param, sizeof (struct samr_LookupDomain));
			param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
			return (MLRPC_DRC_OK);
		}

		param->status = NT_STATUS_SUCCESS;
	} else {
		param->sid = NULL;
		param->status = NT_SC_ERROR(NT_STATUS_NO_SUCH_DOMAIN);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_EnumLocalDomains
 *
 * This is a request for the local domains supported by this server.
 * All we do here is validate the handle and set the status. The real
 * work is done in samr_s_enum_local_domains.
 */
static int
samr_s_EnumLocalDomains(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_EnumLocalDomain *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;
	DWORD status;

	if (samr_hdlookup(mxa, id, SAMR_KEY_CONNECT) == NULL)
		status = NT_STATUS_ACCESS_DENIED;
	else
		status = samr_s_enum_local_domains(param, mxa);

	if (status == NT_STATUS_SUCCESS) {
		param->enum_context = param->info->entries_read;
		param->total_entries = param->info->entries_read;
		param->status = NT_STATUS_SUCCESS;
	} else {
		bzero(param, sizeof (struct samr_EnumLocalDomain));
		param->status = NT_SC_ERROR(status);
	}

	return (MLRPC_DRC_OK);
}


/*
 * samr_s_enum_local_domains
 *
 * This function should only be called via samr_s_EnumLocalDomains to
 * ensure that the appropriate validation is performed. We will answer
 * queries about two domains: the local domain, synonymous with the
 * local hostname, and the BUILTIN domain. So we return these two
 * strings.
 *
 * Returns NT status values.
 */
static DWORD
samr_s_enum_local_domains(struct samr_EnumLocalDomain *param,
    struct mlrpc_xaction *mxa)
{
	struct samr_LocalDomainInfo *info;
	struct samr_LocalDomainEntry *entry;
	char *hostname;

	hostname = MLRPC_HEAP_MALLOC(mxa, MAXHOSTNAMELEN);
	if (hostname == NULL)
		return (NT_STATUS_NO_MEMORY);

	if (smb_gethostname(hostname, MAXHOSTNAMELEN, 1) != 0)
		return (NT_STATUS_NO_MEMORY);

	entry = MLRPC_HEAP_NEWN(mxa, struct samr_LocalDomainEntry, 2);
	if (entry == NULL)
		return (NT_STATUS_NO_MEMORY);

	bzero(entry, (sizeof (struct samr_LocalDomainEntry) * 2));
	(void) mlsvc_string_save((ms_string_t *)&entry[0].name, hostname, mxa);
	(void) mlsvc_string_save((ms_string_t *)&entry[1].name, "Builtin", mxa);

	info = MLRPC_HEAP_NEW(mxa, struct samr_LocalDomainInfo);
	if (info == NULL)
		return (NT_STATUS_NO_MEMORY);

	info->entries_read = 2;
	info->entry = entry;
	param->info = info;
	return (NT_STATUS_SUCCESS);
}

/*
 * samr_s_OpenDomain
 *
 * This is a request to open a domain within the local SAM database.
 * The caller must supply a valid connect handle.
 * We return a handle to be used to access objects within this domain.
 */
static int
samr_s_OpenDomain(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_OpenDomain *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;
	nt_domain_t *domain;

	if (samr_hdlookup(mxa, id, SAMR_KEY_CONNECT) == NULL) {
		bzero(&param->domain_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
		return (MLRPC_DRC_OK);
	}

	if ((domain = nt_domain_lookup_sid((nt_sid_t *)param->sid)) == NULL) {
		bzero(&param->domain_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_CANT_ACCESS_DOMAIN_INFO);
		return (MLRPC_DRC_OK);
	}

	if ((domain->type != NT_DOMAIN_BUILTIN) &&
	    (domain->type != NT_DOMAIN_LOCAL)) {
		bzero(&param->domain_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_CANT_ACCESS_DOMAIN_INFO);
		return (MLRPC_DRC_OK);
	}

	id = samr_hdalloc(mxa, SAMR_KEY_DOMAIN, domain->type, 0);
	if (id) {
		bcopy(id, &param->domain_handle, sizeof (samr_handle_t));
		param->status = 0;
	} else {
		bzero(&param->domain_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_QueryDomainInfo
 *
 * The caller should pass a domain handle.
 *
 * Windows 95 Server Manager sends requests for levels 6 and 7 when
 * the services menu item is selected. Level 2 is basically for getting
 * number of users, groups, and aliases in a domain.
 * We have no information on what the various information levels mean.
 */
static int
samr_s_QueryDomainInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_QueryDomainInfo *param = arg;
	struct samr_QueryDomainInfoRes *info;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->domain_handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;
	char *domain;
	char hostname[MAXHOSTNAMELEN];
	int alias_cnt;
	int rc;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL) {
		bzero(param, sizeof (struct samr_QueryDomainInfo));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	info = MLRPC_HEAP_NEW(mxa, struct samr_QueryDomainInfoRes);
	if (info == NULL) {
		bzero(param, sizeof (struct samr_QueryDomainInfo));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
		return (MLRPC_DRC_OK);
	}
	info->switch_value = param->info_level;
	param->info = info;

	data = (samr_keydata_t *)hd->nh_data;

	switch (data->kd_type) {
	case NT_DOMAIN_BUILTIN:
		domain = "BUILTIN";
		break;

	case NT_DOMAIN_LOCAL:
		rc = smb_gethostname(hostname, MAXHOSTNAMELEN, 1);
		if (rc != 0) {
			bzero(param, sizeof (struct samr_QueryDomainInfo));
			param->status = NT_SC_ERROR(NT_STATUS_INTERNAL_ERROR);
			return (MLRPC_DRC_OK);
		}
		domain = hostname;
		break;

	default:
		bzero(param, sizeof (struct samr_QueryDomainInfo));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	switch (param->info_level) {
	case SAMR_QUERY_DOMAIN_INFO_6:
		info->ru.info6.unknown1 = 0x00000000;
		info->ru.info6.unknown2 = 0x00147FB0;
		info->ru.info6.unknown3 = 0x00000000;
		info->ru.info6.unknown4 = 0x00000000;
		info->ru.info6.unknown5 = 0x00000000;
		param->status = NT_STATUS_SUCCESS;
		break;

	case SAMR_QUERY_DOMAIN_INFO_7:
		info->ru.info7.unknown1 = 0x00000003;
		param->status = NT_STATUS_SUCCESS;
		break;

	case SAMR_QUERY_DOMAIN_INFO_2:
		rc = (data->kd_type == NT_DOMAIN_BUILTIN)
		    ? smb_lgrp_numbydomain(SMB_LGRP_BUILTIN, &alias_cnt)
		    : smb_lgrp_numbydomain(SMB_LGRP_LOCAL, &alias_cnt);
		if (rc != SMB_LGRP_SUCCESS) {
			bzero(param, sizeof (struct samr_QueryDomainInfo));
			param->status = NT_SC_ERROR(NT_STATUS_INTERNAL_ERROR);
			return (MLRPC_DRC_OK);
		}

		info->ru.info2.unknown1 = 0x00000000;
		info->ru.info2.unknown2 = 0x80000000;

		(void) mlsvc_string_save((ms_string_t *)&(info->ru.info2.s1),
		    "", mxa);

		(void) mlsvc_string_save(
		    (ms_string_t *)&(info->ru.info2.domain), domain, mxa);

		(void) mlsvc_string_save((ms_string_t *)&(info->ru.info2.s2),
		    "", mxa);

		info->ru.info2.sequence_num = 0x0000002B;
		info->ru.info2.unknown3 = 0x00000000;
		info->ru.info2.unknown4 = 0x00000001;
		info->ru.info2.unknown5 = 0x00000003;
		info->ru.info2.unknown6 = 0x00000001;
		info->ru.info2.num_users = 0;
		info->ru.info2.num_groups = 0;
		info->ru.info2.num_aliases = alias_cnt;
		param->status = NT_STATUS_SUCCESS;
		break;

	default:
		bzero(param, sizeof (struct samr_QueryDomainInfo));
		return (MLRPC_DRC_FAULT_REQUEST_OPNUM_INVALID);
	};

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_LookupNames
 *
 * The definition for this interface is obviously wrong but I can't
 * seem to get it to work the way I think it should. It should
 * support multiple name lookup but I can only get one working for now.
 */
static int
samr_s_LookupNames(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_LookupNames *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;
	well_known_account_t *wka;
	smb_group_t grp;
	smb_passwd_t smbpw;
	nt_sid_t *sid;
	uint32_t status = NT_STATUS_SUCCESS;
	int rc;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL)
		status = NT_STATUS_INVALID_HANDLE;

	if (param->n_entry != 1)
		status = NT_STATUS_ACCESS_DENIED;

	if (param->name.str == NULL) {
		/*
		 * Windows NT returns NT_STATUS_NONE_MAPPED.
		 * Windows 2000 returns STATUS_INVALID_ACCOUNT_NAME.
		 */
		status = NT_STATUS_NONE_MAPPED;
	}

	if (status != NT_STATUS_SUCCESS) {
		bzero(param, sizeof (struct samr_LookupNames));
		param->status = NT_SC_ERROR(status);
		return (MLRPC_DRC_OK);
	}

	param->rids.rid = MLRPC_HEAP_NEW(mxa, DWORD);
	param->rid_types.rid_type = MLRPC_HEAP_NEW(mxa, DWORD);

	data = (samr_keydata_t *)hd->nh_data;

	switch (data->kd_type) {
	case NT_DOMAIN_BUILTIN:
		wka = nt_builtin_lookup((char *)param->name.str);
		if (wka != NULL) {
			param->rids.n_entry = 1;
			(void) nt_sid_get_rid(wka->binsid, &param->rids.rid[0]);
			param->rid_types.n_entry = 1;
			param->rid_types.rid_type[0] = wka->sid_name_use;
			param->status = NT_STATUS_SUCCESS;
			return (MLRPC_DRC_OK);
		}
		break;

	case NT_DOMAIN_LOCAL:
		rc = smb_lgrp_getbyname((char *)param->name.str, &grp);
		if (rc == SMB_LGRP_SUCCESS) {
			param->rids.n_entry = 1;
			param->rids.rid[0] = grp.sg_rid;
			param->rid_types.n_entry = 1;
			param->rid_types.rid_type[0] = grp.sg_id.gs_type;
			param->status = NT_STATUS_SUCCESS;
			smb_lgrp_free(&grp);
			return (MLRPC_DRC_OK);
		}

		if (smb_pwd_getpasswd((const char *)param->name.str,
		    &smbpw) != NULL) {
			if (smb_idmap_getsid(smbpw.pw_uid, SMB_IDMAP_USER,
			    &sid) == IDMAP_SUCCESS) {
				param->rids.n_entry = 1;
				(void) nt_sid_get_rid(sid, &param->rids.rid[0]);
				param->rid_types.n_entry = 1;
				param->rid_types.rid_type[0] = SidTypeUser;
				param->status = NT_STATUS_SUCCESS;
				free(sid);
				return (MLRPC_DRC_OK);
			}
		}
		break;

	default:
		bzero(param, sizeof (struct samr_LookupNames));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	param->rids.n_entry = 0;
	param->rid_types.n_entry = 0;
	param->status = NT_SC_ERROR(NT_STATUS_NONE_MAPPED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_OpenUser
 *
 * This is a request to open a user within a specified domain in the
 * local SAM database. The caller must supply a valid domain handle,
 * obtained via a successful domain open request. The user is
 * specified by the rid in the request.
 */
static int
samr_s_OpenUser(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_OpenUser *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL) {
		bzero(&param->user_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	data = (samr_keydata_t *)hd->nh_data;

	id = samr_hdalloc(mxa, SAMR_KEY_USER, data->kd_type, param->rid);
	if (id == NULL) {
		bzero(&param->user_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
	} else {
		bcopy(id, &param->user_handle, sizeof (samr_handle_t));
		/*
		 * Need QueryUserInfo(level 21).
		 */
		samr_hdfree(mxa, id);
		bzero(&param->user_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_DeleteUser
 *
 * Request to delete a user within a specified domain in the local
 * SAM database.  The caller should supply a valid user handle.
 */
/*ARGSUSED*/
static int
samr_s_DeleteUser(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_DeleteUser *param = arg;

	bzero(param, sizeof (struct samr_DeleteUser));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_QueryUserInfo
 *
 * The caller should provide a valid user key.
 *
 * Returns:
 * NT_STATUS_SUCCESS
 * NT_STATUS_ACCESS_DENIED
 * NT_STATUS_INVALID_INFO_CLASS
 */
/*ARGSUSED*/
static int
samr_s_QueryUserInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_QueryUserInfo *param = arg;

	bzero(param, sizeof (struct samr_QueryUserInfo));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_QueryUserGroups
 *
 * Request the list of groups of which a user is a member.
 * The user is identified from the handle, which contains an
 * rid in the discriminator field. Note that this is a local user.
 */
static int
samr_s_QueryUserGroups(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_QueryUserGroups *param = arg;
	struct samr_UserGroupInfo *info;
	struct samr_UserGroups *group;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->user_handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;
	well_known_account_t *wka;
	nt_sid_t *user_sid = NULL;
	nt_sid_t *dom_sid;
	smb_group_t grp;
	smb_giter_t gi;
	uint32_t status;
	int size;
	int ngrp_max;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_USER)) == NULL) {
		status = NT_STATUS_ACCESS_DENIED;
		goto query_error;
	}

	data = (samr_keydata_t *)hd->nh_data;
	switch (data->kd_type) {
	case NT_DOMAIN_BUILTIN:
		wka = nt_builtin_lookup("builtin");
		if (wka == NULL) {
			status = NT_STATUS_INTERNAL_ERROR;
			goto query_error;
		}
		dom_sid = wka->binsid;
		break;
	case NT_DOMAIN_LOCAL:
		dom_sid = nt_domain_local_sid();
		break;
	default:
		status = NT_STATUS_INVALID_HANDLE;
		goto query_error;
	}

	user_sid = nt_sid_splice(dom_sid, data->kd_rid);
	if (user_sid == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto query_error;
	}

	info = MLRPC_HEAP_NEW(mxa, struct samr_UserGroupInfo);
	if (info == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto query_error;
	}
	bzero(info, sizeof (struct samr_UserGroupInfo));

	size = 32 * 1024;
	info->groups = MLRPC_HEAP_MALLOC(mxa, size);
	if (info->groups == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto query_error;
	}
	ngrp_max = size / sizeof (struct samr_UserGroups);

	if (smb_lgrp_iteropen(&gi) != SMB_LGRP_SUCCESS) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto query_error;
	}

	info->n_entry = 0;
	group = info->groups;
	while ((info->n_entry < ngrp_max) &&
	    (smb_lgrp_iterate(&gi, &grp) == SMB_LGRP_SUCCESS)) {
		if (smb_lgrp_is_member(&grp, user_sid)) {
			group->rid = grp.sg_rid;
			group->attr = grp.sg_attr;
			group++;
			info->n_entry++;
		}
		smb_lgrp_free(&grp);
	}
	smb_lgrp_iterclose(&gi);

	free(user_sid);
	param->info = info;
	param->status = NT_STATUS_SUCCESS;
	return (MLRPC_DRC_OK);

query_error:
	free(user_sid);
	bzero(param, sizeof (struct samr_QueryUserGroups));
	param->status = NT_SC_ERROR(status);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_OpenGroup
 *
 * This is a request to open a group within the specified domain in the
 * local SAM database. The caller must supply a valid domain handle,
 * obtained via a successful domain open request. The group is
 * specified by the rid in the request. If this is a local RID it
 * should already be encoded with type information.
 *
 * We return a handle to be used to access information about this group.
 */
static int
samr_s_OpenGroup(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_OpenGroup *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL) {
		bzero(&param->group_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	data = (samr_keydata_t *)hd->nh_data;
	id = samr_hdalloc(mxa, SAMR_KEY_GROUP, data->kd_type, param->rid);

	if (id) {
		bcopy(id, &param->group_handle, sizeof (samr_handle_t));
		param->status = 0;
	} else {
		bzero(&param->group_handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_Connect
 *
 * This is a request to connect to the local SAM database.
 * We don't support any form of update request and our database doesn't
 * contain any private information, so there is little point in doing
 * any access access checking here.
 *
 * Return a handle for use with subsequent SAM requests.
 */
static int
samr_s_Connect(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_Connect *param = arg;
	ndr_hdid_t *id;

	id = samr_hdalloc(mxa, SAMR_KEY_CONNECT, NT_DOMAIN_NULL, 0);
	if (id) {
		bcopy(id, &param->handle, sizeof (samr_handle_t));
		param->status = 0;
	} else {
		bzero(&param->handle, sizeof (samr_handle_t));
		param->status = NT_SC_ERROR(NT_STATUS_NO_MEMORY);
	}

	return (MLRPC_DRC_OK);
}

/*
 * samr_s_GetUserPwInfo
 *
 * This is a request to get a user's password.
 */
/*ARGSUSED*/
static int
samr_s_GetUserPwInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_GetUserPwInfo *param = arg;

	bzero(param, sizeof (struct samr_GetUserPwInfo));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_CreateUser
 */
/*ARGSUSED*/
static int
samr_s_CreateUser(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_CreateUser *param = arg;

	bzero(&param->user_handle, sizeof (samr_handle_t));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_ChangeUserPasswd
 */
/*ARGSUSED*/
static int
samr_s_ChangeUserPasswd(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_ChangeUserPasswd *param = arg;

	bzero(param, sizeof (struct samr_ChangeUserPasswd));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_GetDomainPwInfo
 */
/*ARGSUSED*/
static int
samr_s_GetDomainPwInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_GetDomainPwInfo *param = arg;

	bzero(param, sizeof (struct samr_GetDomainPwInfo));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_SetUserInfo
 */
/*ARGSUSED*/
static int
samr_s_SetUserInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_SetUserInfo *param = arg;

	bzero(param, sizeof (struct samr_SetUserInfo));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_QueryDispInfo
 *
 * This function is supposed to return local user information.
 * As we don't support local users, this function dosen't send
 * back any information.
 *
 * Added template that returns information for Administrator and Guest
 * builtin users.  All information is hard-coded from packet captures.
 */
static int
samr_s_QueryDispInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_QueryDispInfo *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->domain_handle;
	DWORD status = 0;

	if (samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN) == NULL) {
		bzero(param, sizeof (struct samr_QueryDispInfo));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

#ifdef SAMR_SUPPORT_USER
	if ((desc->discrim != SAMR_LOCAL_DOMAIN) || (param->start_idx != 0)) {
		param->total_size = 0;
		param->returned_size = 0;
		param->switch_value = 1;
		param->count = 0;
		param->users = 0;
	} else {
		param->total_size = 328;
		param->returned_size = 328;
		param->switch_value = 1;
		param->count = 2;
		param->users = (struct user_disp_info *)MLRPC_HEAP_MALLOC(mxa,
		    sizeof (struct user_disp_info));

		param->users->count = 2;
		param->users->acct[0].index = 1;
		param->users->acct[0].rid = 500;
		param->users->acct[0].ctrl = 0x210;

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->users->acct[0].name,
		    "Administrator", mxa);

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->users->acct[0].fullname,
		    "Built-in account for administering the computer/domain",
		    mxa);

		bzero(&param->users->acct[0].desc, sizeof (samr_string_t));

		param->users->acct[1].index = 2;
		param->users->acct[1].rid = 501;
		param->users->acct[1].ctrl = 0x211;

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->users->acct[1].name,
		    "Guest", mxa);

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->users->acct[1].fullname,
		    "Built-in account for guest access to the computer/domain",
		    mxa);

		bzero(&param->users->acct[1].desc, sizeof (samr_string_t));
	}
#else
	param->total_size = 0;
	param->returned_size = 0;
	param->switch_value = 1;
	param->count = 0;
	param->users = 0;
#endif
	param->status = status;
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_EnumDomainGroups
 *
 *
 * This function is supposed to return local group information.
 * As we don't support local users, this function dosen't send
 * back any information.
 *
 * Added template that returns information for a domain group as None.
 * All information is hard-coded from packet captures.
 */
static int
samr_s_EnumDomainGroups(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_EnumDomainGroups *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->domain_handle;
	DWORD status = NT_STATUS_SUCCESS;

	if (samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN) == NULL)
		status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);

	param->total_size = 0;
	param->returned_size = 0;
	param->switch_value = 3;
	param->count = 0;
	param->groups = 0;
	param->status = status;
	return (MLRPC_DRC_OK);

#ifdef SAMR_SUPPORT_GROUPS
	if ((desc->discrim != SAMR_LOCAL_DOMAIN) || (param->start_idx != 0)) {
		param->total_size = 0;
		param->returned_size = 0;
		param->switch_value = 3;
		param->count = 0;
		param->groups = 0;
	} else {
		param->total_size = 64;
		param->returned_size = 64;
		param->switch_value = 3;
		param->count = 1;
		param->groups = (struct group_disp_info *)MLRPC_HEAP_MALLOC(
		    mxa, sizeof (struct group_disp_info));

		param->groups->count = 1;
		param->groups->acct[0].index = 1;
		param->groups->acct[0].rid = 513;
		param->groups->acct[0].ctrl = 0x7;
		(void) mlsvc_string_save(
		    (ms_string_t *)&param->groups->acct[0].name, "None", mxa);

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->groups->acct[0].desc,
		    "Ordinary users", mxa);
	}

	param->status = NT_STATUS_SUCCESS;
	return (MLRPC_DRC_OK);
#endif
}

/*
 * samr_s_OpenAlias
 *
 * Lookup for requested alias, if it exists return a handle
 * for that alias. The alias domain sid should match with
 * the passed domain handle.
 */
static int
samr_s_OpenAlias(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_OpenAlias *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->domain_handle;
	ndr_handle_t *hd;
	uint32_t status;
	samr_keydata_t *data;
	int rc;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL) {
		status = NT_STATUS_INVALID_HANDLE;
		goto open_alias_err;
	}

	if (param->access_mask != SAMR_ALIAS_ACCESS_GET_INFO) {
		status = NT_STATUS_ACCESS_DENIED;
		goto open_alias_err;
	}

	data = (samr_keydata_t *)hd->nh_data;
	rc = smb_lgrp_getbyrid(param->rid, (smb_gdomain_t)data->kd_type, NULL);
	if (rc != SMB_LGRP_SUCCESS) {
		status = NT_STATUS_NO_SUCH_ALIAS;
		goto open_alias_err;
	}

	id = samr_hdalloc(mxa, SAMR_KEY_ALIAS, data->kd_type, param->rid);
	if (id) {
		bcopy(id, &param->alias_handle, sizeof (samr_handle_t));
		param->status = NT_STATUS_SUCCESS;
		return (MLRPC_DRC_OK);
	}

	status = NT_STATUS_NO_MEMORY;

open_alias_err:
	bzero(&param->alias_handle, sizeof (samr_handle_t));
	param->status = NT_SC_ERROR(status);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_CreateDomainAlias
 *
 * Creates a local group in the security database, which is the
 * security accounts manager (SAM)
 * For more information you can look at MSDN page for NetLocalGroupAdd.
 * This RPC is used by CMC and right now it returns access denied.
 * The peice of code that creates a local group doesn't get compiled.
 */
/*ARGSUSED*/
static int
samr_s_CreateDomainAlias(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_CreateDomainAlias *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->alias_handle;

	if (samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN) == NULL) {
		bzero(param, sizeof (struct samr_CreateDomainAlias));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	bzero(param, sizeof (struct samr_CreateDomainAlias));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);

#ifdef SAMR_SUPPORT_ADD_ALIAS
	DWORD status = NT_STATUS_SUCCESS;
	nt_group_t *grp;
	char *alias_name;

	alias_name = param->alias_name.str;
	if (alias_name == 0) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto create_alias_err;
	}

	/*
	 * Check access mask.  User should be member of
	 * Administrators or Account Operators local group.
	 */
	status = nt_group_add(alias_name, 0,
	    NT_GROUP_AF_ADD | NT_GROUP_AF_LOCAL);

	if (status != NT_STATUS_SUCCESS)
		goto create_alias_err;

	grp = nt_group_getinfo(alias_name, RWLOCK_READER);
	if (grp == NULL) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto create_alias_err;
	}

	(void) nt_sid_get_rid(grp->sid, &param->rid);
	nt_group_putinfo(grp);
	handle = mlsvc_get_handle(MLSVC_IFSPEC_SAMR, SAMR_ALIAS_KEY,
	    param->rid);
	bcopy(handle, &param->alias_handle, sizeof (samr_handle_t));

	param->status = 0;
	return (MLRPC_DRC_OK);

create_alias_err:
	bzero(&param->alias_handle, sizeof (samr_handle_t));
	param->status = NT_SC_ERROR(status);
	return (MLRPC_DRC_OK);
#endif
}

/*
 * samr_s_SetAliasInfo
 *
 * Similar to NetLocalGroupSetInfo.
 */
static int
samr_s_SetAliasInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_SetAliasInfo *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->alias_handle;
	DWORD status = NT_STATUS_SUCCESS;

	if (samr_hdlookup(mxa, id, SAMR_KEY_ALIAS) == NULL)
		status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);

	param->status = status;
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_QueryAliasInfo
 *
 * Retrieves information about the specified local group account
 * by given handle.
 */
static int
samr_s_QueryAliasInfo(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_QueryAliasInfo *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->alias_handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;
	smb_group_t grp;
	uint32_t status;
	int rc;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_ALIAS)) == NULL) {
		status = NT_STATUS_INVALID_HANDLE;
		goto query_alias_err;
	}

	data = (samr_keydata_t *)hd->nh_data;
	rc = smb_lgrp_getbyrid(data->kd_rid, (smb_gdomain_t)data->kd_type,
	    &grp);
	if (rc != SMB_LGRP_SUCCESS) {
		status = NT_STATUS_NO_SUCH_ALIAS;
		goto query_alias_err;
	}

	switch (param->level) {
	case SAMR_QUERY_ALIAS_INFO_1:
		param->ru.info1.level = param->level;
		(void) mlsvc_string_save(
		    (ms_string_t *)&param->ru.info1.name, grp.sg_name, mxa);

		(void) mlsvc_string_save(
		    (ms_string_t *)&param->ru.info1.desc, grp.sg_cmnt, mxa);

		param->ru.info1.unknown = 1;
		break;

	case SAMR_QUERY_ALIAS_INFO_3:
		param->ru.info3.level = param->level;
		(void) mlsvc_string_save(
		    (ms_string_t *)&param->ru.info3.desc, grp.sg_cmnt, mxa);
		break;

	default:
		smb_lgrp_free(&grp);
		status = NT_STATUS_INVALID_INFO_CLASS;
		goto query_alias_err;
	};

	smb_lgrp_free(&grp);
	param->address = (DWORD)(uintptr_t)&param->ru;
	param->status = 0;
	return (MLRPC_DRC_OK);

query_alias_err:
	param->status = NT_SC_ERROR(status);
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_DeleteDomainAlias
 *
 * Deletes a local group account and all its members from the
 * security database, which is the security accounts manager (SAM) database.
 * Only members of the Administrators or Account Operators local group can
 * execute this function.
 * For more information you can look at MSDN page for NetLocalGroupSetInfo.
 *
 * This RPC is used by CMC and right now it returns access denied.
 * The peice of code that removes a local group doesn't get compiled.
 */
static int
samr_s_DeleteDomainAlias(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_DeleteDomainAlias *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->alias_handle;

	if (samr_hdlookup(mxa, id, SAMR_KEY_ALIAS) == NULL) {
		bzero(param, sizeof (struct samr_DeleteDomainAlias));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	bzero(param, sizeof (struct samr_DeleteDomainAlias));
	param->status = NT_SC_ERROR(NT_STATUS_ACCESS_DENIED);
	return (MLRPC_DRC_OK);

#ifdef SAMR_SUPPORT_DEL_ALIAS
	nt_group_t *grp;
	char *alias_name;
	DWORD status;

	grp = nt_groups_lookup_rid(desc->discrim);
	if (grp == 0) {
		status = NT_STATUS_NO_SUCH_ALIAS;
		goto delete_alias_err;
	}

	alias_name = strdup(grp->name);
	if (alias_name == 0) {
		status = NT_STATUS_NO_MEMORY;
		goto delete_alias_err;
	}

	status = nt_group_delete(alias_name);
	free(alias_name);
	if (status != NT_STATUS_SUCCESS)
		goto delete_alias_err;

	param->status = 0;
	return (MLRPC_DRC_OK);

delete_alias_err:
	param->status = NT_SC_ERROR(status);
	return (MLRPC_DRC_OK);
#endif
}

/*
 * samr_s_EnumDomainAliases
 *
 * This function sends back a list which contains all local groups' name.
 */
static int
samr_s_EnumDomainAliases(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_EnumDomainAliases *param = arg;
	ndr_hdid_t *id = (ndr_hdid_t *)&param->domain_handle;
	ndr_handle_t *hd;
	samr_keydata_t *data;
	smb_group_t grp;
	smb_giter_t gi;
	int cnt, skip, i;
	struct name_rid *info;

	if ((hd = samr_hdlookup(mxa, id, SAMR_KEY_DOMAIN)) == NULL) {
		bzero(param, sizeof (struct samr_EnumDomainAliases));
		param->status = NT_SC_ERROR(NT_STATUS_INVALID_HANDLE);
		return (MLRPC_DRC_OK);
	}

	data = (samr_keydata_t *)hd->nh_data;

	(void) smb_lgrp_numbydomain((smb_gdomain_t)data->kd_type, &cnt);
	if (cnt <= param->resume_handle) {
		param->aliases = (struct aliases_info *)MLRPC_HEAP_MALLOC(mxa,
		    sizeof (struct aliases_info));

		bzero(param->aliases, sizeof (struct aliases_info));
		param->out_resume = 0;
		param->entries = 0;
		param->status = NT_STATUS_SUCCESS;
		return (MLRPC_DRC_OK);
	}

	cnt -= param->resume_handle;
	param->aliases = (struct aliases_info *)MLRPC_HEAP_MALLOC(mxa,
	    sizeof (struct aliases_info) + (cnt-1) * sizeof (struct name_rid));

	if (smb_lgrp_iteropen(&gi) != SMB_LGRP_SUCCESS) {
		bzero(param, sizeof (struct samr_EnumDomainAliases));
		param->status = NT_SC_ERROR(NT_STATUS_INTERNAL_ERROR);
		return (MLRPC_DRC_OK);
	}

	skip = i = 0;
	info = param->aliases->info;
	while (smb_lgrp_iterate(&gi, &grp) == SMB_LGRP_SUCCESS) {
		if ((skip++ >= param->resume_handle) &&
		    (grp.sg_domain == data->kd_type) && (i++ < cnt)) {
			info->rid = grp.sg_rid;
			(void) mlsvc_string_save((ms_string_t *)&info->name,
			    grp.sg_name, mxa);

			info++;
		}
		smb_lgrp_free(&grp);
	}
	smb_lgrp_iterclose(&gi);

	param->aliases->count = i;
	param->aliases->address = i;

	param->out_resume = i;
	param->entries = i;
	param->status = 0;
	return (MLRPC_DRC_OK);
}

/*
 * samr_s_Connect3
 *
 * This is the connect3 form of the  connect request. It contains an
 * extra parameter over samr_Connect. See samr_s_Connect for other
 * details. NT returns an RPC fault - so we can do the same for now.
 * Doing it this way should avoid the unsupported opnum error message
 * appearing in the log.
 */
/*ARGSUSED*/
static int
samr_s_Connect3(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_Connect3 *param = arg;

	bzero(param, sizeof (struct samr_Connect3));
	return (MLRPC_DRC_FAULT_REQUEST_OPNUM_INVALID);
}


/*
 * samr_s_Connect4
 *
 * This is the connect4 form of the connect request used by Windows XP.
 * Returns an RPC fault for now.
 */
/*ARGSUSED*/
static int
samr_s_Connect4(void *arg, struct mlrpc_xaction *mxa)
{
	struct samr_Connect4 *param = arg;

	bzero(param, sizeof (struct samr_Connect4));
	return (MLRPC_DRC_FAULT_REQUEST_OPNUM_INVALID);
}

static mlrpc_stub_table_t samr_stub_table[] = {
	{ samr_s_ConnectAnon,		SAMR_OPNUM_ConnectAnon },
	{ samr_s_CloseHandle,		SAMR_OPNUM_CloseHandle },
	{ samr_s_LookupDomain,		SAMR_OPNUM_LookupDomain },
	{ samr_s_EnumLocalDomains,	SAMR_OPNUM_EnumLocalDomains },
	{ samr_s_OpenDomain,		SAMR_OPNUM_OpenDomain },
	{ samr_s_QueryDomainInfo,	SAMR_OPNUM_QueryDomainInfo },
	{ samr_s_LookupNames,		SAMR_OPNUM_LookupNames },
	{ samr_s_OpenUser,		SAMR_OPNUM_OpenUser },
	{ samr_s_DeleteUser,		SAMR_OPNUM_DeleteUser },
	{ samr_s_QueryUserInfo,		SAMR_OPNUM_QueryUserInfo },
	{ samr_s_QueryUserGroups,	SAMR_OPNUM_QueryUserGroups },
	{ samr_s_OpenGroup,		SAMR_OPNUM_OpenGroup },
	{ samr_s_Connect,		SAMR_OPNUM_Connect },
	{ samr_s_GetUserPwInfo,		SAMR_OPNUM_GetUserPwInfo },
	{ samr_s_CreateUser,		SAMR_OPNUM_CreateUser },
	{ samr_s_ChangeUserPasswd,	SAMR_OPNUM_ChangeUserPasswd },
	{ samr_s_GetDomainPwInfo,	SAMR_OPNUM_GetDomainPwInfo },
	{ samr_s_SetUserInfo,		SAMR_OPNUM_SetUserInfo },
	{ samr_s_Connect3,		SAMR_OPNUM_Connect3 },
	{ samr_s_Connect4,		SAMR_OPNUM_Connect4 },
	{ samr_s_QueryDispInfo,		SAMR_OPNUM_QueryDispInfo },
	{ samr_s_OpenAlias,		SAMR_OPNUM_OpenAlias },
	{ samr_s_CreateDomainAlias,	SAMR_OPNUM_CreateDomainAlias },
	{ samr_s_SetAliasInfo,		SAMR_OPNUM_SetAliasInfo },
	{ samr_s_QueryAliasInfo,	SAMR_OPNUM_QueryAliasInfo },
	{ samr_s_DeleteDomainAlias,	SAMR_OPNUM_DeleteDomainAlias },
	{ samr_s_EnumDomainAliases,	SAMR_OPNUM_EnumDomainAliases },
	{ samr_s_EnumDomainGroups,	SAMR_OPNUM_EnumDomainGroups },
	{0}
};

/*
 * There is a bug in the way that midl and the marshalling code handles
 * unions so we need to fix some of the data offsets at runtime. The
 * following macros and the fixup functions handle the corrections.
 */

DECL_FIXUP_STRUCT(samr_QueryAliasInfo_ru);
DECL_FIXUP_STRUCT(samr_QueryAliasInfoRes);
DECL_FIXUP_STRUCT(samr_QueryAliasInfo);

DECL_FIXUP_STRUCT(QueryUserInfo_result_u);
DECL_FIXUP_STRUCT(QueryUserInfo_result);
DECL_FIXUP_STRUCT(samr_QueryUserInfo);

void
fixup_samr_QueryAliasInfo(struct samr_QueryAliasInfo *val)
{
	unsigned short size1 = 0;
	unsigned short size2 = 0;
	unsigned short size3 = 0;

	switch (val->level) {
		CASE_INFO_ENT(samr_QueryAliasInfo, 1);
		CASE_INFO_ENT(samr_QueryAliasInfo, 3);

		default:
			return;
	};

	size2 = size1 + (2 * sizeof (DWORD));
	size3 = size2 + sizeof (mlrpcconn_request_hdr_t) + sizeof (DWORD);

	FIXUP_PDU_SIZE(samr_QueryAliasInfo_ru, size1);
	FIXUP_PDU_SIZE(samr_QueryAliasInfoRes, size2);
	FIXUP_PDU_SIZE(samr_QueryAliasInfo, size3);
}

void
fixup_samr_QueryUserInfo(struct samr_QueryUserInfo *val)
{
	unsigned short size1 = 0;
	unsigned short size2 = 0;
	unsigned short size3 = 0;

	switch (val->switch_index) {
		CASE_INFO_ENT(samr_QueryUserInfo, 1);
		CASE_INFO_ENT(samr_QueryUserInfo, 6);
		CASE_INFO_ENT(samr_QueryUserInfo, 7);
		CASE_INFO_ENT(samr_QueryUserInfo, 8);
		CASE_INFO_ENT(samr_QueryUserInfo, 9);
		CASE_INFO_ENT(samr_QueryUserInfo, 16);

		default:
			return;
	};

	size2 = size1 + (2 * sizeof (DWORD));
	size3 = size2 + sizeof (mlrpcconn_request_hdr_t) + sizeof (DWORD);

	FIXUP_PDU_SIZE(QueryUserInfo_result_u, size1);
	FIXUP_PDU_SIZE(QueryUserInfo_result, size2);
	FIXUP_PDU_SIZE(samr_QueryUserInfo, size3);
}

/*
 * As long as there is only one entry in the union, there is no need
 * to patch anything.
 */
/*ARGSUSED*/
void
fixup_samr_QueryGroupInfo(struct samr_QueryGroupInfo *val)
{
}