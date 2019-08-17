#include "fiber.h"
#include "common.h"

#include "bt_adapter.h"
#include "bt_command.h"
#include "bt_channel.h"
#include "bt_device.h"

#include "sdp.h"

#include <bitset>
#include <chrono>
#include <thread>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <fstream>
#include <sstream>

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

void csr_set_bdaddr(const bdaddr_t &addr)
{
    hci.reset();

    printf("reset\n");

    block rsp;

    u8 set_addr_pkt[] = {0xc2,
                         0x02, 0x00, 0x0c, 0x00, 0x11, 0x47, 0x03, 0x70,
                         0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    set_addr_pkt[17] = addr.b[2];
    set_addr_pkt[19] = addr.b[0];
    set_addr_pkt[20] = addr.b[1];
    set_addr_pkt[21] = addr.b[3];
    set_addr_pkt[23] = addr.b[4];
    set_addr_pkt[24] = addr.b[5];

    bt::command set_addr(hci, OGF_VENDOR_CMD, 0x00);
    set_addr.write(set_addr_pkt);
    set_addr.send();

    unsigned char reset_pkt[] = {0xc2,
                                 0x02, 0x00, 0x09, 0x00,
                                 0x00, 0x00, 0x01, 0x40, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    bt::command reset(hci, OGF_VENDOR_CMD, 0x00);
    reset.write(reset_pkt);
    reset.send();
}

void intel_set_bdaddr(const bdaddr_t &addr)
{
    hci.reset();

    printf("reset\n");

    block rsp;

    bt::command set_addr(hci, OGF_VENDOR_CMD, 0x31);
    set_addr.write(addr);
    set_addr.send();

    hci.reset();
}

void debug_pro()
{
    hci.start_inquiry(0x9e8b33, 0x30, 255);

    while (true)
    {
        block pkt;
        hci.inquiry_result.next(&pkt);

        pkt.read_u8();
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
            /* auto hat = */ input.read_u8();
            /* auto s1 = */ input.read_u16();
            /* auto s2 = */ input.read_u16();
            /* auto s3 = */ input.read_u16();
            /* auto s4 = */ input.read_u16();

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

            printf("%s\n", buttons.to_string().c_str());
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

    report_x30() : report_x30(0, 0, 0x800, 0x800, 0x800, 0x800) {}

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

static condition manual_cv;
static std::pair<usize, report_x30> manual(0, report_x30());
static bool run_script = false;
static bool manual_input = false;

void read_console()
{
    char *line;
    usize len;

    while (getline(&line, &len, stdin) != -1)
    {
        std::stringstream src(line);

        std::string btn;
        usize delay;

        if (!(src >> btn))
            continue;

        if (btn == "run")
        {
            run_script = true;
        }
        else
        {
            if (!(src >> delay))
                continue;

            if (btn == "up")
                manual.second.b3 |= 0x02;
            else if (btn == "down")
                manual.second.b3 |= 0x01;
            else if (btn == "left")
                manual.second.b3 |= 0x08;
            else if (btn == "right")
                manual.second.b3 |= 0x04;
            else if (btn == "a")
                manual.second.b1 |= 0x08;
            else if (btn == "b")
                manual.second.b1 |= 0x04;
            else if (btn == "x")
                manual.second.b1 |= 0x02;
            else if (btn == "y")
                manual.second.b1 |= 0x01;
            else if (btn == "minus")
                manual.second.b2 |= 0x01;
            else if (btn == "plus")
                manual.second.b2 |= 0x02;
            else if (btn == "home")
                manual.second.b2 |= 0x10;
            else if (btn == "capture")
                manual.second.b2 |= 0x20;
            else if (btn == "l")
                manual.second.b3 |= 0x40;
            else if (btn == "r")
                manual.second.b1 |= 0x40;

            manual.first = delay;
            manual_input = true;
            fiber::input([] { manual_cv.notify(); });
        }

        fiber::input([] { manual_cv.notify(); });
    }
}

template <typename T>
bool next_bit(T &src)
{
    char c;
    while (src >> c)
    {
        if (c == '0')
            return false;
        if (c == '1')
            return true;
    }
    error("invalid bit");
    return false;
}

template <typename T>
bool parse(T &src, report_x30 *out, usize *time)
{
    new (out) report_x30();

    std::string test;

    while (true)
    {
        if (!(src >> test))
            return false;

        if (test == "//")
        {
            std::getline(src, test);
        }
        else
            break;
    }

    *time = std::stoi(test, 0, 16);

    out->b3 |= next_bit(src) << 1; // up
    out->b3 |= next_bit(src) << 0; // down
    out->b3 |= next_bit(src) << 3; // left
    out->b3 |= next_bit(src) << 2; // right

    out->b1 |= next_bit(src) << 3; // a
    out->b1 |= next_bit(src) << 2; // b
    out->b1 |= next_bit(src) << 1; // x
    out->b1 |= next_bit(src) << 0; // y

    out->b3 |= next_bit(src) << 7; // l
    out->b1 |= next_bit(src) << 7; // r
    out->b1 |= next_bit(src) << 8; // zl
    out->b3 |= next_bit(src) << 8; // zr

    out->b2 |= next_bit(src) << 3; // lstick
    out->b2 |= next_bit(src) << 2; // rstick

    out->b2 |= next_bit(src) << 0; // minus
    out->b2 |= next_bit(src) << 1; // plus
    out->b2 |= next_bit(src) << 4; // home
    out->b2 |= next_bit(src) << 5; // capture

    return true;
}

void fake_pro()
{
    bt::device console(hci, switch_addr);
    bt::channel hid_control(console);
    bt::channel hid_interrupt(console);

    if (true)
    {
        console.connect(0xcc18, 0x02, 0x00, 0x00);

        printf("waiting for connect\n");
        console.connected.wait();

        console.authenticate();
        printf("waiting for authenticate\n");
        console.authenticated.wait();

        console.encrypt(0x01);
        printf("waiting for encrypt\n");
        console.encrypted.wait();

        console.connect(hid_control, 0x11);
        console.connect(hid_interrupt, 0x13);
    }
    else
    {
        hci.set_scan_mode(0x03);

        console.accept(0x00);
        console.connected.wait();

        hci.set_scan_mode(0x02);

        console.accept(hid_control, 0x11);
        console.accept(hid_interrupt, 0x13);
    }

    hid_control.configure();
    hid_interrupt.configure();

    printf("done\n");

    hci.set_scan_mode(0x00);
    // hci.set_page_scan_timing(0x800, 0x012);
    // hci.set_inquiry_scan_timing(0x800, 0x012);

    console.qos_setup(0x00, 0x02, 100 * 60, 100 * 60, 1250, 1250);

    u8 report_mode = 0x3f;
    std::unordered_map<u32, u8> SPI;

    fiber::create("input-loop", [&report_mode, &hid_interrupt] {
        auto input_mode = &report_mode;
        auto c = &hid_interrupt;

        usize counter = 0;
        std::deque<std::pair<usize, report_x30>> inputs;

        while (true)
        {
            if (++counter == 0x80)
                counter = 0;

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
                c->send(block(cmd, send_pkt.size));
                fiber::delay(250);
                continue;
            }
            else if (inputs.size() != 0)
            {
                auto &pair = inputs.front();
                auto report = pair.second;
                report.timer = counter;
                send_pkt.write(report);

                c->send(block(cmd, send_pkt.size));
                printf("send %d\n", pair.first);
                fiber::delay(pair.first);
                inputs.pop_front();
            }
            else
            {
                manual_cv.wait();
                if (run_script)
                {
                    run_script = false;

                    usize delay;
                    report_x30 report;

                    std::ifstream src;
                    src.open("inputs.txt");

                    while (parse(src, &report, &delay))
                        inputs.emplace_back(delay, report);

                    printf("got %d inputs\n", inputs.size());
                }
                else if (manual_input)
                {
                    manual_input = false;
                    inputs.emplace_back(manual);
                    manual.second.b1 = manual.second.b2 = manual.second.b3 = 0;
                    inputs.emplace_back(std::make_pair(50, report_x30()));
                }
            }
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
            /*auto rumbleTiming = */ pkt.read_u8();
            /*auto lRumble =      */ pkt.read_u32();
            /*auto rRumble =      */ pkt.read_u32();
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
            // auto lRumble = pkt.read_u32();
            // auto rRumble = pkt.read_u32();
        }
    }
}

void print_block(const block &pkt)
{
    const u8 *data = pkt.data;
    const u8 *end = data + pkt.size;

    while (data < end)
    {
        for (int j = 0; j < 8 && data < end; ++j, ++data)
            printf("%02x ", *data);
        printf(" ");
        for (int j = 0; j < 8 && data < end; ++j, ++data)
            printf("%02x ", *data);
        printf("\n");
    }
}

void inspect_pro()
{
    hci.start_inquiry(0x9e8b33, 0x30, 255);

    while (true)
    {
        block pkt;
        hci.inquiry_result.next(&pkt);

        pkt.read_u8();
        auto info = pkt.advance<extended_inquiry_info>();

        char tmp[50];
        ba2str(&info->bdaddr, tmp);
        printf("%s\n", tmp);

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

    block pkt;
    hid_interrupt.data.next(&pkt);

    u8 send_data[0x100];
    frame send(send_data, sizeof(send_data));

    send.write_u8(0xa2);           // HID OUTPUT
    send.write_u8(0x01);           // OUTPUT 0x01
    send.write_u8(0x00);           // rumble timing
    send.write_u8(0x00, 8);        // rumble data
    send.write_u8(0x11);           // WRITE SPI
    send.write_u32(htobl(0x6050)); // addr
    send.write_u8(3 * 2);          // maximum size = 0x1d

    // default:       0x323232 0xffffff
    // purple gray:   0x5e35b1 0xbdbdbd
    // purple yellow: 0x5e35b1 0xefef09

    send.write_u8(0x5e);
    send.write_u8(0x35);
    send.write_u8(0xb1);

    send.write_u8(0xef);
    send.write_u8(0xef);
    send.write_u8(0x09);

    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d

    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d

    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d

    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d
    // send.write_u8(0xFF); // maximum size = 0x1d

    hid_interrupt.send(block(send_data, send.size));
    while (true)
        hid_interrupt.data.next(&pkt);
    return;

    u8 pairing_info_buffer[0x1000];
    frame pairing_info(pairing_info_buffer, sizeof(pairing_info_buffer));

    for (int i = 0x2000; i < 0x3000; i += 0x1d)
    {
        usize count = std::min(0x1d, 0x3000 - i);

        u8 buffer[1024];

        frame send(buffer, sizeof(buffer));
        send.write_u8(0xa2);      // HID OUTPUT
        send.write_u8(0x01);      // OUTPUT 0x01
        send.write_u8(0x00);      // rumble timing
        send.write_u8(0x00, 8);   // rumble data
        send.write_u8(0x10);      // READ SPI
        send.write_u32(htobl(i)); // addr
        send.write_u8(count);     // maximum size = 0x1d

        hid_interrupt.send(block(buffer, send.size));

        do
            hid_interrupt.data.next(&pkt);
        while (pkt.data[1] != 0x21);

        u8 count_recv = pkt.data[20];
        printf("%zx %x\n", count, count_recv);

        pairing_info.write(pkt.data + 21, count);
    }

    int fd = open("tmp", O_WRONLY | O_TRUNC | O_CREAT);
    if (fd < 0)
    {
        perror("failed to save link key");
        return;
    }

    write(fd, pairing_info_buffer, pairing_info.size);
    close(fd);
}

void pipe(bt::channel &src, bt::channel &dst)
{
    while (true)
    {
        block pkt;
        src.data.next(&pkt);

        if (pkt.size == 0)
            break;

        dst.send(pkt);
    }
}

void proxy_pro()
{
    // hci.start_inquiry(0x9e8b33, 0x30, 255);

    // while (true)
    // {
    //     block pkt;
    //     hci.inquiry_result.next(&pkt);

    //     auto count = pkt.read_u8();
    //     auto info = pkt.advance<extended_inquiry_info>();
    //     if (info->bdaddr != pro_addr)
    //         continue;

    //     hci.stop_inquiry();
    //     break;
    // }

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

    // pro.qos_setup(0x00, 0x02, 100 * 60, 100 * 60, 1250, 1250);

    bt::channel pro_ctrl(pro);
    bt::channel pro_data(pro);

    pro.connect(pro_ctrl, 0x11);
    pro.connect(pro_data, 0x13);

    pro_ctrl.configure();
    pro_data.configure();

    printf("acquired pro controller\n");

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

    bt::channel console_ctrl(console);
    bt::channel console_data(console);

    console.connect(console_ctrl, 0x11);
    console.connect(console_data, 0x13);

    console_ctrl.configure();
    console_data.configure();

    // hci.set_scan_mode(0x00);

    console.qos_setup(0x00, 0x02, 100 * 60, 100 * 60, 1250, 1250);

    printf("acquired console\n");

    fiber::create("pro -> console", [&] {
        pipe(pro_data, console_data);
    });

    pipe(console_data, pro_data);
}

u8 thing_checksum(const block &data)
{
    u8 sum = 0;

    for (usize i = 0; i < data.size; ++i)
        sum += data.data[i];

    return (-sum) & 0xFF;
}

void dump_mem(bt::channel &hid, u32 addr, u32 size, const char *out)
{
    int romFd = open(out, O_WRONLY | O_TRUNC | O_CREAT);
    if (romFd < 0)
    {
        perror("failed to open tmp");
        return;
    }

    u32 block_size = size;
    while (block_size >= 0xf0)
        block_size >>= 1;

    printf("dump @ 0x%x 0x%x to '%s' block %x\n", addr, size, out, block_size);

    u8 hid_buffer[1024];

    frame hid_send(hid_buffer, sizeof(hid_buffer));
    hid_send.write_u8(0x53);                      // HID SET_REPORT FEATURE
    hid_send.write_u8(0x71);                      // SETUP_READ
    hid_send.write_u32(htole32(addr));            // addr
    hid_send.write_u16(htole16((u16)block_size)); // len
    hid_send.write_u8(thing_checksum(block(hid_buffer + 1, 7)));
    hid.send(block(hid_buffer, hid_send.size));
    block recv;
    hid.data.next(&recv);

    for (usize i = 0; i < size;)
    {
        hid_send = frame(hid_buffer, sizeof(hid_buffer));
        hid_send.write_u8(0x43); // HID GET_REPORT FEATURE
        hid_send.write_u8(0x72); // READ
        // no checksum?

        hid.send(block(hid_buffer, hid_send.size));
        hid.data.next(&recv);

        u8 hidp = recv.read_u8();
        u8 cksm = thing_checksum(block(recv.data, 7 + block_size));

        u8 opcode = recv.read_u8();
        u32 rsp_addr = le32toh(recv.read_u32());
        u16 rsp_len = le16toh(recv.read_u16());

        const u8 *rsp_data = recv.data;
        recv.skip(rsp_len);
        u8 rsp_cksm = recv.read_u8();

        assert(hidp == 0xa3);
        assert(rsp_addr == addr + i);
        assert(rsp_len == block_size);

        if (cksm == rsp_cksm)
        {
            printf("%zx\n", i);
            write(romFd, rsp_data, rsp_len);
            i += block_size;
        }
        else
        {
            printf("%zx CHECKSUM FAIL\n", i);
            hid_send = frame(hid_buffer, sizeof(hid_buffer));
            hid_send.write_u8(0x53);                      // HID SET_REPORT FEATURE
            hid_send.write_u8(0x71);                      // SETUP_READ
            hid_send.write_u32(htole32(addr + i));        // addr
            hid_send.write_u16(htole16((u16)block_size)); // len
            hid_send.write_u8(thing_checksum(block(hid_buffer + 1, 7)));
            hid.send(block(hid_buffer, hid_send.size));
            hid.data.next(&recv);
        }
    }

    close(romFd);

    printf("dump complete\n");
}

void dump_pro()
{
    hci.start_inquiry(0x9e8b33, 0x30, 255);

    while (true)
    {
        block pkt;
        hci.inquiry_result.next(&pkt);

        pkt.read_u8();
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

    block pkt;
    hid_interrupt.data.next(&pkt);

    printf("dumping...\n");

    // dump_mem(hid_control, 0x00000000, 0xC8000, "rom1");
    // dump_mem(hid_control, 0x00260000, 0x0C000, "rom2");
    dump_mem(hid_control, 0xF8000000, 0x80000, "spi1");

    return;

    int fd = open("tmp", O_WRONLY | O_TRUNC | O_CREAT);
    if (fd < 0)
    {
        perror("failed to open tmp");
        return;
    }

    usize mem_idx = 0;
    usize mem_end = 0x80000;

    while (mem_idx < mem_end)
    {
        usize count = std::min((usize)0x1d, mem_end - mem_idx);

        u8 buffer[1024];

        frame send(buffer, sizeof(buffer));
        send.write_u8(0xa2);            // HID OUTPUT
        send.write_u8(0x01);            // OUTPUT 0x01
        send.write_u8(0x00);            // rumble timing
        send.write_u8(0x00, 8);         // rumble data
        send.write_u8(0x10);            // READ SPI
        send.write_u32(htobl(mem_idx)); // addr
        send.write_u8(count);           // maximum size = 0x1d

        hid_interrupt.send(block(buffer, send.size));

        do
            hid_interrupt.data.next(&pkt);
        while (pkt.data[1] != 0x21);

        u8 count_recv = pkt.data[20];
        printf("0x%zx 0x%zx 0x%x\n", mem_idx, count, count_recv);

        write(fd, pkt.data + 21, count);

        mem_idx += count;
    }

    close(fd);
}

void sdp_host(bt::device &device)
{
    sdp::service pnp;
    {
        pnp.set_attr(SDP_ATTR_RECORD_HANDLE, sdp::uint32(0x00010001));

        sdp::sequence pnp_class_id_list(sdp::uuid16(PNP_INFO_SVCLASS_ID));

        pnp.set_attr(SDP_ATTR_SVCLASS_ID_LIST, pnp_class_id_list);

        sdp::sequence psm1proto(sdp::uuid16(L2CAP_UUID), sdp::uint16(0x0001));
        sdp::sequence sdpproto(sdp::uuid16(SDP_UUID));
        sdp::sequence pnp_protocols(psm1proto, sdpproto);

        pnp.set_attr(SDP_ATTR_PROTO_DESC_LIST, pnp_protocols);

        sdp::sequence pnp_desc(sdp::uuid16(PNP_INFO_PROFILE_ID), sdp::uint16(0x0100));

        pnp.set_attr(SDP_ATTR_PFILE_DESC_LIST, sdp::sequence(0, pnp_desc));

        pnp.set_attr(SDP_ATTR_SVCNAME_PRIMARY, sdp::text("Wireless Gamepad PnP Server"));
        pnp.set_attr(SDP_ATTR_SVCDESC_PRIMARY, sdp::text("Gamepad"));
        pnp.set_attr(SDP_ATTR_SPECIFICATION_ID, sdp::uint16(0x0103));
        pnp.set_attr(SDP_ATTR_VENDOR_ID, sdp::uint16(0x057e));
        pnp.set_attr(SDP_ATTR_PRODUCT_ID, sdp::uint16(0x2009));
        pnp.set_attr(SDP_ATTR_VERSION, sdp::uint16(0x0001));
        pnp.set_attr(SDP_ATTR_PRIMARY_RECORD, sdp::boolean(true));
        pnp.set_attr(SDP_ATTR_VENDOR_ID_SOURCE, sdp::uint16(0x0002));
    }

    sdp::service hid;
    {
        const u8 descriptor[] = {0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x06, 0x01, 0xff, 0x85, 0x21, 0x09, 0x21, 0x75, 0x08, 0x95, 0x30, 0x81, 0x02, 0x85, 0x30, 0x09, 0x30, 0x75, 0x08, 0x95, 0x30, 0x81, 0x02, 0x85, 0x31, 0x09, 0x31, 0x75, 0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x32, 0x09, 0x32, 0x75, 0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x33, 0x09, 0x33, 0x75, 0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x3f, 0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02, 0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x05, 0x09, 0x75, 0x04, 0x95, 0x01, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34, 0x16, 0x00, 0x00, 0x27, 0xff, 0xff, 0x00, 0x00, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02, 0x06, 0x01, 0xff, 0x85, 0x01, 0x09, 0x01, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0x85, 0x10, 0x09, 0x10, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0x85, 0x11, 0x09, 0x11, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0x85, 0x12, 0x09, 0x12, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0xc0};
        hid.set_attr(SDP_ATTR_RECORD_HANDLE, sdp::uint32(0x00010000));

        sdp::sequence hid_class_id_list(sdp::uuid16(HID_SVCLASS_ID));

        hid.set_attr(SDP_ATTR_SVCLASS_ID_LIST, hid_class_id_list);

        sdp::sequence psm17proto(sdp::uuid16(L2CAP_UUID), sdp::uint16(0x0011));
        sdp::sequence psm19proto(sdp::uuid16(L2CAP_UUID), sdp::uint16(0x0013));
        sdp::sequence hidproto(sdp::uuid16(HIDP_UUID));

        hid.set_attr(SDP_ATTR_PROTO_DESC_LIST, sdp::sequence(psm17proto, hidproto));
        hid.set_attr(SDP_ATTR_BROWSE_GRP_LIST, sdp::sequence(sdp::uuid16(PUBLIC_BROWSE_GROUP)));

        sdp::sequence hid_desc(sdp::uuid16(HID_PROFILE_ID), sdp::uint16(0x0101));

        hid.set_attr(SDP_ATTR_PFILE_DESC_LIST, sdp::sequence(0, hid_desc));
        hid.set_attr(SDP_ATTR_PROTO_DESC_LIST, sdp::sequence(psm19proto, hidproto));

        hid.set_attr(SDP_ATTR_SVCNAME_PRIMARY, sdp::text("Wireless Gamepad"));
        hid.set_attr(SDP_ATTR_SVCDESC_PRIMARY, sdp::text("Gamepad"));
        hid.set_attr(SDP_ATTR_PROVNAME_PRIMARY, sdp::text("Nintendo"));

        hid.set_attr(SDP_ATTR_HID_PARSER_VERSION, sdp::uint16(0x0111));
        hid.set_attr(SDP_ATTR_HID_DEVICE_SUBCLASS, sdp::uint8(0x08));
        hid.set_attr(SDP_ATTR_HID_COUNTRY_CODE, sdp::uint8(33));
        hid.set_attr(SDP_ATTR_HID_VIRTUAL_CABLE, sdp::boolean(true));
        hid.set_attr(SDP_ATTR_HID_RECONNECT_INITIATE, sdp::boolean(true));

        sdp::sequence report_descriptor(sdp::uint8(0x22), sdp::text(block(descriptor, sizeof(descriptor))));
        hid.set_attr(SDP_ATTR_HID_DESCRIPTOR_LIST, sdp::sequence(0, report_descriptor));

        // hid.set_attr(SDP_ATTR_HID_LANG_ID_BASE_LIST, sdp::uint16(0x0111));
        hid.set_attr(SDP_ATTR_HID_BATTERY_POWER, sdp::boolean(true));
        hid.set_attr(SDP_ATTR_HID_REMOTE_WAKEUP, sdp::boolean(true));
        hid.set_attr(SDP_ATTR_HID_SUPERVISION_TIMEOUT, sdp::uint16(3200));
        hid.set_attr(SDP_ATTR_HID_NORMALLY_CONNECTABLE, sdp::boolean(false));
        hid.set_attr(SDP_ATTR_HID_BOOT_DEVICE, sdp::boolean(false));
    }

    while (true)
    {
        printf("waiting for sdp connection\n");

        bt::channel sdp(device);
        device.accept(sdp, 0x01);
        sdp.configure();

        printf("got sdp connection\n");
        while (true)
        {
            block pkt;
            sdp.data.next(&pkt);

            if (pkt.size == 0)
                break;

            auto hdr = pkt.advance<sdp_pdu_hdr_t>();
            if (!hdr)
                continue;

            u8 buffer[1024];
            frame res(buffer, sizeof(buffer));
            auto res_hdr = res.advance<sdp_pdu_hdr_t>();
            res_hdr->tid = hdr->tid;

            if (hdr->pdu_id == SDP_SVC_SEARCH_ATTR_REQ)
            {
            }
            else if (hdr->pdu_id == SDP_SVC_SEARCH_REQ)
            {
                auto ty = pkt.read_u8();
                usize search_size;
                if (ty == SDP_SEQ8)
                    search_size = pkt.read_u8();
                else if (ty == SDP_SEQ16)
                    search_size = ntohs(pkt.read_u16());
                else if (ty == SDP_SEQ32)
                    search_size = ntohl(pkt.read_u8());

                auto p_start = res.size;

                res.write_u16(htons(0x0001));
                auto count = res.advance_u16();
                *count = 0;

                auto search_end = pkt.data + search_size;
                while (pkt.data < search_end)
                {
                    pkt.read_u8();
                    auto svc_id = ntohs(pkt.read_u16());
                    if (svc_id == HID_SVCLASS_ID)
                    {
                        (*count)++;
                        res.write_u32(htonl(0x00010000));
                    }
                    else if (svc_id == PNP_INFO_SVCLASS_ID)
                    {
                        (*count)++;
                        res.write_u32(htonl(0x00010001));
                    }
                }

                *count = htons(*count);
                res.write_u8(0);

                res_hdr->pdu_id = SDP_SVC_SEARCH_RSP;
                res_hdr->plen = htons(res.size - p_start);
            }
            else if (hdr->pdu_id == SDP_SVC_ATTR_REQ)
            {
                res_hdr->pdu_id = SDP_SVC_ATTR_RSP;
                auto p_start = res.size;

                auto record = ntohl(pkt.read_u32());
                if (record == 0x00010000)
                {
                    auto size = res.advance<u16>();
                    auto start = res.size;
                    hid.get_attrs(res);
                    *size = htons(res.size - start);
                    res.write_u8(0x00);
                }
                else if (record == 0x00010001)
                {
                    auto size = res.advance<u16>();
                    auto start = res.size;
                    pnp.get_attrs(res);
                    *size = htons(res.size - start);
                    res.write_u8(0x00);
                }

                res_hdr->plen = htons(res.size - p_start);
            }

            sdp.send(block(buffer, res.size));
        }

        printf("finished sdp connection\n");
    }
}

void pair_console()
{
    hci.set_scan_mode(0x03);

    bt::device console(hci, switch_addr);

    console.accept(0x00);
    console.connected.wait();

    hci.set_scan_mode(0x02);

    fiber::create("SDP", [&] { sdp_host(console); });

    printf("connected to console\n");

    bt::channel hid_control(console);
    bt::channel hid_interrupt(console);

    console.accept(hid_control, 0x11);
    console.accept(hid_interrupt, 0x13);

    printf("ready console\n");

    task().wait();
    // console.disconnect(0x11);
}

void configure_adapter()
{
    printf("start configure adapter\n");

    u8 event_mask[] = {0xFF, 0xFF, 0xFb, 0xFF, 0x07, 0xF8, 0xbf, 0x3d};
    block rsp;

    hci.reset();

    hci.set_event_mask(event_mask);
    hci.clear_event_filter();

    hci.set_default_link_policy(0x07);

    hci.set_scan_mode(0x02);
    // hci.set_page_scan_timing(0x1000, 0x400);
    hci.set_page_scan_timing(0x800, 0x200);
    // hci.set_inquiry_scan_timing(0x1000, 0x400);
    hci.set_inquiry_scan_timing(0x800, 0x200);

    hci.set_inquiry_mode(0x02);

    hci.set_pin_type(0x00);
    // hci.set_simple_pairing_mode(0x01);

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
    // str2ba("B8:27:EB:37:24:FF", &self); // black rpi
    // str2ba("00:DB:DF:5E:03:43", &self); // laptop

    str2ba("dc:68:eb:3b:f8:46", &pro_addr); // mine
    // str2ba("04:03:D6:1A:7C:12", &pro_addr); // grant's

    // str2ba("b8:8a:ec:91:17:c2", &switch_addr); // my switch
    // str2ba("04:03:D6:25:A5:37", &switch_addr); // grant's

    // fiber::create("reset", [] { intel_set_bdaddr(self); });
    fiber::create("main", [] {
        configure_adapter();
        fake_pro();
    });

    std::thread(read_console).detach();

    while (true)
    {
        fiber::run();
    }
}
