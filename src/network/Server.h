#pragma once
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "../Defines.h"

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

class ThreadPool {
public:
    explicit ThreadPool(size_t threads) : m_stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->m_queue_mutex);
                        this->m_cv.wait(lock, [this] {
                            return this->m_stop || !this->m_tasks.empty();
                        });
                        if (this->m_stop && this->m_tasks.empty()) {
                            return;
                        }
                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_stop) return;
            m_tasks.emplace(std::move(task));
        }
        m_cv.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    bool m_stop;
};

enum class ActionType {
    CLOSE_CLIENT,
    ENABLE_POLLING
};

struct ClientAction {
    socket_t fd;
    ActionType type;
};

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

    void closeClient(socket_t fd);
    void enablePolling(socket_t fd);

private:
    uint16_t m_port;
    socket_t m_server_fd = (socket_t)-1;
    bool m_running = false;
    DataHandler m_handler;
    std::vector<pollfd> m_fds;

    ThreadPool m_thread_pool{akkon::THREAD_POOL_SIZE};
    std::vector<ClientAction> m_pending_actions;
    std::mutex m_action_mutex;
};

} // namespace network
