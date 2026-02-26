#ifndef EVOCLAW_MEMORY_MEMORY_STORE_H_
#define EVOCLAW_MEMORY_MEMORY_STORE_H_

#include "memory/memory_types.h"
#include "config/config.h"
#include <expected>
#include <string>
#include <vector>

namespace evoclaw {

/// 记忆存储纯虚接口
class MemoryStore {
 public:
  virtual ~MemoryStore() = default;

  /// 存储记忆条目
  virtual std::expected<void, Error> Store(const MemoryEntry& entry) = 0;

  /// 按 ID 获取
  virtual std::expected<MemoryEntry, Error> Get(const std::string& id) = 0;

  /// 按类型查询
  virtual std::expected<std::vector<MemoryEntry>, Error> QueryByType(
      MemoryType type, int limit = 50) = 0;

  /// 按关键词搜索（摘要匹配）
  virtual std::expected<std::vector<MemoryEntry>, Error> Search(
      const std::string& query, int limit = 10) = 0;

  /// 更新记忆条目
  virtual std::expected<void, Error> Update(const MemoryEntry& entry) = 0;

  /// 删除记忆条目
  virtual std::expected<void, Error> Delete(const std::string& id) = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_MEMORY_MEMORY_STORE_H_
