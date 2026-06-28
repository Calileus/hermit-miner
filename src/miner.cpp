#include "lib/job.h"
#include "lib/logger.h"
#include "lib/miner_config.h"
#include "lib/miner_runtime.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

std::atomic<bool> g_stop_requested{false};

void on_signal(int /*signal_code*/) {
    g_stop_requested.store(true, std::memory_order_relaxed);
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
        const int rc = run_stratum_loop(cfg, logger, g_stop_requested);
        if (rc != 0) {
            return rc;
        }
        if (g_stop_requested.load(std::memory_order_relaxed)) {
            logger.info("miner", cfg.node_id, "Graceful shutdown complete", "reason=signal");
        }
        return 0;
    }

    const MiningResult result = mine(cfg, logger, &g_stop_requested);
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
