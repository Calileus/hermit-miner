#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <thread>

namespace mining {
class Logger;
}

struct MinerConfig {
    std::string node_id = "CP-1";
    std::string worker_id = "cp1";
    std::string payout_address;
    std::string pool_username;
    std::string pool_host = "pool.example.com";
    std::uint32_t pool_port = 3333;
    std::string pool_password = "x";
    std::string pool_password_env;
    bool pool_enabled = false;
    std::uint32_t pool_notify_timeout_sec = 30;
    std::uint32_t pool_max_cycles = 0;
    std::uint32_t pool_max_reconnect_attempts = 0;
    std::uint32_t pool_reconnect_initial_sec = 1;
    std::uint32_t pool_reconnect_max_sec = 30;
    std::string prefix = "hello-bitcoin";
    std::uint32_t difficulty_bits = 24U;
    std::uint32_t thread_count = std::max(1U, std::thread::hardware_concurrency());
    std::uint32_t report_interval_ms = 1000U;
    std::string log_output = "logs/miner.log";
    std::string health_output;
};

bool load_config(const std::string& path, MinerConfig& cfg);
std::string pool_username(const MinerConfig& cfg);
void apply_secret_overrides_from_env(MinerConfig& cfg, mining::Logger& logger);
bool validate_config(const MinerConfig& cfg, std::string& error_message);
