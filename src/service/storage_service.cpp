#include "storage_service.h"
#include "utils/env_utils.h"
#include "utils/logger.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpTypes.h>
#include <aws/s3/S3Client.h>
#include <miniocpp/client.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <string>
#include <set>

namespace {

struct MinioEndpoint {
    std::string host;
    bool https;
};

std::string get_storage_env(
    const char* s3_name,
    const char* minio_name,
    const std::string& fallback) {
    const std::string s3_value = EnvUtils::get_env_or_default(s3_name, "");
    if (!s3_value.empty()) {
        return s3_value;
    }

    return EnvUtils::get_env_or_default(minio_name, fallback);
}

int get_storage_env_int(
    const char* s3_name,
    const char* minio_name,
    int fallback) {
    const std::string s3_value = EnvUtils::get_env_or_default(s3_name, "");
    if (!s3_value.empty()) {
        return EnvUtils::get_env_int_or_default(s3_name, fallback);
    }

    return EnvUtils::get_env_int_or_default(minio_name, fallback);
}

// Removes trailing slashes so endpoint strings can be joined predictably.
std::string trim_trailing_slash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

// Converts an endpoint URL into the host and scheme format expected by MinIO SDK.
MinioEndpoint parse_minio_endpoint(const std::string& endpoint) {
    MinioEndpoint parsed{trim_trailing_slash(endpoint), true};

    const std::string http_prefix = "http://";
    const std::string https_prefix = "https://";

    if (parsed.host.rfind(http_prefix, 0) == 0) {
        parsed.host = parsed.host.substr(http_prefix.size());
        parsed.https = false;
    } else if (parsed.host.rfind(https_prefix, 0) == 0) {
        parsed.host = parsed.host.substr(https_prefix.size());
        parsed.https = true;
    }

    return parsed;
}


// Creates a MinIO SDK base URL from the configured endpoint and region.
minio::s3::BaseUrl make_minio_base_url(const std::string& endpoint) {
    const MinioEndpoint parsed = parse_minio_endpoint(endpoint);
    const std::string region = get_storage_env("S3_REGION", "MINIO_REGION", "us-east-1");
    return minio::s3::BaseUrl(parsed.host, parsed.https, region);
}


// Creates a static credential provider from the MinIO access key and secret.
minio::creds::StaticProvider make_minio_provider() {
    return minio::creds::StaticProvider(
        get_storage_env("S3_ACCESS_KEY", "MINIO_ACCESS_KEY", "minioadmin"),
        get_storage_env("S3_SECRET_KEY", "MINIO_SECRET_KEY", "minioadmin"));
}

// Replaces unsafe filename characters and rejects empty or dot-only names.
std::string sanitize_filename(const std::string& filename) {
    std::string clean;
    clean.reserve(filename.size());
    for (unsigned char c : filename) {
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            clean.push_back(static_cast<char>(c));
        } else {
            clean.push_back('_');
        }
    }

    if (clean.empty() || clean == "." || clean == "..") {
        return "";
    }
    return clean;
}

// Lowercases a string for case-insensitive validation checks.
std::string to_lower(std::string value) {
    // std::tolower expects either EOF(end of file) or a value representable as unsigned char
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// Returns the lowercase file extension, including the leading dot.
std::string file_extension(const std::string& filename) {
    const std::size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    return to_lower(filename.substr(dot));
}

// Rejects filenames that try to include directories or parent-directory markers.
bool has_path_traversal(const std::string& filename) {
    // std::string::npos --> no position, not found
    return filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos ||
        filename.find("..") != std::string::npos;
}

// Checks whether an upload MIME type is allowed by this application.
bool is_allowed_content_type(const std::string& content_type) {
    static const std::set<std::string> allowed = {
        "text/plain",
        "application/pdf",
        "image/png",
        "image/jpeg",
        "application/msword",
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    };
    return allowed.count(to_lower(content_type)) > 0;
}

// Blocks executable or script-like extensions even when the MIME type is allowed.
bool is_blocked_extension(const std::string& filename) {
    static const std::set<std::string> blocked = {
        ".bat", ".cmd", ".com", ".dll", ".exe", ".js", ".ps1", ".sh"
    };
    return blocked.count(file_extension(filename)) > 0;
}

Aws::String aws_string(const std::string& value) {
    return Aws::String(value.data(), value.size());
}

Aws::Http::HttpMethod aws_http_method(minio::http::Method method) {
    if (method == minio::http::Method::kPut) {
        return Aws::Http::HttpMethod::HTTP_PUT;
    }
    return Aws::Http::HttpMethod::HTTP_GET;
}

// Writes a storage-layer failure to the backend logger.
void log_storage_error(const std::string& message) {
    Logger::get_instance()->log(ERROR, "storage error: " + message);
}

// Builds a JSON storage error response and logs the same reason.
storage_json storage_error(const std::string& message) {
    log_storage_error(message);
    storage_json res;
    res["error"] = message;
    return res;
}

struct AwsSdkLifecycle {
    AwsSdkLifecycle() {
        Aws::InitAPI(options);
    }

    ~AwsSdkLifecycle() {
        Aws::ShutdownAPI(options);
    }

    Aws::SDKOptions options;
};

void ensure_aws_sdk_initialized() {
    static AwsSdkLifecycle lifecycle;
}

// Uses AWS SDK for C++ to create a real AWS S3 presigned object URL.
storage_json create_aws_sdk_presigned_url(
    minio::http::Method method,
    const std::string& bucket,
    const std::string& object_key,
    int expires_seconds) {

    storage_json res;

    const std::string access_key = get_storage_env("S3_ACCESS_KEY", "MINIO_ACCESS_KEY", "");
    const std::string secret_key = get_storage_env("S3_SECRET_KEY", "MINIO_SECRET_KEY", "");
    const std::string region = get_storage_env("S3_REGION", "MINIO_REGION", "us-east-1");
    if (access_key.empty() || secret_key.empty()) {
        return storage_error("missing S3 credentials");
    }

    ensure_aws_sdk_initialized();

    Aws::Client::ClientConfiguration config;
    config.region = aws_string(region);

    const Aws::Auth::AWSCredentials credentials(
        aws_string(access_key),
        aws_string(secret_key));
    Aws::S3::S3Client client(
        credentials,
        config,
        Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
        true);

    const Aws::String url = client.GeneratePresignedUrl(
        aws_string(bucket),
        aws_string(object_key),
        aws_http_method(method),
        static_cast<long long>(expires_seconds));
    if (url.empty()) {
        return storage_error("failed to create AWS S3 presigned URL");
    }

    res["url"] = std::string(url.c_str(), url.size());
    return res;
}

// Returns the current Unix timestamp in milliseconds for unique object keys.
long long current_epoch_millis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

// Asks the MinIO SDK to create a presigned object URL for the requested method.
storage_json create_presigned_url(
    minio::http::Method method,
    const std::string& endpoint,
    const std::string& bucket,
    const std::string& object_key,
    int expires_seconds) {

    storage_json res;
    if (endpoint.find("amazonaws.com") != std::string::npos) {
        return create_aws_sdk_presigned_url(method, bucket, object_key, expires_seconds);
    }

    minio::s3::BaseUrl base_url = make_minio_base_url(endpoint);
    minio::creds::StaticProvider provider = make_minio_provider();
    minio::s3::Client client(base_url, &provider);

    minio::s3::GetPresignedObjectUrlArgs args;
    args.bucket = bucket;
    args.object = object_key;
    args.method = method;
    args.expiry_seconds = static_cast<unsigned int>(expires_seconds);

    minio::s3::GetPresignedObjectUrlResponse resp =
        client.GetPresignedObjectUrl(args);
    if (!resp) {
        return storage_error(resp.Error().String());
    }

    res["url"] = resp.url;
    return res;
}


// Extracts the stored object key from a public MinIO URL.
std::string extract_object_key_from_public_url(
    const std::string& public_url,
    const std::string& public_endpoint,
    const std::string& bucket) {

    const std::string prefix = public_endpoint + "/" + bucket + "/";
    if (public_url.rfind(prefix, 0) != 0) {
        return "";
    }
    return public_url.substr(prefix.size());
}

} // namespace


// Validates upload metadata and returns a presigned PUT URL plus public file info.
storage_json StorageService::create_upload_url(
    int user_id,
    const std::string& filename,
    const std::string& content_type) {

    storage_json res;
    try {
        const std::string clean_filename = sanitize_filename(filename);
        if (clean_filename.empty()) {
            return storage_error("invalid filename");
        }

        const int max_filename_length =
            get_storage_env_int("S3_MAX_FILENAME_LENGTH", "MINIO_MAX_FILENAME_LENGTH", 255);
        if (filename.size() > static_cast<std::size_t>(max_filename_length)) {
            return storage_error("filename too long");
        }
        if (has_path_traversal(filename)) {
            return storage_error("invalid filename");
        }
        if (is_blocked_extension(clean_filename)) {
            return storage_error("blocked file type");
        }

        if (content_type.empty()) {
            return storage_error("invalid content_type");
        }

        if (!is_allowed_content_type(content_type)) {
            return storage_error("unsupported content_type");
        }

        const std::string bucket = get_storage_env("S3_BUCKET", "MINIO_BUCKET", "webserver-files");
        const std::string public_endpoint = trim_trailing_slash(
            get_storage_env("S3_PUBLIC_ENDPOINT", "MINIO_PUBLIC_ENDPOINT", "http://localhost:9000"));

        const std::string object_key =
            "users/" + std::to_string(user_id) + "/uploads/" +
            std::to_string(current_epoch_millis()) + "-" + clean_filename;

        const std::string public_url =
            public_endpoint + "/" + bucket + "/" + object_key;

        const int expires_seconds =
            get_storage_env_int("S3_UPLOAD_URL_EXPIRES", "MINIO_UPLOAD_URL_EXPIRES", 300);

        storage_json upload = create_presigned_url(
            minio::http::Method::kPut,
            public_endpoint,
            bucket,
            object_key,
            expires_seconds);
        if (upload.contains("error")) {
            return upload;
        }

        res["upload_url"] = upload["url"];
        res["public_url"] = public_url;
        res["object_key"] = object_key;
        res["bucket"] = bucket;
        res["content_type"] = content_type;
        res["expires_in"] = expires_seconds;

        return res;
    } catch (const std::exception& error) {
        Logger::get_instance()->log(
            ERROR,
            std::string("storage upload-url error: ") + error.what());
    } catch (...) {
        Logger::get_instance()->log(ERROR, "storage upload-url error");
    }

    res["error"] = "storage service unavailable";
    return res;
}


// Returns a short-lived presigned GET URL for an existing public file URL.
storage_json StorageService::create_download_url(const std::string& public_url) {
    storage_json res;
    try {
        const std::string bucket = get_storage_env("S3_BUCKET", "MINIO_BUCKET", "webserver-files");
        const std::string public_endpoint = trim_trailing_slash(
            get_storage_env("S3_PUBLIC_ENDPOINT", "MINIO_PUBLIC_ENDPOINT", "http://localhost:9000"));

        const std::string object_key =
            extract_object_key_from_public_url(public_url, public_endpoint, bucket);
        if (object_key.empty()) {
            return storage_error("invalid file url");
        }

        const int expires_seconds =
            get_storage_env_int("S3_DOWNLOAD_URL_EXPIRES", "MINIO_DOWNLOAD_URL_EXPIRES", 300);

        storage_json download = create_presigned_url(
            minio::http::Method::kGet,
            public_endpoint,
            bucket,
            object_key,
            expires_seconds);
        if (download.contains("error")) {
            return download;
        }

        res["download_url"] = download["url"];
        res["public_url"] = public_url;
        res["object_key"] = object_key;
        res["bucket"] = bucket;
        res["expires_in"] = expires_seconds;

        return res;
    } catch (const std::exception& error) {
        Logger::get_instance()->log(
            ERROR,
            std::string("storage download-url error: ") + error.what());
    } catch (...) {
        Logger::get_instance()->log(ERROR, "storage download-url error");
    }

    res["error"] = "storage service unavailable";
    return res;
}


// Deletes the MinIO object referenced by a public URL.
bool StorageService::delete_file(const std::string& public_url, std::string& error) {
    try {
        const std::string bucket = get_storage_env("S3_BUCKET", "MINIO_BUCKET", "webserver-files");
        const std::string public_endpoint = trim_trailing_slash(
            get_storage_env("S3_PUBLIC_ENDPOINT", "MINIO_PUBLIC_ENDPOINT", "http://localhost:9000"));
        const std::string internal_endpoint = trim_trailing_slash(
            get_storage_env("S3_ENDPOINT", "MINIO_ENDPOINT", public_endpoint));

        const std::string object_key =
            extract_object_key_from_public_url(public_url, public_endpoint, bucket);
        if (object_key.empty()) {
            error = "invalid file url";
            log_storage_error(error);
            return false;
        }

        minio::s3::BaseUrl base_url = make_minio_base_url(internal_endpoint);
        minio::creds::StaticProvider provider = make_minio_provider();
        minio::s3::Client client(base_url, &provider);

        minio::s3::RemoveObjectArgs args;
        args.bucket = bucket;
        args.object = object_key;

        minio::s3::RemoveObjectResponse resp = client.RemoveObject(args);
        if (!resp) {
            error = resp.Error().String();
            log_storage_error(error);
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        error = std::string("storage delete error: ") + ex.what();
        Logger::get_instance()->log(ERROR, error);
    } catch (...) {
        error = "storage delete error";
        Logger::get_instance()->log(ERROR, error);
    }

    return false;
}
