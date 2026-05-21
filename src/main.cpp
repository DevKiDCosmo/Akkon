#include "memory/MemoryManager.h"
#include "network/Server.h"
#include "monitor/SystemMonitor.h"
#include "monitor/HealthMonitor.h"
#include "network/protocols/HttpHandler.h"
#include "network/protocols/WebSocketHandler.h"
#include "network/protocols/AkkonProtocolHandler.h"
#include "index/QueryEngine.h"
#include "security/LifecycleManager.h"
#include "shell/Shell.h"
#include "construction/MasterDB.h"
#include "construction/Utils.h"
#include "crypto/Hash.h"
#include "db/DataStorage.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <set>
#include <mutex>
#include <functional>
#include <atomic>
#include <filesystem>

static std::atomic<size_t> g_total_queries{0};


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

std::string extractJsonString(const std::string& json, const std::string& field) {
    auto pos = json.find("\"" + field + "\":\"");
    if (pos == std::string::npos) pos = json.find("\"" + field + "\": \"");
    if (pos != std::string::npos) {
        pos = json.find("\"", pos + field.length() + 3);
        auto end = json.find("\"", pos + 1);
        if (pos != std::string::npos && end != std::string::npos) {
            return json.substr(pos + 1, end - pos - 1);
        }
    }
    return "";
}

std::string extractQueryString(const std::string& path, const std::string& field) {
    std::string match = field + "=";
    auto pos = path.find(match);
    if (pos != std::string::npos) {
        auto end = path.find("&", pos);
        if (end == std::string::npos) end = path.find(" ", pos);
        if (end != std::string::npos) {
            return path.substr(pos + match.length(), end - pos - match.length());
        }
    }
    return "";
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
    
    shell::Shell cli;
    if (int rc = cli.run(argc, argv); rc != 0) {
        return rc;
    }
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") return 0;
    }

    construction::MasterDB master(construction::ensureDbDirectory());
    master.ensureOpen();
    db::DataStorage data_storage;
    auto existing_dbs = master.loadExisting();
    data_storage.initialize(existing_dbs);

    auto runtime_domain = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
    if (runtime_domain) {
        runtime_domain->allocate(existing_dbs.size() * 512 * 1024);
    }

    indexer::QueryEngine query_engine(1000000);
    data_storage.loadAllIdentifiers(query_engine);
    network::Server server(2409);
    
    std::set<network::socket_t> ws_clients;
    std::mutex ws_mutex;

    std::thread broadcast_thread([&server, &ws_clients, &ws_mutex, &mem_manager]() {
        while (!server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto runtime = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
            auto req_domain = mem_manager.getDomain(memory::RuntimeDomain::REQUEST);
            
            size_t ram_allocated = (runtime ? runtime->allocated() : 0) + (req_domain ? req_domain->allocated() : 0);
            size_t ram_capacity = (runtime ? runtime->capacity() : 0) + (req_domain ? req_domain->capacity() : 0);

            auto health = monitor::HealthMonitor::checkHealth(ram_allocated, ram_capacity);

            std::string payload = "{\"action\":\"stats\", \"reliability\":" + std::to_string(health.reliability_score) + 
                                  ", \"disk_space\":" + std::to_string(health.available_disk_bytes) + 
                                  ", \"ram_allocated\":" + std::to_string(ram_allocated) + 
                                  ", \"ram_capacity\":" + std::to_string(ram_capacity) + 
                                  ", \"total_queries\":" + std::to_string(g_total_queries.load()) + "}";
            std::string frame = network::WebSocketHandler::encodeFrame(payload);
            
            std::lock_guard<std::mutex> lock(ws_mutex);
            for (auto it = ws_clients.begin(); it != ws_clients.end(); ) {
                if (!server.sendData(*it, frame)) {
                    it = ws_clients.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Reset the transient request memory domain at the end of every broadcast tick
            mem_manager.resetDomain(memory::RuntimeDomain::REQUEST);
        }
    });
    
    server.setDataHandler([&server, &query_engine, &master, &data_storage, &ws_clients, &ws_mutex, &mem_manager](auto client_fd, const std::string& request) {
        if (monitor::SystemMonitor::isDebugMode()) {
            monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Received request (FD: " + std::to_string(client_fd) + ", len=" + std::to_string(request.length()) + "):\n" + request.substr(0, 1000));
        }
        if (security::LifecycleManager::getLockdownState() != security::LockdownState::NONE) {
            if (network::HttpHandler::isHttpRequest(request) && !network::WebSocketHandler::isWebSocketUpgrade(request)) {
                server.sendData(client_fd, "HTTP/1.1 503 Service Unavailable\r\n\r\nLOCKDOWN IN EFFECT. ALL REQUESTS BLOCKED.");
            }
            return;
        }

        // Allocate transient memory in the REQUEST domain for processing the input request buffer
        auto req_domain = mem_manager.getDomain(memory::RuntimeDomain::REQUEST);
        if (req_domain) {
            size_t alloc_size = ((request.length() + 63) / 64) * 64;
            req_domain->allocate(alloc_size);
        }

        if (network::HttpHandler::isHttpRequest(request) || request.starts_with("OPTIONS ")) {
            if (request.starts_with("OPTIONS ")) {
                std::string res = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
                server.sendData(client_fd, res);
                return;
            }

            if (request.find("POST /api/v1/insert") != std::string::npos) {
                g_total_queries++;
                std::string identifier = extractJsonString(request, "identifier");
                std::string pwk = extractJsonString(request, "pwk");
                
                if (identifier.length() == 64 && pwk.length() == 64) {
                    // Check disk space guardrail (100MB limit)
                    bool low_space = false;
                    try {
                        std::filesystem::path dbDir = std::filesystem::current_path() / "db";
                        if (!std::filesystem::exists(dbDir)) {
                            dbDir = std::filesystem::current_path();
                        }
                        std::filesystem::space_info si = std::filesystem::space(dbDir);
                        if (si.available < 100LL * 1024LL * 1024LL) {
                            low_space = true;
                        }
                    } catch (...) {
                        // ignore/fallback
                    }

                    if (low_space) {
                        std::string body = "{\"error\":\"insufficient storage\"}";
                        std::string res = "HTTP/1.1 507 Insufficient Storage\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                        server.sendData(client_fd, res);
                        return;
                    }

                    if (query_engine.query(identifier) == indexer::QueryResult::DEFINITELY_YES) {
                        std::string body = "{\"error\":\"duplicate identifier\"}";
                        std::string res = "HTTP/1.1 409 Conflict\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                        server.sendData(client_fd, res);
                        return;
                    }

                    query_engine.insert(identifier);
                    db::DbEntity entity;
                    entity.identifyer = identifier;
                    entity.pwk = pwk;
                    entity.uuid = crypto::Hash::sha256(identifier + pwk);
                    data_storage.insertEntity(entity);

                    // Track memory footprint of newly added persistent record in the RUNTIME domain
                    auto runtime_domain = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
                    if (runtime_domain) {
                        runtime_domain->allocate(256);
                    }
                    
                    std::string body = "{\"status\":\"success\"}";
                    std::string res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                    if (monitor::SystemMonitor::isDebugMode()) {
                        monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "API Inserted identifier: " + identifier);
                    }
                } else {
                    std::string body = "{\"error\":\"invalid fields or size limits (must be 64 char hashes)\"}";
                    std::string res = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                }
                return;
            }
            if (request.find("GET /api/v1/exists") != std::string::npos) {
                g_total_queries++;
                std::string identifier = extractQueryString(request, "identifier");
                if (identifier.length() == 64) {
                    indexer::QueryResult res = query_engine.query(identifier);
                    std::string res_str = (res == indexer::QueryResult::PROBABLY_YES) ? "PROBABLY_YES" : (res == indexer::QueryResult::DEFINITELY_YES ? "DEFINITELY_YES" : "DEFINITELY_NO");
                    std::string body = "{\"result\":\"" + res_str + "\"}";
                    std::string http_res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, http_res);
                    if (monitor::SystemMonitor::isDebugMode()) {
                        monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "API Queried identifier: " + identifier + " -> " + res_str);
                    }
                } else {
                    std::string body = "{\"error\":\"invalid identifier size (must be 64 char hash)\"}";
                    std::string res = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                }
                return;
            }

            if (network::WebSocketHandler::isWebSocketUpgrade(request)) {
                std::string ws_response = network::WebSocketHandler::generateHandshakeResponse(request);
                server.sendData(client_fd, ws_response);
                std::lock_guard<std::mutex> lock(ws_mutex);
                ws_clients.insert(client_fd);
                if (monitor::SystemMonitor::isDebugMode()) {
                    monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "WebSocket connection upgraded (FD: " + std::to_string(client_fd) + ")");
                }
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
            if (monitor::SystemMonitor::isDebugMode()) {
                monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Received unknown payload or WebSocket frame (ignoring as API is now REST)");
            }
        }
    });

    monitor::SystemMonitor::log(monitor::LogLevel::INFO, "Akkon Server running on port 2409");
    server.start();

    if (broadcast_thread.joinable()) {
        broadcast_thread.join();
    }

    return 0;
}