#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this script with sudo." >&2
  exit 1
fi

DEPLOY_USER="${DEPLOY_USER:-${SUDO_USER:-ubuntu}}"
DEPLOY_PATH="${DEPLOY_PATH:-/opt/webserver}"

apt-get update
apt-get install -y ca-certificates curl gnupg

install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
  | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
chmod a+r /etc/apt/keyrings/docker.gpg

. /etc/os-release
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu ${VERSION_CODENAME} stable" \
  > /etc/apt/sources.list.d/docker.list

apt-get update
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

usermod -aG docker "${DEPLOY_USER}"
mkdir -p "${DEPLOY_PATH}/releases"
chown -R "${DEPLOY_USER}:${DEPLOY_USER}" "${DEPLOY_PATH}"

systemctl enable --now docker

cat <<EOF
Docker is installed.

Next:
1. Log out and back in so group membership applies for ${DEPLOY_USER}.
2. Create ${DEPLOY_PATH}/.env.vm from deploy/vm/.env.vm.example.
3. Add GitHub environment secrets for vm-production.
EOF
