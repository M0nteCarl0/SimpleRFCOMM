# SimpleRFCOMM C++20 API Guide

This guide provides a comprehensive reference for all classes, methods, types, and programming recipes in **SimpleRFCOMM**.

---

## Table of Contents
1. [Include Header](#include-header)
2. [Address (`rfcomm::Address`)](#address-rfcommaddress)
3. [UUID (`rfcomm::Uuid`)](#uuid-rfcommuuid)
4. [Socket (`rfcomm::Socket`)](#socket-rfcommsocket)
5. [Server (`rfcomm::Server`)](#server-rfcommserver)
6. [Client (`rfcomm::Client`)](#client-rfcommclient)
7. [Service Discovery Protocol (`rfcomm::sdp`)](#service-discovery-protocol-rfcommsdp)
8. [FTDI style Serial com port (`rfcomm::SerialPort`)](#ftdi-style-serial-com-port-rfcommserialport)
9. [High Throughput Optimization Guidelines](#high-throughput-optimization-guidelines)
10. [Code Recipes](#code-recipes)

---

## Include Header

Include the primary umbrella header to access the complete API:

```cpp
#include <simple_rfcomm/simple_rfcomm.hpp>
```

All classes and functions are under the `rfcomm` namespace.

---

## Address (`rfcomm::Address`)

Represents a 48-bit Bluetooth Device Address (BD_ADDR / MAC address).

```cpp
struct Address {
    std::array<uint8_t, 6> bytes{0, 0, 0, 0, 0, 0};
    ...
};
```

### Methods

| Method | Description |
| :--- | :--- |
| `static Address parse(std::string_view str)` | Parses `"XX:XX:XX:XX:XX:XX"` or `"XX-XX-XX-XX-XX-XX"`. Throws `std::invalid_argument` on syntax error. |
| `static std::optional<Address> try_parse(std::string_view str) noexcept` | Non-throwing version. Returns `std::nullopt` on failure. |
| `static constexpr Address any() noexcept` | Returns wildcard address `00:00:00:00:00:00`. |
| `std::string to_string() const` | Formats address as uppercase string `"XX:XX:XX:XX:XX:XX"`. |
| `uint64_t to_uint64() const noexcept` | Returns address in lower 48 bits of `uint64_t` (Windows `BTH_ADDR` format). |
| `static Address from_uint64(uint64_t val) noexcept` | Creates `Address` from 64-bit integer. |
| `<=>` and `==` | Default three-way comparison operators for sorting and map keys. |

### Example

```cpp
auto addr = rfcomm::Address::parse("9C:C7:D3:F6:E6:A8");
std::cout << "Target: " << addr.to_string() << "\n";

// Map lookup
std::map<rfcomm::Address, std::string> device_names;
device_names[addr] = "Jetson Orin";
```

---

## UUID (`rfcomm::Uuid`)

Represents a 128-bit UUID used by Bluetooth SDP for service identification.

```cpp
struct Uuid {
    std::array<uint8_t, 16> bytes{0};
    ...
};
```

### Methods

| Method | Description |
| :--- | :--- |
| `static Uuid parse(std::string_view str)` | Parses full 128-bit UUID (e.g. `"e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12"`) or 4-digit hex short UUID (e.g. `"1101"`). Throws on error. |
| `static std::optional<Uuid> try_parse(std::string_view str) noexcept` | Non-throwing parser. |
| `static constexpr Uuid from_16bit(uint16_t short_uuid) noexcept` | Converts a 16-bit short UUID into Bluetooth Base UUID (`0000xxxx-0000-1000-8000-00805F9B34FB`). |
| `static constexpr Uuid serial_port() noexcept` | Returns standard Bluetooth Serial Port Profile (SPP) UUID (`0x1101`). |
| `std::string to_string() const` | Formats UUID as lowercase string `"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"`. |
| `<=>` and `==` | Comparison operators. |

### Example

```cpp
// Custom service UUID
auto custom_uuid = rfcomm::Uuid::parse("4a98fc1b-9f57-4244-b659-17f12e30dd0a");

// Standard Serial Port Profile
auto spp_uuid = rfcomm::Uuid::serial_port();
```

---

## Socket (`rfcomm::Socket`)

An RAII wrapper managing a connected RFCOMM stream socket. Move-only (cannot be copied).

### Methods

#### Sending Data
- `std::streamsize send(std::span<const uint8_t> data)`
  Sends a chunk of data. Returns the number of bytes actually sent, or `-1` on error.
- `std::streamsize send(std::string_view str)`
  Helper to send string text directly.
- `bool send_all(std::span<const uint8_t> data)`
  Repeatedly loops until the entire buffer is sent or an error occurs. Returns `true` on complete success, `false` on failure.

#### Receiving Data
- `std::streamsize recv(std::span<uint8_t> buffer)`
  Reads available data into the buffer. Returns the number of bytes read, `0` on peer disconnect, or `-1` on error.
- `bool recv_all(std::span<uint8_t> buffer)`
  Loops until the buffer is completely filled or the connection closes. Returns `true` if fully filled.
- `std::vector<uint8_t> recv_some(size_t max_bytes = 4096)`
  Reads up to `max_bytes` into a newly allocated vector.

#### Configuration & Control
- `bool set_buffer_sizes(int sndbuf_bytes = 524288, int rcvbuf_bytes = 524288)`
  Sets OS socket `SO_SNDBUF` and `SO_RCVBUF` sizes. Crucial for high throughput!
- `bool set_timeouts(std::chrono::milliseconds send_timeout, std::chrono::milliseconds recv_timeout)`
  Sets blocking send/recv timeouts.
- `bool set_blocking(bool blocking)`
  Enables or disables blocking mode.
- `void close()`
  Explicitly closes the socket.
- `bool is_open() const noexcept`
  Checks if the socket handle is valid and active.

#### Metadata
- `Address peer_address() const noexcept` — Remote device MAC address.
- `uint8_t peer_channel() const noexcept` — Remote RFCOMM channel (1..30).
- `uint8_t local_channel() const noexcept` — Local RFCOMM channel (1..30).
- `uintptr_t native_handle() const noexcept` — Native OS descriptor (`SOCKET` on Windows, `int` on Linux).

---

## Server (`rfcomm::Server`)

Listens for incoming RFCOMM connections and optionally registers service records in Bluetooth SDP.

### Methods

- `bool listen(uint8_t channel = 0, int backlog = 4)`
  Starts listening on a specific channel (1..30). Pass `0` to let the OS assign an available channel.
- `bool listen_with_sdp(const Uuid& uuid, const std::string& service_name = "RFCOMM Service", uint8_t channel = 0, int backlog = 4)`
  Starts listening on an available channel and publishes the service in Bluetooth SDP. Clients can find it by `uuid`.
- `std::unique_ptr<Socket> accept()`
  Blocks until a client connects. Returns a connected `Socket`, or `nullptr` on error or when server is stopped.
- `void close()`
  Stops listening and unregisters any active SDP records.
- `bool is_listening() const noexcept`
  Returns `true` if listening.
- `uint8_t channel() const noexcept`
  Returns the assigned RFCOMM channel number.

---

## Client (`rfcomm::Client`)

Connects to remote Bluetooth RFCOMM servers.

### Methods

- `std::unique_ptr<Socket> connect(const Address& addr, uint8_t channel, std::chrono::milliseconds timeout = 10000ms)`
  Connects directly to target device and RFCOMM channel.
- `std::unique_ptr<Socket> connect_by_sdp(const Address& addr, const Uuid& uuid, std::chrono::milliseconds timeout = 10000ms)`
  Performs SDP query on the target device to find the RFCOMM channel matching `uuid`, then establishes the connection.

---

## Service Discovery Protocol (`rfcomm::sdp`)

### Standalone Resolution
```cpp
std::optional<uint8_t> rfcomm::sdp::resolve_channel(const Address& target, const Uuid& uuid);
```
Queries the remote device's SDP database and returns the RFCOMM channel number if found.

### RAII Registration Guard (`rfcomm::sdp::Registration`)
```cpp
auto reg = rfcomm::sdp::Registration::create(channel, uuid, "Service Name", "Provider");
if (reg && reg->is_registered()) {
    // Record is published in SDP daemon
}
// Automatically unregistered when reg goes out of scope!
```

---

## FTDI style Serial com port (`rfcomm::SerialPort`)

The `rfcomm::SerialPort` class provides an interface modeled after the classic **FTDI D2XX / Windows Comm API / POSIX termios** serial port programming paradigm, specifically tailored to the characteristics of Bluetooth RFCOMM.

### Features
- **Transparent SDP Connection**: Connect by SPP (`0x1101`) or custom UUID without having to hardcode channels.
- **Socket Attachment**: Wrap any existing incoming connection from `Server::accept()` into a `SerialPort`.
- **Fast Internal Read Caching**: Drains RFCOMM frames into an internal cache to make `read_byte()` and `read_line()` up to **50x faster** by avoiding 1-byte system calls.
- **FTDI D2XX Compatibility**: `set_timeouts()` (matching `FT_SetTimeouts`), `get_rx_queue_status()` (matching `FT_GetQueueStatus`), and `purge()` (matching `FT_Purge`).
- **Modem Status Signals**: Track and control DTR, RTS, CTS, DSR.

### Methods

| Method | Description |
| :--- | :--- |
| `bool open(addr, channel, timeout)` | Opens direct connection to target address and channel. |
| `bool open_by_uuid(addr, uuid, timeout)` | Discovers channel via SDP and connects. Default UUID is Serial Port Profile (`0x1101`). |
| `bool attach(std::unique_ptr<Socket> sock)` | Wraps an existing connected socket (e.g. accepted from `Server`). |
| `void close()` | Closes port and flushes internal buffers. |
| `bool is_open() const noexcept` | Checks connection status. |
| `void set_baud_rate(BaudRate baud)` | Sets virtual baud rate (`Baud9600` .. `Baud921600` / `Maximum`). |
| `void set_data_characteristics(...)` | Configures 8-N-1 / data bits, stop bits, parity. |
| `void set_timeouts(read_ms, write_ms)` | Sets read and write timeouts matching `FT_SetTimeouts`. |
| `size_t read(buffer)` | Reads available data (returns actual bytes read). |
| `bool read_exact(buffer)` | Blocks until the buffer is completely filled or timeout expires. |
| `std::optional<uint8_t> read_byte()` | Reads next byte from queue. Returns `nullopt` on timeout. |
| `std::string read_line(delimiter, timeout)` | Reads characters until `delimiter` (default `'\n'`). Auto-strips trailing `'\r'`. |
| `size_t write(data)` | Writes bytes to Bluetooth serial stream. |
| `bool write_byte(uint8_t byte)` | Sends a single byte. |
| `bool write_line(line, eol)` | Sends string followed by `"\r\n"`. |
| `size_t get_rx_queue_status()` | Returns number of bytes waiting in RX buffer (matching `FT_GetQueueStatus`). |
| `void purge(PurgeFlags flags)` | Purges RX/TX buffers (matching `FT_Purge`). Flags: `PurgeFlags::Rx`, `PurgeFlags::Tx`, `PurgeFlags::All`. |
| `void set_dtr(bool)`, `set_rts(bool)` | Sets DTR and RTS line states. |
| `bool get_cts()`, `bool get_dsr()` | Reads CTS and DSR line states. |

### Example

```cpp
rfcomm::SerialPort serial;

// Connect to remote Bluetooth device by standard Serial Port Profile (SPP)
if (serial.open_by_uuid("9C:C7:D3:F6:E6:A8")) {
    serial.set_baud_rate(rfcomm::BaudRate::Baud115200);
    serial.set_timeouts(2000ms, 2000ms);

    // Write AT command line
    serial.write_line("AT+GMR");

    // Check if response arrived in RX queue
    std::this_thread::sleep_for(50ms);
    std::cout << "Bytes available: " << serial.get_rx_queue_status() << "\n";

    // Read response line
    std::string reply = serial.read_line();
    std::cout << "Device replied: " << reply << "\n";

    serial.close();
}
```

---

## High Throughput Optimization Guidelines

To achieve transfer speeds close to the physical radio limit (~200 KB/s / ~1.6 Mbps):

1. **Large Socket Buffers**:
   Standard OS socket buffers are often only 4–16 KB. When sending at 1.6 Mbps, this buffer drains in milliseconds, causing window stalls. `SimpleRFCOMM` automatically sets `SO_SNDBUF` and `SO_RCVBUF` to **512 KB** (`rfcomm::DEFAULT_BUFFER_SIZE`).
2. **Chunk Size**:
   Send data in chunks between **8 KB and 32 KB** (16 KB is recommended). Chunks larger than 4 KB allow the Bluetooth controller to fill 3-DH5 (1021 bytes) baseband frames back-to-back without incurring syscall overhead.
3. **Avoid Tiny Packets**:
   Sending 10–50 bytes individually incurs high Bluetooth packet header overhead (~70% overhead) and drastically degrades speed to ~5–10 KB/s. Use buffering or batching.
4. **Use `send_all` / `recv_all`**:
   Ensures that partial stream transfers are handled without data loss or buffer reallocations.

---

## Code Recipes

### Recipe 1: High-Speed Streaming Server

```cpp
#include <simple_rfcomm/simple_rfcomm.hpp>
#include <iostream>
#include <vector>

int main() {
    rfcomm::Server server;
    auto uuid = rfcomm::Uuid::parse("e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12");

    if (!server.listen_with_sdp(uuid, "Speed Stream Server")) {
        std::cerr << "Listen failed!\n";
        return 1;
    }

    std::cout << "Waiting on channel " << (int)server.channel() << "...\n";
    auto sock = server.accept();
    std::cout << "Connected: " << sock->peer_address().to_string() << "\n";

    std::vector<uint8_t> buffer(32 * 1024);
    size_t total_received = 0;

    while (true) {
        auto r = sock->recv(buffer);
        if (r <= 0) break; // Client disconnected
        total_received += r;
    }

    std::cout << "Received total: " << total_received << " bytes\n";
    return 0;
}
```

### Recipe 2: High-Speed Streaming Client

```cpp
#include <simple_rfcomm/simple_rfcomm.hpp>
#include <iostream>
#include <vector>

int main() {
    rfcomm::Client client;
    auto target_addr = rfcomm::Address::parse("9C:C7:D3:F6:E6:A8");
    auto uuid = rfcomm::Uuid::parse("e3b0c442-98fc-1c14-9afe-4c9c2e0b5f12");

    auto sock = client.connect_by_sdp(target_addr, uuid);
    if (!sock) {
        std::cerr << "Connect failed!\n";
        return 1;
    }

    std::vector<uint8_t> chunk(16 * 1024, 0xAB);
    size_t total_to_send = 5 * 1024 * 1024; // 5 MB
    size_t sent = 0;

    while (sent < total_to_send) {
        size_t current = std::min(chunk.size(), total_to_send - sent);
        if (!sock->send_all(std::span<const uint8_t>(chunk.data(), current))) {
            break;
        }
        sent += current;
    }

    std::cout << "Sent " << sent << " bytes successfully!\n";
    return 0;
}
```

### Recipe 3: Resilient Connection with Retry Loop

```cpp
#include <simple_rfcomm/simple_rfcomm.hpp>
#include <iostream>
#include <thread>

std::unique_ptr<rfcomm::Socket> connect_with_retry(
    const rfcomm::Address& target,
    const rfcomm::Uuid& uuid,
    int max_retries = 5
) {
    rfcomm::Client client;
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        std::cout << "Attempt " << attempt << "/" << max_retries << "...\n";
        auto sock = client.connect_by_sdp(target, uuid, std::chrono::seconds(5));
        if (sock) return sock;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return nullptr;
}
```
