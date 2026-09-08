/*
 *  Elliptic curves over GF(p): generic functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * References:
 *
 * SEC1 https://www.secg.org/sec1-v2.pdf
 * GECC = Guide to Elliptic Curve Cryptography - Hankerson, Menezes, Vanstone
 * FIPS 186-3 http://csrc.nist.gov/publications/fips/fips186-3/fips_186-3.pdf
 * RFC 4492 for the related TLS structures and constants
 * - https://www.rfc-editor.org/rfc/rfc4492
 * RFC 7748 for the Curve448 and Curve25519 curve definitions
 * - https://www.rfc-editor.org/rfc/rfc7748
 *
 * [Curve25519] https://cr.yp.to/ecdh/curve25519-20060209.pdf
 *
 * [2] CORON, Jean-S'ebastien. Resistance against differential power analysis
 *     for elliptic curve cryptosystems. In : Cryptographic Hardware and
 *     Embedded Systems. Springer Berlin Heidelberg, 1999. p. 292-302.
 *     <http://link.springer.com/chapter/10.1007/3-540-48059-5_25>
 *
 * [3] HEDABOU, Mustapha, PINEL, Pierre, et B'EN'ETEAU, Lucien. A comb method to
 *     render ECC resistant against Side Channel Attacks. IACR Cryptology
 *     ePrint Archive, 2004, vol. 2004, p. 342.
 *     <http://eprint.iacr.org/2004/342.pdf>
 */

#include "common.h"

#if defined(MBEDTLS_ECP_C)

#include "mbedtls/bn_mul.h"
#include "mbedtls/ecp.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/threading.h"

#include "ecp_invasive.h"

#include <string.h>

/*
 * RT58x MBEDTLS_ECP_ALT implementation, forked from the upstream mbedTLS 2.28
 * library/ecp.c. Deviations from upstream:
 *
 *  - the MBEDTLS_ECP_ALT guard below is inverted (the library compiles the
 *    module out when the macro is defined, so the alt module provides it);
 *  - mbedtls_ecp_mul_restartable() and mbedtls_ecp_muladd_restartable()
 *    dispatch SECP256R1 to the crypto engine (hosal_crypto_ecc); everything
 *    else is the unmodified software implementation;
 *  - everything that rt58x-chip-mbedtls-config.h compiles out has been
 *    deleted, namely the Montgomery-curve arithmetic (no CURVE25519/CURVE448),
 *    the restartable/interruptible ECC machinery (no MBEDTLS_ECP_RESTARTABLE),
 *    the MBEDTLS_ECP_*_ALT function-level hooks (no MBEDTLS_ECP_INTERNAL_ALT)
 *    and the fixed-point comb cache (MBEDTLS_ECP_FIXED_POINT_OPTIM is 0).
 *
 * When resyncing with a newer upstream mbedTLS, diff against upstream's
 * short-Weierstrass paths rather than expecting a whole-file match.
 */
#if defined(MBEDTLS_ECP_ALT)

/* hw_p256_point_from_ecp() / hw_p256_point_to_ecp() / hw_p256_mul_le():
 * the MPI <-> engine conversion helpers shared with ecdh_alt.c and ecdsa_alt.c.
 * Pulls in hosal_crypto_ecc.h and hw_crypto_lock.h. */
#include "hw_ecc_p256.h"

/* Parameter validation macros based on platform_util.h */
#define ECP_VALIDATE_RET(cond) MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define ECP_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

#include "mbedtls/platform.h"

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
/* R = k * P on the crypto engine, mapping the engine status to an mbedtls
 * error. All buffers little-endian. Must be called with valid inputs only
 * (k in [1, N-1], P on curve): the engine cannot represent infinity. */
static int ecp_hw_p256_mul_le(ECPoint_P256 * result, const ECPoint_P256 * base, const uint8_t k_le[32])
{
    return (hw_p256_mul_le(result, base, k_le) == 0) ? 0 : MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
}

/* Hardware R = m * P for SECP256R1. Inputs already validated by the caller
 * (mbedtls_ecp_check_privkey/check_pubkey). */
static int ecp_hw_mul_p256(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P)
{
    int ret;
    uint8_t k_le[32];
    ECPoint_P256 base;
    ECPoint_P256 result;
    mbedtls_mpi mp1;

    /* The engine rejects k = 1 and k = N-1 by design (gfp_point_p256_mult
     * returns STATUS_INVALID_PARAM for them, on both RT582 and RT584), so
     * compute those here, like mbedtls_ecp_mul_shortcuts. Not constant
     * time, but for these k the result reveals k regardless. */
    if (mbedtls_mpi_cmp_int(m, 1) == 0)
    {
        return mbedtls_ecp_copy(R, P);
    }
    mbedtls_mpi_init(&mp1);
    ret = mbedtls_mpi_add_int(&mp1, m, 1);
    if (ret == 0 && mbedtls_mpi_cmp_mpi(&mp1, &grp->N) == 0)
    {
        /* m = N - 1: R = -P */
        ret = mbedtls_ecp_copy(R, P);
        if (ret == 0 && mbedtls_mpi_cmp_int(&R->Y, 0) != 0)
        {
            ret = mbedtls_mpi_sub_mpi(&R->Y, &grp->P, &R->Y);
        }
        mbedtls_mpi_free(&mp1);
        return ret;
    }
    mbedtls_mpi_free(&mp1);
    if (ret != 0)
    {
        return ret;
    }

    if ((ret = mbedtls_mpi_write_binary_le(m, k_le, sizeof(k_le))) != 0)
    {
        goto cleanup;
    }
    if ((ret = hw_p256_point_from_ecp(&base, P)) != 0)
    {
        goto cleanup;
    }
    if ((ret = ecp_hw_p256_mul_le(&result, &base, k_le)) != 0)
    {
        goto cleanup;
    }
    ret = hw_p256_point_to_ecp(R, &result);

cleanup:
    mbedtls_platform_zeroize(k_le, sizeof(k_le));
    return ret;
}

/* Hardware R = m * P + n * Q for SECP256R1. Returns
 * MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE when a corner case the affine engine
 * cannot handle is detected (caller falls back to software). */
static int ecp_hw_muladd_p256(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P,
                              const mbedtls_mpi * n, const mbedtls_ecp_point * Q)
{
    int ret;
    int status;
    uint8_t m_le[32];
    uint8_t n_le[32];
    ECPoint_P256 base_p, base_q, mp, nq, sum;
    hosal_crypto_ecc_p256_t op;

    /* The engine needs scalars in [1, N-1] and affine on-curve points; let
     * the software shortcuts handle 0 / +-1 and invalid input. */
    if (mbedtls_ecp_check_privkey(grp, m) != 0 || mbedtls_ecp_check_privkey(grp, n) != 0 || mbedtls_ecp_check_pubkey(grp, P) != 0 ||
        mbedtls_ecp_check_pubkey(grp, Q) != 0)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    if ((ret = mbedtls_mpi_write_binary_le(m, m_le, sizeof(m_le))) != 0)
    {
        goto cleanup;
    }
    if ((ret = mbedtls_mpi_write_binary_le(n, n_le, sizeof(n_le))) != 0)
    {
        goto cleanup;
    }
    if ((ret = hw_p256_point_from_ecp(&base_p, P)) != 0)
    {
        goto cleanup;
    }
    if ((ret = hw_p256_point_from_ecp(&base_q, Q)) != 0)
    {
        goto cleanup;
    }

    if ((ret = ecp_hw_p256_mul_le(&mp, &base_p, m_le)) != 0)
    {
        goto cleanup;
    }
    if ((ret = ecp_hw_p256_mul_le(&nq, &base_q, n_le)) != 0)
    {
        goto cleanup;
    }

    /* The affine point-add cannot double (mP == nQ) nor produce the point at
     * infinity (mP == -nQ). Both cases share the same x coordinate — fall
     * back to the software path there. */
    if (memcmp(mp.x, nq.x, sizeof(mp.x)) == 0)
    {
        ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
        goto cleanup;
    }

    memset(&op, 0, sizeof(op));
    op.crypto_operation = HOSAL_GFP_P256_ADD;
    op.p_point_result   = &sum;
    op.p_point_x1       = &mp;
    op.p_point_x2       = &nq;

    hw_crypto_lock();
    status = hosal_crypto_ecc_p256(&op);
    hw_crypto_unlock();
    if (status != 0)
    {
        ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        goto cleanup;
    }

    ret = hw_p256_point_to_ecp(R, &sum);

cleanup:
    mbedtls_platform_zeroize(m_le, sizeof(m_le));
    mbedtls_platform_zeroize(n_le, sizeof(n_le));
    return ret;
}
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */

#include "mbedtls/ecp_internal.h"

#if defined(MBEDTLS_HMAC_DRBG_C)
#include "mbedtls/hmac_drbg.h"
#elif defined(MBEDTLS_CTR_DRBG_C)
#include "mbedtls/ctr_drbg.h"
#else
#error "Invalid configuration detected. Include check_config.h to ensure that the configuration is valid."
#endif

#if defined(MBEDTLS_SELF_TEST)
/*
 * Counts of point addition and doubling, and field multiplications.
 * Used to test resistance of point multiplication to simple timing attacks.
 */
static unsigned long add_count, dbl_count, mul_count;
#endif

/*
 * Currently ecp_mul() takes a RNG function as an argument, used for
 * side-channel protection, but it can be NULL. The initial reasoning was
 * that people will pass non-NULL RNG when they care about side-channels, but
 * unfortunately we have some APIs that call ecp_mul() with a NULL RNG, with
 * no opportunity for the user to do anything about it.
 *
 * The obvious strategies for addressing that include:
 * - change those APIs so that they take RNG arguments;
 * - require a global RNG to be available to all crypto modules.
 *
 * Unfortunately those would break compatibility. So what we do instead is
 * have our own internal DRBG instance, seeded from the secret scalar.
 *
 * The following is a light-weight abstraction layer for doing that with
 * HMAC_DRBG (first choice) or CTR_DRBG.
 */

#if defined(MBEDTLS_HMAC_DRBG_C)

/* DRBG context type */
typedef mbedtls_hmac_drbg_context ecp_drbg_context;

/* DRBG context init */
static inline void ecp_drbg_init(ecp_drbg_context * ctx)
{
    mbedtls_hmac_drbg_init(ctx);
}

/* DRBG context free */
static inline void ecp_drbg_free(ecp_drbg_context * ctx)
{
    mbedtls_hmac_drbg_free(ctx);
}

/* DRBG function */
static inline int ecp_drbg_random(void * p_rng, unsigned char * output, size_t output_len)
{
    return mbedtls_hmac_drbg_random(p_rng, output, output_len);
}

/* DRBG context seeding */
static int ecp_drbg_seed(ecp_drbg_context * ctx, const mbedtls_mpi * secret, size_t secret_len)
{
    int ret;
    unsigned char secret_bytes[MBEDTLS_ECP_MAX_BYTES];
    /* The list starts with strong hashes */
    const mbedtls_md_type_t md_type   = (mbedtls_md_type_t) (mbedtls_md_list()[0]);
    const mbedtls_md_info_t * md_info = mbedtls_md_info_from_type(md_type);

    if (secret_len > MBEDTLS_ECP_MAX_BYTES)
    {
        ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(secret, secret_bytes, secret_len));

    ret = mbedtls_hmac_drbg_seed_buf(ctx, md_info, secret_bytes, secret_len);

cleanup:
    mbedtls_platform_zeroize(secret_bytes, secret_len);

    return ret;
}

#elif defined(MBEDTLS_CTR_DRBG_C)

/* DRBG context type */
typedef mbedtls_ctr_drbg_context ecp_drbg_context;

/* DRBG context init */
static inline void ecp_drbg_init(ecp_drbg_context * ctx)
{
    mbedtls_ctr_drbg_init(ctx);
}

/* DRBG context free */
static inline void ecp_drbg_free(ecp_drbg_context * ctx)
{
    mbedtls_ctr_drbg_free(ctx);
}

/* DRBG function */
static inline int ecp_drbg_random(void * p_rng, unsigned char * output, size_t output_len)
{
    return mbedtls_ctr_drbg_random(p_rng, output, output_len);
}

/*
 * Since CTR_DRBG doesn't have a seed_buf() function the way HMAC_DRBG does,
 * we need to pass an entropy function when seeding. So we use a dummy
 * function for that, and pass the actual entropy as customisation string.
 * (During seeding of CTR_DRBG the entropy input and customisation string are
 * concatenated before being used to update the secret state.)
 */
static int ecp_ctr_drbg_null_entropy(void * ctx, unsigned char * out, size_t len)
{
    (void) ctx;
    memset(out, 0, len);
    return 0;
}

/* DRBG context seeding */
static int ecp_drbg_seed(ecp_drbg_context * ctx, const mbedtls_mpi * secret, size_t secret_len)
{
    int ret;
    unsigned char secret_bytes[MBEDTLS_ECP_MAX_BYTES];

    if (secret_len > MBEDTLS_ECP_MAX_BYTES)
    {
        ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(secret, secret_bytes, secret_len));

    ret = mbedtls_ctr_drbg_seed(ctx, ecp_ctr_drbg_null_entropy, NULL, secret_bytes, secret_len);

cleanup:
    mbedtls_platform_zeroize(secret_bytes, secret_len);

    return ret;
}

#else
#error "Invalid configuration detected. Include check_config.h to ensure that the configuration is valid."
#endif /* DRBG modules */

/* The RT58x config does not define MBEDTLS_ECP_RESTARTABLE, so the
 * restart sub-contexts and budget accounting reduce to no-ops. */
#define ECP_RS_ENTER(sub) (void) rs_ctx;
#define ECP_RS_LEAVE(sub) (void) rs_ctx;

/*
 * List of supported curves:
 *  - internal ID
 *  - TLS NamedCurve ID (RFC 4492 sec. 5.1.1, RFC 7071 sec. 2, RFC 8446 sec. 4.2.7)
 *  - size in bits
 *  - readable name
 *
 * Curves are listed in order: largest curves first, and for a given size,
 * fastest curves first. This provides the default order for the SSL module.
 *
 * Reminder: update profiles in x509_crt.c when adding a new curves!
 */
static const mbedtls_ecp_curve_info ecp_supported_curves[] = {
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP256R1, 23, 256, "secp256r1" },
#endif
    { MBEDTLS_ECP_DP_NONE, 0, 0, NULL },
};

#define ECP_NB_CURVES sizeof(ecp_supported_curves) / sizeof(ecp_supported_curves[0])

static mbedtls_ecp_group_id ecp_supported_grp_id[ECP_NB_CURVES];

/*
 * List of supported curves and associated info
 */
const mbedtls_ecp_curve_info * mbedtls_ecp_curve_list(void)
{
    return ecp_supported_curves;
}

/*
 * List of supported curves, group ID only
 */
const mbedtls_ecp_group_id * mbedtls_ecp_grp_id_list(void)
{
    static int init_done = 0;

    if (!init_done)
    {
        size_t i = 0;
        const mbedtls_ecp_curve_info * curve_info;

        for (curve_info = mbedtls_ecp_curve_list(); curve_info->grp_id != MBEDTLS_ECP_DP_NONE; curve_info++)
        {
            ecp_supported_grp_id[i++] = curve_info->grp_id;
        }
        ecp_supported_grp_id[i] = MBEDTLS_ECP_DP_NONE;

        init_done = 1;
    }

    return ecp_supported_grp_id;
}

/*
 * Get the curve info for the internal identifier
 */
const mbedtls_ecp_curve_info * mbedtls_ecp_curve_info_from_grp_id(mbedtls_ecp_group_id grp_id)
{
    const mbedtls_ecp_curve_info * curve_info;

    for (curve_info = mbedtls_ecp_curve_list(); curve_info->grp_id != MBEDTLS_ECP_DP_NONE; curve_info++)
    {
        if (curve_info->grp_id == grp_id)
        {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the curve info from the TLS identifier
 */
const mbedtls_ecp_curve_info * mbedtls_ecp_curve_info_from_tls_id(uint16_t tls_id)
{
    const mbedtls_ecp_curve_info * curve_info;

    for (curve_info = mbedtls_ecp_curve_list(); curve_info->grp_id != MBEDTLS_ECP_DP_NONE; curve_info++)
    {
        if (curve_info->tls_id == tls_id)
        {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the curve info from the name
 */
const mbedtls_ecp_curve_info * mbedtls_ecp_curve_info_from_name(const char * name)
{
    const mbedtls_ecp_curve_info * curve_info;

    if (name == NULL)
    {
        return NULL;
    }

    for (curve_info = mbedtls_ecp_curve_list(); curve_info->grp_id != MBEDTLS_ECP_DP_NONE; curve_info++)
    {
        if (strcmp(curve_info->name, name) == 0)
        {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the type of a curve
 */
mbedtls_ecp_curve_type mbedtls_ecp_get_type(const mbedtls_ecp_group * grp)
{
    if (grp->G.X.p == NULL)
    {
        return MBEDTLS_ECP_TYPE_NONE;
    }

    if (grp->G.Y.p == NULL)
    {
        return MBEDTLS_ECP_TYPE_MONTGOMERY;
    }
    else
    {
        return MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS;
    }
}

/*
 * Initialize (the components of) a point
 */
void mbedtls_ecp_point_init(mbedtls_ecp_point * pt)
{
    ECP_VALIDATE(pt != NULL);

    mbedtls_mpi_init(&pt->X);
    mbedtls_mpi_init(&pt->Y);
    mbedtls_mpi_init(&pt->Z);
}

/*
 * Initialize (the components of) a group
 */
void mbedtls_ecp_group_init(mbedtls_ecp_group * grp)
{
    ECP_VALIDATE(grp != NULL);

    grp->id = MBEDTLS_ECP_DP_NONE;
    mbedtls_mpi_init(&grp->P);
    mbedtls_mpi_init(&grp->A);
    mbedtls_mpi_init(&grp->B);
    mbedtls_ecp_point_init(&grp->G);
    mbedtls_mpi_init(&grp->N);
    grp->pbits  = 0;
    grp->nbits  = 0;
    grp->h      = 0;
    grp->modp   = NULL;
    grp->t_pre  = NULL;
    grp->t_post = NULL;
    grp->t_data = NULL;
    grp->T      = NULL;
    grp->T_size = 0;
}

/*
 * Initialize (the components of) a key pair
 */
void mbedtls_ecp_keypair_init(mbedtls_ecp_keypair * key)
{
    ECP_VALIDATE(key != NULL);

    mbedtls_ecp_group_init(&key->grp);
    mbedtls_mpi_init(&key->d);
    mbedtls_ecp_point_init(&key->Q);
}

/*
 * Unallocate (the components of) a point
 */
void mbedtls_ecp_point_free(mbedtls_ecp_point * pt)
{
    if (pt == NULL)
    {
        return;
    }

    mbedtls_mpi_free(&(pt->X));
    mbedtls_mpi_free(&(pt->Y));
    mbedtls_mpi_free(&(pt->Z));
}

/*
 * Unallocate (the components of) a group
 */
void mbedtls_ecp_group_free(mbedtls_ecp_group * grp)
{
    size_t i;

    if (grp == NULL)
    {
        return;
    }

    if (grp->h != 1)
    {
        mbedtls_mpi_free(&grp->P);
        mbedtls_mpi_free(&grp->A);
        mbedtls_mpi_free(&grp->B);
        mbedtls_ecp_point_free(&grp->G);
        mbedtls_mpi_free(&grp->N);
    }

    if (grp->T != NULL)
    {
        for (i = 0; i < grp->T_size; i++)
        {
            mbedtls_ecp_point_free(&grp->T[i]);
        }
        mbedtls_free(grp->T);
    }

    mbedtls_platform_zeroize(grp, sizeof(mbedtls_ecp_group));
}

/*
 * Unallocate (the components of) a key pair
 */
void mbedtls_ecp_keypair_free(mbedtls_ecp_keypair * key)
{
    if (key == NULL)
    {
        return;
    }

    mbedtls_ecp_group_free(&key->grp);
    mbedtls_mpi_free(&key->d);
    mbedtls_ecp_point_free(&key->Q);
}

/*
 * Copy the contents of a point
 */
int mbedtls_ecp_copy(mbedtls_ecp_point * P, const mbedtls_ecp_point * Q)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->X, &Q->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->Y, &Q->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->Z, &Q->Z));

cleanup:
    return ret;
}

/*
 * Copy the contents of a group object
 */
int mbedtls_ecp_group_copy(mbedtls_ecp_group * dst, const mbedtls_ecp_group * src)
{
    ECP_VALIDATE_RET(dst != NULL);
    ECP_VALIDATE_RET(src != NULL);

    return mbedtls_ecp_group_load(dst, src->id);
}

/*
 * Set point to zero
 */
int mbedtls_ecp_set_zero(mbedtls_ecp_point * pt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(pt != NULL);

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->X, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Y, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 0));

cleanup:
    return ret;
}

/*
 * Tell if a point is zero
 */
int mbedtls_ecp_is_zero(mbedtls_ecp_point * pt)
{
    ECP_VALIDATE_RET(pt != NULL);

    return mbedtls_mpi_cmp_int(&pt->Z, 0) == 0;
}

/*
 * Compare two points lazily
 */
int mbedtls_ecp_point_cmp(const mbedtls_ecp_point * P, const mbedtls_ecp_point * Q)
{
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    if (mbedtls_mpi_cmp_mpi(&P->X, &Q->X) == 0 && mbedtls_mpi_cmp_mpi(&P->Y, &Q->Y) == 0 && mbedtls_mpi_cmp_mpi(&P->Z, &Q->Z) == 0)
    {
        return 0;
    }

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

/*
 * Import a non-zero point from ASCII strings
 */
int mbedtls_ecp_point_read_string(mbedtls_ecp_point * P, int radix, const char * x, const char * y)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(x != NULL);
    ECP_VALIDATE_RET(y != NULL);

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->X, radix, x));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->Y, radix, y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&P->Z, 1));

cleanup:
    return ret;
}

/*
 * Export a point into unsigned binary data (SEC1 2.3.3 and RFC7748)
 */
int mbedtls_ecp_point_write_binary(const mbedtls_ecp_group * grp, const mbedtls_ecp_point * P, int format, size_t * olen,
                                   unsigned char * buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    size_t plen;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(olen != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(format == MBEDTLS_ECP_PF_UNCOMPRESSED || format == MBEDTLS_ECP_PF_COMPRESSED);

    plen = mbedtls_mpi_size(&grp->P);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        /*
         * Common case: P == 0
         */
        if (mbedtls_mpi_cmp_int(&P->Z, 0) == 0)
        {
            if (buflen < 1)
            {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x00;
            *olen  = 1;

            return 0;
        }

        if (format == MBEDTLS_ECP_PF_UNCOMPRESSED)
        {
            *olen = 2 * plen + 1;

            if (buflen < *olen)
            {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x04;
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->X, buf + 1, plen));
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->Y, buf + 1 + plen, plen));
        }
        else if (format == MBEDTLS_ECP_PF_COMPRESSED)
        {
            *olen = plen + 1;

            if (buflen < *olen)
            {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x02 + mbedtls_mpi_get_bit(&P->Y, 0);
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->X, buf + 1, plen));
        }
    }
#endif

cleanup:
    return ret;
}

/*
 * Import a point from unsigned binary data (SEC1 2.3.4 and RFC7748)
 */
int mbedtls_ecp_point_read_binary(const mbedtls_ecp_group * grp, mbedtls_ecp_point * pt, const unsigned char * buf, size_t ilen)
{
    int ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    size_t plen;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(ilen == 0 || buf != NULL);

    if (ilen < 1)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    plen = mbedtls_mpi_size(&grp->P);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        if (buf[0] == 0x00)
        {
            if (ilen == 1)
            {
                return mbedtls_ecp_set_zero(pt);
            }
            else
            {
                return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            }
        }

        if (buf[0] != 0x04)
        {
            return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
        }

        if (ilen != 2 * plen + 1)
        {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pt->X, buf + 1, plen));
        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pt->Y, buf + 1 + plen, plen));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 1));
    }
#endif

cleanup:
    return ret;
}

/*
 * Import a point from a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_read_point(const mbedtls_ecp_group * grp, mbedtls_ecp_point * pt, const unsigned char ** buf, size_t buf_len)
{
    unsigned char data_len;
    const unsigned char * buf_start;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(buf_len == 0 || *buf != NULL);

    /*
     * We must have at least two bytes (1 for length, at least one for data)
     */
    if (buf_len < 2)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    data_len = *(*buf)++;
    if (data_len < 1 || data_len > buf_len - 1)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * Save buffer start for read_binary and update buf
     */
    buf_start = *buf;
    *buf += data_len;

    return mbedtls_ecp_point_read_binary(grp, pt, buf_start, data_len);
}

/*
 * Export a point as a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_write_point(const mbedtls_ecp_group * grp, const mbedtls_ecp_point * pt, int format, size_t * olen,
                                unsigned char * buf, size_t blen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);
    ECP_VALIDATE_RET(olen != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(format == MBEDTLS_ECP_PF_UNCOMPRESSED || format == MBEDTLS_ECP_PF_COMPRESSED);

    /*
     * buffer length must be at least one, for our length byte
     */
    if (blen < 1)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if ((ret = mbedtls_ecp_point_write_binary(grp, pt, format, olen, buf + 1, blen - 1)) != 0)
    {
        return ret;
    }

    /*
     * write length to the first byte and update total length
     */
    buf[0] = (unsigned char) *olen;
    ++*olen;

    return 0;
}

/*
 * Set a group from an ECParameters record (RFC 4492)
 */
int mbedtls_ecp_tls_read_group(mbedtls_ecp_group * grp, const unsigned char ** buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group_id grp_id;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(len == 0 || *buf != NULL);

    if ((ret = mbedtls_ecp_tls_read_group_id(&grp_id, buf, len)) != 0)
    {
        return ret;
    }

    return mbedtls_ecp_group_load(grp, grp_id);
}

/*
 * Read a group id from an ECParameters record (RFC 4492) and convert it to
 * mbedtls_ecp_group_id.
 */
int mbedtls_ecp_tls_read_group_id(mbedtls_ecp_group_id * grp, const unsigned char ** buf, size_t len)
{
    uint16_t tls_id;
    const mbedtls_ecp_curve_info * curve_info;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(len == 0 || *buf != NULL);

    /*
     * We expect at least three bytes (see below)
     */
    if (len < 3)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * First byte is curve_type; only named_curve is handled
     */
    if (*(*buf)++ != MBEDTLS_ECP_TLS_NAMED_CURVE)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * Next two bytes are the namedcurve value
     */
    tls_id = *(*buf)++;
    tls_id <<= 8;
    tls_id |= *(*buf)++;

    if ((curve_info = mbedtls_ecp_curve_info_from_tls_id(tls_id)) == NULL)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    *grp = curve_info->grp_id;

    return 0;
}

/*
 * Write the ECParameters record corresponding to a group (RFC 4492)
 */
int mbedtls_ecp_tls_write_group(const mbedtls_ecp_group * grp, size_t * olen, unsigned char * buf, size_t blen)
{
    const mbedtls_ecp_curve_info * curve_info;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(buf != NULL);
    ECP_VALIDATE_RET(olen != NULL);

    if ((curve_info = mbedtls_ecp_curve_info_from_grp_id(grp->id)) == NULL)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * We are going to write 3 bytes (see below)
     */
    *olen = 3;
    if (blen < *olen)
    {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    /*
     * First byte is curve_type, always named_curve
     */
    *buf++ = MBEDTLS_ECP_TLS_NAMED_CURVE;

    /*
     * Next two bytes are the namedcurve value
     */
    MBEDTLS_PUT_UINT16_BE(curve_info->tls_id, buf, 0);

    return 0;
}

/*
 * Wrapper around fast quasi-modp functions, with fall-back to mbedtls_mpi_mod_mpi.
 * See the documentation of struct mbedtls_ecp_group.
 *
 * This function is in the critial loop for mbedtls_ecp_mul, so pay attention to perf.
 */
static int ecp_modp(mbedtls_mpi * N, const mbedtls_ecp_group * grp)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (grp->modp == NULL)
    {
        return mbedtls_mpi_mod_mpi(N, N, &grp->P);
    }

    /* N->s < 0 is a much faster test, which fails only if N is 0 */
    if ((N->s < 0 && mbedtls_mpi_cmp_int(N, 0) != 0) || mbedtls_mpi_bitlen(N) > 2 * grp->pbits)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    MBEDTLS_MPI_CHK(grp->modp(N));

    /* N->s < 0 is a much faster test, which fails only if N is 0 */
    while (N->s < 0 && mbedtls_mpi_cmp_int(N, 0) != 0)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(N, N, &grp->P));
    }

    while (mbedtls_mpi_cmp_mpi(N, &grp->P) >= 0)
    {
        /* we known P, N and the result are positive */
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(N, N, &grp->P));
    }

cleanup:
    return ret;
}

/*
 * Fast mod-p functions expect their argument to be in the 0..p^2 range.
 *
 * In order to guarantee that, we need to ensure that operands of
 * mbedtls_mpi_mul_mpi are in the 0..p range. So, after each operation we will
 * bring the result back to this range.
 *
 * The following macros are shortcuts for doing that.
 */

/*
 * Reduce a mbedtls_mpi mod p in-place, general case, to use after mbedtls_mpi_mul_mpi
 */
#if defined(MBEDTLS_SELF_TEST)
#define INC_MUL_COUNT mul_count++;
#else
#define INC_MUL_COUNT
#endif

#define MOD_MUL(N)                                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        MBEDTLS_MPI_CHK(ecp_modp(&(N), grp));                                                                                      \
        INC_MUL_COUNT                                                                                                              \
    } while (0)

static inline int mbedtls_mpi_mul_mod(const mbedtls_ecp_group * grp, mbedtls_mpi * X, const mbedtls_mpi * A, const mbedtls_mpi * B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(X, A, B));
    MOD_MUL(*X);
cleanup:
    return ret;
}

/*
 * Reduce a mbedtls_mpi mod p in-place, to use after mbedtls_mpi_sub_mpi
 * N->s < 0 is a very fast test, which fails only if N is 0
 */
#define MOD_SUB(N)                                                                                                                 \
    while ((N).s < 0 && mbedtls_mpi_cmp_int(&(N), 0) != 0)                                                                         \
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&(N), &(N), &grp->P))

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
static inline int mbedtls_mpi_sub_mod(const mbedtls_ecp_group * grp, mbedtls_mpi * X, const mbedtls_mpi * A, const mbedtls_mpi * B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(X, A, B));
    MOD_SUB(*X);
cleanup:
    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Reduce a mbedtls_mpi mod p in-place, to use after mbedtls_mpi_add_mpi and mbedtls_mpi_mul_int.
 * We known P, N and the result are positive, so sub_abs is correct, and
 * a bit faster.
 */
#define MOD_ADD(N)                                                                                                                 \
    while (mbedtls_mpi_cmp_mpi(&(N), &grp->P) >= 0)                                                                                \
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(&(N), &(N), &grp->P))

static inline int mbedtls_mpi_add_mod(const mbedtls_ecp_group * grp, mbedtls_mpi * X, const mbedtls_mpi * A, const mbedtls_mpi * B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(X, A, B));
    MOD_ADD(*X);
cleanup:
    return ret;
}

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
static inline int mbedtls_mpi_shift_l_mod(const mbedtls_ecp_group * grp, mbedtls_mpi * X, size_t count)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(X, count));
    MOD_ADD(*X);
cleanup:
    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * For curves in short Weierstrass form, we do all the internal operations in
 * Jacobian coordinates.
 *
 * For multiplication, we'll use a comb method with countermeasures against
 * SPA, hence timing attacks.
 */

/*
 * Normalize jacobian coordinates so that Z == 0 || Z == 1  (GECC 3.2.1)
 * Cost: 1N := 1I + 3M + 1S
 */
static int ecp_normalize_jac(const mbedtls_ecp_group * grp, mbedtls_ecp_point * pt)
{
    if (mbedtls_mpi_cmp_int(&pt->Z, 0) == 0)
    {
        return 0;
    }

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi Zi, ZZi;
    mbedtls_mpi_init(&Zi);
    mbedtls_mpi_init(&ZZi);

    /*
     * X = X / Z^2  mod p
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&Zi, &pt->Z, &grp->P));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &ZZi, &Zi, &Zi));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->X, &pt->X, &ZZi));

    /*
     * Y = Y / Z^3  mod p
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->Y, &pt->Y, &ZZi));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->Y, &pt->Y, &Zi));

    /*
     * Z = 1
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 1));

cleanup:

    mbedtls_mpi_free(&Zi);
    mbedtls_mpi_free(&ZZi);

    return ret;
}

/*
 * Normalize jacobian coordinates of an array of (pointers to) points,
 * using Montgomery's trick to perform only one inversion mod P.
 * (See for example Cohen's "A Course in Computational Algebraic Number
 * Theory", Algorithm 10.3.4.)
 *
 * Warning: fails (returning an error) if one of the points is zero!
 * This should never happen, see choice of w in ecp_mul_comb().
 *
 * Cost: 1N(t) := 1I + (6t - 3)M + 1S
 */
static int ecp_normalize_jac_many(const mbedtls_ecp_group * grp, mbedtls_ecp_point * T[], size_t T_size)
{
    if (T_size < 2)
    {
        return ecp_normalize_jac(grp, *T);
    }

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i;
    mbedtls_mpi *c, u, Zi, ZZi;

    if ((c = mbedtls_calloc(T_size, sizeof(mbedtls_mpi))) == NULL)
    {
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }

    for (i = 0; i < T_size; i++)
    {
        mbedtls_mpi_init(&c[i]);
    }

    mbedtls_mpi_init(&u);
    mbedtls_mpi_init(&Zi);
    mbedtls_mpi_init(&ZZi);

    /*
     * c[i] = Z_0 * ... * Z_i
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&c[0], &T[0]->Z));
    for (i = 1; i < T_size; i++)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &c[i], &c[i - 1], &T[i]->Z));
    }

    /*
     * u = 1 / (Z_0 * ... * Z_n) mod P
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&u, &c[T_size - 1], &grp->P));

    for (i = T_size - 1;; i--)
    {
        /*
         * Zi = 1 / Z_i mod p
         * u = 1 / (Z_0 * ... * Z_i) mod P
         */
        if (i == 0)
        {
            MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&Zi, &u));
        }
        else
        {
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &Zi, &u, &c[i - 1]));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &u, &u, &T[i]->Z));
        }

        /*
         * proceed as in normalize()
         */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &ZZi, &Zi, &Zi));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T[i]->X, &T[i]->X, &ZZi));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T[i]->Y, &T[i]->Y, &ZZi));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T[i]->Y, &T[i]->Y, &Zi));

        /*
         * Post-precessing: reclaim some memory by shrinking coordinates
         * - not storing Z (always 1)
         * - shrinking other coordinates, but still keeping the same number of
         *   limbs as P, as otherwise it will too likely be regrown too fast.
         */
        MBEDTLS_MPI_CHK(mbedtls_mpi_shrink(&T[i]->X, grp->P.n));
        MBEDTLS_MPI_CHK(mbedtls_mpi_shrink(&T[i]->Y, grp->P.n));
        mbedtls_mpi_free(&T[i]->Z);

        if (i == 0)
        {
            break;
        }
    }

cleanup:

    mbedtls_mpi_free(&u);
    mbedtls_mpi_free(&Zi);
    mbedtls_mpi_free(&ZZi);
    for (i = 0; i < T_size; i++)
    {
        mbedtls_mpi_free(&c[i]);
    }
    mbedtls_free(c);

    return ret;
}

/*
 * Conditional point inversion: Q -> -Q = (Q.X, -Q.Y, Q.Z) without leak.
 * "inv" must be 0 (don't invert) or 1 (invert) or the result will be invalid
 */
static int ecp_safe_invert_jac(const mbedtls_ecp_group * grp, mbedtls_ecp_point * Q, unsigned char inv)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char nonzero;
    mbedtls_mpi mQY;

    mbedtls_mpi_init(&mQY);

    /* Use the fact that -Q.Y mod P = P - Q.Y unless Q.Y == 0 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&mQY, &grp->P, &Q->Y));
    nonzero = mbedtls_mpi_cmp_int(&Q->Y, 0) != 0;
    MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign(&Q->Y, &mQY, inv & nonzero));

cleanup:
    mbedtls_mpi_free(&mQY);

    return ret;
}

/*
 * Point doubling R = 2 P, Jacobian coordinates
 *
 * Based on http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#doubling-dbl-1998-cmo-2 .
 *
 * We follow the variable naming fairly closely. The formula variations that trade a MUL for a SQR
 * (plus a few ADDs) aren't useful as our bignum implementation doesn't distinguish squaring.
 *
 * Standard optimizations are applied when curve parameter A is one of { 0, -3 }.
 *
 * Cost: 1D := 3M + 4S          (A ==  0)
 *             4M + 4S          (A == -3)
 *             3M + 6S + 1a     otherwise
 */
static int ecp_double_jac(const mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_ecp_point * P)
{
#if defined(MBEDTLS_SELF_TEST)
    dbl_count++;
#endif

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi M, S, T, U;

    mbedtls_mpi_init(&M);
    mbedtls_mpi_init(&S);
    mbedtls_mpi_init(&T);
    mbedtls_mpi_init(&U);

    /* Special case for A = -3 */
    if (grp->A.p == NULL)
    {
        /* M = 3(X + Z^2)(X - Z^2) */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &P->Z, &P->Z));
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mod(grp, &T, &P->X, &S));
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &U, &P->X, &S));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &T, &U));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(&M, &S, 3));
        MOD_ADD(M);
    }
    else
    {
        /* M = 3.X^2 */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &P->X, &P->X));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(&M, &S, 3));
        MOD_ADD(M);

        /* Optimize away for "koblitz" curves with A = 0 */
        if (mbedtls_mpi_cmp_int(&grp->A, 0) != 0)
        {
            /* M += A.Z^4 */
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &P->Z, &P->Z));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T, &S, &S));
            MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &T, &grp->A));
            MBEDTLS_MPI_CHK(mbedtls_mpi_add_mod(grp, &M, &M, &S));
        }
    }

    /* S = 4.X.Y^2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T, &P->Y, &P->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, &T, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &P->X, &T));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, &S, 1));

    /* U = 8.Y^4 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &U, &T, &T));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, &U, 1));

    /* T = M^2 - 2.S */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T, &M, &M));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &T, &T, &S));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &T, &T, &S));

    /* S = M(S - T) - U */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &S, &S, &T));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &S, &S, &M));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &S, &S, &U));

    /* U = 2.Y.Z */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &U, &P->Y, &P->Z));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, &U, 1));

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->X, &T));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->Y, &S));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->Z, &U));

cleanup:
    mbedtls_mpi_free(&M);
    mbedtls_mpi_free(&S);
    mbedtls_mpi_free(&T);
    mbedtls_mpi_free(&U);

    return ret;
}

/*
 * Addition: R = P + Q, mixed affine-Jacobian coordinates (GECC 3.22)
 *
 * The coordinates of Q must be normalized (= affine),
 * but those of P don't need to. R is not normalized.
 *
 * Special cases: (1) P or Q is zero, (2) R is zero, (3) P == Q.
 * None of these cases can happen as intermediate step in ecp_mul_comb():
 * - at each step, P, Q and R are multiples of the base point, the factor
 *   being less than its order, so none of them is zero;
 * - Q is an odd multiple of the base point, P an even multiple,
 *   due to the choice of precomputed points in the modified comb method.
 * So branches for these cases do not leak secret information.
 *
 * We accept Q->Z being unset (saving memory in tables) as meaning 1.
 *
 * Cost: 1A := 8M + 3S
 */
static int ecp_add_mixed(const mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_ecp_point * P,
                         const mbedtls_ecp_point * Q)
{
#if defined(MBEDTLS_SELF_TEST)
    add_count++;
#endif

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi T1, T2, T3, T4, X, Y, Z;

    /*
     * Trivial cases: P == 0 or Q == 0 (case 1)
     */
    if (mbedtls_mpi_cmp_int(&P->Z, 0) == 0)
    {
        return mbedtls_ecp_copy(R, Q);
    }

    if (Q->Z.p != NULL && mbedtls_mpi_cmp_int(&Q->Z, 0) == 0)
    {
        return mbedtls_ecp_copy(R, P);
    }

    /*
     * Make sure Q coordinates are normalized
     */
    if (Q->Z.p != NULL && mbedtls_mpi_cmp_int(&Q->Z, 1) != 0)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init(&T1);
    mbedtls_mpi_init(&T2);
    mbedtls_mpi_init(&T3);
    mbedtls_mpi_init(&T4);
    mbedtls_mpi_init(&X);
    mbedtls_mpi_init(&Y);
    mbedtls_mpi_init(&Z);

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T1, &P->Z, &P->Z));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T2, &T1, &P->Z));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T1, &T1, &Q->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T2, &T2, &Q->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &T1, &T1, &P->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &T2, &T2, &P->Y));

    /* Special cases (2) and (3) */
    if (mbedtls_mpi_cmp_int(&T1, 0) == 0)
    {
        if (mbedtls_mpi_cmp_int(&T2, 0) == 0)
        {
            ret = ecp_double_jac(grp, R, P);
            goto cleanup;
        }
        else
        {
            ret = mbedtls_ecp_set_zero(R);
            goto cleanup;
        }
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &Z, &P->Z, &T1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T3, &T1, &T1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T4, &T3, &T1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T3, &T3, &P->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&T1, &T3));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, &T1, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &X, &T2, &T2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &X, &X, &T1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &X, &X, &T4));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &T3, &T3, &X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T3, &T3, &T2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &T4, &T4, &P->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, &Y, &T3, &T4));

    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->X, &X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->Y, &Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&R->Z, &Z));

cleanup:

    mbedtls_mpi_free(&T1);
    mbedtls_mpi_free(&T2);
    mbedtls_mpi_free(&T3);
    mbedtls_mpi_free(&T4);
    mbedtls_mpi_free(&X);
    mbedtls_mpi_free(&Y);
    mbedtls_mpi_free(&Z);

    return ret;
}

/*
 * Randomize jacobian coordinates:
 * (X, Y, Z) -> (l^2 X, l^3 Y, l Z) for random l
 * This is sort of the reverse operation of ecp_normalize_jac().
 *
 * This countermeasure was first suggested in [2].
 */
static int ecp_randomize_jac(const mbedtls_ecp_group * grp, mbedtls_ecp_point * pt, int (*f_rng)(void *, unsigned char *, size_t),
                             void * p_rng)
{

    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi l, ll;

    mbedtls_mpi_init(&l);
    mbedtls_mpi_init(&ll);

    /* Generate l such that 1 < l < p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_random(&l, 2, &grp->P, f_rng, p_rng));

    /* Z = l * Z */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->Z, &pt->Z, &l));

    /* X = l^2 * X */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &ll, &l, &l));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->X, &pt->X, &ll));

    /* Y = l^3 * Y */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &ll, &ll, &l));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &pt->Y, &pt->Y, &ll));

cleanup:
    mbedtls_mpi_free(&l);
    mbedtls_mpi_free(&ll);

    if (ret == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE)
    {
        ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
    }
    return ret;
}

/*
 * Check and define parameters used by the comb method (see below for details)
 */
#if MBEDTLS_ECP_WINDOW_SIZE < 2 || MBEDTLS_ECP_WINDOW_SIZE > 7
#error "MBEDTLS_ECP_WINDOW_SIZE out of bounds"
#endif

/* d = ceil( n / w ) */
#define COMB_MAX_D (MBEDTLS_ECP_MAX_BITS + 1) / 2

/* number of precomputed points */
#define COMB_MAX_PRE (1 << (MBEDTLS_ECP_WINDOW_SIZE - 1))

/*
 * Compute the representation of m that will be used with our comb method.
 *
 * The basic comb method is described in GECC 3.44 for example. We use a
 * modified version that provides resistance to SPA by avoiding zero
 * digits in the representation as in [3]. We modify the method further by
 * requiring that all K_i be odd, which has the small cost that our
 * representation uses one more K_i, due to carries, but saves on the size of
 * the precomputed table.
 *
 * Summary of the comb method and its modifications:
 *
 * - The goal is to compute m*P for some w*d-bit integer m.
 *
 * - The basic comb method splits m into the w-bit integers
 *   x[0] .. x[d-1] where x[i] consists of the bits in m whose
 *   index has residue i modulo d, and computes m * P as
 *   S[x[0]] + 2 * S[x[1]] + .. + 2^(d-1) S[x[d-1]], where
 *   S[i_{w-1} .. i_0] := i_{w-1} 2^{(w-1)d} P + ... + i_1 2^d P + i_0 P.
 *
 * - If it happens that, say, x[i+1]=0 (=> S[x[i+1]]=0), one can replace the sum by
 *    .. + 2^{i-1} S[x[i-1]] - 2^i S[x[i]] + 2^{i+1} S[x[i]] + 2^{i+2} S[x[i+2]] ..,
 *   thereby successively converting it into a form where all summands
 *   are nonzero, at the cost of negative summands. This is the basic idea of [3].
 *
 * - More generally, even if x[i+1] != 0, we can first transform the sum as
 *   .. - 2^i S[x[i]] + 2^{i+1} ( S[x[i]] + S[x[i+1]] ) + 2^{i+2} S[x[i+2]] ..,
 *   and then replace S[x[i]] + S[x[i+1]] = S[x[i] ^ x[i+1]] + 2 S[x[i] & x[i+1]].
 *   Performing and iterating this procedure for those x[i] that are even
 *   (keeping track of carry), we can transform the original sum into one of the form
 *   S[x'[0]] +- 2 S[x'[1]] +- .. +- 2^{d-1} S[x'[d-1]] + 2^d S[x'[d]]
 *   with all x'[i] odd. It is therefore only necessary to know S at odd indices,
 *   which is why we are only computing half of it in the first place in
 *   ecp_precompute_comb and accessing it with index abs(i) / 2 in ecp_select_comb.
 *
 * - For the sake of compactness, only the seven low-order bits of x[i]
 *   are used to represent its absolute value (K_i in the paper), and the msb
 *   of x[i] encodes the sign (s_i in the paper): it is set if and only if
 *   if s_i == -1;
 *
 * Calling conventions:
 * - x is an array of size d + 1
 * - w is the size, ie number of teeth, of the comb, and must be between
 *   2 and 7 (in practice, between 2 and MBEDTLS_ECP_WINDOW_SIZE)
 * - m is the MPI, expected to be odd and such that bitlength(m) <= w * d
 *   (the result will be incorrect if these assumptions are not satisfied)
 */
static void ecp_comb_recode_core(unsigned char x[], size_t d, unsigned char w, const mbedtls_mpi * m)
{
    size_t i, j;
    unsigned char c, cc, adjust;

    memset(x, 0, d + 1);

    /* First get the classical comb values (except for x_d = 0) */
    for (i = 0; i < d; i++)
    {
        for (j = 0; j < w; j++)
        {
            x[i] |= mbedtls_mpi_get_bit(m, i + d * j) << j;
        }
    }

    /* Now make sure x_1 .. x_d are odd */
    c = 0;
    for (i = 1; i <= d; i++)
    {
        /* Add carry and update it */
        cc   = x[i] & c;
        x[i] = x[i] ^ c;
        c    = cc;

        /* Adjust if needed, avoiding branches */
        adjust = 1 - (x[i] & 0x01);
        c |= x[i] & (x[i - 1] * adjust);
        x[i] = x[i] ^ (x[i - 1] * adjust);
        x[i - 1] |= adjust << 7;
    }
}

/*
 * Precompute points for the adapted comb method
 *
 * Assumption: T must be able to hold 2^{w - 1} elements.
 *
 * Operation: If i = i_{w-1} ... i_1 is the binary representation of i,
 *            sets T[i] = i_{w-1} 2^{(w-1)d} P + ... + i_1 2^d P + P.
 *
 * Cost: d(w-1) D + (2^{w-1} - 1) A + 1 N(w-1) + 1 N(2^{w-1} - 1)
 *
 * Note: Even comb values (those where P would be omitted from the
 *       sum defining T[i] above) are not needed in our adaption
 *       the comb method. See ecp_comb_recode_core().
 *
 * This function currently works in four steps:
 * (1) [dbl]      Computation of intermediate T[i] for 2-power values of i
 * (2) [norm_dbl] Normalization of coordinates of these T[i]
 * (3) [add]      Computation of all T[i]
 * (4) [norm_add] Normalization of all T[i]
 *
 * Step 1 can be interrupted but not the others; together with the final
 * coordinate normalization they are the largest steps done at once, depending
 * on the window size. Here are operation counts for P-256:
 *
 * step     (2)     (3)     (4)
 * w = 5    142     165     208
 * w = 4    136      77     160
 * w = 3    130      33     136
 * w = 2    124      11     124
 *
 * So if ECC operations are blocking for too long even with a low max_ops
 * value, it's useful to set MBEDTLS_ECP_WINDOW_SIZE to a lower value in order
 * to minimize maximum blocking time.
 */
static int ecp_precompute_comb(const mbedtls_ecp_group * grp, mbedtls_ecp_point T[], const mbedtls_ecp_point * P, unsigned char w,
                               size_t d, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    size_t j                   = 0;
    const unsigned char T_size = 1U << (w - 1);
    mbedtls_ecp_point *cur, *TT[COMB_MAX_PRE - 1];

    (void) rs_ctx;

    /*
     * Set T[0] = P and
     * T[2^{l-1}] = 2^{dl} P for l = 1 .. w-1 (this is not the final value)
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_copy(&T[0], P));

    j = 0;

    for (; j < d * (w - 1); j++)
    {
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_DBL);

        i   = 1U << (j / d);
        cur = T + i;

        if (j % d == 0)
        {
            MBEDTLS_MPI_CHK(mbedtls_ecp_copy(cur, T + (i >> 1)));
        }

        MBEDTLS_MPI_CHK(ecp_double_jac(grp, cur, cur));
    }

    /*
     * Normalize current elements in T. As T has holes,
     * use an auxiliary array of pointers to elements in T.
     */
    j = 0;
    for (i = 1; i < T_size; i <<= 1)
    {
        TT[j++] = T + i;
    }

    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV + 6 * j - 2);

    MBEDTLS_MPI_CHK(ecp_normalize_jac_many(grp, TT, j));

    /*
     * Compute the remaining ones using the minimal number of additions
     * Be careful to update T[2^l] only after using it!
     */
    MBEDTLS_ECP_BUDGET((T_size - 1) * MBEDTLS_ECP_OPS_ADD);

    for (i = 1; i < T_size; i <<= 1)
    {
        j = i;
        while (j--)
        {
            MBEDTLS_MPI_CHK(ecp_add_mixed(grp, &T[i + j], &T[j], &T[i]));
        }
    }

    /*
     * Normalize final elements in T. Even though there are no holes now, we
     * still need the auxiliary array for homogeneity with the previous
     * call. Also, skip T[0] which is already normalised, being a copy of P.
     */
    for (j = 0; j + 1 < T_size; j++)
    {
        TT[j] = T + j + 1;
    }

    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV + 6 * j - 2);

    MBEDTLS_MPI_CHK(ecp_normalize_jac_many(grp, TT, j));

cleanup:

    return ret;
}

/*
 * Select precomputed point: R = sign(i) * T[ abs(i) / 2 ]
 *
 * See ecp_comb_recode_core() for background
 */
static int ecp_select_comb(const mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_ecp_point T[], unsigned char T_size,
                           unsigned char i)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char ii, j;

    /* Ignore the "sign" bit and scale down */
    ii = (i & 0x7Fu) >> 1;

    /* Read the whole table to thwart cache-based timing attacks */
    for (j = 0; j < T_size; j++)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign(&R->X, &T[j].X, j == ii));
        MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign(&R->Y, &T[j].Y, j == ii));
    }

    /* Safely invert result if i is "negative" */
    MBEDTLS_MPI_CHK(ecp_safe_invert_jac(grp, R, i >> 7));

cleanup:
    return ret;
}

/*
 * Core multiplication algorithm for the (modified) comb method.
 * This part is actually common with the basic comb method (GECC 3.44)
 *
 * Cost: d A + d D + 1 R
 */
static int ecp_mul_comb_core(const mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_ecp_point T[],
                             unsigned char T_size, const unsigned char x[], size_t d, int (*f_rng)(void *, unsigned char *, size_t),
                             void * p_rng, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point Txi;
    size_t i;

    mbedtls_ecp_point_init(&Txi);

    (void) rs_ctx;

    {
        int have_rng = 1;

        /* Start with a non-zero point and randomize its coordinates */
        i = d;
        MBEDTLS_MPI_CHK(ecp_select_comb(grp, R, T, T_size, x[i]));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&R->Z, 1));

        if (have_rng)
        {
            MBEDTLS_MPI_CHK(ecp_randomize_jac(grp, R, f_rng, p_rng));
        }
    }

    while (i != 0)
    {
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_DBL + MBEDTLS_ECP_OPS_ADD);
        --i;

        MBEDTLS_MPI_CHK(ecp_double_jac(grp, R, R));
        MBEDTLS_MPI_CHK(ecp_select_comb(grp, &Txi, T, T_size, x[i]));
        MBEDTLS_MPI_CHK(ecp_add_mixed(grp, R, R, &Txi));
    }

cleanup:

    mbedtls_ecp_point_free(&Txi);

    return ret;
}

/*
 * Recode the scalar to get constant-time comb multiplication
 *
 * As the actual scalar recoding needs an odd scalar as a starting point,
 * this wrapper ensures that by replacing m by N - m if necessary, and
 * informs the caller that the result of multiplication will be negated.
 *
 * This works because we only support large prime order for Short Weierstrass
 * curves, so N is always odd hence either m or N - m is.
 *
 * See ecp_comb_recode_core() for background.
 */
static int ecp_comb_recode_scalar(const mbedtls_ecp_group * grp, const mbedtls_mpi * m, unsigned char k[COMB_MAX_D + 1], size_t d,
                                  unsigned char w, unsigned char * parity_trick)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi M, mm;

    mbedtls_mpi_init(&M);
    mbedtls_mpi_init(&mm);

    /* N is always odd (see above), just make extra sure */
    if (mbedtls_mpi_get_bit(&grp->N, 0) != 1)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /* do we need the parity trick? */
    *parity_trick = (mbedtls_mpi_get_bit(m, 0) == 0);

    /* execute parity fix in constant time */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&M, m));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&mm, &grp->N, m));
    MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign(&M, &mm, *parity_trick));

    /* actual scalar recoding */
    ecp_comb_recode_core(k, d, w, &M);

cleanup:
    mbedtls_mpi_free(&mm);
    mbedtls_mpi_free(&M);

    return ret;
}

/*
 * Perform comb multiplication (for short Weierstrass curves)
 * once the auxiliary table has been pre-computed.
 *
 * Scalar recoding may use a parity trick that makes us compute -m * P,
 * if that is the case we'll need to recover m * P at the end.
 */
static int ecp_mul_comb_after_precomp(const mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m,
                                      const mbedtls_ecp_point * T, unsigned char T_size, unsigned char w, size_t d,
                                      int (*f_rng)(void *, unsigned char *, size_t), void * p_rng, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char parity_trick;
    unsigned char k[COMB_MAX_D + 1];
    mbedtls_ecp_point * RR = R;
    int have_rng           = 1;

    MBEDTLS_MPI_CHK(ecp_comb_recode_scalar(grp, m, k, d, w, &parity_trick));
    MBEDTLS_MPI_CHK(ecp_mul_comb_core(grp, RR, T, T_size, k, d, f_rng, p_rng, rs_ctx));
    MBEDTLS_MPI_CHK(ecp_safe_invert_jac(grp, RR, parity_trick));

    /*
     * Knowledge of the jacobian coordinates may leak the last few bits of the
     * scalar [1], and since our MPI implementation isn't constant-flow,
     * inversion (used for coordinate normalization) may leak the full value
     * of its input via side-channels [2].
     *
     * [1] https://eprint.iacr.org/2003/191
     * [2] https://eprint.iacr.org/2020/055
     *
     * Avoid the leak by randomizing coordinates before we normalize them.
     */
    if (have_rng)
    {
        MBEDTLS_MPI_CHK(ecp_randomize_jac(grp, RR, f_rng, p_rng));
    }

    MBEDTLS_MPI_CHK(ecp_normalize_jac(grp, RR));

cleanup:
    return ret;
}

/*
 * Pick window size based on curve size
 */
static unsigned char ecp_pick_window_size(const mbedtls_ecp_group * grp)
{
    unsigned char w;

    /*
     * Minimize the number of multiplications, that is minimize
     * 10 * d * w + 18 * 2^(w-1) + 11 * d + 7 * w, with d = ceil( nbits / w )
     * (see costs of the various parts, with 1S = 1M)
     */
    w = grp->nbits >= 384 ? 5 : 4;

    /* Upstream bumps w by one when P == G so the table can be cached in
     * grp->T; MBEDTLS_ECP_FIXED_POINT_OPTIM is 0 here, so that never happens. */

    /*
     * Make sure w is within bounds.
     * (The last test is useful only for very small curves in the test suite.)
     */
#if (MBEDTLS_ECP_WINDOW_SIZE < 6)
    if (w > MBEDTLS_ECP_WINDOW_SIZE)
    {
        w = MBEDTLS_ECP_WINDOW_SIZE;
    }
#endif
    if (w >= grp->nbits)
    {
        w = 2;
    }

    return w;
}

/*
 * Multiplication using the comb method - for curves in short Weierstrass form
 *
 * This function is mainly responsible for administrative work:
 * - managing the restart context if enabled
 * - managing the table of precomputed points (passed between the below two
 *   functions): allocation, computation, ownership transfer, freeing.
 *
 * It delegates the actual arithmetic work to:
 *      ecp_precompute_comb() and ecp_mul_comb_with_precomp()
 *
 * See comments on ecp_comb_recode_core() regarding the computation strategy.
 */
static int ecp_mul_comb(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P,
                        int (*f_rng)(void *, unsigned char *, size_t), void * p_rng, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char w, i;
    size_t d;
    unsigned char T_size  = 0;
    mbedtls_ecp_point * T = NULL;
    ecp_drbg_context drbg_ctx;

    ecp_drbg_init(&drbg_ctx);

    ECP_RS_ENTER(rsm);

    if (f_rng == NULL)
    {
        /* Adjust pointers */
        f_rng = &ecp_drbg_random;
        p_rng = &drbg_ctx;

        /* Initialize internal DRBG if necessary */
        {
            const size_t m_len = (grp->nbits + 7) / 8;
            MBEDTLS_MPI_CHK(ecp_drbg_seed(p_rng, m, m_len));
        }
    }

    /* Pick window size and deduce related sizes. MBEDTLS_ECP_FIXED_POINT_OPTIM
     * is 0 in the RT58x config, so the P == G case (which would cache the
     * precomputed table in grp->T) never applies and the table is always
     * allocated, used and freed within this call. */
    w      = ecp_pick_window_size(grp);
    T_size = 1U << (w - 1);
    d      = (grp->nbits + w - 1) / w;

    T = mbedtls_calloc(T_size, sizeof(mbedtls_ecp_point));
    if (T == NULL)
    {
        ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
        goto cleanup;
    }

    for (i = 0; i < T_size; i++)
    {
        mbedtls_ecp_point_init(&T[i]);
    }

    MBEDTLS_MPI_CHK(ecp_precompute_comb(grp, T, P, w, d, rs_ctx));

    /* Actual comb multiplication using precomputed points */
    MBEDTLS_MPI_CHK(ecp_mul_comb_after_precomp(grp, R, m, T, T_size, w, d, f_rng, p_rng, rs_ctx));

cleanup:

    ecp_drbg_free(&drbg_ctx);

    if (T != NULL)
    {
        for (i = 0; i < T_size; i++)
        {
            mbedtls_ecp_point_free(&T[i]);
        }
        mbedtls_free(T);
    }

    /* prevent caller from using invalid value */
    int should_free_R = (ret != 0);
    if (should_free_R)
    {
        mbedtls_ecp_point_free(R);
    }

    ECP_RS_LEAVE(rsm);

    return ret;
}

#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Restartable multiplication R = m * P
 */
int mbedtls_ecp_mul_restartable(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P,
                                int (*f_rng)(void *, unsigned char *, size_t), void * p_rng, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);

    (void) rs_ctx;

    int restarting = 0;
    /* skip argument check when restarting */
    if (!restarting)
    {
        /* check_privkey is free */
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_CHK);

        /* Common sanity checks */
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_privkey(grp, m));
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
    }

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    /* P-256 goes to the crypto engine; m and P were validated above. */
    if (grp->id == MBEDTLS_ECP_DP_SECP256R1)
    {
        MBEDTLS_MPI_CHK(ecp_hw_mul_p256(grp, R, m, P));
        goto cleanup;
    }
#endif

    ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        MBEDTLS_MPI_CHK(ecp_mul_comb(grp, R, m, P, f_rng, p_rng, rs_ctx));
    }
#endif

cleanup:

    return ret;
}

/*
 * Multiplication R = m * P
 */
int mbedtls_ecp_mul(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P,
                    int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);
    return mbedtls_ecp_mul_restartable(grp, R, m, P, f_rng, p_rng, NULL);
}

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * Check that an affine point is valid as a public key,
 * short weierstrass curves (SEC1 3.2.3.1)
 */
static int ecp_check_pubkey_sw(const mbedtls_ecp_group * grp, const mbedtls_ecp_point * pt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi YY, RHS;

    /* pt coordinates must be normalized for our checks */
    if (mbedtls_mpi_cmp_int(&pt->X, 0) < 0 || mbedtls_mpi_cmp_int(&pt->Y, 0) < 0 || mbedtls_mpi_cmp_mpi(&pt->X, &grp->P) >= 0 ||
        mbedtls_mpi_cmp_mpi(&pt->Y, &grp->P) >= 0)
    {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

    mbedtls_mpi_init(&YY);
    mbedtls_mpi_init(&RHS);

    /*
     * YY = Y^2
     * RHS = X (X^2 + A) + B = X^3 + A X + B
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &YY, &pt->Y, &pt->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &RHS, &pt->X, &pt->X));

    /* Special case for A = -3 */
    if (grp->A.p == NULL)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&RHS, &RHS, 3));
        MOD_SUB(RHS);
    }
    else
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mod(grp, &RHS, &RHS, &grp->A));
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, &RHS, &RHS, &pt->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mod(grp, &RHS, &RHS, &grp->B));

    if (mbedtls_mpi_cmp_mpi(&YY, &RHS) != 0)
    {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
    }

cleanup:

    mbedtls_mpi_free(&YY);
    mbedtls_mpi_free(&RHS);

    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * R = m * P with shortcuts for m == 0, m == 1 and m == -1
 * NOT constant-time - ONLY for short Weierstrass!
 */
static int mbedtls_ecp_mul_shortcuts(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m,
                                     const mbedtls_ecp_point * P, mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (mbedtls_mpi_cmp_int(m, 0) == 0)
    {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_set_zero(R));
    }
    else if (mbedtls_mpi_cmp_int(m, 1) == 0)
    {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
    }
    else if (mbedtls_mpi_cmp_int(m, -1) == 0)
    {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
        if (mbedtls_mpi_cmp_int(&R->Y, 0) != 0)
        {
            MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&R->Y, &grp->P, &R->Y));
        }
    }
    else
    {
        MBEDTLS_MPI_CHK(mbedtls_ecp_mul_restartable(grp, R, m, P, NULL, NULL, rs_ctx));
    }

cleanup:
    return ret;
}

/*
 * Restartable linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd_restartable(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m,
                                   const mbedtls_ecp_point * P, const mbedtls_mpi * n, const mbedtls_ecp_point * Q,
                                   mbedtls_ecp_restart_ctx * rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point mP;
    mbedtls_ecp_point * pmP = &mP;
    mbedtls_ecp_point * pR  = R;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(n != NULL);
    ECP_VALIDATE_RET(Q != NULL);

    if (mbedtls_ecp_get_type(grp) != MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    /* Try the crypto engine first; it reports FEATURE_UNAVAILABLE for the
     * corner cases (scalar 0/±1, doubling, point at infinity) which then
     * run through the software path below. */
    if (grp->id == MBEDTLS_ECP_DP_SECP256R1)
    {
        ret = ecp_hw_muladd_p256(grp, R, m, P, n, Q);
        if (ret != MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE)
        {
            return ret;
        }
    }
#endif

    mbedtls_ecp_point_init(&mP);

    ECP_RS_ENTER(ma);

    MBEDTLS_MPI_CHK(mbedtls_ecp_mul_shortcuts(grp, pmP, m, P, rs_ctx));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul_shortcuts(grp, pR, n, Q, rs_ctx));

    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_ADD);
    MBEDTLS_MPI_CHK(ecp_add_mixed(grp, pR, pmP, pR));
    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV);
    MBEDTLS_MPI_CHK(ecp_normalize_jac(grp, pR));

cleanup:

    mbedtls_ecp_point_free(&mP);

    ECP_RS_LEAVE(ma);

    return ret;
}

/*
 * Linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd(mbedtls_ecp_group * grp, mbedtls_ecp_point * R, const mbedtls_mpi * m, const mbedtls_ecp_point * P,
                       const mbedtls_mpi * n, const mbedtls_ecp_point * Q)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(R != NULL);
    ECP_VALIDATE_RET(m != NULL);
    ECP_VALIDATE_RET(P != NULL);
    ECP_VALIDATE_RET(n != NULL);
    ECP_VALIDATE_RET(Q != NULL);
    return mbedtls_ecp_muladd_restartable(grp, R, m, P, n, Q, NULL);
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Check that a point is valid as a public key
 */
int mbedtls_ecp_check_pubkey(const mbedtls_ecp_group * grp, const mbedtls_ecp_point * pt)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(pt != NULL);

    /* Must use affine coordinates */
    if (mbedtls_mpi_cmp_int(&pt->Z, 1) != 0)
    {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        return ecp_check_pubkey_sw(grp, pt);
    }
#endif
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

/*
 * Check that an mbedtls_mpi is valid as a private key
 */
int mbedtls_ecp_check_privkey(const mbedtls_ecp_group * grp, const mbedtls_mpi * d)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        /* see SEC1 3.2 */
        if (mbedtls_mpi_cmp_int(d, 1) < 0 || mbedtls_mpi_cmp_mpi(d, &grp->N) >= 0)
        {
            return MBEDTLS_ERR_ECP_INVALID_KEY;
        }
        else
        {
            return 0;
        }
    }
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
static int mbedtls_ecp_gen_privkey_sw(const mbedtls_mpi * N, mbedtls_mpi * d, int (*f_rng)(void *, unsigned char *, size_t),
                                      void * p_rng)
{
    int ret = mbedtls_mpi_random(d, 1, N, f_rng, p_rng);
    switch (ret)
    {
    case MBEDTLS_ERR_MPI_NOT_ACCEPTABLE:
        return MBEDTLS_ERR_ECP_RANDOM_FAILED;
    default:
        return ret;
    }
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Generate a private key
 */
int mbedtls_ecp_gen_privkey(const mbedtls_ecp_group * grp, mbedtls_mpi * d, int (*f_rng)(void *, unsigned char *, size_t),
                            void * p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        return mbedtls_ecp_gen_privkey_sw(&grp->N, d, f_rng, p_rng);
    }
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

/*
 * Generate a keypair with configurable base point
 */
int mbedtls_ecp_gen_keypair_base(mbedtls_ecp_group * grp, const mbedtls_ecp_point * G, mbedtls_mpi * d, mbedtls_ecp_point * Q,
                                 int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(G != NULL);
    ECP_VALIDATE_RET(Q != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    MBEDTLS_MPI_CHK(mbedtls_ecp_gen_privkey(grp, d, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, Q, d, G, f_rng, p_rng));

cleanup:
    return ret;
}

/*
 * Generate key pair, wrapper for conventional base point
 */
int mbedtls_ecp_gen_keypair(mbedtls_ecp_group * grp, mbedtls_mpi * d, mbedtls_ecp_point * Q,
                            int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    ECP_VALIDATE_RET(grp != NULL);
    ECP_VALIDATE_RET(d != NULL);
    ECP_VALIDATE_RET(Q != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    return mbedtls_ecp_gen_keypair_base(grp, &grp->G, d, Q, f_rng, p_rng);
}

/*
 * Generate a keypair, prettier wrapper
 */
int mbedtls_ecp_gen_key(mbedtls_ecp_group_id grp_id, mbedtls_ecp_keypair * key, int (*f_rng)(void *, unsigned char *, size_t),
                        void * p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    ECP_VALIDATE_RET(key != NULL);
    ECP_VALIDATE_RET(f_rng != NULL);

    if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0)
    {
        return ret;
    }

    return mbedtls_ecp_gen_keypair(&key->grp, &key->d, &key->Q, f_rng, p_rng);
}

#define ECP_CURVE25519_KEY_SIZE 32
/*
 * Read a private key.
 */
int mbedtls_ecp_read_key(mbedtls_ecp_group_id grp_id, mbedtls_ecp_keypair * key, const unsigned char * buf, size_t buflen)
{
    int ret = 0;

    ECP_VALIDATE_RET(key != NULL);
    ECP_VALIDATE_RET(buf != NULL);

    if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0)
    {
        return ret;
    }

    ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&key->d, buf, buflen));

        MBEDTLS_MPI_CHK(mbedtls_ecp_check_privkey(&key->grp, &key->d));
    }

#endif
cleanup:

    if (ret != 0)
    {
        mbedtls_mpi_free(&key->d);
    }

    return ret;
}

/*
 * Write a private key.
 */
int mbedtls_ecp_write_key(mbedtls_ecp_keypair * key, unsigned char * buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    ECP_VALIDATE_RET(key != NULL);
    ECP_VALIDATE_RET(buflen == 0 || buf != NULL);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS)
    {
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&key->d, buf, buflen));
    }

#endif
cleanup:

    return ret;
}

/*
 * Check a public-private key pair
 */
int mbedtls_ecp_check_pub_priv(const mbedtls_ecp_keypair * pub, const mbedtls_ecp_keypair * prv)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group grp;
    ECP_VALIDATE_RET(pub != NULL);
    ECP_VALIDATE_RET(prv != NULL);

    if (pub->grp.id == MBEDTLS_ECP_DP_NONE || pub->grp.id != prv->grp.id || mbedtls_mpi_cmp_mpi(&pub->Q.X, &prv->Q.X) ||
        mbedtls_mpi_cmp_mpi(&pub->Q.Y, &prv->Q.Y) || mbedtls_mpi_cmp_mpi(&pub->Q.Z, &prv->Q.Z))
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_group_init(&grp);

    /* mbedtls_ecp_mul() needs a non-const group... */
    mbedtls_ecp_group_copy(&grp, &prv->grp);

    /* Also checks d is valid */
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &Q, &prv->d, &prv->grp.G, NULL, NULL));

    if (mbedtls_mpi_cmp_mpi(&Q.X, &prv->Q.X) || mbedtls_mpi_cmp_mpi(&Q.Y, &prv->Q.Y) || mbedtls_mpi_cmp_mpi(&Q.Z, &prv->Q.Z))
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);

    return ret;
}

#if defined(MBEDTLS_SELF_TEST)

/* Adjust the exponent to be a valid private point for the specified curve.
 * This is sometimes necessary because we use a single set of exponents
 * for all curves but the validity of values depends on the curve. */
static int self_test_adjust_exponent(const mbedtls_ecp_group * grp, mbedtls_mpi * m)
{
    int ret = 0;
    switch (grp->id)
    {
        /* If Curve25519 is available, then that's what we use for the
         * Montgomery test, so we don't need the adjustment code. */
    default:
        /* Non-Montgomery curves and Curve25519 need no adjustment. */
        (void) grp;
        (void) m;
        goto cleanup;
    }
cleanup:
    return ret;
}

/* Calculate R = m.P for each m in exponents. Check that the number of
 * basic operations doesn't depend on the value of m. */
static int self_test_point(int verbose, mbedtls_ecp_group * grp, mbedtls_ecp_point * R, mbedtls_mpi * m,
                           const mbedtls_ecp_point * P, const char * const * exponents, size_t n_exponents)
{
    int ret  = 0;
    size_t i = 0;
    unsigned long add_c_prev, dbl_c_prev, mul_c_prev;
    add_count = 0;
    dbl_count = 0;
    mul_count = 0;

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(m, 16, exponents[0]));
    MBEDTLS_MPI_CHK(self_test_adjust_exponent(grp, m));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, R, m, P, NULL, NULL));

    for (i = 1; i < n_exponents; i++)
    {
        add_c_prev = add_count;
        dbl_c_prev = dbl_count;
        mul_c_prev = mul_count;
        add_count  = 0;
        dbl_count  = 0;
        mul_count  = 0;

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(m, 16, exponents[i]));
        MBEDTLS_MPI_CHK(self_test_adjust_exponent(grp, m));
        MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, R, m, P, NULL, NULL));

        if (add_count != add_c_prev || dbl_count != dbl_c_prev || mul_count != mul_c_prev)
        {
            ret = 1;
            break;
        }
    }

cleanup:
    if (verbose != 0)
    {
        if (ret != 0)
        {
            mbedtls_printf("failed (%u)\n", (unsigned int) i);
        }
        else
        {
            mbedtls_printf("passed\n");
        }
    }
    return ret;
}

/*
 * Checkup routine
 */
int mbedtls_ecp_self_test(int verbose)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R, P;
    mbedtls_mpi m;

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    /* Exponents especially adapted for secp192k1, which has the lowest
     * order n of all supported curves (secp192r1 is in a slightly larger
     * field but the order of its base point is slightly smaller). */
    const char * sw_exponents[] = {
        "000000000000000000000000000000000000000000000001", /* one */
        "FFFFFFFFFFFFFFFFFFFFFFFE26F2FC170F69466A74DEFD8C", /* n - 1 */
        "5EA6F389A38B8BC81E767753B15AA5569E1782E30ABE7D25", /* random */
        "400000000000000000000000000000000000000000000000", /* one and zeros */
        "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", /* all ones */
        "555555555555555555555555555555555555555555555555", /* 101010... */
    };
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&R);
    mbedtls_ecp_point_init(&P);
    mbedtls_mpi_init(&m);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    /* Use secp192r1 if available, or any available curve */
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, mbedtls_ecp_curve_list()->grp_id));

    if (verbose != 0)
    {
        mbedtls_printf("  ECP SW test #1 (constant op_count, base point G): ");
    }
    /* Do a dummy multiplication first to trigger precomputation */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&m, 2));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &P, &m, &grp.G, NULL, NULL));
    ret = self_test_point(verbose, &grp, &R, &m, &grp.G, sw_exponents, sizeof(sw_exponents) / sizeof(sw_exponents[0]));
    if (ret != 0)
    {
        goto cleanup;
    }

    if (verbose != 0)
    {
        mbedtls_printf("  ECP SW test #2 (constant op_count, other point): ");
    }
    /* We computed P = 2G last time, use it */
    ret = self_test_point(verbose, &grp, &R, &m, &P, sw_exponents, sizeof(sw_exponents) / sizeof(sw_exponents[0]));
    if (ret != 0)
    {
        goto cleanup;
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

cleanup:

    if (ret < 0 && verbose != 0)
    {
        mbedtls_printf("Unexpected error, return code = %08X\n", (unsigned int) ret);
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&m);

    if (verbose != 0)
    {
        mbedtls_printf("\n");
    }

    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_ECP_ALT */

#endif /* MBEDTLS_ECP_C */
