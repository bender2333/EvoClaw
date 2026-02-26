#ifndef EVOCLAW_BUS_ZMQ_MESSAGE_BUS_H_
#define EVOCLAW_BUS_ZMQ_MESSAGE_BUS_H_

#include "bus/message_bus.h"
#include <zmq.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace evoclaw {

/// ZeroMQ 消息总线实现
/// Pub-Sub（事件广播）+ DEALER-ROUTER（请求响应）
class ZmqMessageBus : public MessageBus {
 public:
  explicit ZmqMessageBus(const std::string& identity);
  ~ZmqMessageBus() override;

  std::expected<void, Error> Connect(const EndpointConfig& endpoints) override;
  std::expected<void, Error> Publish(const std::string& topic, const Message& msg) override;
  std::expected<void, Error> Subscribe(
      const std::string& topic,
      std::function<void(const Message&)> handler) override;
  std::expected<void, Error> Send(const Message& msg) override;
  std::expected<void, Error> OnRequest(
      const std::string& msg_type,
      std::function<Message(const Message&)> handler) override;
  void Run() override;
  void Stop() override;

 private:
  std::string identity_;
  std::unique_ptr<zmq::context_t> ctx_;
  std::unique_ptr<zmq::socket_t> pub_socket_;
  std::unique_ptr<zmq::socket_t> sub_socket_;
  std::unique_ptr<zmq::socket_t> dealer_socket_;
  std::unique_ptr<zmq::socket_t> router_socket_;

  std::unordered_map<std::string, std::function<void(const Message&)>> sub_handlers_;
  std::unordered_map<std::string, std::function<Message(const Message&)>> req_handlers_;
  std::mutex handlers_mutex_;
  std::atomic<bool> running_{false};
};

}  // namespace evoclaw

#endif  // EVOCLAW_BUS_ZMQ_MESSAGE_BUS_H_
