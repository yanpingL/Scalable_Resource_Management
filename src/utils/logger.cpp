#include "logger.h"
#include <iostream>
#include <ctime>
#include <utility>

Logger::Logger()
    : running(true),
      worker(&Logger::worker_loop, this) {}

// Returns the shared logger instance.
Logger* Logger::get_instance() {
    static Logger instance;
    return &instance;
}

// Flushes queued log entries before shutdown.
Logger::~Logger() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();

    if (worker.joinable()) {
        worker.join();
    }
}

// Formats the current local time for log entries.
std::string get_time() {
    time_t now = time(nullptr);
    tm local_tm{};
    char buf[64];
    localtime_r(&now, &local_tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buf);
}

// Converts a log level enum into display text.
std::string level_to_string(LogLevel level) {
    switch(level) {
        case INFO: return "INFO";
        case ERROR: return "ERROR";
        case DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void Logger::worker_loop() {
    while (true) {
        std::list<std::string> pending;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] {
                return !running || !log_queue.empty();
            });

            if (!running && log_queue.empty()) {
                break;
            }

            pending.splice(pending.end(), log_queue);
        }

        for (const std::string& log_msg : pending) {
            std::cerr << log_msg << '\n';
        }
    }
}

// Queues one log line for the async logger thread.
void Logger::log(LogLevel level, const std::string& message) {
    std::string log_msg =
        "[" + get_time() + "] [" +
        level_to_string(level) + "] " +
        message;

    {
        std::lock_guard<std::mutex> lock(mtx);
        log_queue.push_back(std::move(log_msg));
    }

    cv.notify_one();
}
