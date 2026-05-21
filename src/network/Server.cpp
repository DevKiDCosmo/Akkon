#include "Server.h"
#include "../monitor/SystemMonitor.h"
#include "../security/LifecycleManager.h"
#include <iostream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
#define socket_poll WSAPoll
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#define close_socket close
#define socket_poll poll
#endif

namespace network {

Server::Server(uint16_t port) : m_port(port) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

Server::~Server() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::setDataHandler(DataHandler handler) {
    m_handler = std::move(handler);
}

void Server::disconnectAll() {
    for (size_t i = 1; i < m_fds.size(); ++i) { // Skip server_fd
        close_socket(m_fds[i].fd);
    }
    m_fds.resize(1); // Keep only the server socket
    monitor::SystemMonitor::log(monitor::LogLevel::ERROR, "Server forcefully disconnected all clients due to IMMEDIATE lockdown.");
}

void Server::stop() {
    m_running = false;
    if (m_server_fd != (socket_t)-1) {
        close_socket(m_server_fd);
        m_server_fd = (socket_t)-1;
    }
}

bool Server::sendData(socket_t client_fd, const std::string& data) {
    ssize_t sent = send(client_fd, data.c_str(), data.size(), 0);
    return sent >= 0;
}

void Server::start() {
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd == (socket_t)-1) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(m_server_fd, FIONBIO, &mode);
#else
    fcntl(m_server_fd, F_SETFL, O_NONBLOCK);
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << m_port << "\n";
        close_socket(m_server_fd);
        return;
    }

    if (listen(m_server_fd, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen\n";
        close_socket(m_server_fd);
        return;
    }

    m_fds.clear();
#ifdef _WIN32
    pollfd pfd = {0};
    pfd.fd = m_server_fd;
    pfd.events = POLLIN;
    m_fds.push_back(pfd);
#else
    m_fds.push_back({m_server_fd, POLLIN, 0});
#endif

    m_running = true;
    std::cout << "Server listening on port " << m_port << "\n";

    while (m_running) {
        static security::LockdownState last_state = security::LockdownState::NONE;
        security::LockdownState current_state = security::LifecycleManager::getLockdownState();
        
        if (current_state == security::LockdownState::IMMEDIATE && last_state != security::LockdownState::IMMEDIATE) {
            disconnectAll();
        }
        last_state = current_state;

        int ret = socket_poll(m_fds.data(), m_fds.size(), 100);
        if (ret < 0) break;
        if (ret == 0) continue;

        if (m_fds[0].revents & POLLIN) {
            socket_t client_fd = accept(m_server_fd, nullptr, nullptr);
            if (client_fd != (socket_t)-1) {
                if (monitor::SystemMonitor::isDebugMode()) {
                    monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "New connection accepted (FD: " + std::to_string(client_fd) + ")");
                }
#ifdef _WIN32
                u_long c_mode = 1;
                ioctlsocket(client_fd, FIONBIO, &c_mode);
                pollfd cpfd = {0};
                cpfd.fd = client_fd;
                cpfd.events = POLLIN;
                m_fds.push_back(cpfd);
#else
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                m_fds.push_back({client_fd, POLLIN, 0});
#endif
            }
        }

        for (size_t i = 1; i < m_fds.size(); ++i) {
            if (m_fds[i].revents & POLLIN) {
                char buffer[4096];
                ssize_t bytes = recv(m_fds[i].fd, buffer, sizeof(buffer), 0);
                if (bytes <= 0) {
                    if (monitor::SystemMonitor::isDebugMode()) {
                        monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Client disconnected (FD: " + std::to_string(m_fds[i].fd) + ")");
                    }
                    close_socket(m_fds[i].fd);
                    m_fds[i].fd = (socket_t)-1;
                } else {
                    if (m_handler) {
                        m_handler(m_fds[i].fd, std::string(buffer, bytes));
                    }
                }
            }
        }

        m_fds.erase(std::remove_if(m_fds.begin(), m_fds.end(), [](const pollfd& p) { return p.fd == (socket_t)-1; }), m_fds.end());
    }
}

} // namespace network
