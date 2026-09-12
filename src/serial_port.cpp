#include "simple_rfcomm/serial_port.hpp"
#include "platform.hpp"
#include <deque>
#include <algorithm>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#  include <winsock2.h>
#else
#  include <sys/ioctl.h>
#endif

namespace rfcomm {

struct SerialPort::Impl {
    std::unique_ptr<Socket> sock;
    BaudRate baud = BaudRate::Baud115200;
    DataBits data_bits = DataBits::Bits8;
    StopBits stop_bits = StopBits::One;
    Parity parity = Parity::None;
    FlowControl flow_control = FlowControl::None;

    std::chrono::milliseconds read_timeout{5000};
    std::chrono::milliseconds write_timeout{5000};

    bool dtr = true;
    bool rts = true;
    bool cts = true;
    bool dsr = true;

    // Internal receive buffer for fast byte-by-byte and line parsing
    std::vector<uint8_t> rx_buf;
    size_t rx_head = 0;

    void clear_rx_cache() {
        rx_buf.clear();
        rx_head = 0;
    }

    size_t cached_bytes_available() const noexcept {
        return rx_buf.size() - rx_head;
    }

    // Refill the internal RX cache from the socket
    bool refill_rx_cache() {
        if (!sock || !sock->is_open()) return false;

        // Reset buffer if exhausted
        if (rx_head >= rx_buf.size()) {
            clear_rx_cache();
        }

        // Allocate temporary buffer for chunk reading
        std::vector<uint8_t> chunk(4096);
        auto r = sock->recv(chunk);
        if (r <= 0) {
            return false;
        }

        if (rx_head > 0) {
            // Compact remaining data
            rx_buf.erase(rx_buf.begin(), rx_buf.begin() + static_cast<ptrdiff_t>(rx_head));
            rx_head = 0;
        }

        rx_buf.insert(rx_buf.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(r));
        return true;
    }
};

SerialPort::SerialPort() : pimpl_(std::make_unique<Impl>()) {}

SerialPort::~SerialPort() = default;

SerialPort::SerialPort(SerialPort&& other) noexcept = default;
SerialPort& SerialPort::operator=(SerialPort&& other) noexcept = default;

bool SerialPort::open(const Address& addr, uint8_t channel, std::chrono::milliseconds timeout) {
    close();
    Client client;
    pimpl_->sock = client.connect(addr, channel, timeout);
    if (!pimpl_->sock) {
        return false;
    }
    pimpl_->sock->set_timeouts(pimpl_->write_timeout, pimpl_->read_timeout);
    return true;
}

bool SerialPort::open(std::string_view mac_address, uint8_t channel, std::chrono::milliseconds timeout) {
    auto addr = Address::try_parse(mac_address);
    if (!addr) return false;
    return open(*addr, channel, timeout);
}

bool SerialPort::open_by_uuid(const Address& addr, const Uuid& uuid, std::chrono::milliseconds timeout) {
    close();
    Client client;
    pimpl_->sock = client.connect_by_sdp(addr, uuid, timeout);
    if (!pimpl_->sock) {
        return false;
    }
    pimpl_->sock->set_timeouts(pimpl_->write_timeout, pimpl_->read_timeout);
    return true;
}

bool SerialPort::open_by_uuid(std::string_view mac_address, std::string_view uuid_str, std::chrono::milliseconds timeout) {
    auto addr = Address::try_parse(mac_address);
    if (!addr) return false;
    auto uuid = Uuid::try_parse(uuid_str);
    if (!uuid) return false;
    return open_by_uuid(*addr, *uuid, timeout);
}

bool SerialPort::attach(std::unique_ptr<Socket> sock) {
    close();
    if (!sock || !sock->is_open()) {
        return false;
    }
    pimpl_->sock = std::move(sock);
    pimpl_->sock->set_timeouts(pimpl_->write_timeout, pimpl_->read_timeout);
    return true;
}

void SerialPort::close() {
    if (pimpl_) {
        pimpl_->clear_rx_cache();
        if (pimpl_->sock) {
            pimpl_->sock->close();
            pimpl_->sock.reset();
        }
    }
}

bool SerialPort::is_open() const noexcept {
    return pimpl_ && pimpl_->sock && pimpl_->sock->is_open();
}

void SerialPort::set_baud_rate(BaudRate baud) noexcept {
    if (pimpl_) pimpl_->baud = baud;
}

BaudRate SerialPort::baud_rate() const noexcept {
    return pimpl_ ? pimpl_->baud : BaudRate::Baud115200;
}

void SerialPort::set_data_characteristics(DataBits data_bits, StopBits stop_bits, Parity parity) noexcept {
    if (pimpl_) {
        pimpl_->data_bits = data_bits;
        pimpl_->stop_bits = stop_bits;
        pimpl_->parity = parity;
    }
}

void SerialPort::set_flow_control(FlowControl flow_control) noexcept {
    if (pimpl_) pimpl_->flow_control = flow_control;
}

void SerialPort::set_timeouts(std::chrono::milliseconds read_timeout, std::chrono::milliseconds write_timeout) {
    if (!pimpl_) return;
    pimpl_->read_timeout = read_timeout;
    pimpl_->write_timeout = write_timeout;
    if (pimpl_->sock) {
        pimpl_->sock->set_timeouts(write_timeout, read_timeout);
    }
}

size_t SerialPort::read(std::span<uint8_t> buffer) {
    if (!is_open() || buffer.empty()) return 0;

    size_t copied = 0;

    // 1. First drain available bytes from internal cache
    if (pimpl_->cached_bytes_available() > 0) {
        size_t available = pimpl_->cached_bytes_available();
        size_t to_copy = std::min(available, buffer.size());
        std::copy_n(pimpl_->rx_buf.begin() + static_cast<ptrdiff_t>(pimpl_->rx_head), to_copy, buffer.begin());
        pimpl_->rx_head += to_copy;
        copied += to_copy;

        if (copied == buffer.size()) {
            return copied;
        }
    }

    // 2. If more bytes needed, read directly into user buffer
    auto remaining = buffer.subspan(copied);
    auto r = pimpl_->sock->recv(remaining);
    if (r > 0) {
        copied += static_cast<size_t>(r);
    }

    return copied;
}

size_t SerialPort::read(void* buffer, size_t size) {
    return read(std::span<uint8_t>(static_cast<uint8_t*>(buffer), size));
}

bool SerialPort::read_exact(std::span<uint8_t> buffer) {
    size_t total = 0;
    while (total < buffer.size()) {
        auto sub = buffer.subspan(total);
        size_t n = read(sub);
        if (n == 0) {
            return false; // Timeout or disconnected
        }
        total += n;
    }
    return true;
}

std::optional<uint8_t> SerialPort::read_byte() {
    if (!is_open()) return std::nullopt;

    if (pimpl_->cached_bytes_available() == 0) {
        if (!pimpl_->refill_rx_cache()) {
            return std::nullopt;
        }
    }

    if (pimpl_->cached_bytes_available() > 0) {
        uint8_t byte = pimpl_->rx_buf[pimpl_->rx_head++];
        return byte;
    }

    return std::nullopt;
}

std::string SerialPort::read_line(char delimiter, std::chrono::milliseconds timeout) {
    if (!is_open()) return {};

    std::string line;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        // Search in internal cache
        while (pimpl_->cached_bytes_available() > 0) {
            char c = static_cast<char>(pimpl_->rx_buf[pimpl_->rx_head++]);
            if (c == delimiter) {
                // Strip trailing \r if line ends with \r\n
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return line;
            }
            line.push_back(c);
        }

        // Check timeout
        if (timeout.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                break;
            }
        }

        // Cache exhausted, refill from socket
        if (!pimpl_->refill_rx_cache()) {
            break; // Disconnect or timeout
        }
    }

    return line;
}

size_t SerialPort::write(std::span<const uint8_t> data) {
    if (!is_open() || data.empty()) return 0;
    auto sent = pimpl_->sock->send(data);
    return sent > 0 ? static_cast<size_t>(sent) : 0;
}

size_t SerialPort::write(const void* data, size_t size) {
    return write(std::span<const uint8_t>(static_cast<const uint8_t*>(data), size));
}

size_t SerialPort::write(std::string_view text) {
    return write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

bool SerialPort::write_byte(uint8_t byte) {
    return write(std::span<const uint8_t>(&byte, 1)) == 1;
}

bool SerialPort::write_line(std::string_view line, std::string_view eol) {
    if (!is_open()) return false;
    std::string full_line = std::string(line) + std::string(eol);
    return pimpl_->sock->send_all(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(full_line.data()), full_line.size()
    ));
}

void SerialPort::flush() {
    // In stream sockets data is sent directly to the TCP/RFCOMM stack
}

size_t SerialPort::get_rx_queue_status() {
    if (!is_open()) return 0;

    size_t in_cache = pimpl_->cached_bytes_available();

    // Query OS socket pending bytes (FIONREAD)
    unsigned long socket_pending = 0;
#if defined(_WIN32) || defined(_WIN64)
    ioctlsocket(static_cast<SOCKET>(pimpl_->sock->native_handle()), FIONREAD, &socket_pending);
#elif defined(__linux__)
    ioctl(static_cast<int>(pimpl_->sock->native_handle()), FIONREAD, &socket_pending);
#endif

    return in_cache + socket_pending;
}

void SerialPort::purge(PurgeFlags flags) {
    if (!pimpl_) return;

    if (flags & PurgeFlags::Rx) {
        pimpl_->clear_rx_cache();
        // Drain any pending bytes in OS socket without blocking
        if (pimpl_->sock) {
            pimpl_->sock->set_blocking(false);
            std::vector<uint8_t> drain_buf(4096);
            while (pimpl_->sock->recv(drain_buf) > 0) {}
            pimpl_->sock->set_blocking(true);
        }
    }
}

void SerialPort::set_dtr(bool state) noexcept {
    if (pimpl_) pimpl_->dtr = state;
}

void SerialPort::set_rts(bool state) noexcept {
    if (pimpl_) pimpl_->rts = state;
}

bool SerialPort::get_cts() const noexcept {
    return pimpl_ ? pimpl_->cts : true;
}

bool SerialPort::get_dsr() const noexcept {
    return pimpl_ ? pimpl_->dsr : true;
}

Address SerialPort::peer_address() const noexcept {
    return pimpl_ && pimpl_->sock ? pimpl_->sock->peer_address() : Address{};
}

uint8_t SerialPort::peer_channel() const noexcept {
    return pimpl_ && pimpl_->sock ? pimpl_->sock->peer_channel() : 0;
}

Socket* SerialPort::underlying_socket() noexcept {
    return pimpl_ ? pimpl_->sock.get() : nullptr;
}

} // namespace rfcomm
