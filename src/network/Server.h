#pragma once
#include <string>
#include <functional>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#endif

namespace network {

#ifdef _WIN32
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

class Server {
public:
    Server(uint16_t port);
    ~Server();

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    void disconnectAll();

    using DataHandler = std::function<void(socket_t, const std::string&)>;
    void setDataHandler(DataHandler handler);

    bool sendData(socket_t client_fd, const std::string& data);

private:
    uint16_t m_port;
    socket_t m_server_fd = (socket_t)-1;
    bool m_running = false;
    DataHandler m_handler;
    std::vector<pollfd> m_fds;
};

} // namespace network
