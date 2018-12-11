#include "bt_adapter.h"

#include "bt_device.h"
#include "bt_command.h"

#include <thread>

#include <unistd.h>

namespace bt
{

void valid_status(const u8 *status)
{
    switch (*status)
    {
    case 0x00: return;
    default:
        throw std::runtime_error(std::string("request failed: ") + std::to_string(*status));
    }
}

adapter::adapter(int num)
{
    fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    sockaddr_hci addr = {0};
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = num;
    addr.hci_channel = HCI_CHANNEL_USER;
    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("failed bind");
        return;
    }

    std::thread([this] { feed(); }).detach();
    fiber::create("adapter", [this] { run(); });
}

adapter::~adapter()
{
    close(fd);
}

void adapter::attach(command &cmd)
{
    commands.emplace(cmd.opcode, cmd);
}

void adapter::attach(device &dev)
{
    devices.emplace(dev.addr, dev);
    if (dev.handle != 0)
        connections.emplace(dev.handle, dev);
}

void adapter::detach(command &cmd)
{
    commands.erase(cmd.opcode);
}

void adapter::detach(device &dev)
{
    devices.erase(dev.addr);
    if (dev.handle != 0)
        connections.erase(dev.handle);
}

void adapter::feed()
{
    while (true)
    {
        int result = ::read(fd, recv_buffer, sizeof(recv_buffer));
        if (result > 0)
        {
            // printf("read %d\n", result);
            fiber::input([&] { recv.emit(result); });
        }
        else
        {
            error("read failed");
        }
    }

    // if (errno == EPIPE)
    // {
    //     while (true)
    //     {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(250));

    //         fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    //         sockaddr_hci addr;
    //         memset(&addr, 0, sizeof(addr));
    //         addr.hci_family = AF_BLUETOOTH;
    //         addr.hci_dev = 0;
    //         addr.hci_channel = HCI_CHANNEL_USER;
    //         if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    //         {
    //             perror("failed bind");
    //             continue;
    //         }

    //         break;
    //     }
    // }
    // else
    // {
    // }
}

void adapter::send(usize size)
{
    while (write(fd, send_buffer, size) < 0)
    {
        perror("write failed");
        if (errno == EAGAIN || errno == EINTR)
            continue;
        error("failed to write");
    }
}

void adapter::run()
{
    static const std::vector<void (adapter::*)(block & pkt)> event_handlers{
        /*00*/ nullptr,
        /*01*/ nullptr, // inquiry complete
        /*02*/ nullptr, // inquiry result
        /*03*/ &adapter::device_event<evt_conn_complete>,
        /*04*/ &adapter::device_event<evt_conn_request>,
        /*05*/ &adapter::connection_event<evt_disconn_complete>,
        /*06*/ &adapter::connection_event<evt_auth_complete>,
        /*07*/ &adapter::device_event<evt_remote_name_req_complete>,
        /*08*/ &adapter::connection_event<evt_encrypt_change>,
        /*09*/ &adapter::connection_event<evt_change_conn_link_key_complete>,
        /*0a*/ &adapter::connection_event<evt_master_link_key_complete>,
        /*0b*/ &adapter::connection_event<evt_read_remote_features_complete>,
        /*0c*/ &adapter::connection_event<evt_read_remote_version_complete>,
        /*0d*/ &adapter::connection_event<evt_qos_setup_complete>,
        /*0e*/ nullptr, // EVT_CMD_COMPLETE
        /*0f*/ nullptr, // EVT_CMD_STATUS
        /*10*/ nullptr, // EVT_HARDWARE_ERROR
        /*11*/ &adapter::connection_event<evt_flush_occured>,
        /*12*/ &adapter::device_event<evt_role_change>,
        /*13*/ nullptr, // number of completed packets
        /*14*/ &adapter::connection_event<evt_mode_change>,
        /*15*/ nullptr, // return link keys
        /*16*/ &adapter::device_event<evt_pin_code_req>,
        /*17*/ &adapter::device_event<evt_link_key_req>,
        /*18*/ &adapter::device_event<evt_link_key_notify>,
        /*19*/ nullptr, // loopback
        /*1a*/ nullptr, // data buffer overflow
        /*1b*/ &adapter::connection_event<evt_max_slots_change>,
        /*1c*/ &adapter::connection_event<evt_read_clock_offset_complete>,
        /*1d*/ &adapter::connection_event<evt_conn_ptype_changed>,
        /*1e*/ &adapter::connection_event<evt_qos_violation>,
        /*1f*/ nullptr, // unused
        /*20*/ &adapter::device_event<evt_pscan_rep_mode_change>,
        /*21*/ &adapter::connection_event<evt_flow_spec_complete>,
        /*22*/ nullptr, // inquiry result with RSSI
        /*23*/ &adapter::connection_event<evt_read_remote_ext_features_complete>,
        /*24*/ nullptr, // unused
        /*25*/ nullptr, // unused
        /*26*/ nullptr, // unused
        /*27*/ nullptr, // unused
        /*28*/ nullptr, // unused
        /*29*/ nullptr, // unused
        /*2a*/ nullptr, // unused
        /*2b*/ nullptr, // unused
        /*2c*/ &adapter::device_event<evt_sync_conn_complete>,
        /*2d*/ &adapter::connection_event<evt_sync_conn_changed>,
        /*2e*/ &adapter::connection_event<evt_sniff_subrating>,
        /*2f*/ nullptr, // extended inquiry result
        /*30*/ &adapter::connection_event<evt_encryption_key_refresh_complete>,
        /*31*/ &adapter::device_event<evt_io_capability_request>,
        /*32*/ &adapter::device_event<evt_io_capability_response>,
        /*33*/ &adapter::device_event<evt_user_confirm_request>,
        /*34*/ &adapter::device_event<evt_user_passkey_request>,
        /*35*/ &adapter::device_event<evt_remote_oob_data_request>,
        /*36*/ &adapter::device_event<evt_simple_pairing_complete>,
        /*37*/ nullptr, // unused
        /*38*/ &adapter::connection_event<evt_link_supervision_timeout_changed>,
        /*39*/ &adapter::connection_event<evt_enhanced_flush_complete>,
        /*3a*/ nullptr, // unused
        /*3b*/ &adapter::device_event<evt_user_passkey_notify>,
        /*3c*/ &adapter::device_event<evt_keypress_notify>,
        /*3d*/ &adapter::device_event<evt_remote_host_features_notify>,
        /*3e*/ nullptr, // LE meta event
        /*3f*/ nullptr, // unused
        /*40*/ nullptr, // physical link complete
        /*41*/ nullptr, // channel selected
        /*42*/ nullptr, // disconnection physical link complete
        /*43*/ nullptr, // physical link loss early warning
        /*44*/ nullptr, // physical link recovery
        /*45*/ nullptr, // logical link complete
        /*46*/ nullptr, // disconnection logical link complete
        /*47*/ &adapter::connection_event<evt_flow_spec_modify_complete>,
        /*48*/ nullptr, // number of completed data blocks
        /*49*/ nullptr, // AMP start test
        /*4a*/ nullptr, // AMP test end
        /*4b*/ nullptr, // AMP receiver report
        /*4c*/ nullptr, // short range mode change complete
        /*4d*/ nullptr, // AMP status change
    };

    while (true)
    {
        usize size;
        recv.next(&size);

        // printf("run %zu\n", size);

        if (size == 0)
            continue;

        block pkt(recv_buffer, size);

        auto type = pkt.read_u8();
        if (type == HCI_ACLDATA_PKT)
        {
            auto hdr = pkt.advance<hci_acl_hdr>();
            if (!hdr)
                continue;

            auto handle = htobs(btohs(hdr->handle) & 0xFFF);
            auto dev = connections.find(handle);
            if (dev == connections.end())
                continue;

            dev->second.acldata(pkt);
        }
        else if (type == HCI_EVENT_PKT)
        {
            auto hdr = pkt.advance<hci_event_hdr>();
            if (!hdr)
                continue;

            // printf("event %x\n", hdr->evt);

            switch (hdr->evt)
            {
            case EVT_CMD_COMPLETE:
            {
                auto evt = pkt.advance<evt_cmd_complete>();
                // printf("complete %x\n", btohs(evt->opcode));
                auto cmd = commands.find(btohs(evt->opcode));
                if (cmd == commands.end())
                    continue;

                cmd->second.result.emit(pkt);
                break;
            }

            case EVT_CMD_STATUS:
            {
                auto evt = pkt.advance<evt_cmd_status>();
                // printf("status %x\n", btohs(evt->opcode));
                auto cmd = commands.find(btohs(evt->opcode));
                if (cmd == commands.end())
                    continue;

                cmd->second.result.emit(block(&evt->status, 1));
                break;
            }

            case EVT_INQUIRY_RESULT:
            case EVT_INQUIRY_RESULT_WITH_RSSI:
            case EVT_EXTENDED_INQUIRY_RESULT:
            {
                inquiry_result.emit(pkt);
                break;
            }

            case EVT_NUM_COMP_PKTS:
            {
                auto count = pkt.read_u8();
                auto handles = (u16 *)pkt.data;
                auto pkt_counts = ((u16 *)pkt.data) + count;
                for (auto i = 0; i < count; ++i)
                {
                    auto dev = connections.find(handles[i]);
                    if (dev == connections.end())
                    {
                        printf("connection not found\n");
                        continue;
                    }

                    dev->second.full_slots -= btohs(pkt_counts[i]);
                    dev->second.slots_changed.notify();
                }
                break;
            }

            case EVT_VENDOR:
                break;

            default:
            {
                auto handler = event_handlers[hdr->evt];
                if (handler == nullptr)
                    continue;

                (this->*handler)(pkt);
            }
            }
        }
    }
}

void adapter::start_inquiry(u32 lap, u8 length, u8 num_rsp)
{
    command cmd(*this, 0x01, 0x0001);
    cmd.write_u8(lap & 0xFF);
    cmd.write_u8((lap >> 8) & 0xFF);
    cmd.write_u8((lap >> 16) & 0xFF);
    cmd.write_u8(length);
    cmd.write_u8(num_rsp);

    valid_status(cmd.run<u8>());
}

void adapter::stop_inquiry()
{
    command cmd(*this, 0x01, 0x0002);
    valid_status(cmd.run<u8>());
}

void adapter::set_default_link_policy(u16 policy)
{
    command cmd(*this, 0x02, 0x000F);
    cmd.write_u16(htobs(policy));
    valid_status(cmd.run<u8>());
}

void adapter::set_event_mask(u8 *mask)
{
    command cmd(*this, 0x03, 0x0001);
    cmd.write(mask, 8);
    valid_status(cmd.run<u8>());
}

void adapter::reset()
{
    command cmd(*this, 0x03, 0x0003);
    valid_status(cmd.run<u8>());
}

void adapter::clear_event_filter()
{
    command cmd(*this, 0x03, 0x0005);
    cmd.write_u8(0x00);
    valid_status(cmd.run<u8>());
}

void adapter::set_pin_type(u8 pin_type)
{
    command cmd(*this, 0x03, 0x000A);
    cmd.write_u8(pin_type);
    valid_status(cmd.run<u8>());
}

void adapter::set_local_name(const char *name)
{
    command cmd(*this, 0x03, 0x0013);
    strncpy((char *)cmd.data, name, 248);
    cmd.data += 248;
    cmd.size += 248;
    cmd.unused -= 248;
    valid_status(cmd.run<u8>());
}

void adapter::set_scan_mode(u8 scan_mode)
{
    command cmd(*this, 0x03, 0x001A);
    cmd.write_u8(scan_mode);
    valid_status(cmd.run<u8>());
}

void adapter::set_page_scan_timing(u16 interval, u16 duration)
{
    command cmd(*this, 0x03, 0x001C);
    cmd.write_u16(htobs(interval));
    cmd.write_u16(htobs(duration));
    valid_status(cmd.run<u8>());
}

void adapter::set_inquiry_scan_timing(u16 interval, u16 duration)
{
    command cmd(*this, 0x03, 0x001E);
    cmd.write_u16(htobs(interval));
    cmd.write_u16(htobs(duration));
    valid_status(cmd.run<u8>());
}

void adapter::set_auth_mode(u8 auth_mode)
{
    command cmd(*this, 0x03, 0x0020);
    cmd.write_u8(auth_mode);
    valid_status(cmd.run<u8>());
}

void adapter::set_device_class(u8 minor, u16 major)
{
    command cmd(*this, 0x03, 0x0024);
    cmd.write_u8(minor << 2);
    cmd.write_u8(major & 0xFF);
    cmd.write_u8(major >> 8);
    valid_status(cmd.run<u8>());
}

void adapter::set_inquiry_mode(u8 inquiry_mode)
{
    command cmd(*this, 0x03, 0x0045);
    cmd.write_u8(inquiry_mode);
    valid_status(cmd.run<u8>());
}

void adapter::set_extended_inquiry_response(u8 fec_required, u8 *data)
{
    command cmd(*this, 0x03, 0x0052);
    cmd.write_u8(fec_required);
    cmd.write(data, 240);
    valid_status(cmd.run<u8>());
}

void adapter::set_simple_pairing_mode(u8 mode)
{
    command cmd(*this, 0x03, 0x0056);
    cmd.write_u8(mode);
    valid_status(cmd.run<u8>());
}

read_local_version_rp *adapter::read_local_version()
{
    command cmd(*this, 0x04, 0x0001);
    return cmd.run<read_local_version_rp>();
}

bdaddr_t *adapter::read_local_address()
{
    command cmd(*this, 0x04, 0x0009);
    block result;
    cmd.run(&result);
    valid_status(result.data);
    return (bdaddr_t *)(result.data + 1);
}

} // namespace bt
