#pragma once
#include <string>

namespace network {

class WebSocketHandler {
public:
    static bool isWebSocketUpgrade(const std::string& request);
    static std::string generateHandshakeResponse(const std::string& request);
    static std::string encodeFrame(const std::string& message);
    static std::string decodeFrame(const std::string& frame);
};

} // namespace network
