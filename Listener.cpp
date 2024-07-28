/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "Listener.h"
#include "NetworkClient.h"

namespace Parlo
{
    /*Constructs a new Listener instance that listens for incoming connections.
    @param context An asio::io_context instance.
    @param endpoint The remote endpoint to connect to.*/
    Listener::Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint)
        : ioContext(context), acceptor(context, endpoint), listenerSocket(context)
    {
        //Use placeholders for std::error_code and Socket arguments.
        onClientConnected = std::bind(&Listener::NewClient_OnClientConnected, this, std::placeholders::_1, std::placeholders::_2);
    }

    Listener::~Listener() {
        stopAccepting();
    }

    /*Asynchronously accepts new connections.*/
	void Listener::acceptAsync() {
        try {
            while (running) {
                if (running) 
                    break;

                listenerSocket.acceptAsync(acceptor, [this](std::error_code ec, Socket& acceptedSocket) {
                    if (!ec) {
                        Logger::Log("New client connected!", LogLevel::info);

                        //Set socket options
                        acceptedSocket.setLinger(true, std::chrono::seconds(5));

                        //Create new client
                        auto newClient = std::make_shared<NetworkClient>(
                            acceptedSocket,
                            this
                        );

                        //Set up event handlers
                        newClient->setOnClientDisconnectedHandler(
                            [this](const std::shared_ptr<NetworkClient>& client) {
                                this->NewClient_OnClientDisconnected(client);
                            }
                        );
                        newClient->setOnConnectionLostHandler(
                            [this](const std::shared_ptr<NetworkClient>& client) {
                                this->NewClient_OnConnectionLost(client);
                            }
                        );

                        if (applyCompression)
                            newClient->setApplyCompression(true);

                        networkClients.add(newClient);

                        if (onClientConnected)
                            onClientConnected(ec, *newClient->getSocket());
                    }
                    else
                        Logger::Log("Error accepting connection: " + ec.message(), LogLevel::error);

                    //Continue accepting new connections.
                    acceptAsync();
                });

                //Sleep for a short time to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (const std::exception& e) {
            Logger::Log("Exception in Listener::acceptAsync(): " + std::string(e.what()), LogLevel::error);
        }
	}

    /*Starts accepting new connections.*/
	void Listener::startAccepting() {
		running = true;
		acceptFuture = std::async(std::launch::async, &Listener::acceptAsync, this);
	}

    /*Stops accepting new connections.*/
	void Listener::stopAccepting() {
		running = false;
		if (acceptFuture.valid())
			acceptFuture.wait();

        //Member objects' destructors are called automatically, so no need to invoke them.
	}

    /*Should compression be applied to packets in incoming connections? Defaults to false.*/
    void Listener::setApplyCompression(bool apply) {
        applyCompression = apply;
    }

    void Listener::NewClient_OnClientConnected(std::error_code error, Socket& socket) {

    }

    void Listener::NewClient_OnClientDisconnected(const std::shared_ptr<NetworkClient>& client) {
        //Handle client disconnection
    }

    void Listener::NewClient_OnConnectionLost(const std::shared_ptr<NetworkClient>& client) {
        //Handle connection loss
    }
}