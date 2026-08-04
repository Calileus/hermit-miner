# hermit_miner Terminology Source of Truth

Purpose: single canonical naming reference for code, docs, and release communications.

## Product and Executables

| Canonical term | Use this wording | Avoid |
|---|---|---|
| Product | hermit_miner | i mine, IMINE |
| Miner executable | hermit_miner | miner app, miner service |
| Fake pool executable | hermit_miner_fake_pool | test pool, mock pool binary |
| Library target | hermit_miner_lib | runtime library |

## Runtime and Feature Terms

| Canonical term | Definition | Avoid |
|---|---|---|
| Stratum session | One connected subscribe/authorize/job/submit lifecycle | connection loop |
| Stratum cycle | One completed mined-and-submitted job unit in a session | job loop |
| Reconnect attempt | One failed session followed by backoff and retry | reconnect cycle |
| Shutdown summary | End-of-run operational counters log line | final stats line |
| Readiness report | End-of-run release gate status log line | health verdict |
| Health snapshot | Optional JSON file written from runtime metrics | metrics dump |

## Configuration Vocabulary

| Canonical term | Canonical key | Notes |
|---|---|---|
| Pool host | pool.host | Supports DNS or IP |
| Pool port | pool.port | 1..65535 |
| Pool username | pool.username | Preferred explicit auth identity |
| Pool password from env | pool.password_env | Preferred secret source |
| TLS guardrail | pool.require_tls | Must remain false until native TLS support exists |
| Reconnect cap | pool.max_reconnect_attempts | 0 means unlimited |
| Reconnect initial delay | pool.reconnect_initial_sec | Backoff lower bound |
| Reconnect max delay | pool.reconnect_max_sec | Backoff upper bound |

## Validation and Release Terms

| Canonical term | Command / target | Notes |
|---|---|---|
| Local quality gate | cmake --build build --target local_quality_check --config Release | Contributor pre-PR gate |
| Release readiness gate | cmake --build build --target release_readiness_check --config Release | Release candidate gate |
| Unified pre-go-live validation | cmake --build build --target pre_go_live_check --config Release | Consolidates setup/preflight/cert checks |
| Automated regression tests | ctest --test-dir build -C Release --output-on-failure --timeout 180 | Standard timeout is 180 |

## Documentation Rules

- Use local certification when referring to offline test readiness.
- Use pre-go-live for rollout preparation activities.
- Use release readiness for release gate pass/fail discussions.
- If docs and code differ, code behavior is the source of truth and docs must be updated.
