# MqBlox Autonomous Orchestration & Engineering Plan

## 1. Mission & Scope
- **Project:** MqBlox (Qt6 Linux launcher for Roblox via Darling, MIT; AMD GPU, Wayland/Hyprland, Arch Linux).
- **Goal:** Reach production-grade stability, performance, security, and desktop compliance satisfying all **DONE** criteria for two consecutive cycles.
- **Operating Mode:** Fully autonomous agent team execution using file-based communication, atomic git feature branches, testing before merging, and local verification before pushing.

---

## 2. Agent Team Roster & Allocation
1. **Orchestrator**: Maintains `PLAN.md`, `TASKS.md`, `PROGRESS.md`, reviews cycle completion against DONE criteria.
2. **Architect**: High-level module decoupling, settings schema, Darling process boundary, records choices in `DECISIONS.md`.
3. **Developer**: Surgical, clean C++20/Qt6 code edits in dedicated `feature/*` branches with Conventional Commits.
4. **Reviewer**: Strict independent peer review (author never reviews self), catches race conditions, resource leaks, rule violations.
5. **Tester**: Unit tests (`tst_mqblox`), AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), ThreadSanitizer (TSan), 10x launch/exit verification.
6. **Profiler**: Benchmarks idle and runtime CPU/RAM/GPU, verifies zero GUI freezes (>100ms) during theme switches, scrolling, search, background animations.
7. **Security & Privacy Auditor**: Zero cookie/password inspection, strict IPC permissions (`0600`), path traversal defenses, no credentials/secrets in git.
8. **Design Reviewer**: UX polish via `ui-ux-pro-max` (minimalism, WCAG contrast, RU/EN localization, Serpantinum dock support).
9. **Release Manager**: Packaging verification (`PKGBUILD`, `install.sh`, `mqblox.desktop`, icons), `CHANGELOG.md`, tags.

---

## 3. Strict DONE Criteria (Must pass 2 full consecutive cycles)
- [ ] **Build & Tests**: Release build zero warnings (`-Wall -Wextra -Wpedantic`), 100% CTest pass.
- [ ] **Sanitizers**: ASan and UBSan clean on all test suites, TSan clean on multi-threaded routines.
- [ ] **Process Lifecycle**: Roblox/Darling launches and cleanly terminates 10 times consecutively (exit code 0), leaving zero orphaned processes (`darling`, `darlingserver`, `RobloxPlayer`) and zero stale lock/PID files (`.init.pid`, `.darlingserver.sock`).
- [ ] **GUI Responsiveness**: Zero main-thread UI stalls > 100ms across theme switching, searching, scrolling (`LockedScrollArea`), and background rendering (`Ton618GlWidget`).
- [ ] **Resource Baselines**: Idle CPU (< 1%) and RAM (< 80 MB) logged in `PROGRESS.md` without regression between cycles.
- [ ] **Clean Lifecycle**: Zero-leak installation, update, and uninstallation in isolated test prefixes/environments.
- [ ] **Hyprland/Wayland Integration**: Window icon and name correctly reported by `hyprctl clients` and mapped to `mqblox.desktop`.
- [ ] **Documentation & Compliance**: README (RU/EN), CHANGELOG, Credits (narezy), and MIT license pristine; GitHub Actions clean.
- [ ] **User Boundary**: Any non-automatable manual checks documented in `NEEDS_USER.md`.

---

## 4. Execution Phases & Milestones
- **Phase 1: Baseline Assessment & Framework Setup** (Current)
  - Initialize orchestration files (`PLAN.md`, `TASKS.md`, `PROGRESS.md`, `DECISIONS.md`, `NEEDS_USER.md`).
  - Verify baseline Release build and CTest suite.
  - Setup ASan/UBSan build configuration.
- **Phase 2: Codebase & Architectural Audit**
  - Audit `DarlingController` lifecycle and zombie process cleanup.
  - Audit IPC sockets, file descriptors, and path sanitization.
  - Audit UI responsiveness and single-buffer blur mechanics.
- **Phase 3: Automated Sanitizer & Concurrency Verification**
  - Build with `-fsanitize=address,undefined` and run full test suites.
  - Test thread safety in background runners and event-loop affinity.
- **Phase 4: Launch/Exit Robustness (10x Loop)**
  - Automated script testing 10 consecutive simulated/headless launch-exit cycles.
  - Stale socket and PID file cleanup verification.
- **Phase 5: Packaging & Hyprland Integration Verification**
  - Verify `install.sh` and `PKGBUILD` in clean isolated root.
  - Verify Hyprland `.desktop` metadata and icon presence.
- **Phase 6: Consecutive DONE Verification Passes**
  - Full pass 1 -> record in `PROGRESS.md`.
  - Full pass 2 -> record in `PROGRESS.md`.
  - Final Release packaging, tags, and sync.
