#include "crypto/Hash.h"
#include <cassert>
#include <iostream>

int main() {
    // TODO: Needs real tests

    std::string test_str = "Akkon";
    std::string hash_out = crypto::Hash::sha256(test_str);
    
    std::string hash_out2 = crypto::Hash::sha256(test_str);
    assert(hash_out == hash_out2);
    
    std::string test_str3 = "AkkonServer";
    std::string hash_out3 = crypto::Hash::sha256(test_str3);
    assert(hash_out != hash_out3);
    
    std::cout << "Hashing tests passed!\n";
    return 0;
}
