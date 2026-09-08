/*
 *  Elliptic curves over GF(p): curve-specific data and functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_ECP_C)

#include "mbedtls/bn_mul.h"
#include "mbedtls/ecp.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"

#include "ecp_invasive.h"

#include <string.h>

/*
 * RT58x MBEDTLS_ECP_ALT companion: fork of the upstream mbedTLS 2.28
 * library/ecp_curves.c (curve constants and group loading are compiled out of
 * the library when MBEDTLS_ECP_ALT is defined, so the alt module has to
 * provide them). Two deviations from upstream, neither functional:
 *
 *  - the MBEDTLS_ECP_ALT guard below is inverted;
 *  - everything that rt58x-chip-mbedtls-config.h compiles out has been
 *    deleted. The RT58x build enables MBEDTLS_ECP_DP_SECP256R1_ENABLED only,
 *    so the domain parameters and fast-reduction routines of the other twelve
 *    curves (secp192/224/384/521r1, secp192/224/256k1, brainpoolP256/384/512r1,
 *    Curve25519, Curve448) were unreachable #ifdef'd-out source. Retained:
 *    secp256r1 constants, the ECP_LOAD_GROUP helpers, the 32-bit chunk
 *    abstraction shared by the p224/p256/p384 reductions, and ecp_mod_p256.
 *
 * When resyncing with a newer upstream mbedTLS, diff against upstream's
 * secp256r1 paths rather than expecting a whole-file match.
 */
#if defined(MBEDTLS_ECP_ALT)

/* Parameter validation macros based on platform_util.h */
#define ECP_VALIDATE_RET(cond) MBEDTLS_INTERNAL_VALIDATE_RET(cond, MBEDTLS_ERR_ECP_BAD_INPUT_DATA)
#define ECP_VALIDATE(cond) MBEDTLS_INTERNAL_VALIDATE(cond)

/*
 * Note: the constants are in little-endian order
 * to be directly usable in MPIs
 */

/*
 * Domain parameters for secp256r1
 */
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
static const mbedtls_mpi_uint secp256r1_p[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
    MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00),
    MBEDTLS_BYTES_TO_T_UINT_8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
    MBEDTLS_BYTES_TO_T_UINT_8(0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF),
};
static const mbedtls_mpi_uint secp256r1_b[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0x4B, 0x60, 0xD2, 0x27, 0x3E, 0x3C, 0xCE, 0x3B),
    MBEDTLS_BYTES_TO_T_UINT_8(0xF6, 0xB0, 0x53, 0xCC, 0xB0, 0x06, 0x1D, 0x65),
    MBEDTLS_BYTES_TO_T_UINT_8(0xBC, 0x86, 0x98, 0x76, 0x55, 0xBD, 0xEB, 0xB3),
    MBEDTLS_BYTES_TO_T_UINT_8(0xE7, 0x93, 0x3A, 0xAA, 0xD8, 0x35, 0xC6, 0x5A),
};
static const mbedtls_mpi_uint secp256r1_gx[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0x96, 0xC2, 0x98, 0xD8, 0x45, 0x39, 0xA1, 0xF4),
    MBEDTLS_BYTES_TO_T_UINT_8(0xA0, 0x33, 0xEB, 0x2D, 0x81, 0x7D, 0x03, 0x77),
    MBEDTLS_BYTES_TO_T_UINT_8(0xF2, 0x40, 0xA4, 0x63, 0xE5, 0xE6, 0xBC, 0xF8),
    MBEDTLS_BYTES_TO_T_UINT_8(0x47, 0x42, 0x2C, 0xE1, 0xF2, 0xD1, 0x17, 0x6B),
};
static const mbedtls_mpi_uint secp256r1_gy[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0xF5, 0x51, 0xBF, 0x37, 0x68, 0x40, 0xB6, 0xCB),
    MBEDTLS_BYTES_TO_T_UINT_8(0xCE, 0x5E, 0x31, 0x6B, 0x57, 0x33, 0xCE, 0x2B),
    MBEDTLS_BYTES_TO_T_UINT_8(0x16, 0x9E, 0x0F, 0x7C, 0x4A, 0xEB, 0xE7, 0x8E),
    MBEDTLS_BYTES_TO_T_UINT_8(0x9B, 0x7F, 0x1A, 0xFE, 0xE2, 0x42, 0xE3, 0x4F),
};
static const mbedtls_mpi_uint secp256r1_n[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0x51, 0x25, 0x63, 0xFC, 0xC2, 0xCA, 0xB9, 0xF3),
    MBEDTLS_BYTES_TO_T_UINT_8(0x84, 0x9E, 0x17, 0xA7, 0xAD, 0xFA, 0xE6, 0xBC),
    MBEDTLS_BYTES_TO_T_UINT_8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF),
    MBEDTLS_BYTES_TO_T_UINT_8(0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF),
};
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
/* For these curves, we build the group parameters dynamically. */
#define ECP_LOAD_GROUP
#endif

#if defined(ECP_LOAD_GROUP)
/*
 * Create an MPI from embedded constants
 * (assumes len is an exact multiple of sizeof(mbedtls_mpi_uint))
 */
static inline void ecp_mpi_load(mbedtls_mpi * X, const mbedtls_mpi_uint * p, size_t len)
{
    X->s = 1;
    X->n = len / sizeof(mbedtls_mpi_uint);
    X->p = (mbedtls_mpi_uint *) p;
}

/*
 * Set an MPI to static value 1
 */
static inline void ecp_mpi_set1(mbedtls_mpi * X)
{
    static const mbedtls_mpi_uint one[] = { 1 };
    X->s                                = 1;
    X->n                                = 1;
    X->p                                = (mbedtls_mpi_uint *) one; /* X->p will not be modified so the cast is safe */
}

/*
 * Make group available from embedded constants
 */
static int ecp_group_load(mbedtls_ecp_group * grp, const mbedtls_mpi_uint * p, size_t plen, const mbedtls_mpi_uint * a, size_t alen,
                          const mbedtls_mpi_uint * b, size_t blen, const mbedtls_mpi_uint * gx, size_t gxlen,
                          const mbedtls_mpi_uint * gy, size_t gylen, const mbedtls_mpi_uint * n, size_t nlen)
{
    ecp_mpi_load(&grp->P, p, plen);
    if (a != NULL)
    {
        ecp_mpi_load(&grp->A, a, alen);
    }
    ecp_mpi_load(&grp->B, b, blen);
    ecp_mpi_load(&grp->N, n, nlen);

    ecp_mpi_load(&grp->G.X, gx, gxlen);
    ecp_mpi_load(&grp->G.Y, gy, gylen);
    ecp_mpi_set1(&grp->G.Z);

    grp->pbits = mbedtls_mpi_bitlen(&grp->P);
    grp->nbits = mbedtls_mpi_bitlen(&grp->N);

    grp->h = 1;

    return 0;
}
#endif /* ECP_LOAD_GROUP */

#if defined(MBEDTLS_ECP_NIST_OPTIM)
/* Forward declarations */
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
static int ecp_mod_p256(mbedtls_mpi *);
#endif

#define NIST_MODP(P) grp->modp = ecp_mod_##P;
#else
#define NIST_MODP(P)
#endif /* MBEDTLS_ECP_NIST_OPTIM */

#if defined(ECP_LOAD_GROUP)
#define LOAD_GROUP_A(G)                                                                                                            \
    ecp_group_load(grp, G##_p, sizeof(G##_p), G##_a, sizeof(G##_a), G##_b, sizeof(G##_b), G##_gx, sizeof(G##_gx), G##_gy,          \
                   sizeof(G##_gy), G##_n, sizeof(G##_n))

#define LOAD_GROUP(G)                                                                                                              \
    ecp_group_load(grp, G##_p, sizeof(G##_p), NULL, 0, G##_b, sizeof(G##_b), G##_gx, sizeof(G##_gx), G##_gy, sizeof(G##_gy),       \
                   G##_n, sizeof(G##_n))
#endif /* ECP_LOAD_GROUP */

/*
 * Set a group using well-known domain parameters
 */
int mbedtls_ecp_group_load(mbedtls_ecp_group * grp, mbedtls_ecp_group_id id)
{
    ECP_VALIDATE_RET(grp != NULL);
    mbedtls_ecp_group_free(grp);

    mbedtls_ecp_group_init(grp);

    grp->id = id;

    switch (id)
    {
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    case MBEDTLS_ECP_DP_SECP256R1:
        NIST_MODP(p256);
        return LOAD_GROUP(secp256r1);
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */

    default:
        grp->id = MBEDTLS_ECP_DP_NONE;
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }
}

#if defined(MBEDTLS_ECP_NIST_OPTIM)
/*
 * Fast reduction modulo the primes used by the NIST curves.
 *
 * These functions are critical for speed, but not needed for correct
 * operations. So, we make the choice to heavily rely on the internals of our
 * bignum library, which creates a tight coupling between these functions and
 * our MPI implementation.  However, the coupling between the ECP module and
 * MPI remains loose, since these functions can be deactivated at will.
 */

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
/*
 * The reader is advised to first understand upstream's ecp_mod_p192() since the
 * same general structure is used here, but with additional complications:
 * (1) chunks of 32 bits, and (2) subtractions.
 */

/*
 * For these primes, we need to handle data in chunks of 32 bits.
 * This makes it more complicated if we use 64 bits limbs in MPI,
 * which prevents us from using a uniform access method as for p192.
 *
 * So, we define a mini abstraction layer to access 32 bit chunks,
 * load them in 'cur' for work, and store them back from 'cur' when done.
 *
 * While at it, also define the size of N in terms of 32-bit chunks.
 */
#define LOAD32 cur = A(i);

#if defined(MBEDTLS_HAVE_INT32) /* 32 bit */

#define MAX32 N->n
#define A(j) N->p[j]
#define STORE32 N->p[i] = cur;

#else /* 64-bit */

#define MAX32 N->n * 2
#define A(j) (j) % 2 ? (uint32_t) (N->p[(j) / 2] >> 32) : (uint32_t) (N->p[(j) / 2])
#define STORE32                                                                                                                    \
    if (i % 2)                                                                                                                     \
    {                                                                                                                              \
        N->p[i / 2] &= 0x00000000FFFFFFFF;                                                                                         \
        N->p[i / 2] |= ((mbedtls_mpi_uint) cur) << 32;                                                                             \
    }                                                                                                                              \
    else                                                                                                                           \
    {                                                                                                                              \
        N->p[i / 2] &= 0xFFFFFFFF00000000;                                                                                         \
        N->p[i / 2] |= (mbedtls_mpi_uint) cur;                                                                                     \
    }

#endif /* sizeof( mbedtls_mpi_uint ) */

/*
 * Helpers for addition and subtraction of chunks, with signed carry.
 */
static inline void add32(uint32_t * dst, uint32_t src, signed char * carry)
{
    *dst += src;
    *carry += (*dst < src);
}

static inline void sub32(uint32_t * dst, uint32_t src, signed char * carry)
{
    *carry -= (*dst < src);
    *dst -= src;
}

#define ADD(j) add32(&cur, A(j), &c);
#define SUB(j) sub32(&cur, A(j), &c);

#define ciL (sizeof(mbedtls_mpi_uint)) /* chars in limb  */
#define biL (ciL << 3)                 /* bits  in limb  */

/*
 * Helpers for the main 'loop'
 */
#define INIT(b)                                                                                                                    \
    int ret       = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;                                                                         \
    signed char c = 0, cc;                                                                                                         \
    uint32_t cur;                                                                                                                  \
    size_t i = 0, bits = (b);                                                                                                      \
    /* N is the size of the product of two b-bit numbers, plus one */                                                              \
    /* limb for fix_negative */                                                                                                    \
    MBEDTLS_MPI_CHK(mbedtls_mpi_grow(N, (b) *2 / biL + 1));                                                                        \
    LOAD32;

#define NEXT                                                                                                                       \
    STORE32;                                                                                                                       \
    i++;                                                                                                                           \
    LOAD32;                                                                                                                        \
    cc = c;                                                                                                                        \
    c  = 0;                                                                                                                        \
    if (cc < 0)                                                                                                                    \
        sub32(&cur, -cc, &c);                                                                                                      \
    else                                                                                                                           \
        add32(&cur, cc, &c);

#define LAST                                                                                                                       \
    STORE32;                                                                                                                       \
    i++;                                                                                                                           \
    cur = c > 0 ? c : 0;                                                                                                           \
    STORE32;                                                                                                                       \
    cur = 0;                                                                                                                       \
    while (++i < MAX32)                                                                                                            \
    {                                                                                                                              \
        STORE32;                                                                                                                   \
    }                                                                                                                              \
    if (c < 0)                                                                                                                     \
        mbedtls_ecp_fix_negative(N, c, bits);

/*
 * If the result is negative, we get it in the form
 * c * 2^bits + N, with c negative and N positive shorter than 'bits'
 */
MBEDTLS_STATIC_TESTABLE
void mbedtls_ecp_fix_negative(mbedtls_mpi * N, signed char c, size_t bits)
{
    size_t i;

    /* Set N := 2^bits - 1 - N. We know that 0 <= N < 2^bits, so
     * set the absolute value to 0xfff...fff - N. There is no carry
     * since we're subtracting from all-bits-one.  */
    for (i = 0; i <= bits / 8 / sizeof(mbedtls_mpi_uint); i++)
    {
        N->p[i] = ~(mbedtls_mpi_uint) 0 - N->p[i];
    }
    /* Add 1, taking care of the carry. */
    i = 0;
    do
    {
        ++N->p[i];
    } while (N->p[i++] == 0 && i <= bits / 8 / sizeof(mbedtls_mpi_uint));
    /* Invert the sign.
     * Now N = N0 - 2^bits where N0 is the initial value of N. */
    N->s = -1;

    /* Add |c| * 2^bits to the absolute value. Since c and N are
     * negative, this adds c * 2^bits. */
    mbedtls_mpi_uint msw = (mbedtls_mpi_uint) -c;
#if defined(MBEDTLS_HAVE_INT64)
    if (bits == 224)
    {
        msw <<= 32;
    }
#endif
    N->p[bits / 8 / sizeof(mbedtls_mpi_uint)] += msw;
}

/*
 * Fast quasi-reduction modulo p256 (FIPS 186-3 D.2.3)
 */
static int ecp_mod_p256(mbedtls_mpi * N)
{
    INIT(256);

    ADD(8);
    ADD(9);
    SUB(11);
    SUB(12);
    SUB(13);
    SUB(14);
    NEXT; // A0

    ADD(9);
    ADD(10);
    SUB(12);
    SUB(13);
    SUB(14);
    SUB(15);
    NEXT; // A1

    ADD(10);
    ADD(11);
    SUB(13);
    SUB(14);
    SUB(15);
    NEXT; // A2

    ADD(11);
    ADD(11);
    ADD(12);
    ADD(12);
    ADD(13);
    SUB(15);
    SUB(8);
    SUB(9);
    NEXT; // A3

    ADD(12);
    ADD(12);
    ADD(13);
    ADD(13);
    ADD(14);
    SUB(9);
    SUB(10);
    NEXT; // A4

    ADD(13);
    ADD(13);
    ADD(14);
    ADD(14);
    ADD(15);
    SUB(10);
    SUB(11);
    NEXT; // A5

    ADD(14);
    ADD(14);
    ADD(15);
    ADD(15);
    ADD(14);
    ADD(13);
    SUB(8);
    SUB(9);
    NEXT; // A6

    ADD(15);
    ADD(15);
    ADD(15);
    ADD(8);
    SUB(10);
    SUB(11);
    SUB(12);
    SUB(13);
    LAST; // A7

cleanup:
    return ret;
}

#undef A
#undef LOAD32
#undef STORE32
#undef MAX32
#undef INIT
#undef NEXT
#undef LAST

#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */

#endif /* MBEDTLS_ECP_NIST_OPTIM */

#endif /* MBEDTLS_ECP_ALT */

#endif /* MBEDTLS_ECP_C */
