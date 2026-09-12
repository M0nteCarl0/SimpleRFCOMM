#pragma once

#include "types.hpp"
#include "socket.hpp"
#include "sdp.hpp"
#include <memory>
#include <chrono>

namespace rfcomm {

using namespace std::chrono_literals;

/**
 * @brief RFCOMM Client supporting direct channel connection or UUID-based SDP discovery.
 */
class Client {
public:
    Client() = default;
    ~Client() = default;

    /**
     * @brief Connect directly to remote Bluetooth address and RFCOMM channel.
     * @param addr Remote Bluetooth BD_ADDR.
     * @param channel Remote RFCOMM channel (1..30).
     * @param timeout Optional connect timeout.
     * @return Connected Socket or nullptr on failure.
     */
    std::unique_ptr<Socket> connect(
        const Address& addr,
        uint8_t channel,
        std::chrono::milliseconds timeout = 10000ms
    );

    /**
     * @brief Discover channel via SDP and connect to the remote service by UUID.
     * @param addr Remote Bluetooth BD_ADDR.
     * @param uuid 128-bit service UUID to search for via SDP.
     * @param timeout Optional timeout.
     * @return Connected Socket or nullptr on failure.
     */
    std::unique_ptr<Socket> connect_by_sdp(
        const Address& addr,
        const Uuid& uuid,
        std::chrono::milliseconds timeout = 10000ms
    );
};

} // namespace rfcomm
