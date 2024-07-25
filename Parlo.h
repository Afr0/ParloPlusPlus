/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________
*/

#pragma once

#include <memory>
#include <vector>
#include <asio.hpp>
#include "Socket.h"
#include "BlockingQueue.h"
#include "ProcessingBuffer.h"
#include "Logger.h"
#include "ParloIDs.h"

#ifdef PARLO_EXPORTS
#define PARLO_API __declspec(dllexport)
#else
#define PARLO_API __declspec(dllimport)
#endif

namespace Parlo
{
    class Listener; //Forward declaration

    /*The NetworkClient class represents a NetworkClient that can connect to a remote endpoint and receive data.
    It can also represent a connected client on the server side.*/
    class PARLO_API NetworkClient : public std::enable_shared_from_this<NetworkClient>
    {
    private:
        Socket& socket;
        Listener* listener;
        size_t maxPacketSize;
        bool applyCompression = false;

        asio::streambuf recvBuffer;
        ProcessingBuffer processingBuffer;
        std::atomic<bool> connected{ true };

        /*Event fired when the server notified that it's disconnecting.*/
        std::function<void(const std::shared_ptr<NetworkClient>&)> onServerDisconnectedHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&)> onClientDisconnectedHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&)> onConnectionLostHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&)> onReceivedHeartbeatHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> onReceivedDataHandler;

        static const int COMPRESSION_BUFFER_SIZE = 32768;

        /*Asynchronously receives data from this NetworkClient's connected endpoint.*/
        void receiveAsync();

        /*Should data be compressed based on the RTT (Round Trip Time)?
        @param data The data to consider.
        @param rtt The round trip time.*/
        bool shouldCompressData(const std::vector<uint8_t>& data, int rtt);

        /*Compresses data.
        @param data The data to compress.
        @returns The compressed data as a std::vector<uint8_t>
        @throws std::runtime_error if zLib couldn't be initialized or data couldn't be compressed.
        @throws std::invalid_argument if data was null.*/
        std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);

        /*Decompresses data.
        @param data The data to compress.
        @returns The decompressed data as a std::vector<uint8_t>
        @throws std::runtime_error if zLib couldn't be initialized or data couldn't be decompressed.
        @throws std::invalid_argument if data was null.*/
        std::vector<uint8_t> decompressData(const std::vector<uint8_t>& data);

        std::mutex aliveMutex;
        /*Is this client's connection still alive?*/
        std::atomic<bool> isAlive = true;

        std::mutex heartbeatsMutex;
        /*How many missed heartbeats do we have?*/
        std::atomic<int> missedHeartbeats = 0;

        /*The last RTT - I.E Round Trip Time, in millisecs.*/
        int lastRTT;
    public:
        NetworkClient(Socket& socket, Listener* listener, size_t maxPacketSize);
        ~NetworkClient() = default;

        /*Asynchronously connects to a remote endpoint.
        @param endpoint The remote endpoint to connect to.*/
        void connectAsync(const asio::ip::tcp::endpoint endpoint);

        /*Sends data asynchronously.
        @param data The data to send.*/
        void sendAsync(const std::vector<uint8_t>& data);

        /*Sets a handler for the event fired when a client disconnects from a server. This handler should be set by a Listener instance.*/
        void setOnClientDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

        /*Sets a handler for the event fired when this NetworkClient instance lost its connection.*/
        void setOnConnectionLostHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

        /*Sets a handler for the event fired when a server disconnects.*/
        void setOnServerDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

        /*Sets a handler for the event fired when a heartbeat was received.*/
        void setOnReceivedHeartbeatHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler);

        /*Sets a handler for the event fired when data was received.*/
        void setOnReceivedDataHandler(std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> handler);
    };

    /*A Listener is used to listen for incoming connections.*/
    class PARLO_API Listener
    {
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
        Listener(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint);
        ~Listener();

        /*Starts accepting new connections.*/
        void startAccepting();
        /*Stops accepting new connections.*/
        void stopAccepting();

        /*@returns A reference to this Listener's internal list of connected clients.*/
        BlockingQueue<std::shared_ptr<NetworkClient>>& Clients() { return networkClients; }
    };

    /*A packet is used to send data across a network.*/
    class PARLO_API Packet
    {
    public:
        //Default constructor to avoid warning about redefinition of Packet class.
        Packet() = default;

        /* Constructs a new Packet instance for TCP transmission.
        @param id The ID of the packet.
        @param serializedData The serialized data of the packet.
        @param isPacketCompressed Is the packet compressed?*/
        Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed = false);

        /* Constructs a new Packet instance for UDP transmission.
        @param id The ID of the packet.
        @param serializedData The serialized data of the packet.
        @param isPacketCompressed Is the packet compressed?
        @param isPacketReliable Should this packet be transferred reliably?*/
        Packet(uint8_t id, const std::vector<uint8_t>& serializedData, bool isPacketCompressed = false, bool isPacketReliable);

        ~Packet() {}

        uint8_t getID() const { return id; }
        uint8_t getIsCompressed() const { return isCompressed; }
        uint16_t getLength() const { return length; }
        const std::vector<uint8_t>& getData() const { return data; }

        std::vector<uint8_t> virtual buildPacket() const;

    protected:
        std::vector<uint8_t> data;

    private:
        uint8_t id;
        uint8_t isCompressed;
        uint8_t isReliable;
        uint16_t length;
        bool isUDP;
    };
}