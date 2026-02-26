#ifndef EVOCLAW_LLM_LLM_CLIENT_H_
#define EVOCLAW_LLM_LLM_CLIENT_H_

#include "common/error.h"

namespace evoclaw::llm {

/// LLM 客户端纯虚基类 — Story 2.1 实现
class LlmClient {
 public:
  virtual ~LlmClient() = default;
};

}  // namespace evoclaw::llm

#endif  // EVOCLAW_LLM_LLM_CLIENT_H_
