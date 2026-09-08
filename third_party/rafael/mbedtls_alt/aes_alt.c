#include "aes_alt.h"
#include <string.h>

#include "mbedtls/platform.h"

#include "hosal_crypto_aes.h"
#include "hw_crypto_lock.h"
#include "log.h"

void mbedtls_aes_init(mbedtls_aes_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    ctx->key_size = 0;
    memset(ctx->key, 0, sizeof(ctx->key));
}

void mbedtls_aes_free(mbedtls_aes_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    /* Zeroize so no key material is left in RAM. */
    ctx->key_size = 0;
    memset(ctx->key, 0, sizeof(ctx->key));
}

int mbedtls_aes_setkey_enc(mbedtls_aes_context * ctx, const unsigned char * key, unsigned int keybits)
{
    switch (keybits)
    {
    case HOSAL_AES_128_BIT:
        ctx->key_size = 16;
        break;
    case HOSAL_AES_192_BIT:
        /* AES-192 is optional for ALT implementations*/
        return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
    case HOSAL_AES_256_BIT:
        ctx->key_size = 32;
        break;
    default:
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }
    memcpy(ctx->key, key, ctx->key_size);
    return 0;
}

int mbedtls_aes_setkey_dec(mbedtls_aes_context * ctx, const unsigned char * key, unsigned int keybits)
{
    switch (keybits)
    {
    case HOSAL_AES_128_BIT:
        ctx->key_size = 16;
        break;
    case HOSAL_AES_192_BIT:
        /* AES-192 is optional for ALT implementations*/
        return MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED;
    case HOSAL_AES_256_BIT:
        ctx->key_size = 32;
        break;
    default:
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }
    memcpy(ctx->key, key, ctx->key_size);
    return 0;
}

int mbedtls_aes_crypt_ecb(mbedtls_aes_context * ctx, int mode, const unsigned char input[16], unsigned char output[16])
{
    hosal_aes_dev_t dev;
    int ret;

    if (ctx == NULL || input == NULL || output == NULL)
    {
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }

    memset(&dev, 0, sizeof(dev));
    dev.crypto_operation = (mode == MBEDTLS_AES_ENCRYPT) ? HOSAL_AES_CRYPTO_ENCRYPT : HOSAL_AES_CRYPTO_DECRYPT;
    switch (ctx->key_size)
    {
    case 16:
        dev.bit = HOSAL_AES_128_BIT;
        break;
    case 32:
        dev.bit = HOSAL_AES_256_BIT;
        break;
    default:
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;
    }
    dev.key_ptr = ctx->key;
    dev.in_ptr  = (uint8_t *) input;
    dev.out_ptr = (uint8_t *) output;

    hw_crypto_lock();
    /* Reload the AES microcode every time: the crypto engine's program
     * memory is shared with the ECC/ECJPAKE microcode (both load into
     * CRYPTO_BASE+0x400..0x6xx), so any hardware ECC operation in between
     * clobbers the AES firmware. The reload must happen under the same
     * lock as the operation itself. */
    hosal_crypto_aes_init();
    ret = hosal_crypto_aes_operation(&dev) == 0 ? 0 : -1;
    hw_crypto_unlock();

    return ret;
}
