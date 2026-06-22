#include "resource_service.h"
#include "dao/resource_dao.h"
#include "dao/user_dao.h"
#include "cache/resource_cache.h"
#include "service/storage_service.h"
#include "utils/logger.h"

#include <exception>

namespace {
std::optional<res_json> parse_cached_json(const std::optional<std::string>& cached) {
     if (!cached.has_value()) {
          return std::nullopt;
     }

     try {
          return res_json::parse(cached.value());
     } catch (...) {
          Logger::get_instance()->log(ERROR, "failed to parse cached resource JSON");
          return std::nullopt;
     }
}

res_json service_error(const std::string& action, const std::exception& error) {
     Logger::get_instance()->log(
          ERROR,
          "resource service error while " + action + ": " + error.what());
     res_json res;
     res["error"] = "resource service unavailable";
     return res;
}

res_json service_error(const std::string& action) {
     Logger::get_instance()->log(ERROR, "resource service error while " + action);
     res_json res;
     res["error"] = "resource service unavailable";
     return res;
}
}

// Creates a resource and returns a JSON status object.
res_json ResourceService::create_resource(const ResourceInfo& Info){
     try {
     Resource resource;
     resource.user_id = Info.user_id;
     resource.title = Info.title;
     resource.content = Info.content;
     resource.is_file = Info.is_file;

     bool ok = ResourceDAO::create_resource(resource);
     res_json res;

     if (!ok){
          res["error"] = ResourceDAO::msg;
     } else {
          ResourceCache::invalidate_resources(Info.user_id);
          res["status"] = "created";
     }
     return res;
     } catch (const std::exception& error) {
          return service_error("creating a resource", error);
     } catch (...) {
          return service_error("creating a resource");
     }
}

// Returns all resources for a user as a JSON array.
res_json ResourceService::get_resources(int user_id){
     try {
     if (auto cached = parse_cached_json(ResourceCache::get_resources(user_id))) {
          Logger::get_instance()->log(DEBUG, "resource list cache hit user_id=" + std::to_string(user_id));
          return cached.value();
     }

     auto vec = ResourceDAO::get_resources(user_id);
     
     res_json res;
     if (!ResourceDAO::msg.empty()){
          res["error"] = ResourceDAO::msg;
          return res;
     }

     res["data"] = res_json::array();

     for (const auto& r : vec){
          res_json item;
          item["id"] = r.id;
          item["title"] = r.title;
          item["content"] = r.content;
          item["is_file"] = r.is_file;

          res["data"].push_back(item);
     }

     ResourceCache::set_resources(user_id, res.dump());
     return res;
     } catch (const std::exception& error) {
          return service_error("loading resources", error);
     } catch (...) {
          return service_error("loading resources");
     }
}

// Returns one resource as a JSON object.
res_json ResourceService::get_resource(int user_id, int id){
     try {
     if (auto cached = parse_cached_json(ResourceCache::get_resource(user_id, id))) {
          Logger::get_instance()->log(DEBUG, "resource cache hit user_id=" +
               std::to_string(user_id) + " id=" + std::to_string(id));
          return cached.value();
     }

     auto resource = ResourceDAO::get_resource(user_id, id);

     res_json res;
     if (!resource.has_value()){
          res["error"] = ResourceDAO::msg;
          return res;
     }

     const auto& r = resource.value();
     res["id"] = r.id;
     res["title"] = r.title;
     res["content"] = r.content;
     res["is_file"] = r.is_file;

     ResourceCache::set_resource(user_id, id, res.dump());
     return res;
     } catch (const std::exception& error) {
          return service_error("loading a resource", error);
     } catch (...) {
          return service_error("loading a resource");
     }
}

// Creates a presigned download URL for a file resource.
res_json ResourceService::get_file_download_url(int user_id, int id){
     try {
     auto resource = ResourceDAO::get_resource(user_id, id);

     res_json res;
     if (!resource.has_value()){
          res["error"] = ResourceDAO::msg;
          return res;
     }

     const auto& r = resource.value();
     if (!r.is_file){
          res["error"] = "resource is not file";
          return res;
     }

     return StorageService::create_download_url(r.content);
     } catch (const std::exception& error) {
          return service_error("creating a file download URL", error);
     } catch (...) {
          return service_error("creating a file download URL");
     }
}

// Updates a resource and returns a JSON status object.
res_json ResourceService::update_resource(const ResourceInfo& Info){
     try {
     Resource resource;
     resource.id = Info.id;
     resource.user_id = Info.user_id;
     resource.title = Info.title;
     resource.content = Info.content;

     bool ok = ResourceDAO::update_resource(resource);
     res_json res;
     if (!ok){
          res["error"] = ResourceDAO::msg;
     } else {
          ResourceCache::invalidate_resource(Info.user_id, Info.id);
          ResourceCache::invalidate_resources(Info.user_id);
          res["status"] = "updated";
     }
     return res;
     } catch (const std::exception& error) {
          return service_error("updating a resource", error);
     } catch (...) {
          return service_error("updating a resource");
     }
}

// Deletes a resource and removes the MinIO object when the resource is a file.
res_json ResourceService::delete_resource(int user_id, int id){
     try {
     auto resource = ResourceDAO::get_resource(user_id, id);
     if (!resource.has_value()) {
         res_json res;
         res["error"] = ResourceDAO::msg;
         return res;
     }

     // if the resource is file, we also need to delete the file stored in MinIO
     if (resource->is_file) {
         std::string error;
         if (!StorageService::delete_file(resource->content, error)) {
             res_json res;
             res["error"] = error;
             return res;
         }
     }

     bool ok = ResourceDAO::delete_resource(user_id, id);

     res_json res;
 
     if (!ok) {
         res["error"] = ResourceDAO::msg;
     } else {
         ResourceCache::invalidate_resource(user_id, id);
         ResourceCache::invalidate_resources(user_id);
         res["status"] = "deleted";
     }
     return res;  
     } catch (const std::exception& error) {
          return service_error("deleting a resource", error);
     } catch (...) {
          return service_error("deleting a resource");
     }
}
