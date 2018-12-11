#include "bt_command.h"

#include "bt_adapter.h"

namespace bt
{

command::command(adapter &hci, u16 ogf, u16 ocf)
    : frame(hci.send_buffer, sizeof(hci.send_buffer)),
      opcode(cmd_opcode_pack(ogf, ocf)),
      hci(hci)
{
    write_u8(HCI_COMMAND_PKT);
    write_u16(htobs(opcode));
    write_u8(0);

    hci.attach(*this);
}

command::~command()
{
    hci.detach(*this);
}

void command::send()
{
    auto size_ptr = (u8 *)(hci.send_buffer + 3);
    *size_ptr = frame::size - 4;

    hci.send(size);
}

// void command::run(block *out)
// {
//     send();

//     hci->register_command(this);
//     *out = result.wait();
//     hci->deregister_command(this);
// }

}; // namespace bt
