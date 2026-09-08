#include "FreeRTOS.h"
#include "hosal_flash.h"
#include "hosal_rf.h"
#include "hosal_sysctrl.h"
#include "lmac15p4.h"
#include "log.h"
#include "mcu.h"
#include "rf_common_init.h"
#include "rf_mcu.h"
#include "task.h"
#include "uart.h"

#ifdef __cplusplus
extern "C" {
#endif
void rf_init(void);
uint8_t rf_init_done(void);
void set_channel(lmac154_channel_t channel);
void tx_single_tone(uint8_t enable);
void tx_pkt_cnt(uint32_t pkt_cnt);
void set_auto_state(uint32_t enable);

#ifdef __cplusplus
}
#endif
