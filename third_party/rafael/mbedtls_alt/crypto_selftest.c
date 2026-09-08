/*
 * Boot-time self test of the RT58x hardware-crypto mbedtls *_ALT modules.
 *
 * Uses the mbedtls built-in self tests where they are compatible with the
 * ALT implementations (AES, ECP, ECJPAKE). SHA-256 gets a local known-answer
 * test instead: mbedtls_sha256_self_test() also exercises SHA-224, which the
 * RT58x driver does not implement. ECDH/ECDSA have no built-in self test in
 * mbedtls, so they are covered by round-trip tests against the hardware.
 *
 * All output goes through mbedtls_printf(), routed to the UART log by
 * MBEDTLS_PLATFORM_PRINTF_MACRO (see selftest_printf.c).
 */
#include "crypto_selftest.h"

#include <stdint.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecjpake.h"
#include "mbedtls/ecp.h"
#include "mbedtls/platform.h"
#include "mbedtls/sha256.h"

#if defined(MBEDTLS_SELF_TEST)

/* Deterministic RNG for the round-trip tests (same LCG the mbedtls ECJPAKE
 * self test uses); no entropy source is set up this early at boot. */
static int test_rng(void * ctx, unsigned char * out, size_t len)
{
    static uint32_t x = 42;

    (void) ctx;
    while (len > 0)
    {
        size_t use = len > 4 ? 4 : len;
        x          = 1664525 * x + 1013904223;
        memcpy(out, &x, use);
        out += use;
        len -= use;
    }
    return 0;
}

/* FIPS-180 SHA-256 vectors. */
static const unsigned char k_sha256_abc[32] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
    0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
};
static const unsigned char k_sha256_two_block[32] = {
    0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
    0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1,
};
static const unsigned char k_sha256_million_a[32] = {
    0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92, 0x81, 0xa1, 0xc7, 0xe2, 0x84, 0xd7, 0x3e, 0x67,
    0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97, 0x20, 0x0e, 0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0,
};

static int sha256_hash(const unsigned char * input, size_t ilen, const size_t * chunks, size_t nchunks, unsigned char output[32])
{
    mbedtls_sha256_context ctx;
    size_t i, off;
    int ret;

    mbedtls_sha256_init(&ctx);
    ret = mbedtls_sha256_starts_ret(&ctx, 0);
    if (ret == 0 && chunks == NULL)
    {
        ret = mbedtls_sha256_update_ret(&ctx, input, ilen);
    }
    for (i = 0, off = 0; ret == 0 && chunks != NULL && i < nchunks; i++)
    {
        ret = mbedtls_sha256_update_ret(&ctx, input + off, chunks[i]);
        off += chunks[i];
    }
    if (ret == 0)
    {
        ret = mbedtls_sha256_finish_ret(&ctx, output);
    }
    mbedtls_sha256_free(&ctx);
    return ret;
}

static int sha256_selftest(void)
{
    static unsigned char buf[1000];
    static const unsigned char abc[]       = { 'a', 'b', 'c' };
    static const unsigned char two_block[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static const size_t chunks[]           = { 1, 3, 13, 39 }; /* = 56 bytes */
    unsigned char digest[32];
    unsigned char digest2[32];
    mbedtls_sha256_context ctx, ctx2;
    int hw_err;
    int i;
    int ret = 0;

    /* Single-shot "abc". */
    if (sha256_hash(abc, sizeof(abc), NULL, 0, digest) != 0 || memcmp(digest, k_sha256_abc, 32) != 0)
    {
        mbedtls_printf("  SHA-256 KAT #1 (abc): failed\n");
        ret = 1;
    }

    /* Two-block vector fed in unaligned chunks to stress driver buffering. */
    if (sha256_hash(two_block, 56, chunks, 4, digest) != 0 || memcmp(digest, k_sha256_two_block, 32) != 0)
    {
        mbedtls_printf("  SHA-256 KAT #2 (chunked update): failed\n");
        ret = 1;
    }

    /* 1,000,000 x 'a' as 1000 multi-part updates. */
    mbedtls_sha256_init(&ctx);
    memset(buf, 'a', sizeof(buf));
    hw_err = mbedtls_sha256_starts_ret(&ctx, 0);
    for (i = 0; hw_err == 0 && i < 1000; i++)
    {
        hw_err = mbedtls_sha256_update_ret(&ctx, buf, sizeof(buf));
    }
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_finish_ret(&ctx, digest);
    }
    mbedtls_sha256_free(&ctx);
    if (hw_err != 0 || memcmp(digest, k_sha256_million_a, 32) != 0)
    {
        mbedtls_printf("  SHA-256 KAT #3 (million 'a'): failed\n");
        ret = 1;
    }

    /* Clone mid-stream, finish both contexts independently. */
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_init(&ctx2);
    hw_err = mbedtls_sha256_starts_ret(&ctx, 0);
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_update_ret(&ctx, abc, 2); /* "ab" */
    }
    mbedtls_sha256_clone(&ctx2, &ctx);
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_update_ret(&ctx, abc + 2, 1); /* "c" */
    }
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_finish_ret(&ctx, digest);
    }
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_update_ret(&ctx2, abc + 2, 1);
    }
    if (hw_err == 0)
    {
        hw_err = mbedtls_sha256_finish_ret(&ctx2, digest2);
    }
    mbedtls_sha256_free(&ctx);
    mbedtls_sha256_free(&ctx2);
    if (hw_err != 0 || memcmp(digest, k_sha256_abc, 32) != 0 || memcmp(digest2, k_sha256_abc, 32) != 0)
    {
        mbedtls_printf("  SHA-256 KAT #4 (clone): failed\n");
        ret = 1;
    }

    return ret;
}

/* Two hardware keypairs must agree on the ECDH shared secret. Exercises
 * mbedtls_ecdh_gen_public() and mbedtls_ecdh_compute_shared() (both ALT). */
static int ecdh_selftest(void)
{
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q1, Q2;
    mbedtls_mpi d1, d2, z1, z2;
    int ret;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q1);
    mbedtls_ecp_point_init(&Q2);
    mbedtls_mpi_init(&d1);
    mbedtls_mpi_init(&d2);
    mbedtls_mpi_init(&z1);
    mbedtls_mpi_init(&z2);

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret == 0)
    {
        ret = mbedtls_ecdh_gen_public(&grp, &d1, &Q1, test_rng, NULL);
    }
    if (ret == 0)
    {
        ret = mbedtls_ecdh_gen_public(&grp, &d2, &Q2, test_rng, NULL);
    }
    if (ret == 0)
    {
        ret = mbedtls_ecdh_compute_shared(&grp, &z1, &Q2, &d1, test_rng, NULL);
    }
    if (ret == 0)
    {
        ret = mbedtls_ecdh_compute_shared(&grp, &z2, &Q1, &d2, test_rng, NULL);
    }
    if (ret == 0 && mbedtls_mpi_cmp_mpi(&z1, &z2) != 0)
    {
        mbedtls_printf("  ECDH: shared secrets differ\n");
        ret = 1;
    }
    if (ret != 0)
    {
        mbedtls_printf("  ECDH: failed (-0x%04X)\n", (unsigned) -ret);
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q1);
    mbedtls_ecp_point_free(&Q2);
    mbedtls_mpi_free(&d1);
    mbedtls_mpi_free(&d2);
    mbedtls_mpi_free(&z1);
    mbedtls_mpi_free(&z2);
    return ret;
}

/* Hardware sign / hardware verify round trip, plus a tampered-message
 * rejection check. Exercises mbedtls_ecdsa_sign()/_verify() (both ALT). */
static int ecdsa_selftest(void)
{
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi d, r, s;
    unsigned char hash[32];
    int ret;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    memset(hash, 0x5a, sizeof(hash));

    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret == 0)
    {
        ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q, test_rng, NULL);
    }
    if (ret == 0)
    {
        ret = mbedtls_ecdsa_sign(&grp, &r, &s, &d, hash, sizeof(hash), test_rng, NULL);
    }
    if (ret == 0)
    {
        ret = mbedtls_ecdsa_verify(&grp, hash, sizeof(hash), &Q, &r, &s);
    }
    if (ret == 0)
    {
        hash[0] ^= 0x01;
        if (mbedtls_ecdsa_verify(&grp, hash, sizeof(hash), &Q, &r, &s) == 0)
        {
            mbedtls_printf("  ECDSA: accepted a tampered message\n");
            ret = 1;
        }
    }
    if (ret != 0)
    {
        mbedtls_printf("  ECDSA: failed (-0x%04X)\n", (unsigned) -ret);
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    return ret;
}

static int aes_selftest(void)
{
    return mbedtls_aes_self_test(1);
}

static int ecp_selftest(void)
{
    return mbedtls_ecp_self_test(1);
}

static int ecjpake_selftest(void)
{
    return mbedtls_ecjpake_self_test(1);
}

int rt58x_crypto_selftest(void)
{
    static const struct
    {
        const char * name;
        int (*fn)(void);
    } tests[] = {
        { "AES (mbedtls self test)", aes_selftest }, { "SHA-256 (KAT)", sha256_selftest },
        { "ECP (mbedtls self test)", ecp_selftest }, { "ECJPAKE (mbedtls self test)", ecjpake_selftest },
        { "ECDH (round trip)", ecdh_selftest },      { "ECDSA (round trip)", ecdsa_selftest },
    };
    unsigned failed = 0;
    size_t i;

    mbedtls_printf("=== mbedtls HW crypto self tests (may take a while) ===\n");
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        int ret = tests[i].fn();
        if (ret != 0)
        {
            failed++;
        }
        mbedtls_printf("%s: %s\n", tests[i].name, ret == 0 ? "PASS" : "FAIL");
    }
    if (failed == 0)
    {
        mbedtls_printf("=== crypto self tests: all passed ===\n");
    }
    else
    {
        mbedtls_printf("=== crypto self tests: %u FAILED ===\n", failed);
    }
    return (int) failed;
}

#else /* !MBEDTLS_SELF_TEST */

int rt58x_crypto_selftest(void)
{
    return 0;
}

#endif /* MBEDTLS_SELF_TEST */
