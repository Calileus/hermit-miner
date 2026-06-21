# Independent Miner Plan

## Target shape

Run each computer as its own standalone miner process:

- same payout wallet across machines
- unique worker ID per machine
- direct pool connection from each machine
- no coordinator and no worker protocol

## Current repo status

Implemented:

- standalone i_mine executable
- offline Stratum client flow (subscribe, authorize, notify, submit)
- local fake pool for offline integration testing
- reconnect loop with backoff
- graceful shutdown on signal
- shutdown summary and readiness report metrics
- GoogleTest local certification suite
- Phase 1: CMake-based production config init and preflight validation
- Phase 2: CMake-based final certification checks (LC-001, LC-003/004 automated; LC-006, LC-009 pending manual)
- Unified pre-go-live validation: single CMake target orchestrating all checks with detailed timestamped logging

## Next steps (pre-go-live)

### Phase 3: Readiness telemetry gate

1. Fill in real values in production config files (payout_address, pool host/port/username/password, worker_ids).
2. Run the unified pre-go-live validation to confirm all configs pass and automated tests succeed:
   ```sh
   cmake --build build --target pre_go_live_check --config Release
   ```
3. Complete manual checks not yet automated (LC-006: reconnect recovery, LC-009: secret redaction).
4. Run 10-15 minute validation session per machine profile against the real pool (or fallback to fake pool).
5. Confirm shutdown logs include:
	- Shutdown summary
	- Readiness report
6. Gate on readiness status:
	- expected status=ready before go-live
	- investigate any degraded/not_ready result

Acceptance criteria:

- readiness status is ready in repeated runs
- no submit_failures or session_failures in steady sessions
- all manual checks (LC-006, LC-009) documented and passed
- detailed log from pre-go-live validation shows all phases passing

## Go-live execution

Prerequisites: Phase 1, 2, and 3 complete (all automated checks passing, manual checks documented).

1. Enable outbound connectivity for miner hosts.
2. Start one miner per machine with its machine-specific config.
3. Monitor first 30-60 minutes for:
	- stable session behavior
	- accepted share growth
	- absence of reconnect storms
4. If instability appears, rollback to offline validation mode and triage.

## Post-go-live follow-ups

1. Add CI to run ctest on pull requests.
2. Add preflight validation to fail fast on obvious misconfiguration (localhost host, placeholder wallet, duplicate worker ids in deployment set).
3. Add env-var based password support to reduce secret exposure in config files.
4. Add optional machine-readable metrics export for long-session trend analysis.
