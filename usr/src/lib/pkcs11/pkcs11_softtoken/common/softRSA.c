/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)softRSA.c	1.14	05/11/09 SMI"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <security/cryptoki.h>
#include <bignum.h>
#include "softGlobal.h"
#include "softSession.h"
#include "softObject.h"
#include "softOps.h"
#include "softRSA.h"
#include "softMAC.h"
#include "softRandom.h"
#include "softCrypt.h"

CK_RV
soft_rsa_encrypt(soft_object_t *key, CK_BYTE_PTR in, uint32_t in_len,
    CK_BYTE_PTR out, int realpublic)
{

	CK_RV rv = CKR_OK;

/* EXPORT DELETE START */

	uchar_t expo[MAX_KEY_ATTR_BUFLEN];
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t expo_len = sizeof (expo);
	uint32_t modulus_len = sizeof (modulus);
	BIGNUM msg;
	RSAkey *rsakey;

	if (realpublic) {
		rv = soft_get_public_attr(key, CKA_PUBLIC_EXPONENT, expo,
		    &expo_len);
		if (rv != CKR_OK) {
			goto clean1;
		}
	} else {
		rv = soft_get_private_attr(key, CKA_PRIVATE_EXPONENT, expo,
		    &expo_len);
		if (rv != CKR_OK) {
			goto clean1;
		}
	}

	rv = soft_get_public_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean1;
	}

	if (expo_len > modulus_len) {
		rv = CKR_KEY_SIZE_RANGE;
		goto clean1;
	}

	rsakey = calloc(1, sizeof (RSAkey));
	if (rsakey == NULL) {
		rv = CKR_HOST_MEMORY;
		goto clean1;
	}

	if (RSA_key_init(rsakey, modulus_len * 4, modulus_len * 4) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean4;
	}

	/* Size for big_init is in (32-bit) words. */
	if (big_init(&msg, (in_len + (int)sizeof (uint32_t) - 1) /
	    (int)sizeof (uint32_t)) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean5;
	}

	/* Convert octet string exponent to big integer format. */
	bytestring2bignum(&(rsakey->e), expo, expo_len);

	/* Convert octet string modulus to big integer format. */
	bytestring2bignum(&(rsakey->n), modulus, modulus_len);

	/* Convert octet string input data to big integer format. */
	bytestring2bignum(&msg, (uchar_t *)in, in_len);

	if (big_cmp_abs(&msg, &(rsakey->n)) > 0) {
		rv = CKR_DATA_LEN_RANGE;
		goto clean6;
	}

	/* Perform RSA computation on big integer input data. */
	if (big_modexp(&msg, &msg, &(rsakey->e), &(rsakey->n), NULL) !=
	    BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean6;
	}

	/* Convert the big integer output data to octet string. */
	bignum2bytestring((uchar_t *)out, &msg, modulus_len);

clean6:
	big_finish(&msg);
clean5:
	RSA_key_finish(rsakey);
clean4:
	free(rsakey);
clean1:

/* EXPORT DELETE END */

	return (rv);
}


CK_RV
soft_rsa_decrypt(soft_object_t *key, CK_BYTE_PTR in, uint32_t in_len,
    CK_BYTE_PTR out)
{

	CK_RV rv = CKR_OK;

/* EXPORT DELETE START */

	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uchar_t prime1[MAX_KEY_ATTR_BUFLEN];
	uchar_t prime2[MAX_KEY_ATTR_BUFLEN];
	uchar_t expo1[MAX_KEY_ATTR_BUFLEN];
	uchar_t expo2[MAX_KEY_ATTR_BUFLEN];
	uchar_t coef[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	uint32_t prime1_len = sizeof (prime1);
	uint32_t prime2_len = sizeof (prime2);
	uint32_t expo1_len = sizeof (expo1);
	uint32_t expo2_len = sizeof (expo2);
	uint32_t coef_len = sizeof (coef);
	BIGNUM msg;
	RSAkey *rsakey;

	rv = soft_get_private_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean1;
	}

	rv = soft_get_private_attr(key, CKA_PRIME_1, prime1, &prime1_len);

	if ((prime1_len == 0) && (rv == CKR_OK)) {
		rv = soft_rsa_encrypt(key, in, in_len, out, 0);
		goto clean1;
	} else {
		if (rv != CKR_OK)
			goto clean1;
	}

	rv = soft_get_private_attr(key, CKA_PRIME_2, prime2, &prime2_len);

	if ((prime2_len == 0) && (rv == CKR_OK)) {
		rv = soft_rsa_encrypt(key, in, in_len, out, 0);
		goto clean1;
	} else {
		if (rv != CKR_OK)
			goto clean1;
	}

	rv = soft_get_private_attr(key, CKA_EXPONENT_1, expo1, &expo1_len);

	if ((expo1_len == 0) && (rv == CKR_OK)) {
		rv = soft_rsa_encrypt(key, in, in_len, out, 0);
		goto clean1;
	} else {
		if (rv != CKR_OK)
			goto clean1;
	}

	rv = soft_get_private_attr(key, CKA_EXPONENT_2, expo2, &expo2_len);

	if ((expo2_len == 0) && (rv == CKR_OK)) {
		rv = soft_rsa_encrypt(key, in, in_len, out, 0);
		goto clean1;
	} else {
		if (rv != CKR_OK)
			goto clean1;
	}

	rv = soft_get_private_attr(key, CKA_COEFFICIENT, coef, &coef_len);

	if ((coef_len == 0) && (rv == CKR_OK)) {
		rv = soft_rsa_encrypt(key, in, in_len, out, 0);
		goto clean1;
	} else {
		if (rv != CKR_OK)
			goto clean1;
	}

	rsakey = calloc(1, sizeof (RSAkey));
	if (rsakey == NULL) {
		rv = CKR_HOST_MEMORY;
		goto clean1;
	}

	/* psize and qsize for RSA_key_init is in bits. */
	if (RSA_key_init(rsakey, prime2_len * 8, prime1_len * 8) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean8;
	}

	/* Size for big_init is in (32-bit) words. */
	if (big_init(&msg, (in_len + (int)sizeof (uint32_t) - 1) /
	    (int)sizeof (uint32_t)) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean9;
	}

	/* Convert octet string input data to big integer format. */
	bytestring2bignum(&msg, (uchar_t *)in, in_len);

	/* Convert octet string modulus to big integer format. */
	bytestring2bignum(&(rsakey->n), modulus, modulus_len);

	if (big_cmp_abs(&msg, &(rsakey->n)) > 0) {
		rv = CKR_DATA_LEN_RANGE;
		goto clean10;
	}

	/* Convert the rest of private key attributes to big integer format. */
	bytestring2bignum(&(rsakey->dmodpminus1), expo2, expo2_len);
	bytestring2bignum(&(rsakey->dmodqminus1), expo1, expo1_len);
	bytestring2bignum(&(rsakey->p), prime2, prime2_len);
	bytestring2bignum(&(rsakey->q), prime1, prime1_len);
	bytestring2bignum(&(rsakey->pinvmodq), coef, coef_len);

	if ((big_cmp_abs(&(rsakey->dmodpminus1), &(rsakey->p)) > 0) ||
	    (big_cmp_abs(&(rsakey->dmodqminus1), &(rsakey->q)) > 0) ||
	    (big_cmp_abs(&(rsakey->pinvmodq), &(rsakey->q)) > 0)) {
		rv = CKR_KEY_SIZE_RANGE;
		goto clean10;
	}

	/* Perform RSA computation on big integer input data. */
	if (big_modexp_crt(&msg, &msg, &(rsakey->dmodpminus1),
	    &(rsakey->dmodqminus1), &(rsakey->p), &(rsakey->q),
	    &(rsakey->pinvmodq), NULL, NULL) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean10;
	}

	/* Convert the big integer output data to octet string. */
	bignum2bytestring((uchar_t *)out, &msg, modulus_len);

clean10:
	big_finish(&msg);
clean9:
	RSA_key_finish(rsakey);
clean8:
	free(rsakey);
clean1:

/* EXPORT DELETE END */

	return (rv);
}

/*
 * Allocate a RSA context for the active encryption or decryption operation.
 * This function is called without the session lock held.
 */
CK_RV
soft_rsa_crypt_init_common(soft_session_t *session_p,
    CK_MECHANISM_PTR pMechanism, soft_object_t *key_p,
    boolean_t encrypt)
{

	soft_rsa_ctx_t *rsa_ctx;
	soft_object_t *tmp_key = NULL;
	CK_RV rv;

	rsa_ctx = calloc(1, sizeof (soft_rsa_ctx_t));
	if (rsa_ctx == NULL) {
		return (CKR_HOST_MEMORY);
	}

	/*
	 * Make a copy of the encryption or decryption key, and save it
	 * in the RSA crypto context since it will be used later for
	 * encryption/decryption. We don't want to hold any object reference
	 * on this original key while doing encryption/decryption.
	 */
	(void) pthread_mutex_lock(&key_p->object_mutex);
	rv = soft_copy_object(key_p, &tmp_key, SOFT_COPY_OBJ_ORIG_SH,
	    NULL);

	if ((rv != CKR_OK) || (tmp_key == NULL)) {
		/* Most likely we ran out of space. */
		(void) pthread_mutex_unlock(&key_p->object_mutex);
		free(rsa_ctx);
		return (rv);
	}

	/* No need to hold the lock on the old object. */
	(void) pthread_mutex_unlock(&key_p->object_mutex);
	rsa_ctx->key = tmp_key;

	(void) pthread_mutex_lock(&session_p->session_mutex);
	if (encrypt) {
		/* Called by C_EncryptInit. */
		session_p->encrypt.context = rsa_ctx;
		session_p->encrypt.mech.mechanism = pMechanism->mechanism;
	} else {
		/* Called by C_DecryptInit. */
		session_p->decrypt.context = rsa_ctx;
		session_p->decrypt.mech.mechanism = pMechanism->mechanism;
	}
	(void) pthread_mutex_unlock(&session_p->session_mutex);

	return (CKR_OK);
}

CK_RV
soft_rsa_encrypt_common(soft_session_t *session_p, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pEncrypted,
    CK_ULONG_PTR pulEncryptedLen, CK_MECHANISM_TYPE mechanism)
{

	soft_rsa_ctx_t *rsa_ctx = session_p->encrypt.context;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_BYTE	plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	CK_BYTE	cipher_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	CK_RV rv = CKR_OK;

	rv = soft_get_public_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (pEncrypted == NULL) {
		/*
		 * Application asks for the length of the output buffer
		 * to hold the ciphertext.
		 */
		*pulEncryptedLen = modulus_len;
		rv = CKR_OK;
		goto clean1;
	}

	if (mechanism == CKM_RSA_PKCS) {
		/*
		 * Input data length needs to be <=
		 * modulus length-MIN_PKCS1_PADLEN.
		 */
		if (ulDataLen > ((CK_ULONG)modulus_len - MIN_PKCS1_PADLEN)) {
			*pulEncryptedLen = modulus_len;
			rv = CKR_DATA_LEN_RANGE;
			goto clean_exit;
		}
	} else {
		/* Input data length needs to be <= modulus length. */
		if (ulDataLen > (CK_ULONG)modulus_len) {
			*pulEncryptedLen = modulus_len;
			rv = CKR_DATA_LEN_RANGE;
			goto clean_exit;
		}
	}

	/* Is the application-supplied buffer large enough? */
	if (*pulEncryptedLen < (CK_ULONG)modulus_len) {
		*pulEncryptedLen = modulus_len;
		rv = CKR_BUFFER_TOO_SMALL;
		goto clean1;
	}

	if (mechanism == CKM_RSA_PKCS) {
		/*
		 * Add PKCS padding to the input data to format a block
		 * type "02" encryption block.
		 */
		rv = soft_encrypt_rsa_pkcs_encode(pData, ulDataLen, plain_data,
		    modulus_len);

		if (rv != CKR_OK)
			goto clean_exit;
	} else {
		/* Pad zeros for the leading bytes of the input data. */
		(void) memset(plain_data, 0x0, modulus_len - ulDataLen);
		(void) memcpy(&plain_data[modulus_len - ulDataLen], pData,
		    ulDataLen);
	}

	rv = soft_rsa_encrypt(key, plain_data, modulus_len, cipher_data, 1);
	if (rv == CKR_OK) {
		(void) memcpy(pEncrypted, cipher_data, modulus_len);
		*pulEncryptedLen = modulus_len;
	}

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	free(session_p->encrypt.context);
	session_p->encrypt.context = NULL;
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	soft_cleanup_object(key);
	free(key);
clean1:
	return (rv);
}


CK_RV
soft_rsa_decrypt_common(soft_session_t *session_p, CK_BYTE_PTR pEncrypted,
    CK_ULONG ulEncryptedLen, CK_BYTE_PTR pData,
    CK_ULONG_PTR pulDataLen, CK_MECHANISM_TYPE mechanism)
{

	soft_rsa_ctx_t *rsa_ctx = session_p->decrypt.context;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_BYTE	plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	CK_RV rv = CKR_OK;

	rv = soft_get_private_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (ulEncryptedLen != (CK_ULONG)modulus_len) {
		rv = CKR_ENCRYPTED_DATA_LEN_RANGE;
		goto clean_exit;
	}

	if (pData == NULL) {
		/*
		 * Application asks for the length of the output buffer
		 * to hold the recovered data.
		 */
		*pulDataLen = modulus_len;
		rv = CKR_OK;
		goto clean1;
	}

	if (mechanism == CKM_RSA_X_509) {
		if (*pulDataLen < (CK_ULONG)modulus_len) {
			*pulDataLen = modulus_len;
			rv = CKR_BUFFER_TOO_SMALL;
			goto clean1;
		}
	}

	rv = soft_rsa_decrypt(key, pEncrypted, modulus_len, plain_data);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (mechanism == CKM_RSA_PKCS) {
		int plain_len = modulus_len;
		uint32_t num_padding;

		/* Strip off the PKCS block formatting data. */
		rv = soft_decrypt_rsa_pkcs_decode(plain_data, &plain_len);
		if (rv != CKR_OK)
			goto clean_exit;

		num_padding = modulus_len - plain_len;
		if (ulEncryptedLen - num_padding > *pulDataLen) {
			*pulDataLen = plain_len;
			rv = CKR_BUFFER_TOO_SMALL;
			goto clean1;
		}

		(void) memcpy(pData, &plain_data[num_padding], plain_len);
		*pulDataLen = plain_len;
	} else {
		(void) memcpy(pData, plain_data, modulus_len);
		*pulDataLen = modulus_len;
	}

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	free(session_p->decrypt.context);
	session_p->decrypt.context = NULL;
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	soft_cleanup_object(key);
	free(key);

clean1:
	return (rv);
}

/*
 * Allocate a RSA context for the active sign or verify operation.
 * This function is called without the session lock held.
 */
CK_RV
soft_rsa_sign_verify_init_common(soft_session_t *session_p,
    CK_MECHANISM_PTR pMechanism, soft_object_t *key_p,
    boolean_t sign)
{
	CK_RV rv = CKR_OK;
	soft_rsa_ctx_t *rsa_ctx;
	CK_MECHANISM digest_mech;
	soft_object_t *tmp_key = NULL;

	if (sign) {
		if ((key_p->class != CKO_PRIVATE_KEY) ||
		    (key_p->key_type != CKK_RSA))
			return (CKR_KEY_TYPE_INCONSISTENT);
	} else {
		if ((key_p->class != CKO_PUBLIC_KEY) ||
		    (key_p->key_type != CKK_RSA))
			return (CKR_KEY_TYPE_INCONSISTENT);
	}

	switch (pMechanism->mechanism) {
	case CKM_MD5_RSA_PKCS:
		digest_mech.mechanism = CKM_MD5;
		rv = soft_digest_init_internal(session_p, &digest_mech);
		if (rv != CKR_OK)
			return (rv);
		break;

	case CKM_SHA1_RSA_PKCS:
		digest_mech.mechanism = CKM_SHA_1;
		digest_mech.pParameter = pMechanism->pParameter;
		digest_mech.ulParameterLen = pMechanism->ulParameterLen;
		rv = soft_digest_init_internal(session_p, &digest_mech);
		if (rv != CKR_OK)
			return (rv);
		break;

	case CKM_SHA256_RSA_PKCS:
		digest_mech.mechanism = CKM_SHA256;
		rv = soft_digest_init_internal(session_p, &digest_mech);
		if (rv != CKR_OK)
			return (rv);
		break;

	case CKM_SHA384_RSA_PKCS:
		digest_mech.mechanism = CKM_SHA384;
		rv = soft_digest_init_internal(session_p, &digest_mech);
		if (rv != CKR_OK)
			return (rv);
		break;

	case CKM_SHA512_RSA_PKCS:
		digest_mech.mechanism = CKM_SHA512;
		rv = soft_digest_init_internal(session_p, &digest_mech);
		if (rv != CKR_OK)
			return (rv);
		break;
	}

	rsa_ctx = malloc(sizeof (soft_rsa_ctx_t));

	if (rsa_ctx == NULL) {
		rv = CKR_HOST_MEMORY;
		goto clean_exit;
	}

	(void) pthread_mutex_lock(&key_p->object_mutex);
	rv = soft_copy_object(key_p, &tmp_key, SOFT_COPY_OBJ_ORIG_SH,
	    NULL);

	if ((rv != CKR_OK) || (tmp_key == NULL)) {
		/* Most likely we ran out of space. */
		(void) pthread_mutex_unlock(&key_p->object_mutex);
		free(rsa_ctx);
		goto clean_exit;
	}

	/* No need to hold the lock on the old object. */
	(void) pthread_mutex_unlock(&key_p->object_mutex);
	rsa_ctx->key = tmp_key;

	(void) pthread_mutex_lock(&session_p->session_mutex);

	if (sign) {
		session_p->sign.context = rsa_ctx;
		session_p->sign.mech.mechanism = pMechanism->mechanism;
	} else {
		session_p->verify.context = rsa_ctx;
		session_p->verify.mech.mechanism = pMechanism->mechanism;
	}

	(void) pthread_mutex_unlock(&session_p->session_mutex);

	return (CKR_OK);

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	if (session_p->digest.context != NULL) {
		free(session_p->digest.context);
		session_p->digest.context = NULL;
		session_p->digest.flags = 0;
	}
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	return (rv);

}


CK_RV
soft_rsa_sign_common(soft_session_t *session_p, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pSigned,
    CK_ULONG_PTR pulSignedLen, CK_MECHANISM_TYPE mechanism)
{

	CK_RV rv = CKR_OK;
	soft_rsa_ctx_t *rsa_ctx = session_p->sign.context;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_BYTE	plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	CK_BYTE	signed_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	rv = soft_get_private_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (pSigned == NULL) {
		/* Application asks for the length of the output buffer. */
		*pulSignedLen = modulus_len;
		rv = CKR_OK;
		goto clean1;
	}

	switch (mechanism) {

	case CKM_RSA_PKCS:

		/*
		 * Input data length needs to be <=
		 * modulus length-MIN_PKCS1_PADLEN.
		 */
		if (ulDataLen > ((CK_ULONG)modulus_len - MIN_PKCS1_PADLEN)) {
			*pulSignedLen = modulus_len;
			rv = CKR_DATA_LEN_RANGE;
			goto clean_exit;
		}
		break;

	case CKM_RSA_X_509:

		/* Input data length needs to be <= modulus length. */
		if (ulDataLen > (CK_ULONG)modulus_len) {
			*pulSignedLen = modulus_len;
			rv = CKR_DATA_LEN_RANGE;
			goto clean_exit;
		}
		break;
	}

	/* Is the application-supplied buffer large enough? */
	if (*pulSignedLen < (CK_ULONG)modulus_len) {
		*pulSignedLen = modulus_len;
		rv = CKR_BUFFER_TOO_SMALL;
		goto clean1;
	}

	switch (mechanism) {

	case CKM_RSA_PKCS:
	case CKM_MD5_RSA_PKCS:
	case CKM_SHA1_RSA_PKCS:
	case CKM_SHA256_RSA_PKCS:
	case CKM_SHA384_RSA_PKCS:
	case CKM_SHA512_RSA_PKCS:
		/*
		 * Add PKCS padding to the input data to format a block
		 * type "01" encryption block.
		 */
		rv = soft_sign_rsa_pkcs_encode(pData, ulDataLen, plain_data,
		    modulus_len);

		if (rv != CKR_OK) {
			goto clean_exit;
		}
		break;

	case CKM_RSA_X_509:

		/* Pad zeros for the leading bytes of the input data. */
		(void) memset(plain_data, 0x0, modulus_len - ulDataLen);
		(void) memcpy(&plain_data[modulus_len - ulDataLen], pData,
		    ulDataLen);
		break;
	}

	/*
	 * Perform RSA encryption with the signer's RSA private key
	 * for signature process.
	 */
	rv = soft_rsa_decrypt(key, plain_data, modulus_len, signed_data);

	if (rv == CKR_OK) {
		(void) memcpy(pSigned, signed_data, modulus_len);
		*pulSignedLen = modulus_len;
	}

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	free(session_p->sign.context);
	session_p->sign.context = NULL;
	if (session_p->digest.context != NULL) {
		free(session_p->digest.context);
		session_p->digest.context = NULL;
		session_p->digest.flags = 0;
	}
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	soft_cleanup_object(key);
	free(key);

clean1:
	return (rv);
}


CK_RV
soft_rsa_verify_common(soft_session_t *session_p, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
    CK_ULONG ulSignatureLen, CK_MECHANISM_TYPE mechanism)
{

	CK_RV rv = CKR_OK;
	soft_rsa_ctx_t *rsa_ctx = session_p->verify.context;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_BYTE	plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	rv = soft_get_public_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (ulSignatureLen != (CK_ULONG)modulus_len) {
		rv = CKR_SIGNATURE_LEN_RANGE;
		goto clean_exit;
	}

	/*
	 * Perform RSA decryption with the signer's RSA public key
	 * for verification process.
	 */
	rv = soft_rsa_encrypt(key, pSignature, modulus_len, plain_data, 1);
	if (rv == CKR_OK) {
		switch (mechanism) {

		case CKM_RSA_PKCS:
		case CKM_MD5_RSA_PKCS:
		case CKM_SHA1_RSA_PKCS:
		case CKM_SHA256_RSA_PKCS:
		case CKM_SHA384_RSA_PKCS:
		case CKM_SHA512_RSA_PKCS:
		{
			/*
			 * Strip off the encoded padding bytes in front of the
			 * recovered data, then compare the recovered data with
			 * the original data.
			 */
			int data_len = modulus_len;

			rv = soft_verify_rsa_pkcs_decode(plain_data, &data_len);
			if (rv != CKR_OK) {
				goto clean_exit;
			}

			if ((CK_ULONG)data_len != ulDataLen) {
				rv = CKR_SIGNATURE_LEN_RANGE;
				goto clean_exit;
			} else if (memcmp(pData,
			    &plain_data[modulus_len - data_len],
			    ulDataLen) != 0) {
				rv = CKR_SIGNATURE_INVALID;
				goto clean_exit;
			}
			break;
		}

		case CKM_RSA_X_509:
			/*
			 * Strip off the encoded padding bytes in front of the
			 * recovered plain_data, then compare the input data
			 * with the recovered data.
			 */
			if (memcmp(pData,
			    plain_data + ulSignatureLen - ulDataLen,
			    ulDataLen) != 0) {
				rv = CKR_SIGNATURE_INVALID;
				goto clean_exit;
			}
			break;
		}
	}

	if (rv == CKR_DATA_LEN_RANGE) {
		if ((mechanism == CKM_MD5_RSA_PKCS) ||
		    (mechanism == CKM_SHA1_RSA_PKCS) ||
		    (mechanism == CKM_SHA256_RSA_PKCS) ||
		    (mechanism == CKM_SHA384_RSA_PKCS) ||
		    (mechanism == CKM_SHA512_RSA_PKCS))
			rv = CKR_SIGNATURE_INVALID;
	}

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	free(session_p->verify.context);
	session_p->verify.context = NULL;
	if (session_p->digest.context != NULL) {
		free(session_p->digest.context);
		session_p->digest.context = NULL;
		session_p->digest.flags = 0;
	}
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	soft_cleanup_object(key);
	free(key);
	return (rv);
}

CK_RV
soft_genRSAkey_set_attribute(soft_object_t *key, RSAkey *rsakey,
    CK_ATTRIBUTE_TYPE type, uint32_t modulus_len, boolean_t public)
{

	uchar_t	*buf, *buf1;
	uint32_t buflen;
	CK_RV rv = CKR_OK;
	biginteger_t *dst = NULL;
	biginteger_t src;

	/*
	 * Allocate the buffer used to store the value of key fields
	 * for bignum2bytestring. Since bignum only deals with a buffer
	 * whose size is multiple of 4, modulus_len is rounded up to be
	 * multiple of 4.
	 */
	if ((buf1 = malloc((modulus_len + 3) & ~3)) == NULL) {
		rv = CKR_HOST_MEMORY;
		goto cleanexit;
	}

	buf = buf1;

	switch (type) {

	case CKA_MODULUS:

		buflen = rsakey->n.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->n, buflen);
		if (public)
			dst = OBJ_PUB_RSA_MOD(key);
		else
			dst = OBJ_PRI_RSA_MOD(key);
		break;

	case CKA_PUBLIC_EXPONENT:

		buflen = rsakey->e.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->e, buflen);
		if (public)
			dst = OBJ_PUB_RSA_PUBEXPO(key);
		else
			dst = OBJ_PRI_RSA_PUBEXPO(key);
		break;

	case CKA_PRIVATE_EXPONENT:

		buflen = rsakey->d.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->d, buflen);
		dst = OBJ_PRI_RSA_PRIEXPO(key);
		break;

	case CKA_PRIME_1:

		buflen = rsakey->q.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->q, buflen);
		dst = OBJ_PRI_RSA_PRIME1(key);
		break;

	case CKA_PRIME_2:

		buflen = rsakey->p.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->p, buflen);
		dst = OBJ_PRI_RSA_PRIME2(key);
		break;

	case CKA_EXPONENT_1:

		buflen = rsakey->dmodqminus1.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->dmodqminus1, buflen);
		dst = OBJ_PRI_RSA_EXPO1(key);
		break;

	case CKA_EXPONENT_2:

		buflen = rsakey->dmodpminus1.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->dmodpminus1, buflen);
		dst = OBJ_PRI_RSA_EXPO2(key);
		break;

	case CKA_COEFFICIENT:

		buflen = rsakey->pinvmodq.len * (int)sizeof (uint32_t);
		bignum2bytestring(buf, &rsakey->pinvmodq, buflen);
		dst = OBJ_PRI_RSA_COEF(key);
		break;
	}

	while (buf[0] == 0) {	/* remove proceeding 0x00 */
		buf++;
		buflen--;
	}

	src.big_value_len = buflen;

	if ((src.big_value = malloc(buflen)) == NULL) {
		rv = CKR_HOST_MEMORY;
		goto cleanexit;
	}
	(void) memcpy(src.big_value, buf, buflen);

	/* Copy the attribute in the key object. */
	copy_bigint_attr(&src, dst);

cleanexit:
	free(buf1);
	return (rv);

}


CK_RV
generate_rsa_key(RSAkey *key, int psize, int qsize, BIGNUM * pubexp,
    boolean_t token_obj)
{
	CK_RV		rv = CKR_OK;

/* EXPORT DELETE START */

	BIGNUM		a, b, c, d, e, f, g, h;
	int		len, keylen, size;
	BIG_ERR_CODE	brv = BIG_OK;

	size = psize + qsize;
	keylen = (size + 31) / 32;
	len = keylen * 2 + 1;
	key->size = size;

	a.malloced = 0;
	b.malloced = 0;
	c.malloced = 0;
	d.malloced = 0;
	e.malloced = 0;
	f.malloced = 0;
	g.malloced = 0;
	h.malloced = 0;

	if ((big_init(&a, len) != BIG_OK) ||
	    (big_init(&b, len) != BIG_OK) ||
	    (big_init(&c, len) != BIG_OK) ||
	    (big_init(&d, len) != BIG_OK) ||
	    (big_init(&e, len) != BIG_OK) ||
	    (big_init(&f, len) != BIG_OK) ||
	    (big_init(&g, len) != BIG_OK) ||
	    (big_init(&h, len) != BIG_OK)) {
		big_finish(&h);
		big_finish(&g);
		big_finish(&f);
		big_finish(&e);
		big_finish(&d);
		big_finish(&c);
		big_finish(&b);
		big_finish(&a);

		return (CKR_HOST_MEMORY);
	}
nextp:
	if ((brv = random_bignum(&a, psize, token_obj)) != BIG_OK) {
		goto ret;
	}

	if ((brv = big_nextprime_pos(&b, &a)) != BIG_OK) {
		goto ret;
	}
	(void) big_sub_pos(&a, &b, &One);
	if ((brv = big_ext_gcd_pos(&f, &d, &g, pubexp, &a)) != BIG_OK) {
		goto ret;
	}
	if (big_cmp_abs(&f, &One) != 0) {
		goto nextp;
	}

	if ((brv = random_bignum(&c, qsize, token_obj)) != BIG_OK) {
		goto ret;
	}

nextq:
	(void) big_add(&a, &c, &Two);

	if (big_bitlength(&a) != qsize) {
		goto nextp;
	}
	if (big_cmp_abs(&a, &b) == 0) {
		goto nextp;
	}
	if ((brv = big_nextprime_pos(&c, &a)) != BIG_OK) {
		goto ret;
	}
	if ((brv = big_mul(&g, &b, &c)) != BIG_OK) {
		goto ret;
	}
	if (big_bitlength(&g) != size) {
		goto nextp;
	}

	(void) big_sub_pos(&a, &b, &One);
	(void) big_sub_pos(&d, &c, &One);

	if ((brv = big_mul(&a, &a, &d)) != BIG_OK) {
		goto ret;
	}
	if ((brv = big_ext_gcd_pos(&f, &d, &h, pubexp, &a)) != BIG_OK) {
		goto ret;
	}
	if (big_cmp_abs(&f, &One) != 0) {
		goto nextq;
	} else {
		(void) big_copy(&e, pubexp);
	}
	if (d.sign == -1) {
		if ((brv = big_add(&d, &d, &a)) != BIG_OK) {
			goto ret;
		}
	}
	(void) big_copy(&(key->p), &b);
	(void) big_copy(&(key->q), &c);
	(void) big_copy(&(key->n), &g);
	(void) big_copy(&(key->d), &d);
	(void) big_copy(&(key->e), &e);

	if ((brv = big_ext_gcd_pos(&a, &f, &h, &b, &c)) != BIG_OK) {
		goto ret;
	}
	if (f.sign == -1) {
		if ((brv = big_add(&f, &f, &c)) != BIG_OK) {
			goto ret;
		}
	}
	(void) big_copy(&(key->pinvmodq), &f);

	(void) big_sub(&a, &b, &One);
	if ((brv = big_div_pos(&a, &f, &d, &a)) != BIG_OK) {
		goto ret;
	}
	(void) big_copy(&(key->dmodpminus1), &f);
	(void) big_sub(&a, &c, &One);
	if ((brv = big_div_pos(&a, &f, &d, &a)) != BIG_OK) {
		goto ret;
	}
	(void) big_copy(&(key->dmodqminus1), &f);

	if ((brv = random_bignum(&h, size, token_obj)) != BIG_OK) {
		goto ret;
	}
	if ((brv = big_div_pos(&a, &h, &h, &g)) != BIG_OK) {
		goto ret;
	}
	if ((brv = big_modexp(&a, &h, &d, &g, NULL)) != BIG_OK) {
		goto ret;
	}

	if ((brv = big_modexp(&b, &a, &e, &g, NULL)) != BIG_OK) {
		goto ret;
	}

	if (big_cmp_abs(&b, &h) != 0) {
		rv = generate_rsa_key(key, psize, qsize, pubexp, token_obj);
		goto ret1;
	} else {
		brv = BIG_OK;
	}

ret:
	rv = convert_rv(brv);
ret1:
	big_finish(&h);
	big_finish(&g);
	big_finish(&f);
	big_finish(&e);
	big_finish(&d);
	big_finish(&c);
	big_finish(&b);
	big_finish(&a);

/* EXPORT DELETE END */

	return (rv);
}


CK_RV
soft_rsa_genkey_pair(soft_object_t *pubkey, soft_object_t *prikey)
{

	CK_RV rv = CKR_OK;
	uint32_t modulus_len;
	uchar_t	pub_expo[MAX_KEY_ATTR_BUFLEN];
	uint32_t pub_expo_len = sizeof (pub_expo);
	BIGNUM	public_exponent = {0};
	RSAkey	rsakey = {0};
	CK_ATTRIBUTE template;

	if ((pubkey == NULL) || (prikey == NULL)) {
		return (CKR_ARGUMENTS_BAD);
	}

	template.pValue = malloc(sizeof (CK_ULONG));

	if (template.pValue == NULL) {
		return (CKR_HOST_MEMORY);
	}

	template.ulValueLen = sizeof (CK_ULONG);

	rv = get_ulong_attr_from_object(OBJ_PUB_RSA_MOD_BITS(pubkey),
	    &template);

	if (rv != CKR_OK) {
		goto clean0;
	}

#ifdef	__sparcv9
	/* LINTED */
	modulus_len = (uint32_t)(*((CK_ULONG *)(template.pValue)));
#else	/* !__sparcv9 */
	modulus_len = *((CK_ULONG *)(template.pValue));
#endif	/* __sparcv9 */

	/* Convert modulus length from bit length to byte length. */
	modulus_len = (modulus_len + 7) / 8;

	/* Modulus length needs to be between min key size and max key size. */
	if ((modulus_len < MIN_RSA_KEYLENGTH_IN_BYTES) ||
	    (modulus_len > MAX_RSA_KEYLENGTH_IN_BYTES)) {
		rv = CKR_ATTRIBUTE_VALUE_INVALID;
		goto clean0;
	}

	rv = soft_get_public_attr(pubkey, CKA_PUBLIC_EXPONENT, pub_expo,
	    &pub_expo_len);
	if (rv != CKR_OK) {
		goto clean0;
	}

	/* Create a public exponent in bignum format. */
	if (big_init(&public_exponent, (modulus_len + 3)/4) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean0;
	}
	bytestring2bignum(&public_exponent, pub_expo, pub_expo_len);

	if (RSA_key_init(&rsakey, modulus_len * 4, modulus_len * 4) != BIG_OK) {
		rv = CKR_HOST_MEMORY;
		goto clean2;
	}

	/* Generate RSA key pair. */
	if ((rv = generate_rsa_key(&rsakey, modulus_len * 4, modulus_len * 4,
	    &public_exponent, (IS_TOKEN_OBJECT(pubkey) ||
	    IS_TOKEN_OBJECT(prikey)))) != CKR_OK) {
		goto clean3;
	}

	/*
	 * Add modulus in public template, and add all eight key fields
	 * in private template.
	 */
	if ((rv = soft_genRSAkey_set_attribute(pubkey, &rsakey,
	    CKA_MODULUS, modulus_len, B_TRUE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_MODULUS, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_PRIVATE_EXPONENT, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_PUBLIC_EXPONENT, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_PRIME_1, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_PRIME_2, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_EXPONENT_1, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_EXPONENT_2, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

	if ((rv = soft_genRSAkey_set_attribute(prikey, &rsakey,
	    CKA_COEFFICIENT, modulus_len, B_FALSE)) != CKR_OK) {
		goto clean3;
	}

clean3:
	RSA_key_finish(&rsakey);
clean2:
	big_finish(&public_exponent);
clean0:
	free(template.pValue);

	return (rv);
}


CK_ULONG
get_rsa_sha1_prefix(CK_MECHANISM_PTR mech, CK_BYTE_PTR *prefix) {
	if (mech->pParameter == NULL) {
		*prefix = (CK_BYTE *)SHA1_DER_PREFIX;
		return (SHA1_DER_PREFIX_Len);
	}

	*prefix = (CK_BYTE *)SHA1_DER_PREFIX_OID;
	return (SHA1_DER_PREFIX_OID_Len);
}

CK_RV
soft_rsa_digest_sign_common(soft_session_t *session_p, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pSigned,
    CK_ULONG_PTR pulSignedLen, CK_MECHANISM_TYPE mechanism, boolean_t Final)
{

	CK_RV rv = CKR_OK;
	CK_BYTE hash[SHA512_DIGEST_LENGTH];  /* space enough for all mechs */
	CK_ULONG hash_len = SHA512_DIGEST_LENGTH;
	/* space enough for all mechs */
	CK_BYTE der_data[SHA512_DIGEST_LENGTH + SHA2_DER_PREFIX_Len];
	CK_ULONG der_data_len;
	soft_rsa_ctx_t *rsa_ctx = session_p->sign.context;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_ULONG der_len;
	CK_BYTE_PTR der_prefix;

	rv = soft_get_private_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		(void) pthread_mutex_lock(&session_p->session_mutex);
		free(session_p->digest.context);
		session_p->digest.context = NULL;
		session_p->digest.flags = 0;
		(void) pthread_mutex_unlock(&session_p->session_mutex);
		soft_cleanup_object(key);
		free(key);
		goto clean1;
	}

	/* Check arguments before performing message digest. */
	if (pSigned == NULL) {
		/* Application asks for the length of the output buffer. */
		*pulSignedLen = modulus_len;
		rv = CKR_OK;
		goto clean1;
	}

	/* Is the application-supplied buffer large enough? */
	if (*pulSignedLen < (CK_ULONG)modulus_len) {
		*pulSignedLen = modulus_len;
		rv = CKR_BUFFER_TOO_SMALL;
		goto clean1;
	}

	if (Final) {
		rv = soft_digest_final(session_p, hash, &hash_len);
	} else {
		rv = soft_digest(session_p, pData, ulDataLen, hash, &hash_len);
	}

	if (rv != CKR_OK) {
		/* free the signature key */
		soft_cleanup_object(key);
		free(key);
		goto clean_exit;
	}

	/*
	 * Prepare the DER encoding of the DigestInfo value by setting it to:
	 *	<MECH>_DER_PREFIX || H
	 *
	 * See rsa_impl.c for more details.
	 */
	switch (session_p->digest.mech.mechanism) {
	case CKM_MD5:
		(void) memcpy(der_data, MD5_DER_PREFIX, MD5_DER_PREFIX_Len);
		(void) memcpy(der_data + MD5_DER_PREFIX_Len, hash, hash_len);
		der_data_len = MD5_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA_1:
		der_len = get_rsa_sha1_prefix(&(session_p->digest.mech),
		    &der_prefix);
		(void) memcpy(der_data, der_prefix, der_len);
		(void) memcpy(der_data + der_len, hash, hash_len);
		der_data_len = der_len + hash_len;
		break;
	case CKM_SHA256:
		(void) memcpy(der_data, SHA256_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA384:
		(void) memcpy(der_data, SHA384_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA512:
		(void) memcpy(der_data, SHA512_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	}

	/*
	 * Now, we are ready to sign the DER_ENCODED data
	 * soft_rsa_sign_common() will free the signature key.
	 */
	rv = soft_rsa_sign_common(session_p, der_data, der_data_len,
	    pSigned, pulSignedLen, mechanism);

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	/* soft_digest_common() has freed the digest context */
	session_p->digest.flags = 0;
	(void) pthread_mutex_unlock(&session_p->session_mutex);

clean1:
	return (rv);
}


CK_RV
soft_rsa_digest_verify_common(soft_session_t *session_p, CK_BYTE_PTR pData,
    CK_ULONG ulDataLen, CK_BYTE_PTR pSigned,
    CK_ULONG ulSignedLen, CK_MECHANISM_TYPE mechanism, boolean_t Final)
{

	CK_RV rv = CKR_OK;
	CK_BYTE hash[SHA512_DIGEST_LENGTH];  /* space for all mechs */
	CK_ULONG hash_len = SHA512_DIGEST_LENGTH;
	CK_BYTE der_data[SHA512_DIGEST_LENGTH + SHA2_DER_PREFIX_Len];
	CK_ULONG der_data_len;
	soft_rsa_ctx_t *rsa_ctx = session_p->verify.context;
	soft_object_t *key = rsa_ctx->key;
	CK_ULONG der_len;
	CK_BYTE_PTR der_prefix;

	if (Final) {
		rv = soft_digest_final(session_p, hash, &hash_len);
	} else {
		rv = soft_digest(session_p, pData, ulDataLen, hash, &hash_len);
	}

	if (rv != CKR_OK) {
		/* free the verification key */
		soft_cleanup_object(key);
		free(key);
		goto clean_exit;
	}

	/*
	 * Prepare the DER encoding of the DigestInfo value as follows:
	 * MD5:		MD5_DER_PREFIX || H
	 * SHA-1:	SHA1_DER_PREFIX || H
	 * SHA2:	SHA2_DER_PREFIX || H
	 *
	 * See rsa_impl.c for more details.
	 */
	switch (session_p->digest.mech.mechanism) {
	case CKM_MD5:
		(void) memcpy(der_data, MD5_DER_PREFIX, MD5_DER_PREFIX_Len);
		(void) memcpy(der_data + MD5_DER_PREFIX_Len, hash, hash_len);
		der_data_len = MD5_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA_1:
		der_len = get_rsa_sha1_prefix(&(session_p->digest.mech),
		    &der_prefix);
		(void) memcpy(der_data, der_prefix, der_len);
		(void) memcpy(der_data + der_len, hash, hash_len);
		der_data_len = der_len + hash_len;
		break;
	case CKM_SHA256:
		(void) memcpy(der_data, SHA256_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA384:
		(void) memcpy(der_data, SHA384_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	case CKM_SHA512:
		(void) memcpy(der_data, SHA512_DER_PREFIX,
		    SHA2_DER_PREFIX_Len);
		(void) memcpy(der_data + SHA2_DER_PREFIX_Len, hash, hash_len);
		der_data_len = SHA2_DER_PREFIX_Len + hash_len;
		break;
	}

	/*
	 * Now, we are ready to verify the DER_ENCODED data using signature.
	 * soft_rsa_verify_common() will free the verification key.
	 */
	rv = soft_rsa_verify_common(session_p, der_data, der_data_len,
	    pSigned, ulSignedLen, mechanism);

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	/* soft_digest_common() has freed the digest context */
	session_p->digest.flags = 0;
	(void) pthread_mutex_unlock(&session_p->session_mutex);

	return (rv);

}


CK_RV
soft_rsa_verify_recover(soft_session_t *session_p, CK_BYTE_PTR pSignature,
    CK_ULONG ulSignatureLen, CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen)
{

	CK_RV rv = CKR_OK;
	soft_rsa_ctx_t *rsa_ctx = session_p->verify.context;
	CK_MECHANISM_TYPE mechanism = session_p->verify.mech.mechanism;
	soft_object_t *key = rsa_ctx->key;
	uchar_t modulus[MAX_KEY_ATTR_BUFLEN];
	uint32_t modulus_len = sizeof (modulus);
	CK_BYTE	plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	rv = soft_get_public_attr(key, CKA_MODULUS, modulus, &modulus_len);
	if (rv != CKR_OK) {
		goto clean_exit;
	}

	if (ulSignatureLen != (CK_ULONG)modulus_len) {
		rv = CKR_SIGNATURE_LEN_RANGE;
		goto clean_exit;
	}

	/*
	 * Perform RSA decryption with the signer's RSA public key
	 * for verification process.
	 */
	rv = soft_rsa_encrypt(key, pSignature, modulus_len, plain_data, 1);
	if (rv == CKR_OK) {
		switch (mechanism) {

		case CKM_RSA_PKCS:
		{
			/*
			 * Strip off the encoded padding bytes in front of the
			 * recovered data.
			 */
			int data_len = modulus_len;

			rv = soft_verify_rsa_pkcs_decode(plain_data, &data_len);
			if (rv != CKR_OK) {
				goto clean_exit;
			}

			/*
			 * If application asks for the length of the output
			 * buffer?
			 */
			if (pData == NULL) {
				*pulDataLen = data_len;
				rv = CKR_OK;
				goto clean1;
			}

			/* Is the application-supplied buffer large enough? */
			if (*pulDataLen < (CK_ULONG)data_len) {
				*pulDataLen = data_len;
				rv = CKR_BUFFER_TOO_SMALL;
				goto clean1;
			}

			(void) memcpy(pData,
			    &plain_data[modulus_len - data_len], data_len);
			*pulDataLen = data_len;

			break;
		}

		case CKM_RSA_X_509:
			/*
			 * If application asks for the length of the output
			 * buffer?
			 */
			if (pData == NULL) {
				*pulDataLen = modulus_len;
				rv = CKR_OK;
				goto clean1;
			}

			/* Is the application-supplied buffer large enough? */
			if (*pulDataLen < (CK_ULONG)modulus_len) {
				*pulDataLen = modulus_len;
				rv = CKR_BUFFER_TOO_SMALL;
				goto clean1;
			}

			(void) memcpy(pData, plain_data, modulus_len);
			*pulDataLen = modulus_len;

			break;
		}
	}

clean_exit:
	(void) pthread_mutex_lock(&session_p->session_mutex);
	free(session_p->verify.context);
	session_p->verify.context = NULL;
	(void) pthread_mutex_unlock(&session_p->session_mutex);
	soft_cleanup_object(key);
	free(key);

clean1:
	return (rv);
}
