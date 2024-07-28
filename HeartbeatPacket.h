/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <chrono>
#include <vector>
#include <cstdint>
#include <memory>

namespace Parlo
{
    class HeartbeatPacket {
    public:
        HeartbeatPacket(std::chrono::milliseconds tSinceLast);

        /*The timestamp for the elapsed time since the last Heartbeat packet was sent.*/
        std::chrono::milliseconds getTimeSinceLast() const;
        /*The timestamp for when this Heartbeat packet was sent.*/
        std::chrono::system_clock::time_point getSentTimestamp() const;

        std::shared_ptr<std::vector<uint8_t>> toByteArray() const;
        static HeartbeatPacket byteArrayToObject(const std::shared_ptr<std::vector<uint8_t>>& arrBytes, bool isPacketCompressed = false);

    private:
        std::chrono::milliseconds timeSinceLast;
        std::chrono::system_clock::time_point sentTimestamp;
    };
}