#pragma once

#include "types.hpp"
#include <string>
#include <optional>
#include <memory>

namespace rfcomm::sdp {

/**
 * @brief Discover the RFCOMM channel number of a remote device service by UUID.
 * @param target Remote Bluetooth device address.
 * @param uuid 128-bit service UUID to search for.
 * @return Channel number (1..30) if found, std::nullopt otherwise.
 */
std::optional<uint8_t> resolve_channel(const Address& target, const Uuid& uuid);

/**
 * @brief RAII Bluetooth SDP Service Record Registration Guard.
 * Automatically unregisters the service record when destroyed.
 */
class Registration {
public:
    Registration();
    ~Registration();

    Registration(Registration&& other) noexcept;
    Registration& operator=(Registration&& other) noexcept;
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    /**
     * @brief Publish a service record in SDP.
     * @param channel RFCOMM channel the service is listening on.
     * @param uuid Service UUID.
     * @param service_name User-friendly service name.
     * @param provider Service provider description.
     */
    static std::unique_ptr<Registration> create(
        uint8_t channel,
        const Uuid& uuid,
        const std::string& service_name = "RFCOMM Service",
        const std::string& provider = "SimpleRFCOMM"
    );

    /**
     * @brief Unregister the service immediately.
     */
    void unregister();

    /**
     * @brief Check if record is currently registered.
     */
    bool is_registered() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace rfcomm::sdp
