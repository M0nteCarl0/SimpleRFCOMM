#include "simple_rfcomm/socket.hpp"
#include "platform.hpp"

namespace rfcomm {

struct Socket::Impl {
    detail::native_socket_t handle = detail::INVALID_SOCKET_HANDLE;
    Address peer_addr{};
    uint8_t peer_ch = 0;
    uint8_t local_ch = 0;

    ~Impl() {
        if (handle != detail::INVALID_SOCKET_HANDLE) {
            detail::close_socket_handle(handle);
            handle = detail::INVALID_SOCKET_HANDLE;
        }
    }
};

Socket::Socket() : pimpl_(std::make_unique<Impl>()) {
    detail::WinsockInit::ensure();
}

Socket::~Socket() = default;

Socket::Socket(Socket&& other) noexcept = default;
Socket& Socket::operator=(Socket&& other) noexcept = default;

std::unique_ptr<Socket> Socket::from_native(uintptr_t handle, Address peer_addr, uint8_t peer_ch) {
    auto sock = std::make_unique<Socket>();
    sock->pimpl_->handle = static_cast<detail::native_socket_t>(handle);
    sock->pimpl_->peer_addr = peer_addr;
    sock->pimpl_->peer_ch = peer_ch;

    // Apply high throughput default buffer sizes
    sock->set_buffer_sizes(DEFAULT_BUFFER_SIZE, DEFAULT_BUFFER_SIZE);

    // Query local port/channel if possible
#if defined(_WIN32) || defined(_WIN64)
    SOCKADDR_BTH sa{};
    int saLen = sizeof(sa);
    if (getsockname(sock->pimpl_->handle, (SOCKADDR*)&sa, &saLen) == 0) {
        sock->pimpl_->local_ch = static_cast<uint8_t>(sa.port);
    }
#elif defined(__linux__)
    struct sockaddr_rc sa{};
    socklen_t saLen = sizeof(sa);
    if (getsockname(sock->pimpl_->handle, (struct sockaddr*)&sa, &saLen) == 0) {
        sock->pimpl_->local_ch = sa.rc_channel;
    }
#endif

    return sock;
}

std::streamsize Socket::send(std::span<const uint8_t> data) {
    if (!is_open() || data.empty()) return 0;

#if defined(_WIN32) || defined(_WIN64)
    int sent = ::send(pimpl_->handle, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
    return sent >= 0 ? static_cast<std::streamsize>(sent) : -1;
#elif defined(__linux__)
    ssize_t sent = ::send(pimpl_->handle, data.data(), data.size(), MSG_NOSIGNAL);
    return sent >= 0 ? static_cast<std::streamsize>(sent) : -1;
#endif
}

bool Socket::send_all(std::span<const uint8_t> data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        auto chunk = data.subspan(total_sent);
        auto sent = send(chunk);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

std::streamsize Socket::recv(std::span<uint8_t> buffer) {
    if (!is_open() || buffer.empty()) return 0;

#if defined(_WIN32) || defined(_WIN64)
    int r = ::recv(pimpl_->handle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    return r >= 0 ? static_cast<std::streamsize>(r) : -1;
#elif defined(__linux__)
    ssize_t r = ::recv(pimpl_->handle, buffer.data(), buffer.size(), 0);
    return r >= 0 ? static_cast<std::streamsize>(r) : -1;
#endif
}

bool Socket::recv_all(std::span<uint8_t> buffer) {
    size_t total_recvd = 0;
    while (total_recvd < buffer.size()) {
        auto chunk = buffer.subspan(total_recvd);
        auto r = recv(chunk);
        if (r <= 0) {
            return false;
        }
        total_recvd += static_cast<size_t>(r);
    }
    return true;
}

std::vector<uint8_t> Socket::recv_some(size_t max_bytes) {
    std::vector<uint8_t> buf(max_bytes);
    auto r = recv(buf);
    if (r > 0) {
        buf.resize(static_cast<size_t>(r));
        return buf;
    }
    return {};
}

bool Socket::set_buffer_sizes(int sndbuf_bytes, int rcvbuf_bytes) {
    if (!is_open()) return false;
    bool ok = true;
    if (sndbuf_bytes > 0) {
#if defined(_WIN32) || defined(_WIN64)
        if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf_bytes), sizeof(sndbuf_bytes)) != 0) ok = false;
#elif defined(__linux__)
        if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_SNDBUF, &sndbuf_bytes, sizeof(sndbuf_bytes)) != 0) ok = false;
#endif
    }
    if (rcvbuf_bytes > 0) {
#if defined(_WIN32) || defined(_WIN64)
        if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf_bytes), sizeof(rcvbuf_bytes)) != 0) ok = false;
#elif defined(__linux__)
        if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_RCVBUF, &rcvbuf_bytes, sizeof(rcvbuf_bytes)) != 0) ok = false;
#endif
    }
    return ok;
}

bool Socket::set_timeouts(std::chrono::milliseconds send_timeout, std::chrono::milliseconds recv_timeout) {
    if (!is_open()) return false;
    bool ok = true;
#if defined(_WIN32) || defined(_WIN64)
    DWORD st = static_cast<DWORD>(send_timeout.count());
    DWORD rt = static_cast<DWORD>(recv_timeout.count());
    if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&st), sizeof(st)) != 0) ok = false;
    if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rt), sizeof(rt)) != 0) ok = false;
#elif defined(__linux__)
    struct timeval st;
    st.tv_sec = send_timeout.count() / 1000;
    st.tv_usec = (send_timeout.count() % 1000) * 1000;
    struct timeval rt;
    rt.tv_sec = recv_timeout.count() / 1000;
    rt.tv_usec = (recv_timeout.count() % 1000) * 1000;
    if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof(st)) != 0) ok = false;
    if (setsockopt(pimpl_->handle, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof(rt)) != 0) ok = false;
#endif
    return ok;
}

bool Socket::set_blocking(bool blocking) {
    if (!is_open()) return false;
#if defined(_WIN32) || defined(_WIN64)
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(pimpl_->handle, FIONBIO, &mode) == 0;
#elif defined(__linux__)
    int flags = fcntl(pimpl_->handle, F_GETFL, 0);
    if (flags < 0) return false;
    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(pimpl_->handle, F_SETFL, flags) == 0;
#endif
}

bool Socket::is_open() const noexcept {
    return pimpl_ && pimpl_->handle != detail::INVALID_SOCKET_HANDLE;
}

void Socket::close() {
    if (pimpl_ && pimpl_->handle != detail::INVALID_SOCKET_HANDLE) {
        detail::close_socket_handle(pimpl_->handle);
        pimpl_->handle = detail::INVALID_SOCKET_HANDLE;
    }
}

Address Socket::peer_address() const noexcept {
    return pimpl_ ? pimpl_->peer_addr : Address{};
}

uint8_t Socket::peer_channel() const noexcept {
    return pimpl_ ? pimpl_->peer_ch : 0;
}

uint8_t Socket::local_channel() const noexcept {
    return pimpl_ ? pimpl_->local_ch : 0;
}

uintptr_t Socket::native_handle() const noexcept {
    return pimpl_ ? static_cast<uintptr_t>(pimpl_->handle) : 0;
}

} // namespace rfcomm
