# Local Certification Checklist (Pre-Internet Go-Live)

Purpose: certify miner readiness in a fully offline environment before enabling outbound internet.

Scope: standalone miner plus local fake pool.

Pass policy:
- Every mandatory test must pass.
- Any mandatory failure blocks internet enablement.
- Repeat failed tests after fixes until all mandatory tests pass.

## Environment Baseline

Required:
- Windows or Linux host with CMake and C++ toolchain.
- No internet required.
- Project root opened at D:/mystuff/repos/hermit_miner (or equivalent).

Build command:
- cmake -S . -B build
- cmake --build build --config Release

Build pass criteria:
- Exit code 0.
- Binaries exist:
  - build/Release/hermit_miner(.exe)
  - build/Release/hermit_miner_fake_pool(.exe)

## Test Matrix

### LC-001 Build Integrity (Mandatory)

Command:
- cmake -S . -B build
- cmake --build build --config Release

Expected pass values:
- Exit code: 0
- Compile/link errors: 0

### LC-002 Fake Pool Boot (Mandatory)

Command:
- build/Release/hermit_miner_fake_pool 3333

Expected pass values:
- Log contains: [fake-pool] listening on 127.0.0.1:3333
- Process remains running until client session completes or manual stop.

### LC-003 Stratum Handshake (Mandatory)

Setup:
- Start LC-002 in terminal A.

Command (terminal B):
- build/Release/hermit_miner --config config/miner-local-stratum.json

Expected pass values in miner logs:
- Connected to pool: exactly 1 per session start.
- Subscribe OK: 1
- Authorize OK: 1
- Stratum session started: 1

Expected pass values in fake pool output:
- mining.subscribe request observed: 1+
- mining.authorize request observed: 1+

### LC-004 Multi-Job Session Continuity (Mandatory)

Setup:
- config/miner-local-stratum.json has pool.max_cycles = 0
- Start fake pool and miner.

Expected pass values (observe within 2 minutes):
- Received mining.notify count: >= 10
- Share accepted count: >= 10
- Connected to pool events after initial connection: 0 additional
- Subscribe/Authorize after session start: 0 additional

Interpretation:
- Confirms persistent one-session multi-job behavior without reconnect per job.

### LC-005 Graceful Ctrl+C Shutdown (Mandatory)

Setup:
- Miner running in infinite mode (LC-004 state).

Action:
- Send Ctrl+C to miner terminal.

Expected pass values in miner logs:
- Message present: Shutdown summary
- Shutdown summary context includes all fields:
  - accepted_count=<integer>
  - session_duration_sec=<integer>
  - last_job_id=<non-empty>
- Message present: Graceful shutdown complete

Expected pass values:
- Log corruption lines: 0 malformed JSON lines in the last 200 lines.

### LC-006 Reconnect Backoff Behavior (Mandatory)

Automation status:
- Automated by `LocalCert.MinerRecoversAfterPoolComesOnline` in the CTest suite.
- Manual rerun remains recommended for environment-specific rollout rehearsal.

Setup:
- Start miner first with fake pool down.

Command:
- build/Release/hermit_miner --config config/miner-local-stratum.json

Expected pass values while pool is down:
- Warning log repeated: Stratum cycle failed; reconnecting
- sleep_sec progression doubles until cap:
  - 1, 2, 4, 8 (with current default cap 8)
- No crash during backoff loop.

Recovery check:
- Start fake pool while miner is still running.

Expected pass values after pool starts:
- Connected to pool appears.
- Subscribe OK and Authorize OK appear.
- Share accepted count becomes >= 1 within 60 seconds.

### LC-007 Session Summary Validity (Mandatory)

Action:
- Stop miner with Ctrl+C after at least 30 seconds runtime.

Expected pass values from final summary line:
- accepted_count >= 1
- session_duration_sec >= 30
- last_job_id starts with job-

### LC-008 Basic Throughput Sanity (Advisory)

Setup:
- Use local fake pool and default offline config.

Expected pass values over 60 seconds:
- Progress lines emitted every report interval.
- At least one progress line with h/s > 100000.

Note:
- Throughput is hardware-dependent; this is a sanity floor, not a performance target.

### LC-009 Secret Redaction Sanity (Mandatory)

Automation status:
- Automated by `LocalCert.MinerRuntimeLogDoesNotLeakConfiguredPasswordMarker` in the CTest suite.
- Manual rerun remains recommended when changing logging/redaction behavior.

Action:
- Set pool.password in config to a known marker string.
- Run a short session and inspect latest logs.

Expected pass values:
- Marker string occurrences in logs: 0
- Username and payout fields may appear only where explicitly intended.

### LC-010 Exit Code Semantics (Advisory)

Action:
- Stop miner via Ctrl+C in interactive shell.

Expected pass values:
- Application logs show graceful shutdown block.
- If shell exit code is non-zero on Windows Ctrl+C, treat as acceptable only if graceful logs are present.

## Runbook (Quick Sequence)

1. Run LC-001.
2. Start LC-002.
3. Run LC-003 and LC-004 for at least 2 minutes.
4. Execute LC-005.
5. Execute LC-006.
6. Execute LC-007.
7. Execute LC-009.

Go/No-Go rule:
- GO only if all mandatory tests pass.

## Certification Record Template

Fill this after each full run:

- Date:
- Host:
- Commit/version:
- Tester:
- LC-001: PASS/FAIL
- LC-002: PASS/FAIL
- LC-003: PASS/FAIL
- LC-004: PASS/FAIL
- LC-005: PASS/FAIL
- LC-006: PASS/FAIL
- LC-007: PASS/FAIL
- LC-009: PASS/FAIL
- Mandatory result (all pass?): YES/NO
- Notes:
