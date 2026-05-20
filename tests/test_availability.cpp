#include "network/Server.h"
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define close_socket close
#endif

void client_thread_func() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    network::socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock != (network::socket_t)-1);
    
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8082);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed \n";
        return;
    }
    
    std::string msg = "GET / HTTP/1.1\r\n\r\n";
    send(sock, msg.c_str(), msg.length(), 0);
    
    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);
    assert(std::string(buffer).find("HTTP/1.1 200 OK") != std::string::npos);
    close_socket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}

int main() {
    network::Server server(8082);
    server.setDataHandler([&server](auto client_fd, const std::string& request) {
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        server.sendData(client_fd, response);
    });
    
    std::thread server_thread([&server]() {
        server.start();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::vector<std::thread> clients;
    for (int i = 0; i < 50; ++i) {
        clients.emplace_back(client_thread_func);
    }
    
    for (auto& t : clients) {
        t.join();
    }
    
    server.stop();
    server_thread.join();
    
    std::cout << "Availability tests passed!\n";
    return 0;
}
