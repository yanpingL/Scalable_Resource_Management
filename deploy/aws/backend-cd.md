# Backend Continuous Deployment

This repository deploys the C++ backend to ECS Fargate through GitHub Actions.

## Workflow

The workflow lives at:

```text
.github/workflows/deploy-backend.yml
```

It does the following:

1. Assumes an AWS IAM role through GitHub OIDC.
2. Builds the backend Docker image for `linux/arm64`.
3. Pushes the image to ECR.
4. Renders and registers a new ECS task definition using the new image tag.
5. Runs a one-off ECS task to apply database migrations.
6. Updates the ECS service and waits for service stability.

The deploy job runs on GitHub's native `ubuntu-24.04-arm` runner so the
ARM64 image is built without QEMU emulation.

## GitHub Configuration

Create a GitHub environment:

```text
production
```

Add this environment secret:

```text
AWS_ROLE_TO_ASSUME=arn:aws:iam::<account-id>:role/<github-actions-deploy-role>
```

Add these environment variables:

```text
ECS_CLUSTER=<your-ecs-cluster-name>
ECS_SERVICE=<your-ecs-service-name>
```

The workflow already defines:

```text
AWS_REGION=ap-southeast-2
ECR_REPOSITORY=webserver-backend
ECS_TASK_DEFINITION=ecs-task-definition.json
ECS_CONTAINER_NAME=webserver-backend
ECS_DESIRED_COUNT=2
```

`ECS_DESIRED_COUNT=2` tells the ECS service to keep two backend tasks
running after each deployment.

## AWS IAM Role

Use GitHub OIDC instead of long-lived access keys.

The deploy role needs permissions for:

```text
ecr:GetAuthorizationToken
ecr:BatchCheckLayerAvailability
ecr:InitiateLayerUpload
ecr:UploadLayerPart
ecr:CompleteLayerUpload
ecr:PutImage
ecs:DescribeServices
ecs:DescribeTasks
ecs:DescribeTaskDefinition
ecs:RegisterTaskDefinition
ecs:RunTask
ecs:UpdateService
iam:PassRole
```

Scope `iam:PassRole` to the ECS task execution role used by
`ecs-task-definition.json`.

## Triggering A Deploy

The workflow runs when backend-related files are pushed to `main`.

You can also run it manually:

```text
GitHub -> Actions -> Deploy Backend -> Run workflow
```
