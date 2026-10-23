---
name: roblox-fastflags-manager
description: Roblox Client architecture, FastFlags configuration (ClientAppSettings.json), framerate uncapping, protocol URL launch arguments, and prefix filesystem structures.
risk: low
source: project
date_added: '2026-10-24'
---

## Use this skill when
- Managing Roblox FastFlags (`ClientAppSettings.json`) for performance tuning and compatibility.
- Configuring graphics rendering backend flags (`FFlagDebugGraphicsPreferOpenGL`, `FFlagDebugGraphicsPreferVulkan`, `FFlagDebugGraphicsPreferMetal`).
- Uncapping framerates (`DFIntTaskSchedulerTargetFps`).
- Parsing and generating launch URLs and command-line arguments for Roblox (`roblox-player://`).
- Managing Roblox client directory structures in macOS prefixes (`Applications/Roblox.app`).

## Do not use this skill when
- Building native Linux Qt6 UI widgets (use `qt6-desktop-expert`).
- Debugging low-level Mach kernel ports (use `darling-macos-ops`).

## FastFlags Configuration Playbook

### 1. Location in macOS Prefix
Roblox reads client settings from:
```
<prefix>/Users/<user>/Applications/Roblox.app/Contents/MacOS/ClientSettings/ClientAppSettings.json
```
If the `ClientSettings` directory does not exist, `mqblox` creates it with mode `0755`.

### 2. Essential Compatibility FastFlags

| FastFlag Key | Recommended Value | Purpose |
|---|---|---|
| `FFlagDebugGraphicsPreferOpenGL` | `"True"` / `true` | Forces OpenGL backend (used with `gl_profile.c`) |
| `FFlagDebugGraphicsPreferVulkan` | `"True"` / `true` | Forces Vulkan backend (used with Mesa Zink) |
| `FFlagDebugGraphicsPreferMetal` | `"False"` / `false`| Disables Metal on Darling (prevents unhandled Metal crashes) |
| `DFIntTaskSchedulerTargetFps` | `144` / `240` / `999` | Uncaps FPS beyond default 60 FPS |
| `FFlagDisablePostFx` | `true` (optional) | Increases FPS on lower-end Linux GPUs |

### 3. Launch URL & URI Protocol Handling
- **Format**:
  `roblox-player:1+launchmode:play+gameinfo:<ticket>+launchtime:<timestamp>+placelauncherurl:<url>`
- **Security Check**:
  - Always validate URI parameters against command injection and path traversal.
  - Never allow raw shell execution of unescaped query string parameters.
  - Reject URLs that do not start with `roblox-player:` or `roblox:`.
