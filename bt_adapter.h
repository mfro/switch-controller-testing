#ifndef BT_ADAPTER_H
#define BT_ADAPTER_H

#include "fiber.h"
#include "common.h"

namespace bt
{

void valid_status(const u8 *status);

class command;
class device;

class adapter
{
public:
    u8 send_buffer[HCI_MAX_FRAME_SIZE];

    emitter<block> inquiry_result;

    adapter(int num);
    ~adapter();

    void send(usize size);

    void attach(command &cmd);
    void attach(device &dev);
    void detach(command &cmd);
    void detach(device &dev);

    void start_inquiry(u32 lap, u8 length, u8 num_rsp);
    void stop_inquiry();

    void set_default_link_policy(u16 policy);

    void set_event_mask(u8 *mask);
    void reset();
    void clear_event_filter();

    void set_pin_type(u8 pin_type);
    void set_local_name(const char *name);
    void set_scan_mode(u8 scan_mode);
    void set_page_scan_timing(u16 interval, u16 duration);
    void set_inquiry_scan_timing(u16 interval, u16 duration);
    void set_auth_mode(u8 auth_mode);
    void set_device_class(u8 minor, u16 major);
    void set_inquiry_mode(u8 inquiry_mode);
    void set_extended_inquiry_response(u8 fec_required, u8 *data);
    void set_simple_pairing_mode(u8 mode);

    read_local_version_rp *read_local_version();
    bdaddr_t *read_local_address();

private:
    u8 recv_buffer[HCI_MAX_FRAME_SIZE];
    emitter<usize> recv;

    std::unordered_map<u16, command &> commands;
    std::unordered_map<bdaddr_t, device &> devices;
    std::unordered_map<u16, device &> connections;

    int fd;
    bool handled = false;

    void feed();
    void run();
    void dispatch(block pkt);

    template <typename T>
    void device_event(block &pkt)
    {
        auto evt = pkt.advance<T>();
        if (!evt)
            return;
        auto dev = devices.find(evt->bdaddr);
        if (dev == devices.end())
        {
            printf("device not found\n");
            return;
        }
        dev->second.event(evt);
    }

    template <typename T>
    void connection_event(block &pkt)
    {
        auto evt = pkt.advance<T>();
        if (!evt)
            return;
        auto dev = connections.find(evt->handle);
        if (dev == connections.end())
        {
            printf("connection not found\n");
            return;
        }
        dev->second.event(evt);
    }
};

} // namespace bt

#endif
