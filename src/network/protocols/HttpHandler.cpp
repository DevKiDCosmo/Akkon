#include "HttpHandler.h"
#include <sstream>

namespace network {

bool HttpHandler::isHttpRequest(const std::string& request) {
    return request.starts_with("GET ") || request.starts_with("POST ");
}

std::string HttpHandler::handleRequest(const std::string& request, std::function<std::string(const std::string&)> router) {
    std::string method, path, version;
    std::istringstream iss(request);
    iss >> method >> path >> version;

    std::string response_body = router(path);
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html\r\n";
    response += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += response_body;

    return response;
}

} // namespace network
