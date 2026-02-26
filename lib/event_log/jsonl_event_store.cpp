#include "event_log/jsonl_event_store.h"
#include "protocol/serializer.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace evoclaw {

JsonlEventStore::JsonlEventStore(const std::filesystem::path& data_dir)
    : data_dir_(data_dir / "events") {
  std::filesystem::create_directories(data_dir_);
}

std::filesystem::path JsonlEventStore::GetFilePath(const std::string& date) const {
  return data_dir_ / (date + ".jsonl");
}

std::string JsonlEventStore::TodayDate() const {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  gmtime_r(&time_t_now, &tm_buf);
  std::ostringstream ss;
  ss << std::put_time(&tm_buf, "%F");
  return ss.str();
}

std::expected<void, Error> JsonlEventStore::Append(const Event& event) {
  std::lock_guard lock(write_mutex_);
  auto path = GetFilePath(TodayDate());
  std::ofstream ofs(path, std::ios::app);
  if (!ofs) {
    return std::unexpected(Error{Error::Code::kIoError, "Cannot open " + path.string(), "JsonlEventStore::Append"});
  }
  ofs << Serializer::SerializeEvent(event) << '\n';
  return {};
}

std::expected<std::vector<Event>, Error> JsonlEventStore::ReadFile(
    const std::filesystem::path& path) const {
  std::vector<Event> events;
  std::ifstream ifs(path);
  if (!ifs) return events;  // 文件不存在返回空

  std::string line;
  while (std::getline(ifs, line)) {
    if (line.empty()) continue;
    auto event = Serializer::DeserializeEvent(line);
    if (event) events.push_back(std::move(*event));
  }
  return events;
}

std::expected<std::vector<Event>, Error> JsonlEventStore::Query(
    const std::string& start_time, const std::string& end_time) {
  std::vector<Event> result;
  // 提取日期部分（YYYY-MM-DD）
  std::string start_date = start_time.substr(0, 10);
  std::string end_date = end_time.substr(0, 10);

  for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
    if (!entry.is_regular_file()) continue;
    std::string filename = entry.path().stem().string();
    if (filename >= start_date && filename <= end_date) {
      auto events = ReadFile(entry.path());
      if (events) {
        for (auto& e : *events) {
          if (e.timestamp >= start_time && e.timestamp <= end_time) {
            result.push_back(std::move(e));
          }
        }
      }
    }
  }
  return result;
}

std::expected<std::vector<Event>, Error> JsonlEventStore::QueryByType(
    const std::string& event_type, int limit) {
  std::vector<Event> result;
  // 从最新文件开始反向遍历
  std::vector<std::filesystem::path> files;
  if (std::filesystem::exists(data_dir_)) {
    for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
      if (entry.is_regular_file()) files.push_back(entry.path());
    }
  }
  std::sort(files.rbegin(), files.rend());

  for (const auto& path : files) {
    auto events = ReadFile(path);
    if (events) {
      for (auto& e : *events) {
        if (e.event_type == event_type) {
          result.push_back(std::move(e));
          if (static_cast<int>(result.size()) >= limit) return result;
        }
      }
    }
  }
  return result;
}

std::expected<Event, Error> JsonlEventStore::GetById(const std::string& event_id) {
  if (!std::filesystem::exists(data_dir_)) {
    return std::unexpected(Error{Error::Code::kNotFound, "Event not found: " + event_id, "JsonlEventStore::GetById"});
  }
  for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
    if (!entry.is_regular_file()) continue;
    auto events = ReadFile(entry.path());
    if (events) {
      for (auto& e : *events) {
        if (e.event_id == event_id) return e;
      }
    }
  }
  return std::unexpected(Error{Error::Code::kNotFound, "Event not found: " + event_id, "JsonlEventStore::GetById"});
}

std::expected<void, Error> JsonlEventStore::ForEach(
    std::function<bool(const Event&)> callback) {
  if (!std::filesystem::exists(data_dir_)) return {};
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
    if (entry.is_regular_file()) files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  for (const auto& path : files) {
    auto events = ReadFile(path);
    if (events) {
      for (const auto& e : *events) {
        if (!callback(e)) return {};
      }
    }
  }
  return {};
}

}  // namespace evoclaw
