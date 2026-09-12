#pragma once

#include "types.hpp"
#include <vector>
#include <span>
#include <string_view>
#include <chrono>
#include <memory>
#include <ios>

namespace rfcomm {

// Default socket buffer size tuned for radio channel limit throughput (512 KB)
inline constexpr int DEFAULT_BUFFER_SIZE = 512 * 1024;

/**
 * @brief Connected RFCOMM stream socket wrapper with optimized throughput buffers.
 */
class Socket {
public:
    Socket();
    ~Socket();

    // Movable, non-copyable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    /**
     * @brief Send data chunk over RFCOMM stream.
     * @return Number of bytes actually sent, or -1 on error.
     */
    std::streamsize send(std::span<const uint8_t> data);

    /**
     * @brief Helper to send string view.
     */
    std::streamsize send(std::string_view str) {
        return send(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size()));
    }

    /**
     * @brief Keep sending until the entire buffer is transmitted or connection closes.
     * @return true if all bytes were sent, false on error.
     */
    bool send_all(std::span<const uint8_t> data);

    /**
     * @brief Receive data chunk from RFCOMM stream.
     * @return Number of bytes received (0 indicates peer disconnected, -1 on error).
     */
    std::streamsize recv(std::span<uint8_t> buffer);

    /**
     * @brief Keep receiving until the entire buffer is filled or connection closes.
     * @return true if buffer was completely filled, false if disconnected or error.
     */
    bool recv_all(std::span<uint8_t> buffer);

    /**
     * @brief Read available bytes up to max_bytes.
     */
    std::vector<uint8_t> recv_some(size_t max_bytes = 4096);

    /**
     * @brief Configure OS socket send and receive buffers (crucial for high RFCOMM speed).
     */
    bool set_buffer_sizes(int sndbuf_bytes = DEFAULT_BUFFER_SIZE, int rcvbuf_bytes = DEFAULT_BUFFER_SIZE);

    /**
     * @brief Set socket timeouts for send and receive operations.
     */
    bool set_timeouts(std::chrono::milliseconds send_timeout, std::chrono::milliseconds recv_timeout);

    /**
     * @brief Enable or disable blocking mode.
     */
    bool set_blocking(bool blocking);

    /**
     * @brief Check if socket is open and valid.
     */
    bool is_open() const noexcept;

    /**
     * @brief Close the socket connection.
     */
    void close();

    /**
     * @brief Remote peer Bluetooth address.
     */
    Address peer_address() const noexcept;

    /**
     * @brief Remote peer RFCOMM channel.
     */
    uint8_t peer_channel() const noexcept;

    /**
     * @brief Local RFCOMM channel.
     */
    uint8_t local_channel() const noexcept;

    /**
     * @brief Native socket descriptor (int on Linux, SOCKET on Windows).
     */
    uintptr_t native_handle() const noexcept;

    /**
     * @brief Internal constructor from native socket and peer info.
     */
    static std::unique_ptr<Socket> from_native(uintptr_t handle, Address peer_addr, uint8_t peer_ch);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rfcomm
