#include "bt_device.h"

#include "bt_adapter.h"
#include "bt_command.h"
#include "bt_channel.h"

#include <unistd.h>
#include <fcntl.h>

namespace bt
{

device::device(adapter &hci, const bdaddr_t &addr)
    : addr(addr), hci(hci)
{
    hci.attach(*this);
}

device::~device()
{
    hci.detach(*this);
}

void device::connect(u16 packet_type, u8 page_scan_rep_mode, u16 clock_offset, u8 role)
{
    command cmd(hci, 0x01, 0x0005);
    cmd.write(addr);
    cmd.write_u16(htobs(packet_type));
    cmd.write_u8(page_scan_rep_mode);
    cmd.write_u8(0x00);
    cmd.write_u16(htobs(clock_offset));
    cmd.write_u8(role);
    valid_status(cmd.run<u8>());
}

void device::accept(u8 role)
{
    accepting = role;
}

void device::authenticate()
{
    command cmd(hci, 0x01, 0x0011);
    cmd.write_u16(htobs(handle));
    valid_status(cmd.run<u8>());
}

void device::encrypt(u8 mode)
{
    command cmd(hci, 0x01, 0x0013);
    cmd.write_u16(htobs(handle));
    cmd.write_u8(mode);
    valid_status(cmd.run<u8>());
}

void device::qos_setup(u8 flags, u8 service_type, u32 token_rate, u32 peak_bw, u32 latency, u32 delay_variation)
{
    command cmd(hci, 0x02, 0x0007);
    cmd.write_u16(htobs(handle));
    cmd.write_u8(flags);
    cmd.write_u8(service_type);
    cmd.write_u32(htobl(token_rate));
    cmd.write_u32(htobl(peak_bw));
    cmd.write_u32(htobl(latency));
    cmd.write_u32(htobl(delay_variation));
    valid_status(cmd.run<u8>());
}

void device::disconnect(u8 reason)
{
    command cmd(hci, 0x01, 0x0006);
    cmd.write_u16(htobs(handle));
    cmd.write_u8(reason);
    valid_status(cmd.run<u8>());
}

void device::connect(channel &ch, u16 psm)
{
    auto local_cid = next_cid++;
    ch.handle = handle;
    channels.emplace(local_cid, ch);

    l2cap_conn_req req;
    req.scid = local_cid;
    req.psm = htobs(psm);
    auto ident = l2cap_cmd(L2CAP_CONN_REQ, req);

    l2cap_conn_rsp *rsp;
    do
    {
        rsp = l2cap_await<l2cap_conn_rsp>(ident);
    } while (btohs(rsp->result) == 0x0001);

    if (rsp->result != 0x0000)
        throw std::runtime_error("l2cap connection failed");

    ch.status = channel_status::CONFIG;
    ch.remote_cid = rsp->dcid;
}

void device::accept(channel &ch, u16 psm)
{
    auto local_cid = next_cid++;
    ch.handle = handle;
    channels.emplace(local_cid, ch);

    promise<u16> accept;
    accepting_psms.emplace(psm, &accept);

    ch.remote_cid = accept.wait();
    ch.status = channel_status::CONFIG;
}

void device::acquire_slot()
{
    // printf("send %zu %zu\n", full_slots, max_slots);

    // auto start = std::chrono::high_resolution_clock::now();

    while (full_slots >= total_slots)
    {
        printf("waiting\n");
        slots_changed.wait();
    }

    // auto end = std::chrono::high_resolution_clock::now();
    // printf("sending %ld\n", std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

    ++full_slots;
    slots_changed.notify();
}

void device::l2cap_send(u8 ident, u8 code, const void *src, usize size)
{
    acquire_slot();

    frame pkt(hci.send_buffer, sizeof(hci.send_buffer));
    pkt.write_u8(HCI_ACLDATA_PKT);
    auto acl = pkt.advance<hci_acl_hdr>();
    acl->handle = handle;
    acl->dlen = htobs(sizeof(l2cap_hdr) + sizeof(l2cap_cmd_hdr) + size);

    auto l2cap = pkt.advance<l2cap_hdr>();
    l2cap->cid = htobs(0x01);
    l2cap->len = htobs(sizeof(l2cap_cmd_hdr) + size);

    auto cmd = pkt.advance<l2cap_cmd_hdr>();
    cmd->code = code;
    cmd->ident = ident;
    cmd->len = htobs(size);

    pkt.write(src, size);
    hci.send(pkt.size);
}

void device::acldata(block &pkt)
{
    auto hdr = pkt.advance<l2cap_hdr>();
    if (!hdr)
        return;

    static const std::vector<void (device::*)(u8, block &)> handlers{
        /* 00 */ nullptr,
        /* 02 */ &device::l2cap<l2cap_conn_req>,
        /* 04 */ &device::l2cap<l2cap_conf_req>,
        /* 06 */ &device::l2cap<l2cap_disconn_req>,
        /* 08 */ &device::l2cap, // echo
        /* 0a */ &device::l2cap<l2cap_info_req>,
        /* 0c */ &device::l2cap<l2cap_create_req>,
        /* 0e */ &device::l2cap<l2cap_move_req>,
        /* 10 */ &device::l2cap<l2cap_move_cfm>,
    };

    if (btohs(hdr->cid) == 0x01)
    {
        auto cmd = pkt.advance<l2cap_cmd_hdr>();
        if (!cmd)
            return;

        if (cmd->code & 0x01)
        {
            auto req = l2cap_commands.find(cmd->ident);
            if (req == l2cap_commands.end())
                return;

            req->second->resolve(pkt);
        }
        else
        {
            auto handler = handlers[cmd->code >> 1];
            if (handler == nullptr)
                return;

            (this->*handler)(cmd->ident, pkt);
        }
    }
    else
    {
        auto channel = channels.find(hdr->cid);
        if (channel == channels.end())
            return;

        channel->second.data.emit(pkt);
    }
}

void device::l2cap(u8 ident, l2cap_conn_req *req)
{
    auto accept = accepting_psms.find(btohs(req->psm));

    l2cap_conn_rsp rsp;
    rsp.scid = req->scid;

    if (accept == accepting_psms.end())
    {
        rsp.dcid = 0;
        rsp.result = htobs(0x0002);
        rsp.status = htobs(0x0000);

        l2cap_reply(ident, L2CAP_CONN_RSP, rsp);
    }
    else
    {
        auto local_cid = htobs(next_cid++);

        rsp.dcid = local_cid;
        rsp.status = htobs(0x0000);
        rsp.result = htobs(0x0000);

        l2cap_reply(ident, L2CAP_CONN_RSP, rsp);

        accept->second->resolve(rsp.scid);
        accepting_psms.erase(accept);
    }
}

void device::l2cap(u8 ident, l2cap_conf_req *req)
{
    auto ch = channels.find(req->dcid);
    if (ch == channels.end() || (ch->second.status != channel_status::OPEN &&
                                 ch->second.status != channel_status::CONFIG))
    {
        u8 buffer[6];
        frame reject(buffer, sizeof(buffer));
        reject.write_u16(htobs(0x0002));
        reject.write_u16(0x0000);
        reject.write_u16(req->dcid);
        l2cap_reply(ident, L2CAP_COMMAND_REJ, buffer);
    }
    else
    {
        l2cap_conf_rsp rsp;
        rsp.scid = ch->second.remote_cid;
        rsp.flags = htobs(0x0000);
        rsp.result = htobs(0x0000);
        l2cap_reply(ident, L2CAP_CONF_RSP, rsp);

        ch->second.handshake.resolve();
    }
}

void device::l2cap(u8 ident, l2cap_disconn_req *req)
{
    auto ch = channels.find(req->dcid);
    if (ch == channels.end())
    {
        u8 buffer[6];
        frame reject(buffer, sizeof(buffer));
        reject.write_u16(htobs(0x0002));
        reject.write_u16(req->scid);
        reject.write_u16(req->dcid);
        l2cap_reply(ident, L2CAP_COMMAND_REJ, buffer);
    }
    else
    {
        u8 buffer[4];
        frame rsp(buffer, sizeof(buffer));
        rsp.write_u16(req->dcid);
        rsp.write_u16(req->scid);
        l2cap_reply(ident, L2CAP_DISCONN_RSP, buffer);
        ch->second.data.emit(block());
        ch->second.status = channel_status::CLOSED;
    }
}

void device::l2cap(u8 ident, block &pkt)
{
    l2cap_send(ident, L2CAP_ECHO_RSP, pkt.data, pkt.size);
}

void device::l2cap(u8 ident, l2cap_info_req *req)
{
    u8 buffer[L2CAP_INFO_RSP_SIZE];
    frame rsp_pkt(buffer, sizeof(buffer));
    auto rsp = rsp_pkt.advance<l2cap_info_rsp>();
    rsp->type = req->type;
    rsp->result = htobs(0x0001);
    l2cap_reply(ident, L2CAP_INFO_RSP, buffer);
}

void device::l2cap(u8 ident, l2cap_create_req *req)
{
}

void device::l2cap(u8 ident, l2cap_move_req *req)
{
}

void device::l2cap(u8 ident, l2cap_move_cfm *req)
{
}

void device::event(evt_conn_complete *evt)
{
    handle = evt->handle;
    hci.attach(*this);
    connected.notify();
}

void device::event(evt_conn_request *evt)
{
    if (accepting < 0)
    {
        command cmd(hci, 0x01, 0x000A);
        cmd.write(addr);
        cmd.write_u8(0x13);
        valid_status(cmd.run<u8>());
    }
    else
    {
        command cmd(hci, 0x01, 0x0009);
        cmd.write(addr);
        cmd.write_u8(accepting);
        valid_status(cmd.run<u8>());
    }
}

void device::event(evt_disconn_complete *evt)
{
    handle = 0;
}

void device::event(evt_auth_complete *evt)
{
    authenticated.notify();
}

void device::event(evt_remote_name_req_complete *evt)
{
}

void device::event(evt_encrypt_change *evt)
{
    encrypted.notify();
}

void device::event(evt_change_conn_link_key_complete *evt)
{
}

void device::event(evt_master_link_key_complete *evt)
{
}

void device::event(evt_read_remote_features_complete *evt)
{
}

void device::event(evt_read_remote_version_complete *evt)
{
}

void device::event(evt_qos_setup_complete *evt)
{
}

void device::event(evt_flush_occured *evt)
{
}

void device::event(evt_role_change *evt)
{
}

void device::event(evt_mode_change *evt)
{
    // if (evt->mode & 0x02)
    //     hci.exit_sniff_mode(conn);
}

void device::event(evt_pin_code_req *evt)
{
}

void device::event(evt_link_key_req *evt)
{
    char filename[9 + 18];
    strcpy(filename, "link_key/");
    ba2str(&addr, filename + 9);

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        command cmd(hci, 0x01, 0x000C);
        cmd.write(addr);
        cmd.send();
    }
    else
    {
        u8 link_key[16];
        read(fd, link_key, sizeof(link_key));
        close(fd);

        command cmd(hci, 0x01, 0x000B);
        cmd.write(addr);
        cmd.write(link_key);
        cmd.send();
    }
}

void device::event(evt_link_key_notify *evt)
{
    char filename[9 + 18];
    strcpy(filename, "link_key/");
    ba2str(&addr, filename + 9);
    filename[9 + 17] = '\0';

    int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT);
    if (fd < 0)
    {
        perror("failed to save link key");
        return;
    }

    write(fd, evt->link_key, sizeof(evt->link_key));
    close(fd);
}

void device::event(evt_max_slots_change *evt)
{
    total_slots = evt->max_slots;
    slots_changed.notify();
}

void device::event(evt_read_clock_offset_complete *evt)
{
}

void device::event(evt_conn_ptype_changed *evt)
{
}

void device::event(evt_qos_violation *evt)
{
}

void device::event(evt_pscan_rep_mode_change *evt)
{
}

void device::event(evt_flow_spec_complete *evt)
{
}

void device::event(evt_read_remote_ext_features_complete *evt)
{
}

void device::event(evt_sync_conn_complete *evt)
{
}

void device::event(evt_sync_conn_changed *evt)
{
}

void device::event(evt_sniff_subrating *evt)
{
}

void device::event(evt_encryption_key_refresh_complete *evt)
{
}

void device::event(evt_io_capability_request *evt)
{
    command cmd(hci, 0x01, 0x002b);
    cmd.write(addr);
    cmd.write_u8(0x03);
    cmd.write_u8(0x00);
    cmd.write_u8(0x00);
    valid_status(cmd.run<u8>());
}

void device::event(evt_io_capability_response *evt)
{
}

void device::event(evt_user_confirm_request *evt)
{
    command cmd(hci, 0x01, 0x002C); // 0x002D: deny. 0x002C: accept
    cmd.write(addr);
    valid_status(cmd.run<u8>());
}

void device::event(evt_user_passkey_request *evt)
{
}

void device::event(evt_remote_oob_data_request *evt)
{
}

void device::event(evt_simple_pairing_complete *evt)
{
}

void device::event(evt_link_supervision_timeout_changed *evt)
{
}

void device::event(evt_enhanced_flush_complete *evt)
{
}

void device::event(evt_user_passkey_notify *evt)
{
}

void device::event(evt_keypress_notify *evt)
{
}

void device::event(evt_remote_host_features_notify *evt)
{
}

void device::event(evt_flow_spec_modify_complete *evt)
{
}

} // namespace bt
