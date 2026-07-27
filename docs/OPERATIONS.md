# i_mine Operations Matrix

## Purpose

Quick symptom-to-action guide for on-call and release operators.

## Quick Triage Table

| Symptom | Likely Cause | Immediate Action | Follow-up |
|---|---|---|---|
| `connect() failed` or `Pool host resolution failed` | DNS/firewall/pool endpoint mismatch | Verify `pool.host`, `pool.port`, outbound connectivity, DNS resolution | Record failure window and escalate to network/provider if persistent |
| Repeated `Stratum cycle failed; reconnecting` with backoff growth | Pool instability or network jitter | Check pool health, host packet loss, and route stability | Tune reconnect window only after root cause is confirmed |
| `Authorize rejected` | Bad credentials or worker format | Verify `pool.username` and secret source (`password` or `password_env`) | Rotate credentials and rerun offline + short online validation |
| `Readiness report status=not_ready` | No accepted shares in session | Confirm `mining.notify` reception and submit path | Run fake-pool certification and compare logs |
| `Readiness report status=degraded` | Shares accepted but submit/session failures exist | Inspect `submit_failures` and `session_failures` in summary | Open incident ticket if recurring across hosts |
| Miner exits at startup with config error | Invalid/malformed config | Fix config fields per startup error and rerun | Add config change review requirement |
| No shutdown summary after Ctrl+C | Unclean termination or external kill | Use graceful stop (Ctrl+C), avoid force-kill for normal ops | Investigate host policies/process managers |

## Standard Escalation Flow

1. Capture latest 200 log lines from miner output log.
2. Capture final `Shutdown summary` and `Readiness report` lines.
3. Classify issue as network, auth, config, or runtime.
4. Reproduce in fake-pool mode when possible.
5. Escalate with evidence bundle (config minus secrets, logs, timestamps).

## Minimum Evidence Bundle

- Host identifier and config filename used.
- Command line used to start miner.
- Last `Shutdown summary` line.
- Last `Readiness report` line.
- 2-5 representative failure lines.

## Operational SLO Baseline (Suggested)

- Session startup success: >= 99% over rolling 24h.
- Reconnect storm alert threshold: > 10 reconnect events in 15 minutes.
- Readiness gate for deployment: status must be `ready` in repeated runs.

## Machine-Readable Health Snapshot

- Configure `logging.health_output` to emit a JSON health snapshot file at session end.
- Recommended automation usage:
	- Alert when `status != "ready"` across repeated validation windows.
	- Alert when `reconnect_events` exceeds threshold within operator-defined interval.
	- Capture snapshot file in incident evidence bundles.

## Safe Rollback

1. Stop production miner process gracefully.
2. Switch to offline test config (`miner-local-stratum-test.json`) for sanity validation.
3. Revert to last known-good binary/config pair.
4. Resume production only after readiness returns to `ready`.

## Log Retention Policy (Baseline)

- Local host rolling retention:
	- Keep at least 7 days of miner logs for active hosts.
	- Keep at least 30 days for incident-related logs.
- Protect integrity:
	- Do not edit historical logs; rotate by file/date only.
	- Preserve final `Shutdown summary`, `Readiness report`, and health snapshot artifacts for incidents.
- Storage hygiene:
	- Purge non-incident logs older than retention threshold.
	- Ensure local disk alerts exist for log volume growth.

## Incident Escalation Policy (Baseline)

- Severity guidance:
	- SEV-1: multi-host outage, persistent `not_ready`, or suspected credential compromise.
	- SEV-2: repeated degraded readiness or reconnect storm on one or more hosts.
	- SEV-3: isolated host issue with known workaround.
	- SEV-4: documentation/process issue without active production impact.
- Escalation path:
	1. On-call engineer triages and captures evidence bundle.
	2. Escalate to runtime owner for protocol/runtime faults.
	3. Escalate to security owner for any credential leakage suspicion.
	4. Escalate to release owner if rollback/redeploy is required.
- Reporting:
	- Use `docs/INCIDENT_TEMPLATE.md` for all SEV-1/SEV-2 incidents.
