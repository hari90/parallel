/*
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

// Program that runs the provided commands in parallel

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

const auto kUsage =
    R"(./parallel [-n <parallelism count>] '<command1>' '<command2>' ...
    Each command is broken down by spaces and double quoted(") strings are treated as a single argument.
    To escape a double quote, use two double quotes("").
    To escape a single quote, use ''\''.)";

// Exit statuses for programs like 'env' that exec other programs. Copied from
// coreutils' system.h
enum {
  EXIT_CANCELED = 125,      /* Internal error prior to exec attempt.  */
  EXIT_CANNOT_INVOKE = 126, /* Program located, but not usable.  */
  EXIT_ENOENT = 127         /* Could not find program to exec.  */
};

// Split the command line arguments by separator.
// If we have a "quoted string", we will treat it as a single argument.
auto Split(const std::string& argv, char separator) {
  std::vector<std::string> result;
  std::string arg;
  bool in_quote = false;
  for (const char* p = argv.c_str(); *p; ++p) {
    if (*p == separator && !in_quote) {
      if (!arg.empty()) {
        result.push_back(arg);
        arg.clear();
      }
    } else if (*p == '"') {
      if (in_quote && *(p + 1) != separator) {
        if (*(p + 1) == '"') {
          arg.push_back(*p);
          p++;
        } else {
          result.push_back(arg);
          arg.clear();
        }
      }
      in_quote = !in_quote;
    } else {
      if ((*p != ' ' && *p != '\n') || in_quote) {
        arg.push_back(*p);
      }
    }
  }

  if (!arg.empty()) {
    result.push_back(arg);
  }

  return result;
}

struct Stats {
  bool success = false;
  long long elapsed_us = 0;
};

void runCommand(const std::string& command, Stats& stats) {
  const auto start_time = std::chrono::steady_clock::now();

  auto pid = fork();
  if (pid < 0) {
    std::cerr << "Cannot fork: " << strerror(errno) << std::endl;
    exit(EXIT_CANCELED);
  }

  if (pid == 0) {
    auto cmd = Split(command, ' ');
    std::vector<char*> args;
    for (auto& arg : cmd) {
      args.push_back(&arg[0]);
    }
    args.push_back(nullptr);
    // Child process.
    int saved_errno;

    execvp(args[0], args.data());
    saved_errno = errno;

    std::cerr << "Cannot run '" << command << "': " << strerror(saved_errno)
              << std::endl;
    _exit(saved_errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
  }

  // Parent process.
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    std::cerr << "Failed waiting for '" << command << "': " << strerror(errno)
              << std::endl;
    _exit(EXIT_CANCELED);
  }

  stats.success = true;
  stats.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - start_time)
                         .count();
}

void PrintStats(const std::vector<Stats>& stats) {
  double min = 0, avg = 0, max = 0;
  int success_count = 0;
  for (const auto& stat : stats) {
    if (stat.success) {
      if (success_count == 0) {
        min = max = avg = stat.elapsed_us;
      } else {
        min = std::min(min, (double)stat.elapsed_us);
        max = std::max(max, (double)stat.elapsed_us);
        avg += stat.elapsed_us;
      }
      success_count++;
    }
  }
  avg /= success_count;
  std::cout << "Min: " << min / 1000 << "ms" << std::endl
            << "Avg: " << avg / 1000 << "ms" << std::endl
            << "Max: " << max / 1000 << "ms" << std::endl;
}

void PrintUsageAndExit() {
  std::cerr << "Invalid arguments" << std::endl << kUsage << std::endl;
  _exit(1);
}

std::pair<std::vector<std::string>, size_t> ParseArgs(int argc, char* argv[]) {
  size_t parallelism = 1;
  std::vector<std::string> commands;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--n") == 0) {
      if (i + 1 >= argc) {
        PrintUsageAndExit();
      }
      try {
        parallelism = std::stoi(argv[i + 1]);
      } catch (const std::exception& e) {
        PrintUsageAndExit();
      }
      i++;
      continue;
    }
    commands.push_back(argv[i]);
  }

  if (commands.empty()) {
    PrintUsageAndExit();
  }

  return {commands, parallelism};
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <command1> <command2> ... <commandN>"
              << std::endl;
    return 1;
  }

  const auto [commands, parallelism] = ParseArgs(argc, argv);

  const int count = commands.size() * parallelism;
  std::vector<std::thread> threads;
  threads.reserve(count);
  std::vector<Stats> stats(count);
  int stat_index = 0;

  for (const auto command : commands) {
    for (int j = 0; j < parallelism; ++j) {
      threads.emplace_back(runCommand, command, std::ref(stats[stat_index++]));
    }
  }

  for (auto& thread : threads) {
    thread.join();
  }

  PrintStats(stats);

  _exit(0);
}