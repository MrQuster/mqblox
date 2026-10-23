---
name: darling-macos-ops
description: Comprehensive operational guide and debugging procedures for the Darling macOS emulation layer, Mach subsystems, prefix lifecycle, and dynamic shims injection.
risk: medium
source: project
date_added: '2026-10-24'
---

## Use this skill when
- Initializing, repairing, or configuring Darling prefixes (`dsystem`, `darling shell`).
- Debugging Mach port communications, thread signaling, or `semaphore_timedwait` hangs.
- Developing or compiling Mach-O shims (`shims/*.c`) and dynamic library hooks (`DYLD_INSERT_LIBRARIES`).
- Analyzing Darling runtime logs and diagnosing macOS Cocoa/CoreFoundation/Metal missing symbols or crash signatures.
- Managing macOS environment variables (`DYLD_FALLBACK_LIBRARY_PATH`, `DARLING_LOG_LEVEL`).

## Do not use this skill when
- The task is purely native Linux Qt6 GUI styling (use `qt6-desktop-expert`).
- The task is desktop file associations or distribution packaging (use `linux-packaging-xdg`).

## Core Concepts & Darling Subsystems

### 1. Prefix Management (`DarlingController`)
- **Prefix Path**: Defaults to `~/.local/share/mqblox/darling_prefix` or user-configured path.
- **Initialization**: Run `darling prefix init` or launch an isolated shell with `DPREFIX=<path> darling shell`.
- **Environment Isolation**: Always isolate environment variables so host Linux configurations do not bleed uncontrollably into the Darling Mach runtime.

### 2. Dynamic Library Injection & Shims
- **Mechanism**: Use `DYLD_INSERT_LIBRARIES` to inject native macOS shims into Roblox or other target binaries.
- **Order of Injection**:
  1. `libsystem_dtrace.dylib`: Stubs out probes that cause assertion failures on Darling.
  2. `libgl_profile.dylib`: Intercepts `CGLChoosePixelFormat` to force OpenGL 4.1 Core Profile.
  3. `libmissing_symbols.dylib`: Supplies dummy implementations of unexported frameworks.
- **Compilation**: Mach-O libraries must be compiled using Darling's internal toolchain or cross-compiled with:
  ```bash
  clang -target x86_64-apple-darwin -dynamiclib -o libgl_profile.dylib gl_profile.c
  ```

### 3. Log Filtering & Diagnosis
- Benign Darling logs (e.g. harmless unimplemented sysctls, pseudo-tty warnings) should be silenced to avoid log spam.
- Critical errors to catch:
  - `dyld: Library not loaded`: Missing dylib dependency.
  - `EXC_BAD_INSTRUCTION` / `SIGILL`: AVX/CPU instruction mismatch or malformed stub.
  - `EXC_BAD_ACCESS` / `SIGSEGV`: Memory violation or uninitialized Mach port.
  - `semaphore_timedwait_signal_trap`: Deadlock in Mach thread coordination.

### 4. Verification Checklist
1. Verify prefix exists and `darling prefix status` reports healthy.
2. Verify all injected dylibs exist at the target paths before spawning `darling shell`.
3. Check permissions of socket endpoints inside Darling `/tmp/`.
