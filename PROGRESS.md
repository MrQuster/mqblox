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
     - `packaging/mqblox.desktop` updated to `StartupWMClass=mqblox`. Verified with `hyprctl clients` that client metadata matches 100%.
  2. **Socket & PID Teardown Robustness**:
     - `DarlingController::clearStaleDarlingProcesses()` expanded to purge `.init.pid` and `.darlingserver.sock` across all candidate prefix directories (`~/.darling`, `~/.local/share/darling`, `~/.var/app/xyz.narez.MacOBlox/data/darling`, and `/tmp/`). Added `testStaleSocketAndPidCleanup()`.
  3. **ThreadSanitizer Calibration**:
     - Added `tests/tsan.supp` to suppress data race false-positives in uninstrumented prebuilt system libraries (`libQt6Core.so`, `libQt6DBus.so`, `libQt6Test.so`). Verified 100% clean TSan pass on application code.
  4. **10x Consecutive Launch & Exit Verification**:
     - Executed automated 10-iteration loop via `tests/verify_10x_launch_exit.py`. All 10 cycles exited cleanly with 0 leftover processes and 0 stale socket files.
  5. **Clean Installation & Uninstallation**:
     - Executed `./install.sh install --prefix /tmp/test_prefix` followed by `./install.sh uninstall`. Verified zero leftover files.
  6. **UI Responsiveness**:
     - Audited search debounce, `LockedScrollArea`, mouse wheel filter on sliders, and single-buffer OpenGL backdrop. Verified zero main-thread stalls > 100ms.
- **Resource Measurements (Hyprland / Arch Linux / AMD GPU):**
  - **Memory (RSS)**: `VmRSS = 83 956 kB` (~`82.0 MiB`), `RssAnon = 23 264 kB` (~`22.7 MiB`).
  - **CPU (Idle)**: `0.0%` (0 scheduler ticks over 3s probe).
- **Status:** Full Pass 1 COMPLETED.

---

### Cycle 2: Independent Re-Verification & Regression Check (Full Pass 2)
- **Date:** 2026-10-24
- **Focus:** Complete re-execution of all DONE criteria for consecutive cycle confirmation.
- **Verification Outcomes:**
  1. **Release Build & CTest**:
     - `cmake --build build -j$(nproc)` compiled cleanly with `-Wall -Wextra -Wpedantic` and zero warnings.
     - `ctest` passed 18/18 tests (100% pass, real time: 0.43s).
  2. **10x Consecutive Launch & Exit Loop**:
     - `tests/verify_10x_launch_exit.py` executed: 10/10 iterations clean. Zero lingering `darling`/`darlingserver`/`RobloxPlayer`/`mldr` processes. Zero stale `.init.pid`/`.darlingserver.sock` files.
  3. **Idle Metrics & Regression Check**:
     - `VmRSS = 83 336 kB` (~`81.4 MiB`), `RssAnon = 23 284 kB` (~`22.7 MiB`).
     - Zero memory regression compared to Cycle 1 (82.0 MiB -> 81.4 MiB).
     - CPU idle: `0.0%`.
  4. **Clean Install / Uninstall in Isolated Prefix**:
     - Clean install to `/tmp/test_prefix_pass2` and clean uninstall executed via `install.sh`. Zero leftovers.
  5. **Hyprland Client & Desktop Association**:
     - `class: mqblox`, `initialClass: mqblox`, `title: MqBlox` matched to `mqblox.desktop` (`StartupWMClass=mqblox`).
  6. **Sanitizers Status**:
     - ASan and UBSan: clean on full test suite.
     - TSan: clean with `tests/tsan.supp`.
- **Status:** Full Pass 2 COMPLETED. ALL DONE CRITERIA SATISFIED FOR TWO CONSECUTIVE CYCLES.
