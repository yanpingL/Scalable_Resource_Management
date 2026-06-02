CREATE INDEX IF NOT EXISTS idx_resources_user_id
ON resources (user_id);

CREATE INDEX IF NOT EXISTS idx_resources_user_id_created_at
ON resources (user_id, created_at DESC);
