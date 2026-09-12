#pragma once

#include <array>
#include <string>
#include <string_view>
#include <optional>
#include <compare>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace rfcomm {

/**
 * @brief Bluetooth Device Address (BD_ADDR, 48-bit MAC address).
 */
struct Address {
    // 6 bytes in network order (most significant byte at index 0: NAP..SAP)
    std::array<uint8_t, 6> bytes{0, 0, 0, 0, 0, 0};

    constexpr Address() = default;
    constexpr Address(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
        : bytes{b0, b1, b2, b3, b4, b5} {}

    constexpr auto operator<=>(const Address&) const = default;

    static constexpr Address any() noexcept {
        return Address{0, 0, 0, 0, 0, 0};
    }

    /**
     * @brief Parse BD_ADDR from string format "XX:XX:XX:XX:XX:XX" or "XX-XX-XX-XX-XX-XX".
     */
    static std::optional<Address> try_parse(std::string_view str) noexcept {
        std::array<uint8_t, 6> parsed{};
        size_t byte_idx = 0;
        int current_val = 0;
        int digits = 0;

        for (char c : str) {
            int hex_val = -1;
            if (c >= '0' && c <= '9') hex_val = c - '0';
            else if (c >= 'a' && c <= 'f') hex_val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') hex_val = c - 'A' + 10;

            if (hex_val >= 0) {
                current_val = (current_val << 4) | hex_val;
                digits++;
                if (digits > 2) return std::nullopt;
            } else if (c == ':' || c == '-') {
                if (digits != 2 || byte_idx >= 5) return std::nullopt;
                parsed[byte_idx++] = static_cast<uint8_t>(current_val);
                current_val = 0;
                digits = 0;
            } else {
                return std::nullopt;
            }
        }

        if (digits != 2 || byte_idx != 5) return std::nullopt;
        parsed[byte_idx] = static_cast<uint8_t>(current_val);

        Address res;
        res.bytes = parsed;
        return res;
    }

    static Address parse(std::string_view str) {
        auto opt = try_parse(str);
        if (!opt) {
            throw std::invalid_argument("Invalid Bluetooth address format: " + std::string(str));
        }
        return *opt;
    }

    std::string to_string() const {
        std::ostringstream oss;
        for (size_t i = 0; i < 6; ++i) {
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(bytes[i]);
            if (i + 1 < 6) oss << ":";
        }
        return oss.str();
    }

    /**
     * @brief Convert 48-bit address to 64-bit unsigned integer (as used in Windows BTH_ADDR).
     */
    uint64_t to_uint64() const noexcept {
        uint64_t val = 0;
        for (size_t i = 0; i < 6; ++i) {
            val = (val << 8) | bytes[i];
        }
        return val;
    }

    /**
     * @brief Create Address from 64-bit unsigned integer (lower 48 bits).
     */
    static Address from_uint64(uint64_t val) noexcept {
        Address res;
        for (int i = 5; i >= 0; --i) {
            res.bytes[i] = static_cast<uint8_t>(val & 0xFF);
            val >>= 8;
        }
        return res;
    }
};

/**
 * @brief 128-bit UUID for Bluetooth SDP service discovery and registration.
 */
struct Uuid {
    // 16 bytes in big-endian network order
    std::array<uint8_t, 16> bytes{0};

    constexpr Uuid() = default;
    constexpr explicit Uuid(const std::array<uint8_t, 16>& b) : bytes(b) {}

    constexpr auto operator<=>(const Uuid&) const = default;

    /**
     * @brief Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB
     */
    static constexpr Uuid from_16bit(uint16_t short_uuid) noexcept {
        Uuid res;
        res.bytes = {
            0x00, 0x00,
            static_cast<uint8_t>((short_uuid >> 8) & 0xFF),
            static_cast<uint8_t>(short_uuid & 0xFF),
            0x00, 0x00, 0x10, 0x00,
            0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
        };
        return res;
    }

    /**
     * @brief Standard Bluetooth Serial Port Profile (SPP) UUID: 00001101-...
     */
    static constexpr Uuid serial_port() noexcept {
        return from_16bit(0x1101);
    }

    /**
     * @brief Parse 128-bit UUID from string format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     *        or 16-bit hex "1101".
     */
    static std::optional<Uuid> try_parse(std::string_view str) noexcept {
        if (str.length() == 4) {
            // Short 16-bit UUID
            int val = 0;
            for (char c : str) {
                int hex_val = -1;
                if (c >= '0' && c <= '9') hex_val = c - '0';
                else if (c >= 'a' && c <= 'f') hex_val = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') hex_val = c - 'A' + 10;
                if (hex_val < 0) return std::nullopt;
                val = (val << 4) | hex_val;
            }
            return from_16bit(static_cast<uint16_t>(val));
        }

        std::array<uint8_t, 16> parsed{};
        size_t byte_idx = 0;
        int current_val = 0;
        int digits = 0;

        for (char c : str) {
            if (c == '-') continue;
            int hex_val = -1;
            if (c >= '0' && c <= '9') hex_val = c - '0';
            else if (c >= 'a' && c <= 'f') hex_val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') hex_val = c - 'A' + 10;

            if (hex_val < 0) return std::nullopt;

            current_val = (current_val << 4) | hex_val;
            digits++;
            if (digits == 2) {
                if (byte_idx >= 16) return std::nullopt;
                parsed[byte_idx++] = static_cast<uint8_t>(current_val);
                current_val = 0;
                digits = 0;
            }
        }

        if (digits != 0 || byte_idx != 16) return std::nullopt;
        return Uuid{parsed};
    }

    static Uuid parse(std::string_view str) {
        auto opt = try_parse(str);
        if (!opt) {
            throw std::invalid_argument("Invalid UUID format: " + std::string(str));
        }
        return *opt;
    }

    std::string to_string() const {
        std::ostringstream oss;
        for (size_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) oss << "-";
            oss << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                << static_cast<int>(bytes[i]);
        }
        return oss.str();
    }
};

} // namespace rfcomm
