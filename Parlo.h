/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo Library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <functional>
#include "PacketHeaders.h"
#include "BlockingQueue.h"
#include <asio.hpp>

#if defined(_WIN32) || defined(_WIN64)
    #ifdef PARLO_EXPORTS
        #define PARLO_API __declspec(dllexport)
    #else
        #define PARLO_API __declspec(dllimport)
    #endif
#else
    #ifdef PARLO_EXPORTS
        #define PARLO_API __attribute__((visibility("default")))
    #else
        #define PARLO_API
    #endif
#endif

#define PARLO_TEMPLATE extern

namespace Parlo
{
    class Listener; //Forward declaration
    class Socket;

    const int MAX_PACKET_SIZE = 1024;

    /*A packet is used to send data across a network.*/
    class Packet
    {
    public:
        PARLO_API Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed = false);
        PARLO_API Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed, bool isPacketReliable);
        PARLO_API ~Packet();

        PARLO_API uint8_t getID() const;
        PARLO_API uint8_t getIsCompressed() const;
        PARLO_API uint16_t getLength() const;
        PARLO_API const std::vector<uint8_t>& getData() const;

        PARLO_API std::vector<uint8_t> buildPacket() const;

    protected:
        std::vector<uint8_t> data;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    /*Buffer used to process incoming data. Only consumed by the Parlo tests library.*/
    class ProcessingBuffer
    {
    public:
        PARLO_API ProcessingBuffer();
        PARLO_API ~ProcessingBuffer();

        using PacketProcessedCallback = std::function<void(const Packet&)>;
        PARLO_API void setOnPacketProcessedHandler(PacketProcessedCallback callback);

        PARLO_API void addData(const std::vector<uint8_t>& data);

        PARLO_API uint8_t operator[](size_t index) const;

        PARLO_API size_t bufferCount() const;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };

    /*The NetworkClient class represents a NetworkClient that can connect to a remote endpoint and receive data.*/
    class NetworkClient : public std::enable_shared_from_this<NetworkClient>
    {
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;

    public:
        PARLO_API NetworkClient(Socket& socket, std::shared_ptr<Listener> listener);
        PARLO_API NetworkClient(Socket& socket);
        PARLO_API ~NetworkClient();

        PARLO_API Socket* getSocket();

        PARLO_API void connectAsync(const asio::ip::tcp::endpoint endpoint);
        PARLO_API void sendAsync(const std::vector<uint8_t>& data);
        
        /*Asynchronously disconnects from a remote endpoint.
        @param sendDisconnectMessage Whether or not to send a disconnection message to the other party. Defaults to true.*/
        PARLO_API void disconnectAsync(bool sendDisconnectMessage = true);

        PARLO_API void setApplyCompression(bool apply);

        PARLO_API std::shared_ptr<NetworkClient> getSharedPtr() {
            return shared_from_this();
        }

        void setOnClientDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);
        void setOnConnectionLostHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);
        void setOnServerDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);
        void setOnReceivedHeartbeatHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);
        void setOnReceivedDataHandler(std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> handler);
    };

    /*A Listener is used to listen for incoming connections.*/
    class Listener : public std::enable_shared_from_this<Listener>
    {
    public:
        PARLO_API Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint);
        PARLO_API ~Listener();

        PARLO_API void startAccepting();
        PARLO_API void stopAccepting();
        PARLO_API BlockingQueue<std::shared_ptr<NetworkClient>>& clients();

        void setApplyCompression(bool apply);

        void setOnClientConnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

        PARLO_API std::shared_ptr<Listener> getSharedPtr() {
            return shared_from_this();
        }
    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };
}
