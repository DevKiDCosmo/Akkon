#pragma once
#include <string>

namespace network {

class AkkonProtocolHandler {
public:
    static bool isAkkonProtocol(const std::string& request);
    static std::string handleRequest(const std::string& request);
};

} // namespace network
