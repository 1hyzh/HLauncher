#include "auth/pkce.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr char kBase64UrlAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url_encode(const std::vector<std::uint8_t>& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index < input.size()) {
        const std::uint32_t octet_a = index < input.size() ? input[index++] : 0;
        const std::uint32_t octet_b = index < input.size() ? input[index++] : 0;
        const std::uint32_t octet_c = index < input.size() ? input[index++] : 0;
        const std::uint32_t triple = (octet_a << 16U) | (octet_b << 8U) | octet_c;

        output.push_back(kBase64UrlAlphabet[(triple >> 18U) & 0x3F]);
        output.push_back(kBase64UrlAlphabet[(triple >> 12U) & 0x3F]);
        output.push_back(kBase64UrlAlphabet[(triple >> 6U) & 0x3F]);
        output.push_back(kBase64UrlAlphabet[triple & 0x3F]);
    }

    const std::size_t remainder = input.size() % 3;
    if (remainder != 0) {
        output.resize(output.size() - (3 - remainder));
    }

    return output;
}

std::string random_urlsafe_token(std::size_t bytes) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(0, 255);

    std::vector<std::uint8_t> raw(bytes);
    for (auto& value : raw) {
        value = static_cast<std::uint8_t>(distribution(generator));
    }

    return base64url_encode(raw);
}

struct Sha256State {
    std::array<std::uint32_t, 8> h{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    std::array<std::uint8_t, 64> block{};
    std::uint64_t total_bits = 0;
    std::size_t block_size = 0;
};

constexpr std::array<std::uint32_t, 64> kSha256Constants{ {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
} };

std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

void sha256_transform(Sha256State& state) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t offset = i * 4;
        w[i] = (static_cast<std::uint32_t>(state.block[offset]) << 24U) |
               (static_cast<std::uint32_t>(state.block[offset + 1]) << 16U) |
               (static_cast<std::uint32_t>(state.block[offset + 2]) << 8U) |
               (static_cast<std::uint32_t>(state.block[offset + 3]));
    }

    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
        const std::uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state.h[0];
    std::uint32_t b = state.h[1];
    std::uint32_t c = state.h[2];
    std::uint32_t d = state.h[3];
    std::uint32_t e = state.h[4];
    std::uint32_t f = state.h[5];
    std::uint32_t g = state.h[6];
    std::uint32_t h = state.h[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const std::uint32_t ch = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 = h + s1 + ch + kSha256Constants[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state.h[0] += a;
    state.h[1] += b;
    state.h[2] += c;
    state.h[3] += d;
    state.h[4] += e;
    state.h[5] += f;
    state.h[6] += g;
    state.h[7] += h;
}

std::array<std::uint8_t, 32> sha256_digest(std::string_view input) {
    Sha256State state;
    state.total_bits = static_cast<std::uint64_t>(input.size()) * 8U;

    for (unsigned char ch : input) {
        state.block[state.block_size++] = ch;
        if (state.block_size == state.block.size()) {
            sha256_transform(state);
            state.block_size = 0;
        }
    }

    state.block[state.block_size++] = 0x80U;
    if (state.block_size > 56) {
        while (state.block_size < 64) {
            state.block[state.block_size++] = 0x00U;
        }
        sha256_transform(state);
        state.block_size = 0;
    }

    while (state.block_size < 56) {
        state.block[state.block_size++] = 0x00U;
    }

    for (int shift = 7; shift >= 0; --shift) {
        state.block[state.block_size++] = static_cast<std::uint8_t>((state.total_bits >> (shift * 8)) & 0xFFU);
    }

    sha256_transform(state);

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t i = 0; i < state.h.size(); ++i) {
        digest[i * 4] = static_cast<std::uint8_t>((state.h[i] >> 24U) & 0xFFU);
        digest[i * 4 + 1] = static_cast<std::uint8_t>((state.h[i] >> 16U) & 0xFFU);
        digest[i * 4 + 2] = static_cast<std::uint8_t>((state.h[i] >> 8U) & 0xFFU);
        digest[i * 4 + 3] = static_cast<std::uint8_t>(state.h[i] & 0xFFU);
    }

    return digest;
}

std::string base64url_encode_digest(const std::array<std::uint8_t, 32>& input) {
    std::string output;
    output.reserve(43);

    std::uint32_t accumulator = 0;
    int bits = 0;
    for (std::uint8_t byte : input) {
        accumulator = (accumulator << 8U) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            output.push_back(kBase64UrlAlphabet[(accumulator >> bits) & 0x3FU]);
        }
    }

    if (bits > 0) {
        output.push_back(kBase64UrlAlphabet[(accumulator << (6 - bits)) & 0x3FU]);
    }

    return output;
}

} // namespace

PkcePair make_pkce_pair() {
    PkcePair pair;
    pair.verifier = random_urlsafe_token(64);
    const auto digest = sha256_digest(pair.verifier);
    pair.challenge = base64url_encode_digest(digest);
    return pair;
}

std::string make_state_token() {
    return random_urlsafe_token(16);
}
