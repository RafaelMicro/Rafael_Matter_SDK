/*
 * Shared helpers for the RT58x hardware P-256 alt implementations
 * (ecdh_alt.c / ecdsa_alt.c). The crypto engine works on little-endian
 * 32-byte scalars and affine points; mbedtls MPIs convert via the
 * *_binary_le functions.
 */

#ifndef HW_ECC_P256_H
#define HW_ECC_P256_H

#include <string.h>

#include "hosal_crypto_ecc.h"
#include "hw_crypto_lock.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int hw_p256_point_from_ecp(ECPoint_P256 * le, const mbedtls_ecp_point * pt)
{
    int ret;
    if ((ret = mbedtls_mpi_write_binary_le(&pt->X, le->x, 32)) != 0)
    {
        return ret;
    }
    return mbedtls_mpi_write_binary_le(&pt->Y, le->y, 32);
}

static inline int hw_p256_point_to_ecp(mbedtls_ecp_point * pt, const ECPoint_P256 * le)
{
    int ret;
    if ((ret = mbedtls_mpi_read_binary_le(&pt->X, le->x, 32)) != 0)
    {
        return ret;
    }
    if ((ret = mbedtls_mpi_read_binary_le(&pt->Y, le->y, 32)) != 0)
    {
        return ret;
    }
    return mbedtls_mpi_lset(&pt->Z, 1);
}

/* result = k * base on the crypto engine, under hw_crypto_lock. Valid
 * inputs only (k in [1, N-1], base on curve). */
static inline int hw_p256_mul_le(ECPoint_P256 * result, const ECPoint_P256 * base, const uint8_t k_le[32])
{
    hosal_crypto_ecc_p256_t op;
    int status;

    memset(&op, 0, sizeof(op));
    op.crypto_operation = HOSAL_GFP_P256_MULTI;
    op.result           = result;
    op.base             = (ECPoint_P256 *) base;
    op.p_key            = (uint32_t *) k_le;

    hw_crypto_lock();
    hosal_crypto_ecc_init(HOSAL_ECC_CURVE_P256_INIT);
    status = hosal_crypto_ecc_p256(&op);
    hw_crypto_unlock();

    return status;
}

/* Convert a hash to the engine's little-endian 32-byte scalar using the
 * ECDSA leftmost-bits rule (truncate long hashes, right-align short ones). */
static inline void hw_p256_hash_to_le(uint8_t out_le[32], const unsigned char * buf, size_t blen)
{
    size_t use = (blen > 32) ? 32 : blen;
    size_t i;

    memset(out_le, 0, 32);
    for (i = 0; i < use; i++)
    {
        out_le[i] = buf[use - 1 - i];
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HW_ECC_P256_H */
