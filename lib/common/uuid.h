#ifndef EVOCLAW_COMMON_UUID_H_
#define EVOCLAW_COMMON_UUID_H_

#include <random>
#include <sstream>
#include <string>
#include <iomanip>

namespace evoclaw {

/// 生成 UUID v4
inline std::string GenerateUuid() {
  static thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

  uint32_t data[4] = {dist(gen), dist(gen), dist(gen), dist(gen)};
  // 设置版本 4 和变体位
  data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
  data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  ss << std::setw(8) << data[0] << '-';
  ss << std::setw(4) << (data[1] >> 16) << '-';
  ss << std::setw(4) << (data[1] & 0xFFFF) << '-';
  ss << std::setw(4) << (data[2] >> 16) << '-';
  ss << std::setw(4) << (data[2] & 0xFFFF);
  ss << std::setw(8) << data[3];
  return ss.str();
}

}  // namespace evoclaw

#endif  // EVOCLAW_COMMON_UUID_H_
