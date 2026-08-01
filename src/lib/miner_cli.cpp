#include "miner_cli.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace {

bool parse_uint32(const std::string& text, std::uint32_t& out) {
    try {
        std::size_t idx = 0;
        const auto value = std::stoul(text, &idx, 10);
        if (idx != text.size() || value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        out = static_cast<std::uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

void print_usage() {
    std::cout
    << "Usage: i_mine [--config path] [--threads N] [--bits N] [--prefix text] [--report-ms N]\n"
    << "  --config      Miner JSON config path (default: config/miner-local-stratum.json)\n"
    << "  --threads     CPU worker threads override\n"
    << "  --bits        Leading zero bits required in hash\n"
    << "  --prefix      Payload prefix override\n"
    << "  --report-ms   Status print interval in milliseconds\n";
}

CliParseResult parse_args(int argc, char** argv, std::string& config_path, MinerConfig& cfg) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return CliParseResult::HelpShown;
        }

        if (i + 1 >= argc) {
            std::cerr << "Missing value for: " << arg << "\n";
            return CliParseResult::Error;
        }

        const std::string value = argv[++i];
        if (arg == "--config") {
            config_path = value;
            if (!load_config(config_path, cfg)) {
                std::cerr << "Failed to read config file: " << config_path << "\n";
                return CliParseResult::Error;
            }
        } else if (arg == "--prefix") {
            cfg.prefix = value;
        } else if (arg == "--bits") {
            if (!parse_uint32(value, cfg.difficulty_bits) || cfg.difficulty_bits > 255U) {
                std::cerr << "Invalid --bits value\n";
                return CliParseResult::Error;
            }
        } else if (arg == "--threads") {
            if (!parse_uint32(value, cfg.thread_count) || cfg.thread_count == 0U) {
                std::cerr << "Invalid --threads value\n";
                return CliParseResult::Error;
            }
        } else if (arg == "--report-ms") {
            if (!parse_uint32(value, cfg.report_interval_ms) || cfg.report_interval_ms == 0U) {
                std::cerr << "Invalid --report-ms value\n";
                return CliParseResult::Error;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return CliParseResult::Error;
        }
    }

    return CliParseResult::Ok;
}
