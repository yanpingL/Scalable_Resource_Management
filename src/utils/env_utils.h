#ifndef ENV_UTILS_H
#define ENV_UTILS_H

#include <cstdlib>
#include <string>

class EnvUtils {
public:
    // Reads a string environment variable or returns the fallback when absent.
    static std::string get_env_or_default(const char* name, const std::string& fallback) {
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return fallback;
        }
        return value;
    }

    // Reads an integer environment variable or returns the fallback when invalid.
    static int get_env_int_or_default(const char* name, int fallback) {
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return fallback;
        }

        char* end = nullptr;
        long parsed = std::strtol(value, &end, 10);
        if (end == value || *end != '\0') {
            return fallback;
        }
        return static_cast<int>(parsed);
    }
};

#endif
