#include "md5.h"
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <vector>
using namespace std;

namespace {

uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

uint32_t rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void encode(uint8_t* output, const uint32_t* input, size_t len) {
    for (size_t i = 0, j = 0; j < len; ++i, j += 4) {
        output[j] = input[i] & 0xff;
        output[j+1] = (input[i] >> 8) & 0xff;
        output[j+2] = (input[i] >> 16) & 0xff;
        output[j+3] = (input[i] >> 24) & 0xff;
    }
}

std::string to_hex_string(const uint8_t* digest) {
    std::ostringstream os;
    for (int i = 0; i < 16; ++i)
        os << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return os.str();
}

} // anonymous namespace

std::string md5(const std::string& input) {
    uint32_t a0 = 0x67452301;
    uint32_t b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe;
    uint32_t d0 = 0x10325476;

    static const uint32_t k[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
        0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
        0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
        0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
        0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
        0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };

    static const uint32_t r[] = {
         7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
         5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
         4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
         6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };

    // Pad message
    size_t new_len = input.size() + 1;
    while (new_len % 64 != 56)
        ++new_len;

    vector<uint8_t> msg(new_len + 8, 0);
    memcpy(msg.data(), input.c_str(), input.size());
    msg[input.size()] = 0x80;

    // uint64_t bits_len = input.size() * 8;
    // memcpy(&msg[new_len], &bits_len, 8);
    uint64_t bits_len = input.size() * 8;
    for (int i = 0; i < 8; ++i) {
        msg[new_len + i] = (bits_len >> (8 * i)) & 0xFF;
    }

    // Process each 512-bit chunk
    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        auto w = reinterpret_cast<uint32_t*>(msg.data() + offset); // ✅ sửa tại đây

        uint32_t A = a0, B = b0, C = c0, D = d0;

        for (int i = 0; i < 64; ++i) {
            uint32_t f, g;
            if (i < 16) { f = F(B,C,D); g = i; }
            else if (i < 32) { f = G(B,C,D); g = (5*i + 1)%16; }
            else if (i < 48) { f = H(B,C,D); g = (3*i + 5)%16; }
            else { f = I(B,C,D); g = (7*i)%16; }

            uint32_t temp = D;
            D = C;
            C = B;
            B = B + rotate_left(A + f + k[i] + w[g], r[i]);
            A = temp;
        }

        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    uint32_t result[4] = {a0, b0, c0, d0};
    uint8_t digest[16];
    encode(digest, result, 16);
    return to_hex_string(digest);
}

string md5Last8(string input) {
    string md5_full = md5(input);
    string last8 = md5_full.substr(md5_full.length() - 8);
    return last8;
}