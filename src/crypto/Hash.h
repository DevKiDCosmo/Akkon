#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace crypto {

class Hash {
public:
    static std::string sha256(const std::string& input);
    static std::string sha256(const std::vector<uint8_t>& input);
    
private:
    static void transform(const uint8_t* message, uint32_t block_nb, uint32_t* h);
};

} // namespace crypto
