#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

/*
 * Tiny child process used by the execution tests. It avoids depending on a
 * system Python/Node installation so the suite runs on a bare machine.
 */
int main(int argc, char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  if (argc < 2) {
    std::fputs("missing mode\n", stderr);
    return 2;
  }

  const std::string mode = argv[1];
  if (mode == "echo") {
    std::fputs("out:hello\n", stdout);
    std::fputs("err:hello\n", stderr);
    std::fflush(stdout);
    std::fflush(stderr);
    return 0;
  }
  if (mode == "exit") {
    return argc > 2 ? std::atoi(argv[2]) : 0;
  }
  if (mode == "stdin") {
    std::string line;
    while (std::getline(std::cin, line)) {
      std::printf("got:%s\n", line.c_str());
      std::fflush(stdout);
    }
    return 0;
  }
  if (mode == "sleep") {
    const int milliseconds = argc > 2 ? std::atoi(argv[2]) : 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
  }
  if (mode == "env") {
    const char* name = argc > 2 ? argv[2] : "";
    const char* value = std::getenv(name);
    std::printf("%s=%s\n", name, value ? value : "<unset>");
    std::fflush(stdout);
    return 0;
  }
  if (mode == "cwd") {
    char buffer[4096];
#ifdef _WIN32
    if (!_getcwd(buffer, sizeof(buffer))) {
#else
    if (!getcwd(buffer, sizeof(buffer))) {
#endif
      return 4;
    }
    std::printf("%s\n", buffer);
    std::fflush(stdout);
    return 0;
  }

  std::fputs("unknown mode\n", stderr);
  return 3;
}
