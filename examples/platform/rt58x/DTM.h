#include "FactoryDataParser.h"
#include "ble_common.h"
#include "cli.h"
#include "flashctl.h"
#include "hosal_dma.h"
#include "hosal_gpio.h"
#include "hosal_rf.h"
#include "hosal_slow_timer.h"
#include "hosal_sysctrl.h"
#include "hosal_timer.h"
#include "hosal_uart.h"
#include "lmac15p4.h"
#include "log.h"
#include "mcu.h"
#include "uart.h"
#include "uart_stdio.h"
#include "util_string.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t wait_dtm(void);
void dtm_init(void);
#ifdef __cplusplus
}
#endif
