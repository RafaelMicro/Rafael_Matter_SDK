/*
 * Boot-time self test of the RT58x hardware-crypto mbedtls *_ALT modules.
 * Shared by the example apps: call rt58x_crypto_selftest() after
 * init_rt58x_platform(), before the Matter stack starts.
 */
#ifndef CRYPTO_SELFTEST_H
#define CRYPTO_SELFTEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Runs every test group and reports progress through mbedtls_printf()
 * (routed to the UART log, see MBEDTLS_PLATFORM_PRINTF_MACRO). Returns the
 * number of failed test groups, 0 when everything passed. Compiled to a
 * no-op returning 0 when MBEDTLS_SELF_TEST is not defined in
 * rt58x-chip-mbedtls-config.h. */
int rt58x_crypto_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_SELFTEST_H */
