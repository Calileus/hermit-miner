#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>

namespace mining {

class Logger;

struct StratumJob {
    std::string job_id;
    std::string prefix;
    std::uint32_t difficulty_bits = 20;
    bool clean_job = true;
};

class StratumClient {
public:
    StratumClient(const std::string& host, std::uint16_t port, Logger& logger, const std::string& node_id);
    ~StratumClient();

    bool connect_to_pool();
    bool subscribe(const std::string& user_agent);
    bool authorize(const std::string& username, const std::string& password);
    bool wait_for_job(StratumJob& out_job, std::uint32_t timeout_sec, const std::atomic<bool>* stop_requested = nullptr);
    bool submit_share(const std::string& username, const std::string& job_id, std::uint32_t nonce);

private:
    std::string host_;
    std::uint16_t port_;
    Logger& logger_;
    std::string node_id_;
    std::uint64_t next_id_ = 1;

    using SocketType = std::intptr_t;

    SocketType socket_fd_;
    bool connected_ = false;
    bool wsa_started_ = false;

    bool send_line(const std::string& line);
    bool receive_line(std::string& out_line, std::uint32_t timeout_sec);
    bool request_response(const std::string& request, std::string& response, std::uint32_t timeout_sec = 10);

    static std::string json_escape(const std::string& input);
    static bool parse_bool_result(const std::string& json, bool& out);
    static bool parse_error_is_null(const std::string& json);
    static bool parse_notify(const std::string& json, StratumJob& out_job);
    static std::string nonce_to_hex(std::uint32_t nonce);
    static bool parse_message_id(const std::string& json, std::uint64_t& out_id);
};

} // namespace mining
