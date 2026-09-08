/*
 * Mutex protecting the RT58x crypto engine. The engine's program memory
 * (CRYPTO_BASE+0x400..) is shared by the AES and ECC/ECJPAKE microcode, so
 * a single lock must serialize ALL hardware crypto operations, including
 * the firmware (re)load that precedes them.
 */

#ifndef HW_CRYPTO_LOCK_H
#define HW_CRYPTO_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

void hw_crypto_lock(void);
void hw_crypto_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_CRYPTO_LOCK_H */
