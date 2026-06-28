#pragma once

#include "miner_config.h"

#include "sha256.h"

#include <atomic>
#include <cstdint>

namespace mining {
class Logger;
}

struct MiningResult {
    bool found = false;
    bool aborted = false;
    std::uint32_t nonce = 0U;
    btc::Hash256 hash{};
    std::uint64_t hashes_tried = 0U;
    double elapsed_seconds = 0.0;
};

MiningResult mine(const MinerConfig& cfg, mining::Logger& logger, const std::atomic<bool>* stop_requested = nullptr);
int run_stratum_loop(const MinerConfig& cfg, mining::Logger& logger, const std::atomic<bool>& stop_requested);
