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

void debug_pro()
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
    int counter = 0;

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

            // u8 report[20];
            // memset(&report, 0, sizeof(report));

            // if (b1 & 0x02) // A
            //     report[0] |= 0x01;
            // if (b1 & 0x01) // B
            //     report[0] |= 0x02;
            // if (b1 & 0x08) // X
            //     report[0] |= 0x04;
            // if (b1 & 0x04) // Y
            //     report[0] |= 0x08;
            // if (b1 & 0x02) // UP
            //     report[0] |= 0x10;
            // if (b1 & 0x02) // DOWN
            //     report[0] |= 0x20;
            // if (b1 & 0x02) // LEFT
            //     report[0] |= 0x40;
            // if (b1 & 0x02) // RIGHT
            //     report[0] |= 0x80;
            // if (b1 & 0x10) // L-BUMPER
            //     report[1] |= 0x01;
            // if (b1 & 0x20) // R-BUMPER
            //     report[1] |= 0x02;
            // if (b2 & 0x04) // L-STICK
            //     report[1] |= 0x04;
            // if (b2 & 0x08) // R-STICK
            //     report[1] |= 0x08;
            // if (b2 & 0x02) // START
            //     report[1] |= 0x10;
            // if (b2 & 0x01) // BACK
            //     report[1] |= 0x20;
            // if (b2 & 0x10) // GUIDE
            //     report[1] |= 0x40;
            // if (b2 & 0x20) // CAPTURE
            //     report[1] |= 0x80;

            // frame sticks(report + 4, 16);
            // sticks.write_u16(htons(s1));
            // sticks.write_u16(htons(s2));
            // sticks.write_u16(htons(s3));
            // sticks.write_u16(htons(s4));

            // if (++counter == 0x02)
            // {
            //     sendto(sock, &report, sizeof(report), 0, (sockaddr *)&remote, sizeof(remote));
            //     counter = 0x00;
            // }

            //send(sock, &report, sizeof(report), 0);

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
                //                printf("sent %02x\n", leds);
            }

            //          printf("%s\n", buttons.to_string().c_str());
        }
        else
        {
        }
    }

    pro.disconnect(0x13);
}

struct __attribute__((packed)) report_x30
{
    u8 timer;
    u8 status = 0x90;
    u8 b1;
    u8 b2;
    u8 b3;
    u8 sl1;
    u8 sl2;
    u8 sl3;
    u8 sr1;
    u8 sr2;
    u8 sr3;
    u8 vibration = 0x00;
    u8 spatialdata[36];

    report_x30(u8 timer, u32 buttons, u16 slx, u16 sly, u16 srx, u16 sry) : timer(timer)
    {
        b1 = buttons & 0xFF;
        b2 = (buttons >> 8) & 0xFF;
        b3 = (buttons >> 16) & 0xFF;
        sl1 = slx & 0xFF;
        sl2 = ((slx >> 8) & 0x0F) | (sly & 0x0F);
        sl3 = sly >> 4;
        sr1 = srx & 0xFF;
        sr2 = ((srx >> 8) & 0x0F) | (sry & 0x0F);
        sr3 = sry >> 4;
    }
};

void fake_pro()
{
    bt::device console(hci, switch_addr);
    console.connect(0xcc18, 0x02, 0x00, 0x00);

    printf("waiting for connect\n");
    console.connected.wait();

    console.authenticate();
    printf("waiting for authenticate\n");
    console.authenticated.wait();

    console.encrypt(0x01);
    printf("waiting for encrypt\n");
    console.encrypted.wait();

    bt::channel hid_control(console);
    bt::channel hid_interrupt(console);

    console.connect(hid_control, 0x11);
    console.connect(hid_interrupt, 0x13);

    hid_control.configure();
    hid_interrupt.configure();

    printf("done\n");

    console.qos_setup(0x00, 0x02, 100 * 60, 100 * 60, 1250, 1250);

    u8 report_mode = 0x3f;
    std::unordered_map<u32, u8> SPI;

    fiber::create("input-loop", [&report_mode, &hid_interrupt] {
        report_x30 sequence[] = {
            report_x30(0x00, 0x020000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x020000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x010000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x010000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x080000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x040000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x080000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x040000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000004, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000008, 0x800, 0x800, 0x800, 0x800),
            report_x30(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800),
        };

        auto input_mode = &report_mode;
        auto c = &hid_interrupt;

        usize counter = 0;

        auto prev = std::chrono::high_resolution_clock::now();
        usize packet_counter = 0;
        while (true)
        {
            u8 cmd[50];
            frame send_pkt(cmd, sizeof(cmd));
            send_pkt.write_u8(0xa1);
            send_pkt.write_u8(*input_mode);

            if (*input_mode == 0x3f)
            {
                send_pkt.write_u8(0x00);           // b1
                send_pkt.write_u8(0x00);           // b2
                send_pkt.write_u8(0x08);           // hat
                send_pkt.write_u16(htobs(0x8000)); // lX
                send_pkt.write_u16(htobs(0x8000)); // lY
                send_pkt.write_u16(htobs(0x8000)); // rX
                send_pkt.write_u16(htobs(0x8000)); // rY
            }
            else if (*input_mode == 0x30)
            {
                usize rate = 2;

                if ((counter / rate) >= sizeof(sequence) / sizeof(report_x30))
                    counter = 0;

                if ((counter / rate) < sizeof(sequence) / sizeof(report_x30))
                {
                    // printf("send %d\n", (int)(counter / rate));
                    auto report = sequence[counter / rate];
                    report.timer = counter;
                    send_pkt.write(report);
                }
                else
                {
                    report_x30 report(0x00, 0x000000, 0x800, 0x800, 0x800, 0x800);
                    report.timer = counter;
                    send_pkt.write(report);
                }

                // if (counter == 0xFF)
                //     counter = 0;
                // else
                ++counter;
            }

            c->send(block(cmd, send_pkt.size));

            auto curr = std::chrono::high_resolution_clock::now();

            if (curr - prev >= std::chrono::seconds(1))
            {
                printf("%zu\n", packet_counter);
                packet_counter = 0;
                prev = curr;
            }
            else
            {
                ++packet_counter;
            }

            fiber::delay(8);
        }
    });

    SPI[0x6000] = 0xFF;

    SPI[0x6050] = 0xFF;
    SPI[0x6051] = 0x00;
    SPI[0x6052] = 0x00;

    SPI[0x6053] = 0x00;
    SPI[0x6054] = 0xFF;
    SPI[0x6055] = 0x00;

    SPI[0x6056] = 0x00;
    SPI[0x6057] = 0x00;
    SPI[0x6058] = 0xFF;

    SPI[0x6059] = 0xFF;
    SPI[0x605a] = 0x00;
    SPI[0x605b] = 0xFF;

    while (true)
    {
        block pkt;
        hid_interrupt.data.next(&pkt);

        // p_data->send(pkt);

        pkt.read_u8();
        auto type = pkt.read_u8();
        if (type == 0x01)
        {
            auto rumbleTiming = pkt.read_u8();
            auto lRumble = pkt.read_u32();
            auto rRumble = pkt.read_u32();
            auto cmdId = pkt.read_u8();

            u8 reply[50];
            memset(reply, 0, sizeof(reply));
            frame reply_pkt(reply, sizeof(reply));

            reply_pkt.write_u8(0xa1);
            reply_pkt.write_u8(0x21);
            reply_pkt.write_u8(0x00);
            reply_pkt.write_u8(0x60);

            reply_pkt.write_u8(0x00);
            reply_pkt.write_u8(0x00);
            reply_pkt.write_u8(0x00);

            reply_pkt.write_u8(0x00);
            reply_pkt.write_u8(0x08);
            reply_pkt.write_u8(0x80);

            reply_pkt.write_u8(0x00);
            reply_pkt.write_u8(0x08);
            reply_pkt.write_u8(0x80);

            reply_pkt.write_u8(0x0f);

            if (cmdId == 0x02) //request device info
            {
                printf("device info\n");
                reply_pkt.write_u8(0x82);
                reply_pkt.write_u8(cmdId);
                reply_pkt.write_u8(0x03);
                reply_pkt.write_u8(0x48);
                reply_pkt.write_u8(0x03);
                reply_pkt.write_u8(0x02);
                reply_pkt.write_u8(0xdc);
                reply_pkt.write_u8(0x68);
                reply_pkt.write_u8(0xeb);
                reply_pkt.write_u8(0x3b);
                reply_pkt.write_u8(0xf8);
                reply_pkt.write_u8(0x46);
                reply_pkt.write_u8(0x03);
                reply_pkt.write_u8(0x01);
            }
            else if (cmdId == 0x03) //set report mode
            {
                report_mode = pkt.read_u8();
                printf("report mode %02x\n", report_mode);

                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x04) // unk ?? triger buttons elapsed time
            {
                printf("triger buttons elapsed time\n");
                reply_pkt.write_u8(0x83);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x08) // set lower power state
            {
                auto mode = pkt.read_u8();
                printf("low power state %02x\n", mode);
                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x10) // SPI read
            {
                auto addr = btohl(pkt.read_u32());
                auto length = pkt.read_u8();
                printf("SPI read %08x %02x\n", addr, length);

                reply_pkt.write_u8(0x90);
                reply_pkt.write_u8(cmdId);
                reply_pkt.write_u32(htobl(addr));
                reply_pkt.write_u8(length);

                for (auto i = addr; i < addr + length; ++i)
                    reply_pkt.write_u8(SPI[i]);
            }
            else if (cmdId == 0x20) // reset NFC IR MCU
            {
                printf("reset MCU\n");

                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x21) // set NFCI IR MCU config
            {
                printf("configure MCU\n");

                reply_pkt.write_u8(0xa0);
                reply_pkt.write_u8(cmdId);
                reply_pkt.write_u8(0x01);
                reply_pkt.write_u8(0x00);
                reply_pkt.write_u8(0xFF);
                reply_pkt.write_u8(0x00);
                reply_pkt.write_u8(0x03);
                reply_pkt.write_u8(0x00);
                reply_pkt.write_u8(0x05);
                reply_pkt.write_u8(0x01);
                for (int i = 0; i < 25; ++i)
                    reply_pkt.write_u8(0x00);
                reply_pkt.write_u8(0x5c);
            }
            else if (cmdId == 0x30) // set player lights
            {
                auto mode = pkt.read_u8();
                printf("player lights %s\n", std::bitset<8>{mode}.to_string().c_str());
                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x40) // set IMU enabled
            {
                auto mode = pkt.read_u8();
                printf("imu %0x\n", mode);
                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else if (cmdId == 0x48) // set vibration enabled
            {
                auto mode = pkt.read_u8();
                printf("vibration %0x\n", mode);
                reply_pkt.write_u8(0x80);
                reply_pkt.write_u8(cmdId);
            }
            else
            {
                printf("unknown subcommand: %02x\n", cmdId);
                reply_pkt.write_u8(0x00);
                reply_pkt.write_u8(cmdId);
            }

            hid_interrupt.send(block(reply, sizeof(reply)));
        }
        else if (type == 0x10)
        {
            auto lRumble = pkt.read_u32();
            auto rRumble = pkt.read_u32();
        }
    }
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
}

int main(int argc, char **argv)
{
    str2ba("00:1a:7d:da:71:12", &self);
    str2ba("dc:68:eb:3b:f8:46", &pro_addr);
    str2ba("b8:8a:ec:91:17:c2", &switch_addr);

    fiber::create("main", [] {
        configure_adapter();
        fake_pro();
    });

    while (true)
    {
        fiber::run();
    }
}
