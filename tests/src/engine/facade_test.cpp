#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "facade/facade.h"

using namespace evoclaw;

// ── Mock LlmClient ──
class MockLlmClient : public LlmClient {
 public:
  MOCK_METHOD((std::expected<ChatResponse, Error>), Chat,
              (const std::vector<ChatMessage>&, const nlohmann::json&), (override));
  MOCK_METHOD((std::expected<ChatResponse, Error>), ChatJson,
              (const std::vector<ChatMessage>&, const nlohmann::json&), (override));
};

// ── Mock MessageBus ──
class MockMessageBus : public MessageBus {
 public:
  MOCK_METHOD((std::expected<void, Error>), Connect, (const EndpointConfig&), (override));
  MOCK_METHOD((std::expected<void, Error>), Publish,
              (const std::string&, const Message&), (override));
  MOCK_METHOD((std::expected<void, Error>), Subscribe,
              (const std::string&, std::function<void(const Message&)>), (override));
  MOCK_METHOD((std::expected<void, Error>), Send, (const Message&), (override));
  MOCK_METHOD((std::expected<void, Error>), OnRequest,
              (const std::string&, std::function<Message(const Message&)>), (override));
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

TEST(FacadeTest, HandleUserMessage) {
  MockLlmClient llm;
  MockMessageBus bus;

  ChatResponse resp;
  resp.content = "你好！有什么可以帮你的？";
  resp.total_tokens = 50;

  EXPECT_CALL(llm, Chat(testing::_, testing::_))
      .WillOnce(testing::Return(resp));
  EXPECT_CALL(bus, Publish(testing::_, testing::_))
      .WillOnce(testing::Return(std::expected<void, Error>{}));

  Facade facade(&llm, &bus, nullptr);
  auto result = facade.HandleUserMessage("user_1", "你好");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "你好！有什么可以帮你的？");
}

TEST(FacadeTest, SessionContext) {
  MockLlmClient llm;

  ChatResponse resp;
  resp.content = "回复内容";

  EXPECT_CALL(llm, Chat(testing::_, testing::_))
      .WillRepeatedly(testing::Return(resp));

  Facade facade(&llm, nullptr, nullptr);
  facade.HandleUserMessage("user_1", "消息1");
  facade.HandleUserMessage("user_1", "消息2");

  auto ctx = facade.GetSessionContext("user_1");
  EXPECT_EQ(ctx["message_count"], 4);  // 2 user + 2 assistant
}

TEST(FacadeTest, EmptySessionContext) {
  Facade facade(nullptr, nullptr, nullptr);
  auto ctx = facade.GetSessionContext("nonexistent");
  EXPECT_EQ(ctx["message_count"], 0);
}

TEST(FacadeTest, ActiveSessionCount) {
  MockLlmClient llm;

  ChatResponse resp;
  resp.content = "ok";

  EXPECT_CALL(llm, Chat(testing::_, testing::_))
      .WillRepeatedly(testing::Return(resp));

  Facade facade(&llm, nullptr, nullptr);
  EXPECT_EQ(facade.GetActiveSessionCount(), 0u);

  facade.HandleUserMessage("user_1", "hi");
  facade.HandleUserMessage("user_2", "hello");
  EXPECT_EQ(facade.GetActiveSessionCount(), 2u);
}

TEST(FacadeTest, LlmError) {
  MockLlmClient llm;

  EXPECT_CALL(llm, Chat(testing::_, testing::_))
      .WillOnce(testing::Return(std::unexpected(
          Error{Error::Code::kInternal, "LLM unavailable", "test"})));

  Facade facade(&llm, nullptr, nullptr);
  auto result = facade.HandleUserMessage("user_1", "test");

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, Error::Code::kInternal);
}

TEST(FacadeTest, FormatResponse) {
  MockLlmClient llm;

  ChatResponse resp;
  resp.content = "格式化后的回复";

  EXPECT_CALL(llm, Chat(testing::_, testing::_))
      .WillOnce(testing::Return(resp));

  Facade facade(&llm, nullptr, nullptr);
  auto result = facade.FormatResponse("原始系统输出");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "格式化后的回复");
}
