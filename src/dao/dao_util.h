#ifndef DAO_UTIL_H
#define DAO_UTIL_H

#include "utils/logger.h"

#include <cstdlib>
#include <libpq-fe.h>
#include <string>

class DaoUtil {
public:
    static bool command_ok(
        PGconn* conn,
        PGresult* result,
        const std::string& sql,
        std::string& msg) {
        if (result == nullptr) {
            msg = std::string("Query failed: ") +
                (conn != nullptr ? PQerrorMessage(conn) : "missing database connection");
            Logger::get_instance()->log(ERROR, msg + " SQL: " + sql);
            return false;
        }

        bool success = PQresultStatus(result) == PGRES_COMMAND_OK;
        if (!success) {
            msg = std::string("Query failed: ") + PQerrorMessage(conn);
            Logger::get_instance()->log(ERROR, msg + " SQL: " + sql);
        }
        return success;
    }

    static bool affected_rows(PGresult* result) {
        if (result == nullptr) {
            return false;
        }

        const char* rows = PQcmdTuples(result);
        return rows != nullptr && rows[0] != '\0' && std::atoi(rows) > 0;
    }

    static bool pg_bool_value(PGresult* result, int row, int col) {
        const char* value = PQgetvalue(result, row, col);
        return value != nullptr && (value[0] == 't' || value[0] == '1');
    }

    static std::string nullable_value(PGresult* result, int row, int col) {
        if (result == nullptr || PQgetisnull(result, row, col)) {
            return "";
        }

        const char* value = PQgetvalue(result, row, col);
        return value != nullptr ? value : "";
    }
};

#endif
