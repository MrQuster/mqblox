---
name: linux-packaging-xdg
description: Linux packaging standards, distribution installers (install.sh, PKGBUILD, Flatpak), XDG Base Directory specification compliance, and desktop integration.
risk: medium
source: project
date_added: '2026-10-24'
---

## Use this skill when
- Updating or auditing `install.sh` for Arch, Debian, Ubuntu, and Fedora.
- Maintaining Arch Linux `PKGBUILD` and AUR packaging.
- Creating or editing Flatpak manifests (`com.github.mrquster.mqblox.json`).
- Auditing filesystem paths for XDG Base Directory Specification compliance.
- Registering MIME types (`roblox-player://`) and `.desktop` application launchers.

## Do not use this skill when
- Working exclusively on C++ internal algorithms (use `cpp-pro`).
- Working on graphics shader or OpenGL context negotiation (use `linux-graphics-pipeline`).

## Standards & Best Practices

### 1. XDG Base Directory Specification Compliance
Always resolve directory paths dynamically using standard environment variables:
- **Configuration**: `$XDG_CONFIG_HOME/mqblox` (default: `~/.config/mqblox`)
  - Stored: UI geometry, user preferences, fastflag overrides.
- **Data**: `$XDG_DATA_HOME/mqblox` (default: `~/.local/share/mqblox`)
  - Stored: Darling prefixes, downloaded game caches, shims.
- **Cache**: `$XDG_CACHE_HOME/mqblox` (default: `~/.cache/mqblox`)
  - Stored: Temporary assets, updater artifacts.
- **Runtime**: `$XDG_RUNTIME_DIR/mqblox` (default: `/run/user/<uid>/mqblox`)
  - Stored: UNIX domain sockets (`mqblox_ipc.sock`), lock files (mode `0700`).

### 2. Desktop Integration Files
- **Launcher Entry (`mqblox.desktop`)**:
  ```ini
  [Desktop Entry]
  Type=Application
  Name=mqblox
  GenericName=Roblox Manager for Linux
  Comment=Native Linux manager and launcher for Roblox via Darling
  Exec=mqblox %u
  Icon=mqblox
  Terminal=false
  Categories=Game;Utility;
  MimeType=x-scheme-handler/roblox-player;x-scheme-handler/roblox;
  ```
- **MIME Type Association**:
  ```bash
  xdg-mime default mqblox.desktop x-scheme-handler/roblox-player
  xdg-mime default mqblox.desktop x-scheme-handler/roblox
  update-desktop-database ~/.local/share/applications/
  ```

### 3. Multi-Distro Dependency Verification
Ensure `install.sh` handles all three major distro package families:
- **Arch / EndeavourOS / Manjaro**:
  `pacman -S --needed qt6-base qt6-wayland cmake ninja vulkan-icd-loader`
- **Debian / Ubuntu / Linux Mint**:
  `apt-get install -y qt6-base-dev qt6-wayland-dev cmake ninja-build libvulkan1`
- **Fedora / RHEL**:
  `dnf install -y qt6-qtbase-devel qt6-qtwayland-devel cmake ninja-build vulkan-loader`
