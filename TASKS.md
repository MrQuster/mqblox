# MqBlox Task Backlog & Tracking

## Status Definitions
- `TODO`: Planned for current or upcoming cycle.
- `IN_PROGRESS`: Currently being worked on by an agent.
- `REVIEW`: Code completed, pending independent peer review.
- `TESTING`: Review approved, currently undergoing automated tests & sanitizers.
- `DONE`: Completed, verified, and committed.
- `BLOCKED`: Waiting on external condition or user action (recorded in `NEEDS_USER.md`).

---

## Active & Upcoming Tasks

| Task ID | Component | Description | Owner | Status |
|---|---|---|---|---|
| TASK-001 | Infra | Create orchestration tracking files (PLAN, TASKS, PROGRESS, DECISIONS, NEEDS_USER) | Orchestrator | DONE |
| TASK-002 | Build/QA | Execute baseline Release build with `-Wall -Wextra -Wpedantic` and run CTest | Tester | IN_PROGRESS |
| TASK-003 | Sanitizers | Configure and execute ASan + UBSan test suite build | Tester | TODO |
| TASK-004 | Core | Audit `DarlingController` process termination & zombie cleanup logic | Developer | TODO |
| TASK-005 | Core/Security | Audit `.darlingserver.sock` and `.init.pid` teardown and socket permissions (`0600`) | Security Auditor | TODO |
| TASK-006 | UI/Perf | Audit main thread stalls (>100ms) on theme change, scrolling, searching, and Ton618GlWidget | Profiler | TODO |
| TASK-007 | UI/Design | Audit UI against `ui-ux-pro-max` (WCAG contrast, RU/EN completeness, single-buffer blur) | Design Reviewer | TODO |
| TASK-008 | Packaging | Test clean install, upgrade, and uninstall in isolated `$HOME`/prefix | Release Manager | TODO |
| TASK-009 | Hyprland | Verify `mqblox.desktop` app ID and dock icon visibility on Wayland/Hyprland | Release Manager | TODO |
| TASK-010 | Lifecycle | Execute 10 consecutive launch/exit cycles verifying clean termination and zero leaks | Tester | TODO |
| TASK-011 | Audit | Verify Credits (narezy), MIT license, and zero cookie/password access | Security Auditor | TODO |
| TASK-012 | Release | Run Full Pass 1 of all DONE criteria | Orchestrator | TODO |
| TASK-013 | Release | Run Full Pass 2 of all DONE criteria | Orchestrator | TODO |
