#ifndef SDP_H
#define SDP_H

#include "fiber.h"
#include "common.h"

#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

namespace sdp
{

template <typename T>
void write(frame &pkt, u8 type, const T &arg)
{
    pkt.write_u8(type);
    pkt.write(arg);
}

void write_long(frame &pkt, u8 unspec, const void *src, u32 size);

struct data
{
    virtual usize size() const = 0;
    virtual void write(frame &pkt) const = 0;
};

struct nil : public data
{
    virtual usize size() const override { return 1; }
    virtual void write(frame &pkt) const override { pkt.write_u8(0); }
};

struct uint8 : public data
{
    u8 value;
    uint8(u8 value) : value(value) {}

    virtual usize size() const override { return 2; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UINT8, value); }
};

struct uint16 : public data
{
    u16 value;
    uint16(u16 value) : value(value) {}

    virtual usize size() const override { return 3; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UINT16, htons(value)); }
};

struct uint32 : public data
{
    u32 value;
    uint32(u32 value) : value(value) {}

    virtual usize size() const override { return 5; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UINT32, htonl(value)); }
};

struct uint64 : public data
{
    u64 value;
    uint64(u64 value) : value(value) {}

    virtual usize size() const override { return 9; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UINT64, hton64(value)); }
};

struct uint128 : public data
{
    u128 value;
    uint128(u128 value) : value(value) {}

    virtual usize size() const override { return 17; }
    virtual void write(frame &pkt) const override
    {
        u128 be;
        hton128(&value, &be);
        sdp::write(pkt, SDP_UINT128, be);
    }
};

struct int8 : public data
{
    i8 value;
    int8(i8 value) : value(value) {}

    virtual usize size() const override { return 2; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_INT8, value); }
};

struct int16 : public data
{
    i16 value;
    int16(i16 value) : value(value) {}

    virtual usize size() const override { return 3; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_INT16, htons(value)); }
};

struct int32 : public data
{
    i32 value;
    int32(i32 value) : value(value) {}

    virtual usize size() const override { return 5; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_INT32, htonl(value)); }
};

struct int64 : public data
{
    i64 value;
    int64(i64 value) : value(value) {}

    virtual usize size() const override { return 9; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_INT64, hton64(value)); }
};

struct uuid16 : public data
{
    u16 value;
    uuid16(u16 value) : value(value) {}

    virtual usize size() const override { return 3; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UUID16, htons(value)); }
};

struct uuid32 : public data
{
    u32 value;
    uuid32(u32 value) : value(value) {}

    virtual usize size() const override { return 5; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_UUID32, htonl(value)); }
};

struct uuid128 : public data
{
    u128 value;
    uuid128(u128 value) : value(value) {}

    virtual usize size() const override { return 17; }
    virtual void write(frame &pkt) const override
    {
        u128 be;
        hton128(&value, &be);
        sdp::write(pkt, SDP_UUID128, be);
    }
};

struct longdata : public data
{
protected:
    usize size_long(u32 size) const
    {
        if (size < (1 << 8))
            return size + 2;
        else if (size < (1 << 16))
            return size + 3;
        else
            return size + 5;
    }

    void write_long(frame &pkt, u8 unspec, const void *src, u32 size) const
    {
        if (size < (1 << 8))
            sdp::write(pkt, unspec | 0x05, (u8)size);
        else if (size < (1 << 16))
            sdp::write(pkt, unspec | 0x06, htons((u16)size));
        else
            sdp::write(pkt, unspec | 0x07, htonl((u32)size));

        pkt.write(src, size);
    }
};

struct text : public longdata
{
    block value;
    text(const char *value) : value((const u8 *)value, strlen(value)) {}
    text(const block &value) : value(value) {}

    virtual usize size() const override { return size_long(value.size); }
    virtual void write(frame &pkt) const override
    {
        write_long(pkt, SDP_TEXT_STR_UNSPEC, value.data, value.size);
    }
};

struct url : public text
{
    virtual void write(frame &pkt) const override
    {
        write_long(pkt, SDP_TEXT_STR_UNSPEC, value.data, value.size);
    }
};

struct boolean : public data
{
    bool value;
    boolean(bool value) : value(value) {}

    virtual usize size() const override { return 2; }
    virtual void write(frame &pkt) const override { sdp::write(pkt, SDP_BOOL, value); }
};

struct sequence : public longdata
{
    sequence() : data(256) {}

    sequence(const sequence &copy) = delete;

    template <typename... TArgs>
    sequence(int, const TArgs &... args)
    {
        std::vector<const sdp::data *> vec{&args...};

        // printf("ctor %zu\n", vec.size());

        for (auto d : vec)
            add(*d);
    }

    template <typename... TArgs>
    sequence(const TArgs &... args)
    {
        std::vector<const sdp::data *> vec{&args...};

        // printf("ctor %zu\n", vec.size());

        for (auto d : vec)
            add(*d);
    }

    virtual usize size() const override { return size_long(index); }
    virtual void write(frame &pkt) const override
    {
        // printf("write sequence %02x %zu %zu\n", pkt.data, index, size_long(index));
        write_long(pkt, SDP_SEQ_UNSPEC, &data[0], index);
    }

    void raw(const block pkt)
    {
        data.resize(index + pkt.size);
        memcpy(&data[index], pkt.data, pkt.size);
        index += pkt.size;
    }

    void add(const data &value)
    {
        data.resize(index + value.size());
        frame dst(&data[index], value.size());
        // printf("add %02x %zu\n", &data[index], value.size());
        value.write(dst);
        index += value.size();
    }

protected:
    std::vector<u8> data;
    usize index = 0;
};

struct alternative : public sequence
{
    virtual void write(frame &pkt) const override
    {
        write_long(pkt, SDP_ALT_UNSPEC, &data[0], index);
    }
};

struct service
{
public:
    void get_attrs(frame &out)
    {
        sequence sq;

        for (const auto &pair : attributes)
        {
            sq.add(uint16(pair.first));
            sq.raw(block(pair.second.data(), pair.second.size()));
        }

        sq.write(out);
    }

    void set_attr(u16 key, const data &value)
    {
        auto i = attributes.try_emplace(key, value.size());
        frame out(&i.first->second[0], i.first->second.size());
        // printf("attr %02x %zu\n", &i.first->second[0], value.size());
        value.write(out);
    }

private:
    std::unordered_map<u16, std::vector<u8>> attributes;
};

}; // namespace sdp

#endif
