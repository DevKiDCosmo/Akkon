#include "Hash.h"
#include <iomanip>
#include <sstream>
#include <cstring>

namespace crypto {

// Constants for SHA-256
static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

void Hash::transform(const uint8_t* message, uint32_t block_nb, uint32_t* h) {
    uint32_t w[64];
    uint32_t wv[8];
    uint32_t t1, t2;
    const uint8_t* sub_block;
    
    for (uint32_t i = 0; i < block_nb; i++) {
        sub_block = message + (i << 6);
        for (int j = 0; j < 16; j++) {
            w[j] = (sub_block[j << 2] << 24) | (sub_block[(j << 2) + 1] << 16) | (sub_block[(j << 2) + 2] << 8) | (sub_block[(j << 2) + 3]);
        }
        for (int j = 16; j < 64; j++) {
            w[j] = SIG1(w[j - 2]) + w[j - 7] + SIG0(w[j - 15]) + w[j - 16];
        }
        for (int j = 0; j < 8; j++) {
            wv[j] = h[j];
        }
        for (int j = 0; j < 64; j++) {
            t1 = wv[7] + EP1(wv[4]) + CH(wv[4], wv[5], wv[6]) + k[j] + w[j];
            t2 = EP0(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
            wv[7] = wv[6];
            wv[6] = wv[5];
            wv[5] = wv[4];
            wv[4] = wv[3] + t1;
            wv[3] = wv[2];
            wv[2] = wv[1];
            wv[1] = wv[0];
            wv[0] = t1 + t2;
        }
        for (int j = 0; j < 8; j++) {
            h[j] += wv[j];
        }
    }
}

std::string Hash::sha256(const std::vector<uint8_t>& input) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    std::vector<uint8_t> msg = input;
    uint64_t msg_len = msg.size() * 8;
    
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>(msg_len >> (i * 8)));
    }
    
    uint32_t block_nb = msg.size() / 64;
    transform(msg.data(), block_nb, h);
    
    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << std::setw(8) << std::setfill('0') << h[i];
    }
    return ss.str();
}

std::string Hash::sha256(const std::string& input) {
    std::vector<uint8_t> vec(input.begin(), input.end());
    return sha256(vec);
}

} // namespace crypto
