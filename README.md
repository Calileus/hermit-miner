# i_mine

This repo is a lean standalone miner scaffold for running multiple independent machines against one pool account with unique worker IDs.

Current scope:

- one miner executable: i_mine
- one local fake pool executable: i_mine_fake_pool
- one config file per machine
- CPU double-SHA256 proof-of-work loop
- offline Stratum subscribe/authorize/notify/submit flow
- JSON-line logging

Not included anymore:

- coordinator service
- worker-to-coordinator protocol
- distributed scheduling
- cluster control plane

## Documentation

- Architecture: `docs/ARCHITECTURE.md`
- Contributing guide: `CONTRIBUTING.md`
- Enterprise readiness report: `ENTERPRISE_READINESS_REPORT.md`
- Enterprise roadmap: `docs/ROADMAP.md`
- Operations runbook: `docs/RUNBOOK.md`
- Operations matrix: `docs/OPERATIONS.md`
- Incident template: `docs/INCIDENT_TEMPLATE.md`
- Release checklist: `docs/RELEASE_CHECKLIST.md`
- Drill log: `docs/DRILL_LOG.md`
- Security guide: `docs/SECURITY.md`
- Local certification checklist: `LOCAL_CERTIFICATION_CHECKLIST.md`

## Reality check

CPU mining will not mine real Bitcoin profitably. Mainnet mining requires ASIC hardware.

## Build

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
```

## Configs

Use one config per machine. In this repo the active configs are:

- config/miner-local-stratum.json (long-running / infinite mode)
- config/miner-local-stratum-test.json (deterministic short test mode)
- config/miner-local-stratum-soak.json (extended stability profile)
- config/miner-production.template.json (production template, no real secrets)

For local machine-specific production configs (git-ignored), use:

- config/miner-prod-cp1.local.json
- config/miner-prod-cp2.local.json
- config/miner-prod-cp3.local.json

### Pre-go-live unified validation

Run the complete pre-go-live checklist with a single command:

```sh
cmake --build build --target pre_go_live_check --config Release
```

This unified target will:
1. Create production config stubs if missing
2. Validate all configs for completeness and safety
3. Run automated certification tests
4. Generate a detailed timestamped log in `logs/pre_go_live_*.log`

The log file contains comprehensive details of all validation phases. Check the log output for any failures before deployment.

Individual phases can still be run separately if needed:

```sh
cmake --build build --target init_prod_configs --config Release
cmake --build build --target preflight_prod_configs --config Release
cmake --build build --target phase2_cert --config Release
```

Release gate target:

```sh
cmake --build build --target release_readiness_check --config Release
```

Release summary snapshot:

```sh
cmake --build build --target release_summary --config Release
```

Local contributor quality gate:

```sh
cmake --build build --target local_quality_check --config Release
```

Recommended pool identity pattern:

- same payout address on every machine
- unique worker ID per machine
- username format usually walletAddress.workerId

Security note:

- Prefer setting `pool.password_env` in config and supplying the real password via environment variable, instead of storing secrets in plaintext files.
- `pool.host` supports both IP addresses and DNS hostnames.

Example (PowerShell):

```powershell
$env:IMINE_POOL_PASSWORD = "your-real-password"
./build/Release/i_mine --config config/miner-local-stratum.json
```

## Run miner

```sh
# Windows multi-config generators
./build/Release/i_mine --config config/miner-local-stratum.json

# Linux/macOS single-config generators
./build/i_mine --config config/miner-local-stratum.json
```

### Long-running mode

- pool.max_cycles = 0 means infinite session mode.
- pool.max_reconnect_attempts = 0 means unlimited reconnect attempts (default).
- For bounded certification runs, set pool.max_reconnect_attempts to a finite value so outage scenarios fail fast.
- Press Ctrl+C for graceful shutdown.
- Reconnect uses exponential backoff via pool.reconnect_initial_sec and pool.reconnect_max_sec.

CLI overrides still apply:

```sh
./build/Release/i_mine --config config/miner-local-stratum.json --threads 4 --bits 20 --prefix test-job
```

## Offline Stratum test

```sh
# Terminal A
./build/Release/i_mine_fake_pool 3333

# Terminal B
./build/Release/i_mine --config config/miner-local-stratum.json
```

## Automated local certification

```sh
ctest --test-dir build -C Release --output-on-failure --timeout 180
```

Scheduled soak automation:

- Workflow: `.github/workflows/soak.yml`
- Trigger: nightly schedule + manual dispatch
- Scope: `LocalCert.MinerSoakProfileAgainstFakePool` with artifact upload

Manual checklist is in LOCAL_CERTIFICATION_CHECKLIST.md.

## Readiness metrics and logs

At shutdown, miner logs emit two operational lines:

- `Shutdown summary`: full counters and rates for session health.
- `Readiness report`: compact go-live signal.

Optional machine-readable output:

- Set `logging.health_output` to write a JSON health snapshot file at end of session.

Key fields in `Shutdown summary`:

- `accepted_count`: accepted shares.
- `jobs_received`: jobs received from pool.
- `shares_found`: local valid nonces found.
- `submits_attempted`: submit attempts made.
- `submit_failures`: failed submit attempts.
- `reconnect_events`: reconnect backoff events.
- `session_failures`: non-zero Stratum session exits.
- `accepted_per_min`: accepted share rate over uptime.
- `session_duration_sec`, `last_job_id`.

`Readiness report` status meanings:

- `ready`: `accepted_count >= 3` and no submit/session failures.
- `degraded`: at least one accepted share, but failures occurred.
- `not_ready`: no accepted shares in session.
