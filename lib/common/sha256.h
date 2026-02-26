#ifndef EVOCLAW_COMMON_SHA256_H_
#define EVOCLAW_COMMON_SHA256_H_

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace evoclaw {

/// 纯 C++ SHA256 实现（无外部依赖）
class Sha256 {
 public:
  static std::string Hash(const std::string& data) {
    Sha256 ctx;
    ctx.Update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    auto digest = ctx.Final();
    std::ostringstream ss;
    for (auto b : digest) ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    return ss.str();
  }

 private:
  std::array<uint32_t, 8> state_;
  std::array<uint8_t, 64> buffer_;
  uint64_t total_len_ = 0;
  size_t buf_len_ = 0;

  static constexpr std::array<uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  };

  static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  static uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
  static uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static uint32_t Sigma0(uint32_t x) { return Rotr(x, 2) ^ Rotr(x, 13) ^ Rotr(x, 22); }
  static uint32_t Sigma1(uint32_t x) { return Rotr(x, 6) ^ Rotr(x, 11) ^ Rotr(x, 25); }
  static uint32_t Gamma0(uint32_t x) { return Rotr(x, 7) ^ Rotr(x, 18) ^ (x >> 3); }
  static uint32_t Gamma1(uint32_t x) { return Rotr(x, 17) ^ Rotr(x, 19) ^ (x >> 10); }

  Sha256() {
    state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
              0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  }

  void Transform(const uint8_t* block) {
    std::array<uint32_t, 64> w;
    for (int i = 0; i < 16; ++i) {
      w[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
             (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
      w[i] = Gamma1(w[i-2]) + w[i-7] + Gamma0(w[i-15]) + w[i-16];
    }

    auto [a, b, c, d, e, f, g, h] = state_;
    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = h + Sigma1(e) + Ch(e, f, g) + kK[i] + w[i];
      uint32_t t2 = Sigma0(a) + Maj(a, b, c);
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
  }

  void Update(const uint8_t* data, size_t len) {
    total_len_ += len;
    while (len > 0) {
      size_t copy = std::min(len, 64 - buf_len_);
      std::memcpy(buffer_.data() + buf_len_, data, copy);
      buf_len_ += copy;
      data += copy;
      len -= copy;
      if (buf_len_ == 64) { Transform(buffer_.data()); buf_len_ = 0; }
    }
  }

  std::array<uint8_t, 32> Final() {
    uint64_t bit_len = total_len_ * 8;
    buffer_[buf_len_++] = 0x80;
    if (buf_len_ > 56) {
      while (buf_len_ < 64) buffer_[buf_len_++] = 0;
      Transform(buffer_.data());
      buf_len_ = 0;
    }
    while (buf_len_ < 56) buffer_[buf_len_++] = 0;
    for (int i = 7; i >= 0; --i) buffer_[buf_len_++] = uint8_t(bit_len >> (i * 8));
    Transform(buffer_.data());

    std::array<uint8_t, 32> digest;
    for (int i = 0; i < 8; ++i) {
      digest[i*4]   = uint8_t(state_[i] >> 24);
      digest[i*4+1] = uint8_t(state_[i] >> 16);
      digest[i*4+2] = uint8_t(state_[i] >> 8);
      digest[i*4+3] = uint8_t(state_[i]);
    }
    return digest;
  }
};

}  // namespace evoclaw

#endif  // EVOCLAW_COMMON_SHA256_H_
