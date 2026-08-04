# hermit_miner Architecture

## Purpose

`hermit_miner` is a standalone Stratum miner scaffold intended for local certification and controlled production rollout across independent machines.

## Runtime Components

- `hermit_miner` executable (`src/miner.cpp`)
  - Loads config and CLI overrides.
  - Establishes Stratum session loop when pool mode is enabled.
  - Runs CPU double-SHA256 proof-of-work workers.
  - Emits readiness and shutdown telemetry lines.
- `hermit_miner_fake_pool` executable (`src/fake_pool.cpp`)
  - Simulates subscribe/authorize/notify/submit Stratum flow for deterministic offline integration tests.
- `hermit_miner_lib` (`src/lib/*.cpp`, `src/sha256.cpp`)
  - `miner_cli`: command-line parsing and usage output.
  - `StratumClient`: socket transport and JSON-RPC message flow.
  - `Logger`: JSON-line logging with sensitive field redaction.
  - `miner_config`: strict config parsing, secret override, and validation.
  - `miner_runtime`: mining engine and Stratum session/reconnect orchestration.
  - `Job` helpers: difficulty checks and hash formatting.

## Build and Test System

- Build tool: CMake (`CMakeLists.txt`).
- Unit/integration tests: GoogleTest + CTest (`src/test/local_cert_tests.cpp`).
- Validation scripts:
  - `scripts/init-prod-configs.cmake`
  - `scripts/preflight-prod-configs.cmake`
  - `scripts/phase2-cert.cmake`
  - `scripts/pre-go-live.cmake`
- CI pipeline: `.github/workflows/ci.yml`.

## Data Flow

1. Operator starts `hermit_miner` with a config file.
2. Config is loaded and validated (including optional `pool.password_env`).
3. Miner connects to pool host/port via DNS/IP resolution.
4. Miner performs Stratum subscribe + authorize.
5. Miner waits for `mining.notify` jobs.
6. Worker threads mine job prefix at requested difficulty.
7. Found shares are submitted via `mining.submit`.
8. Session-level counters are logged in `Shutdown summary` and `Readiness report`.

## Reliability and Recovery

- Exponential reconnect backoff using config bounds.
- Optional reconnect-attempt cap (`pool.max_reconnect_attempts`) to fail fast in bounded validation runs.
- Graceful shutdown on SIGINT/SIGTERM.
- Session restarts after recoverable Stratum cycle failures.
- Connection loss during request/notify wait now fails the session immediately (no hidden timeout spin loop).

## Security Model

- Secrets can be supplied by environment variable (`password_env`).
- Logger redacts known sensitive keys and key/value patterns.
- Local production config files are git-ignored (`config/*.local.json`).
- Transport uses plaintext TCP today; `pool.require_tls` is a fail-fast intent flag until native TLS transport is implemented.

## Configuration Model

- Miner config is parsed by a strict in-process JSON parser in `src/lib/miner_config.cpp`.
- Primary schema uses nested objects:
  - `pool`: network/auth/session settings
  - `hashing`: mining execution settings
  - `logging`: output path
- Backward-compatible flat key parsing is retained for migration safety.

## Cryptography Components

- SHA256 implementation is centralized in `src/sha256.cpp` and `src/sha256.h`.
- Duplicate SHA256 sources under `src/lib` were removed to avoid divergence.

## Known Limitations

- Observability is log-based only; no metrics endpoint yet.
- Miner lifecycle orchestration is concentrated in `src/miner.cpp`.
