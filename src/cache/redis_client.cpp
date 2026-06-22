#include "cache/redis_client.h"
#include "utils/logger.h"

#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>

namespace {
constexpr int DEFAULT_REDIS_POOL_SIZE = 10;
}

// Returns the shared Redis client singleton.
RedisClient* RedisClient::get_instance() {
    static RedisClient instance;
    return &instance;
}

// Closes Redis contexts and wakes any waiting threads during shutdown.
RedisClient::~RedisClient() {
    std::lock_guard<std::mutex> lock(mtx_);
    clear_pool();
    available_ = false;
    cv_.notify_all();
}

// Initializes the Redis connection pool using the configured host and auth settings.
bool RedisClient::init(
    const std::string& host,
    int port,
    const std::string& password,
    bool use_tls) {
    std::lock_guard<std::mutex> lock(mtx_);

    clear_pool();

    host_ = host;
    port_ = port;
    password_ = password;
    use_tls_ = use_tls;
    max_conn_ = DEFAULT_REDIS_POOL_SIZE;
    available_ = false;

    for (int i = 0; i < max_conn_; i++) {
        redisContext* context = create_connection();
        if (context == nullptr) {
            clear_pool();
            cv_.notify_all();
            return false;
        }

        conn_pool_.push(context);
    }

    available_ = true;
    cv_.notify_all();
    Logger::get_instance()->log(
        INFO,
        "Redis cache connected with pool_size=" + std::to_string(max_conn_));
    return true;
}

// Opens and validates one Redis connection, including optional TLS and auth.
redisContext* RedisClient::create_connection() {
    redisContext* context = redisConnect(host_.c_str(), port_);
    if (context == nullptr || context->err) {
        std::string error = context == nullptr
            ? "cannot allocate Redis context"
            : context->errstr;
        Logger::get_instance()->log(ERROR, "Redis connection failed: " + error);

        if (context != nullptr) {
            redisFree(context);
        }

        return nullptr;
    }

    if (use_tls_) {
        redisSSLContextError ssl_error = REDIS_SSL_CTX_NONE;
        redisSSLContext* ssl_context = redisCreateSSLContext(
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            host_.c_str(),
            &ssl_error);

        if (ssl_context == nullptr) {
            Logger::get_instance()->log(ERROR, "Redis TLS context creation failed");
            redisFree(context);
            return nullptr;
        }

        if (redisInitiateSSLWithContext(context, ssl_context) != REDIS_OK) {
            std::string error = context->errstr;
            Logger::get_instance()->log(ERROR, "Redis TLS handshake failed: " + error);
            redisFreeSSLContext(ssl_context);
            redisFree(context);
            return nullptr;
        }

        redisFreeSSLContext(ssl_context);
    }

    if (!password_.empty()) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "AUTH %s", password_.c_str()));

        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::string error = reply == nullptr ? context->errstr : reply->str;
            Logger::get_instance()->log(ERROR, "Redis auth failed: " + error);

            if (reply != nullptr) {
                freeReplyObject(reply);
            }
            redisFree(context);
            return nullptr;
        }

        freeReplyObject(reply);
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "PING"));
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        std::string error = reply == nullptr ? context->errstr : reply->str;
        Logger::get_instance()->log(ERROR, "Redis ping failed: " + error);

        if (reply != nullptr) {
            freeReplyObject(reply);
        }
        redisFree(context);
        return nullptr;
    }

    freeReplyObject(reply);
    return context;
}

// Reports whether the Redis pool currently has usable connections.
bool RedisClient::is_available() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return available_;
}

// Waits for and leases one Redis context from the pool.
redisContext* RedisClient::get_connection() {
    std::unique_lock<std::mutex> lock(mtx_);
    if (!available_) {
        return nullptr;
    }

    cv_.wait(lock, [this] {
        return !available_ || !conn_pool_.empty();
    });

    if (!available_ || conn_pool_.empty()) {
        return nullptr;
    }

    redisContext* context = conn_pool_.front();
    conn_pool_.pop();
    return context;
}

// Returns a healthy Redis context to the pool.
void RedisClient::release_connection(redisContext* context) {
    if (context == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!available_) {
            redisFree(context);
            return;
        }

        conn_pool_.push(context);
    }

    cv_.notify_one();
}

// Drops a failed Redis context and tries to replace it.
void RedisClient::discard_connection(redisContext* context) {
    if (context != nullptr) {
        redisFree(context);
    }

    redisContext* replacement = create_connection();
    if (replacement != nullptr) {
        release_connection(replacement);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        available_ = false;
        clear_pool();
    }

    cv_.notify_all();
}

// Frees every Redis context currently held by the pool.
void RedisClient::clear_pool() {
    while (!conn_pool_.empty()) {
        redisFree(conn_pool_.front());
        conn_pool_.pop();
    }
    max_conn_ = 0;
}

// Reads a string value from Redis, returning nullopt on miss or failure.
std::optional<std::string> RedisClient::get(const std::string& key) {
    redisContext* context = get_connection();
    if (context == nullptr) {
        return std::nullopt;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context, "GET %s", key.c_str()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis GET failed: " + std::string(context->errstr));
        discard_connection(context);
        return std::nullopt;
    }

    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }

    freeReplyObject(reply);
    release_connection(context);
    return result;
}

// Stores a Redis string value with a TTL.
bool RedisClient::set(const std::string& key, const std::string& value, int ttl_seconds) {
    if (ttl_seconds <= 0) {
        return false;
    }

    redisContext* context = get_connection();
    if (context == nullptr) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context, "SETEX %s %d %b", key.c_str(), ttl_seconds, value.data(), value.size()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis SETEX failed: " + std::string(context->errstr));
        discard_connection(context);
        return false;
    }

    bool ok = reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK";
    freeReplyObject(reply);
    release_connection(context);
    return ok;
}

// Deletes a Redis key from the cache.
bool RedisClient::del(const std::string& key) {
    redisContext* context = get_connection();
    if (context == nullptr) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context, "DEL %s", key.c_str()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis DEL failed: " + std::string(context->errstr));
        discard_connection(context);
        return false;
    }

    bool ok = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    release_connection(context);
    return ok;
}
