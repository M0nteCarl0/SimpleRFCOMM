#pragma once

#include "types.hpp"
#include "socket.hpp"
#include "sdp.hpp"
#include <memory>
#include <string>

namespace rfcomm {

/**
 * @brief RFCOMM Server with optional automatic SDP service registration.
 */
class Server {
public:
    Server();
    ~Server();

    Server(Server&& other) noexcept;
    Server& operator=(Server&& other) noexcept;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief Start listening on an RFCOMM channel.
     * @param channel Channel number (1..30), or 0 to let the OS assign an available channel.
     * @param backlog Max pending connection backlog.
     * @return true if listening started successfully.
     */
    bool listen(uint8_t channel = 0, int backlog = 4);

    /**
     * @brief Start listening and register the service in SDP so clients can find it by UUID.
     * @param uuid 128-bit UUID identifying this service.
     * @param service_name User-visible service name.
     * @param channel Desired channel (0 for auto-assignment).
     * @param backlog Pending connections backlog.
     * @return true if listening and SDP registration succeeded.
     */
    bool listen_with_sdp(
        const Uuid& uuid,
        const std::string& service_name = "RFCOMM Service",
        uint8_t channel = 0,
        int backlog = 4
    );

    /**
     * @brief Wait and accept an incoming client connection.
     * @return Connected Socket instance, or nullptr on error/server shutdown.
     */
    std::unique_ptr<Socket> accept();

    /**
     * @brief Stop listening and unregister SDP service.
     */
    void close();

    /**
     * @brief Check if server is currently listening.
     */
    bool is_listening() const noexcept;

    /**
     * @brief Get the actual assigned channel number.
     */
    uint8_t channel() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rfcomm
