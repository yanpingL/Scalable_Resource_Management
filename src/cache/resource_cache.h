#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <optional>
#include <string>

class ResourceCache {
public:
    static std::optional<std::string> get_resource(int user_id, int resource_id);
    static bool set_resource(int user_id, int resource_id, const std::string& value);
    static bool invalidate_resource(int user_id, int resource_id);

    static std::optional<std::string> get_resources(int user_id);
    static bool set_resources(int user_id, const std::string& value);
    static bool invalidate_resources(int user_id);

private:
    static std::string resource_key(int user_id, int resource_id);
    static std::string resources_key(int user_id);
};

#endif
