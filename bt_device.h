#ifndef BT_DEVICE_H
#define BT_DEVICE_H

#include "fiber.h"
#include "common.h"

namespace bt
{

class channel;
class adapter;

class device
{
    friend class adapter;
    friend class channel;

public:
    const bdaddr_t addr;

    condition connected;
    condition encrypted;
    condition authenticated;

    device(adapter &hci, const bdaddr_t &addr);
    ~device();

    void connect(u16 packet_Type, u8 page_scan_rep_mode, u16 clock_offset, u8 role);
    void accept(u8 role);

    void authenticate();
    void encrypt(u8 mode);

    void disconnect(u8 reason);

    void connect(channel &ch, u16 psm);
    void accept(channel &ch, u16 psm);

private:
    std::unordered_map<u8, promise<block> *> l2cap_commands;
    std::unordered_map<u16, promise<u16> *> accepting_psms;
    std::unordered_map<u16, channel &> channels;
    adapter &hci;

    condition slots_changed;
    u16 full_slots = 0;
    u16 total_slots = 0;

    u16 handle = 0;
    u16 next_cid = 0x0040;
    u8 next_ident = 0x01;

    int accepting = -1;

    void acquire_slot();
    void l2cap_send(u8 ident, u8 code, const void *src, usize size);

    template <typename TArg>
    u8 l2cap_cmd(u8 code, const TArg &arg)
    {
        auto ident = next_ident++;
        l2cap_send(ident, code, &arg, sizeof(TArg));
        return ident;
    }

    template <typename TOut>
    TOut *l2cap_await(u8 ident)
    {
        promise<block> result;
        l2cap_commands.emplace(ident, &result);
        auto ret = result.wait();
        l2cap_commands.erase(ident);

        return (TOut *)ret.data;
    }

    template <typename TOut, typename TArg>
    TOut *l2cap_cmd(u8 code, const TArg &arg)
    {
        auto ident = l2cap_cmd(code, arg);
        return l2cap_await<TOut>(ident);
    }

    template <typename TArg>
    void l2cap_reply(u8 ident, u8 code, const TArg &arg)
    {
        l2cap_send(ident, code, &arg, sizeof(TArg));
    }

    void acldata(block &pkt);

    template <typename T>
    void l2cap(u8 ident, block &pkt)
    {
        auto evt = pkt.advance<T>();
        if (!evt)
            return;

        l2cap(ident, evt);
    }

    void l2cap(u8 ident, l2cap_conn_req *req);
    void l2cap(u8 ident, l2cap_conf_req *req);
    void l2cap(u8 ident, l2cap_disconn_req *req);
    void l2cap(u8 ident, block &pkt); // echo
    void l2cap(u8 ident, l2cap_info_req *req);
    void l2cap(u8 ident, l2cap_create_req *req);
    void l2cap(u8 ident, l2cap_move_req *req);
    void l2cap(u8 ident, l2cap_move_cfm *req);

    void event(evt_conn_complete *evt);
    void event(evt_conn_request *evt);
    void event(evt_disconn_complete *evt);
    void event(evt_auth_complete *evt);
    void event(evt_remote_name_req_complete *evt);
    void event(evt_encrypt_change *evt);
    void event(evt_change_conn_link_key_complete *evt);
    void event(evt_master_link_key_complete *evt);
    void event(evt_read_remote_features_complete *evt);
    void event(evt_read_remote_version_complete *evt);
    void event(evt_qos_setup_complete *evt);
    void event(evt_flush_occured *evt);
    void event(evt_role_change *evt);
    void event(evt_mode_change *evt);
    void event(evt_pin_code_req *evt);
    void event(evt_link_key_req *evt);
    void event(evt_link_key_notify *evt);
    void event(evt_max_slots_change *evt);
    void event(evt_read_clock_offset_complete *evt);
    void event(evt_conn_ptype_changed *evt);
    void event(evt_qos_violation *evt);
    void event(evt_pscan_rep_mode_change *evt);
    void event(evt_flow_spec_complete *evt);
    void event(evt_read_remote_ext_features_complete *evt);
    void event(evt_sync_conn_complete *evt);
    void event(evt_sync_conn_changed *evt);
    void event(evt_sniff_subrating *evt);
    void event(evt_encryption_key_refresh_complete *evt);
    void event(evt_io_capability_request *evt);
    void event(evt_io_capability_response *evt);
    void event(evt_user_confirm_request *evt);
    void event(evt_user_passkey_request *evt);
    void event(evt_remote_oob_data_request *evt);
    void event(evt_simple_pairing_complete *evt);
    void event(evt_link_supervision_timeout_changed *evt);
    void event(evt_enhanced_flush_complete *evt);
    void event(evt_user_passkey_notify *evt);
    void event(evt_keypress_notify *evt);
    void event(evt_remote_host_features_notify *evt);
    void event(evt_flow_spec_modify_complete *evt);
};

} // namespace bt

#endif
