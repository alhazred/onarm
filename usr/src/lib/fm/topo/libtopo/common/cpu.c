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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)cpu.c	1.4	06/12/20 SMI"

#include <errno.h>
#include <limits.h>
#include <strings.h>
#include <unistd.h>
#include <topo_error.h>
#include <fm/topo_mod.h>
#include <sys/fm/protocol.h>

#include <topo_method.h>
#include <cpu.h>

/*
 * platform specific cpu module
 */
#define	PLATFORM_CPU_VERSION	CPU_VERSION
#define	PLATFORM_CPU_NAME	"platform-cpu"

static int cpu_enum(topo_mod_t *, tnode_t *, const char *, topo_instance_t,
    topo_instance_t, void *, void *);
static void cpu_release(topo_mod_t *, tnode_t *);
static int cpu_nvl2str(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);
static int cpu_str2nvl(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);
static int cpu_fmri_asru(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);
static nvlist_t *fmri_create(topo_mod_t *, uint32_t, uint8_t, char *);

static const topo_method_t cpu_methods[] = {
	{ TOPO_METH_NVL2STR, TOPO_METH_NVL2STR_DESC, TOPO_METH_NVL2STR_VERSION,
	    TOPO_STABILITY_INTERNAL, cpu_nvl2str },
	{ TOPO_METH_STR2NVL, TOPO_METH_STR2NVL_DESC, TOPO_METH_STR2NVL_VERSION,
	    TOPO_STABILITY_INTERNAL, cpu_str2nvl },
	{ TOPO_METH_ASRU_COMPUTE, TOPO_METH_ASRU_COMPUTE_DESC,
	    TOPO_METH_ASRU_COMPUTE_VERSION, TOPO_STABILITY_INTERNAL,
	    cpu_fmri_asru },
	{ TOPO_METH_FMRI, TOPO_METH_FMRI_DESC, TOPO_METH_FMRI_VERSION,
	    TOPO_STABILITY_INTERNAL, cpu_fmri_asru },
	{ NULL }
};

static const topo_modops_t cpu_ops =
	{ cpu_enum, cpu_release };

static const topo_modinfo_t cpu_info =
	{ "cpu", FM_FMRI_SCHEME_CPU, CPU_VERSION, &cpu_ops };

int
cpu_init(topo_mod_t *mod, topo_version_t version)
{
	cpu_node_t *cpuip;

	if (getenv("TOPOCPUDEBUG"))
		topo_mod_setdebug(mod);
	topo_mod_dprintf(mod, "initializing cpu builtin\n");

	if (version != CPU_VERSION)
		return (topo_mod_seterrno(mod, EMOD_VER_NEW));

	if ((cpuip = topo_mod_zalloc(mod, sizeof (cpu_node_t))) == NULL)
		return (topo_mod_seterrno(mod, EMOD_NOMEM));

	if ((cpuip->cn_kc = kstat_open()) == NULL) {
		topo_mod_dprintf(mod, "kstat_open failed: %s\n",
		    strerror(errno));
		topo_mod_free(mod, cpuip, sizeof (cpu_node_t));
		return (-1);
	}

	cpuip->cn_ncpustats = sysconf(_SC_CPUID_MAX);
	if ((cpuip->cn_cpustats = topo_mod_zalloc(mod, (
	    cpuip->cn_ncpustats + 1) * sizeof (kstat_t *))) == NULL) {
		(void) kstat_close(cpuip->cn_kc);
		topo_mod_free(mod, cpuip, sizeof (cpu_node_t));
		return (-1);
	}

	if (topo_mod_register(mod, &cpu_info, TOPO_VERSION) != 0) {
		topo_mod_dprintf(mod, "failed to register cpu_info: "
		    "%s\n", topo_mod_errmsg(mod));
		topo_mod_free(mod, cpuip->cn_cpustats,
		    (cpuip->cn_ncpustats + 1) * sizeof (kstat_t *));
		(void) kstat_close(cpuip->cn_kc);
		topo_mod_free(mod, cpuip, sizeof (cpu_node_t));
		return (-1);
	}

	topo_mod_setspecific(mod, (void *)cpuip);

	return (0);
}

void
cpu_fini(topo_mod_t *mod)
{
	cpu_node_t *cpuip;

	cpuip = topo_mod_getspecific(mod);

	if (cpuip->cn_cpustats != NULL)
		topo_mod_free(mod, cpuip->cn_cpustats,
		    (cpuip->cn_ncpustats + 1) * sizeof (kstat_t *));

	(void) kstat_close(cpuip->cn_kc);
	topo_mod_free(mod, cpuip, sizeof (cpu_node_t));

	topo_mod_unregister(mod);
}

static int
cpu_kstat_init(cpu_node_t *cpuip, int i)
{
	kstat_t *ksp;

	if (cpuip->cn_cpustats[i] == NULL) {
		if ((ksp = kstat_lookup(cpuip->cn_kc, "cpu_info", i, NULL)) ==
		    NULL || kstat_read(cpuip->cn_kc, ksp, NULL) < 0)
			return (-1);

		cpuip->cn_cpustats[i] = ksp;
	} else {
		ksp = cpuip->cn_cpustats[i];
	}

	return (ksp->ks_instance);
}

/*ARGSUSED*/
static int
cpu_create(topo_mod_t *mod, tnode_t *rnode, const char *name,
    topo_instance_t min, topo_instance_t max, cpu_node_t *cpuip)
{
	int i;
	processorid_t cpu_id;
	char *s, sbuf[21];
	kstat_named_t *ks;
	nvlist_t *fmri;

	for (i = 0; i <= cpuip->cn_ncpustats; i++) {

		if ((cpu_id = cpu_kstat_init(cpuip, i)) < 0)
			continue;

		if ((ks = kstat_data_lookup(cpuip->cn_cpustats[i],
		    "device_ID")) != NULL) {
			(void) snprintf(sbuf, 21, "%llX", ks->value.ui64);
			s = sbuf;
		} else {
			s = NULL;
		}

		if ((fmri = fmri_create(mod, cpu_id, 0, s)) == NULL)
			continue;
		(void) topo_node_bind(mod, rnode, name, cpu_id, fmri);
		nvlist_free(fmri);
	}

	return (0);
}


/*ARGSUSED*/
static int
cpu_enum(topo_mod_t *mod, tnode_t *pnode, const char *name,
    topo_instance_t min, topo_instance_t max, void *arg, void *notused2)
{
	topo_mod_t *nmp;
	cpu_node_t *cpuip = (cpu_node_t *)arg;

	if ((nmp = topo_mod_load(mod, PLATFORM_CPU_NAME,
				PLATFORM_CPU_VERSION)) == NULL) {
		if (topo_mod_errno(mod) == ETOPO_MOD_NOENT) {
			/*
			 * There is no platform specific cpu module, so use
			 * the default enumeration with kstats of this builtin
			 * cpu module.
			 */
			if (topo_node_range_create(mod, pnode, name, 0,
						cpuip->cn_ncpustats + 1) < 0) {
				topo_mod_dprintf(mod,
					"cpu enumeration failed to create "
					"cpu range [0-%d]: %s\n",
					cpuip->cn_ncpustats + 1,
					topo_mod_errmsg(mod));
				return (-1); /* mod_errno set */
			}
			(void) topo_method_register(mod, pnode, cpu_methods);
			return (cpu_create(mod, pnode, name, min, max, cpuip));

		} else {
			/* Fail to load the module */
			topo_mod_dprintf(mod,
					"Failed to load module %s: %s",
					PLATFORM_CPU_NAME,
					topo_mod_errmsg(mod));
			return (-1);
		}
	}

	if (topo_mod_enumerate(nmp, pnode, PLATFORM_CPU_NAME, name,
				min, max, NULL) < 0) {
		topo_mod_dprintf(mod,
				"%s failed to enumerate: %s",
				PLATFORM_CPU_NAME,
				topo_mod_errmsg(mod));
		return (-1);
	}
	(void) topo_method_register(mod, pnode, cpu_methods);

	return (0);
}

static void
cpu_release(topo_mod_t *mod, tnode_t *node)
{
	topo_method_unregister_all(mod, node);
}

ssize_t
fmri_nvl2str(nvlist_t *nvl, uint8_t version, char *buf, size_t buflen)
{
	int rc;
	uint32_t cpuid;
	uint64_t serint;
	char *serstr;

	if (version == CPU_SCHEME_VERSION0) {
		if (nvlist_lookup_uint32(nvl, FM_FMRI_CPU_ID, &cpuid) != 0 ||
		    nvlist_lookup_uint64(nvl, FM_FMRI_CPU_SERIAL_ID, &serint)
		    != 0)
			return (0);

		return (snprintf(buf, buflen, "cpu:///%s=%u/%s=%llX",
		    FM_FMRI_CPU_ID, cpuid, FM_FMRI_CPU_SERIAL_ID,
		    (u_longlong_t)serint));
	} else if (version == CPU_SCHEME_VERSION1) {
		if (nvlist_lookup_uint32(nvl, FM_FMRI_CPU_ID, &cpuid) != 0)
			return (0);

		/*
		 * Serial number is an optional element
		 */
		if ((rc = nvlist_lookup_string(nvl, FM_FMRI_CPU_SERIAL_ID,
		    &serstr)) != 0)
			if (rc == ENOENT)
				return (snprintf(buf, buflen, "cpu:///%s=%u",
				    FM_FMRI_CPU_ID, cpuid));
			else
				return (0);
		else
			return (snprintf(buf, buflen, "cpu:///%s=%u/%s=%s",
			    FM_FMRI_CPU_ID, cpuid, FM_FMRI_CPU_SERIAL_ID,
			    serstr));

	} else {
		return (0);
	}
}

/*ARGSUSED*/
static int
cpu_nvl2str(topo_mod_t *mod, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	uint8_t fver;
	ssize_t len;
	char *name;

	if (version > TOPO_METH_NVL2STR_VERSION)
		return (topo_mod_seterrno(mod, EMOD_VER_NEW));

	if (nvlist_lookup_uint8(in, FM_VERSION, &fver) != 0)
		return (topo_mod_seterrno(mod, EMOD_FMRI_VERSION));

	if ((len = fmri_nvl2str(in, fver, NULL, 0)) == 0 ||
	    (name = topo_mod_alloc(mod, len + 1)) == NULL ||
	    fmri_nvl2str(in, fver, name, len + 1) == 0)
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));

	if (topo_mod_nvalloc(mod, out, NV_UNIQUE_NAME) < 0) {
		topo_mod_free(mod, name, len + 1);
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));
	}

	if (nvlist_add_string(*out, "fmri-string", name) != 0) {
		topo_mod_free(mod, name, len + 1);
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));
	}
	topo_mod_free(mod, name, len + 1);

	return (0);
}

/*ARGSUSED*/
static int
cpu_str2nvl(topo_mod_t *mod, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	int err;
	ulong_t cpuid;
	char *str, *s, *end;
	char *serial = NULL;
	nvlist_t *fmri;

	if (version > TOPO_METH_STR2NVL_VERSION)
		return (topo_mod_seterrno(mod, EMOD_VER_NEW));

	if (nvlist_lookup_string(in, "fmri-string", &str) != 0)
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));

	/* We're expecting a string version of a cpu scheme FMRI */
	if (strncmp(str, "cpu:///", 7) != 0)
		return (topo_mod_seterrno(mod, EMOD_FMRI_MALFORM));

	s = strchr(str + 7, '=');
	if (s == NULL)
		return (topo_mod_seterrno(mod, EMOD_FMRI_MALFORM));

	++s;
	cpuid = strtoul(s, &end, 0);

	if (cpuid == ULONG_MAX && errno == ERANGE)
		return (topo_mod_seterrno(mod, EMOD_FMRI_MALFORM));

	if (*(s = end) == '/') {
		s = strchr(s, '=');
		++s;
		serial = s;
	}

	if (topo_mod_nvalloc(mod, &fmri, NV_UNIQUE_NAME) != 0)
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));

	err = nvlist_add_uint8(fmri, FM_VERSION, CPU_SCHEME_VERSION1);
	err |= nvlist_add_string(fmri, FM_FMRI_SCHEME, FM_FMRI_SCHEME_CPU);
	err |= nvlist_add_uint32(fmri, FM_FMRI_CPU_ID, (uint32_t)cpuid);
	err |= nvlist_add_uint8(fmri, FM_FMRI_CPU_MASK, 0);
	if (serial != NULL)
		err |= nvlist_add_string(fmri, FM_FMRI_CPU_SERIAL_ID,
		    serial);

	if (err != 0) {
		nvlist_free(fmri);
		return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));
	}
	*out = fmri;

	return (0);
}

static nvlist_t *
fmri_create(topo_mod_t *mod, uint32_t cpu_id, uint8_t cpumask, char *s)
{
	int err;
	nvlist_t *fmri;

	if (topo_mod_nvalloc(mod, &fmri, NV_UNIQUE_NAME) != 0) {
		(void) topo_mod_seterrno(mod, EMOD_FMRI_NVL);
		return (NULL);
	}

	err = nvlist_add_uint8(fmri, FM_VERSION, FM_CPU_SCHEME_VERSION);
	err |= nvlist_add_string(fmri, FM_FMRI_SCHEME, FM_FMRI_SCHEME_CPU);
	err |= nvlist_add_uint32(fmri, FM_FMRI_CPU_ID, cpu_id);
	err |= nvlist_add_uint8(fmri, FM_FMRI_CPU_MASK, cpumask);
	if (s != NULL)
		err |= nvlist_add_string(fmri, FM_FMRI_CPU_SERIAL_ID, s);
	if (err != 0) {
		nvlist_free(fmri);
		(void) topo_mod_seterrno(mod, EMOD_FMRI_NVL);
		return (NULL);
	}

	return (fmri);
}

/*ARGSUSED*/
static int
cpu_fmri_asru(topo_mod_t *mod, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	int rc;
	uint32_t cpu_id;
	uint8_t cpumask = 0;
	char *serial = NULL;

	if ((rc = nvlist_lookup_uint32(in, FM_FMRI_CPU_ID, &cpu_id)) != 0) {
		if (rc == ENOENT)
			return (topo_mod_seterrno(mod, EMOD_METHOD_INVAL));
		else
			return (topo_mod_seterrno(mod, EMOD_FMRI_NVL));
	}

	(void) nvlist_lookup_string(in, FM_FMRI_CPU_SERIAL_ID, &serial);
	(void) nvlist_lookup_uint8(in, FM_FMRI_CPU_MASK, &cpumask);

	*out = fmri_create(mod, cpu_id, cpumask, serial);

	return (0);
}