#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
#endif

void close_socket(SocketType fd) {
#ifdef _WIN32
    if (fd != kInvalidSocket) {
        closesocket(fd);
    }
#else
    if (fd != kInvalidSocket) {
        close(fd);
    }
#endif
}

bool send_line(SocketType fd, const std::string& line) {
    const std::string out = line + "\n";
    const char* ptr = out.c_str();
    std::size_t left = out.size();
    while (left > 0) {
#ifdef _WIN32
        const int sent = send(fd, ptr, static_cast<int>(left), 0);
#else
        const ssize_t sent = send(fd, ptr, left, 0);
#endif
        if (sent <= 0) {
            return false;
        }
        left -= static_cast<std::size_t>(sent);
        ptr += sent;
    }
    return true;
}

bool receive_line(SocketType fd, std::string& out) {
    out.clear();
    char ch = 0;
    while (true) {
#ifdef _WIN32
        const int rc = recv(fd, &ch, 1, 0);
#else
        const ssize_t rc = recv(fd, &ch, 1, 0);
#endif
        if (rc <= 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            out.push_back(ch);
        }
    }
}

std::uint64_t parse_id(const std::string& json) {
    const std::regex id_pattern("\\\"id\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, id_pattern) || match.size() < 2) {
        return 0;
    }
    try {
        return std::stoull(match[1].str());
    } catch (...) {
        return 0;
    }
}

std::string json_resp(std::uint64_t id, const std::string& result, const std::string& error = "null") {
    std::ostringstream oss;
    oss << "{\"id\":" << id << ",\"result\":" << result << ",\"error\":" << error << "}";
    return oss.str();
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = 3333;
    if (argc >= 2) {
        const int v = std::atoi(argv[1]);
        if (v > 0 && v <= 65535) {
            port = static_cast<std::uint16_t>(v);
        }
    }

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    SocketType server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == kInvalidSocket) {
        std::cerr << "socket() failed\n";
        return 2;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "bind() failed\n";
        close_socket(server_fd);
        return 3;
    }

    if (listen(server_fd, 1) != 0) {
        std::cerr << "listen() failed\n";
        close_socket(server_fd);
        return 4;
    }

    std::cout << "[fake-pool] listening on 127.0.0.1:" << port << "\n";

    sockaddr_in client_addr{};
#ifdef _WIN32
    int client_len = sizeof(client_addr);
#else
    socklen_t client_len = sizeof(client_addr);
#endif
    SocketType client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd == kInvalidSocket) {
        std::cerr << "accept() failed\n";
        close_socket(server_fd);
        return 5;
    }

    std::cout << "[fake-pool] client connected\n";

    bool notified = false;
    std::uint32_t job_seq = 1;
    while (true) {
        std::string line;
        if (!receive_line(client_fd, line)) {
            break;
        }

        std::cout << "[fake-pool] << " << line << "\n";

        const std::uint64_t id = parse_id(line);
        if (line.find("\"method\":\"mining.subscribe\"") != std::string::npos) {
            send_line(client_fd, json_resp(id, "[[\"mining.notify\",\"deadbeef\"],\"abcdef01\",4]"));
            continue;
        }

        if (line.find("\"method\":\"mining.authorize\"") != std::string::npos) {
            send_line(client_fd, json_resp(id, "true"));
            if (!notified) {
                std::ostringstream notify;
                notify << "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job-" << job_seq
                       << "\",\"offline-job-" << job_seq << "\",20,true]}";
                send_line(client_fd, notify.str());
                notified = true;
            }
            continue;
        }

        if (line.find("\"method\":\"mining.submit\"") != std::string::npos) {
            send_line(client_fd, json_resp(id, "true"));
            ++job_seq;
            std::ostringstream notify;
            notify << "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"job-" << job_seq
                   << "\",\"offline-job-" << job_seq << "\",20,true]}";
            send_line(client_fd, notify.str());
            continue;
        }

        send_line(client_fd, json_resp(id, "false", "[20,\"unknown method\",null]"));
    }

    close_socket(client_fd);
    close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[fake-pool] shutdown\n";
    return 0;
}
