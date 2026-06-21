#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace btc {

using Hash256 = std::array<std::uint8_t, 32>;

Hash256 sha256(const std::uint8_t* data, std::size_t len);
Hash256 sha256(const std::vector<std::uint8_t>& data);
Hash256 double_sha256(const std::vector<std::uint8_t>& data);

} // namespace btc
