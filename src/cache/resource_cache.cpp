#include "cache/resource_cache.h"
#include "cache/redis_client.h"
#include "utils/logger.h"

#include <string>

namespace {
// Single-resource entries can live longer because they are invalidated by id.
constexpr int RESOURCE_CACHE_TTL_SECONDS = 300;

// Lists change whenever a user creates, updates, or deletes a resource.
constexpr int RESOURCE_LIST_CACHE_TTL_SECONDS = 120;

// Logs cache hits, misses, and disabled/unavailable Redis as separate cases.
void log_cache_get_result(
    const std::string& label,
    const std::string& key,
    const std::optional<std::string>& cached) {
    if (cached.has_value()) {
        Logger::get_instance()->log(
            DEBUG,
            label + " cache hit key=" + key +
                " bytes=" + std::to_string(cached->size()));
        return;
    }

    const std::string reason = RedisClient::get_instance()->is_available()
        ? "miss"
        : "redis_unavailable";
    Logger::get_instance()->log(
        DEBUG,
        label + " cache " + reason + " key=" + key);
}

// Logs cache writes/invalidation at DEBUG on success and ERROR on failure.
void log_cache_write_result(
    const std::string& action,
    const std::string& label,
    const std::string& key,
    bool ok) {
    Logger::get_instance()->log(
        ok ? DEBUG : ERROR,
        label + " cache " + action + (ok ? " ok key=" : " failed key=") + key);
}
}

// Returns cached JSON for one resource, or nullopt on miss.
std::optional<std::string> ResourceCache::get_resource(int user_id, int resource_id) {
    const std::string key = resource_key(user_id, resource_id);
    auto cached = RedisClient::get_instance()->get(key);
    log_cache_get_result("resource", key, cached);
    return cached;
}

// Stores serialized JSON for one resource.
bool ResourceCache::set_resource(int user_id, int resource_id, const std::string& value) {
    const std::string key = resource_key(user_id, resource_id);
    bool ok = RedisClient::get_instance()->set(
        key,
        value,
        RESOURCE_CACHE_TTL_SECONDS);
    Logger::get_instance()->log(
        ok ? DEBUG : ERROR,
        "resource cache set " + std::string(ok ? "ok" : "failed") +
            " key=" + key +
            " ttl_seconds=" + std::to_string(RESOURCE_CACHE_TTL_SECONDS) +
            " bytes=" + std::to_string(value.size()));
    return ok;
}

// Deletes one cached resource entry.
bool ResourceCache::invalidate_resource(int user_id, int resource_id) {
    const std::string key = resource_key(user_id, resource_id);
    bool ok = RedisClient::get_instance()->del(key);
    log_cache_write_result("invalidate", "resource", key, ok);
    return ok;
}

// Returns cached JSON for a user's resource list, or nullopt on miss.
std::optional<std::string> ResourceCache::get_resources(int user_id) {
    const std::string key = resources_key(user_id);
    auto cached = RedisClient::get_instance()->get(key);
    log_cache_get_result("resource_list", key, cached);
    return cached;
}

// Stores serialized JSON for a user's resource list.
bool ResourceCache::set_resources(int user_id, const std::string& value) {
    const std::string key = resources_key(user_id);
    bool ok = RedisClient::get_instance()->set(
        key,
        value,
        RESOURCE_LIST_CACHE_TTL_SECONDS);
    Logger::get_instance()->log(
        ok ? DEBUG : ERROR,
        "resource_list cache set " + std::string(ok ? "ok" : "failed") +
            " key=" + key +
            " ttl_seconds=" + std::to_string(RESOURCE_LIST_CACHE_TTL_SECONDS) +
            " bytes=" + std::to_string(value.size()));
    return ok;
}

// Deletes the cached resource list for one user.
bool ResourceCache::invalidate_resources(int user_id) {
    const std::string key = resources_key(user_id);
    bool ok = RedisClient::get_instance()->del(key);
    log_cache_write_result("invalidate", "resource_list", key, ok);
    return ok;
}

// Builds the Redis key for one resource.
std::string ResourceCache::resource_key(int user_id, int resource_id) {
    return "resource:" + std::to_string(user_id) + ":" + std::to_string(resource_id);
}

// Builds the Redis key for one user's resource list.
std::string ResourceCache::resources_key(int user_id) {
    return "resources:user:" + std::to_string(user_id);
}
