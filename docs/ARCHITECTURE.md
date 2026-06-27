# i_mine Architecture

## Purpose

`i_mine` is a standalone Stratum miner scaffold intended for local certification and controlled production rollout across independent machines.

## Runtime Components

- `i_mine` executable (`src/miner.cpp`)
  - Loads config and CLI overrides.
  - Establishes Stratum session loop when pool mode is enabled.
  - Runs CPU double-SHA256 proof-of-work workers.
  - Emits readiness and shutdown telemetry lines.
- `i_mine_fake_pool` executable (`src/fake_pool.cpp`)
  - Simulates subscribe/authorize/notify/submit Stratum flow for deterministic offline integration tests.
- `i_mine_lib` (`src/lib/*.cpp`, `src/sha256.cpp`)
  - `StratumClient`: socket transport and JSON-RPC message flow.
  - `Logger`: JSON-line logging with sensitive field redaction.
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

1. Operator starts `i_mine` with a config file.
2. Config is loaded and validated (including optional `pool.password_env`).
3. Miner connects to pool host/port via DNS/IP resolution.
4. Miner performs Stratum subscribe + authorize.
5. Miner waits for `mining.notify` jobs.
6. Worker threads mine job prefix at requested difficulty.
7. Found shares are submitted via `mining.submit`.
8. Session-level counters are logged in `Shutdown summary` and `Readiness report`.

## Reliability and Recovery

- Exponential reconnect backoff using config bounds.
- Graceful shutdown on SIGINT/SIGTERM.
- Session restarts after recoverable Stratum cycle failures.

## Security Model

- Secrets can be supplied by environment variable (`password_env`).
- Logger redacts known sensitive keys and key/value patterns.
- Local production config files are git-ignored (`config/*.local.json`).

## Configuration Model

- Miner config is parsed by a strict in-process JSON parser in `src/miner.cpp`.
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
