#!/usr/bin/env bash
set -euo pipefail

AWS_REGION="${AWS_REGION:-ap-southeast-2}"
RDS_INSTANCE_ID="${RDS_INSTANCE_ID:-webserver-postgres}"
ECS_CLUSTER="${ECS_CLUSTER:-webserver-cluster}"
ECS_SERVICE="${ECS_SERVICE:-webserver-service}"
ECS_DESIRED_COUNT="${ECS_DESIRED_COUNT:-1}"
WAIT="${WAIT:-true}"

echo "Starting AWS backend resources in ${AWS_REGION}"
echo "RDS instance: ${RDS_INSTANCE_ID}"
echo "ECS service: ${ECS_CLUSTER}/${ECS_SERVICE} -> desired count ${ECS_DESIRED_COUNT}"

rds_status="$(aws rds describe-db-instances \
  --region "${AWS_REGION}" \
  --db-instance-identifier "${RDS_INSTANCE_ID}" \
  --query "DBInstances[0].DBInstanceStatus" \
  --output text)"

if [[ "${rds_status}" == "available" ]]; then
  echo "RDS is already available."
elif [[ "${rds_status}" == "starting" ]]; then
  echo "RDS is already starting."
elif [[ "${rds_status}" == "stopped" ]]; then
  echo "Starting RDS..."
  aws rds start-db-instance \
    --region "${AWS_REGION}" \
    --db-instance-identifier "${RDS_INSTANCE_ID}" \
    --no-cli-pager >/dev/null
else
  echo "RDS is currently '${rds_status}'. Not issuing start-db-instance." >&2
fi

if [[ "${WAIT}" == "true" ]]; then
  echo "Waiting for RDS to become available..."
  aws rds wait db-instance-available \
    --region "${AWS_REGION}" \
    --db-instance-identifier "${RDS_INSTANCE_ID}"
fi

echo "Scaling ECS service..."
aws ecs update-service \
  --region "${AWS_REGION}" \
  --cluster "${ECS_CLUSTER}" \
  --service "${ECS_SERVICE}" \
  --desired-count "${ECS_DESIRED_COUNT}" \
  --no-cli-pager >/dev/null

if [[ "${WAIT}" == "true" ]]; then
  echo "Waiting for ECS service stability..."
  aws ecs wait services-stable \
    --region "${AWS_REGION}" \
    --cluster "${ECS_CLUSTER}" \
    --services "${ECS_SERVICE}"
fi

echo "AWS backend resources are started."
