#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cassert>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include "sqlite3.h"

std::string getBinaryPath() {
    const char* env_binary = std::getenv("AKKON_BINARY");
    if (env_binary && std::filesystem::exists(env_binary)) {
        return env_binary;
    }
    std::vector<std::string> possible_paths = {
        "./Akkon",
        "./cmake-build-debug/Akkon",
        "../cmake-build-debug/Akkon",
        "../../cmake-build-debug/Akkon",
        "../../../cmake-build-debug/Akkon"
    };
    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path)) {
            return std::filesystem::absolute(path).string();
        }
    }
    return "";
}

std::string sendHttpRequest(const std::string& method, const std::string& path, const std::string& body = "") {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
    
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(2409);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return "";
    }
    
    std::string req = method + " " + path + " HTTP/1.1\r\nHost: localhost:2409\r\n";
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
    ssize_t bytes;
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
    close(sock);
    return response;
}

pid_t startServer(const std::filesystem::path& sandbox) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Fork failed" << std::endl;
        exit(1);
    }
    
    if (pid == 0) {
        // Child process
        if (chdir(sandbox.c_str()) != 0) {
            std::cerr << "Failed to chdir to sandbox" << std::endl;
            exit(1);
        }
        
        int fd = open("server.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        
        char* argv[] = {const_cast<char*>("./Akkon"), const_cast<char*>("-c"), const_cast<char*>("3"), const_cast<char*>("--debug"), nullptr};
        execv("./Akkon", argv);
        std::cerr << "Failed to exec Akkon" << std::endl;
        exit(1);
    }
    
    return pid;
}

void stopServer(pid_t pid) {
    kill(pid, SIGTERM);
    int status;
    waitpid(pid, &status, 0);
}

int main() {
    std::cout << "=== Akkon Database Integration Test ===" << std::endl;
    
    std::filesystem::path sandbox = std::filesystem::current_path() / "test_sandbox";
    if (std::filesystem::exists(sandbox)) {
        std::filesystem::remove_all(sandbox);
    }
    std::filesystem::create_directories(sandbox);
    
    std::string bin_path = getBinaryPath();
    std::cout << "Binary path: " << bin_path << std::endl;
    assert(!bin_path.empty() && "Akkon binary not found");
    
    std::filesystem::path sandbox_bin = sandbox / "Akkon";
    std::filesystem::copy_file(bin_path, sandbox_bin);
    std::filesystem::permissions(sandbox_bin, std::filesystem::perms::owner_all | std::filesystem::perms::group_all);
    
    // ----------------------------------------------------
    // RUN 1: Start server, register entity, verify DB file
    // ----------------------------------------------------
    std::cout << "\n[RUN 1] Starting Akkon server..." << std::endl;
    pid_t pid1 = startServer(sandbox);
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
    
    std::cout << "Stopping server (Run 1)..." << std::endl;
    stopServer(pid1);
    
    // Verify physical SQLite shard files
    std::cout << "Inspecting SQLite shards directly..." << std::endl;
    bool found_in_db = false;
    std::filesystem::path db_dir = sandbox / "db";
    assert(std::filesystem::exists(db_dir) && "db folder does not exist");
    
    for (const auto& entry : std::filesystem::directory_iterator(db_dir)) {
        if (entry.path().extension() == ".db" && entry.path().filename() != "master.db") {
            sqlite3* db = nullptr;
            if (sqlite3_open(entry.path().c_str(), &db) == SQLITE_OK) {
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
    pid_t pid2 = startServer(sandbox);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "Verifying exists query without re-inserting..." << std::endl;
    std::string res_exists_reboot = sendHttpRequest("GET", "/api/v1/exists?identifier=" + test_id);
    std::cout << "Exists Response (Reboot): " << res_exists_reboot << std::endl;
    assert(res_exists_reboot.find("DEFINITELY_YES") != std::string::npos && "Query should be DEFINITELY_YES on startup");
    
    std::cout << "Verifying duplicate insert rejection..." << std::endl;
    std::string res_duplicate = sendHttpRequest("POST", "/api/v1/insert", insert_body);
    std::cout << "Duplicate Insert Response: " << res_duplicate << std::endl;
    assert(res_duplicate.find("409 Conflict") != std::string::npos && "Duplicate insert should return 409 Conflict");
    
    std::cout << "Stopping server (Run 2)..." << std::endl;
    stopServer(pid2);
    
    // Teardown sandbox
    std::filesystem::remove_all(sandbox);
    std::cout << "\n=== All database integration tests passed! ===" << std::endl;
    return 0;
}
