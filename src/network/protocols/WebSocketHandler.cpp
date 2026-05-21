#include "WebSocketHandler.h"
#include <algorithm>
#include <cctype>
#include <vector>
#include <cstdint>

namespace network {

namespace {

std::string base64_encode(const unsigned char* src, size_t len) {
    std::string out;
    int val = 0, valb = -6;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = src[i];
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

void sha1(const std::string& msg, unsigned char hash[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    std::vector<uint8_t> block(msg.begin(), msg.end());
    uint64_t bit_len = block.size() * 8;
    block.push_back(0x80);
    while ((block.size() % 64) != 56) {
        block.push_back(0x00);
    }
    for (int i = 7; i >= 0; i--) {
        block.push_back((bit_len >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < block.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (block[chunk + i * 4] << 24) |
                   (block[chunk + i * 4 + 1] << 16) |
                   (block[chunk + i * 4 + 2] << 8) |
                   (block[chunk + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (val << 1) | (val >> 31);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    hash[0] = (h0 >> 24) & 0xFF; hash[1] = (h0 >> 16) & 0xFF; hash[2] = (h0 >> 8) & 0xFF; hash[3] = h0 & 0xFF;
    hash[4] = (h1 >> 24) & 0xFF; hash[5] = (h1 >> 16) & 0xFF; hash[6] = (h1 >> 8) & 0xFF; hash[7] = h1 & 0xFF;
    hash[8] = (h2 >> 24) & 0xFF; hash[9] = (h2 >> 16) & 0xFF; hash[10] = (h2 >> 8) & 0xFF; hash[11] = h2 & 0xFF;
    hash[12] = (h3 >> 24) & 0xFF; hash[13] = (h3 >> 16) & 0xFF; hash[14] = (h3 >> 8) & 0xFF; hash[15] = h3 & 0xFF;
    hash[16] = (h4 >> 24) & 0xFF; hash[17] = (h4 >> 16) & 0xFF; hash[18] = (h4 >> 8) & 0xFF; hash[19] = h4 & 0xFF;
}

} // namespace

bool WebSocketHandler::isWebSocketUpgrade(const std::string& request) {
    std::string req_lower = request;
    std::transform(req_lower.begin(), req_lower.end(), req_lower.begin(), ::tolower);
    return req_lower.find("upgrade: websocket") != std::string::npos;
}

std::string WebSocketHandler::generateHandshakeResponse(const std::string& request) {
    std::string req_lower = request;
    std::transform(req_lower.begin(), req_lower.end(), req_lower.begin(), ::tolower);

    std::string key_header = "sec-websocket-key: ";
    auto pos_lower = req_lower.find(key_header);
    if (pos_lower == std::string::npos) return "";
    
    auto pos = pos_lower + key_header.length();
    auto end_pos = request.find("\r\n", pos);
    std::string key = request.substr(pos, end_pos - pos);
    
    // Trim spaces
    while (!key.empty() && std::isspace(key.front())) key.erase(0, 1);
    while (!key.empty() && std::isspace(key.back())) key.pop_back();

    // Mathematically calculate accept key according to RFC 6455
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char sha1_hash[20];
    sha1(key + magic, sha1_hash);
    std::string accept_key = base64_encode(sha1_hash, 20);
    
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";
    return response;
}

std::string WebSocketHandler::encodeFrame(const std::string& message) {
    std::string frame;
    frame.push_back((char)0x81); // FIN + Text
    if (message.size() <= 125) {
        frame.push_back((char)message.size());
    } else {
        frame.push_back((char)126);
        frame.push_back((char)((message.size() >> 8) & 0xFF));
        frame.push_back((char)(message.size() & 0xFF));
    }
    frame += message;
    return frame;
}

std::string WebSocketHandler::decodeFrame(const std::string& frame) {
    if (frame.size() < 2) return "";
    uint8_t payload_len = frame[1] & 0x7F;
    size_t offset = 2;
    if (payload_len == 126) {
        offset = 4;
    } else if (payload_len == 127) {
        offset = 10;
    }
    
    bool masked = (frame[1] & 0x80) != 0;
    std::string mask = masked ? frame.substr(offset, 4) : "";
    if (masked) offset += 4;
    
    if (offset > frame.size()) return "";
    std::string decoded = frame.substr(offset);
    if (masked) {
        for (size_t i = 0; i < decoded.size(); ++i) {
            decoded[i] ^= mask[i % 4];
        }
    }
    return decoded;
}

std::vector<std::string> WebSocketHandler::decodeFrames(const std::string& buffer) {
    std::vector<std::string> payloads;
    size_t bytes_left = buffer.size();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
    size_t index = 0;

    while (bytes_left >= 2) {
        uint8_t first_byte = data[index];
        uint8_t second_byte = data[index + 1];
        bool fin = (first_byte & 0x80) != 0;
        uint8_t opcode = first_byte & 0x0F;
        
        bool masked = (second_byte & 0x80) != 0;
        uint64_t payload_len = second_byte & 0x7F;
        
        size_t header_len = 2;
        if (payload_len == 126) {
            header_len = 4;
        } else if (payload_len == 127) {
            header_len = 10;
        }
        
        if (masked) {
            header_len += 4;
        }
        
        if (bytes_left < header_len) {
            break; // Incomplete header
        }
        
        size_t len_offset = 2;
        if (payload_len == 126) {
            payload_len = (data[index + 2] << 8) | data[index + 3];
            len_offset = 4;
        } else if (payload_len == 127) {
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | data[index + 2 + i];
            }
            len_offset = 10;
        }
        
        uint8_t mask[4] = {0};
        if (masked) {
            mask[0] = data[index + len_offset];
            mask[1] = data[index + len_offset + 1];
            mask[2] = data[index + len_offset + 2];
            mask[3] = data[index + len_offset + 3];
        }
        
        if (bytes_left < header_len + payload_len) {
            break; // Incomplete frame payload
        }
        
        std::string payload(payload_len, '\0');
        const uint8_t* payload_src = data + index + header_len;
        for (size_t i = 0; i < payload_len; ++i) {
            if (masked) {
                payload[i] = static_cast<char>(payload_src[i] ^ mask[i % 4]);
            } else {
                payload[i] = static_cast<char>(payload_src[i]);
            }
        }
        
        if (opcode == 1 || opcode == 2) {
            payloads.push_back(std::move(payload));
        }
        
        size_t frame_size = header_len + payload_len;
        index += frame_size;
        bytes_left -= frame_size;
    }
    return payloads;
}

} // namespace network
