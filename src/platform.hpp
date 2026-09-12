#pragma once

#include "simple_rfcomm/types.hpp"
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2bth.h>
#  include <bthsdpdef.h>
#  include <bluetoothapis.h>
#  pragma comment(lib, "ws2_32.lib")
#  pragma comment(lib, "bthprops.lib")

namespace rfcomm::detail {
    using native_socket_t = SOCKET;
    inline constexpr native_socket_t INVALID_SOCKET_HANDLE = INVALID_SOCKET;

    inline void close_socket_handle(native_socket_t s) noexcept {
        if (s != INVALID_SOCKET) {
            closesocket(s);
        }
    }

    struct WinsockInit {
        WinsockInit() {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        ~WinsockInit() {
            WSACleanup();
        }
        static void ensure() {
            static WinsockInit instance;
        }
    };

    inline BTH_ADDR address_to_bth(const Address& addr) noexcept {
        return static_cast<BTH_ADDR>(addr.to_uint64());
    }

    inline Address bth_to_address(BTH_ADDR bth) noexcept {
        return Address::from_uint64(static_cast<uint64_t>(bth));
    }

    inline GUID uuid_to_guid(const Uuid& u) noexcept {
        GUID g;
        // u.bytes: 16 bytes big-endian
        g.Data1 = (static_cast<uint32_t>(u.bytes[0]) << 24) |
                  (static_cast<uint32_t>(u.bytes[1]) << 16) |
                  (static_cast<uint32_t>(u.bytes[2]) << 8)  |
                   static_cast<uint32_t>(u.bytes[3]);
        g.Data2 = (static_cast<uint16_t>(u.bytes[4]) << 8)  |
                   static_cast<uint16_t>(u.bytes[5]);
        g.Data3 = (static_cast<uint16_t>(u.bytes[6]) << 8)  |
                   static_cast<uint16_t>(u.bytes[7]);
        for (int i = 0; i < 8; ++i) {
            g.Data4[i] = u.bytes[8 + i];
        }
        return g;
    }

    inline Uuid guid_to_uuid(const GUID& g) noexcept {
        Uuid u;
        u.bytes[0] = static_cast<uint8_t>((g.Data1 >> 24) & 0xFF);
        u.bytes[1] = static_cast<uint8_t>((g.Data1 >> 16) & 0xFF);
        u.bytes[2] = static_cast<uint8_t>((g.Data1 >> 8) & 0xFF);
        u.bytes[3] = static_cast<uint8_t>(g.Data1 & 0xFF);
        u.bytes[4] = static_cast<uint8_t>((g.Data2 >> 8) & 0xFF);
        u.bytes[5] = static_cast<uint8_t>(g.Data2 & 0xFF);
        u.bytes[6] = static_cast<uint8_t>((g.Data3 >> 8) & 0xFF);
        u.bytes[7] = static_cast<uint8_t>(g.Data3 & 0xFF);
        for (int i = 0; i < 8; ++i) {
            u.bytes[8 + i] = g.Data4[i];
        }
        return u;
    }
}

#elif defined(__linux__)
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <bluetooth/bluetooth.h>
#  include <bluetooth/rfcomm.h>
#  include <bluetooth/sdp.h>
#  include <bluetooth/sdp_lib.h>

namespace rfcomm::detail {
    using native_socket_t = int;
    inline constexpr native_socket_t INVALID_SOCKET_HANDLE = -1;

    inline void close_socket_handle(native_socket_t s) noexcept {
        if (s >= 0) {
            ::close(s);
        }
    }

    struct WinsockInit {
        static void ensure() noexcept {}
    };

    inline bdaddr_t address_to_bdaddr(const Address& addr) noexcept {
        bdaddr_t b;
        // bdaddr_t has b[0] = least significant byte (NAP..SAP reversed)
        for (int i = 0; i < 6; ++i) {
            b.b[i] = addr.bytes[5 - i];
        }
        return b;
    }

    inline Address bdaddr_to_address(const bdaddr_t& b) noexcept {
        Address a;
        for (int i = 0; i < 6; ++i) {
            a.bytes[5 - i] = b.b[i];
        }
        return a;
    }
}

#else
#  error "Unsupported operating system! SimpleRFCOMM supports Windows and Linux."
#endif
