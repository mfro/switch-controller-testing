#ifndef BT_CHANNEL_H
#define BT_CHANNEL_H

#include "fiber.h"
#include "common.h"

namespace bt
{

enum class channel_status : u8
{
    CLOSED,
    WAIT_CONNECT,
    WAIT_CONNECT_RSP,
    CONFIG,
    OPEN,
    WAIT_DISCONNECT,
    WAIT_CREATE,
    WAIT_CREATE_RSP,
    WAIT_MOVE,
    WAIT_MOVE_RSP,
    WAIT_MOVE_CONFIRM,
    WAIT_CONFIRM_RSP,
};

class adapter;
class device;

class channel
{
    friend class device;
    friend class adapter;

public:
    channel(device &dev);
    ~channel();

    void configure();

    void send(const block &src);

    task handshake;
    emitter<block> data;

private:
    device &dev;

    u16 handle;
    u16 remote_cid;
    channel_status status = channel_status::CLOSED;
};

} // namespace bt

#endif
