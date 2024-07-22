/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <functional>
#include "PacketHeaders.h"
#include "Packet.h"

namespace Parlo
{
    const int MAX_PACKET_SIZE = 1024;

    class ProcessingBuffer 
    {
    public:
        ProcessingBuffer();
        ~ProcessingBuffer();

        using PacketProcessedCallback = std::function<void(const Packet&)>;
        void setOnPacketProcessedHandler(PacketProcessedCallback callback);
       
        void addData(const std::vector<uint8_t>& data);

    private:
        std::queue<uint8_t> internalBuffer;
        std::mutex mutex;
        std::condition_variable cv;
        std::thread processingThread;
        std::atomic<bool> stopProcessing;

        bool hasReadHeader = false;
        uint8_t currentID = 0;
        bool isCompressed = false;
        uint16_t currentLength = 0;

        PacketProcessedCallback onPacketProcessedHandler;

        void processPackets();
        void readHeader();
    };
}
