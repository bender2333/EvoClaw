// 集成测试：ZeroMQ 消息总线通信
// 使用 inproc:// transport 做进程内真实 ZeroMQ 通信
#include <gtest/gtest.h>
#include "bus/zmq_message_bus.h"
#include "protocol/serializer.h"
#include "common/uuid.h"
#include <thread>
#include <chrono>

using namespace evoclaw;

// 基础连通性测试：验证 ZmqMessageBus 可以创建和销毁
TEST(BusIntegrationTest, CreateAndDestroy) {
  ZmqMessageBus bus("test-node");
  // 不连接，只验证构造/析构不崩溃
}

// TODO: M1 验收时补充完整的 Pub-Sub 和 DEALER-ROUTER 集成测试
// 需要两个 bus 实例在不同线程中通信
