# Release Checklist (Controlled Rollout)

Use this checklist for each production rollout candidate.

## Release Metadata

- Release ID:
- Candidate branch/commit:
- Prepared by:
- Planned rollout date (UTC):
- Rollout scope (hosts/environments):

## 1) Quality and Security Gates

- [ ] `cmake --build build --target local_quality_check --config Release` passed.
- [ ] `cmake --build build --target release_readiness_check --config Release` passed.
- [ ] CI status is green (build/test + secret scan).
- [ ] No unresolved high-severity issues in current milestone tracking.

## 2) Config and Secret Readiness

- [ ] Machine-local config files are in `config/*.local.json` (not tracked).
- [ ] Placeholder values removed from rollout configs.
- [ ] Secret source validated (`pool.password_env` preferred).
- [ ] Endpoint and worker identity verified for each rollout host.

## 3) Operational Evidence

- [ ] Recent soak evidence reviewed (scheduled workflow history/logs).
- [ ] Latest readiness trend reviewed (`status=ready` under expected conditions).
- [ ] Incident drill status reviewed (or drill executed for this cycle).
- [ ] Latest drill entry recorded in `docs/DRILL_LOG.md`.

## 4) Change Risk and Ownership

- [ ] Required reviewers from `.github/CODEOWNERS` approved the PR.
- [ ] Runtime/security/build ownership paths covered where relevant.
- [ ] Release notes summarize user-visible and operator-visible changes.

## 5) Rollout and Monitoring Plan

- [ ] Rollout sequence defined (host order/batches).
- [ ] Rollback criteria defined.
- [ ] Rollback command/config bundle prepared.
- [ ] On-call contact acknowledged rollout window.

## 6) Post-Rollout Validation

- [ ] Startup logs show successful connect/subscribe/authorize.
- [ ] `Readiness report` indicates expected status.
- [ ] Health snapshot (if enabled) captured and validated.
- [ ] No reconnect storm or repeated degraded readiness.

## Sign-off

- Runtime owner:
- Security owner:
- Build/release owner:
- Operations owner:
- Final approval date (UTC):

Drill reference (ID/date):
