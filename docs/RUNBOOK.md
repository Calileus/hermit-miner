# i_mine Operations Runbook

## Scope

Operational procedures for offline certification, controlled go-live, and incident handling.

## Prerequisites

- CMake and C++ toolchain installed.
- Access to config files under `config/`.
- For production mode, network reachability to pool host and port.

## Standard Build and Test

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --timeout 180 -V
```

## Pre-Go-Live Checklist

1. Initialize machine-local production configs:
   ```sh
   cmake --build build --target init_prod_configs --config Release
   ```
2. Fill each `config/miner-prod-*.local.json` with real values.
  - Startup will fail if `pool.username` or `pool.password` still contains `REPLACE_WITH`.
3. Prefer secret-by-env:
   - Set `pool.password_env` (for example `IMINE_POOL_PASSWORD`).
   - Export the variable on each machine before launch.
4. Run unified validation:
   ```sh
   cmake --build build --target pre_go_live_check --config Release
   ```
5. Complete manual checks from `LOCAL_CERTIFICATION_CHECKLIST.md`.

Reconnect policy tip:

- Use `pool.max_reconnect_attempts=0` for long-running production mode.
- Use a finite `pool.max_reconnect_attempts` for bounded validation profiles so pool outages fail fast instead of looping indefinitely.
- When the cap is reached, miner exits non-zero and logs `Reconnect attempt limit reached` for deterministic gate behavior.
- Keep `pool.reconnect_max_sec >= pool.reconnect_initial_sec`.

## Start / Stop

- Start (Windows):
  ```sh
  .\build\Release\i_mine --config config/miner-prod-cp1.local.json
  ```
- Stop:
  - Use `Ctrl+C` and confirm `Graceful shutdown complete` appears in logs.

## Health Signals

Check the latest logs for:

- `Connected to pool`
- `Subscribe OK`
- `Authorize OK`
- `Share accepted`
- `Shutdown summary`
- `Readiness report`

Readiness status policy:

- `ready`: accepted shares and no submit/session failures.
- `degraded`: accepted shares with failures.
- `not_ready`: no accepted shares.

## Incident Response

### Symptom: Cannot connect to pool

- Verify `pool.host` and `pool.port` in config.
- Verify DNS resolution and outbound firewall rules.
- Confirm credentials are set and environment variable is present when using `password_env`.

### Symptom: Reconnect storm

- Review `Stratum cycle failed; reconnecting` frequency.
- Verify pool stability and local network packet loss.
- Temporarily lower thread count if host is resource-constrained.

### Symptom: No accepted shares

- Confirm `mining.notify` events are received.
- Validate pool credentials and worker format (`wallet.worker`).
- Validate difficulty and prefix behavior with fake pool.

## Backup and Recovery

- Backup machine-local config files and deployment artifacts.
- Recovery flow:
  1. Restore binary and config.
  2. Run local fake-pool certification.
  3. Re-enable production pool connectivity.

## Change Management

- Require CI green status on pull requests.
- Require at least one reviewer for runtime-affecting changes.
- Re-run local certification suite for all Stratum and config changes.
