#include "WebSocketHandler.h"

namespace network {

bool WebSocketHandler::isWebSocketUpgrade(const std::string& request) {
    return request.find("Upgrade: websocket") != std::string::npos;
}

std::string WebSocketHandler::generateHandshakeResponse(const std::string& request) {
    std::string key_header = "Sec-WebSocket-Key: ";
    auto pos = request.find(key_header);
    if (pos == std::string::npos) return "";
    
    pos += key_header.length();
    auto end_pos = request.find("\r\n", pos);
    std::string key = request.substr(pos, end_pos - pos);
    
    // Hardcoded mock accept key for now to avoid SHA1 dependency bloat
    std::string accept_key = "mocked_accept_key=";
    
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

} // namespace network
