#include "lib/job.h"
#include "lib/logger.h"
#include "lib/stratum.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

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
    std::uint32_t pool_reconnect_initial_sec = 1;
    std::uint32_t pool_reconnect_max_sec = 30;
    std::string prefix = "hello-bitcoin";
    std::uint32_t difficulty_bits = 24U;
    std::uint32_t thread_count = std::max(1U, std::thread::hardware_concurrency());
    std::uint32_t report_interval_ms = 1000U;
    std::string log_output = "logs/miner.log";
};

struct Result {
    bool found = false;
    bool aborted = false;
    std::uint32_t nonce = 0U;
    btc::Hash256 hash{};
    std::uint64_t hashes_tried = 0U;
    double elapsed_seconds = 0.0;
};

struct SessionStats {
    std::uint64_t jobs_received = 0;
    std::uint64_t shares_found = 0;
    std::uint64_t submits_attempted = 0;
    std::uint64_t submit_failures = 0;
    std::uint64_t accepted_count = 0;
    std::uint64_t reconnect_events = 0;
    std::uint64_t session_failures = 0;
    int last_failure_rc = 0;
    std::string last_job_id = "none";
    std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
};

std::atomic<bool> g_stop_requested{false};

void on_signal(int /*signal_code*/) {
    g_stop_requested.store(true, std::memory_order_relaxed);
}

std::string read_text_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

bool parse_json_string(const std::string& json, const std::string& key, std::string& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, pattern) || match.size() < 2) {
        return false;
    }

    out = match[1].str();
    return true;
}

bool parse_json_uint(const std::string& json, const std::string& key, std::uint64_t& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern) || match.size() < 2) {
        return false;
    }

    try {
        out = std::stoull(match[1].str());
    } catch (...) {
        return false;
    }

    return true;
}

bool parse_json_bool(const std::string& json, const std::string& key, bool& out) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern) || match.size() < 2) {
        return false;
    }

    out = (match[1].str() == "true");
    return true;
}

bool load_config(const std::string& path, MinerConfig& cfg) {
    cfg = MinerConfig{};
    const std::string json = read_text_file(path);
    if (json.empty()) {
        return false;
    }

    std::string text_value;
    std::uint64_t uint_value = 0;

    if (parse_json_string(json, "node_id", text_value)) {
        cfg.node_id = text_value;
    }
    if (parse_json_string(json, "worker_id", text_value)) {
        cfg.worker_id = text_value;
    }
    if (parse_json_string(json, "payout_address", text_value)) {
        cfg.payout_address = text_value;
    }
    if (parse_json_string(json, "username", text_value)) {
        cfg.pool_username = text_value;
    }
    if (parse_json_string(json, "host", text_value)) {
        cfg.pool_host = text_value;
    }
    if (parse_json_uint(json, "port", uint_value) && uint_value > 0U && uint_value <= 65535U) {
        cfg.pool_port = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_string(json, "password", text_value)) {
        cfg.pool_password = text_value;
    }
    if (parse_json_string(json, "password_env", text_value)) {
        cfg.pool_password_env = text_value;
    }
    if (parse_json_bool(json, "enabled", cfg.pool_enabled)) {
        // parsed
    }
    if (parse_json_uint(json, "notify_timeout_sec", uint_value) && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_notify_timeout_sec = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_uint(json, "max_cycles", uint_value) && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_max_cycles = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_uint(json, "reconnect_initial_sec", uint_value) && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_reconnect_initial_sec = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_uint(json, "reconnect_max_sec", uint_value) && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_reconnect_max_sec = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_string(json, "prefix", text_value)) {
        cfg.prefix = text_value;
    }
    if (parse_json_uint(json, "difficulty_bits", uint_value) && uint_value <= 255U) {
        cfg.difficulty_bits = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_uint(json, "threads", uint_value) && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.thread_count = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_uint(json, "report_interval_ms", uint_value) && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.report_interval_ms = static_cast<std::uint32_t>(uint_value);
    }
    if (parse_json_string(json, "output", text_value)) {
        cfg.log_output = text_value;
    }

    return true;
}

void print_usage() {
    std::cout
        << "Usage: i_mine [--config path] [--threads N] [--bits N] [--prefix text] [--report-ms N]\n"
        << "  --config      Miner JSON config path (default: config/miner-local-stratum.json)\n"
        << "  --threads     CPU worker threads override\n"
        << "  --bits        Leading zero bits required in hash\n"
        << "  --prefix      Payload prefix override\n"
        << "  --report-ms   Status print interval in milliseconds\n";
}

bool parse_uint32(const std::string& s, std::uint32_t& out) {
    try {
        std::size_t idx = 0;
        const auto value = std::stoul(s, &idx, 10);
        if (idx != s.size() || value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        out = static_cast<std::uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_args(int argc, char** argv, std::string& config_path, MinerConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return false;
        }

        if (i + 1 >= argc) {
            std::cerr << "Missing value for: " << arg << "\n";
            return false;
        }

        const std::string value = argv[++i];
        if (arg == "--config") {
            config_path = value;
            if (!load_config(config_path, cfg)) {
                std::cerr << "Failed to read config file: " << config_path << "\n";
                return false;
            }
        } else if (arg == "--prefix") {
            cfg.prefix = value;
        } else if (arg == "--bits") {
            if (!parse_uint32(value, cfg.difficulty_bits) || cfg.difficulty_bits > 255U) {
                std::cerr << "Invalid --bits value\n";
                return false;
            }
        } else if (arg == "--threads") {
            if (!parse_uint32(value, cfg.thread_count) || cfg.thread_count == 0U) {
                std::cerr << "Invalid --threads value\n";
                return false;
            }
        } else if (arg == "--report-ms") {
            if (!parse_uint32(value, cfg.report_interval_ms) || cfg.report_interval_ms == 0U) {
                std::cerr << "Invalid --report-ms value\n";
                return false;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    return true;
}

std::string pool_username(const MinerConfig& cfg);

void apply_secret_overrides_from_env(MinerConfig& cfg, mining::Logger& logger) {
    if (cfg.pool_password_env.empty()) {
        return;
    }

#ifdef _WIN32
    char* env_val = nullptr;
    std::size_t env_len = 0;
    const errno_t env_rc = _dupenv_s(&env_val, &env_len, cfg.pool_password_env.c_str());
    const bool missing = (env_rc != 0 || env_val == nullptr || env_len == 0 || env_val[0] == '\0');
    if (missing) {
        if (env_val != nullptr) {
            std::free(env_val);
        }
        logger.warn("config", cfg.node_id, "pool.password_env is set but environment variable is missing", cfg.pool_password_env);
        return;
    }

    cfg.pool_password = env_val;
    std::free(env_val);
#else
    const char* env_val = std::getenv(cfg.pool_password_env.c_str());
    if (env_val == nullptr || env_val[0] == '\0') {
        logger.warn("config", cfg.node_id, "pool.password_env is set but environment variable is missing", cfg.pool_password_env);
        return;
    }

    cfg.pool_password = env_val;
#endif
    logger.info("config", cfg.node_id, "Loaded pool password from environment variable", cfg.pool_password_env);
}

bool validate_config(const MinerConfig& cfg, std::string& error_message) {
    if (cfg.thread_count == 0U) {
        error_message = "hashing.threads must be greater than 0";
        return false;
    }
    if (cfg.report_interval_ms == 0U) {
        error_message = "hashing.report_interval_ms must be greater than 0";
        return false;
    }

    if (!cfg.pool_enabled) {
        return true;
    }

    if (cfg.pool_host.empty()) {
        error_message = "pool.host must be set when pool.enabled=true";
        return false;
    }
    if (cfg.pool_port == 0U || cfg.pool_port > 65535U) {
        error_message = "pool.port must be in range 1..65535 when pool.enabled=true";
        return false;
    }
    if (pool_username(cfg).empty()) {
        error_message = "pool.username (or payout_address+worker_id) must be set when pool.enabled=true";
        return false;
    }
    if (cfg.pool_password.empty()) {
        error_message = "pool.password is empty; set pool.password or pool.password_env";
        return false;
    }
    if (cfg.pool_notify_timeout_sec == 0U) {
        error_message = "pool.notify_timeout_sec must be greater than 0";
        return false;
    }
    if (cfg.pool_reconnect_initial_sec == 0U || cfg.pool_reconnect_max_sec == 0U) {
        error_message = "pool reconnect backoff values must be greater than 0";
        return false;
    }

    return true;
}

std::string pool_username(const MinerConfig& cfg) {
    if (!cfg.pool_username.empty()) {
        return cfg.pool_username;
    }
    if (cfg.payout_address.empty()) {
        return cfg.worker_id;
    }
    return cfg.payout_address + "." + cfg.worker_id;
}

Result mine(const MinerConfig& cfg, mining::Logger& logger, const std::atomic<bool>* stop_requested = nullptr) {
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> total_hashes{0U};
    std::mutex result_mutex;
    Result result;

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    workers.reserve(cfg.thread_count);

    for (std::uint32_t tid = 0; tid < cfg.thread_count; ++tid) {
        workers.emplace_back([&, tid]() {
            std::vector<std::uint8_t> payload(cfg.prefix.begin(), cfg.prefix.end());
            payload.resize(payload.size() + sizeof(std::uint32_t));

            std::uint64_t local_hashes = 0U;
            for (std::uint64_t nonce64 = tid; nonce64 <= std::numeric_limits<std::uint32_t>::max(); nonce64 += cfg.thread_count) {
                if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed)) {
                    break;
                }
                if (stop.load(std::memory_order_relaxed)) {
                    break;
                }

                const std::uint32_t nonce = static_cast<std::uint32_t>(nonce64);
                const std::size_t base = payload.size() - sizeof(std::uint32_t);
                payload[base + 0U] = static_cast<std::uint8_t>(nonce & 0xFFU);
                payload[base + 1U] = static_cast<std::uint8_t>((nonce >> 8U) & 0xFFU);
                payload[base + 2U] = static_cast<std::uint8_t>((nonce >> 16U) & 0xFFU);
                payload[base + 3U] = static_cast<std::uint8_t>((nonce >> 24U) & 0xFFU);

                const btc::Hash256 hash = btc::double_sha256(payload);
                ++local_hashes;

                if (mining::meets_difficulty(hash, cfg.difficulty_bits)) {
                    bool expected = false;
                    if (stop.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        result.found = true;
                        result.nonce = nonce;
                        result.hash = hash;
                    }
                    break;
                }
            }

            total_hashes.fetch_add(local_hashes, std::memory_order_relaxed);
        });
    }

    while (!stop.load(std::memory_order_relaxed)) {
        if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed)) {
            stop.store(true, std::memory_order_relaxed);
            result.aborted = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.report_interval_ms));
        const auto now = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(now - start).count();
        const std::uint64_t hashes = total_hashes.load(std::memory_order_relaxed);
        const double hps = sec > 0.0 ? static_cast<double>(hashes) / sec : 0.0;

        std::ostringstream status;
        status << "hashes=" << hashes
               << " elapsed=" << std::fixed << std::setprecision(2) << sec << "s"
               << " h/s=" << std::setprecision(0) << hps;
        logger.info("miner", cfg.node_id, "Progress", status.str());

        if (hashes >= std::numeric_limits<std::uint32_t>::max()) {
            stop.store(true, std::memory_order_relaxed);
            break;
        }
    }

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();
    result.hashes_tried = total_hashes.load(std::memory_order_relaxed);
    return result;
}

bool start_stratum_session(const MinerConfig& cfg, mining::Logger& logger, mining::StratumClient& client) {
    const std::string username = pool_username(cfg);

    if (!client.connect_to_pool()) {
        return false;
    }
    if (!client.subscribe("i_mine/stratum-offline-ready")) {
        return false;
    }
    if (!client.authorize(username, cfg.pool_password)) {
        return false;
    }

    logger.info("stratum", cfg.node_id, "Stratum session started", "user=" + username);
    return true;
}

int run_stratum_session(const MinerConfig& cfg, mining::Logger& logger, std::uint32_t& cycles_done, SessionStats& stats) {
    mining::StratumClient client(
        cfg.pool_host,
        static_cast<std::uint16_t>(cfg.pool_port),
        logger,
        cfg.node_id);

    if (!start_stratum_session(cfg, logger, client)) {
        return 4;
    }

    const std::string username = pool_username(cfg);
    while (!g_stop_requested.load(std::memory_order_relaxed)
        && (cfg.pool_max_cycles == 0U || cycles_done < cfg.pool_max_cycles)) {
        mining::StratumJob job;
        if (!client.wait_for_job(job, cfg.pool_notify_timeout_sec, &g_stop_requested)) {
            if (g_stop_requested.load(std::memory_order_relaxed)) {
                return 0;
            }
            logger.warn("stratum", cfg.node_id, "Session lost while waiting for job; reconnect required");
            return 6;
        }

        ++stats.jobs_received;

        MinerConfig mining_cfg = cfg;
        if (!job.prefix.empty()) {
            mining_cfg.prefix = job.prefix;
        }
        mining_cfg.difficulty_bits = job.difficulty_bits;

        std::ostringstream job_ctx;
        job_ctx << "job_id=" << job.job_id
                << " prefix=" << mining_cfg.prefix
                << " bits=" << mining_cfg.difficulty_bits
                << " clean_job=" << (job.clean_job ? "true" : "false");
        logger.info("stratum", cfg.node_id, "Mining on Stratum job", job_ctx.str());

        const Result result = mine(mining_cfg, logger, &g_stop_requested);
        if (result.aborted) {
            return 0;
        }
        if (!result.found) {
            logger.warn("stratum", cfg.node_id, "No valid nonce found for job", job.job_id);
            continue;
        }

        ++stats.shares_found;
        ++stats.submits_attempted;

        if (!client.submit_share(username, job.job_id, result.nonce)) {
            ++stats.submit_failures;
            logger.warn("stratum", cfg.node_id, "Submit failed; reconnect required", job.job_id);
            return 8;
        }

        ++cycles_done;
        ++stats.accepted_count;
        stats.last_job_id = job.job_id;
        logger.info("stratum", cfg.node_id, "Stratum cycle completed", job.job_id);
    }

    logger.info("stratum", cfg.node_id, "Stratum session reached cycle limit", "cycles=" + std::to_string(cycles_done));
    return 0;
}

int run_stratum_loop(const MinerConfig& cfg, mining::Logger& logger) {
    std::uint32_t cycles_done = 0;
    std::uint32_t backoff_sec = std::max(1U, cfg.pool_reconnect_initial_sec);
    const std::uint32_t backoff_max = std::max(backoff_sec, cfg.pool_reconnect_max_sec);
    SessionStats stats;
    stats.started_at = std::chrono::steady_clock::now();

    while (!g_stop_requested.load(std::memory_order_relaxed)
        && (cfg.pool_max_cycles == 0U || cycles_done < cfg.pool_max_cycles)) {
        const int rc = run_stratum_session(cfg, logger, cycles_done, stats);
        if (rc == 0) {
            backoff_sec = std::max(1U, cfg.pool_reconnect_initial_sec);
            continue;
        }

        ++stats.reconnect_events;
        ++stats.session_failures;
        stats.last_failure_rc = rc;

        std::ostringstream ctx;
        ctx << "rc=" << rc
            << " sleep_sec=" << backoff_sec
            << " cycles_done=" << cycles_done;
        logger.warn("stratum", cfg.node_id, "Stratum cycle failed; reconnecting", ctx.str());

        if (g_stop_requested.load(std::memory_order_relaxed)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(backoff_sec));
        backoff_sec = std::min(backoff_max, backoff_sec * 2U);
    }

    const auto now = std::chrono::steady_clock::now();
    const auto duration_sec = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - stats.started_at).count());
    const double minutes = duration_sec > 0U ? static_cast<double>(duration_sec) / 60.0 : 0.0;
    const double accepted_per_min = minutes > 0.0
        ? static_cast<double>(stats.accepted_count) / minutes
        : static_cast<double>(stats.accepted_count);

    std::string readiness_status = "not_ready";
    if (stats.accepted_count >= 3U && stats.submit_failures == 0U && stats.session_failures == 0U) {
        readiness_status = "ready";
    } else if (stats.accepted_count > 0U) {
        readiness_status = "degraded";
    }

    std::ostringstream summary;
    summary << "accepted_count=" << stats.accepted_count
            << " jobs_received=" << stats.jobs_received
            << " shares_found=" << stats.shares_found
            << " submits_attempted=" << stats.submits_attempted
            << " submit_failures=" << stats.submit_failures
            << " reconnect_events=" << stats.reconnect_events
            << " session_failures=" << stats.session_failures
            << " last_failure_rc=" << stats.last_failure_rc
            << " session_duration_sec=" << duration_sec
            << " accepted_per_min=" << std::fixed << std::setprecision(2) << accepted_per_min
            << " last_job_id=" << stats.last_job_id;
    logger.info("stratum", cfg.node_id, "Shutdown summary", summary.str());

    std::ostringstream readiness;
    readiness << "status=" << readiness_status
              << " accepted_count=" << stats.accepted_count
              << " submit_failures=" << stats.submit_failures
              << " session_failures=" << stats.session_failures
              << " reconnect_events=" << stats.reconnect_events;
    logger.info("stratum", cfg.node_id, "Readiness report", readiness.str());

    logger.info("stratum", cfg.node_id, "Stratum loop finished", "cycles=" + std::to_string(cycles_done));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
#ifdef SIGTERM
    std::signal(SIGTERM, on_signal);
#endif

    const std::string default_config_path = "config/miner-local-stratum.json";
    std::string config_path = default_config_path;
    MinerConfig cfg;
    const bool default_config_loaded = load_config(config_path, cfg);
    if (!parse_args(argc, argv, config_path, cfg)) {
        return 1;
    }
    if (config_path == default_config_path && !default_config_loaded) {
        std::cerr << "Failed to read default config file: " << config_path << "\n";
        return 1;
    }

    mining::Logger logger(cfg.log_output, mining::LogLevel::INFO);
    apply_secret_overrides_from_env(cfg, logger);

    std::string config_error;
    if (!validate_config(cfg, config_error)) {
        logger.error("config", cfg.node_id, "Invalid configuration", config_error);
        return 1;
    }

    std::ostringstream startup;
    startup << "worker_id=" << cfg.worker_id
            << " threads=" << cfg.thread_count
            << " bits=" << cfg.difficulty_bits
            << " pool_enabled=" << (cfg.pool_enabled ? "true" : "false")
            << " pool_user=" << pool_username(cfg)
            << " pool_host=" << cfg.pool_host << ":" << cfg.pool_port;
    logger.info("miner", cfg.node_id, "Independent miner starting", startup.str());

    if (cfg.pool_enabled) {
        const int rc = run_stratum_loop(cfg, logger);
        if (rc != 0) {
            return rc;
        }
        if (g_stop_requested.load(std::memory_order_relaxed)) {
            logger.info("miner", cfg.node_id, "Graceful shutdown complete", "reason=signal");
        }
        return 0;
    }

    const Result result = mine(cfg, logger, &g_stop_requested);
    if (result.aborted) {
        logger.info("miner", cfg.node_id, "Graceful shutdown complete", "reason=signal");
        return 0;
    }
    if (!result.found) {
        logger.warn("miner", cfg.node_id, "No valid nonce found in 32-bit nonce space");
        return 2;
    }

    const double hps = result.elapsed_seconds > 0.0
        ? static_cast<double>(result.hashes_tried) / result.elapsed_seconds
        : 0.0;

    std::ostringstream done;
    done << "nonce=" << result.nonce
         << " hash=" << mining::hash_to_hex(result.hash)
         << " tries=" << result.hashes_tried
         << " elapsed_seconds=" << std::fixed << std::setprecision(3) << result.elapsed_seconds
         << " rate_hps=" << std::setprecision(0) << hps;
    logger.info("miner", cfg.node_id, "Proof-of-work complete", done.str());

    std::cout << "Found nonce: " << result.nonce << "\n"
              << "Hash:       " << mining::hash_to_hex(result.hash) << "\n"
              << "Tries:      " << result.hashes_tried << "\n"
              << "Elapsed:    " << std::fixed << std::setprecision(3) << result.elapsed_seconds << " s\n"
              << "Rate:       " << std::setprecision(0) << hps << " H/s\n";
    return 0;
}
