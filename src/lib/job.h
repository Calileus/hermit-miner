#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "sha256.h"

namespace mining {

// Generic job representation - coin-neutral
struct Job {
    std::string job_id;
    bool clean_job = false;
    std::string algorithm = "generic";
    std::vector<std::uint8_t> job_data;  // Raw job bytes (header template, etc.)
    std::uint64_t nonce_start = 0;
    std::uint64_t nonce_range = 0;
    std::string target;  // Hex-encoded target
    std::uint32_t difficulty_bits = 0;

    // Parse from JSON would go here in Phase 2
};

// Generic share representation
struct Share {
    std::string job_id;
    std::uint64_t nonce = 0;
    btc::Hash256 hash{};
    std::string timestamp;  // ISO 8601
    std::string status;     // "pending", "accepted", "stale", "rejected"
    std::string reason;     // Error reason if rejected
};

// Validate a candidate hash against difficulty
bool meets_difficulty(const btc::Hash256& hash, std::uint32_t difficulty_bits);

// Convert hash to hex string
std::string hash_to_hex(const btc::Hash256& hash);

// Convert hex string to bytes
std::vector<std::uint8_t> hex_to_bytes(const std::string& hex);

} // namespace mining
