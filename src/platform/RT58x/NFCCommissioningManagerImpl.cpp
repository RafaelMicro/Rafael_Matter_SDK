/*
 *
 *    Copyright (c) 2025 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <lib/core/Global.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <platform/internal/NFCCommissioningManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_NFC_BASED_COMMISSIONING

namespace chip {
namespace DeviceLayer {
namespace Internal {

namespace {
Global<NFCCommissioningManagerImpl> sInstance;
} // namespace

NFCCommissioningManager & NFCCommissioningMgr()
{
    return sInstance.get();
}

NFCCommissioningManagerImpl & NFCCommissioningMgrImpl()
{
    return sInstance.get();
}

CHIP_ERROR NFCCommissioningManagerImpl::_Init()
{
    // TODO: Initialize the NFC front end (tag emulation) and register the
    // receive path. Received data must be handed to mNfcBase->OnNfcTagResponse()
    // on the Matter thread (via PlatformMgr().ScheduleWork()), never from ISR context.
    return CHIP_NO_ERROR;
}

void NFCCommissioningManagerImpl::_Shutdown()
{
    // TODO: Disable the NFC front end.
    mNfcBase = nullptr;
}

bool NFCCommissioningManagerImpl::CanSendToPeer([[maybe_unused]] const Transport::PeerAddress & address)
{
    // TODO: Return true while a reader is in the field and the session is valid.
    return false;
}

CHIP_ERROR NFCCommissioningManagerImpl::SendToNfcTag([[maybe_unused]] const Transport::PeerAddress & address,
                                                     [[maybe_unused]] System::PacketBufferHandle && msgBuf)
{
    // TODO: NFC is reader-driven (half-duplex). Queue msgBuf so it can be
    // returned in chunks on the next reader requests.
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // CHIP_DEVICE_CONFIG_ENABLE_NFC_BASED_COMMISSIONING
