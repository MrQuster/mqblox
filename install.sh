#!/usr/bin/env bash
# MqBlox × MacOBlox One-Command Installer / Updater / Uninstaller
# License: MIT (Copyright (c) 2026 Narezany, Copyright (c) 2026 MrQuster)
set -euo pipefail

REPO_URL="${MQBLOX_REPO_URL:-https://github.com/MrQuster/mqblox}"
APP_NAME="mqblox"
MODE="install"
PREFIX="${PREFIX:-$HOME/.local}"
RELEASE_TAG=""

usage() {
  cat <<'EOF'
Usage: ./install.sh [install|update|uninstall] [options]

Commands:
  install               Build and install MqBlox (default)
  update                Update from Git tag / GitHub Release and reinstall
  uninstall             Remove all installed MqBlox files cleanly

Options:
  --prefix <dir>        Installation prefix (default: ~/.local, or /usr/local with --system)
  --system              Install into /usr/local (requires root/sudo)
  --tag <vX.Y.Z|latest> Download and install a versioned tarball from GitHub Releases
  -h, --help            Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    install|update|uninstall)
      MODE="$1"
      shift
      ;;
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    --system)
      PREFIX="/usr/local"
      shift
      ;;
    --tag)
      RELEASE_TAG="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

detect_distro() {
  if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    printf '%s' "${ID:-linux}"
  else
    printf 'linux'
  fi
}

check_dependencies() {
  local distro
  distro=$(detect_distro)
  local missing=()

  for cmd in cmake clang unzip; do
    command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
  done

  export PATH="$HOME/.local/bin:$PATH"
  if ! command -v ld.lld >/dev/null 2>&1 && ! command -v ld64.lld >/dev/null 2>&1; then
    missing+=("lld")
  fi

  if ! command -v qmake6 >/dev/null 2>&1 && ! command -v qmake >/dev/null 2>&1 && [[ ! -d /usr/lib/cmake/Qt6 && ! -d /usr/lib64/cmake/Qt6 ]]; then
    missing+=("qt6-base/qt6-svg")
  fi

  if [[ ${#missing[@]} -gt 0 ]]; then
    printf '[!] Missing build/runtime dependencies: %s\n' "${missing[*]}" >&2
    case "$distro" in
      arch|cachyos|endeavouros|manjaro)
        printf '    Install on Arch Linux: sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-svg qt6-wayland clang lld unzip\n' >&2
        ;;
      fedora|nobara)
        printf '    Install on Fedora: sudo dnf install -y cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtsvg-devel clang lld unzip\n' >&2
        ;;
      debian|ubuntu|linuxmint|pop)
        printf '    Install on Debian/Ubuntu: sudo apt install -y build-essential cmake ninja-build qt6-base-dev libqt6svg6-dev clang lld unzip\n' >&2
        ;;
      *)
        printf '    Please install CMake, Qt6 (Core, Gui, Widgets, Network, OpenGL, Svg, Test), clang, lld, and unzip.\n' >&2
        ;;
    esac
    exit 1
  fi
}

refresh_desktop_caches() {
  local share_dir="$PREFIX/share"
  if command -v update-desktop-database >/dev/null 2>&1 && [[ -d "$share_dir/applications" ]]; then
    update-desktop-database "$share_dir/applications" 2>/dev/null || true
  fi
  if command -v gtk-update-icon-cache >/dev/null 2>&1 && [[ -d "$share_dir/icons/hicolor" ]]; then
    gtk-update-icon-cache -q -t -f "$share_dir/icons/hicolor" 2>/dev/null || true
  fi
  if command -v xdg-mime >/dev/null 2>&1 && [[ -f "$share_dir/applications/mqblox.desktop" ]]; then
    xdg-mime default mqblox.desktop x-scheme-handler/roblox 2>/dev/null || true
    xdg-mime default mqblox.desktop x-scheme-handler/roblox-player 2>/dev/null || true
  fi
}

do_uninstall() {
  printf '[1/2] Removing MqBlox files from %s ...\n' "$PREFIX"
  rm -f -- "$PREFIX/bin/mqblox" "$PREFIX/bin/mqblox-bin"
  rm -rf -- "$PREFIX/share/mqblox" "$PREFIX/share/licenses/mqblox"
  rm -f -- "$PREFIX/share/applications/mqblox.desktop"
  rm -f -- "$PREFIX/share/icons/hicolor/scalable/apps/mqblox.svg"
  for sz in 16 22 24 32 48 64 128 256 512; do
    rm -f -- "$PREFIX/share/icons/hicolor/${sz}x${sz}/apps/mqblox.png"
  done
  printf '[2/2] Refreshing desktop and icon caches...\n'
  refresh_desktop_caches
  printf '[OK] MqBlox uninstalled cleanly from %s.\n' "$PREFIX"
}

if [[ "$MODE" == "uninstall" ]]; then
  do_uninstall
  exit 0
fi

if [[ -n "${BASH_SOURCE[0]:-}" && -f "${BASH_SOURCE[0]}" ]]; then
  SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
else
  SCRIPT_DIR=""
fi
WORK_SRC="$SCRIPT_DIR"
TMP_DL=""
cleanup() {
  if [[ -n "$TMP_DL" && -d "$TMP_DL" ]]; then
    rm -rf -- "$TMP_DL"
  fi
}
trap cleanup EXIT

if [[ -n "$RELEASE_TAG" ]] || [[ -z "$SCRIPT_DIR" ]] || [[ ! -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
  check_dependencies
  TMP_DL=$(mktemp -d)
  if [[ -n "$RELEASE_TAG" ]]; then
    tar_url="$REPO_URL/archive/refs/tags/$RELEASE_TAG.tar.gz"
  else
    tar_url="$REPO_URL/archive/refs/heads/main.tar.gz"
  fi
  printf '[*] Downloading MqBlox source archive (%s)...\n' "$tar_url"
  if ! curl -fsSL "$tar_url" -o "$TMP_DL/src.tar.gz"; then
    tar_url="$REPO_URL/archive/refs/tags/v36.0.0.tar.gz"
    printf '[*] Falling back to tag archive (%s)...\n' "$tar_url"
    curl -fsSL "$tar_url" -o "$TMP_DL/src.tar.gz"
  fi
  mkdir -p "$TMP_DL/extracted"
  tar -xzf "$TMP_DL/src.tar.gz" -C "$TMP_DL/extracted" --strip-components=1
  WORK_SRC="$TMP_DL/extracted"
elif [[ "$MODE" == "update" && -d "$SCRIPT_DIR/.git" ]]; then
  printf '[*] Updating local Git checkout...\n'
  git -C "$SCRIPT_DIR" fetch --tags --prune
  if [[ -n "$RELEASE_TAG" ]]; then
    git -C "$SCRIPT_DIR" checkout "$RELEASE_TAG"
  else
    git -C "$SCRIPT_DIR" pull --ff-only
  fi
fi

check_dependencies

printf '[1/4] Configuring & building MqBlox C++20/Qt6 launcher...\n'
cmake -S "$WORK_SRC" -B "$WORK_SRC/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$WORK_SRC/build" -j"$(nproc)"

printf '[2/4] Running unit test suite (ctest)...\n'
QT_QPA_PLATFORM=offscreen ctest --test-dir "$WORK_SRC/build" --output-on-failure

printf '[3/4] Installing MqBlox to %s ...\n' "$PREFIX"
cmake --install "$WORK_SRC/build"

# Ensure Exec in installed .desktop points cleanly to the installed binary if ~/.local/bin is not in system PATH
if [[ "$PREFIX" == "$HOME/.local" && -f "$PREFIX/share/applications/mqblox.desktop" ]]; then
  sed -i "s|^Exec=mqblox %u$|Exec=$PREFIX/bin/mqblox %u|" "$PREFIX/share/applications/mqblox.desktop"
fi

printf '[4/4] Building Darwin Mach-O shim (libMacOBloxShims.dylib) if Darling sysroot is present...\n'
if "$PREFIX/share/mqblox/shims/build_debug_shim.sh" >/dev/null 2>&1; then
  mkdir -p "${XDG_DATA_HOME:-$HOME/.local/share}/macoblox"
  cp -f "$PREFIX/share/mqblox/shims/build/libMacOBloxShims.dylib" \
    "${XDG_DATA_HOME:-$HOME/.local/share}/macoblox/libMacOBloxShims.dylib"
  printf '      Built and installed libMacOBloxShims.dylib -> %s\n' \
    "${XDG_DATA_HOME:-$HOME/.local/share}/macoblox/libMacOBloxShims.dylib"
else
  printf '      (Skipped pre-building dylib: Darling sysroot not yet installed; First-Run Wizard will build it on demand)\n'
fi

refresh_desktop_caches
printf '[OK] MqBlox v36.0 (%s) completed successfully! Binary: %s/bin/mqblox\n' "$MODE" "$PREFIX"
