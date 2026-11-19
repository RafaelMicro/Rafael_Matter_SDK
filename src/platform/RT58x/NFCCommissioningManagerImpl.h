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

/**
 *    @file
 *          Provides an implementation of the NFCCommissioningManager singleton object
 *          for the RT58x platforms.
 */

#pragma once

#include <platform/internal/NFCCommissioningManager.h>

#if CHIP_DEVICE_CONFIG_ENABLE_NFC_BASED_COMMISSIONING

#include <system/SystemPacketBuffer.h>
#include <transport/raw/PeerAddress.h>

namespace chip {
namespace Transport {
class NFCBase;
} // namespace Transport

namespace DeviceLayer {
namespace Internal {

class NFCCommissioningManagerImpl final : public NFCCommissioningManager
{
    friend NFCCommissioningManager;

public:
    // ===== Members called by Transport::NFCBase (src/transport/raw/NFC.cpp).

    void SetNFCBase(Transport::NFCBase * nfcBase) { mNfcBase = nfcBase; }
    bool CanSendToPeer(const Transport::PeerAddress & address);
    CHIP_ERROR SendToNfcTag(const Transport::PeerAddress & address, System::PacketBufferHandle && msgBuf);

private:
    // ===== Members that implement the NFCCommissioningManager internal interface.

    CHIP_ERROR _Init();
    void _Shutdown();

    // The reader role is only used by commissioners. RT58x acts as the tag
    // (commissionee), so the reader transport is just stored for the interface.
    Nfc::NFCReaderTransport * _GetNFCReaderTransport() const { return mReaderTransport; }
    void _SetNFCReaderTransport(Nfc::NFCReaderTransport * readerTransport) { mReaderTransport = readerTransport; }

    Nfc::NFCReaderTransport * mReaderTransport = nullptr;
    Transport::NFCBase * mNfcBase              = nullptr;
};

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // CHIP_DEVICE_CONFIG_ENABLE_NFC_BASED_COMMISSIONING
