#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cassert>
#include "sqlite3.h"
#include "testsuite/TestFramework.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
#define close_socket closesocket
#define INVALID_SOCKET_VAL INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define close_socket close
#define INVALID_SOCKET_VAL -1
#endif

const int TEST_PORT = 2415;

std::string sendHttpRequest(const std::string& method, const std::string& path, const std::string& body = "") {
#ifdef _WIN32
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (sock == INVALID_SOCKET_VAL) return "";
    
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TEST_PORT);
#ifdef _WIN32
    InetPtonA(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
#else
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
#endif
    
#ifdef _WIN32
    DWORD tv = 2000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close_socket(sock);
        return "";
    }
    
    std::string req = method + " " + path + " HTTP/1.1\r\nHost: localhost:" + std::to_string(TEST_PORT) + "\r\n";
    if (!body.empty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    }
    req += "Connection: close\r\n\r\n";
    if (!body.empty()) {
        req += body;
    }
    
    send(sock, req.c_str(), req.length(), 0);
    
    std::string response;
    char buffer[1024];
    int bytes;
    size_t content_length = 0;
    bool header_finished = false;
    
    while (true) {
        bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }
        buffer[bytes] = '\0';
        response.append(buffer, bytes);
        
        if (!header_finished) {
            size_t pos = response.find("\r\n\r\n");
            if (pos != std::string::npos) {
                header_finished = true;
                size_t cl_pos = response.find("Content-Length: ");
                if (cl_pos != std::string::npos && cl_pos < pos) {
                    size_t val_pos = cl_pos + 16;
                    size_t end_line = response.find("\r\n", val_pos);
                    if (end_line != std::string::npos) {
                        content_length = std::stoul(response.substr(val_pos, end_line - val_pos));
                    }
                }
                size_t body_received = response.length() - (pos + 4);
                if (body_received >= content_length) {
                    break;
                }
            }
        } else {
            size_t pos = response.find("\r\n\r\n");
            size_t body_received = response.length() - (pos + 4);
            if (body_received >= content_length) {
                break;
            }
        }
    }
    close_socket(sock);
    return response;
}

int main() {
    std::cout << "=== Akkon Database Integration Test ===" << std::endl;
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::filesystem::path original_cwd = std::filesystem::current_path();
    std::filesystem::path sandbox = original_cwd / "test_sandbox";
    if (std::filesystem::exists(sandbox)) {
        std::filesystem::remove_all(sandbox);
    }
    std::filesystem::create_directories(sandbox);
    
    // Resolve absolute path to binary before chdir
    const char* env_binary = std::getenv("AKKON_BINARY");
    std::string bin_path;
    if (env_binary && std::filesystem::exists(env_binary)) {
        bin_path = std::filesystem::absolute(env_binary).string();
    } else {
        std::vector<std::string> possible_paths = {
            "./Akkon",
            "./cmake-build-debug/Akkon",
            "../cmake-build-debug/Akkon",
            "../../cmake-build-debug/Akkon",
            "../../../cmake-build-debug/Akkon",
            "./Akkon.exe",
            "./cmake-build-debug/Akkon.exe",
            "../cmake-build-debug/Akkon.exe",
            "../../cmake-build-debug/Akkon.exe",
            "../../../cmake-build-debug/Akkon.exe"
        };
        for (const auto& path : possible_paths) {
            if (std::filesystem::exists(path)) {
                bin_path = std::filesystem::absolute(path).string();
                break;
            }
        }
    }
    
    std::cout << "Resolved Binary path: " << bin_path << std::endl;
    assert(!bin_path.empty() && "Akkon binary not found");
    
    // Set AKKON_BINARY environment variable to the absolute resolved path so TestFramework finds it
#ifdef _WIN32
    _putenv_s("AKKON_BINARY", bin_path.c_str());
#else
    setenv("AKKON_BINARY", bin_path.c_str(), 1);
#endif

    // Change current directory to sandbox
    std::filesystem::current_path(sandbox);
    
    // ----------------------------------------------------
    // RUN 1: Start server, register entity, verify DB file
    // ----------------------------------------------------
    std::cout << "\n[RUN 1] Starting Akkon server..." << std::endl;
    testsuite::TestFramework framework;
    std::vector<std::string> server_args = {"-c", "3", "--debug", "--port", std::to_string(TEST_PORT)};
    std::string tracker_id1 = framework.createInstance(server_args, "test_logs");
    assert(!tracker_id1.empty() && "Failed to create server instance Run 1");
    
    std::this_thread::sleep_for(std::chrono::seconds(1)); // wait for startup
    
    std::string test_id = "1111111111111111111111111111111111111111111111111111111111111111";
    std::string test_pwk = "2222222222222222222222222222222222222222222222222222222222222222";
    std::string insert_body = "{\"identifier\":\"" + test_id + "\",\"pwk\":\"" + test_pwk + "\"}";
    
    std::cout << "Registering identifier via API..." << std::endl;
    std::string res_insert = sendHttpRequest("POST", "/api/v1/insert", insert_body);
    std::cout << "Insert Response: " << res_insert << std::endl;
    assert(res_insert.find("200 OK") != std::string::npos && "Failed to insert");
    
    std::cout << "Verifying exists query..." << std::endl;
    std::string res_exists = sendHttpRequest("GET", "/api/v1/exists?identifier=" + test_id);
    std::cout << "Exists Response: " << res_exists << std::endl;
    assert(res_exists.find("DEFINITELY_YES") != std::string::npos && "Query should be DEFINITELY_YES");

    // Test short-form APIs (GET /api/{64b} and POST /api/{128b})
    std::string test_id_short = "3333333333333333333333333333333333333333333333333333333333333333";
    std::string test_pwk_short = "4444444444444444444444444444444444444444444444444444444444444444";
    std::cout << "Registering identifier via short-form POST API..." << std::endl;
    std::string res_insert_short = sendHttpRequest("POST", "/api/" + test_id_short + test_pwk_short);
    std::cout << "Short-form Insert Response: " << res_insert_short << std::endl;
    assert(res_insert_short.find("200 OK") != std::string::npos && "Failed to short insert");

    std::cout << "Verifying short-form exists query..." << std::endl;
    std::string res_exists_short = sendHttpRequest("GET", "/api/" + test_id_short);
    std::cout << "Short-form Exists Response: " << res_exists_short << std::endl;
    assert(res_exists_short.find("DEFINITELY_YES") != std::string::npos && "Short query should be DEFINITELY_YES");

    std::cout << "Verifying legacy exists query for short-form inserted identifier..." << std::endl;
    std::string res_exists_short_legacy = sendHttpRequest("GET", "/api/v1/exists?identifier=" + test_id_short);
    std::cout << "Legacy Exists Response: " << res_exists_short_legacy << std::endl;
    assert(res_exists_short_legacy.find("DEFINITELY_YES") != std::string::npos && "Legacy query for short insert should be DEFINITELY_YES");

    std::cout << "Verifying short-form duplicate insert rejection..." << std::endl;
    std::string res_insert_short_dup = sendHttpRequest("POST", "/api/" + test_id_short + test_pwk_short);
    std::cout << "Short-form Duplicate Insert Response: " << res_insert_short_dup << std::endl;
    assert(res_insert_short_dup.find("409 Conflict") != std::string::npos && "Duplicate short insert should return 409 Conflict");

    // Test POST /api/v1/create alias
    std::string test_id_create = "5555555555555555555555555555555555555555555555555555555555555555";
    std::string test_pwk_create = "6666666666666666666666666666666666666666666666666666666666666666";
    std::string create_body = "{\"identifier\":\"" + test_id_create + "\",\"pwk\":\"" + test_pwk_create + "\"}";
    std::cout << "Registering identifier via create alias..." << std::endl;
    std::string res_create = sendHttpRequest("POST", "/api/v1/create", create_body);
    std::cout << "Create Response: " << res_create << std::endl;
    assert(res_create.find("200 OK") != std::string::npos && "Failed to create via alias");

    std::cout << "Verifying short-form query for create alias..." << std::endl;
    std::string res_exists_create = sendHttpRequest("GET", "/api/" + test_id_create);
    std::cout << "Exists Response for Create: " << res_exists_create << std::endl;
    assert(res_exists_create.find("DEFINITELY_YES") != std::string::npos && "Query for create alias should be DEFINITELY_YES");

    std::cout << "Verifying duplicate create rejection..." << std::endl;
    std::string res_create_dup = sendHttpRequest("POST", "/api/v1/create", create_body);
    std::cout << "Duplicate Create Response: " << res_create_dup << std::endl;
    assert(res_create_dup.find("409 Conflict") != std::string::npos && "Duplicate create should return 409 Conflict");
    
    std::cout << "Stopping server (Run 1)..." << std::endl;
    framework.stopInstance(tracker_id1);
    framework.waitForInstance(tracker_id1, 5000);
    
    // Verify physical SQLite shard files
    std::cout << "Inspecting SQLite shards directly..." << std::endl;
    bool found_in_db = false;
    std::filesystem::path db_dir = std::filesystem::current_path() / "db";
    assert(std::filesystem::exists(db_dir) && "db folder does not exist");
    
    for (const auto& entry : std::filesystem::directory_iterator(db_dir)) {
        if (entry.path().extension() == ".db" && entry.path().filename() != "master.db") {
            sqlite3* db = nullptr;
            if (sqlite3_open(entry.path().string().c_str(), &db) == SQLITE_OK) {
                std::string check_sql = "SELECT count(*) FROM information WHERE identifier='" + test_id + "';";
                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        int count = sqlite3_column_int(stmt, 0);
                        if (count > 0) {
                            found_in_db = true;
                            std::cout << "  Found record in shard: " << entry.path().filename().string() << std::endl;
                        }
                    }
                    sqlite3_finalize(stmt);
                }
                sqlite3_close(db);
            }
        }
    }
    assert(found_in_db && "Entity was not physically saved in any SQLite shard");
    
    // ----------------------------------------------------
    // RUN 2: Restart server (reboot), verify load & reject
    // ----------------------------------------------------
    std::cout << "\n[RUN 2] Restarting Akkon server (reboot)..." << std::endl;
    std::string tracker_id2 = framework.createInstance(server_args, "test_logs");
    assert(!tracker_id2.empty() && "Failed to create server instance Run 2");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "Verifying exists query without re-inserting..." << std::endl;
    std::string res_exists_reboot = sendHttpRequest("GET", "/api/v1/exists?identifier=" + test_id);
    std::cout << "Exists Response (Reboot): " << res_exists_reboot << std::endl;
    assert(res_exists_reboot.find("DEFINITELY_YES") != std::string::npos && "Query should be DEFINITELY_YES on startup");
    
    std::cout << "Verifying short-form exists query without re-inserting..." << std::endl;
    std::string res_exists_reboot_short = sendHttpRequest("GET", "/api/" + test_id_short);
    std::cout << "Short-form Exists Response (Reboot): " << res_exists_reboot_short << std::endl;
    assert(res_exists_reboot_short.find("DEFINITELY_YES") != std::string::npos && "Short query should be DEFINITELY_YES on startup");

    std::cout << "Verifying create-alias exists query without re-inserting..." << std::endl;
    std::string res_exists_reboot_create = sendHttpRequest("GET", "/api/" + test_id_create);
    std::cout << "Create-alias Exists Response (Reboot): " << res_exists_reboot_create << std::endl;
    assert(res_exists_reboot_create.find("DEFINITELY_YES") != std::string::npos && "Create query should be DEFINITELY_YES on startup");
    
    std::cout << "Verifying duplicate insert rejection..." << std::endl;
    std::string res_duplicate = sendHttpRequest("POST", "/api/v1/insert", insert_body);
    std::cout << "Duplicate Insert Response: " << res_duplicate << std::endl;
    assert(res_duplicate.find("409 Conflict") != std::string::npos && "Duplicate insert should return 409 Conflict");
    
    std::cout << "Stopping server (Run 2)..." << std::endl;
    framework.stopInstance(tracker_id2);
    framework.waitForInstance(tracker_id2, 5000);
    
    // Change directory back to original CWD and clean up
    std::filesystem::current_path(original_cwd);
    std::filesystem::remove_all(sandbox);
    
#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "\n=== All database integration tests passed! ===" << std::endl;
    return 0;
}
