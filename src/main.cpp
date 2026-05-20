#include "memory/MemoryManager.h"
#include "network/Server.h"
#include "monitor/SystemMonitor.h"
#include "network/protocols/HttpHandler.h"
#include "network/protocols/WebSocketHandler.h"
#include "network/protocols/AkkonProtocolHandler.h"
#include "index/QueryEngine.h"
#include "security/LifecycleManager.h"
#include "shell/Shell.h"
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

std::string serveStaticFile(const std::string& path) {
    std::string filepath = "webview" + (path == "/" ? "/index.html" : path);
    std::ifstream file(filepath);
    if (!file.is_open()) return "404 Not Found";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    if (argc == 4 && std::string(argv[1]) == "--watchdog") {
        HANDLE read_pipe = (HANDLE)std::stoull(argv[2]);
        DWORD parent_pid = (DWORD)std::stoull(argv[3]);
        monitor::SystemMonitor::runWatchdog(read_pipe, parent_pid);
        return 0;
    }
#else
    // Ignore SIGPIPE to prevent crashes on closed sockets or pipes (macOS/Linux)
    signal(SIGPIPE, SIG_IGN);
#endif

    bool debug_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--debug") debug_mode = true;
    }

    if (!monitor::SystemMonitor::init(debug_mode)) {
        return 0;
    }
    
    monitor::SystemMonitor::log(monitor::LogLevel::INFO, "Akkon Server starting up");
    if (debug_mode) monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Debug mode enabled");

    security::LifecycleManager::start();

    memory::MemoryManager mem_manager;
    
    // Parse arguments and establish MasterDB source of truth
    shell::Shell cli;
    if (int rc = cli.run(argc, argv); rc != 0) {
        return rc;
    }
    
    // Check if it was just a help command
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") return 0;
    }

    indexer::QueryEngine query_engine(10000);
    network::Server server(8080);
    
    server.setDataHandler([&server, &query_engine](auto client_fd, const std::string& request) {
        if (security::LifecycleManager::getLockdownState() != security::LockdownState::NONE) {
            if (network::HttpHandler::isHttpRequest(request) && !network::WebSocketHandler::isWebSocketUpgrade(request)) {
                server.sendData(client_fd, "HTTP/1.1 503 Service Unavailable\r\n\r\nLOCKDOWN IN EFFECT. ALL REQUESTS BLOCKED.");
            }
            return;
        }

        if (network::HttpHandler::isHttpRequest(request)) {
            if (network::WebSocketHandler::isWebSocketUpgrade(request)) {
                std::string ws_response = network::WebSocketHandler::generateHandshakeResponse(request);
                server.sendData(client_fd, ws_response);
            } else {
                std::string http_response = network::HttpHandler::handleRequest(request, [](const std::string& path) {
                    if (path.starts_with("/api/stats")) {
                        return std::string("{\"allocated\": 1024, \"capacity\": 1048576, \"total_queries\": 5, \"hit_rate\": 80.0}");
                    }
                    return serveStaticFile(path);
                });
                server.sendData(client_fd, http_response);
            }
        } else if (network::AkkonProtocolHandler::isAkkonProtocol(request)) {
            std::string response = network::AkkonProtocolHandler::handleRequest(request);
            server.sendData(client_fd, response);
        } else {
            std::string decoded = network::WebSocketHandler::decodeFrame(request);
            if (!decoded.empty()) {
                std::string email = "";
                auto pos = decoded.find("\"email\":\"");
                if (pos == std::string::npos) pos = decoded.find("\"email\": \"");
                if (pos != std::string::npos) {
                    pos = decoded.find("\"", pos + 8);
                    auto end = decoded.find("\"", pos + 1);
                    if (pos != std::string::npos && end != std::string::npos) {
                        email = decoded.substr(pos + 1, end - pos - 1);
                    }
                }
                if (email.empty()) email = decoded;

                if (decoded.find("\"action\":\"insert\"") != std::string::npos || decoded.find("\"action\": \"insert\"") != std::string::npos) {
                    query_engine.insert(email);
                    server.sendData(client_fd, network::WebSocketHandler::encodeFrame("{\"action\":\"insert_ack\"}"));
                } else if (decoded.find("\"action\":\"query\"") != std::string::npos || decoded.find("\"action\": \"query\"") != std::string::npos) {
                    indexer::QueryResult res = query_engine.query(email);
                    std::string res_str = (res == indexer::QueryResult::PROBABLY_YES) ? "PROBABLY_YES" : (res == indexer::QueryResult::DEFINITELY_YES ? "DEFINITELY_YES" : "DEFINITELY_NO");
                    server.sendData(client_fd, network::WebSocketHandler::encodeFrame("{\"action\":\"query_ack\", \"result\":\"" + res_str + "\"}"));
                } else if (decoded.find("\"action\":\"debug\"") != std::string::npos || decoded.find("\"action\": \"debug\"") != std::string::npos) {
                    server.sendData(client_fd, network::WebSocketHandler::encodeFrame("{\"action\":\"debug_ack\", \"port\":\"8080\", \"interface\":\"0.0.0.0\"}"));
                } else {
                    query_engine.insert(email);
                    server.sendData(client_fd, network::WebSocketHandler::encodeFrame("{\"action\":\"insert_ack\"}"));
                }
            }
        }
    });

    monitor::SystemMonitor::log(monitor::LogLevel::INFO, "Akkon Server running on port 8080");
    server.start();

    return 0;
}