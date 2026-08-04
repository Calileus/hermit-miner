# Contributing to hermit_miner

## Scope

This repository favors small, reviewable, behavior-safe changes.

## Branch and PR Guidelines

- Keep pull requests focused on one concern.
- Include rationale and risk notes in the PR description.
- Do not mix refactors with unrelated feature changes.

## Required Local Checks

Run these before opening a PR:

```sh
cmake --build build --target local_quality_check --config Release
```

This target runs:

1. Repository secret scan
2. Build (Release)
3. Full test suite

## Required CI Checks

PR must pass CI workflow checks:

- Secret Scan (Repository)
- Configure/Build
- CTest regression suite

## Ownership and Required Reviews

Repository ownership is declared in `.github/CODEOWNERS`.

Required review paths:

- Runtime/protocol changes (`src/miner.cpp`, `src/lib/miner_runtime.*`, `src/lib/stratum.*`, `src/lib/job.*`) require runtime owner review.
- Config/secret parsing changes (`src/lib/miner_config.*`, `config/*.json`) require runtime + security owner review.
- CI/build/script changes (`.github/workflows/*`, `scripts/*.cmake`, `CMakeLists.txt`) require build owner review.
- Security policy and scanning (`docs/SECURITY.md`, `scripts/ci-secret-scan.cmake`) require security owner review.

PRs affecting multiple ownership areas should request all relevant owner groups.

## Security Rules

- Never commit real credentials or private keys.
- Prefer `pool.password_env` instead of plaintext passwords in configs.
- Keep machine-specific configs in `config/*.local.json` only.

## Testing Rules

- Add or update tests for behavior changes.
- Keep existing tests passing; avoid introducing flaky timing dependencies.
- For protocol or config changes, run full `ctest` locally.

## Documentation Rules

Update docs when behavior or operator workflow changes:

- `README.md` for command surface changes
- `docs/ARCHITECTURE.md` for module boundary changes
- `docs/RUNBOOK.md` and `docs/OPERATIONS.md` for operational changes
- `docs/SECURITY.md` for security-affecting changes
