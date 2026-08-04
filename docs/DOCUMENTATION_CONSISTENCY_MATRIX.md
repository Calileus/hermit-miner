# Documentation Consistency Matrix

Scope: README, runbook, operations docs, release checklist, security guide, architecture notes, and certification checklist.

## Matrix

| Topic | Source of truth | Consumed in | Status | Notes / Action |
|---|---|---|---|---|
| Product naming | docs/TERMINOLOGY.md | README.md, docs/* | Aligned | Use hermit_miner consistently |
| Build command | README.md | docs/RUNBOOK.md, LOCAL_CERTIFICATION_CHECKLIST.md | Aligned | CMake configure + Release build |
| Test command timeout | README.md | docs/RUNBOOK.md, docs/SECURITY.md | Fixed | Standardized to timeout 180 |
| Workspace path examples | Runtime workspace root | LOCAL_CERTIFICATION_CHECKLIST.md | Fixed | Updated to D:/mystuff/repos/hermit_miner |
| Pre-go-live target | README.md | docs/RUNBOOK.md, PLAN.md | Aligned | pre_go_live_check naming consistent |
| Release gate target | docs/RELEASE_CHECKLIST.md | README.md, docs/ROADMAP.md | Aligned | release_readiness_check consistent |
| Local quality target | CONTRIBUTING.md | README.md, docs/ROADMAP.md | Aligned | local_quality_check consistent |
| Readiness status semantics | src/lib/miner_runtime.cpp | README.md, docs/RUNBOOK.md, docs/OPERATIONS.md | Aligned | ready / degraded / not_ready definitions match |
| TLS support behavior | src/lib/miner_config.cpp validation | README.md, docs/RUNBOOK.md, docs/SECURITY.md | Aligned | require_tls=true fails fast |
| Secret source preference | src/lib/miner_config.cpp + docs policy | README.md, docs/SECURITY.md, CONTRIBUTING.md | Aligned | password_env preferred |
| Reconnect cap behavior | src/lib/miner_runtime.cpp | docs/RUNBOOK.md, PLAN.md | Aligned | max_reconnect_attempts=0 unlimited |
| Health snapshot behavior | src/lib/miner_runtime.cpp | README.md, docs/OPERATIONS.md | Aligned | Incremental emit pairing documented |

## Contradictions Found and Resolved in This Checkpoint

1. Test timeout mismatch:
   - Previous: docs/SECURITY.md used timeout 30 while README/runbook used 180.
   - Resolution: standardized security guide to timeout 180.

2. Workspace path mismatch:
   - Previous: LOCAL_CERTIFICATION_CHECKLIST.md referenced D:/mystuff/hermit_miner.
   - Resolution: corrected to D:/mystuff/repos/hermit_miner.

## Remaining Documentation Risks

- Enterprise report is historical and may drift from future runtime changes unless refreshed per release cycle.
- Manual checklist execution evidence is not embedded in docs; release packets should include explicit dated run artifacts.
