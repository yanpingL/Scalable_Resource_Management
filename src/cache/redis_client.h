#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <optional>
#include <string>
#include <mutex>

struct redisContext;

class RedisClient {
public:
    static RedisClient* get_instance();
    ~RedisClient();

    bool init(
        const std::string& host,
        int port,
        const std::string& password = "",
        bool use_tls = false);
    bool is_available() const;

    std::optional<std::string> get(const std::string& key);
    bool set(const std::string& key, const std::string& value, int ttl_seconds);
    bool del(const std::string& key);

private:
    RedisClient() = default;

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    std::string host_;
    int port_ = 6379;
    std::string password_;
    bool use_tls_ = false;
    bool available_ = false;
    redisContext* context_ = nullptr;
    mutable std::mutex mtx_;
};

#endif
