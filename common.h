#ifndef COMMON_H
#define COMMON_H

#include <stack>
#include <queue>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

#include <bitset>
#include <tuple>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <setjmp.h>
#include <ucontext.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = uint128_t;
using usize = size_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using namespace std::placeholders;

void error(const std::string &str);
std::string to_hex(usize value, int width = 0);

template <typename T>
T *advance(u8 **data, usize *size = nullptr)
{
    if (size != nullptr)
    {
        if (*size < sizeof(T))
            return nullptr;

        *size -= sizeof(T);
    }

    auto ptr = (T *)*data;
    *data += sizeof(T);
    return ptr;
}

struct frame
{
    u8 *data;
    usize size;
    usize unused;

    frame(u8 *data, usize available)
        : data(data), size(0), unused(available) {}

    void write_u8(u8 arg) { write(arg); }
    void write_u16(u16 arg) { write(arg); }
    void write_u32(u32 arg) { write(arg); }
    void write_u64(u64 arg) { write(arg); }

    void write_i8(i8 arg) { write(arg); }
    void write_i16(i16 arg) { write(arg); }
    void write_i32(i32 arg) { write(arg); }
    void write_i64(i64 arg) { write(arg); }

    ::u8 *advance_u8() { return advance<::u8>(); }
    ::u16 *advance_u16() { return advance<::u16>(); }
    ::u32 *advance_u32() { return advance<::u32>(); }
    ::u64 *advance_u64() { return advance<::u64>(); }

    ::i8 *advance_i8() { return advance<::i8>(); }
    ::i16 *advance_i16() { return advance<::i16>(); }
    ::i32 *advance_i32() { return advance<::i32>(); }
    ::i64 *advance_i64() { return advance<::i64>(); }

    void write_u8(u8 value, usize s)
    {
        if (unused < s)
            error("frame is not large enough");

        memset(data, value, size);

        size += s;
        data += s;
        unused -= s;
    }

    void write(const void *src, usize s)
    {
        if (unused < s)
            error("frame is not large enough");

        memcpy(data, src, s);

        size += s;
        data += s;
        unused -= s;
    }

    template <typename T>
    void write(const T &arg)
    {
        write((void *)&arg, sizeof(T));
    }

    template <typename T>
    T *advance()
    {
        if (unused < sizeof(T))
            error("frame is not large enough");

        auto ptr = (T *)data;

        size += sizeof(T);
        data += sizeof(T);
        unused -= sizeof(T);

        return ptr;
    }
};

struct block
{
    const u8 *data;
    usize size;

    block() : block(nullptr, 0) {}
    block(const u8 *data, usize size) : data(data), size(size) {}

    u8 read_u8() { return read<u8>(); }
    u16 read_u16() { return read<u16>(); }
    u32 read_u32() { return read<u32>(); }
    u64 read_u64() { return read<u64>(); }

    i8 read_i8() { return read<i8>(); }
    i16 read_i16() { return read<i16>(); }
    i32 read_i32() { return read<i32>(); }
    i64 read_i64() { return read<i64>(); }

    void skip(usize count)
    {
        size -= count;
        data += count;
    }

    template <typename T>
    void read(T *out)
    {
        if (size < sizeof(T))
            return;

        *out = (T *)data;

        size -= sizeof(T);
        data += sizeof(T);
    }

    template <typename T>
    T read()
    {
        if (size < sizeof(T))
            return 0;

        auto ptr = (T *)data;

        size -= sizeof(T);
        data += sizeof(T);

        return *ptr;
    }

    template <typename T>
    T *advance()
    {
        if (size < sizeof(T))
            return nullptr;

        auto ptr = (T *)data;

        size -= sizeof(T);
        data += sizeof(T);

        return ptr;
    }
};

inline constexpr u8 operator"" _u8(unsigned long long arg) noexcept
{
    return static_cast<u8>(arg);
}
inline constexpr u16 operator"" _u16(unsigned long long arg) noexcept
{
    return static_cast<u16>(arg);
}
inline constexpr u32 operator"" _u32(unsigned long long arg) noexcept
{
    return static_cast<u32>(arg);
}
inline constexpr u64 operator"" _u64(unsigned long long arg) noexcept
{
    return static_cast<u64>(arg);
}
inline constexpr usize operator"" _usize(unsigned long long arg) noexcept
{
    return static_cast<usize>(arg);
}

inline constexpr i8 operator"" _i8(unsigned long long arg) noexcept
{
    return static_cast<i8>(arg);
}
inline constexpr i16 operator"" _i16(unsigned long long arg) noexcept
{
    return static_cast<i16>(arg);
}
inline constexpr i32 operator"" _i32(unsigned long long arg) noexcept
{
    return static_cast<i32>(arg);
}
inline constexpr i64 operator"" _i64(unsigned long long arg) noexcept
{
    return static_cast<i64>(arg);
}

namespace std
{

template <>
struct hash<bdaddr_t>
{
    size_t operator()(const bdaddr_t &addr) const
    {
        return (size_t)addr.b[0] |
               ((size_t)addr.b[1] << 8) |
               ((size_t)addr.b[2] << 16) |
               ((size_t)addr.b[3] << 24) |
               ((size_t)addr.b[4] << 32) |
               ((size_t)addr.b[5] << 48);
    }
};

template <>
struct equal_to<bdaddr_t>
{
    bool operator()(const bdaddr_t &lhs, const bdaddr_t &rhs) const
    {
        return memcmp(lhs.b, rhs.b, 6) == 0;
    }
};

}; // namespace std

bool operator==(const bdaddr_t &b1, const bdaddr_t &b2);
bool operator!=(const bdaddr_t &b1, const bdaddr_t &b2);

#endif
