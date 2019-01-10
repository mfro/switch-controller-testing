#include "fiber.h"
#include "common.h"

#include "bt_adapter.h"
#include "bt_command.h"
#include "bt_channel.h"
#include "bt_device.h"

#include <bitset>
#include <chrono>
#include <arpa/inet.h>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;

// A,B,X,Y
// up,down,left,right
// l-bumper,r-bumper,l-stick,r-stick
// start,back,guide,capture

static bt::adapter hci(0);
static bdaddr_t self;
static bdaddr_t pro_addr;
static bdaddr_t switch_addr;

void start_adapter()
{
    hci.start_inquiry(0x9e8b33, 0x30, 255);

    while (true)
    {
        block pkt;
        hci.inquiry_result.next(&pkt);

        auto count = pkt.read_u8();
        auto info = pkt.advance<extended_inquiry_info>();
        if (info->bdaddr != pro_addr)
            continue;

        hci.stop_inquiry();
        break;
    }

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

    printf("done\n");

    u8 previous_leds = 0xFF;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in remote = {0};
    remote.sin_family = AF_INET;
    remote.sin_port = 25565;
    inet_pton(AF_INET, "10.0.1.32", &remote.sin_addr);

    printf("connecting...\n");

    if (connect(sock, (sockaddr *)&remote, sizeof(remote)) < 0)
        error("failed to connect");

    printf("connected to feeder\n");

    while (true)
    {
        block input;
        hid_interrupt.data.next(&input);

        input.read_u8();
        auto type = input.read_u8();
        if (type == 0x3f)
        {
            auto b1 = input.read_u8();
            auto b2 = input.read_u8();
            auto hat = input.read_u8();
            auto s1 = input.read_u16();
            auto s2 = input.read_u16();
            auto s3 = input.read_u16();
            auto s4 = input.read_u16();

            u8 report[20];
            memset(&report, 0, sizeof(report));

            report[0] = (b1 & 0x02) |        // A
                        ((b1 & 0x01) << 1) | // B
                        ((b1 & 0x08) << 2) | // X
                        ((b1 & 0x04) << 3) | // Y
                        ((0) << 4) |         // UP
                        ((0) << 5) |         // DOWN
                        ((0) << 6) |         // LEFT
                        ((0) << 7);          // RIGHT

            report[1] = (b1 & 0x10) |        // l-bumper
                        ((b1 & 0x20) << 1) | // r-bumper
                        ((b2 & 0x04) << 2) | // l-stick
                        ((b2 & 0x08) << 3) | // r-stick
                        ((b2 & 0x02) << 4) | // start
                        ((b2 & 0x01) << 5) | // back
                        ((b2 & 0x10) << 6) | // guide
                        ((b2 & 0x20) << 7);  // capture

            send(sock, &report, sizeof(report), 0);

            std::bitset<16> buttons;
            buttons[0] = b2 & 0x01;  // minus
            buttons[1] = b2 & 0x02;  // plus
            buttons[2] = b2 & 0x04;  // lstick
            buttons[3] = b2 & 0x08;  // rstick
            buttons[4] = b2 & 0x10;  // home
            buttons[5] = b2 & 0x20;  // capture
            buttons[6] = b2 & 0x40;  // 0
            buttons[7] = b2 & 0x80;  // 0
            buttons[8] = b1 & 0x01;  // B
            buttons[9] = b1 & 0x02;  // A
            buttons[10] = b1 & 0x04; // Y
            buttons[11] = b1 & 0x08; // X
            buttons[12] = b1 & 0x10; // L
            buttons[13] = b1 & 0x20; // R
            buttons[14] = b1 & 0x40; // ZL
            buttons[15] = b1 & 0x80; // ZR

            auto leds = b1;
            if (leds != previous_leds)
            {
                // printf("check %02x %02x\n", leds, previous_leds);
                previous_leds = leds;
                u8 cmd[13];
                frame send_pkt(cmd, sizeof(cmd));
                send_pkt.write_u8(0xa2);
                send_pkt.write_u8(0x01);
                send_pkt.write_u8(0x00);

                send_pkt.write_u8(0x00);
                send_pkt.write_u8(0x01);
                send_pkt.write_u8(0x40);
                send_pkt.write_u8(0x40);

                send_pkt.write_u8(0x00);
                send_pkt.write_u8(0x01);
                send_pkt.write_u8(0x40);
                send_pkt.write_u8(0x40);

                send_pkt.write_u8(0x30);
                send_pkt.write_u8(leds);

                hid_interrupt.send(block(cmd, send_pkt.size));
                printf("sent %02x\n", leds);
            }

            printf("%s\n", buttons.to_string().c_str());
        }
        else
        {
        }
    }

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
