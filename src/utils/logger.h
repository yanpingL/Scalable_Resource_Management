#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

enum LogLevel {
    INFO,
    ERROR,
    DEBUG
};

class Logger {
public:
    static Logger* get_instance();

    void init(const std::string& filename);

    void log(LogLevel level, const std::string& message);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void worker_loop();

    std::ofstream log_file;
    std::mutex mtx;
    std::condition_variable cv;
    std::list<std::string> log_queue;
    bool running;
    std::thread worker;
};

#endif
