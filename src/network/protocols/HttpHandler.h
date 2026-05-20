#pragma once
#include <string>
#include <functional>

namespace network {

class HttpHandler {
public:
    static bool isHttpRequest(const std::string& request);
    static std::string handleRequest(const std::string& request, std::function<std::string(const std::string&)> router);
};

} // namespace network
