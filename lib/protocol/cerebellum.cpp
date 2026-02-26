#include "protocol/cerebellum.h"
#include "protocol/serializer.h"
#include "common/uuid.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace evoclaw {

Cerebellum::Cerebellum(std::shared_ptr<RuleEngine> rule_engine)
    : rule_engine_(std::move(rule_engine)) {}

std::expected<Message, Error> Cerebellum::PreProcess(Message msg) {
  // 填充 msg_id
  if (msg.msg_id.empty()) {
    msg.msg_id = GenerateUuid();
  }

  // 填充 timestamp
  if (msg.timestamp.empty()) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&time_t_now, &tm_buf);
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%FT%TZ");
    msg.timestamp = ss.str();
  }

  // 形式约束验证（如果 payload 中包含 _constraints 字段）
  if (rule_engine_ && msg.payload.contains("_constraints")) {
    auto result = rule_engine_->Validate(msg.payload, msg.payload["_constraints"]);
    if (!result) return std::unexpected(result.error());
    if (!result->passed) {
      return std::unexpected(Error{
          Error::Code::kInvalidArg,
          "Formal constraint violation: " + result->reason,
          "Cerebellum::PreProcess"});
    }
  }

  return msg;
}

std::expected<Message, Error> Cerebellum::PostProcess(const std::string& raw) {
  auto msg = Serializer::Deserialize(raw);
  if (!msg) return msg;

  // 接收端约束验证
  if (rule_engine_ && msg->payload.contains("_constraints")) {
    auto result = rule_engine_->Validate(msg->payload, msg->payload["_constraints"]);
    if (!result) return std::unexpected(result.error());
    if (!result->passed) {
      return std::unexpected(Error{
          Error::Code::kInvalidArg,
          "Received message failed constraint validation: " + result->reason,
          "Cerebellum::PostProcess"});
    }
  }

  return msg;
}

}  // namespace evoclaw
