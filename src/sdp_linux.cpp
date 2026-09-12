#if defined(__linux__)

#include "simple_rfcomm/sdp.hpp"
#include "platform.hpp"
#include <iostream>

namespace rfcomm::sdp {

struct Registration::Impl {
    sdp_session_t* session = nullptr;
    sdp_record_t* record = nullptr;
    bool registered = false;

    ~Impl() {
        unregister();
    }

    void unregister() {
        if (session && record && registered) {
            sdp_record_unregister(session, record);
            registered = false;
        }
        if (session) {
            sdp_close(session);
            session = nullptr;
        }
        // record is freed by BlueZ or unregister
        record = nullptr;
    }
};

Registration::Registration() : pimpl_(std::make_unique<Impl>()) {}

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
    const std::string& provider
) {
    auto reg = std::make_unique<Registration>();
    auto& impl = *reg->pimpl_;

    impl.record = sdp_record_alloc();
    if (!impl.record) {
        return nullptr;
    }

    uuid_t root_uuid;
    sdp_uuid128_create(&root_uuid, const_cast<uint8_t*>(uuid.bytes.data()));
    sdp_set_service_id(impl.record, root_uuid);

    sdp_list_t *root_list = sdp_list_append(0, &root_uuid);
    sdp_set_service_classes(impl.record, root_list);
    sdp_list_free(root_list, 0);

    uuid_t group_uuid;
    sdp_uuid16_create(&group_uuid, PUBLIC_BROWSE_GROUP);
    sdp_list_t *group_list = sdp_list_append(0, &group_uuid);
    sdp_set_browse_groups(impl.record, group_list);
    sdp_list_free(group_list, 0);

    uuid_t l2cap_uuid, rfcomm_uuid;
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    sdp_list_t *l2cap_list = sdp_list_append(0, &l2cap_uuid);
    sdp_list_t *proto_list = sdp_list_append(0, l2cap_list);

    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    uint8_t ch = channel;
    sdp_data_t *channel_data = sdp_data_alloc(SDP_UINT8, &ch);
    sdp_list_t *rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
    sdp_list_append(rfcomm_list, channel_data);
    sdp_list_append(proto_list, rfcomm_list);

    sdp_list_t *access_proto_list = sdp_list_append(0, proto_list);
    sdp_set_access_protos(impl.record, access_proto_list);

    sdp_set_info_attr(impl.record, service_name.c_str(), provider.c_str(), service_name.c_str());

    bdaddr_t any_addr = {};
    bdaddr_t local_addr = {{0, 0, 0, 0xff, 0xff, 0xff}};
    impl.session = sdp_connect(&any_addr, &local_addr, SDP_RETRY_IF_BUSY);
    if (!impl.session) {
        return nullptr;
    }

    if (sdp_record_register(impl.session, impl.record, 0) < 0) {
        sdp_close(impl.session);
        impl.session = nullptr;
        return nullptr;
    }

    impl.registered = true;
    return reg;
}

std::optional<uint8_t> resolve_channel(const Address& target, const Uuid& uuid) {
    bdaddr_t target_bdaddr = detail::address_to_bdaddr(target);
    bdaddr_t any_addr = {};

    sdp_session_t *session = sdp_connect(&any_addr, &target_bdaddr, SDP_RETRY_IF_BUSY);
    if (!session) {
        return std::nullopt;
    }

    uuid_t svc_uuid;
    sdp_uuid128_create(&svc_uuid, const_cast<uint8_t*>(uuid.bytes.data()));
    sdp_list_t *search_list = sdp_list_append(0, &svc_uuid);

    uint32_t range = 0x0000ffff;
    sdp_list_t *attr_list = sdp_list_append(0, &range);

    sdp_list_t *response_list = nullptr;
    int status = sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attr_list, &response_list);

    uint8_t found_channel = 0;
    if (status == 0 && response_list) {
        for (sdp_list_t *r = response_list; r; r = r->next) {
            auto *rec = static_cast<sdp_record_t *>(r->data);
            sdp_list_t *proto_list = nullptr;
            if (sdp_get_access_protos(rec, &proto_list) == 0) {
                for (sdp_list_t *p = proto_list; p; p = p->next) {
                    auto *pds = static_cast<sdp_list_t *>(p->data);
                    for (sdp_list_t *d = pds; d; d = d->next) {
                        auto *sdp_data = static_cast<sdp_data_t *>(d->data);
                        int proto[10];
                        int i = 0;
                        for (; sdp_data; sdp_data = sdp_data->next) {
                            if (sdp_data->dtd == SDP_UUID16 || sdp_data->dtd == SDP_UUID32 || sdp_data->dtd == SDP_UUID128) {
                                proto[i++] = sdp_data->val.uuid.value.uuid16;
                            }
                            if (sdp_data->dtd == SDP_UINT8 && i > 0 && proto[i - 1] == RFCOMM_UUID) {
                                found_channel = sdp_data->val.uint8;
                                break;
                            }
                        }
                        if (found_channel > 0) break;
                    }
                    sdp_list_free(pds, 0);
                    if (found_channel > 0) break;
                }
                sdp_list_free(proto_list, 0);
            }
            sdp_record_free(rec);
            if (found_channel > 0) break;
        }
        sdp_list_free(response_list, 0);
    }

    sdp_list_free(search_list, 0);
    sdp_list_free(attr_list, 0);
    sdp_close(session);

    if (found_channel > 0) {
        return found_channel;
    }
    return std::nullopt;
}

} // namespace rfcomm::sdp

#endif // __linux__
