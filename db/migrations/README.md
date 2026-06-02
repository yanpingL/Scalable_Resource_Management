# Database Migrations

Migration files are applied to a database in filename order.

Rules:

- Do not edit a migration after it has been applied to production.
- Add a new numbered migration for each future schema change.
- Keep migrations safe for existing data whenever possible.
- Review destructive changes such as `DROP`, `DELETE`, and column type changes before applying them.

The `schema_migrations` table records which migration versions have already
been applied.

Run migrations manually with:

```bash
export POSTGRES_HOST=localhost
export POSTGRES_PORT=5432
export POSTGRES_DB=webdb
export POSTGRES_USER=webuser
export POSTGRES_PASSWORD=webpass123

./scripts/run_migrations.sh
```

The runner requires `psql` in `PATH`. Add this marker at the top of a future
migration that must run outside a transaction, such as `CREATE INDEX
CONCURRENTLY`:

```sql
-- migrate: no-transaction
```
