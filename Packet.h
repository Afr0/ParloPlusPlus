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
#include <cstdint>
#include <stdexcept>
#include "PacketHeaders.h"

namespace Parlo
{
    class Packet
    {
    public:
        /// <summary>
        /// Constructs a new Packet instance for TCP transmission.
        /// </summary>
        /// <param name="id">The ID of the packet.</param>
        /// <param name="serializedData">The serialized data of the packet.</param>
        /// <param name="isPacketCompressed">Is the packet compressed?</param>
        Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed = false)
            : id(id), isCompressed(isPacketCompressed ? 1 : 0), isReliable(0),
            length(static_cast<uint16_t>(PacketHeaders::STANDARD + serializedData.size())),
            data(serializedData), isUDP(false)
        {
            if (serializedData.empty())
                throw std::invalid_argument("SerializedData cannot be null!");
        }

        /// <summary>
        /// Constructs a new Packet instance for UDP transmission.
        /// </summary>
        /// <param name="id">The ID of the packet.</param>
        /// <param name="serializedData">The serialized data of the packet.</param>
        /// <param name="isPacketCompressed">Is the packet compressed?</param>
        /// <param name="isPacketReliable">Is this packet supposed to be transferred reliably?</param>
        Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed, bool isPacketReliable)
            : id(id), isCompressed(isPacketCompressed ? 1 : 0), isReliable(isPacketReliable ? 1 : 0),
            length(static_cast<uint16_t>(PacketHeaders::UDP + serializedData.size())),
            data(serializedData), isUDP(true)
        {
            if (serializedData.empty())
                throw std::invalid_argument("SerializedData cannot be null!");
        }

        ~Packet() {}

        uint8_t getID() const { return id; }
        uint8_t getIsCompressed() const { return isCompressed; }
        uint16_t getLength() const { return length; }
        const std::vector<uint8_t>& getData() const { return data; }

        std::vector<uint8_t> virtual buildPacket() const 
        {
            std::vector<uint8_t> packetData;
            packetData.push_back(id);
            packetData.push_back(isCompressed);

            if (isUDP)
                packetData.push_back(isReliable);

            packetData.push_back(static_cast<uint8_t>(length & 0xFF));
            packetData.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));

            packetData.insert(packetData.end(), data.begin(), data.end());

            return packetData;
        }

    protected:
        std::vector<uint8_t> data;

    private:
        uint8_t id;
        uint8_t isCompressed;
        uint8_t isReliable;
        uint16_t length;
        bool isUDP;
    };
}
