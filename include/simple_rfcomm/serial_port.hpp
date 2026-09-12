#pragma once

#include "types.hpp"
#include "socket.hpp"
#include "client.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <chrono>
#include <optional>
#include <memory>

namespace rfcomm {

using namespace std::chrono_literals;

/**
 * @brief Purge flags matching FTDI / Win32 Comm API.
 */
enum class PurgeFlags : uint32_t {
    Rx  = 1 << 0,  // Purge incoming receive buffer
    Tx  = 1 << 1,  // Purge outgoing transmit buffer
    All = Rx | Tx  // Purge both buffers
};

inline constexpr PurgeFlags operator|(PurgeFlags a, PurgeFlags b) noexcept {
    return static_cast<PurgeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(PurgeFlags a, PurgeFlags b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief Virtual baud rates for compatibility with serial protocols (OBD-II, GPS, Arduino, etc.).
 * Note: Bluetooth RFCOMM physical transfer runs at the maximum speed the radio channel allows.
 */
enum class BaudRate : uint32_t {
    Baud9600   = 9600,
    Baud19200  = 19200,
    Baud38400  = 38400,
    Baud57600  = 57600,
    Baud115200 = 115200,
    Baud230400 = 230400,
    Baud460800 = 460800,
    Baud921600 = 921600,
    Maximum    = 0 // Radio limit
};

enum class DataBits : uint8_t { Bits7 = 7, Bits8 = 8 };
enum class StopBits : uint8_t { One = 1, Two = 2 };
enum class Parity : uint8_t { None = 0, Odd = 1, Even = 2, Mark = 3, Space = 4 };
enum class FlowControl : uint8_t { None = 0, RtsCts = 1, DtrDsr = 2, XonXoff = 3 };

/**
 * @brief FTDI style Serial com port wrapper for Bluetooth RFCOMM.
 * 
 * Provides an intuitive, serial-port-like interface (read, write, purge, get_rx_queue_status,
 * set_timeouts, read_line) while handling RFCOMM-specific nuances such as:
 * - Transparent SDP discovery (connect by SPP or custom UUID without knowing the channel).
 * - High-speed internal read buffer caching to avoid thousands of 1-byte Bluetooth syscalls.
 * - Millisecond-precision timeouts matching FTDI D2XX (FT_SetTimeouts).
 * - Purging of internal and OS socket queues.
 */
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // Movable, non-copyable
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    /**
     * @brief Open connection directly to target address and channel.
     */
    bool open(const Address& addr, uint8_t channel, std::chrono::milliseconds timeout = 5000ms);
    bool open(std::string_view mac_address, uint8_t channel, std::chrono::milliseconds timeout = 5000ms);

    /**
     * @brief Open connection via Bluetooth SDP service discovery (defaults to SPP 0x1101).
     */
    bool open_by_uuid(const Address& addr, const Uuid& uuid = Uuid::serial_port(), std::chrono::milliseconds timeout = 5000ms);
    bool open_by_uuid(std::string_view mac_address, std::string_view uuid_str, std::chrono::milliseconds timeout = 5000ms);

    /**
     * @brief Attach an existing connected RFCOMM socket (e.g. from Server::accept).
     */
    bool attach(std::unique_ptr<Socket> sock);

    /**
     * @brief Close the connection and flush buffers.
     */
    void close();

    /**
     * @brief Check if port is open and connected.
     */
    bool is_open() const noexcept;

    // --- Configuration ---

    void set_baud_rate(BaudRate baud) noexcept;
    BaudRate baud_rate() const noexcept;

    void set_data_characteristics(DataBits data_bits, StopBits stop_bits, Parity parity) noexcept;
    void set_flow_control(FlowControl flow_control) noexcept;

    /**
     * @brief Set read and write timeouts matching FTDI FT_SetTimeouts.
     */
    void set_timeouts(std::chrono::milliseconds read_timeout, std::chrono::milliseconds write_timeout);

    // --- Read Operations ---

    /**
     * @brief Read up to buffer.size() bytes. Returns actual bytes read (0 on timeout).
     */
    size_t read(std::span<uint8_t> buffer);
    size_t read(void* buffer, size_t size);

    /**
     * @brief Block and read exactly buffer.size() bytes until filled or timeout/EOF.
     */
    bool read_exact(std::span<uint8_t> buffer);

    /**
     * @brief Read a single byte. Returns std::nullopt on timeout or disconnect.
     */
    std::optional<uint8_t> read_byte();

    /**
     * @brief Read a line of text up to delimiter (default '\n') or timeout.
     */
    std::string read_line(char delimiter = '\n', std::chrono::milliseconds timeout = 5000ms);

    // --- Write Operations ---

    /**
     * @brief Write buffer to the Bluetooth serial stream. Returns actual bytes written.
     */
    size_t write(std::span<const uint8_t> data);
    size_t write(const void* data, size_t size);
    size_t write(std::string_view text);

    /**
     * @brief Write a single byte.
     */
    bool write_byte(uint8_t byte);

    /**
     * @brief Write string followed by end-of-line delimiter (default "\r\n").
     */
    bool write_line(std::string_view line, std::string_view eol = "\r\n");

    /**
     * @brief Flush any pending write data.
     */
    void flush();

    // --- Queue Status & Purge (FTDI Style) ---

    /**
     * @brief Get number of bytes currently available in the receive queue (FT_GetQueueStatus).
     */
    size_t get_rx_queue_status();

    /**
     * @brief Alias for get_rx_queue_status().
     */
    size_t bytes_available() { return get_rx_queue_status(); }

    /**
     * @brief Purge receive and/or transmit buffers matching FT_Purge.
     */
    void purge(PurgeFlags flags = PurgeFlags::All);

    // --- Modem / Handshake Control Lines ---

    void set_dtr(bool state) noexcept;
    void set_rts(bool state) noexcept;
    bool get_cts() const noexcept;
    bool get_dsr() const noexcept;

    // --- Bluetooth Information ---

    Address peer_address() const noexcept;
    uint8_t peer_channel() const noexcept;
    Socket* underlying_socket() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rfcomm
