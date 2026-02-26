// evoclaw-compiler 入口
#include <iostream>
#include <csignal>
#include <fstream>
#include <unistd.h>

namespace {
volatile sig_atomic_t g_running = 1;
void SignalHandler(int) { g_running = 0; }
}

int main(int argc, char* argv[]) {
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  std::string config_path = "config/evoclaw.yaml";
  if (argc > 1) config_path = argv[1];

  // 写入 PID 文件
  {
    std::ofstream pid_file("/tmp/evoclaw-compiler.pid");
    pid_file << getpid();
  }

  std::cout << "[compiler] EvoClaw Compiler started (PID: " << getpid() << ")\n";

  // TODO: 初始化配置、消息总线、Observer、Evolver、Compiler
  while (g_running) {
    sleep(1);
  }

  std::cout << "[compiler] Shutting down...\n";
  std::remove("/tmp/evoclaw-compiler.pid");
  return 0;
}
