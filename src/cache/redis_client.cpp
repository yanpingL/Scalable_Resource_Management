#include "cache/redis_client.h"
#include "utils/logger.h"

#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>

RedisClient* RedisClient::get_instance() {
    static RedisClient instance;
    return &instance;
}

RedisClient::~RedisClient() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::init(
    const std::string& host,
    int port,
    const std::string& password,
    bool use_tls) {
    std::lock_guard<std::mutex> lock(mtx_);

    host_ = host;
    port_ = port;
    password_ = password;
    use_tls_ = use_tls;

    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }

    context_ = redisConnect(host_.c_str(), port_);
    if (context_ == nullptr || context_->err) {
        std::string error = context_ == nullptr
            ? "cannot allocate Redis context"
            : context_->errstr;
        Logger::get_instance()->log(ERROR, "Redis connection failed: " + error);

        if (context_ != nullptr) {
            redisFree(context_);
            context_ = nullptr;
        }

        available_ = false;
        return false;
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
            redisFree(context_);
            context_ = nullptr;
            available_ = false;
            return false;
        }

        if (redisInitiateSSLWithContext(context_, ssl_context) != REDIS_OK) {
            std::string error = context_->errstr;
            Logger::get_instance()->log(ERROR, "Redis TLS handshake failed: " + error);
            redisFreeSSLContext(ssl_context);
            redisFree(context_);
            context_ = nullptr;
            available_ = false;
            return false;
        }

        redisFreeSSLContext(ssl_context);
    }

    if (!password_.empty()) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context_, "AUTH %s", password_.c_str()));

        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::string error = reply == nullptr ? context_->errstr : reply->str;
            Logger::get_instance()->log(ERROR, "Redis auth failed: " + error);

            if (reply != nullptr) {
                freeReplyObject(reply);
            }
            redisFree(context_);
            context_ = nullptr;
            available_ = false;
            return false;
        }

        freeReplyObject(reply);
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(context_, "PING"));
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
        std::string error = reply == nullptr ? context_->errstr : reply->str;
        Logger::get_instance()->log(ERROR, "Redis ping failed: " + error);

        if (reply != nullptr) {
            freeReplyObject(reply);
        }
        redisFree(context_);
        context_ = nullptr;
        available_ = false;
        return false;
    }

    freeReplyObject(reply);
    available_ = true;
    Logger::get_instance()->log(INFO, "Redis cache connected");
    return available_;
}

bool RedisClient::is_available() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return available_;
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!available_ || context_ == nullptr) {
        return std::nullopt;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "GET %s", key.c_str()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis GET failed: " + std::string(context_->errstr));
        available_ = false;
        return std::nullopt;
    }

    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return result;
}

bool RedisClient::set(const std::string& key, const std::string& value, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!available_ || context_ == nullptr || ttl_seconds <= 0) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SETEX %s %d %b", key.c_str(), ttl_seconds, value.data(), value.size()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis SETEX failed: " + std::string(context_->errstr));
        available_ = false;
        return false;
    }

    bool ok = reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK";
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!available_ || context_ == nullptr) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "DEL %s", key.c_str()));

    if (reply == nullptr) {
        Logger::get_instance()->log(ERROR, "Redis DEL failed: " + std::string(context_->errstr));
        available_ = false;
        return false;
    }

    bool ok = reply->type == REDIS_REPLY_INTEGER;
    freeReplyObject(reply);
    return ok;
}
