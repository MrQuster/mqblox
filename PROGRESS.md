# MqBlox Engineering Progress Log

## Cycle Summary Log

### Cycle 0: Initialization & Framework Setup
- **Date:** 2026-10-24
- **Focus:** Team alignment, orchestration tracking initialization, baseline compilation.
- **Actions Taken:**
  - Created `PLAN.md`, `TASKS.md`, `PROGRESS.md`, `DECISIONS.md`, `NEEDS_USER.md`.
  - Registered 18 agents and sub-agents.
  - Installed 5 domain skills in `.agents/skills/`.

---

### Cycle 1: Full Audit, Sanitizers, Process Lifecycle, and Desktop Integration (Full Pass 1)
- **Date:** 2026-10-24
- **Focus:** Zero warnings, ASan/UBSan/TSan verification, 10x launch/exit verification, Hyprland dock integration, XDG compliance.
- **Findings & Fixes:**
  1. **Hyprland/Wayland Dock Matching**:
     - `packaging/mqblox.desktop` had `StartupWMClass=RobloxPlayer`, preventing Serpantinum dock and Hyprland from matching `class: mqblox` to the desktop entry.
     - **Fix**: Updated `StartupWMClass=mqblox`. Verified with `hyprctl clients` that client metadata (`class: mqblox`, `initialClass: mqblox`, `title: MqBlox`) matches 100%.
  2. **Socket & PID Teardown Robustness**:
     - `DarlingController::clearStaleDarlingProcesses()` previously only purged files from `m_settingsMgr->darlingPrefixDir()`. If the user utilized MacOBlox Flatpak or alternate paths, `.init.pid` and `.darlingserver.sock` could linger.
     - **Fix**: Expanded cleanup to scan and purge stale `.init.pid` and `.darlingserver.sock` across all candidate prefix directories (`~/.darling`, `~/.local/share/darling`, `~/.var/app/xyz.narez.MacOBlox/data/darling`, and `/tmp/`). Added `testStaleSocketAndPidCleanup()` in unit test suite.
  3. **ThreadSanitizer Calibration**:
     - Added `tests/tsan.supp` to suppress data race false-positives in uninstrumented prebuilt system libraries (`libQt6Core.so`, `libQt6DBus.so`, `libQt6Test.so`). Verified 100% clean TSan pass on application code.
  4. **10x Consecutive Launch & Exit Verification**:
     - Executed automated 10-iteration loop via `tests/verify_10x_launch_exit.py`. All 10 cycles exited cleanly with 0 leftover processes (`RobloxPlayer`, `darling`, `darlingserver`, `mldr`) and 0 stale `.init.pid` / `.darlingserver.sock` files.
  5. **Clean Installation & Uninstallation**:
     - Executed `./install.sh install --prefix /tmp/test_prefix` followed by `./install.sh uninstall --prefix /tmp/test_prefix`. Verified zero leftover files.
  6. **UI Responsiveness & Thread Stalls**:
     - Audited search debounce (90ms), `setUpdatesEnabled(false)` card toggling, `LockedScrollArea` horizontal locking, mouse wheel filter on sliders/comboboxes, and single-buffer OpenGL backdrop. Verified zero main-thread stalls > 100ms.
- **Resource Measurements (Hyprland / Arch Linux / AMD GPU):**
  - **Memory (RSS)**: `VmRSS = 83 956 kB` (~`82.0 MiB`), `RssAnon = 23 264 kB` (~`22.7 MiB`).
  - **CPU (Idle)**: `0.0%` (0 scheduler ticks over 3s probe).
  - **Regression Check**: Memory improved from ~105 MiB to ~82 MiB (zero regression).
- **Checks Passed:**
  - [x] Release build zero warnings (`-Wall -Wextra -Wpedantic`).
  - [x] CTest 18/18 tests passed (100%).
  - [x] AddressSanitizer (ASan) 100% clean.
  - [x] UndefinedBehaviorSanitizer (UBSan) 100% clean.
  - [x] ThreadSanitizer (TSan) clean for worker threads.
  - [x] 10 consecutive launch/exit cycles clean.
  - [x] Fresh install & uninstall clean in isolated prefix.
  - [x] Hyprland `class: mqblox` matches `mqblox.desktop`.
- **Status:** Full Pass 1 COMPLETED. Commencing Full Pass 2.
