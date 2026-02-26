#ifndef EVOCLAW_MEMORY_MEMORY_TYPES_H_
#define EVOCLAW_MEMORY_MEMORY_TYPES_H_

#include <string>
#include <nlohmann/json.hpp>

namespace evoclaw {

/// 记忆类型枚举
enum class MemoryType {
  kWorking,    // 工作记忆（当前会话上下文）
  kEpisodic,   // 情景记忆（历史任务记录）
  kSemantic,   // 语义记忆（提炼的知识）
  kUserModel,  // 用户模型（结构化偏好）
  kChallenge,  // 挑战集（失败/边界案例）
};

/// 记忆条目
struct MemoryEntry {
  std::string id;
  MemoryType type;
  std::string timestamp;
  std::string summary;        // 摘要
  nlohmann::json content;     // 完整内容
  nlohmann::json metadata;    // 元数据（标签、关联任务等）
  int access_count = 0;       // 访问次数（用于 TTL 和蒸馏决策）
};

inline std::string MemoryTypeToString(MemoryType type) {
  switch (type) {
    case MemoryType::kWorking: return "working";
    case MemoryType::kEpisodic: return "episodic";
    case MemoryType::kSemantic: return "semantic";
    case MemoryType::kUserModel: return "user_model";
    case MemoryType::kChallenge: return "challenge";
  }
  return "unknown";
}

inline MemoryType StringToMemoryType(const std::string& s) {
  if (s == "working") return MemoryType::kWorking;
  if (s == "episodic") return MemoryType::kEpisodic;
  if (s == "semantic") return MemoryType::kSemantic;
  if (s == "user_model") return MemoryType::kUserModel;
  if (s == "challenge") return MemoryType::kChallenge;
  return MemoryType::kWorking;
}

}  // namespace evoclaw

#endif  // EVOCLAW_MEMORY_MEMORY_TYPES_H_
