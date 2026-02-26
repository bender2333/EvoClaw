// evoclaw-dashboard 入口
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

  {
    std::ofstream pid_file("/tmp/evoclaw-dashboard.pid");
    pid_file << getpid();
  }

  std::cout << "[dashboard] EvoClaw Dashboard started (PID: " << getpid() << ")\n";
  std::cout << "[dashboard] Web panel: http://localhost:3000\n";

  // TODO: 初始化 REST API + SSE + 静态文件服务
  while (g_running) {
    sleep(1);
  }

  std::cout << "[dashboard] Shutting down...\n";
  std::remove("/tmp/evoclaw-dashboard.pid");
  return 0;
}
