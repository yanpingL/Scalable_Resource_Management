#include "user_dao.h"
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

void set_user_dao_error(const std::string& message) {
    UserDAO::msg = message;
    Logger::get_instance()->log(ERROR, message);
}

void set_user_dao_exception(const char* action, const std::exception& error) {
    set_user_dao_error(
        std::string("User database error while ") + action + ": " + error.what());
}

void set_user_dao_unknown_exception(const char* action) {
    set_user_dao_error(std::string("User database error while ") + action + ".");
}
}

std::string UserDAO::msg;

// Creates a user record.
bool UserDAO::create_user(const User& user) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_user_dao_error("DB connection failed.");
            return false;
        }

        const char* sql =
            "INSERT INTO users (name, email, password) VALUES ($1, $2, $3)";
        const char* values[] = {
            user.name.c_str(),
            user.email.c_str(),
            user.password.c_str(),
        };

        ResultPtr result(PQexecParams(
            conn.get(),
            sql,
            3,
            nullptr,
            values,
            nullptr,
            nullptr,
            0),
            PQclear);

        return DaoUtil::command_ok(conn.get(), result.get(), sql, msg);
    } catch (const std::exception& error) {
        set_user_dao_exception("creating a user", error);
    } catch (...) {
        set_user_dao_unknown_exception("creating a user");
    }

    return false;
}

// Loads one user by email for login.
std::optional<User> UserDAO::get_user_by_email(const std::string& email) {
    msg.clear();

    try {
        ConnectionPtr conn(connection_pool::get_instance()->get_connection());

        if (!conn) {
            set_user_dao_error("DB connection failed.");
            return std::nullopt;
        }

        const char* sql =
            "SELECT id, name, email, password FROM users WHERE email=$1";
        const char* values[] = {
            email.c_str(),
        };

        Logger::get_instance()->log(DEBUG, std::string("SQL: ") + sql);

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
            return std::nullopt;
        }

        if (PQntuples(result.get()) == 0) {
            set_user_dao_error("User not found");
            return std::nullopt;
        }

        User user;
        user.id = std::atoi(PQgetvalue(result.get(), 0, 0));
        user.name = DaoUtil::nullable_value(result.get(), 0, 1);
        user.email = DaoUtil::nullable_value(result.get(), 0, 2);
        user.password = DaoUtil::nullable_value(result.get(), 0, 3);

        return user;
    } catch (const std::exception& error) {
        set_user_dao_exception("loading a user", error);
    } catch (...) {
        set_user_dao_unknown_exception("loading a user");
    }

    return std::nullopt;
}
