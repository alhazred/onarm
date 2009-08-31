/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * lib/kad5/kadm_host_srv_names.c
 */

#include "admin.h"
#include <stdio.h>
#include <os-proto.h>

#define	KADM5_MASTER "admin_server"

/*
 * Find the admin server for the given realm. If the realm is null or
 * the empty string, find the admin server for the default realm.
 * Returns 0 on succsess (KADM5_OK). It is the callers responsibility to
 * free the storage allocated to the admin server, master.
 */
kadm5_ret_t
kadm5_get_master(krb5_context context, const char *realm, char **master)
{
	char *def_realm;
	char *delim;
#ifdef KRB5_DNS_LOOKUP
	struct sockaddr *addrs;
	int naddrs;
	unsigned short dns_portno;
	char dns_host[MAX_DNS_NAMELEN];
	krb5_data dns_realm;
	krb5_error_code dns_ret = 1;
#endif /* KRB5_DNS_LOOKUP */

	if (realm == 0 || *realm == '\0')
		krb5_get_default_realm(context, &def_realm);

	(void) profile_get_string(context->profile, "realms",
				realm ? realm : def_realm,
				KADM5_MASTER, 0, master);

	if ((*master != NULL) && ((delim = strchr(*master, ':')) != NULL))
		*delim = '\0';
#ifdef KRB5_DNS_LOOKUP
	if (*master == NULL) {
		/*
		 * Initialize realm info for (possible) DNS lookups.
		 */
		dns_realm.data = strdup(realm ? realm : def_realm);
		dns_realm.length = strlen(realm ? realm : def_realm);
		dns_realm.magic = 0;

		dns_ret = krb5_get_servername(context, &dns_realm,
				"_kerberos-adm", "_udp",
				dns_host, &dns_portno);
		if (dns_ret == 0)
			*master = strdup(dns_host);

		if (dns_realm.data)
			free(dns_realm.data);
	}
#endif /* KRB5_DNS_LOOKUP */
	return (*master ? KADM5_OK : KADM5_NO_SRV);
}

/*
 * Get the host base service name for the admin principal. Returns
 * KADM5_OK on success. Caller must free the storage allocated for
 * host_service_name.
 */
kadm5_ret_t
kadm5_get_adm_host_srv_name(krb5_context context,
			    const char *realm, char **host_service_name)
{
	kadm5_ret_t ret;
	char *name;
	char *host;


	if (ret = kadm5_get_master(context, realm, &host))
		return (ret);

	name = malloc(strlen(KADM5_ADMIN_HOST_SERVICE)+ strlen(host) + 2);
	if (name == NULL) {
		free(host);
		return (ENOMEM);
	}
	sprintf(name, "%s@%s", KADM5_ADMIN_HOST_SERVICE, host);
	free(host);
	*host_service_name = name;

	return (KADM5_OK);
}

/*
 * Get the host base service name for the changepw principal. Returns
 * KADM5_OK on success. Caller must free the storage allocated for
 * host_service_name.
 */
kadm5_ret_t
kadm5_get_cpw_host_srv_name(krb5_context context,
			    const char *realm, char **host_service_name)
{
	kadm5_ret_t ret;
	char *name;
	char *host;


	if (ret = kadm5_get_master(context, realm, &host))
		return (ret);

	name = malloc(strlen(KADM5_CHANGEPW_HOST_SERVICE) + strlen(host) + 2);
	if (name == NULL) {
		free(host);
		return (ENOMEM);
	}
	sprintf(name, "%s@%s", KADM5_CHANGEPW_HOST_SERVICE, host);
	free(host);
	*host_service_name = name;

	return (KADM5_OK);
}

/*
 * Get the host base service name for the kiprop principal. Returns
 * KADM5_OK on success. Caller must free the storage allocated
 * for host_service_name.
 */
kadm5_ret_t kadm5_get_kiprop_host_srv_name(krb5_context context,
				    const char *realm,
				    char **host_service_name) {
	kadm5_ret_t ret;
	char *name;
	char *host;


	if (ret = kadm5_get_master(context, realm, &host))
		return (ret);

	name = malloc(strlen(KADM5_KIPROP_HOST_SERVICE) + strlen(host) + 2);
	if (name == NULL) {
		free(host);
		return (ENOMEM);
	}
	sprintf(name, "%s@%s", KADM5_KIPROP_HOST_SERVICE, host);
	free(host);
	*host_service_name = name;

	return (KADM5_OK);
}
