#include "simple_rfcomm/client.hpp"
#include "platform.hpp"

namespace rfcomm {

std::unique_ptr<Socket> Client::connect(
    const Address& addr,
    uint8_t channel,
    std::chrono::milliseconds timeout
) {
    detail::WinsockInit::ensure();

#if defined(_WIN32) || defined(_WIN64)
    detail::native_socket_t s = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (s == detail::INVALID_SOCKET_HANDLE) {
        return nullptr;
    }

    // Set buffer sizes for maximum throughput
    int bufSize = DEFAULT_BUFFER_SIZE;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));

    if (timeout.count() > 0) {
        DWORD ms = static_cast<DWORD>(timeout.count());
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    }

    SOCKADDR_BTH sa{};
    sa.addressFamily = AF_BTH;
    sa.btAddr = detail::address_to_bth(addr);
    sa.port = channel;

    if (::connect(s, reinterpret_cast<SOCKADDR*>(&sa), sizeof(sa)) != 0) {
        detail::close_socket_handle(s);
        return nullptr;
    }

    return Socket::from_native(static_cast<uintptr_t>(s), addr, channel);

#elif defined(__linux__)
    detail::native_socket_t s = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        return nullptr;
    }

    int bufSize = DEFAULT_BUFFER_SIZE;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    if (timeout.count() > 0) {
        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    struct sockaddr_rc addr_rc{};
    addr_rc.rc_family = AF_BLUETOOTH;
    addr_rc.rc_bdaddr = detail::address_to_bdaddr(addr);
    addr_rc.rc_channel = channel;

    if (::connect(s, reinterpret_cast<struct sockaddr*>(&addr_rc), sizeof(addr_rc)) != 0) {
        detail::close_socket_handle(s);
        return nullptr;
    }

    return Socket::from_native(static_cast<uintptr_t>(s), addr, channel);
#endif
}

std::unique_ptr<Socket> Client::connect_by_sdp(
    const Address& addr,
    const Uuid& uuid,
    std::chrono::milliseconds timeout
) {
    auto channel = sdp::resolve_channel(addr, uuid);
    if (!channel) {
        return nullptr;
    }
    return connect(addr, *channel, timeout);
}

} // namespace rfcomm
