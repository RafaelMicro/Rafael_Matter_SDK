/*
 * EC-JPAKE context for the RT58x hardware implementation
 * (MBEDTLS_ECJPAKE_ALT). The hardware driver structures are allocated with
 * pvPortMalloc in mbedtls_ecjpake_setup() — i.e. only for handshakes that
 * actually use EC-JPAKE — and released in mbedtls_ecjpake_free().
 */

#ifndef ECJPAKE_ALT_H
#define ECJPAKE_ALT_H

#include "hosal_crypto_ecjpake.h" /* ECJPAKE_CTX, ECJPAKEKeyKP */
#include "mbedtls/ecp.h"
#include "mbedtls/md.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbedtls_ecjpake_context
{
    ECJPAKE_CTX * ecc_ctx;    /* pvPortMalloc'd in setup() */
    ECJPAKEKeyKP * key_array; /* pvPortMalloc'd in setup(), 4 entries:
                               * [0..1] = client X1,X2 / [2..3] = server */
    mbedtls_ecp_group_id gid;
    mbedtls_md_type_t md_type;
    int point_format; /* written directly by ssl_cli.c / ssl_srv.c */
    int is_setup;     /* nonzero once mbedtls_ecjpake_setup() succeeded */
} mbedtls_ecjpake_context;

#include "mbedtls/ecjpake.h"

#ifdef __cplusplus
}
#endif

#endif // ECJPAKE_ALT_H
