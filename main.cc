#include "fiber.h"
#include "common.h"

#include "bt_adapter.h"
#include "bt_command.h"
#include "bt_channel.h"
#include "bt_device.h"

static bt::adapter hci(0);
static bdaddr_t self;
static bdaddr_t pro_addr;
static bdaddr_t switch_addr;

void start_adapter()
{
    bt::device pro(hci, pro_addr);
    pro.connect(0xcc18, 0x02, 0x00, 0x00);
    printf("waiting for connect\n");
    pro.connected.wait();

    pro.authenticate();
    printf("waiting for authenticate\n");
    pro.authenticated.wait();

    pro.encrypt(0x01);
    printf("waiting for encrypt\n");
    pro.encrypted.wait();

    bt::channel hid_control(pro);
    bt::channel hid_interrupt(pro);

    pro.connect(hid_control, 0x11);
    pro.connect(hid_interrupt, 0x13);

    hid_control.configure();
    hid_interrupt.configure();

    pro.disconnect(0x13);
}

void configure_adapter()
{
    printf("start configure adapter\n");

    u8 event_mask[] = {0xFF, 0xFF, 0xFb, 0xFF, 0x07, 0xF8, 0xbf, 0x3d};
    block rsp;

    hci.reset();

    hci.set_event_mask(event_mask);
    hci.clear_event_filter();

    hci.set_default_link_policy(0x0F);

    hci.set_scan_mode(0x02);
    hci.set_page_scan_timing(0x1000, 0x800);
    hci.set_inquiry_scan_timing(0x1000, 0x800);

    hci.set_inquiry_mode(0x02);

    hci.set_pin_type(0x00);
    hci.set_simple_pairing_mode(0x01);

    hci.set_local_name("Pro Controller");
    // hci.set_device_class(0x01, 0x1c01);
    hci.set_device_class(0x02, 0x0025);

    auto version = hci.read_local_version();

    printf("hci_version: %02x\n", version->hci_ver);
    printf("hci_revised: %d\n", (int)btohs(version->hci_rev));
    printf("lmp_version: %d\n", version->lmp_ver);
    printf("manufacture: %d\n", (int)btohs(version->manufacturer));
    printf("lmp_subvers: %d\n", (int)btohs(version->lmp_subver));

    auto localaddr = hci.read_local_address();

    char name[18];
    ba2str(localaddr, name);
    printf("local address: %s\n", name);

    u8 description[240] = {0};
    description[0] = 0x0f;
    description[1] = 0x09;
    strcpy((char *)description + 2, "Pro Controller");

    hci.set_extended_inquiry_response(0, description);

    bt::command set_lap(hci, 0x03, 0x003a);
    set_lap.write_u8(0x01);
    set_lap.write_u8(0x33);
    set_lap.write_u8(0x8b);
    set_lap.write_u8(0x9e);
    set_lap.run(&rsp);

    printf("finished configure adapter\n");

    start_adapter();
}

int main(int argc, char **argv)
{
    str2ba("00:1a:7d:da:71:12", &self);
    str2ba("dc:68:eb:3b:f8:46", &pro_addr);
    str2ba("b8:8a:ec:91:17:c2", &switch_addr);

    fiber::create("main", configure_adapter);

    while (true)
    {
        fiber::run();
    }
}
