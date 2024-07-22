/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "GoodbyePacket.h"
#include <cstring>

GoodbyePacket::GoodbyePacket(int timeoutSeconds)
    : timeout(timeoutSeconds),
    sentTime(std::chrono::system_clock::now()) 
{
}

/// <summary>
/// Serializes this class to a byte array.
/// </summary>
/// <returns>A byte array representation of this class.</returns>
std::vector<uint8_t> GoodbyePacket::toByteArray() const 
{
    std::vector<uint8_t> bytes;

    //Serialize timeout (in seconds)
    auto timeoutSec = timeout.count();
    bytes.insert(bytes.end(), reinterpret_cast<uint8_t*>(&timeoutSec), reinterpret_cast<uint8_t*>(&timeoutSec) + sizeof(timeoutSec));

    //Serialize sentTime (as time since epoch in seconds)
    auto sentTimeSec = std::chrono::duration_cast<std::chrono::seconds>(sentTime.time_since_epoch()).count();
    bytes.insert(bytes.end(), reinterpret_cast<uint8_t*>(&sentTimeSec), reinterpret_cast<uint8_t*>(&sentTimeSec) + sizeof(sentTimeSec));

    return bytes;
}

/// <summary>
/// Deserializes this class from a byte array.
/// </summary>
/// <returns>A GoodbyePacket instance.</returns>
GoodbyePacket GoodbyePacket::fromByteArray(const std::vector<uint8_t>& arrBytes) 
{
    if (arrBytes.size() < sizeof(std::chrono::seconds::rep) + sizeof(std::chrono::system_clock::time_point::duration::rep))
        throw std::runtime_error("Byte array too small for GoodbyePacket deserialization.");

    GoodbyePacket packet;
    std::size_t offset = 0;

    //Deserialize timeout
    std::chrono::seconds::rep timeoutSec;
    std::memcpy(&timeoutSec, arrBytes.data() + offset, sizeof(timeoutSec));
    offset += sizeof(timeoutSec);
    packet.timeout = std::chrono::seconds(timeoutSec);

    //Deserialize sentTime
    std::chrono::system_clock::time_point::duration::rep sentTimeSec;
    std::memcpy(&sentTimeSec, arrBytes.data() + offset, sizeof(sentTimeSec));
    packet.sentTime = std::chrono::system_clock::time_point(std::chrono::seconds(sentTimeSec));

    return packet;
}
