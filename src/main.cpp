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

static std::atomic<size_t> g_queries_lifetime_init{0};
static std::atomic<size_t> g_queries_since_reboot{0};


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

std::vector<std::string> extractJsonArray(const std::string& json, const std::string& field) {
    std::vector<std::string> items;
    auto field_pos = json.find("\"" + field + "\"");
    if (field_pos == std::string::npos) return items;
    
    auto start_bracket = json.find("[", field_pos);
    if (start_bracket == std::string::npos) return items;
    
    auto end_bracket = json.find("]", start_bracket);
    if (end_bracket == std::string::npos) return items;
    
    std::string array_content = json.substr(start_bracket + 1, end_bracket - start_bracket - 1);
    std::stringstream ss(array_content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t start = item.find('"');
        if (start != std::string::npos) {
            size_t end = item.find('"', start + 1);
            if (end != std::string::npos) {
                items.push_back(item.substr(start + 1, end - start - 1));
            }
        }
    }
    return items;
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
    g_queries_lifetime_init.store(master.getLastTotalQueries());
    db::DataStorage data_storage;
    auto existing_dbs = master.loadExisting();
    data_storage.initialize(existing_dbs);

    auto runtime_domain = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
    if (runtime_domain) {
        runtime_domain->allocate(existing_dbs.size() * 512 * 1024);
    }

    indexer::QueryEngine query_engine(1000000);
    data_storage.loadAllIdentifiers(query_engine);

    int port = 2409;
    for (int i = 1; i < argc; ++i) {
        if ((std::string(argv[i]) == "--port" || std::string(argv[i]) == "-p") && i + 1 < argc) {
            try {
                port = std::stoi(argv[i + 1]);
            } catch (...) {
                // Ignore parsing errors and default to 2409
            }
        }
    }

    network::Server server(port);
    
    std::set<network::socket_t> ws_clients;
    std::recursive_mutex ws_mutex;

    auto escapeJsonString = [](const std::string& input) {
        std::string output;
        for (char c : input) {
            if (c == '"') output += "\\\"";
            else if (c == '\\') output += "\\\\";
            else if (c == '\n') output += "\\n";
            else if (c == '\r') output += "\\r";
            else if (c == '\t') output += "\\t";
            else output += c;
        }
        return output;
    };

    static thread_local bool in_log_callback = false;
    monitor::SystemMonitor::setLogCallback([&server, &ws_clients, &ws_mutex, escapeJsonString](monitor::LogLevel level, const std::string& formatted_log) {
        if (in_log_callback) return;
        in_log_callback = true;
        
        std::string escaped_log = escapeJsonString(formatted_log);
        std::string payload = "{\"action\":\"log\", \"level\":" + std::to_string(static_cast<int>(level)) + 
                              ", \"message\":\"" + escaped_log + "\"}";
        std::string frame = network::WebSocketHandler::encodeFrame(payload);
        
        std::lock_guard<std::recursive_mutex> lock(ws_mutex);
        for (auto it = ws_clients.begin(); it != ws_clients.end(); ) {
            if (!server.sendData(*it, frame)) {
                server.closeClient(*it);
                it = ws_clients.erase(it);
            } else {
                ++it;
            }
        }
        
        in_log_callback = false;
    });

    std::atomic<bool> server_finished{false};
    std::thread broadcast_thread([&server, &ws_clients, &ws_mutex, &mem_manager, &master, &server_finished]() {
        while (!server.isRunning() && !server_finished.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        while (server.isRunning() && !server_finished.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            auto runtime = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
            auto req_domain = mem_manager.getDomain(memory::RuntimeDomain::REQUEST);
            
            size_t ram_runtime = (runtime ? runtime->allocated() : 0);
            size_t ram_request = (req_domain ? req_domain->allocated() : 0);
            size_t ram_allocated = ram_runtime + ram_request;
            size_t ram_capacity = (runtime ? runtime->capacity() : 0) + (req_domain ? req_domain->capacity() : 0);

            auto health = monitor::HealthMonitor::checkHealth(ram_allocated, ram_capacity);

            // Get current timestamp for metrics db
            auto ftime = std::chrono::system_clock::now();
            std::time_t cftime = std::chrono::system_clock::to_time_t(ftime);
            std::tm tm{};
#ifndef _WIN32
            localtime_r(&cftime, &tm);
#else
            localtime_s(&tm, &cftime);
#endif
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            std::string timestamp = ss.str();

            size_t current_total = g_queries_lifetime_init.load() + g_queries_since_reboot.load();
            // Store metrics in database
            master.insertMetrics(timestamp, ram_allocated, ram_runtime, ram_request, ram_capacity, health.available_disk_bytes, health.reliability_score, current_total);

            std::string payload = "{\"action\":\"stats\", \"reliability\":" + std::to_string(health.reliability_score) + 
                                  ", \"disk_space\":" + std::to_string(health.available_disk_bytes) + 
                                  ", \"ram_allocated\":" + std::to_string(ram_allocated) + 
                                  ", \"ram_runtime\":" + std::to_string(ram_runtime) + 
                                  ", \"ram_request\":" + std::to_string(ram_request) + 
                                  ", \"ram_capacity\":" + std::to_string(ram_capacity) + 
                                  ", \"total_queries\":" + std::to_string(current_total) + 
                                  ", \"reboot_queries\":" + std::to_string(g_queries_since_reboot.load()) + "}";
            std::string frame = network::WebSocketHandler::encodeFrame(payload);
            
            std::lock_guard<std::recursive_mutex> lock(ws_mutex);
            for (auto it = ws_clients.begin(); it != ws_clients.end(); ) {
                if (!server.sendData(*it, frame)) {
                    server.closeClient(*it);
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
        static std::atomic<uint32_t> s_tracker_counter{0};
        uint32_t current_id = s_tracker_counter.fetch_add(1) % 0x1000000;
        char tracker_buf[16];
        snprintf(tracker_buf, sizeof(tracker_buf), "TRK_%06X", current_id);
        std::string tracker_id(tracker_buf);

        if (monitor::SystemMonitor::isDebugMode()) {
            monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Received request (FD: " + std::to_string(client_fd) + ", len=" + std::to_string(request.length()) + "):\n" + request.substr(0, 1000), tracker_id);
        }
        if (security::LifecycleManager::getLockdownState() != security::LockdownState::NONE) {
            if (network::HttpHandler::isHttpRequest(request) && !network::WebSocketHandler::isWebSocketUpgrade(request)) {
                server.sendData(client_fd, "HTTP/1.1 503 Service Unavailable\r\n\r\nLOCKDOWN IN EFFECT. ALL REQUESTS BLOCKED.");
            }
            server.closeClient(client_fd);
            return;
        }

        bool is_ws_client = false;
        {
            std::lock_guard<std::recursive_mutex> lock(ws_mutex);
            is_ws_client = (ws_clients.find(client_fd) != ws_clients.end());
        }

        if (is_ws_client) {
            auto payloads = network::WebSocketHandler::decodeFrames(request);
            for (const auto& payload : payloads) {
                std::string action = extractJsonString(payload, "action");
                if (action == "exists_batch") {
                    std::vector<std::string> ids = extractJsonArray(payload, "identifiers");
                    std::string batch_id = extractJsonString(payload, "batch_id");
                    
                    std::string results_json = "{";
                    for (size_t i = 0; i < ids.size(); ++i) {
                        const auto& id = ids[i];
                        if (id.length() == 64) {
                            g_queries_since_reboot++;
                            indexer::QueryResult qres = query_engine.query(id);
                            std::string res_str = (qres == indexer::QueryResult::PROBABLY_YES) ? "PROBABLY_YES" : 
                                                  (qres == indexer::QueryResult::DEFINITELY_YES ? "DEFINITELY_YES" : "DEFINITELY_NO");
                            results_json += "\"" + id + "\":\"" + res_str + "\"";
                            if (i + 1 < ids.size()) {
                                results_json += ",";
                            }
                        }
                    }
                    results_json += "}";

                    std::string response_payload = "{\"action\":\"exists_batch_response\", \"batch_id\":\"" + batch_id + "\", \"results\":" + results_json + "}";
                    std::string response_frame = network::WebSocketHandler::encodeFrame(response_payload);
                    server.sendData(client_fd, response_frame);
                }
            }
            server.enablePolling(client_fd);
            return;
        }

        auto req_domain = mem_manager.getDomain(memory::RuntimeDomain::REQUEST);
        if (req_domain) {
            size_t alloc_size = ((request.length() + 63) / 64) * 64;
            req_domain->allocate(alloc_size);
        }

        if (network::HttpHandler::isHttpRequest(request) || request.starts_with("OPTIONS ")) {
            if (request.starts_with("OPTIONS ")) {
                std::string res = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
                server.sendData(client_fd, res);
                server.closeClient(client_fd);
                return;
            }

            std::string method, path;
            {
                std::istringstream iss(request);
                iss >> method >> path;
            }
            auto q_pos = path.find('?');
            std::string clean_path = (q_pos == std::string::npos) ? path : path.substr(0, q_pos);

            auto is_hex_str = [](const std::string& s, size_t start) {
                for (size_t i = start; i < s.length(); ++i) {
                    char c = s[i];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        return false;
                    }
                }
                return true;
            };

            bool is_short_insert = (method == "POST" && clean_path.starts_with("/api/") && clean_path.length() == 133 && is_hex_str(clean_path, 5));
            bool is_short_exists = (method == "GET" && clean_path.starts_with("/api/") && clean_path.length() == 69 && is_hex_str(clean_path, 5));

            if (request.find("POST /api/v1/insert") != std::string::npos || request.find("POST /api/v1/create") != std::string::npos || is_short_insert) {
                g_queries_since_reboot++;
                std::string identifier;
                std::string pwk;
                if (is_short_insert) {
                    identifier = clean_path.substr(5, 64);
                    pwk = clean_path.substr(69, 64);
                } else {
                    identifier = extractJsonString(request, "identifier");
                    pwk = extractJsonString(request, "pwk");
                }
                
                if (identifier.length() == 64 && pwk.length() == 64) {
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
                    }

                    if (low_space) {
                        std::string body = "{\"error\":\"insufficient storage\"}";
                        std::string res = "HTTP/1.1 507 Insufficient Storage\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                        server.sendData(client_fd, res);
                        server.closeClient(client_fd);
                        return;
                    }

                    if (query_engine.query(identifier) == indexer::QueryResult::DEFINITELY_YES) {
                        std::string body = "{\"error\":\"duplicate identifier\"}";
                        std::string res = "HTTP/1.1 409 Conflict\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                        server.sendData(client_fd, res);
                        server.closeClient(client_fd);
                        return;
                    }

                    query_engine.insert(identifier);
                    db::DbEntity entity;
                    entity.identifyer = identifier;
                    entity.pwk = pwk;
                    entity.uuid = crypto::Hash::sha256(identifier + pwk);
                    data_storage.insertEntity(entity, tracker_id);

                    auto runtime_domain = mem_manager.getDomain(memory::RuntimeDomain::RUNTIME);
                    if (runtime_domain) {
                        runtime_domain->allocate(256);
                    }
                    
                    std::string body = "{\"status\":\"success\"}";
                    std::string res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                    if (monitor::SystemMonitor::isDebugMode()) {
                        monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "API Inserted identifier: " + identifier, tracker_id);
                    }
                } else {
                    std::string body = "{\"error\":\"invalid fields or size limits (must be 64 char hashes)\"}";
                    std::string res = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                }
                server.closeClient(client_fd);
                return;
            }
            if (request.find("GET /api/v1/exists") != std::string::npos || is_short_exists) {
                g_queries_since_reboot++;
                std::string identifier;
                if (is_short_exists) {
                    identifier = clean_path.substr(5, 64);
                } else {
                    identifier = extractQueryString(request, "identifier");
                }
                if (identifier.length() == 64) {
                    indexer::QueryResult res = query_engine.query(identifier);
                    std::string res_str = (res == indexer::QueryResult::PROBABLY_YES) ? "PROBABLY_YES" : (res == indexer::QueryResult::DEFINITELY_YES ? "DEFINITELY_YES" : "DEFINITELY_NO");
                    std::string body = "{\"result\":\"" + res_str + "\"}";
                    std::string http_res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, http_res);
                    if (monitor::SystemMonitor::isDebugMode()) {
                        monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "API Queried identifier: " + identifier + " -> " + res_str, tracker_id);
                    }
                } else {
                    std::string body = "{\"error\":\"invalid identifier size (must be 64 char hash)\"}";
                    std::string res = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    server.sendData(client_fd, res);
                }
                server.closeClient(client_fd);
                return;
            }

            if (network::WebSocketHandler::isWebSocketUpgrade(request)) {
                std::string ws_response = network::WebSocketHandler::generateHandshakeResponse(request);
                server.sendData(client_fd, ws_response);
                {
                    std::lock_guard<std::recursive_mutex> lock(ws_mutex);
                    ws_clients.insert(client_fd);
                }
                if (monitor::SystemMonitor::isDebugMode()) {
                    monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "WebSocket connection upgraded (FD: " + std::to_string(client_fd) + ")", tracker_id);
                }
                server.enablePolling(client_fd);
            } else {
                std::string http_response = network::HttpHandler::handleRequest(request, [](const std::string& path) {
                    if (path.starts_with("/api/stats")) {
                        return std::string("{\"allocated\": 1024, \"capacity\": 1048576, \"total_queries\": 5, \"hit_rate\": 80.0}");
                    }
                    return serveStaticFile(path);
                });
                server.sendData(client_fd, http_response);
                server.closeClient(client_fd);
            }
        } else if (network::AkkonProtocolHandler::isAkkonProtocol(request)) {
            std::string response = network::AkkonProtocolHandler::handleRequest(request);
            server.sendData(client_fd, response);
            server.closeClient(client_fd);
        } else {
            if (monitor::SystemMonitor::isDebugMode()) {
                monitor::SystemMonitor::log(monitor::LogLevel::DEBUG, "Received unknown payload or WebSocket frame (ignoring as API is now REST)", tracker_id);
            }
            server.closeClient(client_fd);
        }
    });

    monitor::SystemMonitor::log(monitor::LogLevel::INFO, "Akkon Server running on port " + std::to_string(port));
    server.start();
    server_finished.store(true);

    if (broadcast_thread.joinable()) {
        broadcast_thread.join();
    }

    return 0;
}