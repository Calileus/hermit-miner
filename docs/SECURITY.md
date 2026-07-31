# i_mine Security Guide

## Scope

Security practices for local certification, production rollout, and operational handling of miner credentials and logs.

## Secrets Handling Policy

- Never commit real production credentials to the repository.
- Store machine-specific production configs in `config/*.local.json` (already git-ignored).
- Prefer environment-driven secrets via `pool.password_env`.
- Rotate pool credentials on any suspected exposure.

## Configuration Security Baseline

Before production launch, verify:

- `pool.enabled` is set intentionally.
- `pool.host` and `pool.port` target the intended endpoint.
- `pool.username` follows wallet/worker policy.
- Secret source is one of:
  - `pool.password_env` (preferred)
  - `pool.password` (fallback only)
- Placeholder values are removed from all production-local configs.

Runtime safety guard:

- When `pool.enabled=true`, miner rejects placeholder credential fields containing `REPLACE_WITH`.

## Logging and Redaction

The logger redacts sensitive patterns in both message and context fields.

Protected keys/patterns include:

- `password`, `passwd`
- `secret`
- `token`, `auth_token`
- `username`
- `payout_address`

Operational expectations:

- No plaintext secrets appear in logs.
- If secrets are observed in logs, treat as incident and rotate immediately.

## Deployment Security Checklist

1. Confirm local production config files are outside version control.
2. Confirm credentials are supplied via environment variable where possible.
3. Run:
  - `cmake -DPROJECT_ROOT=. -P scripts/ci-secret-scan.cmake`
   - `cmake --build build --target pre_go_live_check --config Release`
   - `ctest --test-dir build -C Release --output-on-failure --timeout 30`
4. Validate final logs for readiness status and absence of secret leakage.
5. Keep only least-privilege outbound connectivity required for pool communication.
6. Confirm log retention and incident escalation policy alignment (`docs/OPERATIONS.md`).

## Incident Response (Credential Exposure)

1. Stop affected miner processes.
2. Rotate pool credentials and regenerate environment secret values.
3. Purge or secure leaked logs according to internal policy.
4. Re-run local certification and release-readiness checks.
5. Resume production only after clean validation.

Use `docs/INCIDENT_TEMPLATE.md` to capture incident evidence and corrective actions.
Use `docs/DRILL_LOG.md` to record periodic drill execution and preparedness evidence.

## Hardening Backlog (Recommended)

- Add host firewall policy templates for miner nodes.
- Add explicit audit logging for config source (`password` vs `password_env`) without exposing values.
