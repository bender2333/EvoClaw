#ifndef EVOCLAW_MEMORY_FS_MEMORY_STORE_H_
#define EVOCLAW_MEMORY_FS_MEMORY_STORE_H_

#include "memory/memory_store.h"
#include "memory/memory_index.h"
#include <filesystem>
#include <mutex>

namespace evoclaw {

/// 文件系统记忆存储实现
/// 按类型和日期组织目录：data/memory/{type}/{date}/{id}.json
class FsMemoryStore : public MemoryStore {
 public:
  explicit FsMemoryStore(const std::filesystem::path& data_dir);

  std::expected<void, Error> Store(const MemoryEntry& entry) override;
  std::expected<MemoryEntry, Error> Get(const std::string& id) override;
  std::expected<std::vector<MemoryEntry>, Error> QueryByType(
      MemoryType type, int limit) override;
  std::expected<std::vector<MemoryEntry>, Error> Search(
      const std::string& query, int limit) override;
  std::expected<void, Error> Update(const MemoryEntry& entry) override;
  std::expected<void, Error> Delete(const std::string& id) override;

 private:
  std::filesystem::path data_dir_;
  MemoryIndex index_;
  std::mutex mutex_;

  std::filesystem::path EntryPath(const MemoryEntry& entry) const;
  std::expected<MemoryEntry, Error> ReadEntry(const std::filesystem::path& path) const;
  std::expected<void, Error> WriteEntry(const std::filesystem::path& path, const MemoryEntry& entry);
  // 内部无锁版本（调用方已持有 mutex_）
  std::expected<MemoryEntry, Error> GetLocked(const std::string& id) const;
};

}  // namespace evoclaw

#endif  // EVOCLAW_MEMORY_FS_MEMORY_STORE_H_
