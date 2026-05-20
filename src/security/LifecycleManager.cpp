#include "LifecycleManager.h"
#include "../monitor/SystemMonitor.h"
#include "../Version.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close
#endif

namespace security {

std::atomic<bool> LifecycleManager::s_running{false};
std::atomic<LockdownState> LifecycleManager::s_lockdown_state{LockdownState::NONE};
std::thread LifecycleManager::s_worker;

void LifecycleManager::start() {
    if (s_running) return;
    s_running = true;
    s_worker = std::thread(backgroundTask);
    monitor::SystemMonitor::log(monitor::LogLevel::INFO, "LifecycleManager started. Monitoring vulnerabilities.");
}

void LifecycleManager::stop() {
    s_running = false;
    if (s_worker.joinable()) {
        s_worker.join();
    }
}

LockdownState LifecycleManager::getLockdownState() {
    return s_lockdown_state.load();
}

void LifecycleManager::triggerLockdown(LockdownState state) {
    s_lockdown_state = state;
    monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "CRITICAL: Manual Lockdown Triggered!");
}

std::string LifecycleManager::fetchVulnerabilityStatus() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
    
    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        closesocket(sock);
        return "";
    }
    
    std::string request = "GET /status?version=" + std::string(AKKON_VERSION) + " HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Connection: close\r\n\r\n";
                          
    send(sock, request.c_str(), request.length(), 0);
    
    char buffer[4096] = {0};
    std::string response;
    while (recv(sock, buffer, 4096, 0) > 0) {
        response += buffer;
        memset(buffer, 0, 4096);
    }
    closesocket(sock);
    return response;
}

void LifecycleManager::backgroundTask() {
    while (s_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!s_running) break;
        
        std::string response = fetchVulnerabilityStatus();
        if (response.empty()) continue;
        
        std::string state_str = "SAFE";
        std::string cve = "";
        std::string msg = "";
        
        auto pos = response.find("\"status\":");
        if (pos != std::string::npos) {
            auto start = response.find("\"", pos + 9) + 1;
            auto end = response.find("\"", start);
            state_str = response.substr(start, end - start);
        }
        
        pos = response.find("\"cve\":");
        if (pos != std::string::npos) {
            auto start = response.find("\"", pos + 6) + 1;
            auto end = response.find("\"", start);
            cve = response.substr(start, end - start);
        }
        
        pos = response.find("\"message\":");
        if (pos != std::string::npos) {
            auto start = response.find("\"", pos + 10) + 1;
            auto end = response.find("\"", start);
            msg = response.substr(start, end - start);
        }
        
        LockdownState new_state = LockdownState::NONE;
        if (state_str == "IMMEDIATE") new_state = LockdownState::IMMEDIATE;
        else if (state_str == "CLOSE") new_state = LockdownState::CLOSE;
        
        if (new_state != s_lockdown_state.load()) {
            s_lockdown_state = new_state;
            if (new_state != LockdownState::NONE) {
                std::string log_msg = "VULNERABILITY DETECTED! " + cve + " - " + msg + " -> LOCKDOWN: " + state_str;
                monitor::SystemMonitor::log(monitor::LogLevel::ERROR, log_msg);
            } else {
                monitor::SystemMonitor::log(monitor::LogLevel::INFO, "System returned to SAFE state.");
            }
        }
    }
}

} // namespace security
