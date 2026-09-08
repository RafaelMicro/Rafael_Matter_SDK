/*
 * UART bridge for mbedtls_printf(), which is only referenced by the
 * MBEDTLS_SELF_TEST code (see MBEDTLS_PLATFORM_PRINTF_MACRO in
 * rt58x-chip-mbedtls-config.h). Newlines are expanded to CRLF for the
 * console; output longer than the local buffer is truncated.
 */
#include <stdarg.h>
#include <stdio.h>

#include "log.h"

int rt58x_mbedtls_selftest_printf(const char * fmt, ...)
{
    char msg[128];
    char out[192];
    va_list ap;
    int len;
    size_t o = 0;

    va_start(ap, fmt);
    len = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (len <= 0)
    {
        return len;
    }

    for (size_t i = 0; msg[i] != '\0' && o < sizeof(out) - 2; i++)
    {
        if (msg[i] == '\n')
        {
            out[o++] = '\r';
        }
        out[o++] = msg[i];
    }
    out[o] = '\0';
    log_printk("%s", out);
    return len;
}
