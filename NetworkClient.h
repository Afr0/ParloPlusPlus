/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________
*/

#pragma once

#include <asio.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <zlib.h>
#include <chrono>
#include "Logger.h"
#include "Socket.h"
#include "ProcessingBuffer.h"
#include "ParloIDs.h"

namespace Parlo
{
    class Packet;
    class Listener; //Forward declaration
    class HeartbeatPacket;

    class NetworkClient : public std::enable_shared_from_this<NetworkClient> {
    public:
        NetworkClient(Socket& socket, Listener* listener, size_t maxPacketSize);
        ~NetworkClient() {};

        Socket* getSocket();

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

        void setApplyCompression(bool apply);

        /*Sends data asynchronously.
        @param data The data to send.*/
        void sendAsync(const std::vector<uint8_t>& data);

        /*Asynchronously connects to a remote endpoint.
        @param endpoint The remote endpoint to connect to.*/
        void connectAsync(const asio::ip::tcp::endpoint endpoint);

        /*Asynchronously disconnects from a remote endpoint.
        @param sendDisconnectMessage Whether or not to send a disconnection message to the other party. Defaults to true.*/
        void disconnectAsync(bool sendDisconnectMessage = true);

    private:
        Socket& socket;
        Listener* listener;
        size_t maxPacketSize;

        /*Only packets larger than this many bytes will be compressed.
        Defaults to 1024 bytes, IE Parlo's max packet size.*/
        const int COMPRESSION_THRESHOLD = 1024;
        
        /*The time in milliseconds for the RTT above which packets will be compressed.*/
        const int RTT_COMPRESSION_THRESHOLD = 100;
        bool applyCompression = false;

        asio::streambuf recvBuffer;
        ProcessingBuffer processingBuffer;
        std::atomic<bool> connected{ true };

        /*Event fired when the server notified that it's disconnecting.*/
        std::function<void(const std::shared_ptr<NetworkClient>&)> onServerDisconnectedHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&)> onClientDisconnectedHandler;
        std::function<void(const std::shared_ptr<NetworkClient>&)> onConnectionLostHandler;

        /*Size of compression and decompression buffer. 
        Zlib benefits from having a larger buffer than 
        MAX_PACKET_SIZE (in ProcessingBuffer) 
        to compress and decompress data.*/
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

        std::thread sendHeartbeatsThread;
        /*Should we stop sending heartbeats? */
        std::atomic<bool> stopSendingHeartbeats = false;

        std::thread heartbeatCheckThread;

        std::mutex aliveMutex;
        /*Is this client's connection still alive?*/
        std::atomic<bool> isAlive = true;

        std::mutex heartbeatsMutex;
        /*How many missed heartbeats do we have?*/
        std::atomic<int> missedHeartbeats = 0;

        /*Should we stop checking for missed heartbeats?*/
        std::atomic<bool> stopCheckMissedHeartbeats = false;
        /*The last RTT - I.E Round Trip Time, in millisecs.*/
        int lastRTT;

        /*Event fired when a heartbeat is received.*/
        std::function<void(const std::shared_ptr<NetworkClient>&)> onReceivedHeartbeatHandler;

        /*Event fired when the NetworkClient instance received data.*/
        std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> onReceivedDataHandler;
    };
}
