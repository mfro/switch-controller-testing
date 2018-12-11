#include "bt_channel.h"

#include "bt_adapter.h"
#include "bt_device.h"

namespace bt
{

channel::channel(device &dev) : dev(dev) {}
channel::~channel() {}

void channel::configure()
{
    u8 packet[L2CAP_CONF_REQ_SIZE + 4];
    auto conf_req = (l2cap_conf_req *)packet;
    conf_req->dcid = remote_cid;
    conf_req->flags = 0x0;
    packet[L2CAP_CONF_REQ_SIZE] = 0x01;
    packet[L2CAP_CONF_REQ_SIZE + 1] = 0x02;
    packet[L2CAP_CONF_REQ_SIZE + 2] = 0xC8;
    packet[L2CAP_CONF_REQ_SIZE + 3] = 0x05;
    dev.l2cap_cmd<l2cap_conf_rsp>(L2CAP_CONF_REQ, packet);

    handshake.wait();
}

void channel::send(const block &src)
{
    dev.acquire_slot();

    frame pkt(dev.hci.send_buffer, sizeof(dev.hci.send_buffer));

    pkt.write_u8(HCI_ACLDATA_PKT);
    auto acl = pkt.advance<hci_acl_hdr>();
    acl->handle = handle | 0x2000;
    acl->dlen = htobs(sizeof(l2cap_hdr) + src.size);

    auto l2cap = pkt.advance<l2cap_hdr>();
    l2cap->cid = remote_cid;
    l2cap->len = htobs(src.size);

    pkt.write(src.data, src.size);
    dev.hci.send(pkt.size);
}

} // namespace bt
