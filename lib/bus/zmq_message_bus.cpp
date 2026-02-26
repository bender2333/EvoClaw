#include "bus/zmq_message_bus.h"
#include "protocol/serializer.h"
#include <iostream>

namespace evoclaw {

ZmqMessageBus::ZmqMessageBus(const std::string& identity)
    : identity_(identity), ctx_(std::make_unique<zmq::context_t>(1)) {}

ZmqMessageBus::~ZmqMessageBus() {
  Stop();
}

std::expected<void, Error> ZmqMessageBus::Connect(const EndpointConfig& endpoints) {
  try {
    // PUB socket — 发布事件
    pub_socket_ = std::make_unique<zmq::socket_t>(*ctx_, zmq::socket_type::pub);
    pub_socket_->bind(endpoints.pub_sub);

    // SUB socket — 订阅事件
    sub_socket_ = std::make_unique<zmq::socket_t>(*ctx_, zmq::socket_type::sub);
    sub_socket_->connect(endpoints.pub_sub);

    // DEALER socket — 发送请求
    dealer_socket_ = std::make_unique<zmq::socket_t>(*ctx_, zmq::socket_type::dealer);
    dealer_socket_->set(zmq::sockopt::routing_id, identity_);

    // ROUTER socket — 接收请求
    router_socket_ = std::make_unique<zmq::socket_t>(*ctx_, zmq::socket_type::router);

    return {};
  } catch (const zmq::error_t& e) {
    return std::unexpected(Error{Error::Code::kInternal, e.what(), "ZmqMessageBus::Connect"});
  }
}

std::expected<void, Error> ZmqMessageBus::Publish(const std::string& topic, const Message& msg) {
  try {
    std::string data = Serializer::Serialize(msg);
    pub_socket_->send(zmq::buffer(topic), zmq::send_flags::sndmore);
    pub_socket_->send(zmq::buffer(data), zmq::send_flags::none);
    return {};
  } catch (const zmq::error_t& e) {
    return std::unexpected(Error{Error::Code::kInternal, e.what(), "ZmqMessageBus::Publish"});
  }
}

std::expected<void, Error> ZmqMessageBus::Subscribe(
    const std::string& topic,
    std::function<void(const Message&)> handler) {
  try {
    sub_socket_->set(zmq::sockopt::subscribe, topic);
    std::lock_guard lock(handlers_mutex_);
    sub_handlers_[topic] = std::move(handler);
    return {};
  } catch (const zmq::error_t& e) {
    return std::unexpected(Error{Error::Code::kInternal, e.what(), "ZmqMessageBus::Subscribe"});
  }
}

std::expected<void, Error> ZmqMessageBus::Send(const Message& msg) {
  try {
    std::string data = Serializer::Serialize(msg);
    dealer_socket_->send(zmq::buffer(data), zmq::send_flags::none);
    return {};
  } catch (const zmq::error_t& e) {
    return std::unexpected(Error{Error::Code::kInternal, e.what(), "ZmqMessageBus::Send"});
  }
}

std::expected<void, Error> ZmqMessageBus::OnRequest(
    const std::string& msg_type,
    std::function<Message(const Message&)> handler) {
  std::lock_guard lock(handlers_mutex_);
  req_handlers_[msg_type] = std::move(handler);
  return {};
}

void ZmqMessageBus::Run() {
  running_ = true;

  zmq::pollitem_t items[] = {
      {sub_socket_->handle(), 0, ZMQ_POLLIN, 0},
      {router_socket_->handle(), 0, ZMQ_POLLIN, 0},
  };

  while (running_) {
    zmq::poll(items, 2, std::chrono::milliseconds(100));

    // 处理 SUB 消息
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t topic_msg, data_msg;
      auto r1 = sub_socket_->recv(topic_msg, zmq::recv_flags::none);
      auto r2 = sub_socket_->recv(data_msg, zmq::recv_flags::none);
      if (r1 && r2) {
        std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
        std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
        auto msg = Serializer::Deserialize(data);
        if (msg) {
          std::lock_guard lock(handlers_mutex_);
          if (auto it = sub_handlers_.find(topic); it != sub_handlers_.end()) {
            it->second(*msg);
          }
        }
      }
    }

    // 处理 ROUTER 消息
    if (items[1].revents & ZMQ_POLLIN) {
      zmq::message_t identity_msg, data_msg;
      auto r1 = router_socket_->recv(identity_msg, zmq::recv_flags::none);
      auto r2 = router_socket_->recv(data_msg, zmq::recv_flags::none);
      if (r1 && r2) {
        std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
        auto msg = Serializer::Deserialize(data);
        if (msg) {
          std::lock_guard lock(handlers_mutex_);
          if (auto it = req_handlers_.find(msg->msg_type); it != req_handlers_.end()) {
            Message response = it->second(*msg);
            std::string resp_data = Serializer::Serialize(response);
            router_socket_->send(identity_msg, zmq::send_flags::sndmore);
            router_socket_->send(zmq::buffer(resp_data), zmq::send_flags::none);
          }
        }
      }
    }
  }
}

void ZmqMessageBus::Stop() {
  running_ = false;
}

}  // namespace evoclaw
