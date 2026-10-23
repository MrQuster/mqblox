---
name: qt6-desktop-expert
description: Modern Qt6 and C++20 desktop application engineering: asynchronous non-blocking GUI event loops, custom widgets, QSS Liquid Glass dark theming, DPI scaling, and Wayland/X11 windowing.
risk: medium
source: project
date_added: '2026-10-24'
---

## Use this skill when
- Building, refining, or debugging Qt6 GUI widgets, dialogs, and window layouts.
- Developing asynchronous background tasks (`QThread`, `QThreadPool`, `QtConcurrent`) that interact with UI.
- Styling widgets with Qt Style Sheets (QSS), dark themes, transparency, or animations.
- Ensuring HiDPI scalability and seamless rendering under Wayland and X11.
- Diagnosing GUI hangs, dropped frames, or main thread blocking.

## Do not use this skill when
- Dealing with low-level Darling Mach-O shims (use `darling-macos-ops`).
- Working purely on C++ algorithms or data structures without Qt involvement (use `cpp-pro`).

## Key Architectural Principles

### 1. Non-Blocking Event Loop (GUI Responsiveness)
- **Zero Heavy I/O on Main Thread**: Never call `QProcess::waitForFinished()`, synchronous network requests, or long filesystem traversals on the GUI thread.
- **Asynchronous Execution Patterns**:
  - For simple one-off jobs: Use `QtConcurrent::run([=](){ ... })` combined with `QFutureWatcher`.
  - For long-running stateful services (`DarlingController`, background downloader): Use dedicated worker objects moved to a `QThread` via `moveToThread(&workerThread)`.
  - Communicate strictly via Qt queued signal-slot connections.

### 2. Modern Qt6 Signal-Slot Binding
- Always use compile-time pointer-to-member-function syntax:
  ```cpp
  connect(sender, &SenderClass::valueChanged, this, &ReceiverClass::onValueChanged);
  ```
- When capturing `this` in lambdas, always provide `this` as the context object:
  ```cpp
  connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
      handleProcessExit(exitCode, status);
  });
  ```

### 3. HyperOS Liquid Glass & Dark QSS Theme
- Maintain dark palette: `#121214` (surface background), `#1c1c20` (panels), `#282830` (borders/inputs).
- Accent colors: Electric cyan/blue `#007aff` or `#0a84ff`.
- Frosted glass effect: Use subtle translucent alpha channels `#ffffff10` for card borders and hover highlights.
- Contrast & Accessibility: Adhere to WCAG AA contrast (minimum 4.5:1 text-to-background ratio).

### 4. Wayland & HiDPI Standards
- Ensure automatic DPI scaling:
  ```cpp
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  ```
- Test window resizing, fractional scaling (125%, 150%), and window minimization under Wayland compositors (Sway, Hyprland, GNOME Wayland).
