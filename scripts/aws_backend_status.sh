#!/usr/bin/env bash
set -euo pipefail

AWS_REGION="${AWS_REGION:-ap-southeast-2}"
RDS_INSTANCE_ID="${RDS_INSTANCE_ID:-webserver-postgres}"
ECS_CLUSTER="${ECS_CLUSTER:-webserver-cluster}"
ECS_SERVICE="${ECS_SERVICE:-webserver-service}"

echo "AWS region: ${AWS_REGION}"
echo
echo "RDS:"
aws rds describe-db-instances \
  --region "${AWS_REGION}" \
  --db-instance-identifier "${RDS_INSTANCE_ID}" \
  --query "DBInstances[0].{id:DBInstanceIdentifier,status:DBInstanceStatus,endpoint:Endpoint.Address,autoRestart:AutomaticRestartTime}" \
  --output table

echo
echo "ECS:"
aws ecs describe-services \
  --region "${AWS_REGION}" \
  --cluster "${ECS_CLUSTER}" \
  --services "${ECS_SERVICE}" \
  --query "services[0].{cluster:clusterArn,service:serviceName,status:status,desired:desiredCount,running:runningCount,pending:pendingCount,taskDefinition:taskDefinition}" \
  --output table

echo
echo "Valkey/Redis:"
aws elasticache describe-replication-groups \
  --region "${AWS_REGION}" \
  --replication-group-id "${REDIS_REPLICATION_GROUP_ID:-webserver-valkey}" \
  --query "ReplicationGroups[0].{id:ReplicationGroupId,status:Status,nodeType:CacheNodeType,automaticFailover:AutomaticFailover,multiAZ:MultiAZ,activeNodes:NodeGroups[0].NodeGroupMembers[*].CacheClusterId,primaryEndpoint:NodeGroups[0].PrimaryEndpoint.Address}" \
  --output table
