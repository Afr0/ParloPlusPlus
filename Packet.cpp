/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "Parlo.h"
#include <memory>

namespace Parlo
{
    class Packet::Impl
    {
    public:
        Impl(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed = false)
            : id(id), isCompressed(isPacketCompressed ? 1 : 0), isReliable(0),
            length(static_cast<uint16_t>(PacketHeaders::STANDARD + serializedData.size())),
            data(serializedData), isUDP(false)
        {
            if (serializedData.empty())
                throw std::invalid_argument("Packet: SerializedData cannot be null!");
        }

        Impl(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed, bool isPacketReliable)
            : id(id), isCompressed(isPacketCompressed ? 1 : 0), isReliable(isPacketReliable ? 1 : 0),
            length(static_cast<uint16_t>(PacketHeaders::UDP + serializedData.size())),
            data(serializedData), isUDP(true)
        {
            if (serializedData.empty())
                throw std::invalid_argument("Packet: SerializedData cannot be null!");
        }

        ~Impl() {}

        uint8_t getID() const { return id; }
        uint8_t getIsCompressed() const { return isCompressed; }
        uint16_t getLength() const { return length; }
        const std::vector<uint8_t>& getData() const { return data; }

        /*Builds a packet ready for transmission by turning it into serialized data.*/
        std::vector<uint8_t> buildPacket() const
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

    private:
        uint8_t id;
        uint8_t isCompressed;
        uint8_t isReliable;
        uint16_t length;
        std::vector<uint8_t> data;
        bool isUDP;

        friend class Packet;
    };

    /*Constructs a new Packet instance for TCP transmission.
    @param id The ID of the packet.
    @param serializedData The serialized data of the packet.
    @param isPacketCompressed Is the packet compressed?*/
    Packet::Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed)
        : pImpl(std::make_unique<Impl>(id, serializedData, isPacketCompressed))
    {
    }

    /*Constructs a new Packet instance for UDP transmission.
    @param id The ID of the packet.
    @param serializedData The serialized data of the packet.
    @param isPacketCompressed Is the packet compressed?
    @param isPacketReliable Is this packet supposed to be transferred reliably?*/
    Packet::Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed, bool isPacketReliable)
        : pImpl(std::make_unique<Impl>(id, serializedData, isPacketCompressed, isPacketReliable))
    {
    }

    Packet::~Packet() = default;

    /*Builds a packet ready for transmission by turning it into serialized data.*/
    std::vector<uint8_t> Packet::buildPacket() const
    {
        return pImpl->buildPacket();
    }

    uint8_t Packet::getID() const
    {
        return pImpl->getID();
    }

    uint8_t Packet::getIsCompressed() const
    {
        return pImpl->getIsCompressed();
    }

    uint16_t Packet::getLength() const
    {
        return pImpl->getLength();
    }

    const std::vector<uint8_t>& Packet::getData() const
    {
        return pImpl->getData();
    }
}
