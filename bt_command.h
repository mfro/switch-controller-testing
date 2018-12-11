#ifndef BT_COMMAND_H
#define BT_COMMAND_H

#include "fiber.h"
#include "common.h"

namespace bt
{

class adapter;

class command : public frame
{
    friend class adapter;

public:
    const u16 opcode;
    emitter<block> result;

    command(adapter &hci, u16 ogf, u16 ocf);
    ~command();

    void send();

    void run(block *out)
    {
        send();
        result.next(out);
    }

    template <typename T>
    T *run()
    {
        send();
        block pkt;
        result.next(&pkt);
        return (T *)pkt.data;
    }

private:
    adapter &hci;
};

} // namespace bt

#endif
