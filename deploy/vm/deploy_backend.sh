#!/usr/bin/env bash
set -euo pipefail

PROJECT_NAME="${COMPOSE_PROJECT_NAME:-webserver-vm}"
COMPOSE_FILE="${COMPOSE_FILE:-docker-compose.vm.yml}"
ENV_FILE="${ENV_FILE:-.env.vm}"

if [[ ! -f "${COMPOSE_FILE}" ]]; then
  echo "Missing compose file: ${COMPOSE_FILE}" >&2
  exit 1
fi

if [[ ! -f "${ENV_FILE}" ]]; then
  echo "Missing env file: ${ENV_FILE}" >&2
  exit 1
fi

compose() {
  docker compose \
    --project-name "${PROJECT_NAME}" \
    --env-file "${ENV_FILE}" \
    -f "${COMPOSE_FILE}" \
    "$@"
}

compose build web1 web2
compose up -d postgres redis minio
compose run --rm minio-init
compose run --rm migrations
compose up -d --remove-orphans nginx web1 web2 postgres redis minio
compose ps
