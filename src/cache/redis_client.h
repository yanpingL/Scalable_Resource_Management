#ifndef REDIS_CLIENT_H
#define REDIS_CLIENT_H

#include <optional>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

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

    redisContext* create_connection();
    redisContext* get_connection();
    void release_connection(redisContext* context);
    void discard_connection(redisContext* context);
    void clear_pool();

    std::string host_;
    int port_ = 6379;
    std::string password_;
    bool use_tls_ = false;
    bool available_ = false;
    int max_conn_ = 0;
    std::queue<redisContext*> conn_pool_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
};

#endif
