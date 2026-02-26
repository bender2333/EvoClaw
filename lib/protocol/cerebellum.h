#ifndef EVOCLAW_PROTOCOL_CEREBELLUM_H_
#define EVOCLAW_PROTOCOL_CEREBELLUM_H_

#include "protocol/message.h"
#include "protocol/rule_engine.h"
#include "config/config.h"
#include <expected>
#include <memory>
#include <vector>

namespace evoclaw {

/// 小脑：消息预处理管线（Cortex 通信子系统）
/// 职责：消息打包、形式约束验证、冲突检测、摘要压缩
class Cerebellum {
 public:
  explicit Cerebellum(std::shared_ptr<RuleEngine> rule_engine);

  /// 发送前预处理：填充 msg_id/timestamp，验证形式约束
  std::expected<Message, Error> PreProcess(Message msg);

  /// 接收后处理：反序列化验证、约束检查
  std::expected<Message, Error> PostProcess(const std::string& raw);

 private:
  std::shared_ptr<RuleEngine> rule_engine_;
};

}  // namespace evoclaw

#endif  // EVOCLAW_PROTOCOL_CEREBELLUM_H_
