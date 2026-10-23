---
description: Guidelines and roles for specialized review, security, Darling, QA, UI/UX, and packaging agents in mqblox.
---

# Specialized Agents Guidelines & Autonomous Team Architecture

This workspace utilizes 6 specialized agents to ensure code quality, platform isolation, macOS runtime translation, stability, and desktop compliance.

## Agent Specifications & Registration
System prompts and full capability specifications are stored in `agents/<agent-name>.md`:
- `code-reviewer`: `agents/code-reviewer.md`
- `security-auditor`: `agents/security-auditor.md`
- `darling-specialist`: `agents/darling-specialist.md`
- `qa-tester`: `agents/qa-tester.md`
- `ui-ux-reviewer`: `agents/ui-ux-reviewer.md`
- `system-packager`: `agents/system-packager.md`

Across restarts and new Antigravity sessions:
1. Verify if the agent is defined. If not, define it via `define_subagent` using the prompt in `agents/<agent-name>.md`.
2. Invoke specialized subagents via `invoke_subagent` for dedicated domain tasks.

## Agent Perspectives & Requirements

1. **Code Quality (`code-reviewer`)**:
   - Modern C++20 standards and clean architecture.
   - Enforce RAII and smart pointer ownership (`std::unique_ptr`, `std::shared_ptr`, `QPointer`).
   - Validate Qt parent-child hierarchy to prevent leaks or use-after-free bugs.
   - Maintain const-correctness and explicit error reporting.

2. **Security & Sandbox Isolation (`security-auditor`)**:
   - Strict permissions on IPC sockets (`QLocalServer`, UNIX domain sockets).
   - Strict path traversal defenses (`../` checks).
   - Safe `QProcess` argument arrays without unescaped shell concatenation.
   - Never run subprocesses with unnecessary elevated permissions.
   - Guard against credential / cookie leakage (`.ROBLOSECURITY`).

3. **Darling Subsystem & Shims (`darling-specialist`)**:
   - Maintain `DarlingController` lifecycle and prefix isolation.
   - Maintain Darwin Mach-O shims (`shims/*.c`, `shims/*.m`) and dynamic library injection (`DYLD_INSERT_LIBRARIES`).
   - OpenGL 4.1 Core Profile enforcement (`gl_profile.c`).
   - ABI stability and stub frameworks for missing macOS APIs (`missing_symbols.c`).

4. **QA & Regression Verification (`qa-tester`)**:
   - Enforce `-Wall -Wextra -Wpedantic` compilation with zero compiler warnings.
   - Run unit test suite `tst_mqblox` via CTest on code changes.
   - Memory sanity checks and offscreen rendering tests (`QT_QPA_PLATFORM=offscreen`).

5. **UI/UX & Thread Responsiveness (`ui-ux-reviewer`)**:
   - Ensure the main GUI event loop is never blocked by process I/O or prefix operations.
   - Modern HyperOS 4 Liquid Glass dark theme fidelity (`resources/stylesheet.qss`).
   - Support HiDPI, keyboard navigation, and native Wayland/X11 rendering.

6. **System Packaging & Desktop Standards (`system-packager`)**:
   - Strict XDG Base Directory compliance (`~/.config`, `~/.local/share`, `~/.cache`).
   - Maintain `install.sh`, `PKGBUILD`, and `mqblox.desktop`.
   - MIME type associations (`x-scheme-handler/roblox`, `x-scheme-handler/roblox-player`).
   - Verify dependencies, including Vulkan loader libraries (`vulkan-icd-loader`) for Qt6/Wayland acceleration.

7. **Mandatory GitHub & Installer Synchronization**:
   - Always keep `install.sh`, `PKGBUILD`, and `CHANGELOG.md` updated with every release/change.
   - Whenever any task or update is completed, always commit and push the changes to GitHub (`git push origin main`).
