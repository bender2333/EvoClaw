#ifndef EVOCLAW_BUS_MESSAGE_BUS_H_
#define EVOCLAW_BUS_MESSAGE_BUS_H_

#include "protocol/message.h"
#include "config/config.h"
#include <expected>
#include <functional>
#include <string>

namespace evoclaw {

/// 消息总线纯虚接口
class MessageBus {
 public:
  virtual ~MessageBus() = default;

  /// 初始化连接
  virtual std::expected<void, Error> Connect(const EndpointConfig& endpoints) = 0;

  /// 发布事件（Pub-Sub）
  virtual std::expected<void, Error> Publish(const std::string& topic, const Message& msg) = 0;

  /// 订阅事件（Pub-Sub）
  virtual std::expected<void, Error> Subscribe(
      const std::string& topic,
      std::function<void(const Message&)> handler) = 0;

  /// 发送请求（DEALER-ROUTER，异步）
  virtual std::expected<void, Error> Send(const Message& msg) = 0;

  /// 注册请求处理器（DEALER-ROUTER）
  virtual std::expected<void, Error> OnRequest(
      const std::string& msg_type,
      std::function<Message(const Message&)> handler) = 0;

  /// 启动消息循环（阻塞）
  virtual void Run() = 0;

  /// 停止消息循环
  virtual void Stop() = 0;
};

}  // namespace evoclaw

#endif  // EVOCLAW_BUS_MESSAGE_BUS_H_
