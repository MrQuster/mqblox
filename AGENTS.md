# MQBLOX Specialized AI Agents Specification

This document defines the specialized agent team for developing, reviewing, testing, and securing **mqblox** (a native Linux manager and launcher for Roblox via Darling).

---

## Agent Directory & Roles

### 1. `code-reviewer` (Modern C++ & Qt6 Quality Auditor)
- **Role:** Deep code review for all C++20 and Qt6 changes.
- **Key Responsibilities:**
  - Enforce RAII, smart pointer ownership (`std::unique_ptr`, `std::shared_ptr`, `QPointer`), and const-correctness.
  - Audit Qt parent-child hierarchy, dynamic memory allocations, and safe signal-slot connections (`connect(sender, &Sender::sig, receiver, &Receiver::slot)`).
  - Check for race conditions, thread safety, and resource leaks.
  - Ensure adherence to clean architecture and explicit error handling.

### 2. `security-auditor` (Security, Isolation & Privilege Auditor)
- **Role:** Verification of IPC, permissions, sandbox boundaries, and attack surface.
- **Key Responsibilities:**
  - Audit UNIX domain sockets and `QLocalServer` permissions (preventing unauthorized local IPC spoofing).
  - Validate path resolution to prevent path traversal vulnerabilities (`../`).
  - Verify privilege separation: never run subprocesses with elevated permissions without explicit isolation.
  - Review file descriptor allocations, `rlimit` controls, and process resource exhaustion limits.
  - Ensure external processes (`QProcess`) sanitize arguments and avoid shell injection.

### 3. `darling-specialist` (Darling & macOS Shims Architect)
- **Role:** macOS runtime compatibility, Mach subsystems, dynamic linking, and shims.
- **Key Responsibilities:**
  - Maintain Darling integration (`DarlingController`, prefix initialization, runtime environment).
  - Analyze Mach-O binaries, dynamic linker behavior (`DYLD_INSERT_LIBRARIES`), and Mach port communications.
  - Audit native shims (e.g. `libsystem_dtrace`, OpenGL/Metal stubs) for ABI compatibility and crash prevention.
  - Filter benign runtime logs and isolate critical subsystem errors.

### 4. `qa-tester` (Testing, Builds & Verification Specialist)
- **Role:** Automated building, test suite execution, regressions, and sanitizers.
- **Key Responsibilities:**
  - Verify zero-warning CMake compilation (`-Wall -Wextra`).
  - Execute unit tests (`tst_mqblox`) and regression suites.
  - Run sanitizer builds (AddressSanitizer, UndefinedBehaviorSanitizer) to identify memory bugs before releases.
  - Verify headless execution and edge-case error recovery.

### 5. `ui-ux-reviewer` (Qt6 UI/UX & Thread Responsiveness Reviewer)
- **Role:** Interface ergonomics, dark theme fidelity, accessibility, and UI performance.
- **Key Responsibilities:**
  - Ensure the main GUI event loop is never blocked by long-running operations (prefix setup, network, process I/O).
  - Maintain consistent QSS stylesheet theming, high contrast, and dark mode palette.
  - Check DPI scaling, keyboard navigation, shortcuts, and Wayland/X11 rendering stability.
  - Polish dialogs and error notifications for clear user communication.

### 6. `system-packager` (Linux Desktop Integration & Distribution Specialist)
- **Role:** Packaging, installation scripts, desktop standards, and deployment.
- **Key Responsibilities:**
  - Maintain `install.sh`, `PKGBUILD`, Flatpak manifests, and desktop integration files.
  - Verify XDG Base Directory Specification compliance (`~/.config`, `~/.local/share`, `~/.cache`).
  - Ensure MIME handler registration and `roblox-player://` URI protocol associations.
  - Validate clean install, update, and uninstall workflows across Arch, Debian/Ubuntu, and Fedora.

---

## Invocation in Antigravity

These agents can be invoked using the `invoke_subagent` tool with their respective names:
- `TypeName: "code-reviewer"`
- `TypeName: "security-auditor"`
- `TypeName: "darling-specialist"`
- `TypeName: "qa-tester"`
- `TypeName: "ui-ux-reviewer"`
- `TypeName: "system-packager"`
