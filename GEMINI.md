# MqBlox Project Guidelines & Autonomous Specialized Agents

## Automatic Subagents Initialization & Persistence
On every session startup, Antigravity has 6 specialized subagents defined and ready for development, verification, security, and packaging. Their full system prompts and definitions are located in `agents/<agent-name>.md`:

1. `code-reviewer` (`agents/code-reviewer.md`): Audits modern C++20 and Qt6 code quality, RAII, memory management, thread safety, and architecture.
2. `security-auditor` (`agents/security-auditor.md`): Audits IPC, socket permissions, path traversal, sandbox boundaries, and process execution security.
3. `darling-specialist` (`agents/darling-specialist.md`): Specialist in Darling macOS runtime compatibility, Mach subsystems, dynamic linking, shims, OpenGL rendering, and Roblox client behavior.
4. `qa-tester` (`agents/qa-tester.md`): Specialist in automated building, unit tests (`tst_mqblox`), regression suites, and compilation verification.
5. `ui-ux-reviewer` (`agents/ui-ux-reviewer.md`): Specialist in Qt6 GUI, Dark Theme, accessibility, UX settings, and event-loop responsiveness.
6. `system-packager` (`agents/system-packager.md`): Specialist in Linux desktop integration, XDG compliance, Flatpak, PKGBUILD, and installation scripts.

Whenever working in a new Antigravity session or after restarts:
- Read `agents/<agent-name>.md` and register each agent using `define_subagent` if not already defined in the active session.
- Delegate tasks to the appropriate specialized subagent using `invoke_subagent`.
- Cross-agent collaboration and guidelines are enforced via `.agents/rules/specialized-agents.md` and `AGENTS.md`.

## Mandatory GitHub & Release Synchronization
- **Always push updates to GitHub**: Whenever any feature, bug fix, or update is implemented, Antigravity MUST always stage, commit, and push the changes to GitHub (`origin main`) as part of completing the task.
- **Installer & Package alignment**: Always keep `install.sh`, `PKGBUILD`, and `CHANGELOG.md` synchronized with the latest updates and dependencies.

## Core Technical Architecture
- **Language**: C++20 with Qt6 (`QtWidgets`, `QtNetwork`, `QtConcurrent`, `QtOpenGL`).
- **Target Platform**: Native Linux (Wayland & X11) managing Roblox via the Darling macOS translation layer.
- **Graphics Pipeline**:
  - **Roblox Client Execution**: Selectable between OpenGL 4.1 Core Profile (`FFlagDebugGraphicsPreferOpenGL=true`, CGL interceptor in `shims/gl_profile.c`) and Vulkan (`FFlagDebugGraphicsPreferVulkan=true`, `MACOBLOX_GRAPHICS_BACKEND=vulkan`, `MESA_LOADER_DRIVER_OVERRIDE=zink`, `GALLIUM_DRIVER=zink`).
  - **Host GUI & Wayland Presentation**: Qt6 Wayland / X11 with hardware acceleration via OpenGL and Vulkan (`vulkan-icd-loader`, `Qt6VulkanServerBufferPlugin`).
- **Privacy & Security**: Never read, store, or transmit `.ROBLOSECURITY` session cookies. Protect local prefix slots under `~/Library/MacOBlox`.
