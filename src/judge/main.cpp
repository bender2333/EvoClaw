// evoclaw-judge 入口（Phase 2 stub）
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
    std::ofstream pid_file("/tmp/evoclaw-judge.pid");
    pid_file << getpid();
  }

  std::cout << "[judge] EvoClaw Meta Judge started (PID: " << getpid() << ") [Phase 2 stub]\n";

  while (g_running) {
    sleep(1);
  }

  std::cout << "[judge] Shutting down...\n";
  std::remove("/tmp/evoclaw-judge.pid");
  return 0;
}
