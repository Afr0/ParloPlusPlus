#pragma once

#include <asio.hpp>
#include <functional>
#include <utility>

namespace Parlo
{
    class Socket {
    public:
        Socket(asio::io_context& io_context) : socket(io_context) {}
        Socket(asio::any_io_executor executor) : socket(executor) {}
        Socket(Socket&& other) noexcept : socket(std::move(other.socket)) {}
        ~Socket() {};
        Socket& operator=(Socket&& other) noexcept {
            if (this != &other)
                socket = std::move(other.socket);

            return *this;
        }

        //Delete copy constructor and copy assignment operator
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        void acceptAsync(asio::ip::tcp::acceptor& acceptor, std::function<void(std::error_code, Socket&)> callback);
        void setLinger(bool enable, std::chrono::seconds timeout);

        /*Returns the native ASIO socket for use with ASIO operations. */
        asio::ip::tcp::socket& native_handle() { return socket; }

        /*Asynchronously connects to a remote endpoint.
        @param endpoint The remote endpoint to connect to.
        @param callback A callback function to be called when the connection attempt completes.*/
        void connectAsync(asio::ip::tcp::endpoint &endpoint, std::function<void(std::error_code)> callback);

        /*Shuts down both send and receive operations on this Socket instance.*/
        void shutdown();

        /*Shuts down send operations on this Socket instance.*/
        void shutdownSend();

        /*Shuts down receive operations on this Socket instance.*/
        void shutdownReceive();

        /*Closes this Socket instance.*/
        void close();

        /*Is this socket still open?*/
        bool isOpen();

        asio::basic_socket<asio::ip::tcp, asio::any_io_executor>::native_handle_type nativeHandle();

    private:
        asio::ip::tcp::socket socket;
    };

}