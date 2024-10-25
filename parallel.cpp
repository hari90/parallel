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

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

/* Exit statuses for programs like 'env' that exec other programs.
   Copied from coreutils' system.h */
enum {
  EXIT_CANCELED = 125,      /* Internal error prior to exec attempt.  */
  EXIT_CANNOT_INVOKE = 126, /* Program located, but not usable.  */
  EXIT_ENOENT = 127         /* Could not find program to exec.  */
};

auto Split(const std::string& s, char seperator) {
  std::vector<std::string> output;
  size_t prev_pos = 0, pos = 0;
  while ((pos = s.find(seperator, pos)) != std::string::npos) {
    std::string substring(s.substr(prev_pos, pos - prev_pos));
    output.push_back(substring);
    prev_pos = ++pos;
  }
  output.push_back(s.substr(prev_pos, pos - prev_pos));  // Last word
  return output;
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
    // Child process.
    int saved_errno;

    auto cmd = Split(command, ' ');
    std::vector<char*> args;
    for (auto& arg : cmd) {
      args.push_back(&arg[0]);
    }
    args.push_back(nullptr);

    execvp(args[0], args.data());
    saved_errno = errno;

    std::cerr << "Cannot run " << args[0] << ": " << strerror(saved_errno)
              << std::endl;
    _exit(saved_errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
  }

  // Parent process.
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    std::cerr << "Failed waiting for '" << command << "': " << strerror(errno)
              << std::endl;
    exit(EXIT_CANCELED);
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <command1> <command2> ... <commandN>"
              << std::endl;
    return 1;
  }

  int parallelism = 1;
  int i = 1;
  if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--n") == 0) {
    if (i + 1 >= argc) {
      std::cerr << "Invalid arguments" << std::endl;
      return 1;
    }
    parallelism = std::stoi(argv[i + 1]);
    i += 2;
  }

  const int count = (argc - i) * parallelism;
  std::vector<std::thread> threads;
  threads.reserve(count);
  std::vector<Stats> stats(count);
  int stat_index = 0;

  for (; i < argc; ++i) {
    for (int j = 0; j < parallelism; ++j) {
      threads.emplace_back(runCommand, argv[i], std::ref(stats[stat_index++]));
    }
  }

  for (auto& thread : threads) {
    thread.join();
  }

  PrintStats(stats);

  return 0;
}