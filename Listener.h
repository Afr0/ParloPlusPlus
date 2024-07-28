/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________
*/

#pragma once

#include "Socket.h"
#include "BlockingQueue.h"
#include <future>
#include <atomic>
#include <asio.hpp>
#include "Logger.h"
#include "ProcessingBuffer.h"

namespace Parlo
{
	class NetworkClient; //Forward declaration

	class Listener {
	private:
		asio::io_context& ioContext;
		asio::ip::tcp::acceptor acceptor;
		BlockingQueue<std::shared_ptr<NetworkClient>> networkClients;
		std::atomic<bool> running{ false };
		std::atomic<bool> applyCompression{ false };
		std::future<void> acceptFuture;
		Socket listenerSocket;

		/*Asynchronously accepts new connections.*/
		void acceptAsync();
		void NewClient_OnClientConnected(std::error_code error, Socket& socket);

		void NewClient_OnClientDisconnected(const std::shared_ptr<NetworkClient>& client);
		void NewClient_OnConnectionLost(const std::shared_ptr<NetworkClient>& client);
	public:
		/*Constructs a new Listener instance that listens for incoming connections.
		@param context An asio::io_context instance.
		@param endpoint The remote endpoint to connect to.*/
		Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint);
		~Listener();

		/*@returns A reference to this Listener's internal list of connected clients.*/
		BlockingQueue<std::shared_ptr<NetworkClient>>& Clients() { return networkClients; }

		/*Starts accepting new connections.*/
		void startAccepting();
		/*Stops accepting new connections.*/
		void stopAccepting();

		void setApplyCompression(bool apply);

		using ClientConnectedHandler = std::function<void(std::error_code, Socket&)>;
		ClientConnectedHandler onClientConnected;
	};
}