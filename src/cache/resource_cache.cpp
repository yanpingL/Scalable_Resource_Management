#include "cache/resource_cache.h"
#include "cache/redis_client.h"

#include <string>

namespace {
constexpr int RESOURCE_CACHE_TTL_SECONDS = 300;
constexpr int RESOURCE_LIST_CACHE_TTL_SECONDS = 120;
}

std::optional<std::string> ResourceCache::get_resource(int user_id, int resource_id) {
    return RedisClient::get_instance()->get(resource_key(user_id, resource_id));
}

bool ResourceCache::set_resource(int user_id, int resource_id, const std::string& value) {
    return RedisClient::get_instance()->set(
        resource_key(user_id, resource_id),
        value,
        RESOURCE_CACHE_TTL_SECONDS);
}

bool ResourceCache::invalidate_resource(int user_id, int resource_id) {
    return RedisClient::get_instance()->del(resource_key(user_id, resource_id));
}

std::optional<std::string> ResourceCache::get_resources(int user_id) {
    return RedisClient::get_instance()->get(resources_key(user_id));
}

bool ResourceCache::set_resources(int user_id, const std::string& value) {
    return RedisClient::get_instance()->set(
        resources_key(user_id),
        value,
        RESOURCE_LIST_CACHE_TTL_SECONDS);
}

bool ResourceCache::invalidate_resources(int user_id) {
    return RedisClient::get_instance()->del(resources_key(user_id));
}

std::string ResourceCache::resource_key(int user_id, int resource_id) {
    return "resource:" + std::to_string(user_id) + ":" + std::to_string(resource_id);
}

std::string ResourceCache::resources_key(int user_id) {
    return "resources:user:" + std::to_string(user_id);
}
