/*
 * ECDSA for RT58x: MBEDTLS_ECDSA_SIGN_ALT / MBEDTLS_ECDSA_VERIFY_ALT, backed
 * by the crypto engine's HOSAL_ECDA_P256 operations (SECP256R1 only).
 *
 * Deterministic ECDSA (MBEDTLS_ECDSA_DETERMINISTIC) still works: mbedtls
 * 2.28 implements it by calling this mbedtls_ecdsa_sign() with an HMAC-DRBG
 * as f_rng, and the nonce k is drawn from f_rng exactly like the software
 * implementation (mbedtls_ecp_gen_privkey).
 *
 * The nonce/keys are converted OUTSIDE the hw_crypto_lock; only the hosal
 * calls run under the lock, matching the other alt implementations.
 */

#include "mbedtls/ecdsa.h"

#if defined(MBEDTLS_ECDSA_SIGN_ALT) || defined(MBEDTLS_ECDSA_VERIFY_ALT)

#include <string.h>

#include "hw_ecc_p256.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_ECDSA_SIGN_ALT)
/*
 * Compute the ECDSA signature of a hashed message.
 */
int mbedtls_ecdsa_sign(mbedtls_ecp_group * grp, mbedtls_mpi * r, mbedtls_mpi * s, const mbedtls_mpi * d, const unsigned char * buf,
                       size_t blen, int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    int ret;
    int status;
    int attempts;
    uint8_t d_le[32];
    uint8_t k_le[32];
    uint8_t hash_le[32];
    mbedtls_mpi k;
    Signature_P256 sig;
    hosal_crypto_ecc_p256_t op;

    if (grp == NULL || r == NULL || s == NULL || d == NULL || buf == NULL || f_rng == NULL)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }
    /* Reject d outside [1, N-1], as the software implementation does. */
    if ((ret = mbedtls_ecp_check_privkey(grp, d)) != 0)
    {
        return ret;
    }

    mbedtls_mpi_init(&k);

    if ((ret = mbedtls_mpi_write_binary_le(d, d_le, sizeof(d_le))) != 0)
    {
        goto cleanup;
    }
    hw_p256_hash_to_le(hash_le, buf, blen);

    for (attempts = 0; attempts < 10; attempts++)
    {
        /* Nonce k in [1, N-1] from f_rng — BEFORE taking the hw lock. */
        if ((ret = mbedtls_ecp_gen_privkey(grp, &k, f_rng, p_rng)) != 0)
        {
            goto cleanup;
        }
        if ((ret = mbedtls_mpi_write_binary_le(&k, k_le, sizeof(k_le))) != 0)
        {
            goto cleanup;
        }

        memset(&op, 0, sizeof(op));
        op.crypto_operation = HOSAL_ECDA_P256_SIGNATURE;
        op.signatrue        = &sig;
        op.p_hash           = (uint32_t *) hash_le;
        op.p_key            = (uint32_t *) d_le;
        op.p_k              = (uint32_t *) k_le;

        hw_crypto_lock();
        hosal_crypto_ecc_init(HOSAL_ECC_CURVE_P256_INIT);
        status = hosal_crypto_ecc_p256(&op);
        hw_crypto_unlock();

        if (status != 0)
        {
            ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
            goto cleanup;
        }

        if ((ret = mbedtls_mpi_read_binary_le(r, sig.r, sizeof(sig.r))) != 0)
        {
            goto cleanup;
        }
        if ((ret = mbedtls_mpi_read_binary_le(s, sig.s, sizeof(sig.s))) != 0)
        {
            goto cleanup;
        }

        /* r == 0 or s == 0 requires a new nonce (astronomically rare). */
        if (mbedtls_mpi_cmp_int(r, 0) != 0 && mbedtls_mpi_cmp_int(s, 0) != 0)
        {
            ret = 0;
            goto cleanup;
        }
    }

    ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;

cleanup:
    mbedtls_mpi_free(&k);
    mbedtls_platform_zeroize(d_le, sizeof(d_le));
    mbedtls_platform_zeroize(k_le, sizeof(k_le));
    mbedtls_platform_zeroize(&sig, sizeof(sig));
    return ret;
}
#endif /* MBEDTLS_ECDSA_SIGN_ALT */

#if defined(MBEDTLS_ECDSA_VERIFY_ALT)
/*
 * Verify an ECDSA signature of a hashed message.
 */
int mbedtls_ecdsa_verify(mbedtls_ecp_group * grp, const unsigned char * buf, size_t blen, const mbedtls_ecp_point * Q,
                         const mbedtls_mpi * r, const mbedtls_mpi * s)
{
    int ret;
    int status;
    uint8_t hash_le[32];
    ECPoint_P256 pub;
    Signature_P256 sig;
    hosal_crypto_ecc_p256_t op;

    if (grp == NULL || buf == NULL || Q == NULL || r == NULL || s == NULL)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    /* Step 1 of SEC1 4.1.4: r and s must be in [1, N-1]. */
    if (mbedtls_mpi_cmp_int(r, 1) < 0 || mbedtls_mpi_cmp_mpi(r, &grp->N) >= 0 || mbedtls_mpi_cmp_int(s, 1) < 0 ||
        mbedtls_mpi_cmp_mpi(s, &grp->N) >= 0)
    {
        return MBEDTLS_ERR_ECP_VERIFY_FAILED;
    }
    /* The public key must be a valid curve point. */
    if ((ret = mbedtls_ecp_check_pubkey(grp, Q)) != 0)
    {
        return ret;
    }

    if ((ret = hw_p256_point_from_ecp(&pub, Q)) != 0)
    {
        return ret;
    }
    if ((ret = mbedtls_mpi_write_binary_le(r, sig.r, sizeof(sig.r))) != 0)
    {
        return ret;
    }
    if ((ret = mbedtls_mpi_write_binary_le(s, sig.s, sizeof(sig.s))) != 0)
    {
        return ret;
    }
    hw_p256_hash_to_le(hash_le, buf, blen);

    memset(&op, 0, sizeof(op));
    op.crypto_operation = HOSAL_ECDA_P256_VERIFY;
    op.signatrue        = &sig;
    op.p_hash           = (uint32_t *) hash_le;
    op.base             = &pub;

    hw_crypto_lock();
    hosal_crypto_ecc_init(HOSAL_ECDA_CURVE_P256_VERIFY_INIT);
    status = hosal_crypto_ecc_p256(&op);
    hw_crypto_unlock();

    if (status != 0)
    {
        return MBEDTLS_ERR_ECP_VERIFY_FAILED;
    }

    return 0;
}
#endif /* MBEDTLS_ECDSA_VERIFY_ALT */

#endif /* MBEDTLS_ECDSA_SIGN_ALT || MBEDTLS_ECDSA_VERIFY_ALT */
