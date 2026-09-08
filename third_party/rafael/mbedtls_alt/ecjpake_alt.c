#include <string.h>

#include "FreeRTOS.h"

#include "ecjpake_alt.h"
#include "hw_crypto_lock.h"
#include "log.h"

/* Serialized ECJPAKEKeyKP: len(1)+fmt(1)+X(64) + len(1)+fmt(1)+V(64) +
 * len(1)+r(32) */
#define ECJPAKE_KKP_SERIALIZED_LEN (1 + 1 + 64 + 1 + 1 + 64 + 1 + 32)

static void ecjpake_dev_setup(mbedtls_ecjpake_context * ctx, hosal_ecjpake_dev_t * dev, hosal_crypto_ecjpake_operation_t op)
{
    memset(dev, 0, sizeof(*dev));
    dev->ctx              = ctx->ecc_ctx;
    dev->gen              = (ECPoint_P256 *) &Curve_Gx_p256_BE;
    dev->crypto_operation = op;
}

static int pick_ecjpake_keykp_to_buf(unsigned char * buf, size_t len, const ECJPAKEKeyKP * key, size_t * out_len)
{
    size_t offset = 0;

    if (len < ECJPAKE_KKP_SERIALIZED_LEN)
    {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    /* public key X: length, uncompressed point format, X.x, X.y */
    buf[offset++] = 0x41;
    buf[offset++] = 0x04;
    memcpy(buf + offset, key->X.x, 32);
    offset += 32;
    memcpy(buf + offset, key->X.y, 32);
    offset += 32;

    /* ZKP ephemeral point V */
    buf[offset++] = 0x41;
    buf[offset++] = 0x04;
    memcpy(buf + offset, key->zkp.V.x, 32);
    offset += 32;
    memcpy(buf + offset, key->zkp.V.y, 32);
    offset += 32;

    /* ZKP scalar r */
    buf[offset++] = 0x20;
    memcpy(buf + offset, key->zkp.r, 32);
    offset += 32;

    if (out_len)
    {
        *out_len = offset;
    }
    return 0;
}

static int parse_ecjpake_keykp_from_buf(const unsigned char * buf, size_t len, ECJPAKEKeyKP * key, size_t * parsed_len)
{
    size_t offset = 0;

    if (len < ECJPAKE_KKP_SERIALIZED_LEN)
    {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    if (buf[offset++] != 0x41)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (buf[offset++] != 0x04)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    memcpy(key->X.x, buf + offset, 32);
    offset += 32;
    memcpy(key->X.y, buf + offset, 32);
    offset += 32;

    if (buf[offset++] != 0x41)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (buf[offset++] != 0x04)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    memcpy(key->zkp.V.x, buf + offset, 32);
    offset += 32;
    memcpy(key->zkp.V.y, buf + offset, 32);
    offset += 32;

    if (buf[offset++] != 0x20)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    memcpy(key->zkp.r, buf + offset, 32);
    offset += 32;

    if (parsed_len)
    {
        *parsed_len = offset;
    }
    return 0;
}

void mbedtls_ecjpake_init(mbedtls_ecjpake_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    /* No allocation here: the SSL layer inits this context for every
     * handshake; the buffers are only allocated in setup(), i.e. when a
     * J-PAKE password is actually configured. */
    memset(ctx, 0, sizeof(*ctx));
    ctx->gid          = MBEDTLS_ECP_DP_NONE;
    ctx->md_type      = MBEDTLS_MD_NONE;
    ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;
}

void mbedtls_ecjpake_free(mbedtls_ecjpake_context * ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    /* Zeroize before freeing: the buffers hold the secret and private
     * keys. */
    if (ctx->ecc_ctx != NULL)
    {
        memset(ctx->ecc_ctx, 0, sizeof(ECJPAKE_CTX));
        vPortFree(ctx->ecc_ctx);
    }
    if (ctx->key_array != NULL)
    {
        memset(ctx->key_array, 0, sizeof(ECJPAKEKeyKP) * 4);
        vPortFree(ctx->key_array);
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->gid     = MBEDTLS_ECP_DP_NONE;
    ctx->md_type = MBEDTLS_MD_NONE;
}

int mbedtls_ecjpake_setup(mbedtls_ecjpake_context * ctx, mbedtls_ecjpake_role role, mbedtls_md_type_t hash,
                          mbedtls_ecp_group_id curve, const unsigned char * secret, size_t len)
{
    if (ctx == NULL || secret == NULL || len == 0 || len > 32)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    /* The hardware only does SHA-256 over SECP256R1 (the only combination
     * the SSL layer ever requests for TLS_ECJPAKE_WITH_AES_128_CCM_8). */
    if (hash != MBEDTLS_MD_SHA256 || curve != MBEDTLS_ECP_DP_SECP256R1)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if (ctx->ecc_ctx == NULL)
    {
        ctx->ecc_ctx = pvPortMalloc(sizeof(ECJPAKE_CTX));
    }
    if (ctx->key_array == NULL)
    {
        ctx->key_array = pvPortMalloc(sizeof(ECJPAKEKeyKP) * 4);
    }
    if (ctx->ecc_ctx == NULL || ctx->key_array == NULL)
    {
        mbedtls_ecjpake_free(ctx);
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }
    memset(ctx->ecc_ctx, 0, sizeof(ECJPAKE_CTX));
    memset(ctx->key_array, 0, sizeof(ECJPAKEKeyKP) * 4);

    ctx->ecc_ctx->role = (role == MBEDTLS_ECJPAKE_SERVER) ? server_role : client_role;

    /* Right-aligned, zero-padded 32-byte big-endian secret. */
    memcpy(&ctx->ecc_ctx->secret[32 - len], secret, len);

    ctx->md_type  = hash;
    ctx->gid      = curve;
    ctx->is_setup = 1;

    return 0;
}

int mbedtls_ecjpake_check(const mbedtls_ecjpake_context * ctx)
{
    if (ctx == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    return 0;
}

int mbedtls_ecjpake_write_round_one(mbedtls_ecjpake_context * ctx, unsigned char * buf, size_t len, size_t * olen,
                                    int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    hosal_ecjpake_dev_t dev;
    ECJPAKEKeyKP * keys[2];
    size_t offset = 0;
    int i;

    if (ctx == NULL || buf == NULL || olen == NULL || f_rng == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (len < 2 * ECJPAKE_KKP_SERIALIZED_LEN)
    {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    if (ctx->ecc_ctx->role == server_role)
    {
        keys[0] = &ctx->key_array[2];
        keys[1] = &ctx->key_array[3];
    }
    else
    {
        keys[0] = &ctx->key_array[0];
        keys[1] = &ctx->key_array[1];
    }

    /* Draw the private keys from the mbedtls RNG BEFORE taking the lock:
     * f_rng is CTR-DRBG on top of the hardware AES, which takes the same
     * non-recursive hw_crypto_lock. */
    f_rng(p_rng, ctx->ecc_ctx->private_key_x1, sizeof(ctx->ecc_ctx->private_key_x1));
    f_rng(p_rng, ctx->ecc_ctx->private_key_x2, sizeof(ctx->ecc_ctx->private_key_x2));

    ecjpake_dev_setup(ctx, &dev, HOSAL_ECJPAKE_GENERATE_ZKP);

    hw_crypto_lock();
    hosal_crypto_ecjpake_init();
    for (i = 0; i < 2; i++)
    {
        dev.key         = keys[i];
        dev.private_key = (i == 0) ? ctx->ecc_ctx->private_key_x1 : ctx->ecc_ctx->private_key_x2;
        if (hosal_crypto_ecjpake_operation(&dev) != 0)
        {
            hw_crypto_unlock();
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }
    }
    hw_crypto_unlock();

    for (i = 0; i < 2; i++)
    {
        size_t pick_len = 0;
        int ret         = pick_ecjpake_keykp_to_buf(buf + offset, len - offset, keys[i], &pick_len);
        if (ret != 0)
        {
            return ret;
        }
        offset += pick_len;

        if (i == 0)
        {
            memcpy(&ctx->ecc_ctx->ecc_point_X1, &keys[i]->X, sizeof(ECPoint_P256));
        }
        else
        {
            memcpy(&ctx->ecc_ctx->ecc_point_X2, &keys[i]->X, sizeof(ECPoint_P256));
        }
    }

    *olen = offset;
    return 0;
}

int mbedtls_ecjpake_read_round_one(mbedtls_ecjpake_context * ctx, const unsigned char * buf, size_t len)
{
    hosal_ecjpake_dev_t dev;
    ECJPAKEKeyKP * keys[2];
    size_t offset = 0;
    int i;

    if (ctx == NULL || buf == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if (ctx->ecc_ctx->role == server_role)
    {
        keys[0] = &ctx->key_array[0];
        keys[1] = &ctx->key_array[1];
    }
    else
    {
        keys[0] = &ctx->key_array[2];
        keys[1] = &ctx->key_array[3];
    }

    for (i = 0; i < 2; i++)
    {
        size_t parsed_len = 0;
        int ret           = parse_ecjpake_keykp_from_buf(buf + offset, len - offset, keys[i], &parsed_len);
        if (ret != 0)
        {
            return ret;
        }
        offset += parsed_len;
    }

    ecjpake_dev_setup(ctx, &dev, HOSAL_ECJPAKE_GENERATE_VERIFY);

    hw_crypto_lock();
    hosal_crypto_ecjpake_init();
    for (i = 0; i < 2; i++)
    {
        dev.key = keys[i];
        if (hosal_crypto_ecjpake_operation(&dev) != 0)
        {
            hw_crypto_unlock();
            return MBEDTLS_ERR_ECP_VERIFY_FAILED;
        }
    }
    hw_crypto_unlock();

    memcpy(&ctx->ecc_ctx->ecc_point_X3, &keys[0]->X, sizeof(ECPoint_P256));
    memcpy(&ctx->ecc_ctx->ecc_point_X4, &keys[1]->X, sizeof(ECPoint_P256));

    return 0;
}

int mbedtls_ecjpake_write_round_two(mbedtls_ecjpake_context * ctx, unsigned char * buf, size_t len, size_t * olen,
                                    int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    hosal_ecjpake_dev_t dev;
    ECJPAKEKeyKP * key;
    size_t offset   = 0;
    size_t pick_len = 0;
    int ret;

    /* The round-two ZKP ephemeral randomness comes from the SDK-internal
     * TRNG inside the driver; f_rng is not used here. */
    (void) f_rng;
    (void) p_rng;

    if (ctx == NULL || buf == NULL || olen == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if (ctx->ecc_ctx->role == server_role)
    {
        key = &ctx->key_array[2];
        /* ServerKeyExchange carries an ECParameters header. */
        const mbedtls_ecp_curve_info * curve_info = mbedtls_ecp_curve_info_from_grp_id(ctx->gid);
        if (curve_info == NULL)
        {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }
        if (len < 3 + ECJPAKE_KKP_SERIALIZED_LEN)
        {
            return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        }
        buf[offset++] = MBEDTLS_ECP_TLS_NAMED_CURVE;
        buf[offset++] = (unsigned char) (curve_info->tls_id >> 8);
        buf[offset++] = (unsigned char) (curve_info->tls_id & 0xFF);
    }
    else
    {
        key = &ctx->key_array[0];
        if (len < ECJPAKE_KKP_SERIALIZED_LEN)
        {
            return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        }
    }

    ecjpake_dev_setup(ctx, &dev, HOSAL_ECJPAKE_GENERATE_ZKP_2);
    dev.key = key;

    hw_crypto_lock();
    hosal_crypto_ecjpake_init();
    ret = hosal_crypto_ecjpake_operation(&dev);
    hw_crypto_unlock();
    if (ret != 0)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    ret = pick_ecjpake_keykp_to_buf(buf + offset, len - offset, key, &pick_len);
    if (ret != 0)
    {
        return ret;
    }
    offset += pick_len;

    *olen = offset;
    return 0;
}

int mbedtls_ecjpake_read_round_two(mbedtls_ecjpake_context * ctx, const unsigned char * buf, size_t len)
{
    hosal_ecjpake_dev_t dev;
    ECJPAKEKeyKP * key;
    size_t offset     = 0;
    size_t parsed_len = 0;
    int ret;

    if (ctx == NULL || buf == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if (ctx->ecc_ctx->role == server_role)
    {
        key = &ctx->key_array[0];
    }
    else
    {
        key = &ctx->key_array[2];
        /* The client receives ServerKeyExchange: skip the 3-byte
         * ECParameters header. */
        if (len < 3 || buf[0] != MBEDTLS_ECP_TLS_NAMED_CURVE)
        {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }
        offset = 3;
    }

    ret = parse_ecjpake_keykp_from_buf(buf + offset, len - offset, key, &parsed_len);
    if (ret != 0)
    {
        return ret;
    }

    ecjpake_dev_setup(ctx, &dev, HOSAL_ECJPAKE_GENERATE_VERIFY_2);
    dev.key = key;

    hw_crypto_lock();
    hosal_crypto_ecjpake_init();
    ret = hosal_crypto_ecjpake_operation(&dev);
    hw_crypto_unlock();
    if (ret != 0)
    {
        return MBEDTLS_ERR_ECP_VERIFY_FAILED;
    }

    memcpy(&ctx->ecc_ctx->share_key, &key->zkp.V, sizeof(ECPoint_P256));

    return 0;
}

int mbedtls_ecjpake_derive_secret(mbedtls_ecjpake_context * ctx, unsigned char * buf, size_t len, size_t * olen,
                                  int (*f_rng)(void *, unsigned char *, size_t), void * p_rng)
{
    hosal_ecjpake_dev_t dev;
    int ret;

    (void) f_rng;
    (void) p_rng;

    if (ctx == NULL || buf == NULL || olen == NULL || !ctx->is_setup)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    if (len < 32)
    {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    ecjpake_dev_setup(ctx, &dev, HOSAL_ECJPAKE_COMPUTE_KEY);
    dev.key = (ctx->ecc_ctx->role == server_role) ? &ctx->key_array[0] : &ctx->key_array[2];
    dev.pms = buf;

    hw_crypto_lock();
    hosal_crypto_ecjpake_init();
    ret = hosal_crypto_ecjpake_operation(&dev);
    hw_crypto_unlock();
    if (ret != 0)
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    *olen = 32;
    return 0;
}
