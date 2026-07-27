# Incident Report Template

Use this template for runtime/security incidents involving miner stability, credential exposure, or readiness degradation.

## 1) Incident Metadata

- Incident ID:
- Date/Time opened (UTC):
- Reporter:
- Severity (SEV-1/2/3/4):
- Affected environments:
- Affected hosts:

## 2) Summary

- What happened:
- User/business impact:
- Detection source (alert/manual/CI):

## 3) Timeline (UTC)

- T0:
- T+X:
- Mitigation started:
- Recovery completed:

## 4) Evidence Bundle

- Config filename(s) used (no plaintext secrets):
- Command line used:
- Last `Shutdown summary` line:
- Last `Readiness report` line:
- Relevant log excerpts (2-5 lines):
- Health snapshot file (if enabled):

## 5) Root Cause Analysis

- Primary root cause:
- Contributing factors:
- Why existing controls did not prevent/detect earlier:

## 6) Containment and Recovery

- Immediate containment actions:
- Rollback/restore actions:
- Validation performed before return to service:

## 7) Corrective Actions

- Short-term fixes (with owner/date):
- Long-term prevention actions (with owner/date):
- Required documentation updates:

## 8) Closure Checklist

- [ ] `local_quality_check` passed on candidate fix
- [ ] `release_readiness_check` run and reviewed
- [ ] Manual checklist items reviewed (if production-impacting)
- [ ] Credentials rotated (if exposure suspected)
- [ ] Post-incident review completed
