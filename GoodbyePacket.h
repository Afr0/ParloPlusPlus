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
#include <stdexcept>

/*Number of seconds for server or client to disconnect by default.*/
enum ParloDefaultTimeouts : int
{
    /// <summary>
    /// Server's default timeout.
    /// </summary>
    Server = 60,

    /// <summary>
    /// Client's default timeout.
    /// </summary>
    Client = 5,
};

/*Internal packet sent by client and server before disconnecting.*/
class GoodbyePacket 
{
public:
    GoodbyePacket() : timeout(0) {} //Default constructor
    GoodbyePacket(int timeoutSeconds);
    ~GoodbyePacket() {};

    //Convert to and from byte array
    std::vector<uint8_t> toByteArray() const;
    static GoodbyePacket fromByteArray(const std::vector<uint8_t>& arrBytes);

    //Getters
    std::chrono::seconds getTimeOut() const;
    std::chrono::system_clock::time_point getSentTime() const;

private:
    std::chrono::seconds timeout;
    std::chrono::system_clock::time_point sentTime;
};
