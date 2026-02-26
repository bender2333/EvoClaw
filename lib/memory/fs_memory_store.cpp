#include "memory/fs_memory_store.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace evoclaw {

FsMemoryStore::FsMemoryStore(const std::filesystem::path& data_dir)
    : data_dir_(data_dir / "memory") {
  std::filesystem::create_directories(data_dir_);

  // 启动时重建索引
  if (std::filesystem::exists(data_dir_)) {
    for (const auto& type_dir : std::filesystem::directory_iterator(data_dir_)) {
      if (!type_dir.is_directory()) continue;
      for (const auto& date_dir : std::filesystem::directory_iterator(type_dir.path())) {
        if (!date_dir.is_directory()) continue;
        for (const auto& file : std::filesystem::directory_iterator(date_dir.path())) {
          if (file.path().extension() == ".json") {
            auto entry = ReadEntry(file.path());
            if (entry) {
              index_.Add(entry->id, entry->type, entry->summary);
            }
          }
        }
      }
    }
  }
}

std::filesystem::path FsMemoryStore::EntryPath(const MemoryEntry& entry) const {
  std::string date = entry.timestamp.substr(0, 10);  // YYYY-MM-DD
  return data_dir_ / MemoryTypeToString(entry.type) / date / (entry.id + ".json");
}

std::expected<MemoryEntry, Error> FsMemoryStore::ReadEntry(const std::filesystem::path& path) const {
  try {
    std::ifstream ifs(path);
    if (!ifs) return std::unexpected(Error{Error::Code::kNotFound, "File not found: " + path.string(), "FsMemoryStore::ReadEntry"});
    auto j = nlohmann::json::parse(ifs);
    MemoryEntry entry;
    entry.id = j.at("id").get<std::string>();
    entry.type = StringToMemoryType(j.at("type").get<std::string>());
    entry.timestamp = j.at("timestamp").get<std::string>();
    entry.summary = j.value("summary", "");
    entry.content = j.value("content", nlohmann::json::object());
    entry.metadata = j.value("metadata", nlohmann::json::object());
    entry.access_count = j.value("access_count", 0);
    return entry;
  } catch (const std::exception& e) {
    return std::unexpected(Error{Error::Code::kInvalidArg, e.what(), "FsMemoryStore::ReadEntry"});
  }
}

std::expected<void, Error> FsMemoryStore::WriteEntry(const std::filesystem::path& path, const MemoryEntry& entry) {
  std::filesystem::create_directories(path.parent_path());
  nlohmann::json j;
  j["id"] = entry.id;
  j["type"] = MemoryTypeToString(entry.type);
  j["timestamp"] = entry.timestamp;
  j["summary"] = entry.summary;
  j["content"] = entry.content;
  j["metadata"] = entry.metadata;
  j["access_count"] = entry.access_count;

  std::ofstream ofs(path);
  if (!ofs) return std::unexpected(Error{Error::Code::kIoError, "Cannot write: " + path.string(), "FsMemoryStore::WriteEntry"});
  ofs << j.dump(2);
  return {};
}

std::expected<void, Error> FsMemoryStore::Store(const MemoryEntry& entry) {
  std::lock_guard lock(mutex_);
  auto result = WriteEntry(EntryPath(entry), entry);
  if (result) index_.Add(entry.id, entry.type, entry.summary);
  return result;
}

std::expected<MemoryEntry, Error> FsMemoryStore::GetLocked(const std::string& id) const {
  // 内部无锁版本——调用方已持有 mutex_
  if (!index_.Contains(id)) {
    return std::unexpected(Error{Error::Code::kNotFound, "Memory not found: " + id, "FsMemoryStore::GetLocked"});
  }
  for (const auto& type_dir : std::filesystem::directory_iterator(data_dir_)) {
    if (!type_dir.is_directory()) continue;
    for (const auto& date_dir : std::filesystem::directory_iterator(type_dir.path())) {
      if (!date_dir.is_directory()) continue;
      auto path = date_dir.path() / (id + ".json");
      if (std::filesystem::exists(path)) return ReadEntry(path);
    }
  }
  return std::unexpected(Error{Error::Code::kNotFound, "Memory not found: " + id, "FsMemoryStore::GetLocked"});
}

std::expected<MemoryEntry, Error> FsMemoryStore::Get(const std::string& id) {
  std::lock_guard lock(mutex_);
  return GetLocked(id);
}

std::expected<std::vector<MemoryEntry>, Error> FsMemoryStore::QueryByType(MemoryType type, int limit) {
  std::lock_guard lock(mutex_);
  auto ids = index_.GetByType(type, limit);
  std::vector<MemoryEntry> result;
  for (const auto& id : ids) {
    auto entry = GetLocked(id);
    if (entry) result.push_back(std::move(*entry));
  }
  return result;
}

std::expected<std::vector<MemoryEntry>, Error> FsMemoryStore::Search(const std::string& query, int limit) {
  std::lock_guard lock(mutex_);
  auto ids = index_.Search(query, limit);
  std::vector<MemoryEntry> result;
  for (const auto& id : ids) {
    auto entry = GetLocked(id);
    if (entry) result.push_back(std::move(*entry));
  }
  return result;
}

std::expected<void, Error> FsMemoryStore::Update(const MemoryEntry& entry) {
  std::lock_guard lock(mutex_);
  auto result = WriteEntry(EntryPath(entry), entry);
  if (result) index_.Add(entry.id, entry.type, entry.summary);
  return result;
}

std::expected<void, Error> FsMemoryStore::Delete(const std::string& id) {
  std::lock_guard lock(mutex_);
  index_.Remove(id);
  for (const auto& type_dir : std::filesystem::directory_iterator(data_dir_)) {
    if (!type_dir.is_directory()) continue;
    for (const auto& date_dir : std::filesystem::directory_iterator(type_dir.path())) {
      if (!date_dir.is_directory()) continue;
      auto path = date_dir.path() / (id + ".json");
      if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
        return {};
      }
    }
  }
  return {};
}

}  // namespace evoclaw
