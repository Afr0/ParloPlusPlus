/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo Library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "Parlo.h"

namespace Parlo 
{
	using onPacketReceived = std::function<void(std::shared_ptr<NetworkClient>, std::shared_ptr<Packet>)>;

	class PacketHandler
	{
	public:
		PacketHandler(uint8_t id, bool encrypted, onPacketReceived handler) : id(id), encrypted(encrypted), handler(handler)
		{

		}

		uint8_t getID() const { return id; }
		bool isEncrypted() const { return encrypted; }
		void handlePacket(std::shared_ptr<NetworkClient> client, std::shared_ptr<Packet> packet) 
		{
			if (handler) 
				handler(client, packet);
		}

		~PacketHandler() {};
	private:
		uint8_t id;
		bool encrypted;
		onPacketReceived handler;
	};
}
