#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MIGRATIONS_DIR="${MIGRATIONS_DIR:-${REPO_ROOT}/db/migrations}"

required_vars=(
  POSTGRES_HOST
  POSTGRES_PORT
  POSTGRES_DB
  POSTGRES_USER
  POSTGRES_PASSWORD
)

for var_name in "${required_vars[@]}"; do
  if [[ -z "${!var_name:-}" ]]; then
    echo "Missing required environment variable: ${var_name}" >&2
    exit 1
  fi
done

if ! command -v psql >/dev/null 2>&1; then
  echo "psql is required but was not found in PATH" >&2
  exit 1
fi

if [[ ! -d "${MIGRATIONS_DIR}" ]]; then
  echo "Migrations directory does not exist: ${MIGRATIONS_DIR}" >&2
  exit 1
fi

export PGPASSWORD="${POSTGRES_PASSWORD}"

PSQL_BASE=(
  psql
  --host="${POSTGRES_HOST}"
  --port="${POSTGRES_PORT}"
  --dbname="${POSTGRES_DB}"
  --username="${POSTGRES_USER}"
  --set=ON_ERROR_STOP=1
  --no-psqlrc
)

echo "Checking database connection..."
"${PSQL_BASE[@]}" --quiet --tuples-only --command="SELECT 1;" >/dev/null

echo "Ensuring migration tracking table exists..."
"${PSQL_BASE[@]}" --quiet --command="
CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(255) PRIMARY KEY,
    applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
"

shopt -s nullglob
migration_files=("${MIGRATIONS_DIR}"/*.sql)
shopt -u nullglob

if (( ${#migration_files[@]} == 0 )); then
  echo "No migration files found in ${MIGRATIONS_DIR}"
  exit 0
fi

applied_count=0
skipped_count=0

for migration_file in "${migration_files[@]}"; do
  version="$(basename "${migration_file}")"
  version_sql="${version//\'/\'\'}"

  already_applied="$("${PSQL_BASE[@]}" \
    --quiet \
    --tuples-only \
    --no-align \
    --command="SELECT 1 FROM schema_migrations WHERE version = '${version_sql}' LIMIT 1;")"

  if [[ "${already_applied}" == "1" ]]; then
    echo "Skipping already-applied migration: ${version}"
    skipped_count=$((skipped_count + 1))
    continue
  fi

  echo "Applying migration: ${version}"

  if grep -Eq '^--[[:space:]]*migrate:[[:space:]]*no-transaction[[:space:]]*$' "${migration_file}"; then
    "${PSQL_BASE[@]}" --file="${migration_file}"
    "${PSQL_BASE[@]}" \
      --command="INSERT INTO schema_migrations (version) VALUES ('${version_sql}');"
  else
    "${PSQL_BASE[@]}" \
      --single-transaction \
      --file="${migration_file}" \
      --command="INSERT INTO schema_migrations (version) VALUES ('${version_sql}');"
  fi

  applied_count=$((applied_count + 1))
done

echo "Migrations complete: ${applied_count} applied, ${skipped_count} skipped."
