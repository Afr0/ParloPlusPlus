#include "pch.h"
#include "ProcessingBuffer.h"
#include "Packet.h"

namespace Parlo
{
	ProcessingBuffer::ProcessingBuffer() {
		stopProcessing = false;
		processingThread = std::thread(&ProcessingBuffer::processPackets, this);
	}

    /*Sets a handler for the OnPacketProcessed event.
    @param PacketProcessedCallback A callback function with the signature: void(const Packet&)*/
    void ProcessingBuffer::setOnPacketProcessedHandler(PacketProcessedCallback callback) {
        onPacketProcessedHandler = callback;
    }

    /*
    * Shovels shit (data) into the buffer.
    * @param The data to add. Needs to be no bigger than MAX_PACKET_SIZE!
    * @exception Throws std::overflow_error if data was larger than MAX_PACKET_SIZE.
    */
    void ProcessingBuffer::addData(const std::vector<uint8_t>& data) {
        if (data.size() > MAX_PACKET_SIZE)
            throw std::overflow_error("Buffer overflow exception");

        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto byte : data)
                internalBuffer.push(byte);
        }
        cv.notify_one();
    }

    /*Processes packets.*/
	void ProcessingBuffer::processPackets() {
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
    void ProcessingBuffer::readHeader() {
        currentID = internalBuffer.front(); internalBuffer.pop();
        isCompressed = internalBuffer.front(); internalBuffer.pop();

        uint16_t length = 0;
        for (int i = 0; i < 2; ++i) {
            length |= (internalBuffer.front() << (i * 8));
            internalBuffer.pop();
        }
        currentLength = length;

        hasReadHeader = true;
    }

    ProcessingBuffer::~ProcessingBuffer() {
        stopProcessing = true;
        cv.notify_all();
        if (processingThread.joinable())
            processingThread.join();
    }
}