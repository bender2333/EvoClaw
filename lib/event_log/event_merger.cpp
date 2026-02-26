#include "event_log/event_merger.h"
#include "protocol/serializer.h"
#include <fstream>

namespace evoclaw {

std::expected<void, Error> EventMerger::MergeRange(
    const std::string& start_date,
    const std::string& end_date,
    const std::filesystem::path& output_path) {
  std::ofstream ofs(output_path);
  if (!ofs) {
    return std::unexpected(Error{Error::Code::kIoError,
        "Cannot open output: " + output_path.string(), "EventMerger::MergeRange"});
  }

  auto result = store_.Query(start_date + "T00:00:00Z", end_date + "T23:59:59Z");
  if (!result) return std::unexpected(result.error());

  for (const auto& event : *result) {
    ofs << Serializer::SerializeEvent(event) << '\n';
  }
  return {};
}

}  // namespace evoclaw
