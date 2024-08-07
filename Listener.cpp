/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#include "pch.h"
#include "BlockingQueue.h"
#include "Parlo.h"
#include "Logger.h"
#include "Socket.h"
#include <memory>

namespace Parlo
{
    /*The Listener is used to accept incoming connections.*/
    class Listener::Impl : public std::enable_shared_from_this<Impl> {
        public:
            Impl(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint)
                : ioContext(context), acceptor(context, endpoint), listenerSocket(context) {
            }

            /*Starts accepting new connections.*/
            void startAccepting();
            /*Stops accepting new connections.*/
            void stopAccepting();

            BlockingQueue<std::shared_ptr<NetworkClient>>& clients();

            /*Sets a handler for the event fired when a client connected.*/
            void setOnClientConnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

            std::shared_ptr<Listener> getListenerSharedPtr() {
                return owner->getSharedPtr();
            }

        private:
            asio::io_context& ioContext;
            asio::ip::tcp::acceptor acceptor;
            BlockingQueue<std::shared_ptr<NetworkClient>> networkClients;
            std::atomic<bool> running{ false };
            std::atomic<bool> applyCompression{ false };
            std::future<void> acceptFuture;
            Socket listenerSocket;

            using ClientConnectedHandler = std::function<void(const std::shared_ptr<NetworkClient>& client)>;
            ClientConnectedHandler onClientConnected;

            using ClientDisconnectedHandler = std::function<void(const std::shared_ptr<NetworkClient>& client)>;
            ClientDisconnectedHandler onClientDisconnected;

            /*Asynchronously accepts new connections.*/
            void acceptAsync();
            void NewClient_OnClientConnected(const std::shared_ptr<NetworkClient>& client);

            void NewClient_OnClientDisconnected(const std::shared_ptr<NetworkClient>& client);
            void NewClient_OnConnectionLost(const std::shared_ptr<NetworkClient>& client);

            void setApplyCompression(bool apply);

            Listener* owner;

            friend class Listener;
    };

    /*Constructs a new Listener instance that listens for incoming connections.
    @param context An asio::io_context instance.
    @param endpoint The remote endpoint to connect to.*/
    Listener::Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint) : 
        pImpl(std::make_unique<Listener::Impl>(context, endpoint)) {
        pImpl->owner = this;
    }

    Listener::~Listener() {
        stopAccepting();
    }

    /*Starts accepting new connections.*/
    void Listener::Impl::startAccepting()
    {
        running = true;
        acceptFuture = std::async(std::launch::async, &Listener::Impl::acceptAsync, shared_from_this());
    }

    /*Stops accepting new connections.*/
    void Listener::Impl::stopAccepting() {
        running = false;

        if (acceptFuture.valid())
            acceptFuture.wait();

        //Member objects' destructors are called automatically, so no need to invoke them.
    }

    BlockingQueue<std::shared_ptr<NetworkClient>>& Listener::Impl::clients() {
        return networkClients;
    }

    /*Asynchronously accepts new connections.*/
    void Listener::Impl::acceptAsync() {
        try {
            while (running) {

                listenerSocket.acceptAsync(acceptor, [this](std::error_code ec, Socket& acceptedSocket) {
                    if (!ec) {
                        Logger::Log("New client connected!", LogLevel::info);

                        //Set socket options
                        acceptedSocket.setLinger(true, std::chrono::seconds(5));

                        //Create new client
                        auto newClient = std::make_shared<NetworkClient>(
                            acceptedSocket,
                            getListenerSharedPtr()//shared_from_this()
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
                            onClientConnected(newClient);
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

    /*Should compression be applied to packets in incoming connections? Defaults to false.
    @param apply Apply compression? Defaults to false.*/
    void Listener::Impl::setApplyCompression(bool apply) {
        applyCompression = apply;
    }

    /*Set a function to be called when a client connects to this Listener instance.
    @param handler The function to be called.*/
    void Listener::Impl::setOnClientConnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onClientConnected = handler;
    }

    void Listener::Impl::NewClient_OnClientConnected(const std::shared_ptr<NetworkClient>& client) {

    }

    /*A client disconnected from this Listener instance.
    @param client The client that disconnected.*/
    void Listener::Impl::NewClient_OnClientDisconnected(const std::shared_ptr<NetworkClient>& client) {
        //Handle client disconnection
        Logger::Log("Client disconnected!", LogLevel::info);

        if (onClientDisconnected)
            onClientDisconnected(client);

        networkClients.take(client);
    }

    /*A client lost its connection to this Listener instance.
    @param client The client that lost its connection.*/
    void Listener::Impl::NewClient_OnConnectionLost(const std::shared_ptr<NetworkClient>& client) {
        //Handle connection loss
        Logger::Log("Client connection lost!", LogLevel::info);

        if (onClientDisconnected)
            onClientDisconnected(client);

        networkClients.take(client);
    }

    void Listener::startAccepting() {
        pImpl->startAccepting();
    }

    /*Stops accepting new connections.*/
    void Listener::stopAccepting() {
        pImpl->stopAccepting();
    }

    BlockingQueue<std::shared_ptr<NetworkClient>>& Listener::clients() {
        return pImpl->clients();
    }

    /*Should compression be applied to packets in incoming connections? Defaults to false.
    @param apply Apply compression? Defaults to false.*/
    void Listener::setApplyCompression(bool apply) {
        pImpl->setApplyCompression(apply);
    }

    /*Set a function to be called when a client connects to this Listener instance.
    @param handler The function to be called.*/
    void Listener::setOnClientConnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        pImpl->setOnClientConnectedHandler(handler);
    }
}