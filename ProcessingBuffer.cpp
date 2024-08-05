/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo Library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "Parlo.h"
#include <memory>

namespace Parlo
{
    /*The processingbuffer processes incoming data and turns them into packets.*/
    class ProcessingBuffer::Impl {
    public:
        Impl();
        ~Impl();

        using PacketProcessedCallback = std::function<void(const Packet&)>;
        void setOnPacketProcessedHandler(PacketProcessedCallback callback);

        void addData(const std::vector<uint8_t>& data);

        uint8_t operator[](size_t index) const;

        size_t bufferCount() const;
    private:
        std::queue<uint8_t> internalBuffer;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::thread processingThread;
        std::atomic<bool> stopProcessing{ false };

        bool hasReadHeader = false;
        uint8_t currentID = 0;
        bool isCompressed = false;
        uint16_t currentLength = 0;

        PacketProcessedCallback onPacketProcessedHandler;

        void processPackets();
        void readHeader();

        friend class ProcessingBuffer;
    };

    ProcessingBuffer::Impl::Impl() : stopProcessing(false) {
        processingThread = std::thread(&Impl::processPackets, this);
    }

    ProcessingBuffer::Impl::~Impl() {
        stopProcessing.store(true, std::memory_order_relaxed);
        cv.notify_all();
        if (processingThread.joinable())
            processingThread.join();
    }

    /*Sets a handler for the OnPacketProcessed event.
    @param PacketProcessedCallback A callback function with the signature: void(const Packet&)*/
    void ProcessingBuffer::Impl::setOnPacketProcessedHandler(PacketProcessedCallback callback) {
        onPacketProcessedHandler = callback;
    }

    /*
    * Shovels shit (data) into the buffer.
    * @param The data to add. Needs to be no bigger than MAX_PACKET_SIZE!
    * @exception Throws std::overflow_error if data was larger than MAX_PACKET_SIZE.
    */
    void ProcessingBuffer::Impl::addData(const std::vector<uint8_t>& data) {
        if (data.size() > MAX_PACKET_SIZE)
            throw std::overflow_error("ProcessingBuffer::addData(): Buffer overflow exception!");

        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto byte : data)
                internalBuffer.push(byte);
        }

        cv.notify_one();
    }

    uint8_t ProcessingBuffer::Impl::operator[](size_t index) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (index >= internalBuffer.size())
            throw std::out_of_range("ProcessingBuffer: Index out of range!");

        auto tempQueue = internalBuffer;
        for (size_t i = 0; i < index; ++i)
            tempQueue.pop();

        return tempQueue.front();
    }

    /*The length of the internal buffer.*/
    size_t ProcessingBuffer::Impl::bufferCount() const {
        return internalBuffer.size();
    }

    /*Processes packets.*/
	void ProcessingBuffer::Impl::processPackets() {
        while (!stopProcessing)
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return !internalBuffer.empty() || stopProcessing; });

            if (stopProcessing) break;

            if (internalBuffer.size() >= static_cast<size_t>(PacketHeaders::STANDARD)) {
                if (!hasReadHeader)
                    readHeader();
            }

            if (hasReadHeader) {
                if (internalBuffer.size() >= static_cast<size_t>(currentLength - PacketHeaders::STANDARD)) {
                    std::vector<uint8_t> packetData(currentLength - PacketHeaders::STANDARD);
                    for (uint16_t i = 0; i < packetData.size(); ++i) {
                        packetData[i] = internalBuffer.front();
                        internalBuffer.pop();
                    }

                    hasReadHeader = false;
                    Packet packet(currentID, packetData, isCompressed);
                    
                    if (onPacketProcessedHandler)
                        onPacketProcessedHandler(packet);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10)); //To prevent hogging the processor
        }
	}

    /*Reads a packet's header.*/
    void ProcessingBuffer::Impl::readHeader() {
        if (internalBuffer.size() < 4) 
            return; //Not enough data to read the header

        currentID = internalBuffer.front();
        internalBuffer.pop();
        isCompressed = internalBuffer.front();
        internalBuffer.pop();

        //Read two bytes for the length and combine them
        uint8_t lengthLow = internalBuffer.front();
        internalBuffer.pop();
        uint8_t lengthHigh = internalBuffer.front();
        internalBuffer.pop();

        currentLength = static_cast<uint16_t>(lengthHigh << 8 | lengthLow);
        std::cout << "lengthLow: " << static_cast<int>(lengthLow) << ", lengthHigh: " << static_cast<int>(lengthHigh) << ", currentLength: " << currentLength << std::endl;
        hasReadHeader = true;
    }

    ProcessingBuffer::ProcessingBuffer()
        : pImpl(std::make_unique<Impl>())
    {}

    ProcessingBuffer::~ProcessingBuffer() {
        if (pImpl) {
            pImpl->stopProcessing.store(true, std::memory_order_relaxed);
            pImpl->cv.notify_all();
            if (pImpl->processingThread.joinable())
                pImpl->processingThread.join();
        }
    }

    /*Sets a handler for the OnPacketProcessed event.
    @param PacketProcessedCallback A callback function with the signature: void(const Packet&)*/
    void ProcessingBuffer::setOnPacketProcessedHandler(PacketProcessedCallback callback) {
        pImpl->setOnPacketProcessedHandler(callback);
    }

    /*
    * Shovels shit (data) into the buffer.
    * @param The data to add. Needs to be no bigger than MAX_PACKET_SIZE!
    * @exception Throws std::overflow_error if data was larger than MAX_PACKET_SIZE.
    */
    void ProcessingBuffer::addData(const std::vector<uint8_t>& data) {
        pImpl->addData(data);
    }

    uint8_t ProcessingBuffer::operator[](size_t index) const {
        return pImpl->operator[](index);
    }

    /*The length of the internal buffer.*/
    size_t ProcessingBuffer::bufferCount() const {
        return pImpl->bufferCount();
    }
}