#include <cgjre/bytecode_scan.h>
#include <string.h>

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | p[3];
}

static int64_t signed32(uint32_t value)
{
    return value <= 0x7fffffffu ? value : (int64_t)value - 0x100000000ll;
}

int cgjre_bytecode_scan(const uint8_t *code, size_t length,
    uint32_t counts[256])
{
    size_t pc = 0;
    if(!code || !counts) return -1;
    memset(counts, 0, 256 * sizeof(*counts));
    while(pc < length) {
        uint8_t op = code[pc];
        size_t width = 1;
        ++counts[op];
        if(op == 0xaa || op == 0xab) {
            size_t cursor = (pc + 4u) & ~(size_t)3u;
            int64_t count;
            if(cursor > length || length - cursor < (op == 0xaa ? 12u : 8u))
                return -1;
            if(op == 0xaa) {
                int64_t low = signed32(be32(code + cursor + 4));
                int64_t high = signed32(be32(code + cursor + 8));
                count = high - low + 1;
                cursor += 12;
                if(count < 0 || (uint64_t)count > (length - cursor) / 4u)
                    return -1;
                cursor += (size_t)count * 4u;
            }
            else {
                count = signed32(be32(code + cursor + 4));
                cursor += 8;
                if(count < 0 || (uint64_t)count > (length - cursor) / 8u)
                    return -1;
                int64_t previous = INT64_MIN;
                for(int64_t i = 0; i < count; ++i) {
                    int64_t key = signed32(be32(code + cursor + (size_t)i * 8u));
                    if(key <= previous) return -1;
                    previous = key;
                }
                cursor += (size_t)count * 8u;
            }
            pc = cursor;
            continue;
        }
        if(op == 0xc4) {
            if(pc + 1 >= length) return -1;
            uint8_t nested = code[pc + 1];
            if(nested == 0x84) width = 6;
            else if((nested >= 0x15 && nested <= 0x19) ||
                    (nested >= 0x36 && nested <= 0x3a) || nested == 0xa9)
                width = 4;
            else return -1;
        }
        else if(op == 0x10 || op == 0x12 ||
                (op >= 0x15 && op <= 0x19) ||
                (op >= 0x36 && op <= 0x3a) || op == 0xa9 || op == 0xbc)
            width = 2;
        else if(op == 0x11 || op == 0x13 || op == 0x14 || op == 0x84 ||
                (op >= 0x99 && op <= 0xa8) ||
                (op >= 0xb2 && op <= 0xb8) || op == 0xbb ||
                op == 0xbd || op == 0xc0 || op == 0xc1 ||
                op == 0xc6 || op == 0xc7)
            width = 3;
        else if(op == 0xc5) width = 4;
        else if(op == 0xb9 || op == 0xba || op == 0xc8 || op == 0xc9)
            width = 5;
        if(width > length - pc) return -1;
        pc += width;
    }
    return 0;
}
