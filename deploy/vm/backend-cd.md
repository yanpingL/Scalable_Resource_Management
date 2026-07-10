# VM Backend Continuous Deployment

This deployment keeps the frontend on Vercel and runs the backend-side stack on
one Linux VM with Docker Compose.

Long-running containers:

```text
nginx
web1
web2
postgres
redis
minio
```

One-off setup containers:

```text
minio-init
migrations
```

## VM Setup

Install Docker and the Docker Compose plugin on the VM, then create a
deployment directory:

```bash
mkdir -p /opt/webserver
```

You can also use the bootstrap script:

```bash
sudo DEPLOY_USER=azureuser DEPLOY_PATH=/opt/webserver bash deploy/vm/bootstrap_vm.sh
```

Create the real environment file on the VM:

```bash
cp deploy/vm/.env.vm.example /opt/webserver/.env.vm
```

Edit `/opt/webserver/.env.vm` with production values. Do not commit that
file.

Only expose nginx publicly. Keep Postgres, Redis, and MinIO private to the
Docker network. The VM nginx config proxies `/api/*` to the C++ backend and
`/webserver-files/*` to MinIO so browser presigned uploads/downloads can work
without exposing the MinIO port directly.

## GitHub Configuration

Create a GitHub environment named:

```text
vm-production
```

Add these environment secrets:

```text
VM_SSH_HOST=<vm-public-ip-or-domain>
VM_SSH_USER=<ssh-user>
VM_SSH_PRIVATE_KEY=<private-key-for-that-user>
```

Optionally add this environment variable:

```text
VM_DEPLOY_PATH=/opt/webserver
```

If omitted, the workflow uses `/opt/webserver`.

## Vercel

Set the frontend `API_BASE_URL` to the VM nginx public URL, for example:

```text
API_BASE_URL=https://api.example.com
```

Then Vercel rewrites `/api/*` to the VM backend.

Use the same origin for `S3_PUBLIC_ENDPOINT` in `/opt/webserver/.env.vm`.
With the default bucket name, browser file URLs will look like:

```text
https://api.example.com/webserver-files/<object-key>
```
