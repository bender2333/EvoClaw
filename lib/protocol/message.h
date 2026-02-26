#ifndef EVOCLAW_PROTOCOL_MESSAGE_H_
#define EVOCLAW_PROTOCOL_MESSAGE_H_

#include <string>

namespace evoclaw::protocol {

/// 消息信封 — Story 1.3 实现
struct Message {
  std::string msg_id;
  std::string msg_type;
  std::string source;
  std::string target;
  std::string timestamp;
  // payload — Story 1.3
};

}  // namespace evoclaw::protocol

#endif  // EVOCLAW_PROTOCOL_MESSAGE_H_
