# MqBlox Architectural & Technical Decisions Record (ADR)

## DEC-001: Autonomous Team Structure & Communication Protocol
- **Date:** 2026-10-24
- **Decision:** Use file-based communication in repository root (`PLAN.md`, `TASKS.md`, `PROGRESS.md`, `DECISIONS.md`, `NEEDS_USER.md`).
- **Rationale:** Ensures resilience across session boundaries, token limits, and agent restarts. State is always reconstructible.

## DEC-002: Approved Skills Inventory
- **Date:** 2026-10-24
- **Decision:** The following skills are approved and registered for project use:
  - `ui-ux-pro-max`: UI/UX design intelligence, contrast, layout, and ergonomic guidelines.
  - `darling-macos-ops`: Darling Mach subsystem lifecycle, dynamic linking shims, and log diagnosis.
  - `qt6-desktop-expert`: Non-blocking GUI event loops, asynchronous concurrency, and Qt6 Wayland integration.
  - `linux-graphics-pipeline`: OpenGL 4.1 Core Profile CGL interception and Vulkan/Zink configuration.
  - `roblox-fastflags-manager`: ClientAppSettings.json validation and FPS uncapping.
  - `linux-packaging-xdg`: Multi-distribution packaging and XDG directory standards.
  - `cpp-pro`, `clean-code`, `code-review-excellence`: C++20 quality and refactoring.
- **Constraints:** No `curl | bash`, no unverified network downloads, strictly local execution.

## DEC-003: Process Isolation & Darling Cleanup Contract
- **Date:** 2026-10-24
- **Decision:** When `mqblox` terminates or when a game session stops, `DarlingController` must ensure no orphaned `RobloxPlayer`, `darling`, or `darlingserver` processes remain, and stale `.darlingserver.sock` or `.init.pid` files in `$DPREFIX` or `/tmp` are cleaned up.
- **Rationale:** Prevents launch failures on subsequent runs and resource leaks on host.

## DEC-004: Graphics Backend Strategy
- **Date:** 2026-10-24
- **Decision:** Support both OpenGL 4.1 Core Profile (via `shims/gl_profile.c`) and Vulkan (via Mesa Zink `GALLIUM_DRIVER=zink`).
- **Rationale:** Ensures optimal compatibility across diverse AMD, Intel, and NVIDIA GPUs on modern Linux Wayland environments.
