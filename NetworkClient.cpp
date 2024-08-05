/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________
*/

#include "pch.h"
#include "HeartbeatPacket.h"
#include "GoodbyePacket.h"
#include "Socket.h"
#include "Logger.h"
#include "ParloIDs.h"
#include "Parlo.h"
#include <zlib.h>
#include <memory>

namespace Parlo
{
    /*The NetworkClient is used to connect to a remote endpoint and receive data.*/
    class NetworkClient::Impl : public std::enable_shared_from_this<Impl> {
    public:
        Impl(Socket& socket, std::shared_ptr<Listener> listener) :
            socket(socket), listener(listener) {}
        Impl(Socket& socket) : socket(socket) {}

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

        std::shared_ptr<NetworkClient> getNetworkClientSharedPtr() {
            return owner->getSharedPtr();
        }

    private:
        Socket& socket;
        std::shared_ptr<Listener> listener;

        /*Only packets larger than this many bytes will be compressed.
        Defaults to 1024 bytes, IE Parlo's max packet size.*/
        const int COMPRESSION_THRESHOLD = 1024;

        /*The time in milliseconds for the RTT above which packets will be compressed.*/
        const int RTT_COMPRESSION_THRESHOLD = 100;
        bool applyCompression = false;

        asio::streambuf recvBuffer;
        ProcessingBuffer processingBuffer;
        std::atomic<bool> connected{ true };

        std::chrono::system_clock::time_point lastHeartbeatSent;

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

        /*Sends a heartbeat to the server. How often is determined by heartbeatInterval.*/
        void sendHeartbeatAsync();
        std::thread sendHeartbeatsThread;
        /*Should we stop sending heartbeats? */
        std::atomic<bool> stopSendingHeartbeats = false;

        /*Periodically checks for missed heartbeats. How often is determined by heartbeatInterval.*/
        void checkForMissedHeartbeats();
        std::thread heartbeatCheckThread;

        std::mutex aliveMutex;
        /*Is this client's connection still alive?*/
        std::atomic<bool> isAlive = true;

        std::mutex heartbeatsMutex;
        /*How many missed heartbeats do we have?*/
        std::atomic<int> missedHeartbeats = 0;

        /*Should we stop checking for missed heartbeats?*/
        std::atomic<bool> stopCheckMissedHeartbeats = false;

        /*Maximum allowed number of missed hearbeats before connection is considered dead.*/
        const int maxMissedHeartbeats = 6;
        int heartbeatInterval = 30; //In seconds.

        /*The last RTT - I.E Round Trip Time, in millisecs.*/
        int lastRTT;

        /*Event fired when a heartbeat is received.*/
        std::function<void(const std::shared_ptr<NetworkClient>&)> onReceivedHeartbeatHandler;

        /*Event fired when the NetworkClient instance received data.*/
        std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> onReceivedDataHandler;

        NetworkClient* owner;

        friend class NetworkClient;
    };

    NetworkClient::NetworkClient(Socket& socket, std::shared_ptr<Listener> listener) :
        pImpl(std::make_unique<NetworkClient::Impl>(socket, listener)) {
        pImpl->owner = this;

        pImpl->processingBuffer.setOnPacketProcessedHandler([this](const Packet& packet) {
            if (packet.getID() == ParloIDs::SGoodbye) { //Server notified client of disconnection.
                if (pImpl->onServerDisconnectedHandler)
                    pImpl->onServerDisconnectedHandler(shared_from_this());

                return;
            }
            if (packet.getID() == ParloIDs::CGoodbye) { //Client notified server of disconnection.
                if (pImpl->onClientDisconnectedHandler)
                    pImpl->onClientDisconnectedHandler(shared_from_this());

                return;
            }
            if (packet.getID() == ParloIDs::Heartbeat) {
                std::lock_guard<std::mutex> aliveLock(pImpl->aliveMutex);
                pImpl->isAlive = true;

                //The std::lock_guard is a RAII (Resource Acquisition Is Initialization) type which 
                //means it acquires the lock when it is created and releases it when it goes out of scope.
                std::lock_guard<std::mutex> heartbeatsLock(pImpl->heartbeatsMutex);
                pImpl->missedHeartbeats = 0;

                auto data = std::make_shared<std::vector<uint8_t>>(packet.getData());
                HeartbeatPacket Heartbeat = HeartbeatPacket::byteArrayToObject(data);

                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                std::chrono::system_clock::time_point sentTimestamp = Heartbeat.getSentTimestamp();
                std::chrono::milliseconds timeSinceLast = Heartbeat.getTimeSinceLast();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTimestamp);
                pImpl->lastRTT = static_cast<int>(duration.count() + timeSinceLast.count());

                if (pImpl->onReceivedHeartbeatHandler)
                    pImpl->onReceivedHeartbeatHandler(shared_from_this());

                return;
            }

            if (packet.getIsCompressed()) {
                auto decompressedData = pImpl->decompressData(packet.getData());
                if (pImpl->onReceivedDataHandler)
                    pImpl->onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), decompressedData, false));
            }
            else {
                if (pImpl->onReceivedDataHandler)
                    pImpl->onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), packet.getData(), false));
            }
            });

        pImpl->heartbeatCheckThread = std::thread(&NetworkClient::Impl::checkForMissedHeartbeats, pImpl.get());
        pImpl->sendHeartbeatsThread = std::thread(&NetworkClient::Impl::sendHeartbeatAsync, pImpl.get());

        pImpl->receiveAsync();
    }

    NetworkClient::NetworkClient(Socket& socket) : pImpl(std::make_unique<NetworkClient::Impl>(socket)) {
        pImpl->processingBuffer.setOnPacketProcessedHandler([this](const Packet& packet) {
            pImpl->owner = this;

            if (packet.getID() == ParloIDs::SGoodbye) { //Server notified client of disconnection.
                if (pImpl->onServerDisconnectedHandler)
                    pImpl->onServerDisconnectedHandler(shared_from_this());
                return;
            }
            if (packet.getID() == ParloIDs::CGoodbye) { //Client notified server of disconnection.
                if (pImpl->onClientDisconnectedHandler)
                    pImpl->onClientDisconnectedHandler(shared_from_this());
                return;
            }
            if (packet.getID() == ParloIDs::Heartbeat) {
                std::lock_guard<std::mutex> aliveLock(pImpl->aliveMutex);
                pImpl->isAlive = true;

                //The std::lock_guard is a RAII (Resource Acquisition Is Initialization) type which 
                //means it acquires the lock when it is created and releases it when it goes out of scope.
                std::lock_guard<std::mutex> heartbeatsLock(pImpl->heartbeatsMutex);
                pImpl->missedHeartbeats = 0;

                auto data = std::make_shared<std::vector<uint8_t>>(packet.getData());
                HeartbeatPacket Heartbeat = HeartbeatPacket::byteArrayToObject(data);

                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                std::chrono::system_clock::time_point sentTimestamp = Heartbeat.getSentTimestamp();
                std::chrono::milliseconds timeSinceLast = Heartbeat.getTimeSinceLast();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTimestamp);
                pImpl->lastRTT = static_cast<int>(duration.count() + timeSinceLast.count());

                if (pImpl->onReceivedHeartbeatHandler)
                    pImpl->onReceivedHeartbeatHandler(shared_from_this());

                return;
            }

            if (packet.getIsCompressed()) {
                auto decompressedData = pImpl->decompressData(packet.getData());
                if (pImpl->onReceivedDataHandler)
                    pImpl->onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), decompressedData, false));
            }
            else {
                if (pImpl->onReceivedDataHandler)
                    pImpl->onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), packet.getData(), false));
            }
            });
    }

    Socket* NetworkClient::Impl::getSocket() {
        return &socket;
    }

    NetworkClient::~NetworkClient() {
        if (!pImpl->stopCheckMissedHeartbeats) {
            pImpl->stopCheckMissedHeartbeats = true;
            if (pImpl->heartbeatCheckThread.joinable())
                pImpl->heartbeatCheckThread.join();
        }

        if (!pImpl->stopSendingHeartbeats) {
            pImpl->stopSendingHeartbeats = true;

            if (pImpl->sendHeartbeatsThread.joinable())
                pImpl->sendHeartbeatsThread.join();
        }
    }

    /*Sets a handler for the event fired when a client disconnects from a server. This handler should be set by a Listener instance.
    @param handler The handler for the event.*/
    void NetworkClient::Impl::setOnClientDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onClientDisconnectedHandler = handler;
    }

    /*Sets a handler for the event fired when this NetworkClient instance lost its connection.
    @param handler The handler for the event.*/
    void NetworkClient::Impl::setOnConnectionLostHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onConnectionLostHandler = handler;
    }

    /*Sets a handler for the event fired when a client disconnects from a server.
    @param handler The handler for the event.*/
    void NetworkClient::Impl::setOnServerDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onServerDisconnectedHandler = handler;
    }

    /*Sets a handler for the event fired when a heartbeat was received.
    @param handler The handler for the event.*/
    void NetworkClient::Impl::setOnReceivedHeartbeatHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onReceivedHeartbeatHandler = handler;
    }

    /*Sets a handler for the event fired when data was received.
    @param handler The handler for the event.*/
    void NetworkClient::Impl::setOnReceivedDataHandler(std::function<void(const std::shared_ptr<NetworkClient>&, 
        const std::shared_ptr<Packet>&)> handler) {
        onReceivedDataHandler = handler;
    }

    /*Should compression be applied to network traffic? Defaults to false.*/
    void NetworkClient::Impl::setApplyCompression(bool apply) {
        applyCompression = apply;
    }

    /*Asynchronously receives data from this NetworkClient's connected endpoint.*/
    void NetworkClient::Impl::receiveAsync() {
        if (!connected) 
            return;

        //Make sure the NetworkClient instance says alive for the duration of the async operation...
        auto self(shared_from_this());
        asio::async_read(socket.native_handle(), recvBuffer, asio::transfer_at_least(1),
            [this, self](std::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    std::vector<uint8_t> data(bytes_transferred);
                    std::istream is(&recvBuffer);
                    is.read(reinterpret_cast<char*>(data.data()), bytes_transferred);

                    try {
                        processingBuffer.addData(data);
                    }
                    catch (const std::overflow_error& e) {
                        Logger::Log("Tried adding too much data into ProcessingBuffer!", LogLevel::warn);
                    }

                    receiveAsync(); //Continue receiving data
                }
                else {
                    Logger::Log("Error in receiveAsync: " + ec.message(), LogLevel::error);
                    connected = false;

                    if (onConnectionLostHandler)
                        onConnectionLostHandler(getNetworkClientSharedPtr());
                }
            }
        );
    }

    /*Sends data asynchronously.
    @param data The data to send.*/
    void NetworkClient::Impl::sendAsync(const std::vector<uint8_t>& data) {
        if (data.empty())
            throw std::invalid_argument("Data cannot be null or empty");
        if (data.size() > Parlo::MAX_PACKET_SIZE)
            throw std::overflow_error("Data size exceeds maximum packet size");

        if (!connected)
            throw std::runtime_error("Socket is not connected");

        std::vector<uint8_t> finalData;

        if (shouldCompressData(data, lastRTT)) {
            auto compressedData = compressData(data);
            Packet *compressedPacket = new Packet(data[0], finalData, true);
            finalData = compressedPacket->buildPacket();
        }
        else
            finalData = data;

        //Make sure the NetworkClient instance says alive for the duration of the async operation...
        auto self(shared_from_this());
        asio::async_write(socket.native_handle(), asio::buffer(data),
            [this, self](std::error_code ec, std::size_t /*bytes_transferred*/) {
                if (ec) {
                    Logger::Log("Error in sendAsync: " + ec.message(), LogLevel::error);
                    connected = false;
                    if (onConnectionLostHandler)
                        onConnectionLostHandler(getNetworkClientSharedPtr());
                }
            });
    }

    /*Should data be compressed based on the RTT (Round Trip Time)?
    @param data The data to consider.
    @param rtt The round trip time.*/
    bool NetworkClient::Impl::shouldCompressData(const std::vector<uint8_t>& data, int rtt) {
        if (data.empty())
            throw std::invalid_argument("Data cannot be null or empty");

        if (!applyCompression)
            return false;

        if (data.size() < COMPRESSION_THRESHOLD)
            return false;

        if (rtt > RTT_COMPRESSION_THRESHOLD)
            return true;

        return false;
    }

    /*Compresses data.
    @param data The data to compress.
    @returns The compressed data as a std::vector<uint8_t>
    @throws std::runtime_error if zLib couldn't be initialized or data couldn't be compressed.
    @throws std::invalid_argument if data was null.*/
    std::vector<uint8_t> NetworkClient::Impl::compressData(const std::vector<uint8_t>& data) {
        if (data.empty())
            throw std::invalid_argument("Data cannot be null or empty");

        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK)
            throw std::runtime_error("deflateInit failed");

        zs.next_in = (Bytef*)data.data();
        zs.avail_in = data.size();

        int ret;
        std::unique_ptr<char[]> outbuffer(new char[COMPRESSION_BUFFER_SIZE]);
        std::vector<uint8_t> outdata;

        do {
            zs.next_out = reinterpret_cast<Bytef*>(outbuffer.get());
            zs.avail_out = COMPRESSION_BUFFER_SIZE;

            ret = deflate(&zs, Z_FINISH);

            if (outdata.size() < zs.total_out) {
                outdata.insert(outdata.end(), outbuffer.get(), outbuffer.get() + zs.total_out - outdata.size());
            }
        } while (ret == Z_OK);

        deflateEnd(&zs);

        if (ret != Z_STREAM_END)
            throw std::runtime_error("Exception during zlib compression");

        return outdata;
    }

    /*Decompresses data.
    @param data The data to compress.
    @returns The decompressed data as a std::vector<uint8_t>
    @throws std::runtime_error if zLib couldn't be initialized or data couldn't be decompressed.
    @throws std::invalid_argument if data was null.*/
    std::vector<uint8_t> NetworkClient::Impl::decompressData(const std::vector<uint8_t>& data) {
        if (data.empty())
            throw std::invalid_argument("Data cannot be null or empty");

        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        if (inflateInit(&zs) != Z_OK)
            throw std::runtime_error("NetworkClient::decompressData: inflateInit failed.");

        zs.next_in = (Bytef*)data.data();
        zs.avail_in = data.size();

        int ret;
        std::unique_ptr<char[]> outBuffer(new char[COMPRESSION_BUFFER_SIZE]);
        std::vector<uint8_t> outData;

        do {
            zs.next_out = reinterpret_cast<Bytef*>(outBuffer.get());
            zs.avail_out = COMPRESSION_BUFFER_SIZE;

            ret = inflate(&zs, 0);

            if (outData.size() < zs.total_out)
                outData.insert(outData.end(), outBuffer.get(), outBuffer.get() + zs.total_out - outData.size());

        } while (ret == Z_OK);

        inflateEnd(&zs);

        if (ret != Z_STREAM_END)
            throw std::runtime_error("NetworkClient::decompressData: Exception during zlib decompression.");

        return outData;
    }

    /*Asynchronously connects to a remote endpoint.
    @param endpoint The remote endpoint to connect to.*/
    void NetworkClient::Impl::connectAsync(asio::ip::tcp::endpoint endpoint) {
        auto self(shared_from_this());
        socket.connectAsync(endpoint, [this, self](std::error_code ec) {
            if (!ec) {
                Logger::Log("Connected to server!", LogLevel::info);
                
                receiveAsync();
                heartbeatCheckThread = std::thread(&NetworkClient::Impl::checkForMissedHeartbeats, self);
                sendHeartbeatsThread = std::thread(&NetworkClient::Impl::sendHeartbeatAsync, self);
            }
            else {
                Logger::Log("Error connecting to server: " + ec.message(), LogLevel::error);
                if (onConnectionLostHandler)
                    onConnectionLostHandler(getNetworkClientSharedPtr());
            }
        });
    }

    void NetworkClient::Impl::sendHeartbeatAsync()
    {
        while (!stopSendingHeartbeats)
        {
            try
            {
                HeartbeatPacket heartbeat((std::chrono::system_clock::now() > lastHeartbeatSent) ?
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastHeartbeatSent) :
                    std::chrono::duration_cast<std::chrono::milliseconds>(lastHeartbeatSent - std::chrono::system_clock::now()));
                lastHeartbeatSent = std::chrono::system_clock::now();
                auto heartbeatData = heartbeat.toByteArray();
                Packet pulse((uint8_t)ParloIDs::Heartbeat, *heartbeatData, false);
                sendAsync(pulse.buildPacket());
            }
            catch (const std::exception& e)
            {
                Logger::Log("Error sending heartbeat: " + std::string(e.what()), LogLevel::error);
            }

            std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval));
        }
    }

    void NetworkClient::Impl::checkForMissedHeartbeats()
    {
        while (!stopCheckMissedHeartbeats)
        {
            std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval));

            {
                std::lock_guard<std::mutex> lock(heartbeatsMutex);
                missedHeartbeats++;
            }

            if (missedHeartbeats > maxMissedHeartbeats)
            {
                {
                    std::lock_guard<std::mutex> lock(aliveMutex);
                    isAlive = false;
                }

                if (onConnectionLostHandler)
                    onConnectionLostHandler(getNetworkClientSharedPtr());
            }
        }
    }

    /*Asynchronously disconnects from a remote endpoint.
    @param sendDisconnectMessage Whether or not to send a disconnection message to the other party. Defaults to true.*/
    void NetworkClient::Impl::disconnectAsync(bool sendDisconnectMessage)
    {
        try
        {
            if (connected && socket.isOpen())
            {
                if (sendDisconnectMessage)
                {
                    GoodbyePacket byePacket((int)ParloDefaultTimeouts::Client);
                    std::vector<uint8_t> byeData = byePacket.toByteArray();
                    Packet goodbye((uint8_t)ParloIDs::CGoodbye, byeData, false);
                    sendAsync(goodbye.buildPacket());
                }

                if (socket.isOpen())
                {
                    //Shutdown both send and receive operations and close the socket
                    socket.shutdown();
                    socket.close();
                }

                if (!stopCheckMissedHeartbeats) {
                    stopCheckMissedHeartbeats = true;
                    if (heartbeatCheckThread.joinable())
                        heartbeatCheckThread.join();
                }

                if (!stopSendingHeartbeats) {
                    stopSendingHeartbeats = true;

                    if (sendHeartbeatsThread.joinable())
                        sendHeartbeatsThread.join();
                }

                connected = false;
            }
        }
        catch (const asio::system_error& e)
        {
            Logger::Log("Exception during NetworkClient::disconnectAsync(): " + std::string(e.what()), LogLevel::error);
        }
        catch (const std::exception& e)
        {
            Logger::Log("Exception during NetworkClient::disconnectAsync(): " + std::string(e.what()), LogLevel::error);
        }
    }

    Socket *NetworkClient::getSocket() {
        return pImpl->getSocket();
    }

    void NetworkClient::setOnClientDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        pImpl->setOnClientDisconnectedHandler(handler);
    }

    void NetworkClient::setOnConnectionLostHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        pImpl->setOnConnectionLostHandler(handler);
    }

    void NetworkClient::setOnServerDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        pImpl->setOnServerDisconnectedHandler(handler);
    }

    void NetworkClient::setOnReceivedHeartbeatHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        pImpl->setOnReceivedHeartbeatHandler(handler);
    }

    void NetworkClient::setOnReceivedDataHandler(std::function<void(const std::shared_ptr<NetworkClient>&, const std::shared_ptr<Packet>&)> handler) {
        pImpl->setOnReceivedDataHandler(handler);
    }

    void NetworkClient::setApplyCompression(bool apply) {
        pImpl->setApplyCompression(apply);
    }

    void NetworkClient::connectAsync(const asio::ip::tcp::endpoint endpoint) {
        pImpl->connectAsync(endpoint);
    }

    void NetworkClient::sendAsync(const std::vector<uint8_t>& data) {
        pImpl->sendAsync(data);
    }

    void NetworkClient::disconnectAsync(bool sendDisconnectMessage) {
        pImpl->disconnectAsync(sendDisconnectMessage);
    }
}