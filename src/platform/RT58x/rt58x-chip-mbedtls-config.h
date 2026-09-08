#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

#include "openthread-core-config.h"

#include <stdio.h>
#include <stdlib.h>

#include <openthread/config.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/memory.h>

#define MBEDTLS_PLATFORM_SNPRINTF_MACRO snprintf

#define MBEDTLS_AES_C
#if (MBEDTLS_VERSION_NUMBER >= 0x03050000)
#define MBEDTLS_AES_ONLY_128_BIT_KEY_LENGTH
#endif
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#if (MBEDTLS_VERSION_NUMBER >= 0x03050000)
#define MBEDTLS_BLOCK_CIPHER_NO_DECRYPT
#endif
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_DEPRECATED_REMOVED
#define MBEDTLS_DEPRECATED_WARNING
#define MBEDTLS_ECJPAKE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED
#define MBEDTLS_MD_C
#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_DTLS_ANTI_REPLAY
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY
#define MBEDTLS_SSL_EXPORT_KEYS
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_PROTO_DTLS
#define MBEDTLS_SSL_TLS_C

#define MBEDTLS_AES_ALT
#define MBEDTLS_SHA256_ALT
#define MBEDTLS_ECDH_COMPUTE_SHARED_ALT
#define MBEDTLS_ECDH_GEN_PUBLIC_ALT
#define MBEDTLS_ECDSA_SIGN_ALT
#define MBEDTLS_ECDSA_VERIFY_ALT
#define MBEDTLS_ECJPAKE_ALT
#define MBEDTLS_ECP_ALT

/* On-device known-answer self tests for the hardware-backed *_ALT modules
 * above (run at boot by the example apps, see CryptoSelfTest.cpp). Comment
 * out MBEDTLS_SELF_TEST to drop the test code and vectors from the image. */
// #define MBEDTLS_SELF_TEST
#if defined(MBEDTLS_SELF_TEST)
/* mbedtls_printf is only referenced by the self tests; route it to the
 * UART log (plain printf goes to the discarding _write stub). */
int rt58x_mbedtls_selftest_printf(const char * fmt, ...);
#define MBEDTLS_PLATFORM_PRINTF_MACRO rt58x_mbedtls_selftest_printf
#endif

/* Modules required by the Matter (CHIP) crypto PAL: ECDH/ECDSA for CASE and
 * operational keys, HKDF/PKCS5 for key derivation and SPAKE2+, X.509 for DAC
 * verification and CSR generation. Enabled unconditionally since Matter does
 * not depend on the OPENTHREAD_CONFIG_* feature flags. */
#define MBEDTLS_BASE64_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_HKDF_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C
#define MBEDTLS_X509_CSR_WRITE_C
#define MBEDTLS_X509_USE_C

#if OPENTHREAD_CONFIG_BORDER_AGENT_ENABLE || OPENTHREAD_CONFIG_COMMISSIONER_ENABLE || OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_SRV_C
#endif

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#endif

#if OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE || OPENTHREAD_CONFIG_TLS_ENABLE
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#endif

#if OPENTHREAD_CONFIG_BLE_TCAT_ENABLE
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_GCM_C
#endif

#if OPENTHREAD_CONFIG_ECDSA_ENABLE && OPENTHREAD_CONFIG_DETERMINISTIC_ECDSA_ENABLE
#define MBEDTLS_ECDSA_DETERMINISTIC
#endif

#define MBEDTLS_MPI_WINDOW_SIZE 1       /**< Maximum windows size used. */
#define MBEDTLS_MPI_MAX_SIZE 32         /**< Maximum number of bytes for usable MPIs. */
#define MBEDTLS_ECP_MAX_BITS 256        /**< Maximum bit size of groups */
#define MBEDTLS_ECP_WINDOW_SIZE 2       /**< Maximum window size used */
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0 /**< Enable fixed-point speed-up */
#define MBEDTLS_ENTROPY_MAX_SOURCES 1   /**< Maximum number of sources supported */

#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
#define MBEDTLS_PLATFORM_STD_CALLOC otPlatCAlloc /**< Default allocator to use, can be undefined */
#define MBEDTLS_PLATFORM_STD_FREE otPlatFree     /**< Default free to use, can be undefined */
#else
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#endif

#if OPENTHREAD_CONFIG_BLE_TCAT_ENABLE
#define MBEDTLS_SSL_MAX_CONTENT_LEN 2000 /**< Maxium fragment length in bytes */
#elif OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE
#define MBEDTLS_SSL_MAX_CONTENT_LEN 900 /**< Maxium fragment length in bytes */
#else
#define MBEDTLS_SSL_MAX_CONTENT_LEN 768 /**< Maxium fragment length in bytes */
#endif

#define MBEDTLS_SSL_IN_CONTENT_LEN MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_OUT_CONTENT_LEN MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8

#if defined(MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

#include "mbedtls/version.h"
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
// Configuration sanity check. Done automatically in Mbed TLS >= 3.0.
#include "mbedtls/check_config.h"
#endif

#endif /* MBEDTLS_CONFIG_H */
