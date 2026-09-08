#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPPlatformMemory.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/KeyValueStoreManager.h>

#ifdef CHIP_CONFIG_USE_SUBSCRIPTION_CALLBACKS
#include "SubscriptionCallback.h"
#endif // CHIP_CONFIG_USE_SUBSCRIPTION_CALLBACKS

void MatterFotaInit(void);
void MatterNetworkInit(void);
void DoFactoryReset(intptr_t arg);
