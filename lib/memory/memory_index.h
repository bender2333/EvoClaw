#ifndef EVOCLAW_MEMORY_MEMORY_INDEX_H_
#define EVOCLAW_MEMORY_MEMORY_INDEX_H_

#include "memory/memory_types.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace evoclaw {

/// 内存索引（按类型 + 关键词索引记忆条目）
/// 避免全量文件扫描，提供 O(1) 按 ID 查找和 O(n) 按类型过滤
class MemoryIndex {
 public:
  void Add(const std::string& id, MemoryType type, const std::string& summary) {
    std::lock_guard lock(mutex_);
    entries_[id] = {type, summary};
    type_index_[type].push_back(id);
  }

  void Remove(const std::string& id) {
    std::lock_guard lock(mutex_);
    if (auto it = entries_.find(id); it != entries_.end()) {
      auto type = it->second.type;
      entries_.erase(it);
      auto& ids = type_index_[type];
      ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    }
  }

  std::vector<std::string> GetByType(MemoryType type, int limit) const {
    std::lock_guard lock(mutex_);
    if (auto it = type_index_.find(type); it != type_index_.end()) {
      auto& ids = it->second;
      int count = std::min(limit, static_cast<int>(ids.size()));
      return {ids.end() - count, ids.end()};  // 最新的 N 条
    }
    return {};
  }

  std::vector<std::string> Search(const std::string& query, int limit) const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> results;
    for (const auto& [id, info] : entries_) {
      if (info.summary.find(query) != std::string::npos) {
        results.push_back(id);
        if (static_cast<int>(results.size()) >= limit) break;
      }
    }
    return results;
  }

  bool Contains(const std::string& id) const {
    std::lock_guard lock(mutex_);
    return entries_.contains(id);
  }

 private:
  struct EntryInfo {
    MemoryType type;
    std::string summary;
  };

  mutable std::mutex mutex_;
  std::unordered_map<std::string, EntryInfo> entries_;
  std::unordered_map<MemoryType, std::vector<std::string>> type_index_;
};

}  // namespace evoclaw

#endif  // EVOCLAW_MEMORY_MEMORY_INDEX_H_
