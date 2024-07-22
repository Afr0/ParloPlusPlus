/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________
*/

#include "pch.h"
#include "NetworkClient.h"
#include "Listener.h" //Avoid circular dependency by including it in cpp instead of header.

namespace Parlo
{
    NetworkClient::NetworkClient(Socket& socket, Listener* listener, size_t maxPacketSize)
        : socket(socket), listener(listener), maxPacketSize(maxPacketSize), processingBuffer()
    {
        processingBuffer.setOnPacketProcessedHandler([this](const Packet& packet) {
            if (packet.getID() == ParloIDs::SGoodbye) { //Server notified client of disconnection.
                if (onServerDisconnectedHandler)
                    onServerDisconnectedHandler(shared_from_this());
                return;
            }
            if (packet.getID() == ParloIDs::CGoodbye) { //Client notified server of disconnection.
                if(onClientDisconnectedHandler)
                    onClientDisconnectedHandler(shared_from_this());
                return;
            }
            if (packet.getID() == ParloIDs::Heartbeat) {
                std::lock_guard<std::mutex> aliveLock(aliveMutex);
                isAlive = true;

                //The std::lock_guard is a RAII (Resource Acquisition Is Initialization) type which 
                //means it acquires the lock when it is created and releases it when it goes out of scope.
                std::lock_guard<std::mutex> heartbeatsLock(heartbeatsMutex);
                missedHeartbeats = 0;

                auto data = std::make_shared<std::vector<uint8_t>>(packet.getData());
                HeartbeatPacket Heartbeat = HeartbeatPacket::byteArrayToObject(data);

                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                std::chrono::system_clock::time_point sentTimestamp = Heartbeat.getSentTimestamp();
                std::chrono::milliseconds timeSinceLast = Heartbeat.getTimeSinceLast();

                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTimestamp);
                lastRTT = static_cast<int>(duration.count() + timeSinceLast.count());

                if (onReceivedHeartbeatHandler)
                    onReceivedHeartbeatHandler(shared_from_this());

                return;
            }

            if (packet.getIsCompressed()) {
                auto decompressedData = decompressData(packet.getData());
                if (onReceivedDataHandler)
                    onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), decompressedData, false));
            }
            else {
                if (onReceivedDataHandler)
                    onReceivedDataHandler(shared_from_this(), std::make_shared<Packet>(packet.getID(), packet.getData(), false));
            }
        });

        receiveAsync();
    }

    Socket* NetworkClient::getSocket() {
        return &socket;
    }

    /*Sets a handler for the event fired when a client disconnects from a server. This handler should be set by a Listener instance.
    @param handler The handler for the event.*/
    void NetworkClient::setOnClientDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onClientDisconnectedHandler = handler;
    }

    /*Sets a handler for the event fired when this NetworkClient instance lost its connection.
    @param handler The handler for the event.*/
    void NetworkClient::setOnConnectionLostHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onConnectionLostHandler = handler;
    }

    /*Sets a handler for the event fired when a client disconnects from a server.
    @param handler The handler for the event.*/
    void NetworkClient::setOnServerDisconnectedHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onServerDisconnectedHandler = handler;
    }

    /*Sets a handler for the event fired when a heartbeat was received.
    @param handler The handler for the event.*/
    void NetworkClient::setOnReceivedHeartbeatHandler(std::function<void(const std::shared_ptr<NetworkClient>&)> handler) {
        onReceivedHeartbeatHandler = handler;
    }

    /*Sets a handler for the event fired when data was received.
    @param handler The handler for the event.*/
    void NetworkClient::setOnReceivedDataHandler(std::function<void(const std::shared_ptr<NetworkClient>&, 
        const std::shared_ptr<Packet>&)> handler) {
        onReceivedDataHandler = handler;
    }

    /*Should compression be applied to network traffic? Defaults to false.*/
    void NetworkClient::setApplyCompression(bool apply) {
        applyCompression = apply;
    }

    /*Asynchronously receives data from this NetworkClient's connected endpoint.*/
    void NetworkClient::receiveAsync() {
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
                        onConnectionLostHandler(self);
                }
            }
        );
    }

    /*Sends data asynchronously.
    @param data The data to send.*/
    void NetworkClient::sendAsync(const std::vector<uint8_t>& data) {
        if (data.empty())
            throw std::invalid_argument("Data cannot be null or empty");
        if (data.size() > MAX_PACKET_SIZE)
            throw std::overflow_error("Data size exceeds maximum packet size");

        if (!connected)
            throw std::runtime_error("Socket is not connected");

        std::vector<uint8_t> finalData;

        if (shouldCompressData(data, 100 /*TODO: Calculate RTT*/)) {
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
                        onConnectionLostHandler(self);
                }
            });
    }

    /*Should data be compressed based on the RTT (Round Trip Time)?
    @param data The data to consider.
    @param rtt The round trip time.*/
    bool NetworkClient::shouldCompressData(const std::vector<uint8_t>& data, int rtt) {
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
    std::vector<uint8_t> NetworkClient::compressData(const std::vector<uint8_t>& data) {
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
    std::vector<uint8_t> NetworkClient::decompressData(const std::vector<uint8_t>& data) {
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
    void NetworkClient::connectAsync(asio::ip::tcp::endpoint endpoint) {
        auto self(shared_from_this());
        socket.connectAsync(endpoint, [this, self](std::error_code ec) {
            if (!ec) {
                Logger::Log("Connected to server!", LogLevel::info);
                receiveAsync();
            }
            else {
                Logger::Log("Error connecting to server: " + ec.message(), LogLevel::error);
                if (onConnectionLostHandler)
                    onConnectionLostHandler(self);
            }
            });
    }
}