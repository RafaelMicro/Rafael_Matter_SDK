#ifndef AES_ALT_H
#define AES_ALT_H
#include "hosal_crypto_aes.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only the key material persists across calls; the hosal_aes_dev_t
 * parameter block is built on the stack per call. No dynamic allocation,
 * so contexts created before the FreeRTOS heap is ready are safe. */
typedef struct
{
    uint8_t key_size;
    uint8_t key[32];
} mbedtls_aes_context;

typedef struct mbedtls_aes_xts_context
{
    mbedtls_aes_context crypt;
    mbedtls_aes_context tweak;
} mbedtls_aes_xts_context;

#include "mbedtls/aes.h"

#ifdef __cplusplus
}
#endif

#endif // AES_ALT_H
