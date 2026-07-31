#include "stratum.h"

#include "logger.h"

#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mining {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;
#endif

void close_socket(NativeSocket fd) {
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

std::uint32_t parse_u32(const std::string& text, std::uint32_t fallback) {
    try {
        const auto value = std::stoull(text);
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return fallback;
        }
        return static_cast<std::uint32_t>(value);
    } catch (...) {
        return fallback;
    }
}

constexpr std::size_t kMaxJsonLineBytes = 64U * 1024U;

} // namespace

StratumClient::StratumClient(const std::string& host, std::uint16_t port, Logger& logger, const std::string& node_id)
    : host_(host), port_(port), logger_(logger), node_id_(node_id), socket_fd_(static_cast<SocketType>(kInvalidSocket)) {}

StratumClient::~StratumClient() {
    close_socket(static_cast<NativeSocket>(socket_fd_));
#ifdef _WIN32
    if (wsa_started_) {
        WSACleanup();
    }
#endif
}

bool StratumClient::connect_to_pool() {
#ifdef _WIN32
    if (!wsa_started_) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            logger_.error("stratum", node_id_, "WSAStartup failed");
            return false;
        }
        wsa_started_ = true;
    }
#endif

    std::string port_text = std::to_string(port_);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addrs = nullptr;
    const int gai_rc = getaddrinfo(host_.c_str(), port_text.c_str(), &hints, &addrs);
    if (gai_rc != 0 || addrs == nullptr) {
        logger_.error("stratum", node_id_, "Pool host resolution failed", host_ + ":" + port_text);
        return false;
    }

    NativeSocket connected_fd = kInvalidSocket;
    for (addrinfo* it = addrs; it != nullptr; it = it->ai_next) {
        NativeSocket fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd == kInvalidSocket) {
            continue;
        }

        if (connect(fd, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) {
            connected_fd = fd;
            break;
        }

        close_socket(fd);
    }
    freeaddrinfo(addrs);

    if (connected_fd == kInvalidSocket) {
        logger_.error("stratum", node_id_, "connect() failed", host_ + ":" + std::to_string(port_));
        return false;
    }

    socket_fd_ = static_cast<SocketType>(connected_fd);
    connected_ = true;
    logger_.info("stratum", node_id_, "Connected to pool", host_ + ":" + std::to_string(port_));
    return true;
}

bool StratumClient::send_line(const std::string& line) {
    if (!connected_) {
        return false;
    }

    if (line.size() > kMaxJsonLineBytes) {
        logger_.error("stratum", node_id_, "Outgoing JSON line exceeds size limit");
        connected_ = false;
        return false;
    }

    const std::string out = line + "\n";
    const char* ptr = out.c_str();
    std::size_t left = out.size();

    while (left > 0) {
#ifdef _WIN32
        const int sent = send(static_cast<NativeSocket>(socket_fd_), ptr, static_cast<int>(left), 0);
#else
        const ssize_t sent = send(static_cast<NativeSocket>(socket_fd_), ptr, left, 0);
#endif
        if (sent <= 0) {
            connected_ = false;
            return false;
        }
        left -= static_cast<std::size_t>(sent);
        ptr += sent;
    }

    return true;
}

bool StratumClient::receive_line(std::string& out_line, std::uint32_t timeout_sec) {
    out_line.clear();
    if (!connected_) {
        return false;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(static_cast<NativeSocket>(socket_fd_), &read_set);

        timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        const int ready = select(static_cast<int>(socket_fd_) + 1, &read_set, nullptr, nullptr, &tv);
        if (ready < 0) {
            connected_ = false;
            return false;
        }
        if (ready == 0) {
            continue;
        }

        char ch = 0;
#ifdef _WIN32
        const int rc = recv(static_cast<NativeSocket>(socket_fd_), &ch, 1, 0);
#else
        const ssize_t rc = recv(static_cast<NativeSocket>(socket_fd_), &ch, 1, 0);
#endif
        if (rc <= 0) {
            connected_ = false;
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            if (out_line.size() >= kMaxJsonLineBytes) {
                logger_.error("stratum", node_id_, "Incoming JSON line exceeded size limit");
                connected_ = false;
                return false;
            }
            out_line.push_back(ch);
        }
    }

    return false;
}

bool StratumClient::request_response(const std::string& request, std::string& response, std::uint32_t timeout_sec) {
    std::uint64_t request_id = 0;
    if (!parse_message_id(request, request_id)) {
        return false;
    }
    if (!send_line(request)) {
        return false;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    while (std::chrono::steady_clock::now() < deadline) {
        std::string line;
        if (!receive_line(line, 1)) {
            if (!connected_) {
                return false;
            }
            continue;
        }

        std::uint64_t response_id = 0;
        if (parse_message_id(line, response_id) && response_id == request_id) {
            response = line;
            return true;
        }

        // Ignore notifications while waiting for response.
        StratumJob ignored_job;
        if (parse_notify(line, ignored_job)) {
            logger_.debug("stratum", node_id_, "Buffered notify while awaiting response", ignored_job.job_id);
            continue;
        }
    }

    return false;
}

std::string StratumClient::json_escape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

bool StratumClient::parse_message_id(const std::string& json, std::uint64_t& out_id) {
    const std::regex id_pattern("\\\"id\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, id_pattern) || match.size() < 2) {
        return false;
    }
    try {
        out_id = std::stoull(match[1].str());
        return true;
    } catch (...) {
        return false;
    }
}

bool StratumClient::parse_bool_result(const std::string& json, bool& out) {
    const std::regex result_pattern("\\\"result\\\"\\s*:\\s*(true|false)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(json, match, result_pattern) || match.size() < 2) {
        return false;
    }
    const std::string token = match[1].str();
    out = (token == "true" || token == "TRUE");
    return true;
}

bool StratumClient::parse_error_is_null(const std::string& json) {
    const std::regex err_pattern("\\\"error\\\"\\s*:\\s*null", std::regex_constants::icase);
    return std::regex_search(json, err_pattern);
}

bool StratumClient::parse_notify(const std::string& json, StratumJob& out_job) {
    const std::regex method_pattern("\\\"method\\\"\\s*:\\s*\\\"mining\\.notify\\\"");
    if (!std::regex_search(json, method_pattern)) {
        return false;
    }

    std::smatch match;
    const std::regex job_pattern(
        "\\\"params\\\"\\s*:\\s*\\[\\s*\\\"([^\\\"]+)\\\"\\s*,\\s*\\\"([^\\\"]*)\\\"\\s*,\\s*([0-9]+)\\s*,\\s*(true|false)",
        std::regex_constants::icase);
    if (!std::regex_search(json, match, job_pattern) || match.size() < 5) {
        return false;
    }

    out_job.job_id = match[1].str();
    out_job.prefix = match[2].str();
    out_job.difficulty_bits = parse_u32(match[3].str(), 20);
    const std::string clean = match[4].str();
    out_job.clean_job = (clean == "true" || clean == "TRUE");
    return true;
}

std::string StratumClient::nonce_to_hex(std::uint32_t nonce) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(8) << nonce;
    return oss.str();
}

bool StratumClient::subscribe(const std::string& user_agent) {
    const std::uint64_t id = next_id_++;
    std::ostringstream req;
    req << "{\"id\":" << id
        << ",\"method\":\"mining.subscribe\""
        << ",\"params\":[\"" << json_escape(user_agent) << "\"]}";

    std::string resp;
    if (!request_response(req.str(), resp, 10)) {
        logger_.error("stratum", node_id_, "Subscribe request timed out");
        return false;
    }

    if (!parse_error_is_null(resp)) {
        logger_.error("stratum", node_id_, "Subscribe returned error", resp);
        return false;
    }

    logger_.info("stratum", node_id_, "Subscribe OK");
    return true;
}

bool StratumClient::authorize(const std::string& username, const std::string& password) {
    const std::uint64_t id = next_id_++;
    std::ostringstream req;
    req << "{\"id\":" << id
        << ",\"method\":\"mining.authorize\""
        << ",\"params\":[\"" << json_escape(username) << "\",\"" << json_escape(password) << "\"]}";

    std::string resp;
    if (!request_response(req.str(), resp, 10)) {
        logger_.error("stratum", node_id_, "Authorize request timed out");
        return false;
    }

    bool accepted = false;
    if (!parse_bool_result(resp, accepted) || !accepted) {
        logger_.error("stratum", node_id_, "Authorize rejected", resp);
        return false;
    }

    logger_.info("stratum", node_id_, "Authorize OK", username);
    return true;
}

bool StratumClient::wait_for_job(StratumJob& out_job, std::uint32_t timeout_sec, const std::atomic<bool>* stop_requested) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    while (std::chrono::steady_clock::now() < deadline) {
        if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed)) {
            logger_.info("stratum", node_id_, "Stop requested while waiting for job");
            return false;
        }

        std::string line;
        if (!receive_line(line, 1)) {
            if (!connected_) {
                logger_.warn("stratum", node_id_, "Connection lost while waiting for mining.notify");
                return false;
            }
            continue;
        }
        if (parse_notify(line, out_job)) {
            logger_.info("stratum", node_id_, "Received mining.notify", out_job.job_id);
            return true;
        }
    }

    logger_.warn("stratum", node_id_, "Timeout waiting for mining.notify");
    return false;
}

bool StratumClient::submit_share(const std::string& username, const std::string& job_id, std::uint32_t nonce) {
    const std::uint64_t id = next_id_++;
    const std::string nonce_hex = nonce_to_hex(nonce);

    std::ostringstream req;
    req << "{\"id\":" << id
        << ",\"method\":\"mining.submit\""
        << ",\"params\":[\"" << json_escape(username) << "\",\"" << json_escape(job_id)
        << "\",\"" << nonce_hex << "\"]}";

    std::string resp;
    if (!request_response(req.str(), resp, 10)) {
        logger_.error("stratum", node_id_, "Submit request timed out", nonce_hex);
        return false;
    }

    bool accepted = false;
    if (!parse_bool_result(resp, accepted)) {
        logger_.error("stratum", node_id_, "Submit response parse failed", resp);
        return false;
    }

    if (accepted) {
        logger_.info("stratum", node_id_, "Share accepted", job_id + ":" + nonce_hex);
    } else {
        logger_.warn("stratum", node_id_, "Share rejected", resp);
    }

    return accepted;
}

} // namespace mining
