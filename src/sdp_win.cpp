#if defined(_WIN32) || defined(_WIN64)

#include "simple_rfcomm/sdp.hpp"
#include "platform.hpp"
#include <vector>
#include <iostream>

namespace rfcomm::sdp {

struct Registration::Impl {
    WSAQUERYSETW qs{};
    std::wstring service_name_w;
    GUID svc_guid{};
    CSADDR_INFO cs_addr{};
    SOCKADDR_BTH sa{};
    bool registered = false;

    ~Impl() {
        unregister();
    }

    void unregister() {
        if (registered) {
            WSASetServiceW(&qs, RNRSERVICE_DELETE, 0);
            registered = false;
        }
    }
};

Registration::Registration() : pimpl_(std::make_unique<Impl>()) {
    detail::WinsockInit::ensure();
}

Registration::~Registration() = default;

Registration::Registration(Registration&& other) noexcept = default;
Registration& Registration::operator=(Registration&& other) noexcept = default;

void Registration::unregister() {
    if (pimpl_) {
        pimpl_->unregister();
    }
}

bool Registration::is_registered() const noexcept {
    return pimpl_ && pimpl_->registered;
}

std::unique_ptr<Registration> Registration::create(
    uint8_t channel,
    const Uuid& uuid,
    const std::string& service_name,
    const std::string& /*provider*/
) {
    auto reg = std::make_unique<Registration>();
    auto& impl = *reg->pimpl_;

    impl.svc_guid = detail::uuid_to_guid(uuid);

    // Convert service name to wide string
    impl.service_name_w = std::wstring(service_name.begin(), service_name.end());

    impl.sa.addressFamily = AF_BTH;
    impl.sa.port = channel;

    impl.cs_addr.LocalAddr.lpSockaddr = reinterpret_cast<LPSOCKADDR>(&impl.sa);
    impl.cs_addr.LocalAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
    impl.cs_addr.iSocketType = SOCK_STREAM;
    impl.cs_addr.iProtocol = BTHPROTO_RFCOMM;

    impl.qs.dwSize = sizeof(WSAQUERYSETW);
    impl.qs.lpszServiceInstanceName = impl.service_name_w.data();
    impl.qs.lpServiceClassId = &impl.svc_guid;
    impl.qs.dwNumberOfCsAddrs = 1;
    impl.qs.lpcsaBuffer = &impl.cs_addr;
    impl.qs.dwNameSpace = NS_BTH;

    int ret = WSASetServiceW(&impl.qs, RNRSERVICE_REGISTER, 0);
    if (ret != 0) {
        return nullptr;
    }

    impl.registered = true;
    return reg;
}

std::optional<uint8_t> resolve_channel(const Address& target, const Uuid& uuid) {
    detail::WinsockInit::ensure();

    std::string addr_str = target.to_string();
    std::wstring context = L"(" + std::wstring(addr_str.begin(), addr_str.end()) + L")";

    GUID svc_guid = detail::uuid_to_guid(uuid);

    WSAQUERYSETW qs{};
    qs.dwSize = sizeof(WSAQUERYSETW);
    qs.lpServiceClassId = &svc_guid;
    qs.dwNameSpace = NS_BTH;
    qs.lpszContext = context.data();

    HANDLE hLookup = NULL;
    DWORD flags = LUP_FLUSHCACHE | LUP_RETURN_ADDR;

    if (WSALookupServiceBeginW(&qs, flags, &hLookup) != 0) {
        return std::nullopt;
    }

    std::vector<BYTE> buffer(4096);
    DWORD buffer_len = static_cast<DWORD>(buffer.size());
    auto* pResults = reinterpret_cast<LPWSAQUERYSETW>(buffer.data());

    int res = WSALookupServiceNextW(hLookup, flags, &buffer_len, pResults);
    if (res != 0 && WSAGetLastError() == WSAEFAULT && buffer_len > buffer.size()) {
        buffer.resize(buffer_len);
        pResults = reinterpret_cast<LPWSAQUERYSETW>(buffer.data());
        res = WSALookupServiceNextW(hLookup, flags, &buffer_len, pResults);
    }

    std::optional<uint8_t> found_channel = std::nullopt;
    if (res == 0 && pResults->lpcsaBuffer != nullptr && pResults->dwNumberOfCsAddrs > 0) {
        auto* pRemote = reinterpret_cast<SOCKADDR_BTH*>(pResults->lpcsaBuffer->RemoteAddr.lpSockaddr);
        if (pRemote) {
            found_channel = static_cast<uint8_t>(pRemote->port);
        }
    }

    WSALookupServiceEnd(hLookup);
    return found_channel;
}

} // namespace rfcomm::sdp

#endif // _WIN32
