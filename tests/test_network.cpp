#include "network/Server.h"
#include <thread>
#include <iostream>
#include <cassert>

int main() {
    network::Server server(8081);
    
    std::thread server_thread([&server]() {
        server.start();
    });
    
    // Just a basic test to see if it binds and starts successfully
    std::this_thread::sleep_for(std::chrono::seconds(1));
    server.stop();
    server_thread.join();
    
    std::cout << "Network server test passed!\n";
    return 0;
}
