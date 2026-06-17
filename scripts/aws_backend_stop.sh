#!/usr/bin/env bash
set -euo pipefail

AWS_REGION="${AWS_REGION:-ap-southeast-2}"
RDS_INSTANCE_ID="${RDS_INSTANCE_ID:-webserver-postgres}"
ECS_CLUSTER="${ECS_CLUSTER:-webserver-cluster}"
ECS_SERVICE="${ECS_SERVICE:-webserver-service}"
WAIT="${WAIT:-true}"

echo "Stopping AWS backend resources in ${AWS_REGION}"
echo "ECS service: ${ECS_CLUSTER}/${ECS_SERVICE} -> desired count 0"
echo "RDS instance: ${RDS_INSTANCE_ID}"

echo "Scaling ECS service to 0..."
aws ecs update-service \
  --region "${AWS_REGION}" \
  --cluster "${ECS_CLUSTER}" \
  --service "${ECS_SERVICE}" \
  --desired-count 0 \
  --no-cli-pager >/dev/null

if [[ "${WAIT}" == "true" ]]; then
  echo "Waiting for ECS service stability..."
  aws ecs wait services-stable \
    --region "${AWS_REGION}" \
    --cluster "${ECS_CLUSTER}" \
    --services "${ECS_SERVICE}"
fi

rds_status="$(aws rds describe-db-instances \
  --region "${AWS_REGION}" \
  --db-instance-identifier "${RDS_INSTANCE_ID}" \
  --query "DBInstances[0].DBInstanceStatus" \
  --output text)"

if [[ "${rds_status}" == "stopped" ]]; then
  echo "RDS is already stopped."
elif [[ "${rds_status}" == "stopping" ]]; then
  echo "RDS is already stopping."
elif [[ "${rds_status}" == "available" ]]; then
  echo "Stopping RDS..."
  aws rds stop-db-instance \
    --region "${AWS_REGION}" \
    --db-instance-identifier "${RDS_INSTANCE_ID}" \
    --no-cli-pager >/dev/null
else
  echo "RDS is currently '${rds_status}'. Not issuing stop-db-instance." >&2
fi

if [[ "${WAIT}" == "true" ]]; then
  echo "Waiting for RDS to stop..."
  while true; do
    rds_status="$(aws rds describe-db-instances \
      --region "${AWS_REGION}" \
      --db-instance-identifier "${RDS_INSTANCE_ID}" \
      --query "DBInstances[0].DBInstanceStatus" \
      --output text)"

    if [[ "${rds_status}" == "stopped" ]]; then
      break
    fi

    echo "RDS status: ${rds_status}"
    sleep 30
  done
fi

echo "AWS backend resources are stopped."
