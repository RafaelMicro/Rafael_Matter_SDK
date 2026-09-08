/*
 * SHA-256 for RT58x (MBEDTLS_SHA256_ALT), backed by hosal_crypto_sha256.
 * The digest state lives inside the context (no heap allocation); the
 * hosal parameter block is built on the stack per call. Every driver call
 * runs under hw_crypto_lock because the SDK warns that SHA-256 must not
 * run concurrently with the ECC engine.
 */

#include <string.h>

#include "mbedtls/platform.h"

#include "hw_crypto_lock.h"
#include "log.h"
#include "sha256_alt.h"

void mbedtls_sha256_init(mbedtls_sha256_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha256_free(mbedtls_sha256_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha256_clone(mbedtls_sha256_context * dst, const mbedtls_sha256_context * src)
{
    if (dst == NULL || src == NULL)
    {
        return;
    }
    /* The state is fully embedded — a plain copy is a complete clone. */
    *dst = *src;
}

int mbedtls_sha256_starts_ret(mbedtls_sha256_context * ctx, int is224)
{
    hosal_sha256_dev_t dev;
    int ret;

    if (ctx == NULL)
    {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
    if (is224)
    {
        /* The driver implements SHA-256 only. */
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }

    memset(&dev, 0, sizeof(dev));
    dev.crypto_operation = HOSAL_SHA256_DIGEST_INIT;

    hw_crypto_lock();
    hosal_crypto_sha256_init();
    ret = hosal_crypto_sha256_operation(&dev);
#if defined(CONFIG_RT584H) || defined(CONFIG_RT584HA4) || defined(CONFIG_RT584L) || defined(CONFIG_RF1301)
    /* On RT584 DIGEST_INIT only clears the context; the SHA-256 initial
     * digest (H0) is loaded into the engine by DIGEST_STARTS. Without it
     * the hash runs from an all-zero state and produces a wrong digest.
     * RT582/RT583 ROM sha256_init does both, and its hosal rejects STARTS. */
    if (ret == 0)
    {
        dev.crypto_operation = HOSAL_SHA256_DIGEST_STARTS;
        ret                  = hosal_crypto_sha256_operation(&dev);
    }
#endif
    hw_crypto_unlock();

    if (ret != 0)
    {
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    }

    ctx->sha_cnxt = dev.sha_cnxt;
    return 0;
}

int mbedtls_sha256_update_ret(mbedtls_sha256_context * ctx, const unsigned char * input, size_t ilen)
{
    hosal_sha256_dev_t dev;
    int ret;

    if (ctx == NULL || (input == NULL && ilen != 0))
    {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }
    if (ilen == 0)
    {
        return 0;
    }

    memset(&dev, 0, sizeof(dev));
    dev.sha_cnxt         = ctx->sha_cnxt;
    dev.crypto_operation = HOSAL_SHA256_DIGEST_UPDATE;
    dev.in_ptr           = (uint8_t *) input;
    dev.in_length        = ilen;

    hw_crypto_lock();
    hosal_crypto_sha256_init();
    ret = hosal_crypto_sha256_operation(&dev);
    hw_crypto_unlock();

    if (ret != 0)
    {
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    }

    ctx->sha_cnxt = dev.sha_cnxt;
    return 0;
}

int mbedtls_sha256_finish_ret(mbedtls_sha256_context * ctx, unsigned char output[32])
{
    hosal_sha256_dev_t dev;
    int ret;

    if (ctx == NULL || output == NULL)
    {
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
    }

    memset(&dev, 0, sizeof(dev));
    dev.sha_cnxt         = ctx->sha_cnxt;
    dev.crypto_operation = HOSAL_SHA256_DIGEST_FINISH;
    dev.out_ptr          = output;

    hw_crypto_lock();
    hosal_crypto_sha256_init();
    ret = hosal_crypto_sha256_operation(&dev);
    hw_crypto_unlock();

    if (ret != 0)
    {
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    }

    ctx->sha_cnxt = dev.sha_cnxt;
    return 0;
}

int mbedtls_internal_sha256_process(mbedtls_sha256_context * ctx, const unsigned char data[64])
{
    /* Raw single-block processing is not exposed by the driver. Nothing
     * links to this with the CCM-only cipher suites used on this platform
     * (it is only reached via mbedtls_md_process / the CBC Lucky13 path). */
    (void) ctx;
    (void) data;
    return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
}
