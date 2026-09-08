/*
 * ECDH for RT58x: MBEDTLS_ECDH_GEN_PUBLIC_ALT / MBEDTLS_ECDH_COMPUTE_SHARED_ALT.
 * The point multiplications run on the crypto engine (SECP256R1 only).
 * The private scalar is drawn from f_rng BEFORE any hardware call, following
 * the hw_crypto_lock discipline used by the other alt implementations.
 */

#include "mbedtls/ecdh.h"

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT) || defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)

#include <string.h>

#include "hw_ecc_p256.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT)
/*
 * Generate the keypair: d random in [1, N-1], Q = d * G.
 */
int mbedtls_ecdh_gen_public(mbedtls_ecp_group * grp, mbedtls_mpi * d, mbedtls_ecp_point * Q,
                            int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    int ret;
    uint8_t d_le[32];
    ECPoint_P256 base;
    ECPoint_P256 result;

    if (grp == NULL || d == NULL || Q == NULL || f_rng == NULL)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    /* Random private key via the software path (MPI only, no HW). */
    if ((ret = mbedtls_ecp_gen_privkey(grp, d, f_rng, p_rng)) != 0)
    {
        return ret;
    }

    if ((ret = mbedtls_mpi_write_binary_le(d, d_le, sizeof(d_le))) != 0)
    {
        goto cleanup;
    }
    if ((ret = hw_p256_point_from_ecp(&base, &grp->G)) != 0)
    {
        goto cleanup;
    }

    if (hw_p256_mul_le(&result, &base, d_le) != 0)
    {
        ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        goto cleanup;
    }

    ret = hw_p256_point_to_ecp(Q, &result);

cleanup:
    mbedtls_platform_zeroize(d_le, sizeof(d_le));
    return ret;
}
#endif /* MBEDTLS_ECDH_GEN_PUBLIC_ALT */

#if defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)
/*
 * Compute the shared secret z = (d * Q).x.
 */
int mbedtls_ecdh_compute_shared(mbedtls_ecp_group * grp, mbedtls_mpi * z, const mbedtls_ecp_point * Q, const mbedtls_mpi * d,
                                int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    int ret;
    uint8_t d_le[32];
    ECPoint_P256 base;
    ECPoint_P256 result;

    (void) f_rng; /* no blinding on the hardware path */
    (void) p_rng;

    if (grp == NULL || z == NULL || Q == NULL || d == NULL)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    /* Validate the peer public key (on curve, not infinity) and our
     * private key range before touching the hardware. */
    if ((ret = mbedtls_ecp_check_pubkey(grp, Q)) != 0)
    {
        return ret;
    }
    if ((ret = mbedtls_ecp_check_privkey(grp, d)) != 0)
    {
        return ret;
    }

    if ((ret = mbedtls_mpi_write_binary_le(d, d_le, sizeof(d_le))) != 0)
    {
        goto cleanup;
    }
    if ((ret = hw_p256_point_from_ecp(&base, Q)) != 0)
    {
        goto cleanup;
    }

    if (hw_p256_mul_le(&result, &base, d_le) != 0)
    {
        ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary_le(z, result.x, sizeof(result.x));

cleanup:
    mbedtls_platform_zeroize(d_le, sizeof(d_le));
    mbedtls_platform_zeroize(&result, sizeof(result));
    return ret;
}
#endif /* MBEDTLS_ECDH_COMPUTE_SHARED_ALT */

#endif /* MBEDTLS_ECDH_GEN_PUBLIC_ALT || MBEDTLS_ECDH_COMPUTE_SHARED_ALT */
