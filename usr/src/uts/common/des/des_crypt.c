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
 *
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 2006 NEC Corporation
 */

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * des_crypt.c, DES encryption library routines
 */

#include <sys/errno.h>
#include <sys/modctl.h>

#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/crypto/common.h>
#include <sys/crypto/spi.h>
#include <sys/sysmacros.h>
#include <sys/strsun.h>
#include <sys/note.h>
#include <des_impl.h>
#include <des_cbc_crypt.h>

/* EXPORT DELETE START */
#include <sys/types.h>
#include <rpc/des_crypt.h>
#include <des/des.h>

#ifdef sun_hardware
#include <sys/ioctl.h>
#ifdef _KERNEL
#include <sys/conf.h>
static int g_desfd = -1;
#define	getdesfd()	(cdevsw[11].d_open(0, 0) ? -1 : 0)
#define	ioctl(a, b, c)	(cdevsw[11].d_ioctl(0, b, c, 0) ? -1 : 0)
#else
#define	getdesfd()	(open("/dev/des", 0, 0))
#endif	/* _KERNEL */
#endif	/* sun */

static int common_crypt(char *key, char *buf, size_t len,
    unsigned int mode, struct desparams *desp);

extern int _des_crypt(char *buf, size_t len, struct desparams *desp);

/* EXPORT DELETE END */

extern struct mod_ops mod_cryptoops;

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops,
	"des encryption",
};

static struct modlcrypto modlcrypto = {
	&mod_cryptoops,
	"DES Kernel SW Provider"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlmisc,
	&modlcrypto,
	NULL
};

/*
 * CSPI information (entry points, provider info, etc.)
 */
typedef enum des_mech_type {
	DES_ECB_MECH_INFO_TYPE,		/* SUN_CKM_DES_ECB */
	DES_CBC_MECH_INFO_TYPE,		/* SUN_CKM_DES_CBC */
	DES_CFB_MECH_INFO_TYPE,		/* SUN_CKM_DES_CFB */
	DES3_ECB_MECH_INFO_TYPE,	/* SUN_CKM_DES3_ECB */
	DES3_CBC_MECH_INFO_TYPE,	/* SUN_CKM_DES3_CBC */
	DES3_CFB_MECH_INFO_TYPE		/* SUN_CKM_DES3_CFB */
} des_mech_type_t;

/* EXPORT DELETE START */

#define	DES_MIN_KEY_LEN		DES_MINBYTES
#define	DES_MAX_KEY_LEN		DES_MAXBYTES
#define	DES3_MIN_KEY_LEN	DES3_MINBYTES
#define	DES3_MAX_KEY_LEN	DES3_MAXBYTES

/* EXPORT DELETE END */

#ifndef DES_MIN_KEY_LEN
#define	DES_MIN_KEY_LEN		0
#endif

#ifndef DES_MAX_KEY_LEN
#define	DES_MAX_KEY_LEN		0
#endif

#ifndef DES3_MIN_KEY_LEN
#define	DES3_MIN_KEY_LEN	0
#endif

#ifndef DES3_MAX_KEY_LEN
#define	DES3_MAX_KEY_LEN	0
#endif

/*
 * Mechanism info structure passed to KCF during registration.
 */
static crypto_mech_info_t des_mech_info_tab[] = {
	/* DES_ECB */
	{SUN_CKM_DES_ECB, DES_ECB_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    DES_MIN_KEY_LEN, DES_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* DES_CBC */
	{SUN_CKM_DES_CBC, DES_CBC_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    DES_MIN_KEY_LEN, DES_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* DES3_ECB */
	{SUN_CKM_DES3_ECB, DES3_ECB_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    DES3_MIN_KEY_LEN, DES3_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BYTES},
	/* DES3_CBC */
	{SUN_CKM_DES3_CBC, DES3_CBC_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC,
	    DES3_MIN_KEY_LEN, DES3_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BYTES}
};

/* operations are in-place if the output buffer is NULL */
#define	DES_ARG_INPLACE(input, output)				\
	if ((output) == NULL)					\
		(output) = (input);

static void des_provider_status(crypto_provider_handle_t, uint_t *);

static crypto_control_ops_t des_control_ops = {
	des_provider_status
};

static int
des_common_init(crypto_ctx_t *, crypto_mechanism_t *, crypto_key_t *,
    crypto_spi_ctx_template_t, crypto_req_handle_t);
static int des_common_init_ctx(des_ctx_t *, crypto_spi_ctx_template_t *,
    crypto_mechanism_t *, crypto_key_t *, des_strength_t, int);
static int des_encrypt_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int des_decrypt_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);

static int des_encrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int des_encrypt_update(crypto_ctx_t *, crypto_data_t *,
    crypto_data_t *, crypto_req_handle_t);
static int des_encrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);

static int des_decrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int des_decrypt_update(crypto_ctx_t *, crypto_data_t *,
    crypto_data_t *, crypto_req_handle_t);
static int des_decrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);

static crypto_cipher_ops_t des_cipher_ops = {
	des_common_init,
	des_encrypt,
	des_encrypt_update,
	des_encrypt_final,
	des_encrypt_atomic,
	des_common_init,
	des_decrypt,
	des_decrypt_update,
	des_decrypt_final,
	des_decrypt_atomic
};

static int des_create_ctx_template(crypto_provider_handle_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_spi_ctx_template_t *,
    size_t *, crypto_req_handle_t);
static int des_free_context(crypto_ctx_t *);

static crypto_ctx_ops_t des_ctx_ops = {
	des_create_ctx_template,
	des_free_context
};

static int des_key_check(crypto_provider_handle_t, crypto_mechanism_t *,
    crypto_key_t *);

static crypto_key_ops_t des_key_ops = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	des_key_check
};

static crypto_ops_t des_crypto_ops = {
	&des_control_ops,
	NULL,
	&des_cipher_ops,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&des_key_ops,
	NULL,
	&des_ctx_ops
};

static crypto_provider_info_t des_prov_info = {
	CRYPTO_SPI_VERSION_1,
	"DES Software Provider",
	CRYPTO_SW_PROVIDER,
	{&modlinkage},
	NULL,
	&des_crypto_ops,
	sizeof (des_mech_info_tab)/sizeof (crypto_mech_info_t),
	des_mech_info_tab
};

static crypto_kcf_provider_handle_t des_prov_handle = NULL;

int
MODDRV_ENTRY_INIT(void)
{
	int ret;

	if ((ret = mod_install(&modlinkage)) != 0)
		return (ret);

	/*
	 * Register with KCF. If the registration fails, log an
	 * error but do not uninstall the module, since the functionality
	 * provided by misc/des should still be available.
	 */
	if ((ret = crypto_register_provider(&des_prov_info,
	    &des_prov_handle)) != CRYPTO_SUCCESS) {
		cmn_err(CE_WARN, "des _init: crypto_register_provider() "
		    "failed (0x%x)", ret);
	}

	return (0);
}


int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Copy 8 bytes
 */
#define	COPY8(src, dst) { \
	char *a = (char *)dst; \
	char *b = (char *)src; \
	*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
	*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
}

/*
 * Copy multiple of 8 bytes
 */
#define	DESCOPY(src, dst, len) { \
	char *a = (char *)dst; \
	char *b = (char *)src; \
	int i; \
	for (i = (size_t)len; i > 0; i -= 8) { \
		*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
		*a++ = *b++; *a++ = *b++; *a++ = *b++; *a++ = *b++; \
	} \
}

/*
 * CBC mode encryption
 */
/* ARGSUSED */
int
cbc_crypt(char *key, char *buf, size_t len, unsigned int mode, char *ivec)
{
	int err = 0;
/* EXPORT DELETE START */
	struct desparams dp;

	dp.des_mode = CBC;
	COPY8(ivec, dp.des_ivec);
	err = common_crypt(key, buf, len, mode, &dp);
	COPY8(dp.des_ivec, ivec);
/* EXPORT DELETE END */
	return (err);
}


/*
 * ECB mode encryption
 */
/* ARGSUSED */
int
ecb_crypt(char *key, char *buf, size_t len, unsigned int mode)
{
	int err = 0;
/* EXPORT DELETE START */
	struct desparams dp;

	dp.des_mode = ECB;
	err = common_crypt(key, buf, len, mode, &dp);
/* EXPORT DELETE END */
	return (err);
}



/* EXPORT DELETE START */
/*
 * Common code to cbc_crypt() & ecb_crypt()
 */
static int
common_crypt(char *key, char *buf, size_t len, unsigned int mode,
    struct desparams *desp)
{
	int desdev;

	if ((len % 8) != 0 || len > DES_MAXDATA)
		return (DESERR_BADPARAM);

	desp->des_dir =
	    ((mode & DES_DIRMASK) == DES_ENCRYPT) ? ENCRYPT : DECRYPT;

	desdev = mode & DES_DEVMASK;
	COPY8(key, desp->des_key);

#ifdef sun_hardware
	if (desdev == DES_HW) {
		int res;

		if (g_desfd < 0 &&
		    (g_desfd == -1 || (g_desfd = getdesfd()) < 0))
				goto software;	/* no hardware device */

		/*
		 * hardware
		 */
		desp->des_len = len;
		if (len <= DES_QUICKLEN) {
			DESCOPY(buf, desp->des_data, len);
			res = ioctl(g_desfd, DESIOCQUICK, (char *)desp);
			DESCOPY(desp->des_data, buf, len);
		} else {
			desp->des_buf = (uchar_t *)buf;
			res = ioctl(g_desfd, DESIOCBLOCK, (char *)desp);
		}
		return (res == 0 ? DESERR_NONE : DESERR_HWERROR);
	}
software:
#endif
	/*
	 * software
	 */
	if (!_des_crypt(buf, len, desp))
		return (DESERR_HWERROR);

	return (desdev == DES_SW ? DESERR_NONE : DESERR_NOHWDEVICE);
}

/*
 * Initialize key schedules for DES and DES3
 */
static int
init_keysched(crypto_key_t *key, void *newbie, des_strength_t strength)
{
	uint8_t corrected_key[DES3_KEYSIZE];

	/*
	 * Only keys by value are supported by this module.
	 */
	switch (key->ck_format) {
	case CRYPTO_KEY_RAW:
		if (strength == DES && key->ck_length != DES_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		if (strength == DES3 && key->ck_length != DES3_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		break;
	default:
		return (CRYPTO_KEY_TYPE_INCONSISTENT);
	}

	/*
	 * Fix parity bits.
	 * Initialize key schedule even if key is weak.
	 */
	if (key->ck_data == NULL)
		return (CRYPTO_ARGUMENTS_BAD);

	des_parity_fix(key->ck_data, strength, corrected_key);
	des_init_keysched(corrected_key, strength, newbie);
	return (CRYPTO_SUCCESS);
}

/* EXPORT DELETE END */

/*
 * KCF software provider control entry points.
 */
/* ARGSUSED */
static void
des_provider_status(crypto_provider_handle_t provider, uint_t *status)
{
	*status = CRYPTO_PROVIDER_READY;
}

/*
 * KCF software provider encrypt entry points.
 */
static int
des_common_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req)
{

/* EXPORT DELETE START */

	des_strength_t strength;
	des_ctx_t *des_ctx;
	int rv;
	int kmflag;

	/*
	 * Only keys by value are supported by this module.
	 */
	if (key->ck_format != CRYPTO_KEY_RAW) {
		return (CRYPTO_KEY_TYPE_INCONSISTENT);
	}

	/* Check mechanism type and parameter length */
	switch (mechanism->cm_type) {
	case DES_ECB_MECH_INFO_TYPE:
	case DES_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param != NULL &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES;
		break;
	case DES3_ECB_MECH_INFO_TYPE:
	case DES3_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param != NULL &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES3_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES3;
		break;
	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	/*
	 * Allocate a context.  Same context is used for DES and DES3.
	 */
	kmflag = crypto_kmflag(req);
	if ((des_ctx = kmem_zalloc(sizeof (des_ctx_t), kmflag)) == NULL)
		return (CRYPTO_HOST_MEMORY);

	if ((rv = des_common_init_ctx(des_ctx, template, mechanism, key,
	    strength, kmflag)) != CRYPTO_SUCCESS) {
		kmem_free(des_ctx, sizeof (des_ctx_t));
		return (rv);
	}

	ctx->cc_provider_private = des_ctx;

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/*
 * Helper DES encrypt update function for iov input data.
 */
static int
des_cipher_update_iov(des_ctx_t *des_ctx, crypto_data_t *input,
    crypto_data_t *output, int (*cipher)(des_ctx_t *, caddr_t, size_t,
    crypto_data_t *))
{
	if (input->cd_miscdata != NULL) {
		if (IS_P2ALIGNED(input->cd_miscdata, sizeof (uint64_t))) {
			/* LINTED: pointer alignment */
			des_ctx->dc_iv = *(uint64_t *)input->cd_miscdata;
		} else {
			uint64_t tmp64;
			uint8_t *tmp = (uint8_t *)input->cd_miscdata;

#ifdef _BIG_ENDIAN
			tmp64 = (((uint64_t)tmp[0] << 56) |
			    ((uint64_t)tmp[1] << 48) |
			    ((uint64_t)tmp[2] << 40) |
			    ((uint64_t)tmp[3] << 32) |
			    ((uint64_t)tmp[4] << 24) |
			    ((uint64_t)tmp[5] << 16) |
			    ((uint64_t)tmp[6] << 8) |
			    (uint64_t)tmp[7]);
#else
			tmp64 = (((uint64_t)tmp[7] << 56) |
			    ((uint64_t)tmp[6] << 48) |
			    ((uint64_t)tmp[5] << 40) |
			    ((uint64_t)tmp[4] << 32) |
			    ((uint64_t)tmp[3] << 24) |
			    ((uint64_t)tmp[2] << 16) |
			    ((uint64_t)tmp[1] << 8) |
			    (uint64_t)tmp[0]);
#endif /* _BIG_ENDIAN */

			des_ctx->dc_iv = tmp64;
		}
	}

	if (input->cd_raw.iov_len < input->cd_length)
		return (CRYPTO_ARGUMENTS_BAD);

	return ((cipher)(des_ctx, input->cd_raw.iov_base + input->cd_offset,
	    input->cd_length, (input == output) ? NULL : output));
}

/*
 * Helper DES encrypt update function for uio input data.
 */
static int
des_cipher_update_uio(des_ctx_t *des_ctx, crypto_data_t *input,
    crypto_data_t *output, int (*cipher)(des_ctx_t *, caddr_t, size_t,
    crypto_data_t *))
{
	uio_t *uiop = input->cd_uio;
	off_t offset = input->cd_offset;
	size_t length = input->cd_length;
	uint_t vec_idx;
	size_t cur_len;

	if (input->cd_miscdata != NULL) {
		if (IS_P2ALIGNED(input->cd_miscdata, sizeof (uint64_t))) {
			/* LINTED: pointer alignment */
			des_ctx->dc_iv = *(uint64_t *)input->cd_miscdata;
		} else {
			uint64_t tmp64;
			uint8_t *tmp = (uint8_t *)input->cd_miscdata;

#ifdef _BIG_ENDIAN
			tmp64 = (((uint64_t)tmp[0] << 56) |
			    ((uint64_t)tmp[1] << 48) |
			    ((uint64_t)tmp[2] << 40) |
			    ((uint64_t)tmp[3] << 32) |
			    ((uint64_t)tmp[4] << 24) |
			    ((uint64_t)tmp[5] << 16) |
			    ((uint64_t)tmp[6] << 8) |
			    (uint64_t)tmp[7]);
#else
			tmp64 = (((uint64_t)tmp[7] << 56) |
			    ((uint64_t)tmp[6] << 48) |
			    ((uint64_t)tmp[5] << 40) |
			    ((uint64_t)tmp[4] << 32) |
			    ((uint64_t)tmp[3] << 24) |
			    ((uint64_t)tmp[2] << 16) |
			    ((uint64_t)tmp[1] << 8) |
			    (uint64_t)tmp[0]);
#endif /* _BIG_ENDIAN */

			des_ctx->dc_iv = tmp64;
		}
	}

	if (input->cd_uio->uio_segflg != UIO_SYSSPACE) {
		return (CRYPTO_ARGUMENTS_BAD);
	}

	/*
	 * Jump to the first iovec containing data to be
	 * processed.
	 */
	for (vec_idx = 0; vec_idx < uiop->uio_iovcnt &&
	    offset >= uiop->uio_iov[vec_idx].iov_len;
	    offset -= uiop->uio_iov[vec_idx++].iov_len)
		;
	if (vec_idx == uiop->uio_iovcnt) {
		/*
		 * The caller specified an offset that is larger than the
		 * total size of the buffers it provided.
		 */
		return (CRYPTO_DATA_LEN_RANGE);
	}

	/*
	 * Now process the iovecs.
	 */
	while (vec_idx < uiop->uio_iovcnt && length > 0) {
		cur_len = MIN(uiop->uio_iov[vec_idx].iov_len -
		    offset, length);

		(cipher)(des_ctx, uiop->uio_iov[vec_idx].iov_base + offset,
		    cur_len, (input == output) ? NULL : output);

		length -= cur_len;
		vec_idx++;
		offset = 0;
	}

	if (vec_idx == uiop->uio_iovcnt && length > 0) {
		/*
		 * The end of the specified iovec's was reached but
		 * the length requested could not be processed, i.e.
		 * The caller requested to digest more data than it provided.
		 */

		return (CRYPTO_DATA_LEN_RANGE);
	}

	return (CRYPTO_SUCCESS);
}

/*
 * Helper DES encrypt update function for mblk input data.
 */
static int
des_cipher_update_mp(des_ctx_t *des_ctx, crypto_data_t *input,
    crypto_data_t *output, int (*cipher)(des_ctx_t *, caddr_t, size_t,
    crypto_data_t *))
{
	off_t offset = input->cd_offset;
	size_t length = input->cd_length;
	mblk_t *mp;
	size_t cur_len;

	if (input->cd_miscdata != NULL) {
		if (IS_P2ALIGNED(input->cd_miscdata, sizeof (uint64_t))) {
			/* LINTED: pointer alignment */
			des_ctx->dc_iv = *(uint64_t *)input->cd_miscdata;
		} else {
			uint64_t tmp64;
			uint8_t *tmp = (uint8_t *)input->cd_miscdata;

#ifdef _BIG_ENDIAN
			tmp64 = (((uint64_t)tmp[0] << 56) |
			    ((uint64_t)tmp[1] << 48) |
			    ((uint64_t)tmp[2] << 40) |
			    ((uint64_t)tmp[3] << 32) |
			    ((uint64_t)tmp[4] << 24) |
			    ((uint64_t)tmp[5] << 16) |
			    ((uint64_t)tmp[6] << 8) |
			    (uint64_t)tmp[7]);
#else
			tmp64 = (((uint64_t)tmp[7] << 56) |
			    ((uint64_t)tmp[6] << 48) |
			    ((uint64_t)tmp[5] << 40) |
			    ((uint64_t)tmp[4] << 32) |
			    ((uint64_t)tmp[3] << 24) |
			    ((uint64_t)tmp[2] << 16) |
			    ((uint64_t)tmp[1] << 8) |
			    (uint64_t)tmp[0]);
#endif /* _BIG_ENDIAN */

			des_ctx->dc_iv = tmp64;
		}
	}

	/*
	 * Jump to the first mblk_t containing data to be processed.
	 */
	for (mp = input->cd_mp; mp != NULL && offset >= MBLKL(mp);
	    offset -= MBLKL(mp), mp = mp->b_cont)
		;
	if (mp == NULL) {
		/*
		 * The caller specified an offset that is larger than the
		 * total size of the buffers it provided.
		 */
		return (CRYPTO_DATA_LEN_RANGE);
	}

	/*
	 * Now do the processing on the mblk chain.
	 */
	while (mp != NULL && length > 0) {
		cur_len = MIN(MBLKL(mp) - offset, length);
		(cipher)(des_ctx, (char *)(mp->b_rptr + offset), cur_len,
		    (input == output) ? NULL : output);

		length -= cur_len;
		offset = 0;
		mp = mp->b_cont;
	}

	if (mp == NULL && length > 0) {
		/*
		 * The end of the mblk was reached but the length requested
		 * could not be processed, i.e. The caller requested
		 * to digest more data than it provided.
		 */
		return (CRYPTO_DATA_LEN_RANGE);
	}

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
des_encrypt(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext, crypto_req_handle_t req)
{
	int ret;

/* EXPORT DELETE START */
	des_ctx_t *des_ctx;

	/*
	 * Plaintext must be a multiple of the block size.
	 * This test only works for non-padded mechanisms
	 * when blocksize is 2^N.
	 */
	if ((plaintext->cd_length & (DES_BLOCK_LEN - 1)) != 0)
		return (CRYPTO_DATA_LEN_RANGE);

	ASSERT(ctx->cc_provider_private != NULL);
	des_ctx = ctx->cc_provider_private;

	DES_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * We need to just return the length needed to store the output.
	 * We should not destroy the context for the following case.
	 */
	if (ciphertext->cd_length < plaintext->cd_length) {
		ciphertext->cd_length = plaintext->cd_length;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	/*
	 * Do an update on the specified input data.
	 */
	ret = des_encrypt_update(ctx, plaintext, ciphertext, req);
	ASSERT(des_ctx->dc_remainder_len == 0);
	(void) des_free_context(ctx);

/* EXPORT DELETE END */

	/* LINTED */
	return (ret);
}

/* ARGSUSED */
static int
des_decrypt(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_data_t *plaintext, crypto_req_handle_t req)
{
	int ret;

/* EXPORT DELETE START */
	des_ctx_t *des_ctx;

	/*
	 * Ciphertext must be a multiple of the block size.
	 * This test only works for non-padded mechanisms
	 * when blocksize is 2^N.
	 */
	if ((ciphertext->cd_length & (DES_BLOCK_LEN - 1)) != 0)
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);

	ASSERT(ctx->cc_provider_private != NULL);
	des_ctx = ctx->cc_provider_private;

	DES_ARG_INPLACE(ciphertext, plaintext);

	/*
	 * We need to just return the length needed to store the output.
	 * We should not destroy the context for the following case.
	 */
	if (plaintext->cd_length < ciphertext->cd_length) {
		plaintext->cd_length = ciphertext->cd_length;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	/*
	 * Do an update on the specified input data.
	 */
	ret = des_decrypt_update(ctx, ciphertext, plaintext, req);
	ASSERT(des_ctx->dc_remainder_len == 0);
	(void) des_free_context(ctx);

/* EXPORT DELETE END */

	/* LINTED */
	return (ret);
}

/* ARGSUSED */
static int
des_encrypt_update(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext, crypto_req_handle_t req)
{
	off_t saved_offset;
	size_t saved_length, out_len;
	int ret = CRYPTO_SUCCESS;

/* EXPORT DELETE START */

	ASSERT(ctx->cc_provider_private != NULL);

	DES_ARG_INPLACE(plaintext, ciphertext);

	/* compute number of bytes that will hold the ciphertext */
	out_len = ((des_ctx_t *)ctx->cc_provider_private)->dc_remainder_len;
	out_len += plaintext->cd_length;
	out_len &= ~(DES_BLOCK_LEN - 1);

	/* return length needed to store the output */
	if (ciphertext->cd_length < out_len) {
		ciphertext->cd_length = out_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_offset = ciphertext->cd_offset;
	saved_length = ciphertext->cd_length;

	/*
	 * Do the DES update on the specified input data.
	 */
	switch (plaintext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = des_cipher_update_iov(ctx->cc_provider_private,
		    plaintext, ciphertext, des_encrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_UIO:
		ret = des_cipher_update_uio(ctx->cc_provider_private,
		    plaintext, ciphertext, des_encrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_MBLK:
		ret = des_cipher_update_mp(ctx->cc_provider_private,
		    plaintext, ciphertext, des_encrypt_contiguous_blocks);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		if (plaintext != ciphertext)
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
	} else {
		ciphertext->cd_length = saved_length;
	}
	ciphertext->cd_offset = saved_offset;

/* EXPORT DELETE END */

	return (ret);
}

/* ARGSUSED */
static int
des_decrypt_update(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_data_t *plaintext, crypto_req_handle_t req)
{
	off_t saved_offset;
	size_t saved_length, out_len;
	int ret = CRYPTO_SUCCESS;

/* EXPORT DELETE START */

	ASSERT(ctx->cc_provider_private != NULL);

	DES_ARG_INPLACE(ciphertext, plaintext);

	/* compute number of bytes that will hold the plaintext */
	out_len = ((des_ctx_t *)ctx->cc_provider_private)->dc_remainder_len;
	out_len += ciphertext->cd_length;
	out_len &= ~(DES_BLOCK_LEN - 1);

	/* return length needed to store the output */
	if (plaintext->cd_length < out_len) {
		plaintext->cd_length = out_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	saved_offset = plaintext->cd_offset;
	saved_length = plaintext->cd_length;

	/*
	 * Do the DES update on the specified input data.
	 */
	switch (ciphertext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = des_cipher_update_iov(ctx->cc_provider_private,
		    ciphertext, plaintext, des_decrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_UIO:
		ret = des_cipher_update_uio(ctx->cc_provider_private,
		    ciphertext, plaintext, des_decrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_MBLK:
		ret = des_cipher_update_mp(ctx->cc_provider_private,
		    ciphertext, plaintext, des_decrypt_contiguous_blocks);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (ret == CRYPTO_SUCCESS) {
		if (ciphertext != plaintext)
			plaintext->cd_length =
			    plaintext->cd_offset - saved_offset;
	} else {
		plaintext->cd_length = saved_length;
	}
	plaintext->cd_offset = saved_offset;

/* EXPORT DELETE END */

	return (ret);
}

/* ARGSUSED */
static int
des_encrypt_final(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_req_handle_t req)
{

/* EXPORT DELETE START */

	des_ctx_t *des_ctx;

	ASSERT(ctx->cc_provider_private != NULL);
	des_ctx = ctx->cc_provider_private;

	/*
	 * There must be no unprocessed plaintext.
	 * This happens if the length of the last data is
	 * not a multiple of the DES block length.
	 */
	if (des_ctx->dc_remainder_len > 0)
		return (CRYPTO_DATA_LEN_RANGE);

	(void) des_free_context(ctx);
	ciphertext->cd_length = 0;

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
des_decrypt_final(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_req_handle_t req)
{

/* EXPORT DELETE START */

	des_ctx_t *des_ctx;

	ASSERT(ctx->cc_provider_private != NULL);
	des_ctx = ctx->cc_provider_private;

	/*
	 * There must be no unprocessed ciphertext.
	 * This happens if the length of the last ciphertext is
	 * not a multiple of the DES block length.
	 */
	if (des_ctx->dc_remainder_len > 0)
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);

	(void) des_free_context(ctx);
	plaintext->cd_length = 0;

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
des_encrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	int ret;

/* EXPORT DELETE START */

	des_ctx_t des_ctx;		/* on the stack */
	des_strength_t strength;
	off_t saved_offset;
	size_t saved_length;

	DES_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * Plaintext must be a multiple of the block size.
	 * This test only works for non-padded mechanisms
	 * when blocksize is 2^N.
	 */
	if ((plaintext->cd_length & (DES_BLOCK_LEN - 1)) != 0)
		return (CRYPTO_DATA_LEN_RANGE);

	/* return length needed to store the output */
	if (ciphertext->cd_length < plaintext->cd_length) {
		ciphertext->cd_length = plaintext->cd_length;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	/* Check mechanism type and parameter length */
	switch (mechanism->cm_type) {
	case DES_ECB_MECH_INFO_TYPE:
	case DES_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param_len > 0 &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES;
		break;
	case DES3_ECB_MECH_INFO_TYPE:
	case DES3_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param_len > 0 &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES3_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES3;
		break;
	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	bzero(&des_ctx, sizeof (des_ctx_t));

	if ((ret = des_common_init_ctx(&des_ctx, template, mechanism, key,
	    strength, crypto_kmflag(req))) != CRYPTO_SUCCESS) {
		return (ret);
	}

	saved_offset = ciphertext->cd_offset;
	saved_length = ciphertext->cd_length;

	/*
	 * Do the update on the specified input data.
	 */
	switch (plaintext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = des_cipher_update_iov(&des_ctx, plaintext, ciphertext,
		    des_encrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_UIO:
		ret = des_cipher_update_uio(&des_ctx, plaintext, ciphertext,
		    des_encrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_MBLK:
		ret = des_cipher_update_mp(&des_ctx, plaintext, ciphertext,
		    des_encrypt_contiguous_blocks);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (des_ctx.dc_flags & DES_PROVIDER_OWNS_KEY_SCHEDULE) {
		bzero(des_ctx.dc_keysched, des_ctx.dc_keysched_len);
		kmem_free(des_ctx.dc_keysched, des_ctx.dc_keysched_len);
	}

	if (ret == CRYPTO_SUCCESS) {
		ASSERT(des_ctx.dc_remainder_len == 0);
		if (plaintext != ciphertext)
			ciphertext->cd_length =
			    ciphertext->cd_offset - saved_offset;
	} else {
		ciphertext->cd_length = saved_length;
	}
	ciphertext->cd_offset = saved_offset;

/* EXPORT DELETE END */

	/* LINTED */
	return (ret);
}

/* ARGSUSED */
static int
des_decrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *ciphertext, crypto_data_t *plaintext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	int ret;

/* EXPORT DELETE START */

	des_ctx_t des_ctx;	/* on the stack */
	des_strength_t strength;
	off_t saved_offset;
	size_t saved_length;

	DES_ARG_INPLACE(ciphertext, plaintext);

	/*
	 * Ciphertext must be a multiple of the block size.
	 * This test only works for non-padded mechanisms
	 * when blocksize is 2^N.
	 */
	if ((ciphertext->cd_length & (DES_BLOCK_LEN - 1)) != 0)
		return (CRYPTO_DATA_LEN_RANGE);

	/* return length needed to store the output */
	if (plaintext->cd_length < ciphertext->cd_length) {
		plaintext->cd_length = ciphertext->cd_length;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	/* Check mechanism type and parameter length */
	switch (mechanism->cm_type) {
	case DES_ECB_MECH_INFO_TYPE:
	case DES_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param_len > 0 &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES;
		break;
	case DES3_ECB_MECH_INFO_TYPE:
	case DES3_CBC_MECH_INFO_TYPE:
		if (mechanism->cm_param_len > 0 &&
		    mechanism->cm_param_len != DES_BLOCK_LEN)
			return (CRYPTO_MECHANISM_PARAM_INVALID);
		if (key->ck_length != DES3_MINBITS)
			return (CRYPTO_KEY_SIZE_RANGE);
		strength = DES3;
		break;
	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	bzero(&des_ctx, sizeof (des_ctx_t));

	if ((ret = des_common_init_ctx(&des_ctx, template, mechanism, key,
	    strength, crypto_kmflag(req))) != CRYPTO_SUCCESS) {
		return (ret);
	}

	saved_offset = plaintext->cd_offset;
	saved_length = plaintext->cd_length;

	/*
	 * Do the update on the specified input data.
	 */
	switch (ciphertext->cd_format) {
	case CRYPTO_DATA_RAW:
		ret = des_cipher_update_iov(&des_ctx, ciphertext, plaintext,
		    des_decrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_UIO:
		ret = des_cipher_update_uio(&des_ctx, ciphertext, plaintext,
		    des_decrypt_contiguous_blocks);
		break;
	case CRYPTO_DATA_MBLK:
		ret = des_cipher_update_mp(&des_ctx, ciphertext, plaintext,
		    des_decrypt_contiguous_blocks);
		break;
	default:
		ret = CRYPTO_ARGUMENTS_BAD;
	}

	if (des_ctx.dc_flags & DES_PROVIDER_OWNS_KEY_SCHEDULE) {
		bzero(des_ctx.dc_keysched, des_ctx.dc_keysched_len);
		kmem_free(des_ctx.dc_keysched, des_ctx.dc_keysched_len);
	}

	if (ret == CRYPTO_SUCCESS) {
		ASSERT(des_ctx.dc_remainder_len == 0);
		if (ciphertext != plaintext)
			plaintext->cd_length =
			    plaintext->cd_offset - saved_offset;
	} else {
		plaintext->cd_length = saved_length;
	}
	plaintext->cd_offset = saved_offset;

/* EXPORT DELETE END */

	/* LINTED */
	return (ret);
}

/*
 * KCF software provider context template entry points.
 */
/* ARGSUSED */
static int
des_create_ctx_template(crypto_provider_handle_t provider,
    crypto_mechanism_t *mechanism, crypto_key_t *key,
    crypto_spi_ctx_template_t *tmpl, size_t *tmpl_size, crypto_req_handle_t req)
{

/* EXPORT DELETE START */

	des_strength_t strength;
	void *keysched;
	size_t size;
	int rv;

	switch (mechanism->cm_type) {
	case DES_ECB_MECH_INFO_TYPE:
		strength = DES;
		break;
	case DES_CBC_MECH_INFO_TYPE:
		strength = DES;
		break;
	case DES3_ECB_MECH_INFO_TYPE:
		strength = DES3;
		break;
	case DES3_CBC_MECH_INFO_TYPE:
		strength = DES3;
		break;
	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	if ((keysched = des_alloc_keysched(&size, strength,
	    crypto_kmflag(req))) == NULL) {
		return (CRYPTO_HOST_MEMORY);
	}

	/*
	 * Initialize key schedule.  Key length information is stored
	 * in the key.
	 */
	if ((rv = init_keysched(key, keysched, strength)) != CRYPTO_SUCCESS) {
		bzero(keysched, size);
		kmem_free(keysched, size);
		return (rv);
	}

	*tmpl = keysched;
	*tmpl_size = size;

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
des_free_context(crypto_ctx_t *ctx)
{

/* EXPORT DELETE START */

	des_ctx_t *des_ctx = ctx->cc_provider_private;

	if (des_ctx != NULL) {
		if (des_ctx->dc_flags & DES_PROVIDER_OWNS_KEY_SCHEDULE) {
			ASSERT(des_ctx->dc_keysched_len != 0);
			bzero(des_ctx->dc_keysched, des_ctx->dc_keysched_len);
			kmem_free(des_ctx->dc_keysched,
			    des_ctx->dc_keysched_len);
		}
		kmem_free(des_ctx, sizeof (des_ctx_t));
		ctx->cc_provider_private = NULL;
	}

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/*
 * Pass it to des_keycheck() which will
 * fix it (parity bits), and check if the fixed key is weak.
 */
/* ARGSUSED */
static int
des_key_check(crypto_provider_handle_t pd, crypto_mechanism_t *mech,
    crypto_key_t *key)
{

/* EXPORT DELETE START */

	int expectedkeylen;
	des_strength_t strength;
	uint8_t keydata[DES3_MAX_KEY_LEN];

	if ((mech == NULL) || (key == NULL))
		return (CRYPTO_ARGUMENTS_BAD);

	switch (mech->cm_type) {
	case DES_ECB_MECH_INFO_TYPE:
	case DES_CBC_MECH_INFO_TYPE:
		expectedkeylen = DES_MINBITS;
		strength = DES;
		break;
	case DES3_ECB_MECH_INFO_TYPE:
	case DES3_CBC_MECH_INFO_TYPE:
		expectedkeylen = DES3_MINBITS;
		strength = DES3;
		break;
	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	if (key->ck_format != CRYPTO_KEY_RAW)
		return (CRYPTO_KEY_TYPE_INCONSISTENT);

	if (key->ck_length != expectedkeylen)
		return (CRYPTO_KEY_SIZE_RANGE);

	bcopy(key->ck_data, keydata, CRYPTO_BITS2BYTES(expectedkeylen));

	if (des_keycheck(keydata, strength, key->ck_data) == B_FALSE)
		return (CRYPTO_WEAK_KEY);

/* EXPORT DELETE END */

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
des_common_init_ctx(des_ctx_t *des_ctx, crypto_spi_ctx_template_t *template,
    crypto_mechanism_t *mechanism, crypto_key_t *key, des_strength_t strength,
    int kmflag)
{
	int rv = CRYPTO_SUCCESS;

/* EXPORT DELETE START */

	void *keysched;
	size_t size;

	if (template == NULL) {
		if ((keysched = des_alloc_keysched(&size, strength,
		    kmflag)) == NULL)
			return (CRYPTO_HOST_MEMORY);
		/*
		 * Initialize key schedule.
		 * Key length is stored in the key.
		 */
		if ((rv = init_keysched(key, keysched,
		    strength)) != CRYPTO_SUCCESS)
			kmem_free(keysched, size);

		des_ctx->dc_flags = DES_PROVIDER_OWNS_KEY_SCHEDULE;
		des_ctx->dc_keysched_len = size;
	} else {
		keysched = template;
	}

	if (strength == DES3) {
		des_ctx->dc_flags |= DES3_STRENGTH;
	}

	if (mechanism->cm_type == DES_CBC_MECH_INFO_TYPE ||
	    mechanism->cm_type == DES3_CBC_MECH_INFO_TYPE) {
		/*
		 * Copy IV into DES context.
		 *
		 * If cm_param == NULL then the IV comes from the
		 * cd_miscdata field in the crypto_data structure.
		 */
		if (mechanism->cm_param != NULL) {
			ASSERT(mechanism->cm_param_len == DES_BLOCK_LEN);
			if (IS_P2ALIGNED(mechanism->cm_param,
			    sizeof (uint64_t))) {
				/* LINTED: pointer alignment */
				des_ctx->dc_iv =
				    *(uint64_t *)mechanism->cm_param;
			} else {
				uint64_t tmp64;
				uint8_t *tmp = (uint8_t *)mechanism->cm_param;

#ifdef _BIG_ENDIAN
				tmp64 = (((uint64_t)tmp[0] << 56) |
				    ((uint64_t)tmp[1] << 48) |
				    ((uint64_t)tmp[2] << 40) |
				    ((uint64_t)tmp[3] << 32) |
				    ((uint64_t)tmp[4] << 24) |
				    ((uint64_t)tmp[5] << 16) |
				    ((uint64_t)tmp[6] << 8) |
				    (uint64_t)tmp[7]);
#else
				tmp64 = (((uint64_t)tmp[7] << 56) |
				    ((uint64_t)tmp[6] << 48) |
				    ((uint64_t)tmp[5] << 40) |
				    ((uint64_t)tmp[4] << 32) |
				    ((uint64_t)tmp[3] << 24) |
				    ((uint64_t)tmp[2] << 16) |
				    ((uint64_t)tmp[1] << 8) |
				    (uint64_t)tmp[0]);
#endif /* _BIG_ENDIAN */

				des_ctx->dc_iv = tmp64;
			}
		}

		des_ctx->dc_lastp = (uint8_t *)&des_ctx->dc_iv;
		des_ctx->dc_flags |= DES_CBC_MODE;
	}
	des_ctx->dc_keysched = keysched;

/* EXPORT DELETE END */

	return (rv);
}
