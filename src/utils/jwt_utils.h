#ifndef JWT_UTILS_H
#define JWT_UTILS_H

#include <optional>
#include <string>

// Creates and verifies short-lived JWT access tokens for authenticated users.
class JwtUtils {
public:
    // Returns a signed token whose subject is the user id, or an empty string if misconfigured.
    static std::string create_jwt(int user_id);

    // Verifies signature, issuer, expiry, and subject; returns the user id when valid.
    static std::optional<int> verify_jwt_and_get_user_id(const std::string& token);

private:
    // Reads the HMAC signing secret from JWT_SECRET.
    static std::string get_secret();

    // Reads the expected issuer from JWT_ISSUER, defaulting to "webserver".
    static std::string get_issuer();

    // Reads token lifetime from JWT_EXPIRES_SECONDS, defaulting to one hour.
    static int get_expires_seconds();
};

#endif
