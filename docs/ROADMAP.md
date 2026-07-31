# i_mine Enterprise Roadmap

Status date: 2026-07-31
Reference: `ENTERPRISE_READINESS_REPORT.md`

## Goal

Move launch posture from **READY WITH RISKS** to **READY** using incremental, low-risk milestones with explicit acceptance criteria.

## Milestone Plan

### M1 - Observability Baseline

Target window: Week 1  
Owner: _TBD_
Status: Implemented (baseline health signal)

Scope:
- Define minimal machine-readable health output strategy (for example: periodic JSON summary line or lightweight metrics file output).
- Define alert thresholds for reconnect storms, session failures, and readiness degradation.

Deliverables:
- Implementation PR for health output.
- Updated `docs/RUNBOOK.md` and `docs/OPERATIONS.md` with alert thresholds.

Implemented artifacts:
- `logging.health_output` support in runtime/config
- Health snapshot JSON emission at session end
- Integration coverage via local certification tests
- Operational guidance in `docs/OPERATIONS.md`

Acceptance criteria:
- At least one deterministic health signal can be consumed by automation.
- Alert thresholds documented and reviewed.

Verification:
- `cmake --build build --target local_quality_check --config Release`
- Manual validation of health output during a short fake-pool run.

### M2 - Stability/Soak Automation

Target window: Week 2  
Owner: _TBD_
Status: Implemented (initial baseline)

Scope:
- Add scheduled CI job for extended soak profile.
- Capture soak artifacts (logs/results) for triage.

Deliverables:
- CI workflow update with scheduled trigger.
- Troubleshooting notes in `docs/OPERATIONS.md`.

Implemented artifacts:
- `.github/workflows/soak.yml`
- `config/miner-local-stratum-soak.json`
- `LocalCert.MinerSoakProfileAgainstFakePool`

Acceptance criteria:
- Scheduled soak runs execute without manual intervention.
- Failure artifacts are retained and discoverable.

Verification:
- CI scheduled run passes at least one cycle.
- Failure simulation confirms artifacts upload.

### M3 - Operational Policy Completion

Target window: Week 3  
Owner: _TBD_
Status: Implemented (baseline policy)

Scope:
- Add explicit log retention/rotation policy.
- Add incident escalation path and contacts template.

Deliverables:
- Policy section in `docs/SECURITY.md` or `docs/OPERATIONS.md`.
- Incident checklist template in docs.

Implemented artifacts:
- `docs/OPERATIONS.md` (log retention + escalation policy)
- `docs/INCIDENT_TEMPLATE.md`
- `docs/SECURITY.md` references

Acceptance criteria:
- Log retention and escalation policy documented and approved.
- On-call handoff can execute using documentation only.

Verification:
- Documentation review checklist signed off.

### M4 - Ownership and Review Controls

Target window: Week 4  
Owner: _TBD_
Status: Implemented (baseline ownership controls)

Scope:
- Define code ownership map and required review paths for runtime/security changes.
- Align contribution process with ownership rules.

Deliverables:
- Ownership section in `CONTRIBUTING.md` (and CODEOWNERS if desired).

Implemented artifacts:
- `.github/CODEOWNERS`
- `CONTRIBUTING.md` ownership and required review policy

Acceptance criteria:
- Runtime/security paths have explicit reviewers assigned.
- PR workflow reflects ownership expectations.

Verification:
- Trial PR confirms ownership routing works.

## Exit Criteria for READY

Project can be marked **READY** when all are true:

1. Milestones M1-M4 completed and documented.
2. `local_quality_check` passes on release candidate.
3. `release_readiness_check` passes and manual checklist items are completed.
4. Repeated readiness sessions report stable `ready` status under expected operational conditions.

## Tracking Template

Use this section to track active progress:

- Current milestone: M2 operational evidence collection (scheduled soak history)
- Blockers: none known in codebase; pending run-history accumulation
- Last gate run result: `local_quality_check` passed (19/19 tests)
- Drill evidence tracking: use `docs/DRILL_LOG.md` and reference latest drill in release checklist
- Next checkpoint date: _TBD_
