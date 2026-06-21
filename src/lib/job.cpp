#include "job.h"

#include <iomanip>
#include <sstream>

namespace mining {

bool meets_difficulty(const btc::Hash256& hash, std::uint32_t difficulty_bits) {
    std::uint32_t bits_left = difficulty_bits;

    for (std::uint8_t byte : hash) {
        if (bits_left == 0U) {
            return true;
        }

        if (bits_left >= 8U) {
            if (byte != 0U) {
                return false;
            }
            bits_left -= 8U;
        } else {
            const std::uint8_t mask = static_cast<std::uint8_t>(0xFFU << (8U - bits_left));
            return (byte & mask) == 0U;
        }
    }

    return true;
}

std::string hash_to_hex(const btc::Hash256& hash) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint8_t b : hash) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        std::uint8_t byte = static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

} // namespace mining
