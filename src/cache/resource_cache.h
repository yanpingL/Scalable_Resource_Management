#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <optional>
#include <string>

// Redis-backed cache for resource JSON payloads owned by one user.
class ResourceCache {
public:
    // Returns cached JSON for one resource, or nullopt on miss/unavailable Redis.
    static std::optional<std::string> get_resource(int user_id, int resource_id);

    // Stores serialized JSON for one resource with the single-resource TTL.
    static bool set_resource(int user_id, int resource_id, const std::string& value);

    // Deletes one resource cache entry after update/delete.
    static bool invalidate_resource(int user_id, int resource_id);

    // Returns cached JSON for the user's resource list.
    static std::optional<std::string> get_resources(int user_id);

    // Stores serialized JSON for the user's resource list with the list TTL.
    static bool set_resources(int user_id, const std::string& value);

    // Deletes the list cache after create/update/delete changes membership or content.
    static bool invalidate_resources(int user_id);

private:
    // Key shape for one resource: resource:<user_id>:<resource_id>.
    static std::string resource_key(int user_id, int resource_id);

    // Key shape for one user's resource list: resources:user:<user_id>.
    static std::string resources_key(int user_id);
};

#endif
