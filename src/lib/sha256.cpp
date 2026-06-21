#include "sha256.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::array<std::uint32_t, 64> K = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t big_sigma0(std::uint32_t x) {
    return rotr(x, 2U) ^ rotr(x, 13U) ^ rotr(x, 22U);
}

inline std::uint32_t big_sigma1(std::uint32_t x) {
    return rotr(x, 6U) ^ rotr(x, 11U) ^ rotr(x, 25U);
}

inline std::uint32_t small_sigma0(std::uint32_t x) {
    return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U);
}

inline std::uint32_t small_sigma1(std::uint32_t x) {
    return rotr(x, 17U) ^ rotr(x, 19U) ^ (x >> 10U);
}

} // namespace

namespace btc {

Hash256 sha256(const std::uint8_t* data, std::size_t len) {
    std::array<std::uint32_t, 8> h = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };

    const std::size_t bit_len = len * 8U;
    const std::size_t total_len = ((len + 9U + 63U) / 64U) * 64U;

    std::vector<std::uint8_t> msg(total_len, 0);
    if (len > 0U) {
        std::memcpy(msg.data(), data, len);
    }

    msg[len] = 0x80U;

    for (std::size_t i = 0; i < 8U; ++i) {
        msg[total_len - 1U - i] = static_cast<std::uint8_t>((bit_len >> (i * 8U)) & 0xFFU);
    }

    std::array<std::uint32_t, 64> w{};

    for (std::size_t offset = 0; offset < total_len; offset += 64U) {
        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t j = offset + i * 4U;
            w[i] = (static_cast<std::uint32_t>(msg[j]) << 24U)
                 | (static_cast<std::uint32_t>(msg[j + 1U]) << 16U)
                 | (static_cast<std::uint32_t>(msg[j + 2U]) << 8U)
                 | (static_cast<std::uint32_t>(msg[j + 3U]));
        }

        for (std::size_t i = 16U; i < 64U; ++i) {
            w[i] = small_sigma1(w[i - 2U]) + w[i - 7U] + small_sigma0(w[i - 15U]) + w[i - 16U];
        }

        std::uint32_t a = h[0];
        std::uint32_t b = h[1];
        std::uint32_t c = h[2];
        std::uint32_t d = h[3];
        std::uint32_t e = h[4];
        std::uint32_t f = h[5];
        std::uint32_t g = h[6];
        std::uint32_t hh = h[7];

        for (std::size_t i = 0; i < 64U; ++i) {
            const std::uint32_t t1 = hh + big_sigma1(e) + ch(e, f, g) + K[i] + w[i];
            const std::uint32_t t2 = big_sigma0(a) + maj(a, b, c);
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    Hash256 out{};
    for (std::size_t i = 0; i < 8U; ++i) {
        out[i * 4U] = static_cast<std::uint8_t>((h[i] >> 24U) & 0xFFU);
        out[i * 4U + 1U] = static_cast<std::uint8_t>((h[i] >> 16U) & 0xFFU);
        out[i * 4U + 2U] = static_cast<std::uint8_t>((h[i] >> 8U) & 0xFFU);
        out[i * 4U + 3U] = static_cast<std::uint8_t>(h[i] & 0xFFU);
    }

    return out;
}

Hash256 sha256(const std::vector<std::uint8_t>& data) {
    return sha256(data.data(), data.size());
}

Hash256 double_sha256(const std::vector<std::uint8_t>& data) {
    const Hash256 first = sha256(data);
    return sha256(first.data(), first.size());
}

} // namespace btc
