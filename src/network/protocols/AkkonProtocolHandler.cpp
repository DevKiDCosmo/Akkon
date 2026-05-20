#include "AkkonProtocolHandler.h"

namespace network {

bool AkkonProtocolHandler::isAkkonProtocol(const std::string& request) {
    return request.starts_with("AKKON ");
}

std::string AkkonProtocolHandler::handleRequest(const std::string& request) {
    return "AKKON_ACK\n";
}

} // namespace network
