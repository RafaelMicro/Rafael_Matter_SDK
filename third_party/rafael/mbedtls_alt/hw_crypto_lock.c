#include "hw_crypto_lock.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t s_crypto_hw_mutex = NULL;
static StaticSemaphore_t s_crypto_hw_mutex_buf;

void hw_crypto_lock(void)
{
    if (s_crypto_hw_mutex == NULL)
    {
        taskENTER_CRITICAL();
        if (s_crypto_hw_mutex == NULL)
        {
            s_crypto_hw_mutex = xSemaphoreCreateMutexStatic(&s_crypto_hw_mutex_buf);
        }
        taskEXIT_CRITICAL();
    }
    xSemaphoreTake(s_crypto_hw_mutex, portMAX_DELAY);
}

void hw_crypto_unlock(void)
{
    xSemaphoreGive(s_crypto_hw_mutex);
}
