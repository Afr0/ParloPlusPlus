#include "pch.h"
#include "Socket.h"


namespace Parlo
{
    /*Asynchronously accepts new connection(s).
    @param acceptor An asio::ip::tcp::acceptor instance for accepting connections.
    @param callback A callback function to be called when a new socket was accepted.*/
    void Socket::acceptAsync(asio::ip::tcp::acceptor& acceptor,
        std::function<void(std::error_code, Socket&)> callback) {
        acceptor.async_accept(socket, [this, callback](std::error_code ec) {
            callback(ec, *this);
        });
    }

    /*Sets a lingering option on this Socket instance.
    @param enable Whether or not to enable it.
    @param timeout The linger's timeout.*/
    void Socket::setLinger(bool enable, std::chrono::seconds timeout)
    {
        asio::socket_base::linger option(enable, static_cast<unsigned int>(timeout.count()));
        std::error_code ec;

        socket.set_option(option, ec);

        if (ec)
            throw std::runtime_error("Failed to set linger option: " + ec.message());
    }
    
    /*Asynchronously connects to a remote endpoint.
    @param endpoint The remote endpoint to connect to.
    @param callback A callback function to be called when the connection attempt completes.*/
    void Socket::connectAsync(asio::ip::tcp::endpoint& endpoint, std::function<void(std::error_code)> callback) {
        socket.async_connect(endpoint, [callback](std::error_code ec) {
            callback(ec);
        });
    }

    /*Shuts down both send and receive operations on this Socket instance.*/
    void Socket::shutdown() {
        socket.shutdown(asio::ip::tcp::socket::shutdown_both);
    }

    /*Shuts down send operations on this Socket instance.*/
    void Socket::shutdownSend() {
        socket.shutdown(asio::ip::tcp::socket::shutdown_send);
    }

    /*Shuts down receive operations on this Socket instance.*/
    void Socket::shutdownReceive() {
        socket.shutdown(asio::ip::tcp::socket::shutdown_receive);
    }

    /*Closes this Socket instance.*/
    void Socket::close() {
        socket.close();
    }

    /*Returns the native handle for this Socket instance.*/
    asio::basic_socket<asio::ip::tcp, asio::any_io_executor>::native_handle_type Socket::nativeHandle() {
        return socket.native_handle();
    }

    bool Socket::isOpen() {
        return socket.is_open();
    }
}