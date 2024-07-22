/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "HeartbeatPacket.h"
#include <cstring>
#include <stdexcept>

namespace Parlo
{
    /*Constructs a new HeartbeatPacket.
    @param id The packet's id.
    @param serializedData The packet's data.
    @param isPacketCompressed Is the packet compressed?*/
    HeartbeatPacket::HeartbeatPacket(uint8_t id, std::chrono::milliseconds timeSinceLast, const std::vector<uint8_t>& serializedData, bool isPacketCompressed)
        : Packet(id, serializedData, isPacketCompressed), m_timeSinceLast(timeSinceLast), m_sentTimestamp(std::chrono::system_clock::now())
    {
    }

    /*The timestamp for the elapsed time since the last Heartbeat packet was sent.*/
    std::chrono::milliseconds HeartbeatPacket::getTimeSinceLast() const
    {
        return m_timeSinceLast;
    }

    /*The timestamp for when this Heartbeat packet was sent.*/
    std::chrono::system_clock::time_point HeartbeatPacket::getSentTimestamp() const
    {
        return m_sentTimestamp;
    }

    /*Serializes a HeartbeatPacket instance into a byte array.
    @returns A byte array.*/
    std::shared_ptr<std::vector<uint8_t>> HeartbeatPacket::toByteArray() const
    {
        auto bytes = std::make_shared<std::vector<uint8_t>>(sizeof(int64_t) * 2);
        int64_t timeSinceLastCount = m_timeSinceLast.count();
        auto sentTimestampCount = std::chrono::duration_cast<std::chrono::milliseconds>(m_sentTimestamp.time_since_epoch()).count();

        std::memcpy(bytes->data(), &timeSinceLastCount, sizeof(timeSinceLastCount));
        std::memcpy(bytes->data() + sizeof(timeSinceLastCount), &sentTimestampCount, sizeof(sentTimestampCount));

        return bytes;
    }

    /*Deserializes a byte array into a HeartbeatPacket instance.
    @param arrBytes The byte array to deserialize.*/
    HeartbeatPacket HeartbeatPacket::byteArrayToObject(const std::shared_ptr<std::vector<uint8_t>>& arrBytes, bool isPacketCompressed)
    {
        if (arrBytes->size() < sizeof(int64_t) * 2 + 1) { // +1 for the id byte
            throw std::runtime_error("HeartbeatPacket::byteArrayToObject(): Invalid byte array size for HeartbeatPacket.");
        }

        int64_t timeSinceLastCount;
        int64_t sentTimestampCount;

        std::memcpy(&timeSinceLastCount, arrBytes->data() + 1, sizeof(timeSinceLastCount)); // +1 to skip the id byte
        std::memcpy(&sentTimestampCount, arrBytes->data() + 1 + sizeof(timeSinceLastCount), sizeof(sentTimestampCount));

        auto timeSinceLast = std::chrono::milliseconds(timeSinceLastCount);
        auto sentTimestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(sentTimestampCount));

        std::vector<uint8_t> serializedData(arrBytes->begin(), arrBytes->end());
        HeartbeatPacket packet(arrBytes->data()[0], timeSinceLast, serializedData, isPacketCompressed);
        packet.m_sentTimestamp = sentTimestamp;
        return packet;
    }
}