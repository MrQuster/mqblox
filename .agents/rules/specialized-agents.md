---
description: Guidelines and roles for specialized review, security, Darling, QA, UI/UX, and packaging agents in mqblox.
---

# Specialized Agents Guidelines

When modifying mqblox, the following agent perspectives must be applied:

1. **Code Quality (`code-reviewer`)**: Modern C++20, RAII, Qt parent-child hierarchy, no raw pointers for ownership, no resource leaks.
2. **Security (`security-auditor`)**: Strict IPC permissions, validated paths, safe `QProcess` argument arrays without unquoted shells, no privilege escalation.
3. **Darling Subsystem (`darling-specialist`)**: Safe Mach-O shim injection, robust fallback for missing macOS frameworks, clean log filtering.
4. **QA Verification (`qa-tester`)**: Compile with zero warnings, verify `ctest` / `tst_mqblox` passing, check edge cases.
5. **UI/UX Performance (`ui-ux-reviewer`)**: Keep main Qt thread non-blocking, responsive widgets, consistent dark mode styling, proper Wayland/X11 behavior.
6. **Packaging (`system-packager`)**: XDG paths compliance, robust `install.sh`, proper desktop MIME associations.
