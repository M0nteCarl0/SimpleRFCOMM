#include "simple_rfcomm/server.hpp"
#include "platform.hpp"
#include <iostream>

namespace rfcomm {

struct Server::Impl {
    detail::native_socket_t listen_fd = detail::INVALID_SOCKET_HANDLE;
    uint8_t channel = 0;
    std::unique_ptr<sdp::Registration> sdp_reg;

    ~Impl() {
        close();
    }

    void close() {
        if (sdp_reg) {
            sdp_reg->unregister();
            sdp_reg.reset();
        }
        if (listen_fd != detail::INVALID_SOCKET_HANDLE) {
            detail::close_socket_handle(listen_fd);
            listen_fd = detail::INVALID_SOCKET_HANDLE;
        }
        channel = 0;
    }
};

Server::Server() : pimpl_(std::make_unique<Impl>()) {
    detail::WinsockInit::ensure();
}

Server::~Server() = default;

Server::Server(Server&& other) noexcept = default;
Server& Server::operator=(Server&& other) noexcept = default;

bool Server::listen(uint8_t channel, int backlog) {
    pimpl_->close();

#if defined(_WIN32) || defined(_WIN64)
    detail::native_socket_t s = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (s == detail::INVALID_SOCKET_HANDLE) {
        return false;
    }

    // Set buffer sizes
    int bufSize = DEFAULT_BUFFER_SIZE;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));

    SOCKADDR_BTH sa{};
    sa.addressFamily = AF_BTH;
    sa.port = (channel == 0) ? BT_PORT_ANY : channel;

    if (::bind(s, reinterpret_cast<SOCKADDR*>(&sa), sizeof(sa)) != 0) {
        detail::close_socket_handle(s);
        return false;
    }

    int saLen = sizeof(sa);
    if (::getsockname(s, reinterpret_cast<SOCKADDR*>(&sa), &saLen) != 0) {
        detail::close_socket_handle(s);
        return false;
    }

    if (::listen(s, backlog) != 0) {
        detail::close_socket_handle(s);
        return false;
    }

    pimpl_->listen_fd = s;
    pimpl_->channel = static_cast<uint8_t>(sa.port);
    return true;

#elif defined(__linux__)
    detail::native_socket_t s = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        return false;
    }

    int bufSize = DEFAULT_BUFFER_SIZE;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    struct sockaddr_rc loc_addr{};
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = bdaddr_t{}; // Any local adapter

    uint8_t bound_channel = 0;
    if (channel != 0) {
        loc_addr.rc_channel = channel;
        if (::bind(s, reinterpret_cast<struct sockaddr*>(&loc_addr), sizeof(loc_addr)) == 0) {
            bound_channel = channel;
        }
    } else {
        // Auto-assign: scan available RFCOMM channels from 1 to 30
        for (uint8_t ch = 1; ch <= 30; ++ch) {
            loc_addr.rc_channel = ch;
            if (::bind(s, reinterpret_cast<struct sockaddr*>(&loc_addr), sizeof(loc_addr)) == 0) {
                bound_channel = ch;
                break;
            }
        }
    }

    if (bound_channel == 0) {
        detail::close_socket_handle(s);
        return false;
    }

    if (::listen(s, backlog) != 0) {
        detail::close_socket_handle(s);
        return false;
    }

    pimpl_->listen_fd = s;
    pimpl_->channel = bound_channel;
    return true;
#endif
}

bool Server::listen_with_sdp(
    const Uuid& uuid,
    const std::string& service_name,
    uint8_t channel,
    int backlog
) {
    if (!listen(channel, backlog)) {
        return false;
    }

    pimpl_->sdp_reg = sdp::Registration::create(pimpl_->channel, uuid, service_name);
    if (!pimpl_->sdp_reg) {
        pimpl_->close();
        return false;
    }

    return true;
}

std::unique_ptr<Socket> Server::accept() {
    if (!is_listening()) return nullptr;

#if defined(_WIN32) || defined(_WIN64)
    SOCKADDR_BTH rem{};
    int remLen = sizeof(rem);
    detail::native_socket_t client_s = ::accept(pimpl_->listen_fd, reinterpret_cast<SOCKADDR*>(&rem), &remLen);
    if (client_s == detail::INVALID_SOCKET_HANDLE) {
        return nullptr;
    }

    Address peer_addr = detail::bth_to_address(rem.btAddr);
    uint8_t peer_ch = static_cast<uint8_t>(rem.port);
    return Socket::from_native(static_cast<uintptr_t>(client_s), peer_addr, peer_ch);

#elif defined(__linux__)
    struct sockaddr_rc rem{};
    socklen_t remLen = sizeof(rem);
    detail::native_socket_t client_s = ::accept(pimpl_->listen_fd, reinterpret_cast<struct sockaddr*>(&rem), &remLen);
    if (client_s < 0) {
        return nullptr;
    }

    Address peer_addr = detail::bdaddr_to_address(rem.rc_bdaddr);
    uint8_t peer_ch = rem.rc_channel;
    return Socket::from_native(static_cast<uintptr_t>(client_s), peer_addr, peer_ch);
#endif
}

void Server::close() {
    if (pimpl_) {
        pimpl_->close();
    }
}

bool Server::is_listening() const noexcept {
    return pimpl_ && pimpl_->listen_fd != detail::INVALID_SOCKET_HANDLE;
}

uint8_t Server::channel() const noexcept {
    return pimpl_ ? pimpl_->channel : 0;
}

} // namespace rfcomm
