#include "resource_dao.h"
#include "dao/dao_util.h"
#include "db/connection_pool.h"
#include "utils/logger.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>

namespace {
struct ConnectionReleaser {
    void operator()(PGconn* conn) const {
        if (conn != nullptr) {
            connection_pool::get_instance()->release_connection(conn);
        }
    }
};

using ConnectionPtr = std::unique_ptr<PGconn, ConnectionReleaser>;
using ResultPtr = std::unique_ptr<PGresult, decltype(&PQclear)>;

void set_resource_dao_error(const std::string& message) {
    ResourceDAO::msg = message;
    Logger::get_instance()->log(ERROR, message);
}

void set_resource_dao_exception(const char* action, const std::exception& error) {
    set_resource_dao_error(
        std::string("Resource database error while ") + action + ": " + error.what());
}

void set_resource_dao_unknown_exception(const char* action) {
    set_resource_dao_error(
        std::string("Resource database error while ") + action + ".");
}
}

std::string ResourceDAO::msg;

// Executes a resource INSERT statement.
bool ResourceDAO::create_resource(const Resource& resource) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_resource_dao_error("DB connection failed.");
            return false;
        }

        const char* sql =
            "INSERT INTO resources (user_id, title, content, is_file) "
            "VALUES ($1::int, $2, $3, $4::boolean)";
        std::string user_id = std::to_string(resource.user_id);
        std::string is_file = resource.is_file ? "true" : "false";
        const char* values[] = {
            user_id.c_str(),
            resource.title.c_str(),
            resource.content.c_str(),
            is_file.c_str(),
        };

        ResultPtr result(PQexecParams(
            conn.get(), // DB connection
            sql,        // SQL query string
            4,          // #Parameters
            nullptr,    // Parameter types
            values,     // Actual values
            nullptr,    // Parameter length
            nullptr,    // Parameter formats
            0),         // Return results in text format, not binary
            PQclear);

        return DaoUtil::command_ok(conn.get(), result.get(), sql, msg);
    } catch (const std::exception& error) {
        set_resource_dao_exception("creating a resource", error);
    } catch (...) {
        set_resource_dao_unknown_exception("creating a resource");
    }

    return false;
}

// Loads all resources owned by one user.
std::vector<Resource> ResourceDAO::get_resources(int user_id) {
    msg.clear();

    std::vector<Resource> res;
    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_resource_dao_error("DB connection failed.");
            return res;
        }

        const char* sql =
            "SELECT id, title, content, is_file FROM resources "
            "WHERE user_id=$1::int";
        std::string user_id_value = std::to_string(user_id);
        const char* values[] = {
            user_id_value.c_str(),
        };

        ResultPtr result(PQexecParams(
            conn.get(),
            sql,
            1,
            nullptr,
            values,
            nullptr,
            nullptr,
            0),
            PQclear);
        if (result == nullptr || PQresultStatus(result.get()) != PGRES_TUPLES_OK) {
            msg = std::string("Query failed: ") + PQerrorMessage(conn.get());
            Logger::get_instance()->log(ERROR, msg + " SQL: " + sql);
            return res;
        }

        int row_count = PQntuples(result.get());
        for (int row = 0; row < row_count; ++row) {
            Resource r;
            r.id = std::atoi(PQgetvalue(result.get(), row, 0));
            r.title = DaoUtil::nullable_value(result.get(), row, 1);
            r.content = DaoUtil::nullable_value(result.get(), row, 2);
            r.is_file = DaoUtil::pg_bool_value(result.get(), row, 3);

            res.push_back(r);
        }
    } catch (const std::exception& error) {
        set_resource_dao_exception("loading resources", error);
    } catch (...) {
        set_resource_dao_unknown_exception("loading resources");
    }

    return res;
}

// Loads one resource by user id and resource id.
std::optional<Resource> ResourceDAO::get_resource(int user_id, int id) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_resource_dao_error("DB connection failed.");
            return std::nullopt;
        }

        const char* sql =
            "SELECT id, title, content, is_file FROM resources "
            "WHERE user_id=$1::int AND id=$2::int";
        std::string user_id_value = std::to_string(user_id);
        std::string id_value = std::to_string(id);
        const char* values[] = {
            user_id_value.c_str(),
            id_value.c_str(),
        };

        ResultPtr result(PQexecParams(
            conn.get(),
            sql,
            2,
            nullptr,
            values,
            nullptr,
            nullptr,
            0),
            PQclear);
        if (result == nullptr || PQresultStatus(result.get()) != PGRES_TUPLES_OK) {
            msg = std::string("Query failed: ") + PQerrorMessage(conn.get());
            Logger::get_instance()->log(ERROR, msg + " SQL: " + sql);
            return std::nullopt;
        }

        if (PQntuples(result.get()) == 0) {
            set_resource_dao_error("Resource not found.");
            return std::nullopt;
        }

        Resource r;
        r.id = std::atoi(PQgetvalue(result.get(), 0, 0));
        r.title = DaoUtil::nullable_value(result.get(), 0, 1);
        r.content = DaoUtil::nullable_value(result.get(), 0, 2);
        r.is_file = DaoUtil::pg_bool_value(result.get(), 0, 3);

        return r;
    } catch (const std::exception& error) {
        set_resource_dao_exception("loading a resource", error);
    } catch (...) {
        set_resource_dao_unknown_exception("loading a resource");
    }

    return std::nullopt;
}

// Executes a resource UPDATE statement and reports whether a row changed.
bool ResourceDAO::update_resource(const Resource& resource) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_resource_dao_error("DB connection failed.");
            return false;
        }

        const char* sql =
            "UPDATE resources SET title=$1, content=$2 "
            "WHERE user_id=$3::int AND id=$4::int";
        std::string user_id = std::to_string(resource.user_id);
        std::string id = std::to_string(resource.id);
        const char* values[] = {
            resource.title.c_str(),
            resource.content.c_str(),
            user_id.c_str(),
            id.c_str(),
        };

        ResultPtr result(PQexecParams(
            conn.get(),
            sql,
            4,
            nullptr,
            values,
            nullptr,
            nullptr,
            0),
            PQclear);
        bool success = DaoUtil::command_ok(conn.get(), result.get(), sql, msg);
        if (success && !DaoUtil::affected_rows(result.get())) {
            success = false;
            set_resource_dao_error("No change proceeded.");
        }

        return success;
    } catch (const std::exception& error) {
        set_resource_dao_exception("updating a resource", error);
    } catch (...) {
        set_resource_dao_unknown_exception("updating a resource");
    }

    return false;
}

// Deletes one resource owned by the given user.
bool ResourceDAO::delete_resource(int user_id, int id) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_resource_dao_error("DB connection failed.");
            return false;
        }

        const char* sql =
            "DELETE FROM resources WHERE id=$1::int AND user_id=$2::int";
        std::string id_value = std::to_string(id);
        std::string user_id_value = std::to_string(user_id);
        const char* values[] = {
            id_value.c_str(),
            user_id_value.c_str(),
        };

        Logger::get_instance()->log(DEBUG, std::string("SQL: ") + sql);

        ResultPtr result(PQexecParams(
            conn.get(),
            sql,
            2,
            nullptr,
            values,
            nullptr,
            nullptr,
            0),
            PQclear);
        bool success = DaoUtil::command_ok(conn.get(), result.get(), sql, msg);
        if (success && !DaoUtil::affected_rows(result.get())) {
            success = false;
            set_resource_dao_error("Resource not found.");
        }

        return success;
    } catch (const std::exception& error) {
        set_resource_dao_exception("deleting a resource", error);
    } catch (...) {
        set_resource_dao_unknown_exception("deleting a resource");
    }

    return false;
}
