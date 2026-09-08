#ifndef MBEDTLS_SHA256_ALT_H
#define MBEDTLS_SHA256_ALT_H

#include "hosal_crypto_sha256.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only the digest state (sha256_context: total/state/buffer, 104 bytes)
 * persists across calls; the hosal_sha256_dev_t parameter block is built
 * on the stack per call. No dynamic allocation, so contexts constructed
 * before the FreeRTOS heap is ready (C++ static init) are safe, and the
 * struct fits Matter's kMAX_Hash_SHA256_Context_Size (120 bytes). */
typedef struct
{
    sha256_context sha_cnxt;
} mbedtls_sha256_context;

#include "mbedtls/sha256.h"

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_SHA256_ALT_H */
