/*
 * Copyright (c) 2026 Rafael Microelectronics Inc. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-RafaelMicro-Proprietary-1.0
 *
 */

/** @file zb_mem_config_app.c
 *
 * @description
 *   ZBOSS memory configuration for this application.
 *
 *   The ZBOSS libraries define their internal tables (gc_*) as weak symbols
 *   sized for a 200-node, high-traffic network (zb_mem_config_max.h).
 *   Including zb_mem_config_context.h here defines strong symbols sized for
 *   this device, which replace the library defaults at link time.
 */

//=============================================================================
//                Include
//=============================================================================
#define ZB_CONFIGURABLE_MEM
#define ZB_LIMIT_VISIBILITY
#define APS_FRAGMENTATION
#define ZB_SOFT_SECURITY
#define ZB_MAC_RX_QUEUE_CAP 4
#define ZB_LITTLE_ENDIAN
#define ZB_NEED_ALIGN
#define ZB_AUTO_ACK_TX
#define ZB_TEST_PROFILE

#ifdef CONFIG_HOSAL_SOC_IDLE_SLEEP
#ifndef ZB_ED_ROLE
#define ZB_ED_ROLE
#endif
#ifndef ZB_USE_SLEEP
#define ZB_USE_SLEEP
#endif
#endif

#include "zb_common.h"

//=============================================================================
//                Memory Configuration
//=============================================================================
#ifdef CONFIG_HOSAL_SOC_IDLE_SLEEP
#define ZB_CONFIG_ROLE_ZED
#define ZB_CONFIG_OVERALL_NETWORK_SIZE 32U
#else
#define ZB_CONFIG_ROLE_ZR
#define ZB_CONFIG_OVERALL_NETWORK_SIZE 64U
#endif
#define ZB_CONFIG_MODERATE_TRAFFIC
#define ZB_CONFIG_APPLICATION_MODERATE

#include "zb_mem_config_common.h"

#undef ZB_CONFIG_SCHEDULER_Q_SIZE
#define ZB_CONFIG_SCHEDULER_Q_SIZE 48U

#include "zb_mem_config_context.h"
