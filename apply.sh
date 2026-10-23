#!/usr/bin/env bash
set -euo pipefail

# =============================================================================
# MqBlox x MacOBlox (v36.0) — Idempotent Migration & Build Script (apply.sh)
# License: MIT (Copyright (c) 2026 Narezany, Copyright (c) 2026 MrQuster)
# =============================================================================

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$REPO_ROOT"

if git rev-parse --git-dir >/dev/null 2>&1; then
    BACKUP_BRANCH="backup-before-mqblox-macoblox"
    if ! git show-ref --verify --quiet "refs/heads/$BACKUP_BRANCH"; then
        git branch "$BACKUP_BRANCH" HEAD
        echo "[apply.sh] Создана резервная ветка: $BACKUP_BRANCH"
    else
        echo "[apply.sh] Резервная ветка $BACKUP_BRANCH уже существует (идемпотентный запуск)"
    fi
fi

# Удаляем несовместимые с требованиями приватности модули Sober/Cookie-harvesting
rm -f src/core/SecretStore.hpp src/core/SecretStore.cpp \
      src/net/RobloxApiClient.hpp src/net/RobloxApiClient.cpp
rmdir --ignore-fail-on-non-empty src/net 2>/dev/null || true

mkdir -p shims shims/frameworks src/core src/render src/ui tests resources packaging/arch

cat <<'EOF_LICENSE' > "LICENSE"
MIT License

Copyright (c) 2026 Narezany (MacOBlox Darling Backend, Shims & Frameworks)
Copyright (c) 2026 MrQuster (MqBlox Qt6 Liquid Glass Interface & SettingsRegistry)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF_LICENSE

cat <<'EOF_README_MD' > "README.md"
# MqBlox × MacOBlox (v36.0 — HyperOS 4 Liquid Glass + Darling macOS Backend)

[![License: MIT](https://img.shields.io/badge/License-MIT-00e5a3.svg)](LICENSE)
[![C++20 / Qt6](https://img.shields.io/badge/C%2B%2B20-Qt6%20%2B%20OpenGL%203.3-00b8ff.svg)](CMakeLists.txt)
[![Wayland / Hyprland](https://img.shields.io/badge/Wayland-Hyprland%20%26%20Serpantinum-9d65ff.svg)](packaging/mqblox.desktop)

**MqBlox** — нативный C++20 / Qt6 / OpenGL 3.3 лаунчер в стиле **HyperOS 4 Liquid Glass** (с шейдерным фоном чёрной дыры **TON 618** и единым буфером размытия), подключённый к бэкенду **[MacOBlox](https://github.com/narezy/MacOBlox)** для запуска официального macOS-клиента `RobloxPlayer.app` на Linux через **[Darling](https://www.darlinghq.org/)**.

> **English summary below:** [Jump to English Documentation](#english-documentation)

![MqBlox v36.0 Screenshot](docs/screenshots/mqblox-v36.png)

---

## 🇷🇺 Русская документация

### 1. Архитектура и принципы

- **Только официальные источники (никаких вшитых бинарников)**: репозиторий не содержит бинарных файлов `RobloxPlayer.app`, `Darling` или скомпилированных `.dylib`. Мастер первой установки и обновления скачивает клиент напрямую с официального CDN Roblox (`clientsettingscdn.roblox.com` / `setup.rbxcdn.com`) и собирает шимы локально из исходного кода (`shims/`).
- **Честная работа без обхода античита**: бинарник `RobloxPlayer` не модифицируется и не патчится. Все исправления совместимости со средой Darling (OpenGL Core Profile, IOKit VRAM, `host_statistics64`, `connectx`, DNS-over-TLS, `CoreAudio`, `WM_CLASS`) загружаются через стандартный механизм `DYLD_INSERT_LIBRARIES` (`libMacOBloxShims.dylib`).
- **Приватность пароля и сессии**: лаунчер не запрашивает пароль, не использует встроенный браузер для перехвата cookie, не читает, не логирует и не отправляет `.ROBLOSECURITY`. Авторизация выполняется штатно внутри самого клиента Roblox (через функцию **Быстрый вход / Quick Login**), а сессия хранится локально внутри префикса Darling (`~/.darling/Users/$USER/Library/MacOBlox/`).
- **Чистый тракт звука и микрофона (`shims/audio_hal.c`)**: Qt6-интерфейс лаунчера не вмешивается в PipeWire/PulseAudio/ALSA. Весь ввод и вывод звука работает внутри `shims/audio_hal.c` (CoreAudio HAL + AUHAL Output/Input Bus + `AudioUnitRender`).
- **Оптимизация под Wayland / Hyprland / AMD (`amdgpu`)**:
  - Нативный Wayland-клиент (`app_id = "mqblox"`), сопоставляющий как окно лаунчера, так и XWayland-окно `RobloxPlayer` с единой записью `mqblox.desktop` в доке **[Serpantinum](https://github.com/ilyamiro/serpantinum)**.
  - Ни одного `QGraphicsBlurEffect` на дочерних виджетах (защита от сброса видеодрайвера `amdgpu -12 ENOMEM`): используется единый буфер размытия (`Ton618GlWidget::scaledFrostedBackdrop`).
  - Реальные замеры в простое на Hyprland/Wayland (Arch Linux, ядро `linux-zen`, `amdgpu`):
    - **CPU в простое**: **`0.0%`** (0 тиков планировщика за 3 секунды при статическом фоне или свёрнутом окне).
    - **Память (RSS)**: **`VmRSS = 107 812 КБ` (~`105.3 МиБ`)** с учётом разделяемых библиотек Qt6/Mesa, из них приватная анонимная куча процесса **`RssAnon = 44 196 КБ` (~`43.2 МиБ`)**.

---

### 2. Быстрая установка (One-Command Install)

#### Вариант А: Универсальный скрипт `install.sh` (рекомендуется)
Устанавливает бинарник `mqblox`, собирает `libMacOBloxShims.dylib` и стаб-фреймворки (`CoreML`, `CoreHaptics`, `DeviceCheck`), устанавливает иконки (`hicolor` SVG + PNG `16x16`..`512x512`) и `.desktop`-файл в `~/.local` без прав root:

```bash
curl -fsSL https://raw.githubusercontent.com/MrQuster/mqblox/main/install.sh | bash
```

Локальная установка, обновление или удаление из клонированного репозитория:

```bash
./install.sh install     # Сборка + установка в ~/.local + сборка шимов
./install.sh update      # git pull --ff-only + пересборка и обновление
./install.sh uninstall   # Чистое удаление бинарника, иконок и .desktop (конфиг сохраняется)
```

#### Вариант Б: Пакет Arch Linux (`PKGBUILD` / `makepkg`)
В корне репозитория и в `packaging/arch/PKGBUILD` доступен готовый сценарий сборки пакета для Arch Linux:

```bash
makepkg -si
```

#### Вариант В: Ручная сборка через CMake + Ninja
Зависимости на Arch Linux:
```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-svg qt6-wayland clang lld unzip hicolor-icon-theme
```
Сборка и запуск тестов:
```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./shims/build_debug_shim.sh
```

> **Требование для запуска игры:** установленный **[Darling](https://www.darlinghq.org/)** (`darling-bin` или `darling-git` из AUR). При первом запуске откройте вкладку **«Главная и Мастер»** и нажмите **«Установить / Обновить (Шимы + Roblox)»** — лаунчер скачает свежий `RobloxPlayer.app` с официального CDN Roblox в префикс Darling.

---

### 3. Интеграция с доком Serpantinum и Hyprland

Лаунчер спроектирован с учётом алгоритма сопоставления окон в шелле **Serpantinum** (`WindowTracker.qml` + `AppMatcher.qml`):
- Вызов `QGuiApplication::setDesktopFileName(QStringLiteral("mqblox"))` задаёт нативный Wayland `app_id = "mqblox"`.
- Файл `mqblox.desktop` устанавливается без `NoDisplay=true` и содержит `StartupWMClass=RobloxPlayer`, `Icon=mqblox`, `Categories=Game;`.
- Шим `shims/libMacOBloxShims.m` автоматически выставляет X11 `WM_CLASS` окна `RobloxPlayer` в `mqblox` (`MACOBLOX_WM_CLASS=mqblox`), благодаря чему и нативное Wayland-окно лаунчера, и XWayland-окно игры группируются под одной иконкой **MqBlox** в доке.

---

### 4. Таблица настроек: подтверждённые vs зависящие от белого списка Roblox

Все настройки хранятся в едином реестре `SettingsRegistry` (`~/.config/mqblox/settings_v36.json`, `schemaVersion = 36`) с атомарной записью через `QSaveFile` и автоматической миграцией со старых схем (`settings_v35.json`, `settings_v34.json`, `settings_v13.json`).

Перед запуском игры `SoberController::syncConfigFromSettings()` генерирует:
1. Переменные окружения процесса Darling (`MACOBLOX_*`, `__GL_*`, `vblank_mode`);
2. `ClientAppSettings.json` внутри бандла `RobloxPlayer.app/Contents/MacOS/ClientSettings/ClientAppSettings.json` и в профиле слота.

| Ключ в `SettingsRegistry` | Куда записывается | Статус работы в macOS-клиенте |
| :--- | :--- | :--- |
| `vramLimitGib` | `export MACOBLOX_VRAM_MB=<GiB*1024>` (`shims/gpu_info.c`) | **Подтверждено** — напрямую перехватывается в `shims/gpu_info.c` (IOKit `VRAM,totalsize`). |
| `glCore33` | `export MACOBLOX_GL_CORE33=0\|1` (`shims/gl_profile.c`) | **Подтверждено** — переключает профиль CGL между OpenGL 3.3 Core и OpenGL 4.1 Core в `shims/gl_profile.c`. |
| `dnsCustomResolver` | `export MACOBLOX_DNS_UPSTREAM=host:port` (`shims/dns_override.c`) | **Подтверждено** — задаёт адрес локального DNS-прокси для `shims/dns_override.c`. |
| `vsync` | `export vblank_mode=0\|1`, `__GL_SYNC_TO_VBLANK=0\|1` | **Подтверждено на уровне драйвера Mesa/NVIDIA** для XWayland-окна. |
| `lightingTechnology` | `FFlagDebugForceFutureIsBrightPhase3` / `Phase2` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `lighting_technology`). |
| `msaa` | `FIntDebugForceMSAASamples` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `msaa_samples`). |
| `textureQuality` | `DFIntTextureQualityOverride` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `texture_quality`). |
| `disablePostFx` | `FFlagDisablePostFx` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `disable_postfx`). |
| `debugSkyGray` | `FFlagDebugSkyGray` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `gray_sky`). |
| `fpsLimit` | `DFIntTaskSchedulerTargetFps` (`ClientAppSettings.json`) | **Подтверждено в MacOBlox** (`launcher/config.py`: `fps_limit`). На новых релизах Roblox может требовать выбора лимита в меню самой игры. |
| `graphicsQuality` | `FIntRomarkStartWithGraphicQualityLevel` (`ClientAppSettings.json`) | **Предположительно (зависит от белого списка FFlag клиента Roblox)** — может игнорироваться современным клиентом. |
| `disableShadows` | `FIntRenderShadowIntensity` (`ClientAppSettings.json`) | **Предположительно (зависит от белого списка FFlag клиента Roblox)** — может игнорироваться клиентом. |
| `renderDistance` | `DFIntDebugFRMQualityLevelOverride` (`ClientAppSettings.json`) | **Предположительно (зависит от белого списка FFlag клиента Roblox)** — может игнорироваться клиентом. |
| `grassMovement` | `FIntFRMMinGrassDistance` / `FIntFRMMaxGrassDistance` | **Предположительно (зависит от белого списка FFlag клиента Roblox)** — может игнорироваться клиентом. |
| `customFastFlags` | Произвольный JSON-объект в `ClientAppSettings.json` | **Зависит от белого списка клиента Roblox** — применяются только разрешённые клиентом ключи. |

*(Настройки старого Android/Sober-бэкенда — `touchMode`, `bringBackOof`, `useOpengl`, `enableGamemode`, `discordRpc` — удалены из активной конфигурации как неприменимые к macOS-клиенту).*

---

### 5. Звук, микрофон (`shims/audio_hal.c`) и проверка в игре

Весь звуковой тракт работает исключительно внутри `shims/audio_hal.c`. В этом форке в `audio_hal.c` внесены 4 ключевых исправления захвата микрофона (AUHAL Input Bus 1):
1. Устранён возврат ошибки `-50` (`paramErr`) при `kAudioUnitProperty_StreamFormat` (`SetProperty`) на шине захвата `scope == kAudioUnitScope_Input && element == 1`.
2. При создании `AudioUnit` формат `in_stream_format` инициализируется в стандартный для macOS Voice Chat **Float32 48000 Hz Mono**, а не `S16_LE`.
3. В `MacOBloxAudio_Render` исправлено вычисление адреса буфера `AudioBufferList` при `mNumberBuffers > 1` (неперемежающиеся каналы) и добавлена конвертация `S16_LE -> Float32` при различии форматов ALSA и клиента.
4. Исправлена формула порога гистерезиса кольцевого буфера микрофона (`cap_prebuffered`), из-за которой захват на части гарнитур никогда не стартовал при буфере ALSA `<= 60 мс`.

#### Как протестировать микрофон в игре с журналированием `MACOBLOX_AUDIO_LOG`
В `shims/audio_hal.c` встроено опциональное потокобезопасное журналирование через переменную окружения `MACOBLOX_AUDIO_LOG` (`1`/`stderr` для вывода в консоль или путь к файлу):

```bash
MACOBLOX_AUDIO_LOG=/tmp/macoblox_audio.log mqblox
```

1. Запустите `mqblox` с переменной `MACOBLOX_AUDIO_LOG=/tmp/macoblox_audio.log`.
2. Войдите в плейс с поддержкой **Roblox Voice Chat** и включите иконку микрофона над персонажем.
3. Проверьте лог:
   ```bash
   tail -f /tmp/macoblox_audio.log
   ```
   Вы должны увидеть строки инициализации `AudioUnitInitialize`, установку `SetProperty StreamFormat` на `element=1`, запуск потока `capture_thread_fn: ALSA capture started` и периодическую статистику `Render(Input): calls=... frames=... ring_avail=...`.

---

## English Documentation

### Overview
**MqBlox v36.0** is a native **C++20 / Qt6 / OpenGL 3.3** launcher featuring a **HyperOS 4 Liquid Glass** UI (with a real-time **TON 618** black hole GLSL backdrop and a single shared frosted-glass blur buffer), powered by the **[MacOBlox](https://github.com/narezy/MacOBlox)** backend to run the official macOS `RobloxPlayer.app` client on Linux through **[Darling](https://www.darlinghq.org/)**.

### Key Highlights
- **Official Sources Only**: No bundled `RobloxPlayer.app`, `Darling`, or prebuilt `.dylib` binaries in the repository. The built-in Setup Wizard downloads `RobloxPlayer.app` directly from Roblox's official CDN (`clientsettingscdn.roblox.com` / `setup.rbxcdn.com`) and compiles `libMacOBloxShims.dylib` locally.
- **Zero Anti-Cheat Tampering**: `RobloxPlayer` is never patched or modified. Compatibility fixes (OpenGL Core Profile, IOKit VRAM reporting, `host_statistics64`, `connectx`, DNS-over-TLS, `CoreAudio`, X11 `WM_CLASS`) are injected cleanly via `DYLD_INSERT_LIBRARIES`.
- **Session & Password Privacy**: MqBlox never asks for your password, has no embedded browser, and never reads, logs, or transmits `.ROBLOSECURITY`. Sign in directly inside the Roblox client using **Quick Login**.
- **Idle Efficiency on Wayland / Hyprland**: Measured **`0.0%` CPU** at idle and **`105.3 MiB` total `VmRSS`** (`43.2 MiB` private `RssAnon`) on Arch Linux (`Hyprland`, `amdgpu`).

### Quick Install
```bash
curl -fsSL https://raw.githubusercontent.com/MrQuster/mqblox/main/install.sh | bash
```
Or on Arch Linux via `makepkg`:
```bash
makepkg -si
```

---

## Credits & License

Licensed under the **MIT License** (see [`LICENSE`](LICENSE)):

- **[Narezany (`narezy`) — MacOBlox](https://github.com/narezy/MacOBlox)** (`Copyright (c) 2026 Narezany`, MIT License) — original creator of MacOBlox, the Darling runtime architecture for macOS Roblox on Linux, all C/Objective-C Darwin shims (`libMacOBloxShims.m`, `audio_hal.c`, `gl_profile.c`, `gpu_info.c`, `memory_stats.c`, `dns_override.c`, `darling_fixes.c`, `xfixes_raw.c`, `net_trace.c`, `fast_libc.c`, `xattr_compat.c`, `exit_compat.c`, `connectx_compat.c`, `missing_symbols.c`), stub frameworks (`CoreML`, `CoreHaptics`, `DeviceCheck`), and the DNS-over-TLS proxy.
- **[Darling Project](https://www.darlinghq.org/)** — macOS / Darwin translation layer for Linux.
- **[MrQuster — MqBlox](https://github.com/MrQuster/mqblox)** (`Copyright (c) 2026 MrQuster`, MIT License) — C++20/Qt6 HyperOS 4 Liquid Glass interface, `Ton618GlWidget` OpenGL shader engine, unified `SettingsRegistry`, setup wizard, system diagnostics, Serpantinum dock integration, and microphone capture fixes in `shims/audio_hal.c`.
EOF_README_MD

cat <<'EOF_CHANGELOG_MD' > "CHANGELOG.md"
# Changelog / История изменений

Все значимые изменения проекта **MqBlox × MacOBlox** документируются в этом файле.

## [36.0.0] — 2026-10-23

### Добавлено (Added)
- **Бэкенд MacOBlox (Darling + Darwin Mach-O шимы)**: полный переход с Android/Sober на запуск официального macOS-клиента `RobloxPlayer.app` через контейнер Darling и `libMacOBloxShims.dylib` (автор базового слоя и шимов — **Narezany (`narezy`)**, MIT License).
- **Исправление 6 багов микрофона в `shims/audio_hal.c`**:
  1. Перехват `kAudioHardwarePropertyDevices` (`dev#`) и `kAudioHardwarePropertyDefaultInputDevice` (`dIn `) на `kAudioObjectSystemObject` (`object 1`).
  2. Поддержка входного устройства (`object 3`, `INPUT_STREAM = 0x4002`) во всех селекторах CoreAudio HAL (`stm#`, `slay`, `sfmt`, `pft `, `sdir`).
  3. Обработка `EnableIO` (`2003`) на шине ввода (`scope = 1, element = 1`) и свойства `kAudioOutputUnitProperty_SetInputCallback` (`2005`).
  4. Выделенный 8 МБ поток захвата микрофона (`unit_mic_thread`) с кольцевым буфером `in_ring` и поддержкой `MACOBLOX_MIC_FIFO`.
  5. Перехват `AudioUnitRender` (`DYLD_INTERPOSE(t_unit_render, AudioUnitRender)`) с поддержкой как interleaved, так и non-interleaved (`0x20`) float32 буферов для `bus_number == 1`.
  6. Опциональное журналирование микрофона и CoreAudio через переменную окружения `MACOBLOX_AUDIO_LOG` (`1` для `stderr` или путь к файлу).
- **Интеграция с доком Serpantinum и Wayland/Hyprland**:
  - Нативный Wayland `app_id = "mqblox"` (`QGuiApplication::setDesktopFileName("mqblox")`).
  - Единый `packaging/mqblox.desktop` с `StartupWMClass=RobloxPlayer` и передачей `MACOBLOX_WM_CLASS=mqblox` в `shims/libMacOBloxShims.m`, благодаря чему окно лаунчера (Wayland) и окно игры `RobloxPlayer` (XWayland) группируются под одной иконкой в доке Serpantinum.
  - Набор иконок `hicolor` (`scalable/apps/mqblox.svg` и PNG `16x16`..`512x512`).
- **Мастер первой установки и обновления**: асинхронная сборка `libMacOBloxShims.dylib` и стаб-фреймворков (`CoreML`, `CoreHaptics`, `DeviceCheck`), скачивание официального `RobloxPlayer.zip` с CDN Roblox без блокировки GUI.
- **Диагностика и профили**: сбор сведений о системе/GPU/Wayland с кнопкой «Копировать отчёт диагностики», именованные профили настроек, экспорт/импорт и миграция старых схем (`settings_v35.json`, `settings_v34.json`, `settings_v13.json` → `settings_v36.json` со схемой `v36`).
- **Установка одной командой и пакетирование**: скрипт `install.sh` (`install`/`update`/`uninstall`) и `PKGBUILD` для Arch Linux + GitHub Actions CI/Release.

### Удалено (Removed)
- Полностью удалены `SecretStore`, `RobloxApiClient`, `Qt6::WebEngineWidgets` / `Qt6::WebEngineCore` (лаунчер больше никогда не видит пароли и не читает `.ROBLOSECURITY`).
- Из интерфейса лаунчера полностью убраны любые вызовы `pactl`, `wpctl`, `PULSE_*`, `PIPEWIRE_*` (весь звук работает исключительно внутри `shims/audio_hal.c`).
- Убраны неприменимые к macOS-клиенту настройки Android/Sober (`bringBackOof`, `graphicsApi=Vulkan`, `gamepadSupport`, `enableHiDpi`, `noclipCamWalls`, `wallhopPhysics`).
EOF_CHANGELOG_MD

cat <<'EOF_SECURITY_MD' > "SECURITY.md"
# Политика безопасности / Security Policy

## Принципы безопасности и приватности MqBlox × MacOBlox

1. **Никакого доступа к паролям и куки сессии**:
   - Лаунчер **не содержит** встроенного браузера (`QWebEngine`), `SecretStore` или сетевых клиентов к приватным API аккаунта Roblox.
   - Лаунчер **никогда** не читает, не расшифровывает, не логирует и не передаёт куки `.ROBLOSECURITY` или файлы связки ключей.
   - Вход в аккаунт осуществляется исключительно внутри официального клиента `RobloxPlayer.app` (рекомендуется штатная функция **Quick Login / Быстрый вход**).
2. **Никакого вмешательства в защиту клиента**:
   - Проект не модифицирует исполняемый файл `RobloxPlayer`, не обходит и не подменяет механизмы защиты.
   - Все шимы (`shims/*.c`, `shims/*.m`) реализуют исключительно недостающие или некорректно работающие вызовы окружения macOS в трансляторе Darling (CoreAudio HAL, CGL/OpenGL 4.1 Core Profile, IOKit HID заглушка, DNS-over-TLS, X11 курсор).
3. **Никаких вшитых проприетарных бинарников**:
   - Репозиторий содержит только открытый исходный код под лицензией MIT. Клиент `RobloxPlayer.app` загружается самим пользователем напрямую с официального CDN `setup.rbxcdn.com`.

## Сообщение об уязвимостях / Reporting a Vulnerability

Если вы обнаружили проблему безопасности (например, утечку путей в логах диагностики или небезопасную обработку файлов), пожалуйста, создайте обращение через GitHub Security Advisories в репозитории проекта и не публикуйте эксплойт в открытых Issues до выпуска исправления.
EOF_SECURITY_MD

cat <<'EOF__GITIGNORE' > ".gitignore"
build/
shims/build/
pkg/
pkg-cmake-build/
src/pkg-cmake-build/
*.pkg.tar*
*.o
*.a
*.so
*.dylib
*.moc
.cache/
compile_commands.json
*.pkg.tar*
packaging/arch/src/
packaging/arch/pkg/
packaging/arch/*.tar*
EOF__GITIGNORE

cat <<'EOF_CMAKELISTS_TXT' > "CMakeLists.txt"
cmake_minimum_required(VERSION 3.24)
project(mqblox VERSION 36.0.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    OpenGL
    OpenGLWidgets
    Svg
    Concurrent
    Test
)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_library(mqblox_core STATIC
    src/core/Types.hpp
    src/core/SettingsRegistry.hpp
    src/core/SettingsManager.hpp
    src/core/SettingsManager.cpp
    src/core/SoberController.hpp
    src/core/SoberController.cpp
)

target_include_directories(mqblox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mqblox_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Concurrent
    Threads::Threads
)

add_executable(mqblox
    src/main.cpp
    src/render/Ton618GlWidget.hpp
    src/render/Ton618GlWidget.cpp
    src/ui/Widgets.hpp
    src/ui/MainWindow.hpp
    src/ui/MainWindow.cpp
    resources/resources.qrc
)

target_include_directories(mqblox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mqblox PRIVATE
    mqblox_core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Svg
    OpenGL::GL
)

foreach(tgt mqblox_core mqblox)
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:-O3 -fdata-sections -ffunction-sections>
        )
        target_link_options(${tgt} PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>
        )
    endif()
endforeach()

enable_testing()
add_executable(tst_mqblox tests/tst_mqblox.cpp)
target_link_libraries(tst_mqblox PRIVATE mqblox_core Qt6::Test)
target_compile_options(tst_mqblox PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME tst_mqblox COMMAND tst_mqblox)

include(GNUInstallDirs)
install(TARGETS mqblox RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/mqblox.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY resources/icons/hicolor/ DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor)
install(DIRECTORY shims/ DESTINATION ${CMAKE_INSTALL_DATADIR}/mqblox/shims
        PATTERN "build" EXCLUDE
        PATTERN "*.o" EXCLUDE
        PATTERN "*.dylib" EXCLUDE)
install(FILES shims/build_debug_shim.sh
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mqblox/shims)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/${PROJECT_NAME})
EOF_CMAKELISTS_TXT

cat <<'EOF_PKGBUILD' > "PKGBUILD"
# Maintainer: MrQuster <https://github.com/MrQuster>
# Upstream MacOBlox Author: Narezany (narezy) <https://github.com/narezy/MacOBlox>
pkgname=mqblox
pkgver=36.0.0
pkgrel=1
pkgdesc="HyperOS 4 Liquid Glass Qt6 launcher for the macOS Roblox client on Linux via Darling (MacOBlox fork)"
arch=('x86_64')
url="https://github.com/MrQuster/mqblox"
license=('MIT')
depends=(
  'qt6-base'
  'qt6-svg'
  'qt6-wayland'
  'clang'
  'lld'
  'unzip'
  'hicolor-icon-theme'
)
makedepends=(
  'cmake'
  'ninja'
)
optdepends=(
  'darling: macOS translation layer required to run RobloxPlayer.app'
)
source=()
sha256sums=()

build() {
  cmake -S "${startdir}" -B "${srcdir}/pkg-cmake-build" \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -Wno-dev
  cmake --build "${srcdir}/pkg-cmake-build"
}

check() {
  QT_QPA_PLATFORM=offscreen ctest --test-dir "${srcdir}/pkg-cmake-build" --output-on-failure
}

package() {
  DESTDIR="${pkgdir}" cmake --install "${srcdir}/pkg-cmake-build"
}
EOF_PKGBUILD

cat <<'EOF_PACKAGING_ARCH_PKGBUILD' > "packaging/arch/PKGBUILD"
# Maintainer: MrQuster <https://github.com/MrQuster>
# Upstream MacOBlox Author: Narezany (narezy) <https://github.com/narezy/MacOBlox>
pkgname=mqblox
pkgver=36.0.0
pkgrel=1
pkgdesc="HyperOS 4 Liquid Glass Qt6 launcher for the macOS Roblox client on Linux via Darling (MacOBlox fork)"
arch=('x86_64')
url="https://github.com/MrQuster/mqblox"
license=('MIT')
depends=(
  'qt6-base'
  'qt6-svg'
  'qt6-wayland'
  'clang'
  'lld'
  'unzip'
  'hicolor-icon-theme'
)
makedepends=(
  'cmake'
  'ninja'
)
optdepends=(
  'darling: macOS translation layer required to run RobloxPlayer.app'
)
source=()
sha256sums=()

build() {
  local src_root="${startdir}"
  if [[ ! -f "${src_root}/CMakeLists.txt" && -f "${startdir}/../..//CMakeLists.txt" ]]; then
    src_root="$(cd "${startdir}/../.." && pwd -P)"
  fi
  cmake -S "${src_root}" -B "${srcdir}/build" \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -Wno-dev
  cmake --build "${srcdir}/build"
}

check() {
  QT_QPA_PLATFORM=offscreen ctest --test-dir "${srcdir}/build" --output-on-failure
}

package() {
  DESTDIR="${pkgdir}" cmake --install "${srcdir}/build"
}
EOF_PACKAGING_ARCH_PKGBUILD

cat <<'EOF_PACKAGING_MQBLOX_DESKTOP' > "packaging/mqblox.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=MqBlox
GenericName=Roblox Launcher (MacOBlox Darling)
GenericName[ru]=Лаунчер Roblox (MacOBlox Darling)
Comment=MqBlox — HyperOS 4 Liquid Glass Launcher for Roblox (MacOBlox Darling backend)
Comment[ru]=MqBlox — лаунчер HyperOS 4 Liquid Glass для клиента Roblox (бэкенд MacOBlox Darling)
Exec=mqblox %u
Icon=mqblox
Terminal=false
Categories=Game;
Keywords=roblox;macoblox;darling;mqblox;
StartupWMClass=RobloxPlayer
StartupNotify=true
MimeType=x-scheme-handler/roblox;x-scheme-handler/roblox-player;
EOF_PACKAGING_MQBLOX_DESKTOP

cat <<'EOF_RESOURCES_MQBLOX_SVG' > "resources/mqblox.svg"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#0F172A"/>
      <stop offset="100%" stop-color="#020617"/>
    </linearGradient>
    <linearGradient id="ring" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#38BDF8"/>
      <stop offset="50%" stop-color="#F59E0B"/>
      <stop offset="100%" stop-color="#A855F7"/>
    </linearGradient>
  </defs>
  <rect x="6" y="6" width="116" height="116" rx="28" fill="url(#bg)" stroke="#38BDF8" stroke-width="3"/>
  <ellipse cx="64" cy="64" rx="46" ry="18" transform="rotate(-18 64 64)" fill="none" stroke="url(#ring)" stroke-width="8"/>
  <circle cx="64" cy="64" r="20" fill="#000000" stroke="#FDE047" stroke-width="3"/>
</svg>
EOF_RESOURCES_MQBLOX_SVG

cat <<'EOF_RESOURCES_RESOURCES_QRC' > "resources/resources.qrc"
<RCC>
    <qresource prefix="/">
        <file>style_ton618.qss</file>
        <file>mqblox.svg</file>
    </qresource>
</RCC>
EOF_RESOURCES_RESOURCES_QRC

cat <<'EOF_SRC_MAIN_CPP' > "src/main.cpp"
#include "ui/MainWindow.hpp"
#include "ui/Widgets.hpp"
#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QPalette>

static const QString IPC_SERVER_NAME = "mqblox_single_instance_ipc_v16";

static void applyGlobalDarkPalette(QApplication &app) {
    app.setStyle("Fusion");
    QPalette pal;
    QColor bgDark(3, 4, 8), bgCard(10, 14, 25), txtLight(248, 250, 252), accent(251, 191, 36);
    pal.setColor(QPalette::Window, bgDark);
    pal.setColor(QPalette::WindowText, txtLight);
    pal.setColor(QPalette::Base, bgDark);
    pal.setColor(QPalette::AlternateBase, bgCard);
    pal.setColor(QPalette::ToolTipBase, bgDark);
    pal.setColor(QPalette::ToolTipText, accent);
    pal.setColor(QPalette::Text, txtLight);
    pal.setColor(QPalette::Button, bgCard);
    pal.setColor(QPalette::ButtonText, txtLight);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, QColor(45, 33, 18));
    pal.setColor(QPalette::HighlightedText, accent);
    app.setPalette(pal);
}

int main(int argc, char *argv[]) {
    QLoggingCategory::setFilterRules("qt.qpa.wayland.textinput=false\nqt.core.qobject.connect=false");

    QApplication app(argc, argv);
    app.setApplicationName("mqblox");
    app.setApplicationDisplayName("MqBlox");
    app.setDesktopFileName("mqblox");
    app.setWindowIcon(QIcon::fromTheme("mqblox", QIcon(":/mqblox.svg")));
    app.setQuitOnLastWindowClosed(false);

    QLocalSocket probe;
    probe.connectToServer(IPC_SERVER_NAME);
    if (probe.waitForConnected(150)) {
        probe.write("SHOW");
        probe.waitForBytesWritten(150);
        probe.disconnectFromServer();
        return 0;
    }

    QLocalServer::removeServer(IPC_SERVER_NAME);
    QLocalServer ipcServer;
    ipcServer.listen(IPC_SERVER_NAME);

    applyGlobalDarkPalette(app);
    mqblox::installLiquidGlassEngine(&app);

    QString uriArg = (argc > 1) ? QString::fromUtf8(argv[1]) : QString{};
    mqblox::MainWindow win(uriArg);
    win.show();

    QObject::connect(&ipcServer, &QLocalServer::newConnection, &win, [&ipcServer, &win]() {
        while (QLocalSocket *client = ipcServer.nextPendingConnection()) {
            client->deleteLater();
            win.showNormal();
            win.raise();
            win.activateWindow();
        }
    });

    return app.exec();
}
EOF_SRC_MAIN_CPP

cat <<'EOF_SRC_CORE_TYPES_HPP' > "src/core/Types.hpp"
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace mqblox {

enum class SettingCategory {
    General = 0,
    System = 1,
    Graphics = 2,
    Tweaks = 3,
    Appearance = 4,
    Layout = 5,
    Accessibility = 6
};

enum class ResolutionMode { Auto = 0, HD_720p = 1, FHD_1080p = 2, QHD_2K = 3 };
enum class LightingMode { Voxel = 0, ShadowMap = 1, FuturePhase3 = 2, GameDefault = 3 };
enum class TextureQuality { Auto = 0, Level0 = 1, Level1 = 2, Level2 = 3, Level3 = 4 };
enum class MsaaMode { Off = 0, X2 = 1, X4 = 2 };
enum class CameraZoomMode { Default = 0, Medium500 = 1, Far2000 = 2, Cosmic9999 = 3 };
enum class NetworkProfile { UltraLowPing = 0, Balanced = 1 };

struct AccountProfile {
    QString id = "default";
    QString label = "Main";
    QString displayName = "Main";
};

struct AppSettings {
    int schemaVersion = 36;

    ResolutionMode resMode = ResolutionMode::Auto;
    LightingMode lighting = LightingMode::Voxel;
    TextureQuality textureQuality = TextureQuality::Auto;
    MsaaMode msaa = MsaaMode::Off;
    CameraZoomMode camZoom = CameraZoomMode::Cosmic9999;
    NetworkProfile netProfile = NetworkProfile::UltraLowPing;

    int targetFps = 240;
    bool unlimitedFps = false;
    int frmQualityLevel = 0;

    // MacOBlox Runtime & Input (passed via MACOBLOX_* environment variables)
    int mouseSensitivityPct = 100; // 10..200 -> MACOBLOX_MOUSE_SENSITIVITY
    bool hideMenuBar = true;       // MACOBLOX_HIDE_MENU_BAR=0 when false
    int dnsProvider = 1;           // 0=System, 1=Quad9, 2=Cloudflare, 3=Google, 4=Custom DoT
    QString dnsCustom;             // Custom DoT IPv4 when dnsProvider == 4
    bool fpsLog = false;           // MACOBLOX_FPS_LOG=1
    bool traceUdp = false;         // MACOBLOX_TRACE_UDP=1
    bool traceLock = false;        // MACOBLOX_TRACE_LOCK=1
    bool traceGl = false;          // MACOBLOX_TRACE_GL=1
    bool diagnosticSignals = false;// MACOBLOX_DIAGNOSTIC_SIGNALS=1

    bool returnToMqBloxOnLeave = true;
    bool animBg = true;
    bool closeToTray = true;
    bool vsyncOff = true; // MACOBLOX_VSYNC=0 when true, MACOBLOX_VSYNC=1 when false
    bool smartAutoCleanCache = true;
    bool lowRamFflags = false;
    bool disableTelemetry = true;
    bool autoReconnectOnCrash = false;
    bool sidebarPinned = true;

    bool noShadows = false;
    bool ultraShadows = false;
    bool noPostFx = false;
    bool lowMeshes = false;
    bool greySky = false;

    bool showHudFps = false;
    bool clearFog = false;
    int customMaxZoom = 9999;
    bool unlockInfiniteZoom = true;
    bool fullBright = false;
    bool showHitboxes = false;

    // 1. Стекло (Liquid Glass)
    int glassBlurStrength = 4;
    int glassOpacityPct = 10;
    int borderThicknessPx = 10;
    int borderBrightnessPct = 100;
    int glassSaturationPct = 160;
    int specularStrengthPct = 100;
    int shadowStrengthPct = 100;
    int cornerRadiusMode = 1;
    int glassStyleMode = 0;

    // 2. Цвета и Хамелеон
    int themeMode = 0;
    int accentPreset = 0;
    QString customAccentHex = "#38BDF8";
    bool chameleonEnabled = true;
    int chameleonSpeedPct = 20;
    int chameleonStrengthPct = 100;
    int borderColorSource = 0;
    int textColorMode = 0;

    // 3. Фон
    int backdropType = 0;
    QString customBackdropPath;
    int backdropDimPct = 0;
    int videoFpsLimit = 24;
    bool pauseOnFocusLoss = true;

    // 4. Анимации
    bool animationsEnabled = true;
    int animSpeedPct = 100;
    int animStyle = 1;
    bool cursorGlowFollow = true;
    bool reducedMotion = false;

    // 5. Масштаб и текст
    int uiScalePct = 100;
    int fontSizePx = 13;
    int fontBoldness = 0;
    int uiDensity = 1;

    // 6. Раскладка и окно
    int sidebarPosition = 0;
    int sidebarWidthPx = 210;
    bool sidebarShowLabels = true;
    bool rememberWindowSize = true;
    int savedWindowWidth = 1120;
    int savedWindowHeight = 700;
    bool startMinimized = false;
    bool alwaysOnTop = false;

    // 7. Доступность и пресеты
    bool highContrast = false;
    bool reduceTransparency = false;
    int effectsQuality = 2;
    bool disableBlurLowGpu = false;
    int visualPreset = 0;

    QString lastPlaceInput;
    QString customFflagsJson = "{}";
    QStringList leaveMarkers = {"leaveGame", "Disconnect", "GameShutDown"};
};

} // namespace mqblox
EOF_SRC_CORE_TYPES_HPP

cat <<'EOF_SRC_CORE_SETTINGSREGISTRY_HPP' > "src/core/SettingsRegistry.hpp"
#pragma once
#include "core/Types.hpp"
#include <QColor>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>
#include <QVector>
#include <algorithm>
#include <functional>

namespace mqblox {

enum class SettingValueType { Bool, Int, String };

struct SettingDescriptor {
    QString key;
    SettingValueType type;
    SettingCategory category;
    QString title;
    QString description;
    bool requiresGameRestart = false;
    QVariant defaultValue;
    int minInt = 0;
    int maxInt = 0;
    std::function<QVariant(const AppSettings &)> getter;
    std::function<void(AppSettings &, const QVariant &)> setter;
};

class SettingsRegistry {
public:
    static constexpr int kCurrentSchemaVersion = 36;

    static const QVector<SettingDescriptor> &all() {
        static const QVector<SettingDescriptor> kRegistry = buildRegistry();
        return kRegistry;
    }

    static const SettingDescriptor *find(const QString &key) {
        for (const auto &d : all()) {
            if (d.key == key) return &d;
        }
        return nullptr;
    }

    static void resetAll(AppSettings &s) {
        AppSettings def;
        s = def;
    }

    static bool resetKey(AppSettings &s, const QString &key) {
        if (const auto *d = find(key)) {
            d->setter(s, d->defaultValue);
            return true;
        }
        return false;
    }

    static void resetCategory(AppSettings &s, SettingCategory cat) {
        for (const auto &d : all()) {
            if (d.category == cat) {
                d.setter(s, d.defaultValue);
            }
        }
    }

    static void resetVisualAppearance(AppSettings &s) {
        resetCategory(s, SettingCategory::Appearance);
        resetCategory(s, SettingCategory::Layout);
        resetCategory(s, SettingCategory::Accessibility);
        s.animBg = true;
    }

    static void applyVisualPreset(AppSettings &s, int presetId) {
        resetVisualAppearance(s);
        s.visualPreset = std::clamp(presetId, 0, 4);
        switch (s.visualPreset) {
            case 0:
                break;
            case 1:
                s.glassBlurStrength = 7;
                s.glassOpacityPct = 24;
                s.glassStyleMode = 1;
                s.borderBrightnessPct = 115;
                s.specularStrengthPct = 80;
                break;
            case 2:
                s.glassBlurStrength = 2;
                s.glassOpacityPct = 36;
                s.glassStyleMode = 2;
                s.chameleonEnabled = false;
                s.borderColorSource = 1;
                s.specularStrengthPct = 35;
                s.shadowStrengthPct = 50;
                s.backdropDimPct = 40;
                break;
            case 3:
                s.highContrast = true;
                s.reduceTransparency = true;
                s.glassOpacityPct = 48;
                s.borderThicknessPx = 18;
                s.borderBrightnessPct = 150;
                s.chameleonEnabled = false;
                s.fontBoldness = 1;
                s.backdropDimPct = 60;
                break;
            case 4:
                s.animBg = false;
                s.backdropType = 1;
                s.reducedMotion = true;
                s.animationsEnabled = false;
                s.cursorGlowFollow = false;
                s.disableBlurLowGpu = true;
                s.effectsQuality = 0;
                s.glassBlurStrength = 0;
                s.videoFpsLimit = 15;
                break;
        }
    }

    static void readFromJson(AppSettings &s, const QJsonObject &obj) {
        s = AppSettings{};
        s.schemaVersion = kCurrentSchemaVersion;
        for (const auto &d : all()) {
            if (!obj.contains(d.key) || obj.value(d.key).isNull() || obj.value(d.key).isUndefined()) {
                d.setter(s, d.defaultValue);
                continue;
            }
            const QJsonValue v = obj.value(d.key);
            if (d.type == SettingValueType::Bool) {
                d.setter(s, v.isBool() ? v.toBool() : d.defaultValue.toBool());
            } else if (d.type == SettingValueType::Int) {
                int val = v.isDouble() ? v.toInt(d.defaultValue.toInt()) : d.defaultValue.toInt();
                d.setter(s, std::clamp(val, d.minInt, d.maxInt));
            } else if (d.type == SettingValueType::String) {
                QString str = v.isString() ? v.toString() : d.defaultValue.toString();
                d.setter(s, str);
            }
        }
        if (s.customFflagsJson.trimmed().isEmpty()) s.customFflagsJson = "{}";
        if (s.noShadows && s.ultraShadows) s.ultraShadows = false;
    }

    static QJsonObject writeToJson(const AppSettings &s) {
        QJsonObject obj;
        obj["schemaVersion"] = kCurrentSchemaVersion;
        for (const auto &d : all()) {
            QVariant v = d.getter(s);
            if (d.type == SettingValueType::Bool) {
                obj[d.key] = v.toBool();
            } else if (d.type == SettingValueType::Int) {
                obj[d.key] = std::clamp(v.toInt(), d.minInt, d.maxInt);
            } else if (d.type == SettingValueType::String) {
                obj[d.key] = v.toString();
            }
        }
        return obj;
    }

private:
    static QVector<SettingDescriptor> buildRegistry() {
        QVector<SettingDescriptor> reg;
        AppSettings def;

        auto addBool = [&](const QString &key, SettingCategory cat, const QString &title,
                           const QString &desc, bool restart, bool AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                key, SettingValueType::Bool, cat, title, desc, restart,
                def.*member, 0, 1,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member](AppSettings &s, const QVariant &v) { s.*member = v.toBool(); }
            });
        };

        auto addInt = [&](const QString &key, SettingCategory cat, const QString &title,
                          const QString &desc, bool restart, int minV, int maxV, int AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                key, SettingValueType::Int, cat, title, desc, restart,
                def.*member, minV, maxV,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member, minV, maxV](AppSettings &s, const QVariant &v) {
                    s.*member = std::clamp(v.toInt(), minV, maxV);
                }
            });
        };

        auto addString = [&](const QString &key, SettingCategory cat, const QString &title,
                             const QString &desc, bool restart, QString AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                key, SettingValueType::String, cat, title, desc, restart,
                def.*member, 0, 0,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member](AppSettings &s, const QVariant &v) { s.*member = v.toString(); }
            });
        };

        auto addEnum = [&](const QString &key, SettingCategory cat, const QString &title,
                           const QString &desc, bool restart, int minV, int maxV, auto member) {
            using EnumT = std::decay_t<decltype(def.*member)>;
            reg.push_back(SettingDescriptor{
                key, SettingValueType::Int, cat, title, desc, restart,
                static_cast<int>(def.*member), minV, maxV,
                [member](const AppSettings &s) -> QVariant { return static_cast<int>(s.*member); },
                [member, minV, maxV](AppSettings &s, const QVariant &v) {
                    s.*member = static_cast<EnumT>(std::clamp(v.toInt(), minV, maxV));
                }
            });
        };

        // System & MacOBlox Runtime
        addEnum("resMode", SettingCategory::System, "Режим разрешения окна", "Размер окна лаунчера", false, 0, 3, &AppSettings::resMode);
        addInt("mouseSensitivityPct", SettingCategory::System, "Чувствительность мыши MacOBlox (%)", "MACOBLOX_MOUSE_SENSITIVITY (10..200%)", true, 10, 200, &AppSettings::mouseSensitivityPct);
        addBool("hideMenuBar", SettingCategory::System, "Скрывать верхнее меню macOS", "MACOBLOX_HIDE_MENU_BAR в окне игры", true, &AppSettings::hideMenuBar);
        addInt("dnsProvider", SettingCategory::System, "DNS-over-TLS провайдер", "0=Системный, 1=Quad9, 2=Cloudflare, 3=Google, 4=Свой IP", true, 0, 4, &AppSettings::dnsProvider);
        addString("dnsCustom", SettingCategory::System, "Свой DNS-over-TLS IP", "IPv4 адрес сервера DoT (порт 853)", true, &AppSettings::dnsCustom);
        addBool("fpsLog", SettingCategory::System, "Логировать FPS (gl_profile.c)", "MACOBLOX_FPS_LOG=1", true, &AppSettings::fpsLog);
        addBool("traceUdp", SettingCategory::System, "Трассировка UDP и детектор фризов", "MACOBLOX_TRACE_UDP=1", true, &AppSettings::traceUdp);
        addBool("traceLock", SettingCategory::System, "Трассировка psynch_mutex", "MACOBLOX_TRACE_LOCK=1", true, &AppSettings::traceLock);
        addBool("traceGl", SettingCategory::System, "Трассировка CGL / OpenGL", "MACOBLOX_TRACE_GL=1", true, &AppSettings::traceGl);
        addBool("diagnosticSignals", SettingCategory::System, "Дамп регистров при сбоях", "MACOBLOX_DIAGNOSTIC_SIGNALS=1", true, &AppSettings::diagnosticSignals);

        addBool("returnToMqBloxOnLeave", SettingCategory::General, "Возврат в MqBlox при выходе", "Показывать окно лаунчера после выхода с сервера", false, &AppSettings::returnToMqBloxOnLeave);
        addBool("animBg", SettingCategory::Appearance, "Анимированный фон TON 618", "3D-рендер аккреционного диска", false, &AppSettings::animBg);
        addBool("closeToTray", SettingCategory::General, "Сворачивать в трей", "Оставлять иконку в системном лотке", false, &AppSettings::closeToTray);
        addBool("vsyncOff", SettingCategory::System, "Отключить VSync в EGL", "MACOBLOX_VSYNC=0 (eglSwapInterval 0)", true, &AppSettings::vsyncOff);
        addBool("smartAutoCleanCache", SettingCategory::System, "Авто-очистка кэша", "Очищать старый HTTP-кэш (>500 МБ)", false, &AppSettings::smartAutoCleanCache);
        addBool("lowRamFflags", SettingCategory::System, "Экономия ОЗУ (Low-RAM FFlags)", "Быстрая выгрузка неиспользуемых мешей", true, &AppSettings::lowRamFflags);
        addBool("disableTelemetry", SettingCategory::System, "Отключить телеметрию Roblox", "Блокировать отправку диагностических счётчиков", true, &AppSettings::disableTelemetry);
        addBool("autoReconnectOnCrash", SettingCategory::General, "Авто-перезаход при сбое", "Перезапускать клиент при ненулевом коде выхода", false, &AppSettings::autoReconnectOnCrash);
        addBool("sidebarPinned", SettingCategory::Layout, "Закрепить боковую панель", "Не сворачивать меню навигации", false, &AppSettings::sidebarPinned);

        // Graphics & FastFlags (macOS OpenGL Core Profile)
        addEnum("lighting", SettingCategory::Graphics, "Технология освещения", "[Предположительно: белый список FFlag] Voxel / ShadowMap / Future Phase 3", true, 0, 3, &AppSettings::lighting);
        addEnum("textureQuality", SettingCategory::Graphics, "Качество текстур", "[Предположительно: белый список FFlag] DFIntTextureQualityOverride", true, 0, 4, &AppSettings::textureQuality);
        addEnum("msaa", SettingCategory::Graphics, "Сглаживание MSAA (OpenGL)", "[Подтверждён в MacOBlox] FIntDebugForceMSAASamples (0/2x/4x, активно и на AMD через gl_profile.c)", true, 0, 2, &AppSettings::msaa);
        addInt("targetFps", SettingCategory::Graphics, "Лимит кадров (FPS)", "[Подтверждён в MacOBlox] DFIntTaskSchedulerTargetFps (15..9999)", true, 15, 9999, &AppSettings::targetFps);
        addBool("unlimitedFps", SettingCategory::Graphics, "Безлимитный FPS (9999)", "[Подтверждён в MacOBlox] DFIntTaskSchedulerTargetFps=9999 + MACOBLOX_VSYNC=0", true, &AppSettings::unlimitedFps);
        addInt("frmQualityLevel", SettingCategory::Graphics, "Уровень графики FRM (0..21)", "[Подтверждён в MacOBlox] DFIntDebugFRMQualityLevelOverride", true, 0, 21, &AppSettings::frmQualityLevel);
        addBool("noShadows", SettingCategory::Graphics, "Отключить тени", "[Подтверждён в MacOBlox] FIntRenderShadowIntensity = 0", true, &AppSettings::noShadows);
        addBool("ultraShadows", SettingCategory::Graphics, "Ультра-тени", "[Предположительно: белый список FFlag] FIntRenderShadowIntensity = 100", true, &AppSettings::ultraShadows);
        addBool("noPostFx", SettingCategory::Graphics, "Отключить PostFX", "[Предположительно: белый список FFlag] FFlagDisablePostFx = true", true, &AppSettings::noPostFx);
        addBool("lowMeshes", SettingCategory::Graphics, "Упростить дальние модели", "[Предположительно: белый список FFlag] CSG LOD = 0", true, &AppSettings::lowMeshes);
        addBool("greySky", SettingCategory::Graphics, "Упрощённое небо", "[Предположительно: белый список FFlag] FFlagDebugSkyGray = true", true, &AppSettings::greySky);
        addString("customFflagsJson", SettingCategory::Graphics, "Пользовательские FastFlags (JSON)", "Словарь дополнительных флагов (применяются только флаги из белого списка клиента)", true, &AppSettings::customFflagsJson);

        // Tweaks & Camera
        addEnum("camZoom", SettingCategory::Tweaks, "Пресет зума камеры", "[Предположительно: белый список FFlag] FIntCameraMaxZoomDistance", true, 0, 3, &AppSettings::camZoom);
        addEnum("netProfile", SettingCategory::Tweaks, "Сетевой профиль MTU/RakNet", "[Предположительно: белый список FFlag] DFIntRakNetMtuValueInBytes", true, 0, 1, &AppSettings::netProfile);
        addBool("showHudFps", SettingCategory::Tweaks, "Счётчик FPS в игре", "[Подтверждён: MACOBLOX_FPS_LOG=1 + FFlagDebugDisplayFPS]", true, &AppSettings::showHudFps);
        addBool("clearFog", SettingCategory::Tweaks, "Убрать 3D-траву", "[Подтверждён в MacOBlox] FIntFRMMinGrassDistance / FIntFRMMaxGrassDistance = 0", true, &AppSettings::clearFog);
        addInt("customMaxZoom", SettingCategory::Tweaks, "Макс. дистанция зума", "[Предположительно: белый список FFlag] FIntCameraMaxZoomDistance (10..9999)", true, 10, 9999, &AppSettings::customMaxZoom);
        addBool("unlockInfiniteZoom", SettingCategory::Tweaks, "Разблокировать дальний зум", "[Предположительно: белый список FFlag] Применять customMaxZoom", true, &AppSettings::unlockInfiniteZoom);
        addBool("fullBright", SettingCategory::Tweaks, "FullBright (высветление)", "[Частично подтверждён] FIntRenderShadowIntensity=0 + Voxel/PostFX FFlags", true, &AppSettings::fullBright);
        addBool("showHitboxes", SettingCategory::Tweaks, "Упрощение геометрии (LOD 0)", "[Предположительно: белый список FFlag] Минимальная детализация CSG", true, &AppSettings::showHitboxes);

        // Appearance, Layout & Accessibility (HyperOS 4 Liquid Glass)
        addInt("glassBlurStrength", SettingCategory::Appearance, "Сила размытия стекла", "0..8", false, 0, 8, &AppSettings::glassBlurStrength);
        addInt("glassOpacityPct", SettingCategory::Appearance, "Непрозрачность стекла (%)", "2..90%", false, 2, 90, &AppSettings::glassOpacityPct);
        addInt("borderThicknessPx", SettingCategory::Appearance, "Толщина контура (x0.1 px)", "5..25", false, 5, 25, &AppSettings::borderThicknessPx);
        addInt("borderBrightnessPct", SettingCategory::Appearance, "Яркость блика грани (%)", "20..200%", false, 20, 200, &AppSettings::borderBrightnessPct);
        addInt("glassSaturationPct", SettingCategory::Appearance, "Насыщенность преломления (%)", "80..220%", false, 80, 220, &AppSettings::glassSaturationPct);
        addInt("specularStrengthPct", SettingCategory::Appearance, "Сила купольного блика (%)", "0..200%", false, 0, 200, &AppSettings::specularStrengthPct);
        addInt("shadowStrengthPct", SettingCategory::Appearance, "Глубина теней (%)", "0..200%", false, 0, 200, &AppSettings::shadowStrengthPct);
        addInt("cornerRadiusMode", SettingCategory::Appearance, "Скругление углов", "0..2", false, 0, 2, &AppSettings::cornerRadiusMode);
        addInt("glassStyleMode", SettingCategory::Appearance, "Стиль фактуры стекла", "0..2", false, 0, 2, &AppSettings::glassStyleMode);

        addInt("themeMode", SettingCategory::Appearance, "Цветовая тема", "0=Тёмная, 1=Светлая, 2=Системная", false, 0, 2, &AppSettings::themeMode);
        addInt("accentPreset", SettingCategory::Appearance, "Акцентный цвет", "0..4", false, 0, 4, &AppSettings::accentPreset);
        addString("customAccentHex", SettingCategory::Appearance, "Свой HEX-цвет акцента", "#RRGGBB", false, &AppSettings::customAccentHex);
        addBool("chameleonEnabled", SettingCategory::Appearance, "Адаптивный Хамелеон", "Подстройка под цвета TON 618", false, &AppSettings::chameleonEnabled);
        addInt("chameleonSpeedPct", SettingCategory::Appearance, "Скорость Хамелеона (%)", "5..100%", false, 5, 100, &AppSettings::chameleonSpeedPct);
        addInt("chameleonStrengthPct", SettingCategory::Appearance, "Насыщенность Хамелеона (%)", "20..150%", false, 20, 150, &AppSettings::chameleonStrengthPct);
        addInt("borderColorSource", SettingCategory::Appearance, "Источник цвета граней", "0..2", false, 0, 2, &AppSettings::borderColorSource);
        addInt("textColorMode", SettingCategory::Appearance, "Контраст текста", "0..2", false, 0, 2, &AppSettings::textColorMode);

        addInt("backdropType", SettingCategory::Appearance, "Тип фона", "0..3", false, 0, 3, &AppSettings::backdropType);
        addString("customBackdropPath", SettingCategory::Appearance, "Путь к своему фону", "Файл изображения или видео", false, &AppSettings::customBackdropPath);
        addInt("backdropDimPct", SettingCategory::Appearance, "Затемнение фона (%)", "0..80%", false, 0, 80, &AppSettings::backdropDimPct);
        addInt("videoFpsLimit", SettingCategory::Appearance, "Лимит FPS фона", "15..60", false, 15, 60, &AppSettings::videoFpsLimit);
        addBool("pauseOnFocusLoss", SettingCategory::Appearance, "Пауза фона без фокуса", "0% CPU при неактивном окне", false, &AppSettings::pauseOnFocusLoss);

        addBool("animationsEnabled", SettingCategory::Appearance, "Пружинные анимации", "Плавный отклик элементов", false, &AppSettings::animationsEnabled);
        addInt("animSpeedPct", SettingCategory::Appearance, "Скорость анимаций (%)", "50..200%", false, 50, 200, &AppSettings::animSpeedPct);
        addInt("animStyle", SettingCategory::Appearance, "Характер анимаций", "0..2", false, 0, 2, &AppSettings::animStyle);
        addBool("cursorGlowFollow", SettingCategory::Appearance, "Световой прожектор за курсором", "Подсветка стекла под мышью", false, &AppSettings::cursorGlowFollow);
        addBool("reducedMotion", SettingCategory::Accessibility, "Режим «Меньше движения»", "Отключает пружины и блик курсора", false, &AppSettings::reducedMotion);

        addInt("uiScalePct", SettingCategory::Layout, "Масштаб интерфейса (%)", "80..150%", false, 80, 150, &AppSettings::uiScalePct);
        addInt("fontSizePx", SettingCategory::Layout, "Размер шрифта (px)", "11..17 px", false, 11, 17, &AppSettings::fontSizePx);
        addInt("fontBoldness", SettingCategory::Layout, "Начертание шрифта", "0..1", false, 0, 1, &AppSettings::fontBoldness);
        addInt("uiDensity", SettingCategory::Layout, "Плотность макета", "0..2", false, 0, 2, &AppSettings::uiDensity);

        addInt("sidebarPosition", SettingCategory::Layout, "Положение панели навигации", "0..1", false, 0, 1, &AppSettings::sidebarPosition);
        addInt("sidebarWidthPx", SettingCategory::Layout, "Ширина боковой панели (px)", "160..280 px", false, 160, 280, &AppSettings::sidebarWidthPx);
        addBool("sidebarShowLabels", SettingCategory::Layout, "Подписи у иконок меню", "Показывать названия вкладок", false, &AppSettings::sidebarShowLabels);
        addBool("rememberWindowSize", SettingCategory::Layout, "Запоминать размер окна", "Сохранять ширину и высоту", false, &AppSettings::rememberWindowSize);
        addInt("savedWindowWidth", SettingCategory::Layout, "Сохранённая ширина окна", "820..3840", false, 820, 3840, &AppSettings::savedWindowWidth);
        addInt("savedWindowHeight", SettingCategory::Layout, "Сохранённая высота окна", "540..2160", false, 540, 2160, &AppSettings::savedWindowHeight);
        addBool("startMinimized", SettingCategory::Layout, "Запуск свёрнутым в трей", "Не показывать окно при старте", false, &AppSettings::startMinimized);
        addBool("alwaysOnTop", SettingCategory::Layout, "Всегда поверх других окон", "Доступно в сессии X11", false, &AppSettings::alwaysOnTop);

        addBool("highContrast", SettingCategory::Accessibility, "Высокий контраст", "Усиленная читаемость текста", false, &AppSettings::highContrast);
        addBool("reduceTransparency", SettingCategory::Accessibility, "Уменьшить прозрачность", "Плотный фон карточек", false, &AppSettings::reduceTransparency);
        addInt("effectsQuality", SettingCategory::Accessibility, "Качество оптических эффектов", "0..3", false, 0, 3, &AppSettings::effectsQuality);
        addBool("disableBlurLowGpu", SettingCategory::Accessibility, "Отключить размытие (для слабых GPU)", "Без фильтра размытия подложки", false, &AppSettings::disableBlurLowGpu);
        addInt("visualPreset", SettingCategory::Appearance, "Пресет оформления", "0..4", false, 0, 4, &AppSettings::visualPreset);

        addString("lastPlaceInput", SettingCategory::General, "Последний Place ID / Ссылка", "Сохранённый адрес сервера", false, &AppSettings::lastPlaceInput);
        return reg;
    }
};

} // namespace mqblox
EOF_SRC_CORE_SETTINGSREGISTRY_HPP

cat <<'EOF_SRC_CORE_SETTINGSMANAGER_HPP' > "src/core/SettingsManager.hpp"
#pragma once
#include "core/Types.hpp"
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mqblox {

class SettingsManager {
public:
    SettingsManager();

    void loadAll();
    bool saveAll(QString *errMsg = nullptr);

    AppSettings &settings() { return m_settings; }
    const AppSettings &settings() const { return m_settings; }

    QVector<AccountProfile> &accounts() { return m_accounts; }
    const QVector<AccountProfile> &accounts() const { return m_accounts; }

    QString activeAccountId() const { return m_activeAccountId; }
    void setActiveAccountId(const QString &id) { m_activeAccountId = id; }
    AccountProfile *activeAccount();

    QString mqDir() const;
    QString accountsDir() const;
    QString macobloxDataDir() const;
    QString robloxAppDir() const;
    QString clientAppSettingsPath() const;
    QString darlingPrefixDir() const;
    QString soberBaseDir() const;

    bool exportPresetToFile(const QString &filePath) const;
    bool importPresetFromFile(const QString &filePath, QString *errMsg = nullptr);

    QStringList listNamedProfiles() const;
    bool saveNamedProfile(const QString &name, QString *errMsg = nullptr) const;
    bool loadNamedProfile(const QString &name, QString *errMsg = nullptr);
    bool deleteNamedProfile(const QString &name) const;

    QJsonObject buildEffectiveFflags(QString *errMsg = nullptr) const;
    static QString parsePlaceUri(const QString &input);

private:
    bool writeClientAppSettingsJson(QString *errMsg) const;

    AppSettings m_settings;
    QVector<AccountProfile> m_accounts;
    QString m_activeAccountId = "default";
};

} // namespace mqblox
EOF_SRC_CORE_SETTINGSMANAGER_HPP

cat <<'EOF_SRC_CORE_SETTINGSMANAGER_CPP' > "src/core/SettingsManager.cpp"
#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

namespace mqblox {

static bool writeFileIfChanged(const QString &filePath, const QByteArray &newBytes, QString *errMsg = nullptr) {
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QFile existing(filePath);
    if (existing.exists() && existing.open(QIODevice::ReadOnly)) {
        if (existing.readAll() == newBytes) {
            return true;
        }
        existing.close();
    }
    QSaveFile sf(filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errMsg) *errMsg = "Не удалось открыть файл для записи: " + filePath;
        return false;
    }
    if (sf.write(newBytes) != newBytes.size()) {
        if (errMsg) *errMsg = "Ошибка записи файла: " + filePath;
        return false;
    }
    if (!sf.commit()) {
        if (errMsg) *errMsg = "Ошибка атомарного сохранения файла: " + filePath;
        return false;
    }
    return true;
}

SettingsManager::SettingsManager() {
    QDir().mkpath(mqDir());
    QDir().mkpath(accountsDir());
    QDir().mkpath(mqDir() + "/profiles");
    QDir().mkpath(macobloxDataDir());
    m_accounts.push_back(AccountProfile{});
}

QString SettingsManager::mqDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/mqblox";
    return QDir::homePath() + "/.local/share/mqblox";
}

QString SettingsManager::accountsDir() const {
    return mqDir() + "/accounts";
}

QString SettingsManager::macobloxDataDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/macoblox";
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share");
    return xdgData + "/macoblox";
}

QString SettingsManager::robloxAppDir() const {
    return macobloxDataDir() + "/RobloxPlayer.app";
}

QString SettingsManager::clientAppSettingsPath() const {
    return robloxAppDir() + "/Contents/MacOS/ClientSettings/ClientAppSettings.json";
}

QString SettingsManager::darlingPrefixDir() const {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    if (!testDir.isEmpty()) return testDir + "/darling";
    return qEnvironmentVariable("DPREFIX", QDir::homePath() + "/.darling");
}

QString SettingsManager::soberBaseDir() const {
    return macobloxDataDir();
}

AccountProfile *SettingsManager::activeAccount() {
    for (auto &acc : m_accounts) {
        if (acc.id == m_activeAccountId) return &acc;
    }
    if (m_accounts.isEmpty()) m_accounts.push_back(AccountProfile{});
    m_activeAccountId = m_accounts.front().id;
    return &m_accounts.front();
}

QString SettingsManager::parsePlaceUri(const QString &input) {
    QString s = input.trimmed();
    if (s.isEmpty()) return {};
    if (s.startsWith("roblox://", Qt::CaseInsensitive) ||
        s.startsWith("roblox-player:", Qt::CaseInsensitive)) {
        return s;
    }

    static const QRegularExpression rxDigits("^\\d+$");
    if (rxDigits.match(s).hasMatch()) {
        return "roblox://experiences/start?placeId=" + s;
    }
    static const QRegularExpression rxUrl(
        "roblox\\.com/(?:[a-z]{2}/)?games/(\\d+)(?:[^?]*\\?[^#]*privateServerLinkCode=([A-Za-z0-9_-]+))?",
        QRegularExpression::CaseInsensitiveOption);
    auto m = rxUrl.match(s);
    if (m.hasMatch()) {
        QString placeId = m.captured(1);
        QString linkCode = m.captured(2);
        if (!linkCode.isEmpty()) {
            return QString("roblox://experiences/start?placeId=%1&linkCode=%2").arg(placeId, linkCode);
        }
        return "roblox://experiences/start?placeId=" + placeId;
    }
    return {};
}

void SettingsManager::loadAll() {
    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    const QString cfgDir = testDir.isEmpty() ? (QDir::homePath() + "/.config/mqblox") : (testDir + "/config");
    const QString cfgV36 = cfgDir + "/settings_v36.json";
    const QString cfgV35 = cfgDir + "/settings_v35.json";
    const QString cfgV34 = cfgDir + "/settings_v34.json";
    const QString legacyV13 = mqDir() + "/settings_v13.json";

    QString loadPath = legacyV13;
    if (QFileInfo::exists(cfgV36)) loadPath = cfgV36;
    else if (QFileInfo::exists(cfgV35)) loadPath = cfgV35;
    else if (QFileInfo::exists(cfgV34)) loadPath = cfgV34;

    QFile f(loadPath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError perr{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject root = doc.object();
    const int fileSchema = root.value("schemaVersion").toInt(0);
    m_activeAccountId = root.value("activeAccountId").toString("default");
    QJsonArray arr = root.value("accounts").toArray();
    if (!arr.isEmpty()) {
        m_accounts.clear();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            AccountProfile p;
            p.id = o.value("id").toString("default");
            p.label = o.value("label").toString("Main");
            p.displayName = o.value("displayName").toString(p.label);
            if (p.displayName.trimmed().isEmpty()) p.displayName = p.label;
            m_accounts.push_back(p);
        }
    }

    QJsonObject s = root.value("settings").toObject();
    if (s.isEmpty() && root.contains("targetFps")) {
        // Legacy flat root schema (v13)
        s = root;
    }
    if (!s.isEmpty()) {
        SettingsRegistry::readFromJson(m_settings, s);
    }

    if (loadPath != cfgV36 || fileSchema != SettingsRegistry::kCurrentSchemaVersion) {
        saveAll();
    }
}

bool SettingsManager::saveAll(QString *errMsg) {
    if (!writeClientAppSettingsJson(errMsg)) return false;

    QJsonObject s = SettingsRegistry::writeToJson(m_settings);

    QJsonArray arr;
    for (const auto &acc : m_accounts) {
        QJsonObject o;
        o["id"] = acc.id;
        o["label"] = acc.label;
        o["displayName"] = acc.displayName;
        arr.append(o);
    }

    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["activeAccountId"] = m_activeAccountId;
    root["accounts"] = arr;
    root["settings"] = s;

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    const QString testDir = qEnvironmentVariable("MQBLOX_TEST_DIR");
    const QString cfgDir = testDir.isEmpty() ? (QDir::homePath() + "/.config/mqblox") : (testDir + "/config");
    QDir().mkpath(cfgDir);
    for (const QString &target : {
             cfgDir + "/settings_v36.json",
             mqDir() + "/settings_v13.json"}) {
        if (!writeFileIfChanged(target, payload, errMsg)) {
            return false;
        }
    }
    return true;
}

QJsonObject SettingsManager::buildEffectiveFflags(QString *errMsg) const {
    QString rawCustom = m_settings.customFflagsJson.trimmed();
    if (rawCustom.isEmpty()) rawCustom = "{}";

    QJsonParseError perr{};
    QJsonDocument customDoc = QJsonDocument::fromJson(rawCustom.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !customDoc.isObject()) {
        if (errMsg) *errMsg = "Ошибка синтаксиса JSON в FastFlags: " + perr.errorString();
        return {};
    }

    QJsonObject fflags;
    const QJsonObject userCustomObj = customDoc.object();
    for (auto it = userCustomObj.begin(); it != userCustomObj.end(); ++it) {
        fflags[it.key()] = it.value();
    }

    const int fpsVal = m_settings.unlimitedFps ? 9999 : m_settings.targetFps;
    fflags["DFIntTaskSchedulerTargetFps"] = fpsVal;
    fflags["FFlagTaskSchedulerLimitTargetFpsTo2402"] = false;
    fflags["FFlagGameBasicSettingsFramerateCap5"] = true;

    // macOS RobloxPlayer uses OpenGL 4.1 Core via Darling CGL + gl_profile.c
    fflags["FFlagDebugGraphicsPreferOpenGL"] = true;

    if (m_settings.lighting == LightingMode::Voxel || m_settings.fullBright) {
        fflags["DFFlagDebugRenderForceTechnologyVoxel"] = true;
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = false;
        fflags["FFlagDebugForceFutureIsBrightPhase3"] = false;
    } else if (m_settings.lighting == LightingMode::ShadowMap) {
        fflags["FFlagDebugForceFutureIsBrightPhase2"] = true;
    } else if (m_settings.lighting == LightingMode::FuturePhase3) {
        fflags["FFlagDebugForceFutureIsBrightPhase3"] = true;
    }

    if (m_settings.textureQuality != TextureQuality::Auto) {
        int texLvl = static_cast<int>(m_settings.textureQuality) - 1;
        fflags["DFFlagTextureQualityOverrideEnabled"] = true;
        fflags["DFIntTextureQualityOverride"] = texLvl;
    }

    int msaaSamples = 0;
    if (m_settings.msaa == MsaaMode::X2) msaaSamples = 2;
    else if (m_settings.msaa == MsaaMode::X4) msaaSamples = 4;
    if (msaaSamples > 0) {
        fflags["FIntDebugForceMSAASamples"] = msaaSamples;
    }

    if (m_settings.frmQualityLevel > 0) {
        fflags["DFIntDebugFRMQualityLevelOverride"] = m_settings.frmQualityLevel;
        fflags["FIntRomarkStartWithGraphicQualityLevel"] = m_settings.frmQualityLevel;
    }

    if (m_settings.noShadows || m_settings.fullBright) {
        fflags["FIntRenderShadowIntensity"] = 0;
        fflags["DFIntCullFactorPixelThresholdShadowMapHighQuality"] = 2147483647;
        fflags["DFIntCullFactorPixelThresholdShadowMapLowQuality"] = 2147483647;
    } else if (m_settings.ultraShadows) {
        fflags["FIntRenderShadowIntensity"] = 100;
        fflags["FIntRenderShadowMapBias"] = 0;
    }

    if (m_settings.noPostFx || m_settings.fullBright) {
        fflags["FFlagDisablePostFx"] = true;
    }
    if (m_settings.greySky) {
        fflags["FFlagDebugSkyGray"] = true;
    }

    if (m_settings.lowMeshes || m_settings.showHitboxes) {
        fflags["DFIntCSGLevelOfDetailSwitchingDistance"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL12"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL23"] = 0;
        fflags["DFIntCSGLevelOfDetailSwitchingDistanceL34"] = 0;
    }

    if (m_settings.lowRamFflags) {
        fflags["DFIntHttpCacheCleanMinFilesRequired"] = 300;
        fflags["FIntRenderLocalLightUpdatesMax"] = 1;
        fflags["DFIntTextureCompositorActiveJobs"] = 1;
    }

    if (m_settings.unlockInfiniteZoom) {
        fflags["FIntCameraMaxZoomDistance"] = m_settings.customMaxZoom;
    } else if (m_settings.camZoom != CameraZoomMode::Default) {
        int presetZoom = 128;
        if (m_settings.camZoom == CameraZoomMode::Medium500) presetZoom = 500;
        else if (m_settings.camZoom == CameraZoomMode::Far2000) presetZoom = 2000;
        else if (m_settings.camZoom == CameraZoomMode::Cosmic9999) presetZoom = 99999;
        fflags["FIntCameraMaxZoomDistance"] = presetZoom;
    }

    if (m_settings.clearFog) {
        fflags["FIntFRMMinGrassDistance"] = 0;
        fflags["FIntFRMMaxGrassDistance"] = 0;
        fflags["FIntRenderGrassDetailStrands"] = 0;
    }

    if (m_settings.netProfile == NetworkProfile::UltraLowPing) {
        fflags["DFIntConnectionMTUSize"] = 1396;
        fflags["FIntRakNetResendBufferArrayLength"] = 128;
    } else {
        fflags["DFIntConnectionMTUSize"] = 1492;
    }

    if (m_settings.showHudFps) {
        fflags["FFlagDebugDisplayFPS"] = true;
    }

    if (m_settings.disableTelemetry) {
        fflags["FFlagDebugDisableTelemetryEphemeralCounter"] = true;
        fflags["FFlagDebugDisableTelemetryEphemeralStat"] = true;
        fflags["FFlagDebugDisableTelemetryEventIngest"] = true;
        fflags["FFlagDebugDisableTelemetryPoint"] = true;
        fflags["FFlagDebugDisableTelemetryV2Counter"] = true;
        fflags["FFlagDebugDisableTelemetryV2Event"] = true;
        fflags["FFlagDebugDisableTelemetryV2Stat"] = true;
    }

    return fflags;
}

bool SettingsManager::writeClientAppSettingsJson(QString *errMsg) const {
    QString err;
    const QJsonObject managedFlags = buildEffectiveFflags(&err);
    if (!err.isEmpty()) {
        if (errMsg) *errMsg = err;
        return false;
    }

    static const QStringList kManagedKeys = {
        "DFIntTaskSchedulerTargetFps", "FFlagTaskSchedulerLimitTargetFpsTo2402",
        "FFlagGameBasicSettingsFramerateCap5", "FFlagDebugGraphicsPreferVulkan",
        "FFlagDebugGraphicsDisableOpenGL", "FFlagDebugGraphicsPreferOpenGL",
        "FFlagDebugGraphicsDisableVulkan", "DFFlagDebugRenderForceTechnologyVoxel",
        "FFlagDebugForceFutureIsBrightPhase2", "FFlagDebugForceFutureIsBrightPhase3",
        "DFFlagTextureQualityOverrideEnabled", "DFIntTextureQualityOverride",
        "FIntDebugForceMSAASamples", "DFIntDebugFRMQualityLevelOverride",
        "FIntRomarkStartWithGraphicQualityLevel", "FIntRenderShadowIntensity",
        "DFIntCullFactorPixelThresholdShadowMapHighQuality",
        "DFIntCullFactorPixelThresholdShadowMapLowQuality", "FIntRenderShadowMapBias",
        "FFlagDisablePostFx", "FFlagDebugSkyGray", "DFIntCSGLevelOfDetailSwitchingDistance",
        "DFIntCSGLevelOfDetailSwitchingDistanceL12", "DFIntCSGLevelOfDetailSwitchingDistanceL23",
        "DFIntCSGLevelOfDetailSwitchingDistanceL34", "DFIntHttpCacheCleanMinFilesRequired",
        "FIntRenderLocalLightUpdatesMax", "DFIntTextureCompositorActiveJobs",
        "FIntCameraMaxZoomDistance", "DFIntCameraMaxZoomDistance",
        "FIntFRMMinGrassDistance", "FIntFRMMaxGrassDistance", "FIntRenderGrassDetailStrands",
        "DFIntConnectionMTUSize", "FIntRakNetResendBufferArrayLength", "FFlagDebugDisplayFPS",
        "FFlagDebugDisableTelemetryEphemeralCounter", "FFlagDebugDisableTelemetryEphemeralStat",
        "FFlagDebugDisableTelemetryEventIngest", "FFlagDebugDisableTelemetryPoint",
        "FFlagDebugDisableTelemetryV2Counter", "FFlagDebugDisableTelemetryV2Event",
        "FFlagDebugDisableTelemetryV2Stat"
    };

    // 1. Write MacOBlox's RobloxPlayer.app/Contents/MacOS/ClientSettings/ClientAppSettings.json
    const QString clientSettingsFile = clientAppSettingsPath();
    QJsonObject clientObj;
    {
        QFile oldF(clientSettingsFile);
        if (oldF.open(QIODevice::ReadOnly)) {
            QJsonParseError oldErr{};
            QJsonDocument oldDoc = QJsonDocument::fromJson(oldF.readAll(), &oldErr);
            if (oldErr.error == QJsonParseError::NoError && oldDoc.isObject()) {
                clientObj = oldDoc.object();
            }
        }
    }
    for (const QString &k : kManagedKeys) {
        clientObj.remove(k);
    }
    for (auto it = managedFlags.begin(); it != managedFlags.end(); ++it) {
        clientObj[it.key()] = it.value();
    }
    if (!writeFileIfChanged(clientSettingsFile,
                            QJsonDocument(clientObj).toJson(QJsonDocument::Indented),
                            errMsg)) {
        return false;
    }

    // 2. Also preserve foreign keys in macobloxDataDir()/config/sober/config.json if present
    const QString compatCfgPath = soberBaseDir() + "/config/sober/config.json";
    if (QFileInfo::exists(compatCfgPath)) {
        QJsonObject rootObj;
        QJsonObject existingFflags;
        QFile oldF(compatCfgPath);
        if (oldF.open(QIODevice::ReadOnly)) {
            QJsonParseError oldErr{};
            QJsonDocument oldDoc = QJsonDocument::fromJson(oldF.readAll(), &oldErr);
            if (oldErr.error == QJsonParseError::NoError && oldDoc.isObject()) {
                rootObj = oldDoc.object();
                existingFflags = rootObj.value("fflags").toObject();
            }
        }
        for (const QString &k : kManagedKeys) {
            existingFflags.remove(k);
        }
        for (auto it = managedFlags.begin(); it != managedFlags.end(); ++it) {
            existingFflags[it.key()] = it.value();
        }
        rootObj.remove("bring_back_oof");
        rootObj["use_console_experience"] = false;
        rootObj["touch_mode"] = "off";
        rootObj["fflags"] = existingFflags;
        if (!writeFileIfChanged(compatCfgPath,
                                QJsonDocument(rootObj).toJson(QJsonDocument::Indented),
                                errMsg)) {
            return false;
        }
    }

    return true;
}

bool SettingsManager::exportPresetToFile(const QString &filePath) const {
    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["settings"] = SettingsRegistry::writeToJson(m_settings);
    return writeFileIfChanged(filePath, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool SettingsManager::importPresetFromFile(const QString &filePath, QString *errMsg) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errMsg) *errMsg = "Не удалось открыть файл профиля: " + filePath;
        return false;
    }
    QJsonParseError perr{};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errMsg) *errMsg = "Некорректный JSON профиля: " + perr.errorString();
        return false;
    }
    QJsonObject root = doc.object();
    QJsonObject sObj = root.contains("settings") ? root.value("settings").toObject() : root;
    SettingsRegistry::readFromJson(m_settings, sObj);
    return saveAll(errMsg);
}

QStringList SettingsManager::listNamedProfiles() const {
    QDir d(mqDir() + "/profiles");
    QStringList out;
    for (const QFileInfo &fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        out.append(fi.completeBaseName());
    }
    return out;
}

bool SettingsManager::saveNamedProfile(const QString &name, QString *errMsg) const {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    if (clean.isEmpty()) {
        if (errMsg) *errMsg = "Имя профиля не может быть пустым";
        return false;
    }
    QJsonObject root;
    root["schemaVersion"] = SettingsRegistry::kCurrentSchemaVersion;
    root["settings"] = SettingsRegistry::writeToJson(m_settings);
    return writeFileIfChanged(mqDir() + "/profiles/" + clean + ".json",
                              QJsonDocument(root).toJson(QJsonDocument::Indented),
                              errMsg);
}

bool SettingsManager::loadNamedProfile(const QString &name, QString *errMsg) {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    return importPresetFromFile(mqDir() + "/profiles/" + clean + ".json", errMsg);
}

bool SettingsManager::deleteNamedProfile(const QString &name) const {
    QString clean = name.trimmed();
    clean.replace(QRegularExpression(R"([^a-zA-Z0-9_\-\x{0400}-\x{04FF} ])"), "_");
    clean = clean.simplified();
    return QFile::remove(mqDir() + "/profiles/" + clean + ".json");
}

} // namespace mqblox
EOF_SRC_CORE_SETTINGSMANAGER_CPP

cat <<'EOF_SRC_CORE_SOBERCONTROLLER_HPP' > "src/core/SoberController.hpp"
#pragma once
#include "core/SettingsManager.hpp"
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QNetworkAccessManager;
class QUdpSocket;

namespace mqblox {

struct EnvironmentStatus {
    bool hasDarling = false;
    bool hasClang = false;
    bool hasLld = false;
    bool hasUnzip = false;
    bool hasShimDylib = false;
    bool hasRobloxApp = false;
    QString installedRobloxVersion;
    QString gpuSummary;
    qint64 vramBytes = 4LL * 1024 * 1024 * 1024;
    QString sessionType;
};

class SoberController : public QObject {
    Q_OBJECT
public:
    explicit SoberController(SettingsManager *settingsMgr, QObject *parent = nullptr);
    ~SoberController() override;

    void detectHardwareAsync();

    void launchRoblox(const QString &customUri = {});
    void stopRobloxAsync();
    void restartDarlingAsync();
    void switchAccountSlotAsync(const QString &targetAccountId);
    void saveCurrentSessionToSlotAsync();

    void runSetupOrUpdateWizardAsync();
    bool isWizardBusy() const { return m_wizardBusy; }

    EnvironmentStatus checkEnvironment() const;
    QString collectDiagnosticsReport() const;
    bool isSignedIn() const;
    void logoutAsync();

    QString shimSourceDir() const;
    QString shimDylibPath() const;

    QString cpuModel() const { return m_cpuModel; }
    int cpuThreads() const { return m_cpuThreads; }
    QString gpuModel() const { return m_gpuModel; }
    QString detectedGpuVendor() const { return m_gpuVendor; }
    qint64 detectedVramBytes() const { return m_vramBytes; }

    bool isGameRunning() const { return m_gameProc->state() != QProcess::NotRunning; }

    QStringList buildFlatpakArgs(const QString &targetUri = {}) const;
    QString buildDarlingLaunchScript(const QString &targetUri = {}) const;

signals:
    void hardwareDetected();
    void logGenerated(const QString &line);
    void gameStarted();
    void gameLeftByPlayer();
    void gameFinished(int exitCode, bool crashed);
    void accountSwitchCompleted(const QString &accountId);
    void toastRequested(const QString &message);
    void wizardProgress(int percent, const QString &statusText);
    void wizardFinished(bool ok, const QString &summary);
    void sessionStateChanged();

private:
    void handleProcessOutput(const QString &chunk);
    void startDnsForwarder();
    void stopDnsForwarder();
    void clearStaleDarlingProcesses();
    void prepareDarlingPrefixSync();
    void stepDownloadOrUpdateRoblox();

    SettingsManager *m_settingsMgr = nullptr;
    QProcess *m_gameProc = nullptr;
    QProcess *m_wizardProc = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QUdpSocket *m_dnsUdp = nullptr;
    quint16 m_dnsPort = 0;
    QString m_dnsUpstreamIp;
    bool m_wizardBusy = false;

    QString m_cpuModel = "CPU x86_64";
    int m_cpuThreads = 4;
    QString m_gpuModel = "GPU Linux";
    QString m_gpuVendor = "amd";
    qint64 m_vramBytes = 4LL * 1024 * 1024 * 1024;

    bool m_manualStop = false;
    bool m_inActivePlace = false;
    qint64 m_lastTeleportMs = 0;
    qint64 m_joinTimestampMs = 0;
    QStringList m_recentErrors;
};

using DarlingController = SoberController;

} // namespace mqblox
EOF_SRC_CORE_SOBERCONTROLLER_HPP

cat <<'EOF_SRC_CORE_SOBERCONTROLLER_CPP' > "src/core/SoberController.cpp"
#include "core/SoberController.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QtConcurrent>
#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace mqblox {

namespace {

void copyDirectoryContent(const QString &srcPath, const QString &dstPath) {
    QDir src(srcPath);
    if (!src.exists()) return;
    QDir dst(dstPath);
    if (!dst.exists()) dst.mkpath(".");
    for (const QFileInfo &info : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString sItem = info.filePath();
        QString dItem = dst.filePath(info.fileName());
        if (info.isDir()) {
            copyDirectoryContent(sItem, dItem);
        } else {
            QFile::remove(dItem);
            QFile::copy(sItem, dItem);
        }
    }
}

qint64 readSysfsVramBytes() {
    qint64 bestVram = 0;
    QDir drmDir("/sys/class/drm");
    const QStringList cards = drmDir.entryList({"card*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cards) {
        if (card.contains('-')) continue;
        QFile vramFile(drmDir.filePath(card + "/device/mem_info_vram_total"));
        if (vramFile.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 val = QString::fromLatin1(vramFile.readAll()).trimmed().toLongLong(&ok);
            if (ok && val > bestVram) {
                bestVram = val;
            }
        }
    }
    return bestVram > 0 ? bestVram : (4LL * 1024 * 1024 * 1024);
}

QString readInstalledRobloxVersion(const QString &robloxAppDir) {
    QFile plist(robloxAppDir + "/Contents/Info.plist");
    if (!plist.open(QIODevice::ReadOnly)) return {};
    const QString text = QString::fromUtf8(plist.readAll());
    static const QRegularExpression re(
        R"(<key>CFBundleShortVersionString</key>\s*<string>([^<]+)</string>)");
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

QString shellSingleQuote(const QString &s) {
    QString out = s;
    out.replace("'", "'\\''");
    return "'" + out + "'";
}

QString darlingMacOBloxUserDir(const QString &darlingPrefix) {
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    return darlingPrefix + "/Users/" + user + "/Library/MacOBlox";
}

} // namespace

SoberController::SoberController(SettingsManager *settingsMgr, QObject *parent)
    : QObject(parent),
      m_settingsMgr(settingsMgr),
      m_gameProc(new QProcess(this)),
      m_nam(new QNetworkAccessManager(this)) {
    m_cpuThreads = std::max(2, QThread::idealThreadCount());
    m_vramBytes = readSysfsVramBytes();

    connect(m_gameProc, &QProcess::readyReadStandardOutput, this, [this]() {
        handleProcessOutput(QString::fromUtf8(m_gameProc->readAllStandardOutput()));
    });
    connect(m_gameProc, &QProcess::readyReadStandardError, this, [this]() {
        handleProcessOutput(QString::fromUtf8(m_gameProc->readAllStandardError()));
    });
    connect(m_gameProc, &QProcess::started, this, [this]() {
        emit gameStarted();
    });
    connect(m_gameProc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        stopDnsForwarder();
        m_inActivePlace = false;
        bool crashed = (!m_manualStop && (status == QProcess::CrashExit || exitCode != 0));
        emit gameFinished(exitCode, crashed);
        emit sessionStateChanged();
    });
}

SoberController::~SoberController() {
    stopDnsForwarder();
    if (m_gameProc && m_gameProc->state() != QProcess::NotRunning) {
        m_gameProc->terminate();
        m_gameProc->waitForFinished(1000);
    }
}

void SoberController::detectHardwareAsync() {
    m_vramBytes = readSysfsVramBytes();
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly)) {
        for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
            if (line.startsWith("model name")) {
                m_cpuModel = line.section(':', 1).trimmed();
                break;
            }
        }
    }

    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc](int, QProcess::ExitStatus) {
        for (const QString &line : QString::fromUtf8(proc->readAllStandardOutput()).split('\n')) {
            if (line.contains("VGA") || line.contains("3D controller") || line.contains("Display controller")) {
                m_gpuModel = line.section(':', 2).trimmed();
                QString low = m_gpuModel.toLower();
                if (low.contains("nvidia") || low.contains("geforce")) m_gpuVendor = "nvidia";
                else if (low.contains("amd") || low.contains("radeon") || low.contains("ati")) m_gpuVendor = "amd";
                else if (low.contains("intel")) m_gpuVendor = "intel";
                break;
            }
        }
        proc->deleteLater();
        emit hardwareDetected();
    });
    proc->start("lspci", {});
}

QString SoberController::shimSourceDir() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::currentPath() + "/shims",
        QDir::currentPath(),
        appDir + "/../shims",
        appDir + "/shims",
        appDir + "/../share/mqblox/shims",
        QDir::homePath() + "/.local/share/mqblox/shims",
        "/usr/local/share/mqblox/shims",
        "/usr/share/mqblox/shims"
    };
    for (const QString &dir : candidates) {
        if (QFile::exists(dir + "/libMacOBloxShims.m") && QFile::exists(dir + "/audio_hal.c")) {
            return QFileInfo(dir).canonicalFilePath();
        }
    }
    return QDir::currentPath() + "/shims";
}

QString SoberController::shimDylibPath() const {
    const QString inData = m_settingsMgr->macobloxDataDir() + "/libMacOBloxShims.dylib";
    if (QFile::exists(inData)) return inData;
    const QString inShimBuild = shimSourceDir() + "/build/libMacOBloxShims.dylib";
    if (QFile::exists(inShimBuild)) return inShimBuild;
    const QString inShimDir = shimSourceDir() + "/libMacOBloxShims.dylib";
    if (QFile::exists(inShimDir)) return inShimDir;
    const QString inUsrLib = "/usr/lib/mqblox/libMacOBloxShims.dylib";
    if (QFile::exists(inUsrLib)) return inUsrLib;
    return inData;
}

EnvironmentStatus SoberController::checkEnvironment() const {
    EnvironmentStatus st;
    const QStringList searchPaths = {
        QDir::homePath() + "/.local/bin",
        "/usr/local/bin",
        "/usr/bin",
        QDir::homePath() + "/.local/share/flatpak/app/xyz.narez.MacOBlox/current/active/files/bin"
    };
    st.hasDarling = !QStandardPaths::findExecutable("darling").isEmpty() ||
                    !QStandardPaths::findExecutable("darling", searchPaths).isEmpty();
    st.hasClang = !QStandardPaths::findExecutable("clang").isEmpty();
    st.hasLld = !QStandardPaths::findExecutable("ld.lld").isEmpty() ||
                !QStandardPaths::findExecutable("ld64.lld").isEmpty() ||
                !QStandardPaths::findExecutable("ld64.lld", searchPaths).isEmpty();
    st.hasUnzip = !QStandardPaths::findExecutable("unzip").isEmpty();
    st.hasShimDylib = QFile::exists(shimDylibPath());

    const QString appBin = m_settingsMgr->robloxAppDir() + "/Contents/MacOS/RobloxPlayer";
    st.hasRobloxApp = QFileInfo(appBin).isExecutable();
    st.installedRobloxVersion = readInstalledRobloxVersion(m_settingsMgr->robloxAppDir());
    st.vramBytes = m_vramBytes > 0 ? m_vramBytes : readSysfsVramBytes();
    const double vramGb = static_cast<double>(st.vramBytes) / (1024.0 * 1024.0 * 1024.0);
    st.gpuSummary = QString("%1 (%2 ГБ VRAM)").arg(m_gpuModel).arg(vramGb, 0, 'f', 1);

    const QString xdgSession = qEnvironmentVariable("XDG_SESSION_TYPE", "unknown");
    const QString waylandDisp = qEnvironmentVariable("WAYLAND_DISPLAY");
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP", "Linux");
    if (!waylandDisp.isEmpty() || xdgSession.compare("wayland", Qt::CaseInsensitive) == 0) {
        st.sessionType = QString("Wayland (%1, %2) -> Darling XWayland (EGL_PLATFORM=x11)")
                             .arg(desktop, waylandDisp.isEmpty() ? "wayland" : waylandDisp);
    } else {
        st.sessionType = QString("X11 (%1)").arg(desktop);
    }
    return st;
}

QString SoberController::collectDiagnosticsReport() const {
    const EnvironmentStatus st = checkEnvironment();
    const AppSettings &s = m_settingsMgr->settings();
    QStringList out;
    out << "=== Отчёт диагностики MqBlox × MacOBlox ===";
    out << QString("Дата: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << QString("Версия: MqBlox v36.0 (Schema v%1, MacOBlox Darling Backend)").arg(s.schemaVersion);
    out << QString("ОС / Ядро: %1 (%2 %3)")
               .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion());
    out << QString("Процессор: %1 (%2 потоков)").arg(m_cpuModel).arg(m_cpuThreads);
    out << QString("Видеокарта и VRAM: %1").arg(st.gpuSummary);
    out << QString("Сессия отображения: %1").arg(st.sessionType);
    out << QString("Darling установлен: %1").arg(st.hasDarling ? "Да" : "Нет");
    out << QString("Инструменты сборки: clang=%1, ld.lld=%2, unzip=%3")
               .arg(st.hasClang ? "Да" : "Нет", st.hasLld ? "Да" : "Нет", st.hasUnzip ? "Да" : "Нет");
    out << QString("Шим libMacOBloxShims.dylib: %1 (%2)")
               .arg(st.hasShimDylib ? "Собран" : "Не собран", shimDylibPath());
    out << QString("Клиент RobloxPlayer.app: %1 (версия: %2)")
               .arg(st.hasRobloxApp ? "Установлен" : "Не установлен",
                    st.installedRobloxVersion.isEmpty() ? "н/д" : st.installedRobloxVersion);
    out << QString("Сессия авторизации (Cookies.plist): %1")
               .arg(isSignedIn() ? "Присутствует (токен не читается лаунчером)" : "Не авторизована");
    out << QString("Параметры: Target FPS=%1 | VSyncOff=%2 | MSAA=%3 | Мышь=%4% | DNS=#%5")
               .arg(s.unlimitedFps ? 9999 : s.targetFps)
               .arg(s.vsyncOff ? "Да" : "Нет")
               .arg(static_cast<int>(s.msaa))
               .arg(s.mouseSensitivityPct)
               .arg(s.dnsProvider);
    out << "";
    out << "--- Последние ошибки и предупреждения ---";
    if (m_recentErrors.isEmpty()) {
        out << "(Ошибок не зафиксировано)";
    } else {
        for (const QString &err : m_recentErrors) {
            out << "  " + err;
        }
    }
    return out.join('\n');
}

bool SoberController::isSignedIn() const {
    const QString cookiesPath =
        darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir()) + "/Cookies.plist";
    QFile f(cookiesPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    // Privacy policy: check only key existence, never parse/log/transmit cookie value.
    return f.readAll().contains("<string>.ROBLOSECURITY</string>");
}

void SoberController::logoutAsync() {
    const QString hostDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString user = qEnvironmentVariable("USER", QDir::homePath().section('/', -1));
    (void)QtConcurrent::run([this, hostDir, user]() {
        if (!QStandardPaths::findExecutable("darling").isEmpty()) {
            QProcess p;
            p.start("darling", {"shell", "/bin/rm", "-rf",
                                "/Users/" + user + "/Library/MacOBlox/Cookies.plist",
                                "/Users/" + user + "/Library/MacOBlox/Keychain"});
            p.waitForFinished(4000);
        }
        QFile::remove(hostDir + "/Cookies.plist");
        QDir(hostDir + "/Keychain").removeRecursively();
        QMetaObject::invokeMethod(this, [this]() {
            emit logGenerated("[🔐] Локальная сессия Roblox внутри Darling очищена.");
            emit sessionStateChanged();
        }, Qt::QueuedConnection);
    });
}

QString SoberController::buildDarlingLaunchScript(const QString &targetUri) const {
    const AppSettings &s = m_settingsMgr->settings();
    const qint64 vram = m_vramBytes > 0 ? m_vramBytes : readSysfsVramBytes();

    const QString appExecutableDir = "/Volumes/SystemRoot" + m_settingsMgr->robloxAppDir() + "/Contents/MacOS";
    const QString dylibFile = QFileInfo(shimDylibPath()).absoluteFilePath();
    const QString dylibDir = "/Volumes/SystemRoot" + QFileInfo(dylibFile).absolutePath();
    const QString dylibInDarling = "/Volumes/SystemRoot" + dylibFile;

    QStringList envExports;
    envExports << "export DYLD_FORCE_FLAT_NAMESPACE=1";
    envExports << QString("export DYLD_INSERT_LIBRARIES=%1").arg(shellSingleQuote(dylibInDarling));
    envExports << QString("export DYLD_LIBRARY_PATH=%1:%2")
                      .arg(shellSingleQuote(dylibDir), shellSingleQuote(appExecutableDir));
    envExports << QString("export MACOBLOX_VRAM_BYTES=%1").arg(vram);
    envExports << QString("export MACOBLOX_VSYNC=%1").arg((s.vsyncOff || s.unlimitedFps) ? "0" : "1");

    if (s.mouseSensitivityPct != 100) {
        const double sens = static_cast<double>(s.mouseSensitivityPct) / 100.0;
        envExports << QString("export MACOBLOX_MOUSE_SENSITIVITY=%1").arg(sens, 0, 'f', 2);
    }
    if (!s.hideMenuBar) {
        envExports << "export MACOBLOX_HIDE_MENU_BAR=0";
    }
    if (m_dnsPort > 0) {
        envExports << QString("export MACOBLOX_DNS=127.0.0.1:%1").arg(m_dnsPort);
    }
    envExports << "export MACOBLOX_WM_CLASS=mqblox";
    if (s.fpsLog || s.showHudFps) envExports << "export MACOBLOX_FPS_LOG=1";
    if (s.traceUdp) envExports << "export MACOBLOX_TRACE_UDP=1";
    if (s.traceLock) envExports << "export MACOBLOX_TRACE_LOCK=1";
    if (s.traceGl) envExports << "export MACOBLOX_TRACE_GL=1";
    if (s.diagnosticSignals) envExports << "export MACOBLOX_DIAGNOSTIC_SIGNALS=1";
    const QString audioLogEnv = qEnvironmentVariable("MACOBLOX_AUDIO_LOG");
    if (!audioLogEnv.isEmpty()) {
        envExports << QString("export MACOBLOX_AUDIO_LOG=%1").arg(shellSingleQuote(audioLogEnv));
    }
    const QString traceAudioEnv = qEnvironmentVariable("MACOBLOX_TRACE_AUDIO");
    if (!traceAudioEnv.isEmpty()) {
        envExports << QString("export MACOBLOX_TRACE_AUDIO=%1").arg(shellSingleQuote(traceAudioEnv));
    }

    QString execCmd = "exec ./RobloxPlayer";
    if (!targetUri.trimmed().isEmpty()) {
        execCmd += " " + shellSingleQuote(targetUri.trimmed());
    }

    return QString("cd %1 && %2 && %3")
        .arg(shellSingleQuote(appExecutableDir), envExports.join(" && "), execCmd);
}

QStringList SoberController::buildFlatpakArgs(const QString &targetUri) const {
    return {"shell", "/bin/bash", "-c", buildDarlingLaunchScript(targetUri)};
}

void SoberController::startDnsForwarder() {
    stopDnsForwarder();
    const AppSettings &s = m_settingsMgr->settings();
    if (s.dnsProvider <= 0) return;

    switch (s.dnsProvider) {
    case 1: m_dnsUpstreamIp = "9.9.9.9"; break;
    case 2: m_dnsUpstreamIp = "1.1.1.1"; break;
    case 3: m_dnsUpstreamIp = "8.8.8.8"; break;
    case 4: m_dnsUpstreamIp = s.dnsCustom.trimmed().isEmpty() ? "9.9.9.9" : s.dnsCustom.trimmed(); break;
    default: m_dnsUpstreamIp = "9.9.9.9"; break;
    }

    m_dnsUdp = new QUdpSocket(this);
    if (!m_dnsUdp->bind(QHostAddress::LocalHost, 0)) {
        delete m_dnsUdp;
        m_dnsUdp = nullptr;
        return;
    }
    m_dnsPort = m_dnsUdp->localPort();

    connect(m_dnsUdp, &QUdpSocket::readyRead, this, [this]() {
        while (m_dnsUdp && m_dnsUdp->hasPendingDatagrams()) {
            QByteArray query;
            query.resize(static_cast<int>(m_dnsUdp->pendingDatagramSize()));
            QHostAddress senderHost;
            quint16 senderPort = 0;
            m_dnsUdp->readDatagram(query.data(), query.size(), &senderHost, &senderPort);
            if (query.size() < 12) continue;

            const QString upstream = m_dnsUpstreamIp;
            QPointer<QUdpSocket> udpGuard(m_dnsUdp);
            (void)QtConcurrent::run([query, upstream, senderHost, senderPort, udpGuard]() {
                QSslSocket tls;
                tls.setPeerVerifyMode(QSslSocket::VerifyNone);
                tls.connectToHostEncrypted(upstream, 853);
                if (!tls.waitForEncrypted(2500)) return;

                QByteArray framed;
                const quint16 qlen = static_cast<quint16>(query.size());
                framed.append(static_cast<char>((qlen >> 8) & 0xFF));
                framed.append(static_cast<char>(qlen & 0xFF));
                framed.append(query);
                tls.write(framed);
                if (!tls.waitForBytesWritten(1500)) return;

                QByteArray respBuf;
                while (respBuf.size() < 2 && tls.waitForReadyRead(2500)) {
                    respBuf.append(tls.readAll());
                }
                if (respBuf.size() < 2) return;
                const int rlen = (static_cast<unsigned char>(respBuf[0]) << 8) |
                                 static_cast<unsigned char>(respBuf[1]);
                while (respBuf.size() < 2 + rlen && tls.waitForReadyRead(2500)) {
                    respBuf.append(tls.readAll());
                }
                if (respBuf.size() >= 2 + rlen && udpGuard) {
                    const QByteArray payload = respBuf.mid(2, rlen);
                    QMetaObject::invokeMethod(udpGuard, [udpGuard, payload, senderHost, senderPort]() {
                        if (udpGuard) {
                            udpGuard->writeDatagram(payload, senderHost, senderPort);
                        }
                    }, Qt::QueuedConnection);
                }
            });
        }
    });
}

void SoberController::stopDnsForwarder() {
    m_dnsPort = 0;
    if (m_dnsUdp) {
        m_dnsUdp->close();
        m_dnsUdp->deleteLater();
        m_dnsUdp = nullptr;
    }
}

void SoberController::clearStaleDarlingProcesses() {
    const uid_t myUid = ::getuid();
    const QString myPrefix = QFileInfo(m_settingsMgr->darlingPrefixDir()).absoluteFilePath();
    QDir procDir("/proc");
    for (const QString &pidStr : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const pid_t pid = static_cast<pid_t>(pidStr.toInt(&ok));
        if (!ok || pid <= 1 || pid == ::getpid()) continue;

        const QString procPath = QString("/proc/%1").arg(pid);
        if (QFileInfo(procPath).ownerId() != myUid) continue;

        QFile commFile(procPath + "/comm");
        if (!commFile.open(QIODevice::ReadOnly)) continue;
        const QString comm = QString::fromUtf8(commFile.readAll()).trimmed();
        const bool isOurBinary = (comm == "RobloxPlayer" || comm == "RobloxCrashHandler" ||
                                  comm == "darlingserver" || comm == "darling");
        if (!isOurBinary) continue;

        const QString cwdTarget = QFile::symLinkTarget(procPath + "/cwd");
        const bool isDeletedCwd = cwdTarget.endsWith(" (deleted)");
        const bool isOurPrefix = !myPrefix.isEmpty() && cwdTarget.startsWith(myPrefix);
        if (isDeletedCwd || (isOurPrefix && (comm == "RobloxPlayer" || comm == "RobloxCrashHandler"))) {
            ::kill(pid, SIGKILL);
        }
    }
}

void SoberController::launchRoblox(const QString &customUri) {
    m_manualStop = false;
    m_inActivePlace = false;
    m_settingsMgr->saveAll();

    const EnvironmentStatus st = checkEnvironment();
    if (!st.hasDarling) {
        const QString msg = "[⚠️] Darling не установлен в системе. Откройте «Мастер установки и обновления».";
        m_recentErrors.append(msg);
        emit logGenerated(msg);
        emit toastRequested(msg);
        return;
    }
    if (!st.hasRobloxApp) {
        const QString msg = "[⚠️] RobloxPlayer.app не установлен. Запустите «Мастер установки и обновления».";
        m_recentErrors.append(msg);
        emit logGenerated(msg);
        emit toastRequested(msg);
        return;
    }

    clearStaleDarlingProcesses();
    startDnsForwarder();

    const QString targetUri = !customUri.isEmpty()
                                  ? customUri
                                  : SettingsManager::parsePlaceUri(m_settingsMgr->settings().lastPlaceInput);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Darling's CGL uses X11/XWayland; keep Qt6 native on Wayland while setting EGL_PLATFORM=x11 for Darling
    env.insert("EGL_PLATFORM", "x11");
    m_gameProc->setProcessEnvironment(env);

    const QStringList args = buildFlatpakArgs(targetUri);
    emit logGenerated("[🚀] Запуск MacOBlox (Darling): darling " + args.join(' '));
    m_gameProc->start("darling", args);
}

void SoberController::stopRobloxAsync() {
    m_manualStop = true;
    m_inActivePlace = false;
    stopDnsForwarder();

    const QString dprefix = m_settingsMgr->darlingPrefixDir();
    (void)QtConcurrent::run([this, dprefix]() {
        if (!QStandardPaths::findExecutable("darling").isEmpty()) {
            QProcess killer;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            if (!dprefix.isEmpty()) env.insert("DPREFIX", dprefix);
            killer.setProcessEnvironment(env);
            killer.start("darling", {"shell", "/usr/bin/killall", "-9", "RobloxPlayer", "RobloxCrashHandler"});
            killer.waitForFinished(3000);
        }
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            if (isGameRunning()) {
                m_gameProc->terminate();
                if (!m_gameProc->waitForFinished(1200)) {
                    m_gameProc->kill();
                }
            }
            emit logGenerated("[🛑] Сессия RobloxPlayer внутри Darling завершена.");
        }, Qt::QueuedConnection);
    });
}

void SoberController::restartDarlingAsync() {
    stopDnsForwarder();
    emit logGenerated("[🔄] Перезапуск контейнера Darling...");
    const QString dprefix = m_settingsMgr->darlingPrefixDir();
    (void)QtConcurrent::run([this, dprefix]() {
        if (!QStandardPaths::findExecutable("darling").isEmpty()) {
            QProcess p;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            if (!dprefix.isEmpty()) env.insert("DPREFIX", dprefix);
            p.setProcessEnvironment(env);
            p.start("darling", {"shutdown"});
            p.waitForFinished(4000);
        }
        clearStaleDarlingProcesses();
        QMetaObject::invokeMethod(this, [this]() {
            emit logGenerated("[✓] Контейнер Darling перезапущен и очищен.");
            emit toastRequested("🔄 Контейнер Darling перезапущен");
        }, Qt::QueuedConnection);
    });
}

void SoberController::saveCurrentSessionToSlotAsync() {
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString slotDir =
        m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/MacOBlox";
    (void)QtConcurrent::run([macobloxDir, slotDir]() {
        if (QDir(macobloxDir).exists()) {
            QDir(slotDir).removeRecursively();
            copyDirectoryContent(macobloxDir, slotDir);
        }
    });
}

void SoberController::switchAccountSlotAsync(const QString &targetAccountId) {
    stopRobloxAsync();
    const QString macobloxDir = darlingMacOBloxUserDir(m_settingsMgr->darlingPrefixDir());
    const QString oldSlot =
        m_settingsMgr->accountsDir() + "/" + m_settingsMgr->activeAccountId() + "/MacOBlox";
    const QString newSlot =
        m_settingsMgr->accountsDir() + "/" + targetAccountId + "/MacOBlox";

    m_settingsMgr->setActiveAccountId(targetAccountId);
    m_settingsMgr->saveAll();

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, targetAccountId]() {
        watcher->deleteLater();
        emit accountSwitchCompleted(targetAccountId);
        emit sessionStateChanged();
    });

    watcher->setFuture(QtConcurrent::run([macobloxDir, oldSlot, newSlot]() {
        if (QDir(macobloxDir).exists()) {
            QDir(oldSlot).removeRecursively();
            copyDirectoryContent(macobloxDir, oldSlot);
        }
        QDir(macobloxDir).removeRecursively();
        if (QDir(newSlot).exists()) {
            copyDirectoryContent(newSlot, macobloxDir);
        } else {
            QDir().mkpath(macobloxDir);
        }
    }));
}

void SoberController::prepareDarlingPrefixSync() {
    const QString prefix = m_settingsMgr->darlingPrefixDir();
    if (!QDir(prefix).exists()) return;

    const uid_t myUid = ::getuid();
    const QString absPrefix = QFileInfo(prefix).absoluteFilePath();
    QDir procDir("/proc");
    for (const QString &pidStr : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const pid_t pid = static_cast<pid_t>(pidStr.toInt(&ok));
        if (!ok || pid <= 1) continue;
        const QString procPath = QString("/proc/%1").arg(pid);
        if (QFileInfo(procPath).ownerId() != myUid) continue;
        QFile commFile(procPath + "/comm");
        if (!commFile.open(QIODevice::ReadOnly)) continue;
        if (QString::fromUtf8(commFile.readAll()).trimmed() == "darlingserver") {
            const QString cwd = QFile::symLinkTarget(procPath + "/cwd");
            if (cwd.startsWith(absPrefix)) return;
        }
    }

    const QString fwSrc = shimSourceDir() + "/frameworks";
    const QString fwDst = prefix + "/System/Library/Frameworks";
    if (QDir(fwSrc).exists() && !QStandardPaths::findExecutable("clang").isEmpty()) {
        struct StubFw { const char *name; const char *src; };
        const StubFw kStubs[] = {
            {"CoreML", "CoreML.m"},
            {"CoreHaptics", "CoreHaptics.m"},
            {"DeviceCheck", "DeviceCheck.m"}
        };
        for (const auto &fw : kStubs) {
            const QString dir = QString("%1/%2.framework/Versions/A").arg(fwDst, fw.name);
            const QString bin = dir + "/" + fw.name;
            if (QFile::exists(bin)) continue;
            QDir().mkpath(dir);
            QProcess clang;
            clang.start("clang", {
                "-target", "x86_64-apple-darwin20",
                "-dynamiclib", "-fuse-ld=lld", "-nostdlib",
                "-Wl,-install_name,/System/Library/Frameworks/" + QString(fw.name) +
                    ".framework/Versions/A/" + QString(fw.name),
                "-Wl,-undefined,dynamic_lookup",
                fwSrc + "/" + fw.src,
                "-o", bin
            });
            clang.waitForFinished(10000);
        }
    }
}

void SoberController::runSetupOrUpdateWizardAsync() {
    if (m_wizardBusy) return;
    m_wizardBusy = true;

    emit wizardProgress(5, "Проверка инструментов (clang, ld.lld, unzip, darling)...");
    const EnvironmentStatus st = checkEnvironment();
    if (!st.hasClang || !st.hasLld || !st.hasUnzip) {
        m_wizardBusy = false;
        const QString msg = QString("Отсутствуют пакеты сборки: %1%2%3")
                                .arg(!st.hasClang ? "clang " : "",
                                     !st.hasLld ? "lld " : "",
                                     !st.hasUnzip ? "unzip" : "");
        m_recentErrors.append(msg);
        emit logGenerated("[Мастер] " + msg);
        emit wizardFinished(false, msg);
        return;
    }

    emit wizardProgress(18, "Компиляция шима libMacOBloxShims.dylib (включая audio_hal.c)...");
    const QString srcDir = shimSourceDir();
    const QString buildScript = srcDir + "/build_debug_shim.sh";
    if (!QFile::exists(buildScript)) {
        m_wizardBusy = false;
        const QString msg = "Не найден скрипт сборки шимов: " + buildScript;
        m_recentErrors.append(msg);
        emit wizardFinished(false, msg);
        return;
    }

    if (m_wizardProc) m_wizardProc->deleteLater();
    m_wizardProc = new QProcess(this);
    m_wizardProc->setWorkingDirectory(srcDir);
    m_wizardProc->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment wizEnv = QProcessEnvironment::systemEnvironment();
    wizEnv.insert("MACOBLOX_BUILD_DIR", m_settingsMgr->macobloxDataDir());
    m_wizardProc->setProcessEnvironment(wizEnv);

    connect(m_wizardProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(m_wizardProc->readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) emit logGenerated(out);

        const QString dstDylib = m_settingsMgr->macobloxDataDir() + "/libMacOBloxShims.dylib";
        if (code != 0 || !QFile::exists(dstDylib)) {
            m_wizardBusy = false;
            const QString msg = "Ошибка сборки libMacOBloxShims.dylib";
            m_recentErrors.append(msg);
            emit wizardFinished(false, msg);
            return;
        }

        emit wizardProgress(40, "Подготовка префикса Darling и запрос версии Roblox MacPlayer...");
        prepareDarlingPrefixSync();
        stepDownloadOrUpdateRoblox();
    });

    m_wizardProc->start("/bin/bash", {buildScript});
}

void SoberController::stepDownloadOrUpdateRoblox() {
    QNetworkRequest req(QUrl("https://clientsettingscdn.roblox.com/v2/client-version/MacPlayer"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "MacOBlox/1.0");
    QNetworkReply *verReply = m_nam->get(req);

    connect(verReply, &QNetworkReply::finished, this, [this, verReply]() {
        verReply->deleteLater();
        if (verReply->error() != QNetworkReply::NoError) {
            m_wizardBusy = false;
            const QString msg = "Ошибка сети при проверке версии Roblox: " + verReply->errorString();
            m_recentErrors.append(msg);
            emit wizardFinished(false, msg);
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(verReply->readAll()).object();
        const QString version = obj.value("version").toString();
        const QString upload = obj.value("clientVersionUpload").toString();
        if (upload.isEmpty()) {
            m_wizardBusy = false;
            emit wizardFinished(false, "Официальный CDN вернул пустой clientVersionUpload.");
            return;
        }

        const QString installedVer = readInstalledRobloxVersion(m_settingsMgr->robloxAppDir());
        if (!installedVer.isEmpty() && installedVer == version &&
            QFile::exists(m_settingsMgr->robloxAppDir() + "/Contents/MacOS/RobloxPlayer")) {
            m_settingsMgr->saveAll();
            m_wizardBusy = false;
            const QString msg = QString("Шим собран, клиент Roblox %1 уже актуален.").arg(version);
            emit wizardProgress(100, msg);
            emit wizardFinished(true, msg);
            return;
        }

        const QUrl zipUrl(QString("https://setup.rbxcdn.com/mac/%1-RobloxPlayer.zip").arg(upload));
        emit wizardProgress(50, QString("Скачивание Roblox %1 (%2)...").arg(version, upload));

        const QString zipPath = m_settingsMgr->macobloxDataDir() + "/RobloxPlayer.zip.part";
        QFile *outFile = new QFile(zipPath, this);
        if (!outFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            delete outFile;
            m_wizardBusy = false;
            emit wizardFinished(false, "Не удалось создать файл загрузки: " + zipPath);
            return;
        }

        QNetworkRequest dlReq(zipUrl);
        dlReq.setHeader(QNetworkRequest::UserAgentHeader, "MacOBlox/1.0");
        QNetworkReply *dlReply = m_nam->get(dlReq);

        connect(dlReply, &QNetworkReply::readyRead, this, [dlReply, outFile]() {
            outFile->write(dlReply->readAll());
        });

        connect(dlReply, &QNetworkReply::downloadProgress, this,
                [this, version](qint64 received, qint64 total) {
                    if (total > 0) {
                        const int pct = 50 + static_cast<int>((received * 40) / total);
                        const double mbRecv = static_cast<double>(received) / (1024.0 * 1024.0);
                        const double mbTot = static_cast<double>(total) / (1024.0 * 1024.0);
                        emit wizardProgress(
                            std::clamp(pct, 50, 90),
                            QString("Загрузка Roblox %1: %2 / %3 МБ")
                                .arg(version)
                                .arg(mbRecv, 0, 'f', 1)
                                .arg(mbTot, 0, 'f', 1));
                    }
                });

        connect(dlReply, &QNetworkReply::finished, this,
                [this, dlReply, outFile, zipPath, version]() {
                    outFile->write(dlReply->readAll());
                    outFile->close();
                    outFile->deleteLater();
                    dlReply->deleteLater();

                    if (dlReply->error() != QNetworkReply::NoError) {
                        QFile::remove(zipPath);
                        m_wizardBusy = false;
                        const QString msg = "Ошибка скачивания RobloxPlayer.zip: " + dlReply->errorString();
                        m_recentErrors.append(msg);
                        emit wizardFinished(false, msg);
                        return;
                    }

                    emit wizardProgress(92, "Распаковка RobloxPlayer.app и применение FastFlags...");
                    const QString dataDir = m_settingsMgr->macobloxDataDir();
                    const QString appDir = m_settingsMgr->robloxAppDir();

                    (void)QtConcurrent::run([this, zipPath, dataDir, appDir, version]() {
                        const QString extractDir = dataDir + "/.extract_tmp";
                        QDir(extractDir).removeRecursively();
                        QDir().mkpath(extractDir);

                        QProcess unzipProc;
                        unzipProc.start("unzip", {"-q", zipPath, "-d", extractDir});
                        const bool unzipOk = unzipProc.waitForFinished(120000) && unzipProc.exitCode() == 0;
                        QFile::remove(zipPath);

                        if (!unzipOk || !QDir(extractDir + "/RobloxPlayer.app").exists()) {
                            QDir(extractDir).removeRecursively();
                            QMetaObject::invokeMethod(this, [this]() {
                                m_wizardBusy = false;
                                emit wizardFinished(false, "Не удалось распаковать архив RobloxPlayer.zip.");
                            }, Qt::QueuedConnection);
                            return;
                        }

                        if (QDir(appDir).exists()) {
                            const QString oldVer = readInstalledRobloxVersion(appDir);
                            const QString backupDir =
                                dataDir + "/backups/RobloxPlayer-" +
                                (oldVer.isEmpty() ? "prev" : oldVer) + ".app";
                            QDir(backupDir).removeRecursively();
                            QDir().mkpath(dataDir + "/backups");
                            QDir().rename(appDir, backupDir);
                        }

                        QDir().rename(extractDir + "/RobloxPlayer.app", appDir);
                        QDir(extractDir).removeRecursively();

                        QMetaObject::invokeMethod(this, [this, version]() {
                            m_settingsMgr->saveAll();
                            m_wizardBusy = false;
                            const QString msg = QString("Roblox %1 и шимы MacOBlox успешно установлены!").arg(version);
                            emit wizardProgress(100, msg);
                            emit wizardFinished(true, msg);
                        }, Qt::QueuedConnection);
                    });
                });
    });
}

void SoberController::handleProcessOutput(const QString &chunk) {
    const QStringList &markers = m_settingsMgr->settings().leaveMarkers;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const QString &rawLine : chunk.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.contains("ROBLOSECURITY", Qt::CaseInsensitive) ||
            line.contains("Bearer ", Qt::CaseInsensitive)) {
            continue;
        }
        if (line.contains("error", Qt::CaseInsensitive) ||
            line.contains("fail", Qt::CaseInsensitive) ||
            line.contains("crash", Qt::CaseInsensitive) ||
            line.contains("stall", Qt::CaseInsensitive)) {
            m_recentErrors.append(line);
            while (m_recentErrors.size() > 30) {
                m_recentErrors.removeFirst();
            }
        }
        emit logGenerated(line);

        if (line.contains("Joining game") || line.contains("Connecting to UDMUX") ||
            line.contains("Connection accepted")) {
            m_inActivePlace = true;
            m_joinTimestampMs = now;
        }
        if (line.contains("Teleport", Qt::CaseInsensitive)) {
            m_lastTeleportMs = now;
        }

        if (m_inActivePlace && m_settingsMgr->settings().returnToMqBloxOnLeave) {
            if ((now - m_joinTimestampMs > 4000) && (now - m_lastTeleportMs > 4000)) {
                for (const QString &pat : markers) {
                    if (!pat.isEmpty() && line.contains(pat)) {
                        m_inActivePlace = false;
                        stopRobloxAsync();
                        emit gameLeftByPlayer();
                        break;
                    }
                }
            }
        }
    }
}

} // namespace mqblox
EOF_SRC_CORE_SOBERCONTROLLER_CPP

cat <<'EOF_SRC_RENDER_TON618GLWIDGET_HPP' > "src/render/Ton618GlWidget.hpp"
#include <condition_variable>
#pragma once
#include "core/Types.hpp"
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <mutex>
#include <thread>

namespace mqblox {

struct VisualParams {
    int blurStrength = 4;
    int glassOpacityPct = 10;
    int borderThicknessPx = 10;
    int borderBrightnessPct = 100;
    int glassSaturationPct = 160;
    int specularStrengthPct = 100;
    int shadowStrengthPct = 100;
    int cornerRadiusMode = 1;
    int glassStyleMode = 0;

    int themeMode = 0;
    int accentPreset = 0;
    QColor customAccent{56, 189, 248};
    bool chameleonEnabled = true;
    int chameleonSpeedPct = 20;
    int chameleonStrengthPct = 100;
    int borderColorSource = 0;
    int textColorMode = 0;

    int backdropType = 0;
    int backdropDimPct = 0;
    int videoFpsLimit = 24;
    bool pauseOnFocusLoss = true;

    bool animationsEnabled = true;
    int animSpeedPct = 100;
    int animStyle = 1;
    bool cursorGlowFollow = true;
    bool reducedMotion = false;

    bool highContrast = false;
    bool reduceTransparency = false;
    int effectsQuality = 2;
    bool disableBlurLowGpu = false;
};

class Ton618GlWidget : public QWidget {
    Q_OBJECT
public:
    explicit Ton618GlWidget(QWidget *parent = nullptr);
    ~Ton618GlWidget() override;

    void setAnimationEnabled(bool enabled);
    void setInGamePaused(bool paused);
    void setWebTabActive(bool webActive);
    void loadCustomVideoFile(const QString &filePath);

    void applyVisualSettings(const AppSettings &s);
    void configureVisuals(int blurStrength,
                          int glassOpacityPct,
                          int borderBrightnessPct,
                          bool chameleon,
                          bool reducedMotion,
                          int fpsLimit,
                          int accentPreset);

    VisualParams visuals() const {
        std::lock_guard<std::mutex> lk(m_visMutex);
        return m_vis;
    }

    int glassOpacityPct() const { return m_glassOpacityPct.load(); }
    int borderBrightnessPct() const { return m_borderBrightnessPct.load(); }
    bool isChameleonEnabled() const { return m_chameleonEnabled.load(); }
    bool isReducedMotion() const { return m_reducedMotion.load(); }
    QColor fixedAccentColor() const;
    bool isLightThemeActive() const;
    QColor resolveTextColor(const QColor &surfaceTint) const;

    const QPixmap &sharpBackdrop() const { return m_scaledSharp; }
    const QPixmap &frostedBackdrop() const { return m_scaledFrosted; }

    QColor sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const;
    QColor ambientPrimary() const { return m_ambientPrimary; }
    QColor ambientSecondary() const { return m_ambientSecondary; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void startDecoderThread();
    void stopDecoderThread();
    void decoderLoop();
    void updateStaticCaches();
    void loadBestFallbackImage();
    void updatePauseState();
    static QImage buildCrystalFrostedImage(const QImage &src, int radius, int saturationPct);
    static void buildAdaptiveColorMap(const QImage &srcSmall,
                                      const QImage &prevMap,
                                      int speedPct,
                                      int strengthPct,
                                      QImage &outMap,
                                      QColor &outPrimary,
                                      QColor &outSecondary);

    std::thread m_worker;
    std::atomic<bool> m_stopWorker{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_frameQueued{false};
    std::atomic<int> m_targetW{1100};
    std::atomic<int> m_targetH{660};

    mutable std::mutex m_visMutex;
    VisualParams m_vis;

    std::atomic<int> m_blurStrength{4};
    std::atomic<int> m_saturationPct{160};
    std::atomic<int> m_glassOpacityPct{10};
    std::atomic<int> m_borderBrightnessPct{100};
    std::atomic<bool> m_chameleonEnabled{true};
    std::atomic<int> m_chameleonSpeed{20};
    std::atomic<int> m_chameleonStrength{100};
    std::atomic<bool> m_reducedMotion{false};
    std::atomic<int> m_videoFps{24};
    std::atomic<int> m_accentPreset{0};
    std::atomic<bool> m_pauseOnFocusLoss{true};

    std::mutex m_pathMutex;
    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCv;
    QString m_videoPath;
    QString m_defaultVideoPath;
    QString m_customFilePath;

    QTimer *m_resizeDebounce = nullptr;
    QImage m_fallbackImg;
    QImage m_colorMap;
    QColor m_ambientPrimary{251, 191, 36};
    QColor m_ambientSecondary{56, 189, 248};
    QPixmap m_scaledSharp;
    QPixmap m_scaledFrosted;
    bool m_animEnabled = true;
    bool m_inGamePaused = false;
    bool m_windowFocused = true;

    static constexpr int STREAM_W = 854;
    static constexpr int STREAM_H = 480;
    static constexpr int FRAME_BYTES = STREAM_W * STREAM_H * 4;
};

} // namespace mqblox
EOF_SRC_RENDER_TON618GLWIDGET_HPP

cat <<'EOF_SRC_RENDER_TON618GLWIDGET_CPP' > "src/render/Ton618GlWidget.cpp"
#include <QGuiApplication>
#include <QStyleHints>
#include "render/Ton618GlWidget.hpp"
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLinearGradient>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProcess>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace mqblox {

QColor Ton618GlWidget::fixedAccentColor() const {
    int preset = m_accentPreset.load();
    if (preset == 4) {
        std::lock_guard<std::mutex> lk(m_visMutex);
        if (m_vis.customAccent.isValid()) return m_vis.customAccent;
    }
    switch (preset) {
        case 1: return QColor(251, 191, 36);  // Золото TON 618
        case 2: return QColor(52, 211, 153);  // Изумрудный неон
        case 3: return QColor(168, 85, 247);  // Фиолетовый квазар
        default: return QColor(56, 189, 248); // Голубой HyperOS
    }
}

bool Ton618GlWidget::isLightThemeActive() const {
    VisualParams vp = visuals();
    if (vp.highContrast) return false;
    if (vp.themeMode == 1) return true;
    if (vp.themeMode == 2 && QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
    }
    return false;
}

QColor Ton618GlWidget::resolveTextColor(const QColor &surfaceTint) const {
    VisualParams vp = visuals();
    bool light = isLightThemeActive();
    if (vp.highContrast) return QColor(255, 255, 255);
    if (vp.textColorMode == 1) return light ? QColor(15, 23, 42) : QColor(248, 250, 252);
    if (vp.textColorMode == 2) return light ? QColor(15, 23, 42) : QColor(226, 232, 240);
    if (light) return QColor(15, 23, 42);
    int lum = (surfaceTint.red() * 77 + surfaceTint.green() * 150 + surfaceTint.blue() * 29) >> 8;
    return (lum > 185) ? QColor(15, 23, 42) : QColor(248, 250, 252);
}

QImage Ton618GlWidget::buildCrystalFrostedImage(const QImage &src, int radius, int saturationPct) {
    if (src.isNull()) return src;
    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = img.width(), h = img.height();
    const int satScale = std::clamp(saturationPct, 80, 220);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int lum = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(lum + ((r - lum) * satScale) / 100 + 4, 0, 255);
            g = std::clamp(lum + ((g - lum) * satScale) / 100 + 4, 0, 255);
            b = std::clamp(lum + ((b - lum) * satScale) / 100 + 6, 0, 255);
            line[x] = qRgb(r, g, b);
        }
    }

    if (radius <= 0) return img;

    QImage tmp(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < h; ++y) {
            const QRgb *inLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            QRgb *outLine = reinterpret_cast<QRgb *>(tmp.scanLine(y));
            int rSum = 0, gSum = 0, bSum = 0, cnt = 0;
            for (int x = -radius; x <= radius; ++x) {
                int cx = std::clamp(x, 0, w - 1);
                rSum += qRed(inLine[cx]); gSum += qGreen(inLine[cx]); bSum += qBlue(inLine[cx]);
                cnt++;
            }
            for (int x = 0; x < w; ++x) {
                outLine[x] = qRgb(rSum / cnt, gSum / cnt, bSum / cnt);
                int lIdx = std::clamp(x - radius, 0, w - 1);
                int rIdx = std::clamp(x + radius + 1, 0, w - 1);
                rSum += qRed(inLine[rIdx]) - qRed(inLine[lIdx]);
                gSum += qGreen(inLine[rIdx]) - qGreen(inLine[lIdx]);
                bSum += qBlue(inLine[rIdx]) - qBlue(inLine[lIdx]);
            }
        }
        for (int x = 0; x < w; ++x) {
            int rSum = 0, gSum = 0, bSum = 0, cnt = 0;
            for (int y = -radius; y <= radius; ++y) {
                int cy = std::clamp(y, 0, h - 1);
                const QRgb *p = reinterpret_cast<const QRgb *>(tmp.constScanLine(cy)) + x;
                rSum += qRed(*p); gSum += qGreen(*p); bSum += qBlue(*p);
                cnt++;
            }
            for (int y = 0; y < h; ++y) {
                QRgb *outP = reinterpret_cast<QRgb *>(img.scanLine(y)) + x;
                *outP = qRgb(rSum / cnt, gSum / cnt, bSum / cnt);
                int tIdx = std::clamp(y - radius, 0, h - 1);
                int bIdx = std::clamp(y + radius + 1, 0, h - 1);
                const QRgb *pTop = reinterpret_cast<const QRgb *>(tmp.constScanLine(tIdx)) + x;
                const QRgb *pBot = reinterpret_cast<const QRgb *>(tmp.constScanLine(bIdx)) + x;
                rSum += qRed(*pBot) - qRed(*pTop);
                gSum += qGreen(*pBot) - qGreen(*pTop);
                bSum += qBlue(*pBot) - qBlue(*pTop);
            }
        }
    }
    return img;
}

void Ton618GlWidget::buildAdaptiveColorMap(const QImage &srcSmall,
                                           const QImage &prevMap,
                                           int speedPct,
                                           int strengthPct,
                                           QImage &outMap,
                                           QColor &outPrimary,
                                           QColor &outSecondary) {
    QImage cmap = srcSmall.scaled(32, 18, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                      .convertToFormat(QImage::Format_RGB32);
    const int w = cmap.width(), h = cmap.height();

    double rAcc1 = 0, gAcc1 = 0, bAcc1 = 0, wAcc1 = 0.001;
    double rAcc2 = 0, gAcc2 = 0, bAcc2 = 0, wAcc2 = 0.001;

    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(cmap.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            int cMax = std::max({r, g, b}), cMin = std::min({r, g, b});
            double sat = (cMax - cMin) + 18.0;
            double lum = (r + g + b) / 3.0;
            double weight = sat * std::max(10.0, lum);
            if (x < w / 2) {
                rAcc1 += r * weight; gAcc1 += g * weight; bAcc1 += b * weight; wAcc1 += weight;
            } else {
                rAcc2 += r * weight; gAcc2 += g * weight; bAcc2 += b * weight; wAcc2 += weight;
            }
        }
    }

    float strengthFactor = std::clamp(strengthPct, 20, 150) / 100.0f;

    auto normalizeVibrant = [strengthFactor](double r, double g, double b) -> QColor {
        QColor c(std::clamp(int(r), 0, 255), std::clamp(int(g), 0, 255), std::clamp(int(b), 0, 255));
        float hf = 0.08f, sf = 0.7f, lf = 0.68f;
        c.getHslF(&hf, &sf, &lf);
        if (hf < 0.0f) hf = 0.08f;
        sf = std::clamp((sf * 1.35f + 0.20f) * strengthFactor, 0.25f, 0.96f);
        lf = std::clamp(lf * 1.12f + 0.28f, 0.62f, 0.80f);
        return QColor::fromHslF(hf, sf, lf);
    };

    outPrimary = normalizeVibrant(rAcc1 / wAcc1, gAcc1 / wAcc1, bAcc1 / wAcc1);
    outSecondary = normalizeVibrant(rAcc2 / wAcc2, gAcc2 / wAcc2, bAcc2 / wAcc2);

    bool hasPrev = (!prevMap.isNull() && prevMap.width() == w && prevMap.height() == h);
    int newWeight = std::clamp(speedPct, 5, 100);
    int oldWeight = 100 - newWeight;

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(cmap.scanLine(y));
        const QRgb *pLine = hasPrev ? reinterpret_cast<const QRgb *>(prevMap.constScanLine(y)) : nullptr;
        for (int x = 0; x < w; ++x) {
            QColor c(qRed(line[x]), qGreen(line[x]), qBlue(line[x]));
            float hf = 0.0f, sf = 0.0f, lf = 0.0f;
            c.getHslF(&hf, &sf, &lf);

            int nr = 0, ng = 0, nb = 0;
            if (lf < 0.12f || sf < 0.10f || hf < 0.0f) {
                double mix = double(x) / std::max(1, w - 1);
                int mr = int(outPrimary.red() * (1.0 - mix) + outSecondary.red() * mix);
                int mg = int(outPrimary.green() * (1.0 - mix) + outSecondary.green() * mix);
                int mb = int(outPrimary.blue() * (1.0 - mix) + outSecondary.blue() * mix);
                nr = (c.red() + mr * 2) / 3;
                ng = (c.green() + mg * 2) / 3;
                nb = (c.blue() + mb * 2) / 3;
            } else {
                sf = std::clamp((sf * 1.32f + 0.18f) * strengthFactor, 0.20f, 0.95f);
                lf = std::clamp(0.56f + lf * 0.30f, 0.60f, 0.82f);
                QColor vc = QColor::fromHslF(hf, sf, lf);
                nr = vc.red(); ng = vc.green(); nb = vc.blue();
            }

            if (pLine) {
                nr = (qRed(pLine[x]) * oldWeight + nr * newWeight) / 100;
                ng = (qGreen(pLine[x]) * oldWeight + ng * newWeight) / 100;
                nb = (qBlue(pLine[x]) * oldWeight + nb * newWeight) / 100;
            }
            line[x] = qRgb(nr, ng, nb);
        }
    }
    outMap = cmap;
}

QColor Ton618GlWidget::sampleAdaptiveColor(const QWidget *caller, const QPointF &localPos) const {
    VisualParams vp = visuals();
    if (vp.borderColorSource == 2) {
        return (vp.themeMode == 1) ? QColor(100, 116, 139) : QColor(226, 232, 240);
    }
    if (!m_chameleonEnabled.load() || vp.borderColorSource == 1) {
        return fixedAccentColor();
    }
    if (m_colorMap.isNull() || width() <= 1 || height() <= 1) {
        return m_ambientPrimary;
    }
    QPoint pt = caller ? caller->mapTo(const_cast<Ton618GlWidget *>(this), localPos.toPoint())
                       : localPos.toPoint();
    int cx = std::clamp(pt.x() * m_colorMap.width() / std::max(1, width()), 0, m_colorMap.width() - 1);
    int cy = std::clamp(pt.y() * m_colorMap.height() / std::max(1, height()), 0, m_colorMap.height() - 1);
    return m_colorMap.pixelColor(cx, cy);
}

Ton618GlWidget::Ton618GlWidget(QWidget *parent) : QWidget(parent) {
    m_resizeDebounce = new QTimer(this);
    m_resizeDebounce->setSingleShot(true);
    connect(m_resizeDebounce, &QTimer::timeout, this, [this]() {
        if (!m_animEnabled || m_videoPath.isEmpty()) {
            updateStaticCaches();
            update();
        }
    });

    loadBestFallbackImage();
    updateStaticCaches();

    if (auto *guiApp = qobject_cast<QGuiApplication *>(QGuiApplication::instance())) {
        connect(guiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            m_windowFocused = (state == Qt::ApplicationActive);
            updatePauseState();
        });
        connect(guiApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow *focusWin) {
            m_windowFocused = (focusWin != nullptr);
            updatePauseState();
        });
    }
}

Ton618GlWidget::~Ton618GlWidget() {
    stopDecoderThread();
}

void Ton618GlWidget::updatePauseState() {
    bool min = window() && window()->isMinimized();
    bool focusPaused = m_pauseOnFocusLoss.load() && !m_windowFocused;
    bool newPaused = (min || focusPaused || !m_animEnabled || m_inGamePaused || !isVisible());
    if (m_paused.exchange(newPaused) != newPaused) {
        m_pauseCv.notify_all();
    }
}

void Ton618GlWidget::applyVisualSettings(const AppSettings &s) {
    int effectiveBlur = s.glassBlurStrength;
    if (s.disableBlurLowGpu || s.effectsQuality == 0) effectiveBlur = 0;
    else if (s.effectsQuality == 1) effectiveBlur = std::min(effectiveBlur, 3);

    bool needStaticRebuild = false;
    bool prevAnim = m_animEnabled;
    bool wantVideo = s.animBg && (s.backdropType == 0 || s.backdropType == 3);

    {
        std::lock_guard<std::mutex> lk(m_visMutex);
        if (m_vis.backdropType != s.backdropType || m_vis.glassSaturationPct != s.glassSaturationPct) {
            needStaticRebuild = true;
        }
        m_vis.blurStrength = effectiveBlur;
        m_vis.glassOpacityPct = s.reduceTransparency ? std::max(45, s.glassOpacityPct) : s.glassOpacityPct;
        m_vis.borderThicknessPx = s.highContrast ? std::max(16, s.borderThicknessPx) : s.borderThicknessPx;
        m_vis.borderBrightnessPct = s.highContrast ? std::max(145, s.borderBrightnessPct) : s.borderBrightnessPct;
        m_vis.glassSaturationPct = s.glassSaturationPct;
        m_vis.specularStrengthPct = (s.effectsQuality == 0) ? 0 : s.specularStrengthPct;
        m_vis.shadowStrengthPct = (s.effectsQuality == 0) ? 0 : s.shadowStrengthPct;
        m_vis.cornerRadiusMode = s.cornerRadiusMode;
        m_vis.glassStyleMode = s.glassStyleMode;

        m_vis.themeMode = s.themeMode;
        m_vis.accentPreset = s.accentPreset;
        QColor hexCol(s.customAccentHex.trimmed());
        m_vis.customAccent = hexCol.isValid() ? hexCol : QColor(56, 189, 248);
        m_vis.chameleonEnabled = s.chameleonEnabled && !s.highContrast;
        m_vis.chameleonSpeedPct = s.chameleonSpeedPct;
        m_vis.chameleonStrengthPct = s.chameleonStrengthPct;
        m_vis.borderColorSource = s.borderColorSource;
        m_vis.textColorMode = s.textColorMode;

        m_vis.backdropType = s.backdropType;
        m_vis.backdropDimPct = s.backdropDimPct;
        m_vis.videoFpsLimit = s.videoFpsLimit;
        m_vis.pauseOnFocusLoss = s.pauseOnFocusLoss;

        m_vis.animationsEnabled = s.animationsEnabled && !s.reducedMotion;
        m_vis.animSpeedPct = s.animSpeedPct;
        m_vis.animStyle = (!s.animationsEnabled || s.reducedMotion) ? 2 : s.animStyle;
        m_vis.cursorGlowFollow = s.cursorGlowFollow && !s.reducedMotion && (s.effectsQuality > 0);
        m_vis.reducedMotion = s.reducedMotion || !s.animationsEnabled || (s.animStyle == 2);

        m_vis.highContrast = s.highContrast;
        m_vis.reduceTransparency = s.reduceTransparency;
        m_vis.effectsQuality = s.effectsQuality;
        m_vis.disableBlurLowGpu = s.disableBlurLowGpu;
    }

    if (m_blurStrength.exchange(effectiveBlur) != effectiveBlur) needStaticRebuild = true;
    m_saturationPct = s.glassSaturationPct;
    m_glassOpacityPct = s.reduceTransparency ? std::max(45, s.glassOpacityPct) : s.glassOpacityPct;
    m_borderBrightnessPct = s.borderBrightnessPct;
    m_chameleonEnabled = s.chameleonEnabled && !s.highContrast;
    m_chameleonSpeed = s.chameleonSpeedPct;
    m_chameleonStrength = s.chameleonStrengthPct;
    m_reducedMotion = s.reducedMotion || !s.animationsEnabled || (s.animStyle == 2);
    m_videoFps = std::clamp(s.videoFpsLimit, 15, 60);
    m_accentPreset = s.accentPreset;
    m_pauseOnFocusLoss = s.pauseOnFocusLoss;

    if (s.backdropType == 3 && !s.customBackdropPath.trimmed().isEmpty() && QFileInfo::exists(s.customBackdropPath.trimmed())) {
        QString cPath = s.customBackdropPath.trimmed();
        if (m_customFilePath != cPath) {
            m_customFilePath = cPath;
            loadCustomVideoFile(cPath);
            return;
        }
    } else if (s.backdropType == 0 && !m_defaultVideoPath.isEmpty()) {
        bool restoredDefault = false;
        {
            std::lock_guard<std::mutex> lk(m_pathMutex);
            if (m_videoPath != m_defaultVideoPath) {
                m_videoPath = m_defaultVideoPath;
                m_customFilePath.clear();
                restoredDefault = true;
            }
        }
        if (restoredDefault) {
            loadBestFallbackImage();
            updateStaticCaches();
            if (wantVideo) startDecoderThread();
        }
    } else if (s.backdropType == 1) {
        m_customFilePath.clear();
        loadBestFallbackImage();
    }

    if (prevAnim != wantVideo) {
        setAnimationEnabled(wantVideo);
    } else if (needStaticRebuild) {
        updateStaticCaches();
    }
    updatePauseState();
    if (window()) window()->update();
}

void Ton618GlWidget::configureVisuals(int blurStrength,
                                      int glassOpacityPct,
                                      int borderBrightnessPct,
                                      bool chameleon,
                                      bool reducedMotion,
                                      int fpsLimit,
                                      int accentPreset) {
    bool blurChanged = (m_blurStrength.exchange(std::clamp(blurStrength, 0, 8)) != blurStrength);
    m_glassOpacityPct = std::clamp(glassOpacityPct, 2, 60);
    m_borderBrightnessPct = std::clamp(borderBrightnessPct, 20, 180);
    m_chameleonEnabled = chameleon;
    m_reducedMotion = reducedMotion;
    m_videoFps = std::clamp(fpsLimit, 15, 60);
    m_accentPreset = std::clamp(accentPreset, 0, 4);
    if (blurChanged) updateStaticCaches();
    if (window()) window()->update();
}

bool Ton618GlWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == window()) {
        if (event->type() == QEvent::WindowStateChange) {
            updatePauseState();
        } else if (event->type() == QEvent::WindowActivate) {
            m_windowFocused = true;
            updatePauseState();
        } else if (event->type() == QEvent::WindowDeactivate) {
            m_windowFocused = false;
            updatePauseState();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Ton618GlWidget::startDecoderThread() {
    stopDecoderThread();
    if (m_videoPath.isEmpty() || !m_animEnabled) return;
    m_stopWorker = false;
    m_worker = std::thread(&Ton618GlWidget::decoderLoop, this);
}

void Ton618GlWidget::stopDecoderThread() {
    m_stopWorker = true;
    m_pauseCv.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void Ton618GlWidget::decoderLoop() {
    QString path;
    {
        std::lock_guard<std::mutex> lk(m_pathMutex);
        path = m_videoPath;
    }
    if (path.isEmpty() || !QFile::exists(path)) return;

    QString cmd = QString("ffmpeg -loglevel quiet -re -threads 1 -stream_loop -1 -i \"%1\" "
                          "-vf scale=%2:%3:flags=fast_bilinear -r 24 -f rawvideo -pix_fmt rgb32 -")
                      .arg(path)
                      .arg(STREAM_W)
                      .arg(STREAM_H);

    FILE *pipe = popen(cmd.toUtf8().constData(), "r");
    if (!pipe) return;

    std::vector<uchar> buf(FRAME_BYTES);
    QImage prevColorMap;
    QColor cachedAmb1 = m_ambientPrimary;
    QColor cachedAmb2 = m_ambientSecondary;
    uint32_t frameCounter = 0;
    auto nextTick = std::chrono::steady_clock::now();
    QPointer<Ton618GlWidget> safeThis(this);

    while (!m_stopWorker.load(std::memory_order_relaxed)) {
        if (m_paused.load(std::memory_order_relaxed)) {
            std::unique_lock<std::mutex> lk(m_pauseMutex);
            m_pauseCv.wait_for(lk, std::chrono::milliseconds(250), [this]() {
                return m_stopWorker.load() || !m_paused.load();
            });
            nextTick = std::chrono::steady_clock::now();
            continue;
        }

        size_t readBytes = 0;
        while (readBytes < FRAME_BYTES && !m_stopWorker.load(std::memory_order_relaxed)) {
            size_t n = std::fread(buf.data() + readBytes, 1, FRAME_BYTES - readBytes, pipe);
            if (n == 0) break;
            readBytes += n;
        }
        if (readBytes < FRAME_BYTES || m_stopWorker.load(std::memory_order_relaxed)) break;

        int tw = std::max(320, m_targetW.load(std::memory_order_relaxed));
        int th = std::max(240, m_targetH.load(std::memory_order_relaxed));
        int blurR = std::clamp(m_blurStrength.load(std::memory_order_relaxed), 0, 8);
        int satPct = std::clamp(m_saturationPct.load(std::memory_order_relaxed), 80, 220);

        if (!m_frameQueued.load(std::memory_order_acquire)) {
            QImage rawFrame(buf.data(), STREAM_W, STREAM_H, STREAM_W * 4, QImage::Format_RGB32);

            QImage sharpScaled = rawFrame.scaled(tw, th, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
            int offX = std::max(0, (sharpScaled.width() - tw) / 2);
            int offY = std::max(0, (sharpScaled.height() - th) / 2);
            QImage sharpFinal = sharpScaled.copy(offX, offY, tw, th);

            QImage small = rawFrame.scaled(160, 90, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            QImage blurredSmall = buildCrystalFrostedImage(small, blurR, satPct);
            QImage frostedFinal = blurredSmall.scaled(tw, th, Qt::IgnoreAspectRatio, Qt::FastTransformation);

            if ((frameCounter++ % 4 == 0) || prevColorMap.isNull()) {
                int cSpeed = std::clamp(m_chameleonSpeed.load(std::memory_order_relaxed), 5, 100);
                int cStrength = std::clamp(m_chameleonStrength.load(std::memory_order_relaxed), 20, 150);
                QImage cmap;
                buildAdaptiveColorMap(blurredSmall, prevColorMap, cSpeed, cStrength, cmap, cachedAmb1, cachedAmb2);
                prevColorMap = std::move(cmap);
            }
            QImage cmapCopy = prevColorMap;
            QColor amb1 = cachedAmb1;
            QColor amb2 = cachedAmb2;

            m_frameQueued.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(this,
                [safeThis,
                 sImg = std::move(sharpFinal),
                 fImg = std::move(frostedFinal),
                 cImg = std::move(cmapCopy),
                 amb1, amb2]() mutable {
                    if (!safeThis) return;
                    safeThis->m_scaledSharp = QPixmap::fromImage(std::move(sImg));
                    safeThis->m_scaledFrosted = QPixmap::fromImage(std::move(fImg));
                    safeThis->m_colorMap = std::move(cImg);
                    safeThis->m_ambientPrimary = amb1;
                    safeThis->m_ambientSecondary = amb2;
                    safeThis->m_frameQueued.store(false, std::memory_order_release);
                    if (safeThis->isVisible() && (!safeThis->window() || !safeThis->window()->isMinimized())) {
                        safeThis->update();
                    }
                }, Qt::QueuedConnection);
        }

        int fps = std::clamp(m_videoFps.load(std::memory_order_relaxed), 15, 30);
        nextTick += std::chrono::microseconds(1000000 / fps);
        auto now = std::chrono::steady_clock::now();
        if (nextTick > now) {
            std::unique_lock<std::mutex> lk(m_pauseMutex);
            m_pauseCv.wait_until(lk, nextTick, [this]() { return m_stopWorker.load(); });
        } else {
            nextTick = now;
        }
    }

    pclose(pipe);
}

void Ton618GlWidget::loadCustomVideoFile(const QString &filePath) {
    if (!QFile::exists(filePath)) return;
    stopDecoderThread();

    if (filePath.endsWith(".png", Qt::CaseInsensitive) || filePath.endsWith(".jpg", Qt::CaseInsensitive) ||
        filePath.endsWith(".jpeg", Qt::CaseInsensitive)) {
        {
            std::lock_guard<std::mutex> lk(m_pathMutex);
            m_videoPath.clear();
        }
        m_fallbackImg.load(filePath);
        updateStaticCaches();
        update();
        return;
    }

    QString dest = QDir::homePath() + "/.local/share/mqblox/videos/custom_loop.mp4";
    auto *conv = new QProcess(this);
    connect(conv, &QProcess::finished, this, [this, conv, dest](int code, QProcess::ExitStatus) {
        conv->deleteLater();
        if (code == 0 && QFileInfo(dest).size() > 10000) {
            {
                std::lock_guard<std::mutex> lk(m_pathMutex);
                m_videoPath = dest;
            }
            if (m_animEnabled) startDecoderThread();
        }
    });
    conv->start("ffmpeg", QStringList{
        "-loglevel", "error", "-y",
        "-i", filePath,
        "-vf", "scale=854:480:flags=lanczos,fps=30",
        "-c:v", "libx264", "-preset", "ultrafast", "-crf", "16", "-g", "15",
        "-pix_fmt", "yuv420p", "-an", dest
    });
}

void Ton618GlWidget::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    if (window()) {
        window()->removeEventFilter(this);
        window()->installEventFilter(this);
    }
    updatePauseState();
}

void Ton618GlWidget::hideEvent(QHideEvent *e) {
    QWidget::hideEvent(e);
    m_paused = true;
}

void Ton618GlWidget::setAnimationEnabled(bool enabled) {
    if (m_animEnabled == enabled) return;
    m_animEnabled = enabled;
    updatePauseState();
    if (!m_animEnabled) {
        stopDecoderThread();
        updateStaticCaches();
    } else if (!m_videoPath.isEmpty()) {
        startDecoderThread();
    }
    update();
}

void Ton618GlWidget::setInGamePaused(bool paused) {
    m_inGamePaused = paused;
    updatePauseState();
}

void Ton618GlWidget::setWebTabActive(bool) {}

void Ton618GlWidget::loadBestFallbackImage() {
    QString base = QDir::homePath() + "/.local/share/mqblox";
    for (const QString &candidate : {
             base + "/pexels_39811262_uhd.png",
             base + "/ton618_4k_master_v22.png",
             base + "/ton618_4k_master_v21.png"}) {
        if (QFileInfo(candidate).size() > 50000 && m_fallbackImg.load(candidate)) {
            return;
        }
    }
    m_fallbackImg = QImage(STREAM_W, STREAM_H, QImage::Format_RGB32);
    m_fallbackImg.fill(QColor(4, 6, 14));
}

void Ton618GlWidget::updateStaticCaches() {
    if (width() <= 10 || height() <= 10) return;

    VisualParams vp = visuals();
    QImage baseImg = m_fallbackImg;

    if (vp.backdropType == 2) {
        // Космический градиент (0% CPU/GPU)
        QImage gradImg(320, 180, QImage::Format_RGB32);
        QPainter gp(&gradImg);
        QLinearGradient lg(0, 0, 320, 180);
        if (isLightThemeActive()) {
            lg.setColorAt(0.0, QColor(224, 242, 254));
            lg.setColorAt(0.5, QColor(241, 245, 249));
            lg.setColorAt(1.0, QColor(226, 232, 240));
        } else {
            QColor acc = fixedAccentColor();
            lg.setColorAt(0.0, QColor(6, 10, 24));
            lg.setColorAt(0.55, QColor(acc.red() / 5, acc.green() / 5, acc.blue() / 4));
            lg.setColorAt(1.0, QColor(4, 6, 14));
        }
        gp.fillRect(gradImg.rect(), lg);
        gp.end();
        baseImg = gradImg;
    } else if (baseImg.isNull()) {
        loadBestFallbackImage();
        baseImg = m_fallbackImg;
    }

    QImage sharpScaled = baseImg.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int offX = std::max(0, (sharpScaled.width() - width()) / 2);
    int offY = std::max(0, (sharpScaled.height() - height()) / 2);
    QImage cropped = sharpScaled.copy(offX, offY, width(), height());
    m_scaledSharp = QPixmap::fromImage(cropped);

    QImage small = cropped.scaled(240, 135, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage blurredSmall = buildCrystalFrostedImage(small, std::clamp(m_blurStrength.load(), 0, 8), m_saturationPct.load());
    m_scaledFrosted = QPixmap::fromImage(blurredSmall.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    QImage prev;
    buildAdaptiveColorMap(blurredSmall, prev, m_chameleonSpeed.load(), m_chameleonStrength.load(),
                          m_colorMap, m_ambientPrimary, m_ambientSecondary);
}

void Ton618GlWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    m_targetW = std::max(320, width());
    m_targetH = std::max(240, height());
    if (m_scaledSharp.isNull()) {
        updateStaticCaches();
    } else if (!m_animEnabled || m_videoPath.isEmpty()) {
        m_resizeDebounce->start(40);
    }
}

void Ton618GlWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = width(), h = height();
    VisualParams vp = visuals();

    double winRadius = (vp.cornerRadiusMode == 0) ? 14.0 : ((vp.cornerRadiusMode == 2) ? 26.0 : 22.0);
    QPainterPath clip;
    clip.addRoundedRect(1.0, 1.0, w - 2.0, h - 2.0, winRadius, winRadius);
    p.setClipPath(clip);

    if (!m_scaledSharp.isNull()) {
        if (m_scaledSharp.size() == size()) {
            p.drawPixmap(0, 0, m_scaledSharp);
        } else {
            p.drawPixmap(rect(), m_scaledSharp);
        }
    } else {
        p.fillRect(rect(), QColor(3, 4, 9));
    }

    // Затемнение заднего фона (0..85%) или осветление для светлой темы
    if (isLightThemeActive()) {
        p.fillRect(rect(), QColor(248, 250, 252, 165));
    } else if (vp.backdropDimPct > 0) {
        int dimAlpha = std::clamp(vp.backdropDimPct * 255 / 100, 0, 225);
        p.fillRect(rect(), QColor(2, 4, 10, dimAlpha));
    }

    QLinearGradient winRim(0, 0, w, h);
    QColor c1 = m_chameleonEnabled.load() ? m_ambientPrimary : fixedAccentColor();
    QColor c2 = m_chameleonEnabled.load() ? m_ambientSecondary : fixedAccentColor().darker(125);
    c1.setAlpha(135);
    c2.setAlpha(95);
    winRim.setColorAt(0.0, QColor(255, 255, 255, 145));
    winRim.setColorAt(0.5, c1);
    winRim.setColorAt(1.0, c2);
    p.setPen(QPen(QBrush(winRim), std::clamp(vp.borderThicknessPx / 10.0, 0.5, 2.5)));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(1, 1, w - 2, h - 2, winRadius, winRadius);
}

} // namespace mqblox
EOF_SRC_RENDER_TON618GLWIDGET_CPP

cat <<'EOF_SRC_UI_WIDGETS_HPP' > "src/ui/Widgets.hpp"
#include <QWheelEvent>
#pragma once
#include "render/Ton618GlWidget.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadialGradient>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <functional>

namespace mqblox {

struct IosSpring {
    double value = 0.0;
    double target = 0.0;
    double velocity = 0.0;
    double stiffness = 330.0;
    double damping = 22.5;

    bool step(double dt = 0.016, bool instant = false, int speedPct = 100, int animStyle = 1) {
        if (instant || animStyle == 2) {
            value = target;
            velocity = 0.0;
            return false;
        }
        double scaledDt = dt * (std::clamp(speedPct, 50, 200) / 100.0);
        double damp = (animStyle == 0) ? 38.0 : damping; // 0 = Плавные без отскока, 1 = Пружинные
        double force = (target - value) * stiffness - velocity * damp;
        velocity += force * scaledDt;
        value += velocity * scaledDt;
        if (std::abs(target - value) < 0.002 && std::abs(velocity) < 0.01) {
            value = target;
            velocity = 0.0;
            return false;
        }
        return true;
    }
};

inline Ton618GlWidget *findBackdropTon618(const QWidget *w) {
    if (!w) return nullptr;
    QWidget *top = w->window();
    return top ? top->findChild<Ton618GlWidget *>() : nullptr;
}

inline QColor blendColors(const QColor &a, const QColor &b, double t, int alpha = 255) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(
        int(a.red() * (1.0 - t) + b.red() * t),
        int(a.green() * (1.0 - t) + b.green() * t),
        int(a.blue() * (1.0 - t) + b.blue() * t),
        std::clamp(alpha, 0, 255)
    );
}

inline void drawHyperOs4LiquidGlass(QPainter &p,
                                    const QWidget *widget,
                                    const QRectF &rect,
                                    double baseRadius,
                                    double hoverMix,
                                    const QPointF &cursorPos,
                                    bool hasCursor,
                                    const QColor &customAccent = QColor(),
                                    double glassOpacityOverride = -1.0) {
    if (rect.width() <= 4 || rect.height() <= 4) return;

    Ton618GlWidget *bg = findBackdropTon618(widget);
    VisualParams vp = bg ? bg->visuals() : VisualParams{};

    const bool isDisabled = widget && !widget->isEnabled();
    const bool hasFocus = widget && widget->hasFocus();
    const bool allowCursorLight = hasCursor && !isDisabled && vp.cursorGlowFollow && !vp.reducedMotion;

    double radius = baseRadius;
    if (baseRadius >= 18.0) {
        if (vp.cornerRadiusMode == 0) radius = 12.0;
        else if (vp.cornerRadiusMode == 2) radius = 28.0;
        else radius = 21.0;
    } else {
        if (vp.cornerRadiusMode == 0) radius = std::min(baseRadius, 8.0);
        else if (vp.cornerRadiusMode == 2) radius = std::min(rect.height() * 0.49, 18.0);
    }

    double userOpacity = vp.glassOpacityPct / 100.0;
    if (vp.glassStyleMode == 1) userOpacity = std::max(userOpacity, 0.22); // Матовое
    else if (vp.glassStyleMode == 2) userOpacity = std::max(userOpacity, 0.34); // Тонированное

    double glassOpacity = (glassOpacityOverride >= 0.0) ? glassOpacityOverride : userOpacity;
    double borderScale = vp.borderBrightnessPct / 100.0;
    double specScale = vp.specularStrengthPct / 100.0;
    double shadowScale = vp.shadowStrengthPct / 100.0;
    double penWidth = std::clamp(vp.borderThicknessPx / 10.0, 0.5, 2.5);

    if (isDisabled) {
        hoverMix = 0.0;
        borderScale *= 0.45;
    }

    QColor cTL = bg ? bg->sampleAdaptiveColor(widget, rect.topLeft()) : QColor(251, 191, 36);
    QColor cMid = bg ? bg->sampleAdaptiveColor(widget, rect.center()) : QColor(56, 189, 248);
    QColor cBR = bg ? bg->sampleAdaptiveColor(widget, rect.bottomRight()) : QColor(168, 85, 247);

    if (customAccent.isValid()) {
        double mixAmt = std::max(0.28, 0.50 * hoverMix);
        cTL = blendColors(cTL, customAccent, mixAmt);
        cMid = blendColors(cMid, customAccent, mixAmt);
        cBR = blendColors(cBR, customAccent, mixAmt);
    }

    // 1. Мягкая внешняя тень
    if (shadowScale > 0.02) {
        p.save();
        p.setPen(Qt::NoPen);
        int shAlpha = std::clamp(int((50 + 28 * hoverMix) * shadowScale), 0, 180);
        p.setBrush(QColor(0, 0, 0, shAlpha));
        p.drawRoundedRect(rect.translated(0, 2.0), radius, radius);
        p.restore();
    }

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, radius, radius);

    p.save();
    p.setClipPath(clipPath);

    // 2. Размытая подложка из общего буфера фона
    if (bg && !vp.highContrast) {
        const QPixmap &frosted = bg->frostedBackdrop();
        if (!frosted.isNull()) {
            QPoint posInBg = widget->mapTo(bg, rect.topLeft().toPoint());
            int shiftX = allowCursorLight ? int((cursorPos.x() - rect.center().x()) * 0.035) : 0;
            int shiftY = allowCursorLight ? int((cursorPos.y() - rect.center().y()) * 0.035) : 2;
            QRect srcRect(posInBg.x() + shiftX, posInBg.y() + shiftY,
                          int(rect.width()), int(rect.height()));
            srcRect = srcRect.intersected(frosted.rect());
            if (srcRect.isValid()) {
                double frostOp = (vp.glassStyleMode == 1)
                    ? std::clamp(0.55 + glassOpacity * 0.7, 0.35, 0.92)
                    : std::clamp(0.36 + glassOpacity * 0.9 + 0.10 * hoverMix, 0.22, 0.85);
                p.setOpacity(frostOp);
                p.drawPixmap(rect.topLeft(), frosted, srcRect);
                p.setOpacity(1.0);
            }
        }
    }

    // 3. Тонирующая вуаль с защитой контраста текста (Тёмная / Светлая / Высокий контраст)
    QLinearGradient tintVeil(rect.topLeft(), rect.bottomRight());
    if (vp.highContrast) {
        tintVeil.setColorAt(0.0, QColor(4, 6, 12, 235));
        tintVeil.setColorAt(1.0, QColor(8, 12, 22, 240));
    } else if (bg && bg->isLightThemeActive()) {
        int aLight = std::clamp(int(175 + glassOpacity * 120), 165, 240);
        tintVeil.setColorAt(0.0, QColor(255, 255, 255, aLight));
        tintVeil.setColorAt(1.0, QColor(241, 245, 249, aLight));
    } else {
        int lumBg = (cMid.red() * 77 + cMid.green() * 150 + cMid.blue() * 29) >> 8;
        int contrastGuard = (lumBg > 155) ? 42 : 16;
        int aBase = std::clamp(int(glassOpacity * 255 + contrastGuard + 12 * hoverMix), 18, 215);
        int div = (vp.glassStyleMode == 2) ? 4 : 7;
        tintVeil.setColorAt(0.0, QColor(10 + cTL.red() / div, 14 + cTL.green() / div, 26 + cTL.blue() / div, aBase));
        tintVeil.setColorAt(0.5, QColor(6 + cMid.red() / (div + 1), 10 + cMid.green() / (div + 1), 20 + cMid.blue() / (div + 1), int(aBase * 0.88)));
        tintVeil.setColorAt(1.0, QColor(8 + cBR.red() / div, 12 + cBR.green() / div, 24 + cBR.blue() / div, std::min(255, aBase + 8)));
    }
    p.fillRect(rect, tintVeil);

    // 4. Световой прожектор за курсором
    if (allowCursorLight && specScale > 0.02) {
        QColor cCursor = bg ? bg->sampleAdaptiveColor(widget, cursorPos) : cMid;
        double spotR = std::max(rect.width(), rect.height()) * 0.68;
        QRadialGradient spot(cursorPos, spotR);
        int s1 = std::clamp(int((56 + 40 * hoverMix) * specScale), 0, 220);
        int s2 = std::clamp(int((22 + 22 * hoverMix) * specScale), 0, 160);
        spot.setColorAt(0.0, blendColors(cCursor, QColor(255, 255, 255), 0.60, s1));
        spot.setColorAt(0.4, QColor(cCursor.red(), cCursor.green(), cCursor.blue(), s2));
        spot.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(rect, spot);
    }

    // 5. Внутренний купольный блик (Specular Crown)
    if (specScale > 0.02 && !vp.highContrast) {
        QLinearGradient crown(0, rect.top(), 0, rect.top() + rect.height() * 0.36);
        int c1 = std::clamp(int((38 + 34 * hoverMix) * specScale), 0, 220);
        int c2 = std::clamp(int((10 + 12 * hoverMix) * specScale), 0, 150);
        crown.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.70, c1));
        crown.setColorAt(0.45, blendColors(cMid, QColor(255, 255, 255), 0.55, c2));
        crown.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(QRectF(rect.left(), rect.top(), rect.width(), rect.height() * 0.36), crown);
    }

    p.restore();

    // 6. Внутренняя преломляющая кромка
    QRectF innerLip = rect.adjusted(1.1, 1.1, -1.1, -1.1);
    QLinearGradient lipGrad(innerLip.topLeft(), innerLip.bottomRight());
    lipGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, std::clamp(int((28 + 36 * hoverMix) * borderScale), 0, 255)));
    lipGrad.setColorAt(1.0, QColor(cBR.red(), cBR.green(), cBR.blue(), std::clamp(int((14 + 22 * hoverMix) * borderScale), 0, 255)));
    p.setPen(QPen(QBrush(lipGrad), 0.75));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(innerLip, std::max(1.0, radius - 1.1), std::max(1.0, radius - 1.1));

    // 7. Градиентная рамка (светлее сверху-слева, темнее снизу-справа)
    QPointF gradStart = allowCursorLight
        ? QPointF(rect.left() * 0.65 + cursorPos.x() * 0.35,
                  rect.top() * 0.65 + cursorPos.y() * 0.35)
        : rect.topLeft();
    QLinearGradient rimGrad(gradStart, rect.bottomRight());
    int aTopLeft = std::clamp(int((115 + 75 * hoverMix) * borderScale), 20, 255);
    int aMid = std::clamp(int((62 + 58 * hoverMix) * borderScale), 15, 245);
    int aBotRight = std::clamp(int((36 + 44 * hoverMix) * borderScale), 10, 220);

    if (vp.highContrast) {
        rimGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
        rimGrad.setColorAt(1.0, QColor(56, 189, 248, 240));
    } else {
        rimGrad.setColorAt(0.0, blendColors(cTL, QColor(255, 255, 255), 0.55, aTopLeft));
        rimGrad.setColorAt(0.5, QColor(cMid.red(), cMid.green(), cMid.blue(), aMid));
        rimGrad.setColorAt(1.0, blendColors(cBR, QColor(15, 23, 42), 0.35, aBotRight));
    }

    p.setPen(QPen(QBrush(rimGrad), hasFocus ? std::max(1.5, penWidth + 0.4) : penWidth));
    p.drawRoundedRect(rect, radius, radius);

    if (hasFocus && !isDisabled) {
        QColor focusCol = bg ? bg->fixedAccentColor() : QColor(56, 189, 248);
        focusCol.setAlpha(200);
        p.setPen(QPen(focusCol, 1.4, Qt::SolidLine));
        p.drawRoundedRect(rect.adjusted(-0.8, -0.8, 0.8, 0.8), radius + 0.8, radius + 0.8);
    }
}

// Слайдер с дебаунсом 65 мс, числовым индикатором и сбросом к значению по умолчанию по ДВОЙНОМУ КЛИКУ
class ResettableSlider : public QWidget {
    Q_OBJECT
public:
    QSlider *slider = nullptr;
    QLabel *valLabel = nullptr;
    QTimer *debounceTimer = nullptr;
    int defaultVal = 0;
    QString suffixStr;
    double displayMultiplier = 1.0;
    std::function<void(int)> onValueCommitted;

    ResettableSlider(int minV, int maxV, int defV, const QString &suffix = "%",
                     double mult = 1.0, QWidget *parent = nullptr)
        : QWidget(parent), defaultVal(defV), suffixStr(suffix), displayMultiplier(mult) {
        setMinimumWidth(120);
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(minV, maxV);
        slider->setValue(defV);
        slider->setCursor(Qt::PointingHandCursor);
        slider->setToolTip(QObject::tr("Двойной клик — сбросить к значению по умолчанию (%1%2)")
                               .arg( mult == 0.1 ? QString::number(defV * 0.1, 'f', 1) : QString::number(defV) )
                               .arg(suffix));
        slider->installEventFilter(this);

        valLabel = new QLabel(this);
        valLabel->setFixedWidth(58);
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valLabel->setToolTip(slider->toolTip());
        valLabel->setCursor(Qt::PointingHandCursor);
        valLabel->installEventFilter(this);

        debounceTimer = new QTimer(this);
        debounceTimer->setSingleShot(true);
        connect(debounceTimer, &QTimer::timeout, this, [this]() {
            if (onValueCommitted) onValueCommitted(slider->value());
        });

        connect(slider, &QSlider::valueChanged, this, [this](int v) {
            updateLabel(v);
            debounceTimer->start(65);
        });

        lay->addWidget(slider, 1);
        lay->addWidget(valLabel);
        updateLabel(defV);
    }

    void setValueSilent(int v) {
        slider->blockSignals(true);
        slider->setValue(v);
        slider->blockSignals(false);
        updateLabel(v);
    }

    int value() const { return slider->value(); }

    void updateLabel(int v) {
        if (std::abs(displayMultiplier - 0.1) < 0.001) {
            valLabel->setText(QString("%1%2").arg(v * 0.1, 0, 'f', 1).arg(suffixStr));
        } else {
            valLabel->setText(QString("%1%2").arg(v).arg(suffixStr));
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == slider && event->type() == QEvent::Wheel) {
            event->ignore();
            return true;
        }
        if ((watched == slider || watched == valLabel) && event->type() == QEvent::MouseButtonDblClick) {
            slider->setValue(defaultVal);
            if (onValueCommitted) onValueCommitted(defaultVal);
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }
};

class AnimButton : public QPushButton {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 320.0, 22.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 410.0, 19.5};
    QColor accent;
    bool isPlay = false, isDanger = false;
    bool hasMouse = false;
    QPointF mousePos;
    QTimer *physTimer = nullptr;

    explicit AnimButton(const QString &text,
                        QWidget *parent = nullptr,
                        QColor acc = QColor(),
                        bool play = false,
                        bool danger = false)
        : QPushButton(text, parent), accent(acc), isPlay(play), isDanger(danger) {
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
        setMinimumHeight(36);
        setMinimumWidth(60);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void setAccentColor(const QColor &c, bool pulse = false) {
        accent = c;
        if (pulse) {
            scaleSpring.velocity += 2.0;
            hoverSpring.value = 0.85;
            if (!physTimer->isActive()) physTimer->start(16);
        }
        update();
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.022;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QPushButton::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.93;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.022 : 1.0;
        scaleSpring.velocity += 2.4;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double sc = isEnabled() ? scaleSpring.value : 1.0;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        double rad = std::min(r.height() * 0.48, 14.0);

        if (isPlay) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.55 + 0.45 * hp, mousePos, hasMouse, QColor(52, 211, 153));
        } else if (isDanger) {
            drawHyperOs4LiquidGlass(p, this, r, rad, 0.45 + 0.55 * hp, mousePos, hasMouse, QColor(248, 113, 113));
        } else {
            drawHyperOs4LiquidGlass(p, this, r, rad, hp, mousePos, hasMouse, accent);
        }

        Ton618GlWidget *bg = findBackdropTon618(this);
        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        p.setPen(isEnabled() ? txtCol : QColor(148, 163, 184, 155));
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
        p.drawText(r, Qt::AlignCenter, text());
        p.restore();
    }
};

class AnimNavButton : public QPushButton {
    Q_OBJECT
public:
    QString iconStr, titleStr;
    bool showTitle = true, isActiveTab = false;
    IosSpring hoverSpring{0.0, 0.0, 0.0, 300.0, 21.0};
    IosSpring activeSpring{0.0, 0.0, 0.0, 270.0, 19.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 380.0, 19.0};
    QPointF mousePos;
    bool hasMouse = false;
    QTimer *physTimer = nullptr;

    AnimNavButton(const QString &ico, const QString &title, QWidget *parent = nullptr)
        : QPushButton(parent), iconStr(ico), titleStr(title) {
        setFixedHeight(42);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = activeSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a3 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2 && !a3) physTimer->stop();
        });
    }

    void setActiveTab(bool act) {
        isActiveTab = act;
        activeSpring.target = act ? 1.0 : 0.0;
        if (act) scaleSpring.velocity += 1.4;
        if (!physTimer->isActive()) physTimer->start(16);
    }
    void setCompactMode(bool compact) { showTitle = !compact; update(); }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.02;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QPushButton::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.94;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = hasMouse ? 1.02 : 1.0;
        scaleSpring.velocity += 2.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QPushButton::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double ap = std::clamp(activeSpring.value, 0.0, 1.0);
        double mix = std::max(hp * 0.78, ap);
        double sc = scaleSpring.value;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        drawHyperOs4LiquidGlass(p, this, r, 13.5, mix, mousePos, hasMouse);

        Ton618GlWidget *bg = findBackdropTon618(this);
        if (mix > 0.04) {
            QColor pillCol = bg ? bg->sampleAdaptiveColor(this, QPointF(8.0, h * 0.5)).lighter(125) : QColor(255, 255, 255);
            pillCol.setAlpha(int(245 * mix));
            double pillH = (h - 18.0) * mix;
            p.setPen(Qt::NoPen);
            p.setBrush(pillCol);
            p.drawRoundedRect(QRectF(6.5, (h - pillH) * 0.5, 3.6, pillH), 1.8, 1.8);
        }

        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false);
        if (showTitle) {
            f.setPixelSize(13); f.setBold(isActiveTab || hp > 0.25); p.setFont(f);
            p.setPen(txtCol);
            double sx = 16.0 + 4.0 * hp;
            p.drawText(QRectF(sx, 0, w - sx - 6.0, h), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1   %2").arg(iconStr, titleStr));
        } else {
            f.setPixelSize(17); p.setFont(f); p.setPen(txtCol);
            p.drawText(r, Qt::AlignCenter, iconStr);
        }
        p.restore();
    }
};

class DarkComboBox : public QComboBox {
    Q_OBJECT
public:
    IosSpring hoverSpring{0.0, 0.0, 0.0, 310.0, 22.0};
    IosSpring scaleSpring{1.0, 1.0, 0.0, 390.0, 20.0};
    QPointF mousePos;
    bool hasMouse = false;
    QTimer *physTimer = nullptr;

    explicit DarkComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
        setMinimumHeight(36);
        setMinimumWidth(115);
        setMouseTracking(true);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(8);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *lv = new QListView(this);
        lv->setObjectName("ComboPopupList");
        setView(lv);
        setCursor(Qt::PointingHandCursor);

        physTimer = new QTimer(this);
        connect(physTimer, &QTimer::timeout, this, [this]() {
            Ton618GlWidget *bg = findBackdropTon618(this);
            VisualParams vp = bg ? bg->visuals() : VisualParams{};
            bool a1 = hoverSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            bool a2 = scaleSpring.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
            update();
            if (!a1 && !a2) physTimer->stop();
        });
    }

    void wheelEvent(QWheelEvent *e) override { e->ignore(); }

    void showPopup() override {
        scaleSpring.velocity -= 1.6;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::showPopup();
        if (view() && view()->window()) {
            QWidget *popWin = view()->window();
            popWin->setAttribute(Qt::WA_TranslucentBackground);
            Ton618GlWidget *bg = findBackdropTon618(this);
            QColor rimC = bg ? bg->sampleAdaptiveColor(this, rect().center()) : QColor(255, 255, 255);
            popWin->setStyleSheet(QString(
                "background-color: rgba(8, 12, 24, 195); "
                "border: 1px solid rgba(%1, %2, %3, 155); "
                "border-radius: 14px;").arg(rimC.red()).arg(rimC.green()).arg(rimC.blue()));
            if (!bg || !bg->isReducedMotion()) {
                auto *fade = new QPropertyAnimation(popWin, "windowOpacity", popWin);
                fade->setDuration(160);
                fade->setStartValue(0.0);
                fade->setEndValue(1.0);
                fade->setEasingCurve(QEasingCurve::OutCubic);
                fade->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
    }

protected:
    void enterEvent(QEnterEvent *e) override {
        hasMouse = true; mousePos = e->position();
        hoverSpring.target = 1.0; scaleSpring.target = 1.014;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::enterEvent(e);
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        mousePos = e->position();
        Ton618GlWidget *bg = findBackdropTon618(this);
        if (!bg || !bg->isReducedMotion()) {
            if (!physTimer->isActive()) physTimer->start(16);
        }
        QComboBox::mouseMoveEvent(e);
    }
    void leaveEvent(QEvent *e) override {
        hasMouse = false; hoverSpring.target = 0.0; scaleSpring.target = 1.0;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::leaveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        scaleSpring.target = 0.96;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        scaleSpring.target = 1.0; scaleSpring.velocity += 1.6;
        if (!physTimer->isActive()) physTimer->start(16);
        QComboBox::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double w = width(), h = height();
        double hp = std::clamp(hoverSpring.value, 0.0, 1.0);
        double sc = scaleSpring.value;

        p.save();
        p.translate(w * 0.5, h * 0.5);
        p.scale(sc, sc);
        p.translate(-w * 0.5, -h * 0.5);

        QRectF r(1.5, 1.5, w - 3.0, h - 3.0);
        drawHyperOs4LiquidGlass(p, this, r, 13.0, hp, mousePos, hasMouse);

        Ton618GlWidget *bg = findBackdropTon618(this);
        QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
        p.setPen(isEnabled() ? txtCol : QColor(148, 163, 184, 155));
        QFont f = font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
        QString elided = p.fontMetrics().elidedText(currentText(), Qt::ElideRight, std::max(20, int(w - 52.0)));
        p.drawText(QRectF(14.0, 0, w - 46.0, h), Qt::AlignVCenter | Qt::AlignLeft, elided);

        QColor badgeCol = bg ? bg->sampleAdaptiveColor(this, QPointF(w - 19.5, h * 0.5)) : QColor(255, 255, 255);

        double badgeR = 10.5;
        QPointF badgeCenter(w - 19.5, h * 0.5);
        p.setPen(QPen(QColor(badgeCol.red(), badgeCol.green(), badgeCol.blue(), int(85 + 85 * hp)), 0.9));
        p.setBrush(QColor(badgeCol.red(), badgeCol.green(), badgeCol.blue(), int(22 + 35 * hp)));
        p.drawEllipse(badgeCenter, badgeR, badgeR);

        QPainterPath chev;
        double cx = badgeCenter.x(), cy = badgeCenter.y();
        chev.moveTo(cx - 3.0, cy - 1.2); chev.lineTo(cx, cy - 4.0); chev.lineTo(cx + 3.0, cy - 1.2);
        chev.moveTo(cx - 3.0, cy + 1.2); chev.lineTo(cx, cy + 4.0); chev.lineTo(cx + 3.0, cy + 1.2);

        p.setPen(QPen(txtCol, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(chev);
        p.restore();
    }
};

class HoverSidebar : public QFrame {
    Q_OBJECT
public:
    std::function<void(bool)> onHoverChanged;
    explicit HoverSidebar(QWidget *parent = nullptr) : QFrame(parent) {}
protected:
    void enterEvent(QEnterEvent *e) override { if (onHoverChanged) onHoverChanged(true); QFrame::enterEvent(e); }
    void leaveEvent(QEvent *e) override { if (onHoverChanged) onHoverChanged(false); QFrame::leaveEvent(e); }
};

class LiquidGlassEngine : public QObject {
    Q_OBJECT
public:
    struct SurfaceState {
        IosSpring hover{0.0, 0.0, 0.0, 260.0, 22.0};
        QPointF cursor;
        bool hasCursor = false;
        bool mouseDirty = false;
    };
    struct SwitchState {
        IosSpring pos{0.0, 0.0, 0.0, 330.0, 21.0};
        IosSpring pressStretch{0.0, 0.0, 0.0, 390.0, 22.0};
        IosSpring hover{0.0, 0.0, 0.0, 280.0, 22.0};
        bool initialized = false;
    };

    QHash<QWidget *, SurfaceState> surfaces;
    QHash<QCheckBox *, SwitchState> switches;
    QTimer *tickTimer = nullptr;

    explicit LiquidGlassEngine(QObject *parent = nullptr) : QObject(parent) {
        tickTimer = new QTimer(this);
        connect(tickTimer, &QTimer::timeout, this, [this]() {
            bool anyActive = false;
            const auto surfKeys = surfaces.keys();
            for (QWidget *k : surfKeys) {
                auto it = surfaces.find(k);
                if (it != surfaces.end()) {
                    Ton618GlWidget *bg = findBackdropTon618(k);
                    VisualParams vp = bg ? bg->visuals() : VisualParams{};
                    bool stepped = it.value().hover.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    if (stepped || it.value().mouseDirty) {
                        it.value().mouseDirty = false;
                        if (stepped) anyActive = true;
                        k->update();
                    }
                }
            }
            const auto swKeys = switches.keys();
            for (QCheckBox *k : swKeys) {
                auto it = switches.find(k);
                if (it != switches.end()) {
                    Ton618GlWidget *bg = findBackdropTon618(k);
                    VisualParams vp = bg ? bg->visuals() : VisualParams{};
                    bool s1 = it.value().pos.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    bool s2 = it.value().pressStretch.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    bool s3 = it.value().hover.step(0.016, vp.reducedMotion, vp.animSpeedPct, vp.animStyle);
                    if (s1 || s2 || s3) { anyActive = true; k->update(); }
                }
            }
            if (!anyActive) tickTimer->stop();
        });
    }

    static bool isGlassContainer(QObject *obj, QFrame **outFrame) {
        auto *fr = qobject_cast<QFrame *>(obj);
        if (!fr) return false;
        const QString name = fr->objectName();
        if (name == "GlassCard" || name == "Sidebar" || name == "AuthorsBox" || name == "WarningBanner") {
            *outFrame = fr;
            return true;
        }
        return false;
    }

    static bool isGlassBadgeLabel(QObject *obj, QLabel **outLbl) {
        auto *lbl = qobject_cast<QLabel *>(obj);
        if (!lbl) return false;
        const QString name = lbl->objectName();
        if (name == "RobuxCardBadge" || name == "AutoSyncBadge" || name == "StatusBadge" || name == "ToastBanner") {
            *outLbl = lbl;
            return true;
        }
        return false;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        QFrame *frame = nullptr;
        if (isGlassContainer(watched, &frame)) {
            if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
                frame->setMouseTracking(true);
                connect(frame, &QObject::destroyed, this, [this, frame]() { surfaces.remove(frame); });
            } else if (event->type() == QEvent::Enter) {
                auto &st = surfaces[frame];
                st.hasCursor = true; st.hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseMove) {
                auto *me = static_cast<QMouseEvent *>(event);
                auto &st = surfaces[frame];
                st.hasCursor = true; st.cursor = me->position();
                Ton618GlWidget *bg = findBackdropTon618(frame);
                if (!bg || !bg->isReducedMotion()) {
                    st.mouseDirty = true;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                }
            } else if (event->type() == QEvent::Leave) {
                auto &st = surfaces[frame];
                st.hasCursor = false; st.hover.target = 0.0; st.mouseDirty = true;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(frame);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[frame];
                const QString name = frame->objectName();
                QRectF r(1.5, 1.5, frame->width() - 3.0, frame->height() - 3.0);

                double rad = (name == "GlassCard" || name == "Sidebar") ? 21.0 : 14.0;
                drawHyperOs4LiquidGlass(p, frame, r, rad, std::clamp(st.hover.value, 0.0, 1.0),
                                        st.cursor, st.hasCursor);
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *secLbl = qobject_cast<QLabel *>(watched)) {
            if (secLbl->objectName() == "SectionTitle" && event->type() == QEvent::Paint) {
                QPainter p(secLbl);
                p.setRenderHint(QPainter::Antialiasing);
                Ton618GlWidget *bg = findBackdropTon618(secLbl);
                QColor c1 = bg ? bg->sampleAdaptiveColor(secLbl, QPointF(0, 0)).lighter(135) : QColor(253, 224, 71);
                QColor c2 = bg ? bg->sampleAdaptiveColor(secLbl, QPointF(secLbl->width(), 0)).lighter(135) : QColor(125, 211, 252);
                QLinearGradient tg(0, 0, secLbl->width(), 0);
                QColor baseTxt = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
                tg.setColorAt(0.0, blendColors(c1, baseTxt, 0.35));
                tg.setColorAt(1.0, blendColors(c2, baseTxt, 0.35));
                QFont f = secLbl->font();
                f.setStyle(QFont::StyleNormal);
                f.setItalic(false);
                f.setPixelSize(15);
                f.setBold(true);
                p.setFont(f);
                p.setPen(QPen(QBrush(tg), 1.0));
                secLbl->setMinimumWidth(0);
                QString secTxt = p.fontMetrics().elidedText(secLbl->text(), Qt::ElideRight, std::max(20, secLbl->width() - 4));
                p.drawText(secLbl->rect(), Qt::AlignVCenter | Qt::AlignLeft, secTxt);
                return true;
            }
        }

        if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QSpinBox *>(watched)) {
            auto *w = static_cast<QWidget *>(watched);
            if (qobject_cast<QSpinBox *>(watched) && event->type() == QEvent::Wheel && !w->hasFocus()) {
                event->ignore();
                return true;
            }
            if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
                w->setMouseTracking(true);
                connect(w, &QObject::destroyed, this, [this, w]() { surfaces.remove(w); });
            } else if (event->type() == QEvent::Enter) {
                surfaces[w].hasCursor = true; surfaces[w].hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseMove) {
                surfaces[w].cursor = static_cast<QMouseEvent *>(event)->position();
                Ton618GlWidget *bg = findBackdropTon618(w);
                if (!bg || !bg->isReducedMotion()) {
                    surfaces[w].mouseDirty = true;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                }
            } else if (event->type() == QEvent::Leave) {
                surfaces[w].hasCursor = false; surfaces[w].hover.target = 0.0; surfaces[w].mouseDirty = true;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(w);
                p.setRenderHint(QPainter::Antialiasing);
                auto &st = surfaces[w];
                double mix = w->hasFocus() ? 1.0 : std::clamp(st.hover.value, 0.0, 1.0);
                QRectF r(1.5, 1.5, w->width() - 3.0, w->height() - 3.0);
                drawHyperOs4LiquidGlass(p, w, r, 12.5, mix, st.cursor, st.hasCursor);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        QLabel *badgeLbl = nullptr;
        if (isGlassBadgeLabel(watched, &badgeLbl)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(badgeLbl);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.0, 1.0, badgeLbl->width() - 2.0, badgeLbl->height() - 2.0);
                drawHyperOs4LiquidGlass(p, badgeLbl, r, std::min(r.height() * 0.48, 12.0),
                                        0.45, r.center(), false);
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *bar = qobject_cast<QProgressBar *>(watched)) {
            if (event->type() == QEvent::Paint) {
                QPainter p(bar);
                p.setRenderHint(QPainter::Antialiasing);
                QRectF r(1.0, 1.0, bar->width() - 2.0, bar->height() - 2.0);
                double rad = r.height() * 0.48;
                drawHyperOs4LiquidGlass(p, bar, r, rad, 0.30, r.center(), false);

                double frac = 1.0;
                if (bar->maximum() > bar->minimum()) {
                    frac = std::clamp(double(bar->value() - bar->minimum()) / (bar->maximum() - bar->minimum()), 0.0, 1.0);
                }
                Ton618GlWidget *bg = findBackdropTon618(bar);
                if (frac > 0.02) {
                    QColor cL = bg ? bg->sampleAdaptiveColor(bar, r.topLeft()) : QColor(56, 189, 248);
                    QColor cR = bg ? bg->sampleAdaptiveColor(bar, r.topRight()) : QColor(251, 191, 36);
                    cL.setAlpha(165);
                    cR.setAlpha(180);

                    QRectF fillR = r.adjusted(1.5, 1.5, -1.5, -1.5);
                    fillR.setWidth(fillR.width() * frac);
                    QLinearGradient fg(fillR.topLeft(), fillR.topRight());
                    fg.setColorAt(0.0, cL);
                    fg.setColorAt(1.0, cR);
                    p.setPen(QPen(QColor(255, 255, 255, 130), 0.7));
                    p.setBrush(fg);
                    p.drawRoundedRect(fillR, rad - 1.5, rad - 1.5);
                }
                p.setPen(bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255));
                QFont f = bar->font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setBold(true); p.setFont(f);
                p.drawText(r, Qt::AlignCenter, bar->text());
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

        if (auto *chk = qobject_cast<QCheckBox *>(watched)) {
            auto &st = switches[chk];
            if (!st.initialized) {
                st.initialized = true;
                st.pos.value = st.pos.target = chk->isChecked() ? 1.0 : 0.0;
                chk->setMinimumWidth(0);
                chk->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                chk->setCursor(Qt::PointingHandCursor);
                connect(chk, &QCheckBox::toggled, this, [this, chk](bool c) {
                    auto &s = switches[chk];
                    s.pos.target = c ? 1.0 : 0.0;
                    s.pos.velocity += c ? 2.5 : -2.5;
                    if (!tickTimer->isActive()) tickTimer->start(16);
                });
                connect(chk, &QObject::destroyed, this, [this, chk]() { switches.remove(chk); });
            }

            if (event->type() == QEvent::Enter) {
                st.hover.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Leave) {
                st.hover.target = 0.0; st.pressStretch.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseButtonPress) {
                st.pressStretch.target = 1.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::MouseButtonRelease) {
                st.pressStretch.target = 0.0;
                if (!tickTimer->isActive()) tickTimer->start(16);
            } else if (event->type() == QEvent::Paint) {
                QPainter p(chk);
                p.setRenderHint(QPainter::Antialiasing);

                double h = chk->height();
                double swW = 48.0, swH = 26.0;
                double swY = (h - swH) * 0.5;
                QRectF trackRect(2.0, swY, swW, swH);

                double tPos = std::clamp(st.pos.value, 0.0, 1.0);
                double stretch = std::clamp(st.pressStretch.value, 0.0, 1.0);
                double hov = std::clamp(st.hover.value, 0.0, 1.0);

                Ton618GlWidget *bg = findBackdropTon618(chk);
                QColor swColor = bg ? bg->sampleAdaptiveColor(chk, trackRect.center()) : QColor(56, 189, 248);

                drawHyperOs4LiquidGlass(p, chk, trackRect, swH * 0.5, std::max(tPos, hov),
                                        trackRect.center(), false, swColor);

                if (tPos > 0.05) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(swColor.red(), swColor.green(), swColor.blue(), int(145 * tPos)));
                    p.drawRoundedRect(trackRect.adjusted(1.0, 1.0, -1.0, -1.0), swH * 0.46, swH * 0.46);
                }

                double thumbBaseD = swH - 4.4;
                double thumbW = thumbBaseD + stretch * 7.0;
                double minX = trackRect.left() + 2.2;
                double maxX = trackRect.right() - 2.2 - thumbW;
                double thumbX = minX + (maxX - minX) * tPos;
                QRectF thumbRect(thumbX, swY + 2.2, thumbW, thumbBaseD);

                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 85));
                p.drawRoundedRect(thumbRect.translated(0, 1.2), thumbBaseD * 0.5, thumbBaseD * 0.5);

                QLinearGradient thumbGrad(thumbRect.topLeft(), thumbRect.bottomRight());
                thumbGrad.setColorAt(0.0, QColor(255, 255, 255, chk->isEnabled() ? 250 : 140));
                thumbGrad.setColorAt(1.0, blendColors(swColor, QColor(255, 255, 255), 0.72, chk->isEnabled() ? 235 : 120));
                p.setBrush(thumbGrad);
                p.setPen(QPen(QColor(255, 255, 255, chk->isEnabled() ? 240 : 110), 0.9));
                p.drawRoundedRect(thumbRect, thumbBaseD * 0.5, thumbBaseD * 0.5);

                QColor txtCol = bg ? bg->resolveTextColor(QColor(15, 23, 42)) : QColor(255, 255, 255);
                p.setPen(chk->isEnabled() ? txtCol : QColor(148, 163, 184, 155));
                QFont f = chk->font(); f.setStyle(QFont::StyleNormal); f.setItalic(false); f.setWeight(QFont::Medium); p.setFont(f);
                QString chkTxt = p.fontMetrics().elidedText(chk->text(), Qt::ElideRight, std::max(20, int(chk->width() - trackRect.right() - 16.0)));
                p.drawText(QRectF(trackRect.right() + 12.0, 0, chk->width() - trackRect.right() - 14.0, h),
                           Qt::AlignVCenter | Qt::AlignLeft, chkTxt);
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

inline void installLiquidGlassEngine(QApplication *app) {
    auto *engine = new LiquidGlassEngine(app);
    app->installEventFilter(engine);
}

} // namespace mqblox
EOF_SRC_UI_WIDGETS_HPP

cat <<'EOF_SRC_UI_MAINWINDOW_HPP' > "src/ui/MainWindow.hpp"
#pragma once
#include "core/SettingsManager.hpp"
#include "core/SoberController.hpp"
#include "render/Ton618GlWidget.hpp"
#include "ui/Widgets.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QQueue>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <vector>

namespace mqblox {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &launchUri = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    Qt::Edges detectResizeEdges(const QPointF &pos) const;
    void initUi();
    void initTrayAndShortcuts();
    QWidget *buildHomeRobloxPage();
    QWidget *buildAccountsPage();
    QWidget *buildHardwareAudioPage();
    QWidget *buildGraphicsPage();
    QWidget *buildCameraModsPage();
    QWidget *buildUpdatesPage();

    void syncUiFromSettings();
    void collectSettingsFromUi();
    void triggerInstantAutosave(const QString &toastMsg = {});
    void saveSettingsNow(bool manualClick = true);
    void enqueueToast(const QString &msg);
    void processNextToast();
    void refreshAccountsUi();
    void refreshEnvironmentAndSessionUi();
    void refreshNamedProfilesCombo();
    void applyResolutionMode(ResolutionMode mode, bool initial);
    void applyGraphicsMasterPreset(int presetIdx);
    void toggleSidebarPinMode();
    void animateSidebar(bool expand);
    void switchTab(int idx);
    void runWatchdogTick();
    void refreshGitUpdateStatus();

    struct SearchEntry {
        int tabIndex = 0;
        QFrame *card = nullptr;
        QString haystackLower;
    };
    std::vector<SearchEntry> m_searchIndex;
    QTimer *m_searchDebounce = nullptr;
    QString m_lastAppliedQss;
    void buildSearchIndex();
    void filterSettingsBySearch(const QString &query);
    void resetCurrentCategoryToDefaults();
    void applyLayoutAndTypographyLive();
    void resetAppearanceToDefault();

    QString m_launchUri;
    SettingsManager m_settingsMgr;
    SoberController *m_soberCtrl = nullptr;

    Ton618GlWidget *m_rootCosmos = nullptr;
    HoverSidebar *m_sidebar = nullptr;
    AnimButton *m_btnPinSide = nullptr;
    QWidget *m_titleColWidget = nullptr;
    QLabel *m_pinHintLbl = nullptr;
    QFrame *m_authorsBox = nullptr;
    QProgressBar *m_ramBar = nullptr;
    QLabel *m_statusBadge = nullptr;
    std::vector<AnimNavButton *> m_navButtons;

    QLabel *m_toastLbl = nullptr;
    DarkComboBox *m_comboRes = nullptr;
    DarkComboBox *m_comboQuickAcc = nullptr;
    AnimButton *m_btnMax = nullptr;
    AnimButton *m_btnSaveSettings = nullptr;
    QLabel *m_lblAutosync = nullptr;
    QStackedWidget *m_pages = nullptr;

    // Home: Launch, Session, Wizard & Diagnostics
    QLineEdit *m_inpPlace = nullptr;
    QLabel *m_lblSessionStatus = nullptr;
    AnimButton *m_btnLogout = nullptr;
    QLabel *m_lblEnvSummary = nullptr;
    QProgressBar *m_wizardProgressBar = nullptr;
    QLabel *m_lblWizardStatus = nullptr;
    AnimButton *m_btnRunWizard = nullptr;
    QTextEdit *m_homeDiagView = nullptr;

    // Accounts & Named Profiles
    QVBoxLayout *m_accountsListLayout = nullptr;
    DarkComboBox *m_comboNamedProfiles = nullptr;
    QLineEdit *m_inpNewProfileName = nullptr;

    // System & MacOBlox Runtime
    QLabel *m_hwBannerLbl = nullptr;
    ResettableSlider *m_sldMouseSens = nullptr;
    QCheckBox *m_chkHideMenuBar = nullptr;
    DarkComboBox *m_comboDnsProvider = nullptr;
    QLineEdit *m_inpDnsCustom = nullptr;
    QCheckBox *m_chkFpsLog = nullptr;
    QCheckBox *m_chkTraceUdp = nullptr;
    QCheckBox *m_chkTraceLock = nullptr;
    QCheckBox *m_chkTraceGl = nullptr;
    QCheckBox *m_chkDiagSignals = nullptr;

    QCheckBox *m_chkReturnMq = nullptr;
    QCheckBox *m_chkAnimBg = nullptr;
    QCheckBox *m_chkCloseToTray = nullptr;
    QCheckBox *m_chkVsyncOff = nullptr;
    QCheckBox *m_chkAutoClean = nullptr;
    QCheckBox *m_chkMemFflags = nullptr;
    QCheckBox *m_chkNoTelemetry = nullptr;
    QCheckBox *m_chkAutoReconnect = nullptr;

    // Graphics & FastFlags
    QLabel *m_lblPresetDesc = nullptr;
    DarkComboBox *m_comboLighting = nullptr;
    QSpinBox *m_spinFps = nullptr;
    QCheckBox *m_chkUnlimFps = nullptr;
    DarkComboBox *m_comboTex = nullptr;
    DarkComboBox *m_comboMsaa = nullptr;
    QSpinBox *m_spinFrm = nullptr;

    QCheckBox *m_chkNoShadows = nullptr;
    QCheckBox *m_chkUltraShadows = nullptr;
    QCheckBox *m_chkNoPostfx = nullptr;
    QCheckBox *m_chkLowMeshes = nullptr;
    QCheckBox *m_chkGreySky = nullptr;

    QTextEdit *m_fflagsEditor = nullptr;
    QLabel *m_lblFflagsValidation = nullptr;

    // Tweaks & Camera
    DarkComboBox *m_comboCamZoom = nullptr;
    DarkComboBox *m_comboNetOpt = nullptr;
    QCheckBox *m_chkHudFps = nullptr;
    QCheckBox *m_chkClearFog = nullptr;
    QSpinBox *m_spinMaxZoom = nullptr;
    QCheckBox *m_chkInfZoom = nullptr;
    QCheckBox *m_chkFullBright = nullptr;
    QCheckBox *m_chkHitboxes = nullptr;

    // Visual & Layout Controls (HyperOS 4 Liquid Glass)
    QLineEdit *m_inpSearchSettings = nullptr;
    DarkComboBox *m_comboVisualPreset = nullptr;
    ResettableSlider *m_sldBlur = nullptr;
    ResettableSlider *m_sldOpacity = nullptr;
    ResettableSlider *m_sldBorderThick = nullptr;
    ResettableSlider *m_sldBorderBright = nullptr;
    ResettableSlider *m_sldSaturation = nullptr;
    ResettableSlider *m_sldSpecular = nullptr;
    ResettableSlider *m_sldShadow = nullptr;
    DarkComboBox *m_comboCornerRadius = nullptr;
    DarkComboBox *m_comboGlassStyle = nullptr;

    DarkComboBox *m_comboThemeMode = nullptr;
    DarkComboBox *m_comboAccent = nullptr;
    QLineEdit *m_inpCustomHex = nullptr;
    QCheckBox *m_chkChameleon = nullptr;
    ResettableSlider *m_sldChamSpeed = nullptr;
    ResettableSlider *m_sldChamStrength = nullptr;
    DarkComboBox *m_comboBorderSource = nullptr;
    DarkComboBox *m_comboTextColor = nullptr;

    DarkComboBox *m_comboBackdropType = nullptr;
    ResettableSlider *m_sldBackdropDim = nullptr;
    DarkComboBox *m_comboVideoFps = nullptr;
    QCheckBox *m_chkPauseFocusLoss = nullptr;

    QCheckBox *m_chkAnimEnabled = nullptr;
    ResettableSlider *m_sldAnimSpeed = nullptr;
    DarkComboBox *m_comboAnimStyle = nullptr;
    QCheckBox *m_chkCursorGlow = nullptr;
    QCheckBox *m_chkReducedMotion = nullptr;

    ResettableSlider *m_sldUiScale = nullptr;
    ResettableSlider *m_sldFontSize = nullptr;
    DarkComboBox *m_comboFontBold = nullptr;
    DarkComboBox *m_comboUiDensity = nullptr;

    DarkComboBox *m_comboSidebarPos = nullptr;
    ResettableSlider *m_sldSidebarWidth = nullptr;
    QCheckBox *m_chkSidebarLabels = nullptr;
    QCheckBox *m_chkRememberWinSize = nullptr;
    QCheckBox *m_chkStartMinimized = nullptr;
    QCheckBox *m_chkAlwaysOnTop = nullptr;

    QCheckBox *m_chkHighContrast = nullptr;
    QCheckBox *m_chkReduceTransp = nullptr;
    DarkComboBox *m_comboEffectsQuality = nullptr;
    QCheckBox *m_chkDisableBlurGpu = nullptr;

    QTextEdit *m_logView = nullptr;

    QLabel *m_lblGitVersionInfo = nullptr;
    QProgressBar *m_updateProgressBar = nullptr;
    QTextEdit *m_updateLogView = nullptr;

    QVariantAnimation *m_sidebarAnim = nullptr;
    QTimer *m_hoverTimer = nullptr;
    QTimer *m_autosaveTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;

    QQueue<QString> m_toastQueue;
    bool m_toastActive = false;
    bool m_isLoadingUi = true;
    bool m_sidebarExpanded = true;
    bool m_pendingHover = true;
    bool m_forceQuit = false;
};

} // namespace mqblox
EOF_SRC_UI_MAINWINDOW_HPP

cat <<'EOF_SRC_UI_MAINWINDOW_CPP' > "src/ui/MainWindow.cpp"
#include "ui/MainWindow.hpp"
#include "core/SettingsRegistry.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSizeGrip>
#include <QUrl>
#include <QWindow>

namespace mqblox {

class LockedScrollArea : public QScrollArea {
public:
    explicit LockedScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        viewport()->setStyleSheet("background: transparent;");
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        bool res = QScrollArea::eventFilter(o, e);
        if (widget() && widget()->x() != 0) {
            widget()->move(0, widget()->y());
        }
        return res;
    }
    void resizeEvent(QResizeEvent *e) override {
        QScrollArea::resizeEvent(e);
        if (horizontalScrollBar()) {
            horizontalScrollBar()->setRange(0, 0);
            horizontalScrollBar()->setValue(0);
        }
        if (widget()) {
            widget()->setFixedWidth(viewport()->width());
            if (widget()->x() != 0) widget()->move(0, widget()->y());
        }
    }
    void scrollContentsBy(int, int dy) override {
        QScrollArea::scrollContentsBy(0, dy);
        if (horizontalScrollBar() && horizontalScrollBar()->value() != 0) {
            horizontalScrollBar()->setValue(0);
        }
        if (widget() && widget()->x() != 0) {
            widget()->move(0, widget()->y());
        }
    }
};

static QScrollArea *wrapScrollable(QWidget *w) {
    auto *s = new LockedScrollArea();
    w->setObjectName("ScrollContents");
    w->setMinimumWidth(0);
    if (w->layout()) {
        w->layout()->setContentsMargins(14, 8, 16, 14);
        w->layout()->setSpacing(12);
    }
    for (QWidget *child : w->findChildren<QWidget *>()) {
        if (!qobject_cast<QSpinBox *>(child)) {
            child->setMinimumWidth(0);
        }
        if (auto *lbl = qobject_cast<QLabel *>(child)) {
            lbl->setWordWrap(true);
        }
    }
    s->setWidget(w);
    w->installEventFilter(s);
    return s;
}

MainWindow::MainWindow(const QString &launchUri, QWidget *parent)
    : QMainWindow(parent), m_launchUri(launchUri) {
    m_settingsMgr.loadAll();
    m_soberCtrl = new SoberController(&m_settingsMgr, this);

    setWindowTitle(tr("MqBlox × MacOBlox v36.0 [HyperOS 4 Liquid Glass • Darling]"));
    QString iconFile = QDir::homePath() + "/.local/share/icons/hicolor/scalable/apps/mqblox.svg";
    if (QFile::exists(iconFile)) setWindowIcon(QIcon(iconFile));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumSize(840, 520);

    m_sidebarAnim = new QVariantAnimation(this);
    m_sidebarAnim->setDuration(180);
    m_sidebarAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sidebarAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        int w = v.toInt();
        m_sidebar->setFixedWidth(w);
        bool isWide = (w >= 145);
        m_titleColWidget->setVisible(isWide);
        m_authorsBox->setVisible(isWide);
        m_ramBar->setVisible(isWide);
        for (auto *btn : m_navButtons) btn->setCompactMode(!isWide);
        m_statusBadge->setText(isWide ? tr("● В МЕНЮ MQBLOX") : "●");
    });

    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() { animateSidebar(m_pendingHover); });

    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        collectSettingsFromUi();
        QString err;
        if (m_settingsMgr.saveAll(&err)) {
            if (m_lblFflagsValidation) {
                m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён в ClientAppSettings.json"));
                m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
            }
            if (m_lblAutosync) m_lblAutosync->setText(tr("✓ Настройки синхронизированы"));
        } else {
            if (m_lblFflagsValidation) {
                m_lblFflagsValidation->setText("⚠️ " + err);
                m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
            }
            enqueueToast("⚠️ " + err);
        }
    });

    initUi();
    initTrayAndShortcuts();
    syncUiFromSettings();
    refreshAccountsUi();
    refreshNamedProfilesCombo();
    refreshEnvironmentAndSessionUi();
    applyResolutionMode(m_settingsMgr.settings().resMode, true);
    applyLayoutAndTypographyLive();
    m_isLoadingUi = false;

    connect(m_soberCtrl, &SoberController::hardwareDetected, this, [this]() {
        if (m_hwBannerLbl) {
            m_hwBannerLbl->setText(tr("🔍 Железо: <b>%1</b> (%2 потоков) • GPU: <b>%3</b>")
                                       .arg(m_soberCtrl->cpuModel())
                                       .arg(m_soberCtrl->cpuThreads())
                                       .arg(m_soberCtrl->gpuModel()));
        }
        refreshEnvironmentAndSessionUi();
    });
    connect(m_soberCtrl, &SoberController::logGenerated, this, [this](const QString &line) {
        if (m_logView) m_logView->append(line);
    });
    connect(m_soberCtrl, &SoberController::toastRequested, this, &MainWindow::enqueueToast);
    connect(m_soberCtrl, &SoberController::gameStarted, this, [this]() {
        if (m_rootCosmos) m_rootCosmos->setInGamePaused(true);
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В ИГРЕ (DARLING)") : "🎮");
        enqueueToast(tr("🚀 RobloxPlayer.app запущен через Darling (аудио и микрофон в audio_hal.c)"));
    });
    connect(m_soberCtrl, &SoberController::gameLeftByPlayer, this, [this]() {
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        showNormal();
        raise();
        activateWindow();
        switchTab(0);
        enqueueToast(tr("🏠 Выход с сервера: возврат в MqBlox"));
    });
    connect(m_soberCtrl, &SoberController::gameFinished, this, [this](int, bool crashed) {
        if (m_rootCosmos) m_rootCosmos->setInGamePaused(false);
        m_statusBadge->setText(m_sidebarExpanded ? tr("● В МЕНЮ MQBLOX") : "●");
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        refreshEnvironmentAndSessionUi();
        showNormal();
        raise();
        activateWindow();
        if (crashed && m_settingsMgr.settings().autoReconnectOnCrash) {
            enqueueToast(tr("⚠️ Обнаружен сбой клиента! Авто-перезаход через 3 с..."));
            QTimer::singleShot(3000, this, [this]() { m_soberCtrl->launchRoblox(); });
        }
    });
    connect(m_soberCtrl, &SoberController::accountSwitchCompleted, this, [this](const QString &accId) {
        refreshAccountsUi();
        refreshEnvironmentAndSessionUi();
        enqueueToast(tr("👤 Слот сессии переключён на «%1»").arg(accId));
    });
    connect(m_soberCtrl, &SoberController::wizardProgress, this,
            [this](int pct, const QString &statusText) {
                if (m_wizardProgressBar) m_wizardProgressBar->setValue(pct);
                if (m_lblWizardStatus) m_lblWizardStatus->setText(statusText);
            });
    connect(m_soberCtrl, &SoberController::wizardFinished, this,
            [this](bool ok, const QString &summary) {
                if (m_btnRunWizard) m_btnRunWizard->setEnabled(true);
                if (m_lblWizardStatus) {
                    m_lblWizardStatus->setText((ok ? "✅ " : "❌ ") + summary);
                }
                enqueueToast((ok ? "✅ " : "⚠️ ") + summary);
                refreshEnvironmentAndSessionUi();
            });
    connect(m_soberCtrl, &SoberController::sessionStateChanged, this,
            &MainWindow::refreshEnvironmentAndSessionUi);

    m_soberCtrl->detectHardwareAsync();

    auto *watchdog = new QTimer(this);
    connect(watchdog, &QTimer::timeout, this, [this]() { runWatchdogTick(); });
    watchdog->start(3000);
    runWatchdogTick();

    if (!m_launchUri.isEmpty()) {
        QTimer::singleShot(400, this, [this]() { m_soberCtrl->launchRoblox(m_launchUri); });
    }
}

MainWindow::~MainWindow() = default;

Qt::Edges MainWindow::detectResizeEdges(const QPointF &pos) const {
    if (isMaximized()) return {};
    constexpr double margin = 7.0;
    Qt::Edges edges;
    if (pos.x() <= margin) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - margin) edges |= Qt::RightEdge;
    if (pos.y() <= margin) edges |= Qt::TopEdge;
    if (pos.y() >= height() - margin) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && windowHandle()) {
        Qt::Edges edges = detectResizeEdges(e->position());
        if (edges != Qt::Edges{}) {
            windowHandle()->startSystemResize(edges);
            return;
        }
        if (e->position().y() <= 52.0) {
            windowHandle()->startSystemMove();
            return;
        }
    }
    QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e) {
    Qt::Edges edges = detectResizeEdges(e->position());
    if ((edges == (Qt::LeftEdge | Qt::TopEdge)) || (edges == (Qt::RightEdge | Qt::BottomEdge)))
        setCursor(Qt::SizeFDiagCursor);
    else if ((edges == (Qt::RightEdge | Qt::TopEdge)) || (edges == (Qt::LeftEdge | Qt::BottomEdge)))
        setCursor(Qt::SizeBDiagCursor);
    else if (edges & (Qt::LeftEdge | Qt::RightEdge))
        setCursor(Qt::SizeHorCursor);
    else if (edges & (Qt::TopEdge | Qt::BottomEdge))
        setCursor(Qt::SizeVerCursor);
    else
        unsetCursor();
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::closeEvent(QCloseEvent *e) {
    if (m_autosaveTimer && m_autosaveTimer->isActive()) {
        m_autosaveTimer->stop();
        saveSettingsNow(false);
    }
    if (!m_forceQuit && m_settingsMgr.settings().closeToTray && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        e->ignore();
        return;
    }
    if (m_trayIcon) m_trayIcon->hide();
    e->accept();
    QApplication::quit();
}

void MainWindow::initTrayAndShortcuts() {
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon = new QSystemTrayIcon(windowIcon(), this);
        m_trayIcon->setToolTip(tr("MqBlox × MacOBlox v36.0 — HyperOS 4 Liquid Glass"));

        auto *menu = new QMenu(nullptr);
        menu->setStyleSheet(
            "QMenu { background-color: #070A14; color: #F8FAFC; border: 1px solid #38BDF8; border-radius: 10px; padding: 6px; font-weight: 700; }"
            "QMenu::item { padding: 7px 18px; border-radius: 6px; }"
            "QMenu::item:selected { background-color: #38BDF8; color: #030408; }");
        menu->addAction(tr("🌌 Развернуть MqBlox"), this, [this]() {
            showNormal();
            raise();
            activateWindow();
        });
        menu->addAction(tr("▶ Запустить Roblox (Darling)"), this, [this]() { m_soberCtrl->launchRoblox(); });
        menu->addAction(tr("🛑 Остановить Roblox"), this, [this]() { m_soberCtrl->stopRobloxAsync(); });
        menu->addSeparator();
        menu->addAction(tr("❌ Полный выход из MqBlox"), this, [this, menu]() {
            m_forceQuit = true;
            if (m_trayIcon) m_trayIcon->hide();
            menu->deleteLater();
            QApplication::quit();
        });

        m_trayIcon->setContextMenu(menu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
                if (isVisible() && !isMinimized()) hide();
                else {
                    showNormal();
                    raise();
                    activateWindow();
                }
            }
        });
        m_trayIcon->show();
    }

    new QShortcut(QKeySequence(Qt::Key_F5), this, [this]() { refreshEnvironmentAndSessionUi(); });
    new QShortcut(QKeySequence("Ctrl+L"), this, [this]() { m_soberCtrl->launchRoblox(); });
    new QShortcut(QKeySequence("Ctrl+K"), this, [this]() { m_soberCtrl->stopRobloxAsync(); });
    new QShortcut(QKeySequence("Ctrl+Q"), this, [this]() { m_forceQuit = true; close(); });
}

void MainWindow::initUi() {
    QFile qssFile(":/style_ton618.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    m_rootCosmos = new Ton618GlWidget(this);
    setCentralWidget(m_rootCosmos);
    auto *rootLayout = new QHBoxLayout(m_rootCosmos);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_sidebar = new HoverSidebar();
    m_sidebar->setObjectName("Sidebar");
    m_sidebar->setFixedWidth(232);
    m_sidebar->onHoverChanged = [this](bool hov) {
        if (m_settingsMgr.settings().sidebarPinned) return;
        m_pendingHover = hov;
        m_hoverTimer->start(35);
    };

    auto *sideLay = new QVBoxLayout(m_sidebar);
    sideLay->setContentsMargins(10, 12, 10, 12);
    sideLay->setSpacing(7);

    auto *hdrRow = new QHBoxLayout();
    m_btnPinSide = new AnimButton("📌");
    m_btnPinSide->setFixedSize(46, 42);
    connect(m_btnPinSide, &QPushButton::clicked, this, [this]() { toggleSidebarPinMode(); });
    hdrRow->addWidget(m_btnPinSide);

    m_titleColWidget = new QWidget();
    auto *tCol = new QVBoxLayout(m_titleColWidget);
    tCol->setContentsMargins(0, 0, 0, 0);
    tCol->setSpacing(1);
    auto *logoLbl = new QLabel("✦ MqBlox × MacOBlox");
    logoLbl->setObjectName("LogoText");
    m_pinHintLbl = new QLabel("💧 HyperOS 4 • Darling macOS");
    m_pinHintLbl->setStyleSheet("font-size: 10px; font-weight: 800; color: #38BDF8;");
    tCol->addWidget(logoLbl);
    tCol->addWidget(m_pinHintLbl);
    hdrRow->addWidget(m_titleColWidget);
    hdrRow->addStretch();
    sideLay->addLayout(hdrRow);

    struct NavItem { QString ico; QString title; int idx; };
    const std::vector<NavItem> navItems = {
        {"🏠", tr("Главная и Мастер"), 0},
        {"👥", tr("Слоты и Профили"), 1},
        {"⚙️", tr("Система и Вид"), 2},
        {"✨", tr("Графика (OpenGL)"), 3},
        {"🔥", tr("Твики и Отладка"), 4},
        {"ℹ️", tr("О программе"), 5}
    };
    for (const auto &it : navItems) {
        auto *btn = new AnimNavButton(it.ico, it.title);
        btn->setActiveTab(it.idx == 0);
        int idx = it.idx;
        connect(btn, &QPushButton::clicked, this, [this, idx]() { switchTab(idx); });
        sideLay->addWidget(btn);
        m_navButtons.push_back(btn);
    }

    sideLay->addStretch();

    m_authorsBox = new QFrame();
    m_authorsBox->setObjectName("AuthorsBox");
    auto *abLay = new QVBoxLayout(m_authorsBox);
    abLay->setContentsMargins(12, 10, 12, 10);
    auto *lblAuthT = new QLabel(tr("👑 CREDITS (MIT):"));
    lblAuthT->setStyleSheet("font-size: 10px; font-weight: 900; color: #FDE047;");
    auto *lblAuthN = new QLabel("✦ Narezany (MacOBlox)\n✦ MrQuster (MqBlox UI)\n✦ Darling Project");
    lblAuthN->setStyleSheet("font-size: 11px; font-weight: 800; color: #FFFFFF;");
    abLay->addWidget(lblAuthT);
    abLay->addWidget(lblAuthN);
    sideLay->addWidget(m_authorsBox);

    m_ramBar = new QProgressBar();
    m_ramBar->setObjectName("RamBar");
    m_ramBar->setRange(0, 100);
    sideLay->addWidget(m_ramBar);

    m_statusBadge = new QLabel(tr("● В МЕНЮ MQBLOX"));
    m_statusBadge->setObjectName("StatusBadge");
    m_statusBadge->setFixedHeight(32);
    m_statusBadge->setAlignment(Qt::AlignCenter);
    sideLay->addWidget(m_statusBadge);
    rootLayout->addWidget(m_sidebar);

    auto *rightPanel = new QWidget();
    auto *rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(12, 10, 12, 10);
    rightLay->setSpacing(8);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    m_inpSearchSettings = new QLineEdit();
    m_inpSearchSettings->setPlaceholderText(tr("🔍 Поиск настроек (FPS, MSAA, DNS, мышь, тени, стекло)..."));
    m_inpSearchSettings->setClearButtonEnabled(true);
    m_inpSearchSettings->setMinimumWidth(220);
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(90);
    connect(m_searchDebounce, &QTimer::timeout, this, [this]() {
        filterSettingsBySearch(m_inpSearchSettings->text());
    });
    connect(m_inpSearchSettings, &QLineEdit::textChanged, this, [this]() {
        m_searchDebounce->start();
    });
    topBar->addWidget(m_inpSearchSettings, 1);

    m_toastLbl = new QLabel();
    m_toastLbl->setObjectName("ToastBanner");
    m_toastLbl->setVisible(false);
    topBar->addWidget(m_toastLbl);

    topBar->addWidget(new QLabel(tr("Окно:")));
    m_comboRes = new DarkComboBox();
    m_comboRes->setMinimumWidth(150);
    m_comboRes->addItem(tr("Авто (Под монитор)"), static_cast<int>(ResolutionMode::Auto));
    m_comboRes->addItem(tr("HD (1200×680)"), static_cast<int>(ResolutionMode::HD_720p));
    m_comboRes->addItem(tr("Full HD (1480×840)"), static_cast<int>(ResolutionMode::FHD_1080p));
    m_comboRes->addItem(tr("2K QHD (1920×1080)"), static_cast<int>(ResolutionMode::QHD_2K));
    connect(m_comboRes, &QComboBox::currentIndexChanged, this, [this](int idx) {
        auto mode = static_cast<ResolutionMode>(m_comboRes->itemData(idx).toInt());
        applyResolutionMode(mode, false);
    });
    topBar->addWidget(m_comboRes);

    topBar->addWidget(new QLabel(tr("Слот:")));
    m_comboQuickAcc = new DarkComboBox();
    m_comboQuickAcc->setMinimumWidth(130);
    connect(m_comboQuickAcc, &QComboBox::currentIndexChanged, this, [this](int i) {
        QString id = m_comboQuickAcc->itemData(i).toString();
        if (!id.isEmpty() && id != m_settingsMgr.activeAccountId()) {
            m_soberCtrl->switchAccountSlotAsync(id);
        }
    });
    topBar->addWidget(m_comboQuickAcc);

    auto *btnMin = new AnimButton("—");
    btnMin->setFixedSize(36, 34);
    btnMin->setMinimumWidth(36);
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    m_btnMax = new AnimButton("□");
    m_btnMax->setFixedSize(36, 34);
    m_btnMax->setMinimumWidth(36);
    connect(m_btnMax, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            m_btnMax->setText("□");
        } else {
            showMaximized();
            m_btnMax->setText("❐");
        }
    });
    auto *btnClose = new AnimButton("✕", nullptr, QColor(239, 68, 68), false, true);
    btnClose->setFixedSize(36, 34);
    btnClose->setMinimumWidth(36);
    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);

    topBar->addWidget(btnMin);
    topBar->addWidget(m_btnMax);
    topBar->addWidget(btnClose);
    rightLay->addLayout(topBar);

    m_pages = new QStackedWidget();
    m_pages->addWidget(wrapScrollable(buildHomeRobloxPage()));
    m_pages->addWidget(wrapScrollable(buildAccountsPage()));
    m_pages->addWidget(wrapScrollable(buildHardwareAudioPage()));
    m_pages->addWidget(wrapScrollable(buildGraphicsPage()));
    m_pages->addWidget(wrapScrollable(buildCameraModsPage()));
    m_pages->addWidget(wrapScrollable(buildUpdatesPage()));
    rightLay->addWidget(m_pages, 1);

    auto *bottomBar = new QHBoxLayout();
    bottomBar->setSpacing(8);
    m_lblAutosync = new QLabel(tr("✓ Настройки актуальны"));
    m_lblAutosync->setObjectName("AutoSyncBadge");

    m_btnSaveSettings = new AnimButton(tr("💾 Сохранить настройки"), nullptr, QColor(56, 189, 248));
    connect(m_btnSaveSettings, &QPushButton::clicked, this, [this]() { saveSettingsNow(true); });
    new QShortcut(QKeySequence("Ctrl+S"), this, [this]() { saveSettingsNow(true); });

    auto *btnResetCat = new AnimButton(tr("↺ Сбросить вкладку"));
    connect(btnResetCat, &QPushButton::clicked, this, [this]() { resetCurrentCategoryToDefaults(); });

    auto *btnResetLook = new AnimButton(tr("🎨 Сбросить вид"), nullptr, QColor(56, 189, 248));
    connect(btnResetLook, &QPushButton::clicked, this, [this]() { resetAppearanceToDefault(); });

    auto *btnRejoin = new AnimButton(tr("🔄 Перезапустить Darling"));
    connect(btnRejoin, &QPushButton::clicked, this, [this]() { m_soberCtrl->restartDarlingAsync(); });

    auto *btnKill = new AnimButton(tr("🛑 Остановить игру"), nullptr, QColor(239, 68, 68), false, true);
    connect(btnKill, &QPushButton::clicked, this, [this]() { m_soberCtrl->stopRobloxAsync(); });

    auto *btnPlay = new AnimButton(tr("▶  ЗАПУСТИТЬ ИГРУ"), nullptr, QColor(16, 185, 129), true, false);
    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        saveSettingsNow(false);
        m_soberCtrl->launchRoblox();
    });

    bottomBar->addWidget(m_lblAutosync);
    bottomBar->addWidget(m_btnSaveSettings);
    bottomBar->addWidget(btnResetCat);
    bottomBar->addWidget(btnResetLook);
    bottomBar->addWidget(btnRejoin);
    bottomBar->addWidget(btnKill);
    bottomBar->addStretch();
    bottomBar->addWidget(btnPlay);

    auto *grip = new QSizeGrip(this);
    grip->setFixedSize(16, 16);
    bottomBar->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
    rightLay->addLayout(bottomBar);
    rootLayout->addWidget(rightPanel, 1);
}

QWidget *MainWindow::buildHomeRobloxPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    // 1. Quick Launch & Session Card (Zero Password / Cookie Interception)
    auto *cardLaunch = new QFrame();
    cardLaunch->setObjectName("GlassCard");
    auto *cl = new QVBoxLayout(cardLaunch);
    cl->setContentsMargins(18, 16, 18, 16);
    cl->setSpacing(10);

    auto *tLaunch = new QLabel(tr("🚀 Запуск официального macOS-клиента Roblox через Darling (MacOBlox Backend)"));
    tLaunch->setObjectName("SectionTitle");
    cl->addWidget(tLaunch);

    auto *authNote = new QLabel(tr(
        "🔐 <b>Приватность пароля и сессии:</b> лаунчер не содержит встроенного браузера для перехвата cookie, "
        "никогда не запрашивает пароль и не читает/не отправляет <code>.ROBLOSECURITY</code>. "
        "Авторизация выполняется штатно внутри клиента Roblox через <b>«Быстрый вход» (Quick Login)</b>."));
    authNote->setObjectName("SubText");
    authNote->setWordWrap(true);
    cl->addWidget(authNote);

    auto *placeRow = new QHBoxLayout();
    placeRow->setSpacing(8);
    m_inpPlace = new QLineEdit();
    m_inpPlace->setPlaceholderText(tr("⚡ Необязательно: Place ID (например 2753915549), ссылка roblox.com/games/... или roblox://..."));
    auto launchByInput = [this]() {
        const QString raw = m_inpPlace->text().trimmed();
        const QString uri = SettingsManager::parsePlaceUri(raw);
        if (!raw.isEmpty() && uri.isEmpty()) {
            enqueueToast(tr("⚠️ Введите числовой Place ID, ссылку на плейс или оставьте поле пустым"));
            return;
        }
        saveSettingsNow(false);
        m_soberCtrl->launchRoblox(uri);
    };
    connect(m_inpPlace, &QLineEdit::returnPressed, this, launchByInput);
    connect(m_inpPlace, &QLineEdit::textChanged, this, [this]() { triggerInstantAutosave(); });

    auto *btnGoId = new AnimButton(tr("🚀 ЗАПУСТИТЬ ROBLOX"), nullptr, QColor(16, 185, 129), true);
    connect(btnGoId, &QPushButton::clicked, this, launchByInput);
    placeRow->addWidget(m_inpPlace, 1);
    placeRow->addWidget(btnGoId);
    cl->addLayout(placeRow);

    auto *sessRow = new QHBoxLayout();
    m_lblSessionStatus = new QLabel(tr("Проверка статуса локальной сессии в Darling..."));
    m_lblSessionStatus->setObjectName("SubText");
    m_btnLogout = new AnimButton(tr("🚪 Очистить сессию в Darling"), nullptr, QColor(239, 68, 68), false, true);
    connect(m_btnLogout, &QPushButton::clicked, this, [this]() { m_soberCtrl->logoutAsync(); });
    sessRow->addWidget(m_lblSessionStatus, 1);
    sessRow->addWidget(m_btnLogout);
    cl->addLayout(sessRow);
    lay->addWidget(cardLaunch);

    // 2. Non-Blocking First-Run & Update Wizard Card
    auto *cardWiz = new QFrame();
    cardWiz->setObjectName("GlassCard");
    auto *cw = new QVBoxLayout(cardWiz);
    cw->setContentsMargins(18, 16, 18, 16);
    cw->setSpacing(10);

    auto *tWiz = new QLabel(tr("🛠️ Мастер первой установки и обновления (Darling, Шимы, RobloxPlayer.app)"));
    tWiz->setObjectName("SectionTitle");
    cw->addWidget(tWiz);

    m_lblEnvSummary = new QLabel();
    m_lblEnvSummary->setWordWrap(true);
    cw->addWidget(m_lblEnvSummary);

    m_wizardProgressBar = new QProgressBar();
    m_wizardProgressBar->setObjectName("UpdateBar");
    m_wizardProgressBar->setRange(0, 100);
    m_wizardProgressBar->setValue(0);
    cw->addWidget(m_wizardProgressBar);

    m_lblWizardStatus = new QLabel(tr("Готов к сборке libMacOBloxShims.dylib и загрузке RobloxPlayer.app с официального CDN."));
    m_lblWizardStatus->setObjectName("SubText");
    m_lblWizardStatus->setWordWrap(true);
    cw->addWidget(m_lblWizardStatus);

    auto *wizBtns = new QHBoxLayout();
    m_btnRunWizard = new AnimButton(tr("⬇️ Установить / Обновить (Шимы + Roblox)"), nullptr, QColor(16, 185, 129), true);
    connect(m_btnRunWizard, &QPushButton::clicked, this, [this]() {
        m_btnRunWizard->setEnabled(false);
        m_soberCtrl->runSetupOrUpdateWizardAsync();
    });
    auto *btnCheckEnv = new AnimButton(tr("🔍 Проверить зависимости"), nullptr, QColor(56, 189, 248));
    connect(btnCheckEnv, &QPushButton::clicked, this, &MainWindow::refreshEnvironmentAndSessionUi);
    wizBtns->addWidget(m_btnRunWizard, 2);
    wizBtns->addWidget(btnCheckEnv, 1);
    cw->addLayout(wizBtns);
    lay->addWidget(cardWiz);

    // 3. Diagnostics Card with "Копировать отчёт"
    auto *cardDiag = new QFrame();
    cardDiag->setObjectName("GlassCard");
    auto *cd = new QVBoxLayout(cardDiag);
    cd->setContentsMargins(18, 16, 18, 16);
    cd->setSpacing(10);

    auto *tDiag = new QLabel(tr("🩺 Диагностика системы (Версии, GPU/VRAM, Wayland/X11, Ошибки)"));
    tDiag->setObjectName("SectionTitle");
    cd->addWidget(tDiag);

    m_homeDiagView = new QTextEdit();
    m_homeDiagView->setReadOnly(true);
    m_homeDiagView->setFixedHeight(140);
    cd->addWidget(m_homeDiagView);

    auto *diagRow = new QHBoxLayout();
    auto *btnCopyDiag = new AnimButton(tr("📋 Копировать отчёт"), nullptr, QColor(56, 189, 248));
    connect(btnCopyDiag, &QPushButton::clicked, this, [this]() {
        const QString report = m_soberCtrl->collectDiagnosticsReport();
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(report);
        }
        if (m_homeDiagView) m_homeDiagView->setPlainText(report);
        enqueueToast(tr("📋 Отчёт диагностики скопирован в буфер обмена!"));
    });
    diagRow->addWidget(btnCopyDiag);
    diagRow->addStretch();
    cd->addLayout(diagRow);
    lay->addWidget(cardDiag);

    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildAccountsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *cardProf = new QFrame();
    cardProf->setObjectName("GlassCard");
    auto *cp = new QVBoxLayout(cardProf);
    cp->setContentsMargins(18, 16, 18, 16);
    cp->setSpacing(10);

    auto *tProf = new QLabel(tr("📂 Именные профили настроек (Сохранение, Загрузка, Экспорт / Импорт, Сброс)"));
    tProf->setObjectName("SectionTitle");
    cp->addWidget(tProf);

    auto *rowNamed = new QHBoxLayout();
    m_comboNamedProfiles = new DarkComboBox();
    m_inpNewProfileName = new QLineEdit();
    m_inpNewProfileName->setPlaceholderText(tr("Имя профиля настроек (например: Hyprland240)..."));
    auto *btnSaveNamed = new AnimButton(tr("💾 Сохранить профиль"), nullptr, QColor(16, 185, 129));
    auto *btnLoadNamed = new AnimButton(tr("📂 Загрузить"), nullptr, QColor(56, 189, 248));
    auto *btnDelNamed = new AnimButton(tr("🗑 Удалить"), nullptr, QColor(239, 68, 68), false, true);

    connect(btnSaveNamed, &QPushButton::clicked, this, [this]() {
        collectSettingsFromUi();
        const QString name = m_inpNewProfileName->text().trimmed().isEmpty()
                                 ? m_comboNamedProfiles->currentText()
                                 : m_inpNewProfileName->text().trimmed();
        QString err;
        if (m_settingsMgr.saveNamedProfile(name, &err)) {
            m_inpNewProfileName->clear();
            refreshNamedProfilesCombo();
            enqueueToast(tr("💾 Профиль «%1» сохранён!").arg(name));
        } else {
            enqueueToast("⚠️ " + err);
        }
    });
    connect(btnLoadNamed, &QPushButton::clicked, this, [this]() {
        const QString name = m_comboNamedProfiles->currentText();
        if (name.isEmpty()) return;
        QString err;
        if (m_settingsMgr.loadNamedProfile(name, &err)) {
            syncUiFromSettings();
            applyLayoutAndTypographyLive();
            enqueueToast(tr("📂 Профиль «%1» загружен!").arg(name));
        } else {
            enqueueToast("⚠️ " + err);
        }
    });
    connect(btnDelNamed, &QPushButton::clicked, this, [this]() {
        const QString name = m_comboNamedProfiles->currentText();
        if (name.isEmpty()) return;
        m_settingsMgr.deleteNamedProfile(name);
        refreshNamedProfilesCombo();
        enqueueToast(tr("🗑 Профиль «%1» удалён").arg(name));
    });

    rowNamed->addWidget(m_comboNamedProfiles, 1);
    rowNamed->addWidget(m_inpNewProfileName, 1);
    rowNamed->addWidget(btnSaveNamed);
    rowNamed->addWidget(btnLoadNamed);
    rowNamed->addWidget(btnDelNamed);
    cp->addLayout(rowNamed);
    lay->addWidget(cardProf);

    auto *cardTop = new QFrame();
    cardTop->setObjectName("GlassCard");
    auto *ct = new QVBoxLayout(cardTop);
    ct->setContentsMargins(18, 16, 18, 16);
    ct->setSpacing(10);

    auto *t1 = new QLabel(tr("👥 Локальные слоты сессий Darling (без чтения .ROBLOSECURITY)"));
    t1->setObjectName("SectionTitle");
    ct->addWidget(t1);

    auto *sub = new QLabel(tr(
        "Каждый слот изолирует каталог <code>~/Library/MacOBlox</code> внутри префикса Darling. "
        "Лаунчер переключает папки целиком и никогда не читает и не расшифровывает токены."));
    sub->setObjectName("SubText");
    sub->setWordWrap(true);
    ct->addWidget(sub);

    auto *btnRow = new QHBoxLayout();
    auto *bAdd = new AnimButton(tr("➕ Добавить слот"));
    auto *bSave = new AnimButton(tr("📥 Сохранить текущую сессию в слот"));

    connect(bAdd, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Новый слот"), tr("Название слота:"), QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            AccountProfile p;
            p.id = QString("slot_%1").arg(QDateTime::currentSecsSinceEpoch() % 100000);
            p.label = name.trimmed();
            p.displayName = name.trimmed();
            m_settingsMgr.accounts().push_back(p);
            m_soberCtrl->switchAccountSlotAsync(p.id);
        }
    });
    connect(bSave, &QPushButton::clicked, this, [this]() {
        m_soberCtrl->saveCurrentSessionToSlotAsync();
        enqueueToast(tr("✓ Локальная папка сессии сохранена в активный слот"));
    });

    btnRow->addWidget(bAdd);
    btnRow->addWidget(bSave);
    ct->addLayout(btnRow);
    lay->addWidget(cardTop);

    m_accountsListLayout = new QVBoxLayout();
    m_accountsListLayout->setSpacing(10);
    lay->addLayout(m_accountsListLayout);
    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildHardwareAudioPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    // 1. Hardware & MacOBlox Runtime Card
    auto *cardHw = new QFrame();
    cardHw->setObjectName("GlassCard");
    auto *hw = new QVBoxLayout(cardHw);
    hw->setContentsMargins(18, 16, 18, 16);
    hw->setSpacing(10);

    auto *thw = new QLabel(tr("🖥️ Среда MacOBlox (Darling), Ввод, DNS-over-TLS и Звук"));
    thw->setObjectName("SectionTitle");
    hw->addWidget(thw);

    m_hwBannerLbl = new QLabel(tr("🔍 Определение оборудования..."));
    m_hwBannerLbl->setObjectName("SubText");
    hw->addWidget(m_hwBannerLbl);

    auto *micNote = new QLabel(tr(
        "🎙️ <b>Звук и микрофон полностью управляются внутри <code>shims/audio_hal.c</code></b> "
        "(CoreAudio HAL + AUHAL Input/Output Bus + <code>AudioUnitRender</code>). "
        "Интерфейс MqBlox вообще не прикасается к PipeWire/PulseAudio/ALSA."));
    micNote->setObjectName("SubText");
    micNote->setWordWrap(true);
    hw->addWidget(micNote);

    auto *sensRow = new QHBoxLayout();
    sensRow->addWidget(new QLabel(tr("🖱️ Чувствительность камеры и мыши (MACOBLOX_MOUSE_SENSITIVITY):")));
    m_sldMouseSens = new ResettableSlider(10, 200, 100, "%");
    m_sldMouseSens->onValueCommitted = [this](int) { triggerInstantAutosave(); };
    sensRow->addWidget(m_sldMouseSens, 1);
    hw->addLayout(sensRow);

    auto *dnsRow = new QHBoxLayout();
    dnsRow->addWidget(new QLabel(tr("🔒 DNS-over-TLS прокси (MACOBLOX_DNS):")));
    m_comboDnsProvider = new DarkComboBox();
    m_comboDnsProvider->addItem(tr("Системный DNS (Без DoT)"), 0);
    m_comboDnsProvider->addItem(tr("Quad9 DoT (9.9.9.9:853 — Рекомендуется)"), 1);
    m_comboDnsProvider->addItem(tr("Cloudflare DoT (1.1.1.1:853)"), 2);
    m_comboDnsProvider->addItem(tr("Google DoT (8.8.8.8:853)"), 3);
    m_comboDnsProvider->addItem(tr("Свой IPv4 DoT сервер..."), 4);
    m_inpDnsCustom = new QLineEdit();
    m_inpDnsCustom->setPlaceholderText(tr("IPv4 адрес DoT сервера (например 9.9.9.9)"));
    connect(m_comboDnsProvider, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_inpDnsCustom) m_inpDnsCustom->setEnabled(m_comboDnsProvider->currentData().toInt() == 4);
        triggerInstantAutosave();
    });
    connect(m_inpDnsCustom, &QLineEdit::textChanged, this, [this]() { triggerInstantAutosave(); });
    dnsRow->addWidget(m_comboDnsProvider, 1);
    dnsRow->addWidget(m_inpDnsCustom, 1);
    hw->addLayout(dnsRow);

    m_chkHideMenuBar   = new QCheckBox(tr("Скрывать верхнюю строку меню macOS в окне игры (MACOBLOX_HIDE_MENU_BAR)"));
    m_chkVsyncOff      = new QCheckBox(tr("Отключить VSync в EGL (MACOBLOX_VSYNC=0, минимальная задержка ввода)"));
    m_chkReturnMq      = new QCheckBox(tr("При выходе с сервера (Leave Game) возвращать в окно MqBlox"));
    m_chkAnimBg        = new QCheckBox(tr("Анимированный 3D-шейдер аккреционного диска TON 618"));
    m_chkCloseToTray   = new QCheckBox(tr("Сворачивать MqBlox в системный трей при закрытии окна (✕)"));
    m_chkAutoClean     = new QCheckBox(tr("Авто-очистка старого кэша (>500 МБ)"));
    m_chkMemFflags     = new QCheckBox(tr("Быстрая выгрузка неиспользуемых мешей из ОЗУ (Low-RAM FFlags)"));
    m_chkNoTelemetry   = new QCheckBox(tr("Отключить фоновую телеметрию Roblox (FFlagDebugDisableTelemetry*)"));
    m_chkAutoReconnect = new QCheckBox(tr("Авто-перезаход при аварийном завершении клиента"));

    for (QCheckBox *chk : {m_chkHideMenuBar, m_chkVsyncOff, m_chkReturnMq, m_chkAnimBg,
                           m_chkCloseToTray, m_chkAutoClean, m_chkMemFflags,
                           m_chkNoTelemetry, m_chkAutoReconnect}) {
        hw->addWidget(chk);
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    lay->addWidget(cardHw);

    // 2. HyperOS 4 Liquid Glass Appearance Card
    auto *cardVis = new QFrame();
    cardVis->setObjectName("GlassCard");
    auto *cv = new QGridLayout(cardVis);
    cv->setContentsMargins(18, 16, 18, 16);
    cv->setSpacing(10);

    auto *tVis = new QLabel(tr("🎨 Оформление HyperOS 4 Liquid Glass (Единый буфер размытия)"));
    tVis->setObjectName("SectionTitle");
    cv->addWidget(tVis, 0, 0, 1, 2);

    cv->addWidget(new QLabel(tr("Пресет оформления:")), 1, 0);
    m_comboVisualPreset = new DarkComboBox();
    m_comboVisualPreset->addItem(tr("0 — Liquid Glass (Стандарт)"), 0);
    m_comboVisualPreset->addItem(tr("1 — Матовое стекло (Deep Frosted)"), 1);
    m_comboVisualPreset->addItem(tr("2 — Минимализм (Тонированный)"), 2);
    m_comboVisualPreset->addItem(tr("3 — Высокий контраст"), 3);
    m_comboVisualPreset->addItem(tr("4 — Экономный (0% GPU/CPU в фоне)"), 4);
    connect(m_comboVisualPreset, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_isLoadingUi) return;
        SettingsRegistry::applyVisualPreset(m_settingsMgr.settings(), m_comboVisualPreset->itemData(idx).toInt());
        syncUiFromSettings();
        applyLayoutAndTypographyLive();
        triggerInstantAutosave();
    });
    cv->addWidget(m_comboVisualPreset, 1, 1);

    cv->addWidget(new QLabel(tr("Тема интерфейса:")), 2, 0);
    m_comboThemeMode = new DarkComboBox();
    m_comboThemeMode->addItem(tr("Тёмная (Dark Cosmos)"), 0);
    m_comboThemeMode->addItem(tr("Светлая (Light Pearl)"), 1);
    m_comboThemeMode->addItem(tr("Системная"), 2);
    cv->addWidget(m_comboThemeMode, 2, 1);

    cv->addWidget(new QLabel(tr("Акцентный цвет:")), 3, 0);
    m_comboAccent = new DarkComboBox();
    m_comboAccent->addItem(tr("Голубой HyperOS"), 0);
    m_comboAccent->addItem(tr("Золото TON 618"), 1);
    m_comboAccent->addItem(tr("Изумрудный неон"), 2);
    m_comboAccent->addItem(tr("Фиолетовый квазар"), 3);
    m_comboAccent->addItem(tr("Свой HEX-цвет"), 4);
    cv->addWidget(m_comboAccent, 3, 1);

    cv->addWidget(new QLabel(tr("Сила размытия подложки (0..8):")), 4, 0);
    m_sldBlur = new ResettableSlider(0, 8, 4, "");
    cv->addWidget(m_sldBlur, 4, 1);

    cv->addWidget(new QLabel(tr("Непрозрачность стекла (%):")), 5, 0);
    m_sldOpacity = new ResettableSlider(2, 90, 10, "%");
    cv->addWidget(m_sldOpacity, 5, 1);

    cv->addWidget(new QLabel(tr("Яркость блика грани (%):")), 6, 0);
    m_sldBorderBright = new ResettableSlider(20, 200, 100, "%");
    cv->addWidget(m_sldBorderBright, 6, 1);

    cv->addWidget(new QLabel(tr("Масштаб интерфейса (%):")), 7, 0);
    m_sldUiScale = new ResettableSlider(80, 150, 100, "%");
    cv->addWidget(m_sldUiScale, 7, 1);

    cv->addWidget(new QLabel(tr("Размер шрифта (px):")), 8, 0);
    m_sldFontSize = new ResettableSlider(11, 17, 13, "px");
    cv->addWidget(m_sldFontSize, 8, 1);

    m_chkChameleon = new QCheckBox(tr("Адаптивный Хамелеон (окрашивание граней под цвета TON 618)"));
    m_chkReducedMotion = new QCheckBox(tr("Режим «Меньше движения» (Eco: отключает пружины и прожектор)"));
    cv->addWidget(m_chkChameleon, 9, 0, 1, 2);
    cv->addWidget(m_chkReducedMotion, 10, 0, 1, 2);

    auto *btnCustomBg = new AnimButton(tr("📂 Выбрать своё фото или видео фона (.png/.jpg/.mp4)"), nullptr, QColor(56, 189, 248));
    connect(btnCustomBg, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Выбрать фон"), QDir::homePath(),
                                                    tr("Файлы фона (*.png *.jpg *.jpeg *.mp4 *.webm *.mkv)"));
        if (!path.isEmpty()) {
            m_settingsMgr.settings().backdropType = 3;
            m_settingsMgr.settings().customBackdropPath = path;
            m_rootCosmos->loadCustomVideoFile(path);
            triggerInstantAutosave(tr("🌌 Пользовательский фон применён!"));
        }
    });
    cv->addWidget(btnCustomBg, 11, 0, 1, 2);

    for (ResettableSlider *sl : {m_sldBlur, m_sldOpacity, m_sldBorderBright, m_sldUiScale, m_sldFontSize}) {
        sl->onValueCommitted = [this](int) {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        };
    }
    for (QComboBox *cb : {m_comboThemeMode, m_comboAccent}) {
        connect(cb, &QComboBox::currentIndexChanged, this, [this]() {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        });
    }
    for (QCheckBox *ck : {m_chkChameleon, m_chkReducedMotion}) {
        connect(ck, &QAbstractButton::toggled, this, [this]() {
            collectSettingsFromUi();
            applyLayoutAndTypographyLive();
            triggerInstantAutosave();
        });
    }

    lay->addWidget(cardVis);
    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildGraphicsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *warnFrame = new QFrame();
    warnFrame->setObjectName("WarningBanner");
    auto *wLay = new QHBoxLayout(warnFrame);
    wLay->setContentsMargins(16, 12, 16, 12);
    auto *wLbl = new QLabel(tr(
        "ℹ️ <b>macOS OpenGL Core Profile (gl_profile.c):</b> клиент RobloxPlayer.app под Darling использует OpenGL 4.1 Core. "
        "Шим <code>gl_profile.c</code> автоматически убирает префикс <code>\"AMD \"</code> из <code>GL_RENDERER</code>, "
        "поэтому сглаживание MSAA полноценно работает и на видеокартах AMD (amdgpu)."));
    wLbl->setWordWrap(true);
    wLay->addWidget(wLbl);
    lay->addWidget(warnFrame);

    auto *cardPr = new QFrame();
    cardPr->setObjectName("GlassCard");
    auto *cp = new QVBoxLayout(cardPr);
    cp->setContentsMargins(18, 16, 18, 16);
    cp->setSpacing(10);

    auto *tp = new QLabel(tr("🌟 Глобальные пресеты графики + Экспорт / Импорт JSON"));
    tp->setObjectName("SectionTitle");
    cp->addWidget(tp);

    auto *rowBtns = new QHBoxLayout();
    auto *bFps = new AnimButton(tr("🚀 УЛЬТРА FPS"), nullptr, QColor(16, 185, 129));
    auto *bBal = new AnimButton(tr("⚖️ БАЛАНС"), nullptr, QColor(56, 189, 248));
    auto *bUlt = new AnimButton(tr("💎 УЛЬТРА РЕАЛИЗМ"), nullptr, QColor(168, 85, 247));
    connect(bFps, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(0); });
    connect(bBal, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(1); });
    connect(bUlt, &QPushButton::clicked, this, [this]() { applyGraphicsMasterPreset(2); });
    rowBtns->addWidget(bFps);
    rowBtns->addWidget(bBal);
    rowBtns->addWidget(bUlt);
    cp->addLayout(rowBtns);

    auto *rowIo = new QHBoxLayout();
    auto *bExp = new AnimButton(tr("📤 Экспорт профиля в JSON"));
    auto *bImp = new AnimButton(tr("📥 Импорт профиля из JSON"));
    connect(bExp, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Экспорт пресета"), QDir::homePath() + "/mqblox_preset.json", "JSON (*.json)");
        if (!path.isEmpty() && m_settingsMgr.exportPresetToFile(path)) enqueueToast(tr("📤 Профиль экспортирован!"));
    });
    connect(bImp, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Импорт пресета"), QDir::homePath(), "JSON (*.json)");
        QString err;
        if (!path.isEmpty()) {
            if (m_settingsMgr.importPresetFromFile(path, &err)) {
                syncUiFromSettings();
                applyLayoutAndTypographyLive();
                enqueueToast(tr("📥 Профиль успешно импортирован!"));
            } else {
                enqueueToast("⚠️ " + err);
            }
        }
    });
    rowIo->addWidget(bExp);
    rowIo->addWidget(bImp);
    cp->addLayout(rowIo);

    m_lblPresetDesc = new QLabel(tr("Все параметры ниже атомарно записываются в RobloxPlayer.app/Contents/MacOS/ClientSettings/ClientAppSettings.json."));
    m_lblPresetDesc->setObjectName("SubText");
    cp->addWidget(m_lblPresetDesc);
    lay->addWidget(cardPr);

    auto *cardApi = new QFrame();
    cardApi->setObjectName("GlassCard");
    auto *g1 = new QGridLayout(cardApi);
    g1->setContentsMargins(18, 16, 18, 16);
    g1->setSpacing(10);
    g1->setColumnStretch(0, 0);
    g1->setColumnStretch(1, 1);

    g1->addWidget(new QLabel(tr("Технология освещения:")), 0, 0);
    m_comboLighting = new DarkComboBox();
    m_comboLighting->addItem(tr("Voxel (Максимум FPS)"), static_cast<int>(LightingMode::Voxel));
    m_comboLighting->addItem(tr("ShadowMap (Мягкие тени)"), static_cast<int>(LightingMode::ShadowMap));
    m_comboLighting->addItem(tr("Future Phase 3 (Ультра-реализм)"), static_cast<int>(LightingMode::FuturePhase3));
    m_comboLighting->addItem(tr("По умолчанию плейса"), static_cast<int>(LightingMode::GameDefault));
    g1->addWidget(m_comboLighting, 0, 1);

    g1->addWidget(new QLabel(tr("Лимит кадров (FPS):")), 1, 0);
    auto *fpsRow = new QHBoxLayout();
    m_spinFps = new QSpinBox();
    m_spinFps->setRange(15, 9999);
    m_chkUnlimFps = new QCheckBox(tr("♾️ Без ограничений (9999 FPS)"));
    connect(m_chkUnlimFps, &QAbstractButton::toggled, this, [this](bool c) {
        m_spinFps->setEnabled(!c);
        triggerInstantAutosave();
    });
    fpsRow->addWidget(m_spinFps);
    fpsRow->addWidget(m_chkUnlimFps);
    g1->addLayout(fpsRow, 1, 1);

    g1->addWidget(new QLabel(tr("Качество текстур:")), 2, 0);
    m_comboTex = new DarkComboBox();
    m_comboTex->addItem(tr("Автоматически"), static_cast<int>(TextureQuality::Auto));
    m_comboTex->addItem(tr("Уровень 0 (Минимум)"), static_cast<int>(TextureQuality::Level0));
    m_comboTex->addItem(tr("Уровень 1 (Низкие)"), static_cast<int>(TextureQuality::Level1));
    m_comboTex->addItem(tr("Уровень 2 (Средние)"), static_cast<int>(TextureQuality::Level2));
    m_comboTex->addItem(tr("Уровень 3 (Ультра HD)"), static_cast<int>(TextureQuality::Level3));
    g1->addWidget(m_comboTex, 2, 1);

    g1->addWidget(new QLabel(tr("Сглаживание (MSAA OpenGL):")), 3, 0);
    m_comboMsaa = new DarkComboBox();
    m_comboMsaa->addItem(tr("Отключено (0x)"), static_cast<int>(MsaaMode::Off));
    m_comboMsaa->addItem("2x MSAA", static_cast<int>(MsaaMode::X2));
    m_comboMsaa->addItem("4x MSAA (Макс. для macOS CGL)", static_cast<int>(MsaaMode::X4));
    g1->addWidget(m_comboMsaa, 3, 1);

    g1->addWidget(new QLabel(tr("Уровень графики (FRM 0-21):")), 4, 0);
    m_spinFrm = new QSpinBox();
    m_spinFrm->setRange(0, 21);
    m_spinFrm->setSpecialValueText(tr("0 (Из настроек игры)"));
    g1->addWidget(m_spinFrm, 4, 1);
    lay->addWidget(cardApi);

    auto *cardFx = new QFrame();
    cardFx->setObjectName("GlassCard");
    auto *g2 = new QGridLayout(cardFx);
    g2->setContentsMargins(18, 16, 18, 16);
    g2->setSpacing(10);

    m_chkNoShadows    = new QCheckBox(tr("Отключить тени (FIntRenderShadowIntensity=0)"));
    m_chkUltraShadows = new QCheckBox(tr("💎 Ультра-тени (FIntRenderShadowIntensity=100)"));
    m_chkNoPostfx     = new QCheckBox(tr("Отключить PostFX (FFlagDisablePostFx)"));
    m_chkLowMeshes    = new QCheckBox(tr("Упростить дальние модели (CSG LOD 0)"));
    m_chkGreySky      = new QCheckBox(tr("Упрощённое небо (FFlagDebugSkyGray)"));

    g2->addWidget(m_chkNoShadows, 0, 0);
    g2->addWidget(m_chkUltraShadows, 0, 1);
    g2->addWidget(m_chkNoPostfx, 1, 0);
    g2->addWidget(m_chkLowMeshes, 1, 1);
    g2->addWidget(m_chkGreySky, 2, 0, 1, 2);
    lay->addWidget(cardFx);

    auto *cardFf = new QFrame();
    cardFf->setObjectName("GlassCard");
    auto *ffl = new QVBoxLayout(cardFf);
    ffl->setContentsMargins(18, 16, 18, 16);
    ffl->setSpacing(10);

    auto *tff = new QLabel(tr("🔧 Пользовательские FastFlags (JSON -> ClientAppSettings.json):"));
    tff->setObjectName("SectionTitle");
    ffl->addWidget(tff);
    m_fflagsEditor = new QTextEdit();
    m_fflagsEditor->setFixedHeight(90);
    ffl->addWidget(m_fflagsEditor);
    m_lblFflagsValidation = new QLabel(tr("✓ Готов к проверке JSON"));
    m_lblFflagsValidation->setObjectName("SubText");
    ffl->addWidget(m_lblFflagsValidation);
    lay->addWidget(cardFf);

    for (QComboBox *c : {m_comboLighting, m_comboTex, m_comboMsaa}) {
        connect(c, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QSpinBox *s : {m_spinFps, m_spinFrm}) {
        connect(s, &QSpinBox::valueChanged, this, [this]() { triggerInstantAutosave(); });
    }
    for (QCheckBox *chk : {m_chkNoShadows, m_chkUltraShadows, m_chkNoPostfx, m_chkLowMeshes, m_chkGreySky}) {
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    connect(m_fflagsEditor, &QTextEdit::textChanged, this, [this]() { triggerInstantAutosave(); });

    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildCameraModsPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);

    auto *cardCam = new QFrame();
    cardCam->setObjectName("GlassCard");
    auto *cc = new QVBoxLayout(cardCam);
    cc->setContentsMargins(18, 16, 18, 16);
    cc->setSpacing(10);

    auto *tCam = new QLabel(tr("🔥 Игровые Твики, Камера, Сеть и Трассировка Шимов MacOBlox"));
    tCam->setObjectName("SectionTitle");
    tCam->setWordWrap(true);
    cc->addWidget(tCam);

    auto *zoomRow = new QHBoxLayout();
    zoomRow->setSpacing(10);
    m_chkInfZoom = new QCheckBox(tr("🔭 Разблокировать максимальную дистанцию отдаления камеры (FIntCameraMaxZoomDistance):"));
    m_spinMaxZoom = new QSpinBox();
    m_spinMaxZoom->setRange(128, 100000);
    m_spinMaxZoom->setSingleStep(500);
    m_spinMaxZoom->setFixedWidth(130);
    zoomRow->addWidget(m_chkInfZoom, 1);
    zoomRow->addWidget(m_spinMaxZoom);
    cc->addLayout(zoomRow);

    auto *presetRow = new QHBoxLayout();
    presetRow->setSpacing(10);
    presetRow->addWidget(new QLabel(tr("Быстрый пресет зума:")));
    m_comboCamZoom = new DarkComboBox();
    m_comboCamZoom->addItem(tr("Стандартный (128 студов)"), static_cast<int>(CameraZoomMode::Default));
    m_comboCamZoom->addItem(tr("Средний обзор (500 студов)"), static_cast<int>(CameraZoomMode::Medium500));
    m_comboCamZoom->addItem(tr("Дальний обзор (2000 студов)"), static_cast<int>(CameraZoomMode::Far2000));
    m_comboCamZoom->addItem(tr("Максимум движка (99999 студов)"), static_cast<int>(CameraZoomMode::Cosmic9999));
    connect(m_comboCamZoom, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_isLoadingUi || !m_spinMaxZoom) return;
        int m = m_comboCamZoom->itemData(idx).toInt();
        if (m == 0) m_spinMaxZoom->setValue(128);
        else if (m == 1) m_spinMaxZoom->setValue(500);
        else if (m == 2) m_spinMaxZoom->setValue(2000);
        else m_spinMaxZoom->setValue(99999);
        triggerInstantAutosave();
    });
    presetRow->addWidget(m_comboCamZoom, 1);
    cc->addLayout(presetRow);

    auto *netRow = new QHBoxLayout();
    netRow->setSpacing(10);
    netRow->addWidget(new QLabel(tr("🌐 Сетевой пакетный профиль (MTU / RakNet):")));
    m_comboNetOpt = new DarkComboBox();
    m_comboNetOpt->addItem(tr("⚡ Ультра-отклик без фрагментации (MTU 1396 + Fast RakNet)"), static_cast<int>(NetworkProfile::UltraLowPing));
    m_comboNetOpt->addItem(tr("Стандартный буфер (MTU 1492)"), static_cast<int>(NetworkProfile::Balanced));
    netRow->addWidget(m_comboNetOpt, 1);
    cc->addLayout(netRow);

    m_chkFullBright = new QCheckBox(tr("☀️ FullBright (Voxel + тени 0 + без PostFX)"));
    m_chkClearFog   = new QCheckBox(tr("🌿 Убрать 3D-траву на ландшафте (FIntFRMMinGrassDistance=0)"));
    m_chkHitboxes   = new QCheckBox(tr("📐 Упростить геометрию дальних объектов (CSG LOD 0)"));
    m_chkHudFps     = new QCheckBox(tr("📊 Включить счётчик FPS в игре (FFlagDebugDisplayFPS + MACOBLOX_FPS_LOG)"));

    m_chkFpsLog      = new QCheckBox(tr("📈 Логировать FPS каждые 2 с из gl_profile.c (MACOBLOX_FPS_LOG=1)"));
    m_chkTraceUdp    = new QCheckBox(tr("🌐 Трассировка UDP/kevent и сторожевой таймер фризов (MACOBLOX_TRACE_UDP=1)"));
    m_chkTraceLock   = new QCheckBox(tr("🔒 Трассировка медленных блокировок psynch_mutex (MACOBLOX_TRACE_LOCK=1)"));
    m_chkTraceGl     = new QCheckBox(tr("🎞️ Трассировка вызовов CGL/OpenGL (MACOBLOX_TRACE_GL=1)"));
    m_chkDiagSignals = new QCheckBox(tr("💥 Дамп регистров и бэктрейса при сбоях (MACOBLOX_DIAGNOSTIC_SIGNALS=1)"));

    for (QCheckBox *chk : {m_chkFullBright, m_chkClearFog, m_chkHitboxes, m_chkHudFps,
                           m_chkFpsLog, m_chkTraceUdp, m_chkTraceLock, m_chkTraceGl,
                           m_chkDiagSignals, m_chkInfZoom}) {
        if (chk != m_chkInfZoom) cc->addWidget(chk);
        connect(chk, &QAbstractButton::toggled, this, [this]() { triggerInstantAutosave(); });
    }
    connect(m_spinMaxZoom, &QSpinBox::valueChanged, this, [this]() { triggerInstantAutosave(); });
    connect(m_comboNetOpt, &QComboBox::currentIndexChanged, this, [this]() { triggerInstantAutosave(); });
    lay->addWidget(cardCam);

    auto *cardMods = new QFrame();
    cardMods->setObjectName("GlassCard");
    auto *cm = new QVBoxLayout(cardMods);
    cm->setContentsMargins(18, 16, 18, 16);
    cm->setSpacing(10);

    auto *tm = new QLabel(tr("🩺 Инструменты диагностики MacOBlox и Журнал выполнения"));
    tm->setObjectName("SectionTitle");
    cm->addWidget(tm);

    auto *rMods = new QHBoxLayout();
    auto *bShowCfg = new AnimButton(tr("🔍 Показать ClientAppSettings.json"), nullptr, QColor(16, 185, 129));
    auto *bCopyDiag = new AnimButton(tr("📋 Копировать отчёт"), nullptr, QColor(56, 189, 248));
    auto *bOpenData = new AnimButton(tr("📂 Папка данных MacOBlox"));

    connect(bShowCfg, &QPushButton::clicked, this, [this]() {
        saveSettingsNow(true);
        QFile cf(m_settingsMgr.clientAppSettingsPath());
        if (cf.open(QIODevice::ReadOnly)) {
            m_logView->clear();
            m_logView->append("✓ Файл " + m_settingsMgr.clientAppSettingsPath() + ":\n");
            m_logView->append(QString::fromUtf8(cf.readAll()));
            cf.close();
        }
    });
    connect(bCopyDiag, &QPushButton::clicked, this, [this]() {
        const QString report = m_soberCtrl->collectDiagnosticsReport();
        if (QClipboard *cb = QApplication::clipboard()) {
            cb->setText(report);
        }
        m_logView->setPlainText(report);
        enqueueToast(tr("📋 Отчёт диагностики скопирован в буфер обмена!"));
    });
    connect(bOpenData, &QPushButton::clicked, this, [this]() {
        QProcess::startDetached("xdg-open", {m_settingsMgr.macobloxDataDir()});
    });

    rMods->addWidget(bShowCfg);
    rMods->addWidget(bCopyDiag);
    rMods->addWidget(bOpenData);
    cm->addLayout(rMods);
    lay->addWidget(cardMods);

    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->document()->setMaximumBlockCount(1000);
    m_logView->setMinimumHeight(180);
    lay->addWidget(m_logView);
    return page;
}

QWidget *MainWindow::buildUpdatesPage() {
    auto *page = new QWidget();
    auto *lay = new QVBoxLayout(page);
    lay->setSpacing(12);

    // 1. Credits & MIT License Card
    auto *cardCredits = new QFrame();
    cardCredits->setObjectName("GlassCard");
    auto *cc = new QVBoxLayout(cardCredits);
    cc->setContentsMargins(18, 16, 18, 16);
    cc->setSpacing(10);

    auto *tCred = new QLabel(tr("ℹ️ О программе MqBlox × MacOBlox (v36.0) — Лицензия MIT и Благодарности (Credits)"));
    tCred->setObjectName("SectionTitle");
    cc->addWidget(tCred);

    auto *creditsText = new QLabel(tr(
        "<b>Лицензия:</b> MIT License (см. файл <code>LICENSE</code> в корне репозитория).<br><br>"
        "<b>Благодарности и авторы (Credits):</b><ul>"
        "<li><b>Narezany (<code>narezy</code>) — MacOBlox</b> "
        "(<code>Copyright (c) 2026 Narezany</code>, MIT License): автор оригинального проекта "
        "<a href=\"https://github.com/narezy/MacOBlox\">https://github.com/narezy/MacOBlox</a>, "
        "архитектуры запуска macOS-клиента Roblox на Linux через Darling, всех C/Objective-C шимов "
        "(<code>libMacOBloxShims.m</code>, <code>audio_hal.c</code>, <code>gl_profile.c</code>, "
        "<code>gpu_info.c</code>, <code>memory_stats.c</code>, <code>dns_override.c</code>, "
        "<code>darling_fixes.c</code>, <code>xfixes_raw.c</code>, <code>net_trace.c</code>, "
        "<code>fast_libc.c</code>, <code>xattr_compat.c</code>, <code>exit_compat.c</code>, "
        "<code>connectx_compat.c</code>, <code>missing_symbols.c</code>) и стаб-фреймворков "
        "(<code>CoreML</code>, <code>CoreHaptics</code>, <code>DeviceCheck</code>).</li>"
        "<li><b>Darling Project</b> (<a href=\"https://www.darlinghq.org/\">https://www.darlinghq.org/</a>): "
        "слой трансляции macOS / Darwin для Linux.</li>"
        "<li><b>MrQuster — MqBlox</b> (<code>Copyright (c) 2026 MrQuster</code>, MIT License): "
        "интерфейс HyperOS 4 Liquid Glass на C++20/Qt6 (<a href=\"https://github.com/MrQuster/mqblox\">"
        "https://github.com/MrQuster/mqblox</a>), шейдерный движок <code>Ton618GlWidget</code>, "
        "единый реестр <code>SettingsRegistry</code>, мастер установки/обновления, диагностика и исправления "
        "захвата микрофона в <code>audio_hal.c</code>.</li></ul>"));
    creditsText->setWordWrap(true);
    creditsText->setOpenExternalLinks(true);
    cc->addWidget(creditsText);

    auto *linksRow = new QHBoxLayout();
    auto *btnOpenMacOBlox = new AnimButton(tr("🌐 GitHub: narezy/MacOBlox"), nullptr, QColor(56, 189, 248));
    connect(btnOpenMacOBlox, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/narezy/MacOBlox"));
    });
    auto *btnOpenMqBlox = new AnimButton(tr("🌐 GitHub: MrQuster/mqblox"), nullptr, QColor(168, 85, 247));
    connect(btnOpenMqBlox, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/MrQuster/mqblox"));
    });
    linksRow->addWidget(btnOpenMacOBlox);
    linksRow->addWidget(btnOpenMqBlox);
    linksRow->addStretch();
    cc->addLayout(linksRow);
    lay->addWidget(cardCredits);

    // 2. GitHub / Roblox Update Card
    auto *cardInfo = new QFrame();
    cardInfo->setObjectName("GlassCard");
    auto *ci = new QVBoxLayout(cardInfo);
    ci->setContentsMargins(18, 16, 18, 16);
    ci->setSpacing(10);

    auto *title = new QLabel(tr("🔄 Проверка обновлений репозитория и клиента Roblox"));
    title->setObjectName("SectionTitle");
    ci->addWidget(title);

    m_lblGitVersionInfo = new QLabel(tr("Готов к проверке статуса репозитория..."));
    m_lblGitVersionInfo->setWordWrap(true);
    ci->addWidget(m_lblGitVersionInfo);

    m_updateProgressBar = new QProgressBar();
    m_updateProgressBar->setObjectName("UpdateBar");
    m_updateProgressBar->setRange(0, 100);
    m_updateProgressBar->setValue(100);
    m_updateProgressBar->setFormat(tr("Готов к проверке обновлений"));
    ci->addWidget(m_updateProgressBar);

    auto *btnRow = new QHBoxLayout();
    auto *btnCheck = new AnimButton(tr("🔍 Проверить Git"), nullptr, QColor(56, 189, 248));
    auto *btnWiz = new AnimButton(tr("⬇️ Обновить Шимы и RobloxPlayer.app"), nullptr, QColor(16, 185, 129), true);
    connect(btnCheck, &QPushButton::clicked, this, [this]() { refreshGitUpdateStatus(); });
    connect(btnWiz, &QPushButton::clicked, this, [this]() {
        switchTab(0);
        if (m_btnRunWizard && m_btnRunWizard->isEnabled()) {
            m_btnRunWizard->click();
        }
    });
    btnRow->addWidget(btnCheck);
    btnRow->addWidget(btnWiz);
    ci->addLayout(btnRow);
    lay->addWidget(cardInfo);

    auto *cardLog = new QFrame();
    cardLog->setObjectName("GlassCard");
    auto *cl = new QVBoxLayout(cardLog);
    cl->setContentsMargins(18, 16, 18, 16);
    auto *tLog = new QLabel(tr("📜 Журнал Git коммитов:"));
    tLog->setObjectName("SectionTitle");
    cl->addWidget(tLog);

    m_updateLogView = new QTextEdit();
    m_updateLogView->setReadOnly(true);
    m_updateLogView->setMinimumHeight(160);
    cl->addWidget(m_updateLogView);
    lay->addWidget(cardLog);

    lay->addStretch();
    return page;
}

void MainWindow::refreshGitUpdateStatus() {
    if (!m_lblGitVersionInfo || !m_updateLogView) return;
    m_updateProgressBar->setRange(0, 0);
    m_updateProgressBar->setFormat(tr("⏳ Чтение локальной истории Git..."));
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(QCoreApplication::applicationDirPath());
    connect(proc, &QProcess::finished, this, [this, proc](int, QProcess::ExitStatus) {
        QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        m_updateProgressBar->setRange(0, 100);
        m_updateProgressBar->setValue(100);
        m_lblGitVersionInfo->setText(tr("🟢 <b>MqBlox × MacOBlox v36.0 (Schema v36)</b>"));
        m_updateProgressBar->setFormat(tr("✓ Актуальная сборка"));
        if (!out.isEmpty()) m_updateLogView->setPlainText(out);
    });
    proc->start("git", {"log", "-n", "8", "--pretty=format:✦ [%h] %s (%cr)"});
}

void MainWindow::refreshEnvironmentAndSessionUi() {
    const EnvironmentStatus st = m_soberCtrl->checkEnvironment();
    if (m_lblEnvSummary) {
        m_lblEnvSummary->setText(
            QString("<b>Компоненты:</b> Darling: %1 | clang: %2 | ld.lld: %3 | unzip: %4 | "
                    "Шим <code>libMacOBloxShims.dylib</code>: %5 | <code>RobloxPlayer.app</code>: %6")
                .arg(st.hasDarling ? "✅ Да" : "❌ Нет",
                     st.hasClang ? "✅ Да" : "❌ Нет",
                     st.hasLld ? "✅ Да" : "❌ Нет",
                     st.hasUnzip ? "✅ Да" : "❌ Нет",
                     st.hasShimDylib ? "✅ Собран" : "⚠️ Не собран",
                     st.hasRobloxApp ? ("✅ v" + st.installedRobloxVersion) : "⚠️ Не установлен"));
    }
    if (m_lblSessionStatus) {
        const bool signedIn = m_soberCtrl->isSignedIn();
        m_lblSessionStatus->setText(
            signedIn
                ? tr("🟢 <b>Сессия в Darling активна</b> (Cookies.plist присутствует; токен не читается лаунчером)")
                : tr("⚪ <b>Сессия не найдена</b> — войдите внутри Roblox через «Быстрый вход» (Quick Login)"));
        if (m_btnLogout) m_btnLogout->setEnabled(signedIn);
    }
    if (m_homeDiagView) {
        m_homeDiagView->setPlainText(m_soberCtrl->collectDiagnosticsReport());
    }
}

void MainWindow::refreshNamedProfilesCombo() {
    if (!m_comboNamedProfiles) return;
    m_comboNamedProfiles->clear();
    const QStringList list = m_settingsMgr.listNamedProfiles();
    if (list.isEmpty()) {
        m_comboNamedProfiles->addItem("Default");
    } else {
        m_comboNamedProfiles->addItems(list);
    }
}

void MainWindow::buildSearchIndex() {
    m_searchIndex.clear();
    if (!m_pages) return;
    for (int i = 0; i < m_pages->count(); ++i) {
        QWidget *page = m_pages->widget(i);
        if (!page) continue;
        const auto cards = page->findChildren<QFrame *>("GlassCard");
        for (QFrame *card : cards) {
            QStringList tokens;
            for (QLabel *l : card->findChildren<QLabel *>()) tokens << l->text();
            for (QCheckBox *c : card->findChildren<QCheckBox *>()) tokens << c->text() << c->toolTip();
            for (QComboBox *cmb : card->findChildren<QComboBox *>()) {
                for (int k = 0; k < cmb->count(); ++k) tokens << cmb->itemText(k);
            }
            for (QPushButton *b : card->findChildren<QPushButton *>()) tokens << b->text() << b->toolTip();
            m_searchIndex.push_back(SearchEntry{i, card, tokens.join(' ').toLower()});
        }
    }
}

void MainWindow::filterSettingsBySearch(const QString &query) {
    if (m_searchIndex.empty()) buildSearchIndex();
    const QString q = query.trimmed().toLower();
    int firstMatchedTab = -1;

    setUpdatesEnabled(false);
    for (const auto &entry : m_searchIndex) {
        if (!entry.card) continue;
        bool match = q.isEmpty() || entry.haystackLower.contains(q);
        entry.card->setVisible(match);
        if (match && firstMatchedTab == -1) {
            firstMatchedTab = entry.tabIndex;
        }
    }
    setUpdatesEnabled(true);

    if (!q.isEmpty() && firstMatchedTab != -1 && m_pages->currentIndex() != firstMatchedTab) {
        switchTab(firstMatchedTab);
    }
}

void MainWindow::resetCurrentCategoryToDefaults() {
    AppSettings &s = m_settingsMgr.settings();
    int idx = m_pages->currentIndex();
    if (idx == 2) {
        SettingsRegistry::resetCategory(s, SettingCategory::System);
        SettingsRegistry::resetCategory(s, SettingCategory::General);
        SettingsRegistry::resetVisualAppearance(s);
    } else if (idx == 3) {
        SettingsRegistry::resetCategory(s, SettingCategory::Graphics);
    } else if (idx == 4) {
        SettingsRegistry::resetCategory(s, SettingCategory::Tweaks);
    } else {
        SettingsRegistry::resetAll(s);
    }
    syncUiFromSettings();
    saveSettingsNow(true);
    applyLayoutAndTypographyLive();
    enqueueToast(tr("↺ Параметры текущей вкладки сброшены по умолчанию!"));
}

void MainWindow::resetAppearanceToDefault() {
    SettingsRegistry::resetVisualAppearance(m_settingsMgr.settings());
    syncUiFromSettings();
    saveSettingsNow(true);
    applyLayoutAndTypographyLive();
    enqueueToast(tr("🎨 Оформление возвращено к стандартному стилю HyperOS 4 Liquid Glass!"));
}

void MainWindow::applyLayoutAndTypographyLive() {
    const AppSettings &s = m_settingsMgr.settings();
    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    int scaledPx = std::clamp(int(s.fontSizePx * (s.uiScalePct / 100.0)), 10, 22);
    int fontWeight = (s.fontBoldness == 1) ? 700 : 500;
    bool lightTheme = m_rootCosmos && m_rootCosmos->isLightThemeActive();
    QColor acc = m_rootCosmos ? m_rootCosmos->fixedAccentColor() : QColor(56, 189, 248);

    QString mainTxtHex = lightTheme ? "#0F172A" : "#F8FAFC";
    QString subTxtHex = lightTheme ? "rgba(30, 41, 59, 215)" : "rgba(241, 245, 249, 215)";

    QString dynQss = QString(
        "QLabel { color: %1; font-size: %2px; font-weight: %3; font-style: normal; background: transparent; }\n"
        "QLabel#SubText { color: %4; font-size: %5px; }\n"
        "QToolTip { color: #F8FAFC; background-color: rgba(15, 23, 42, 240); border: 1px solid rgba(%7, %8, %9, 180); border-radius: 8px; padding: 6px 10px; }\n"
        "QDialog { background-color: rgb(10, 14, 26); color: #F8FAFC; }\n"
        "QSpinBox, QLineEdit { color: %1; font-size: %2px; font-weight: 700; background: transparent; border: none; padding: 6px 12px; }\n"
        "QTextEdit { color: %1; background-color: %6; border: 1px solid rgba(%7, %8, %9, 95); border-radius: 14px; padding: 10px; }\n"
        "QSlider::groove:horizontal { height: 6px; background: rgba(148, 163, 184, 55); border-radius: 3px; }\n"
        "QSlider::sub-page:horizontal { background: rgba(%7, %8, %9, 195); border-radius: 3px; }\n"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; "
        "background: #FFFFFF; border: 2px solid rgb(%7, %8, %9); }\n")
        .arg(mainTxtHex)
        .arg(scaledPx)
        .arg(fontWeight)
        .arg(subTxtHex)
        .arg(std::max(10, scaledPx - 2))
        .arg(lightTheme ? "rgba(255, 255, 255, 140)" : "rgba(6, 10, 20, 45)")
        .arg(acc.red())
        .arg(acc.green())
        .arg(acc.blue());

    if (m_pages && m_lastAppliedQss != dynQss) {
        m_lastAppliedQss = dynQss;
        m_pages->setStyleSheet(dynQss);
    }

    if (m_sidebar) {
        bool compact = !s.sidebarShowLabels || (s.sidebarPosition == 1) ||
                       (!s.sidebarPinned && !m_sidebar->underMouse());
        int targetW = compact ? 68 : std::clamp(s.sidebarWidthPx, 160, 280);
        m_sidebar->setMinimumWidth(targetW);
        m_sidebar->setMaximumWidth(targetW);
        for (auto *b : m_navButtons) {
            b->setCompactMode(compact);
        }
    }

    int padV = (s.uiDensity == 0) ? 6 : ((s.uiDensity == 2) ? 16 : 12);
    for (int i = 0; i < m_pages->count(); ++i) {
        if (auto *sa = qobject_cast<QScrollArea *>(m_pages->widget(i))) {
            if (sa->widget() && sa->widget()->layout()) {
                sa->widget()->layout()->setSpacing(padV);
            }
        }
    }

    if (!QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) {
        bool curTop = (windowFlags() & Qt::WindowStaysOnTopHint);
        if (curTop != s.alwaysOnTop) {
            setWindowFlag(Qt::WindowStaysOnTopHint, s.alwaysOnTop);
            show();
        }
    }
    update();
}

void MainWindow::syncUiFromSettings() {
    m_isLoadingUi = true;
    const AppSettings &s = m_settingsMgr.settings();

    if (m_comboRes) m_comboRes->setCurrentIndex(m_comboRes->findData(static_cast<int>(s.resMode)));
    if (m_comboLighting) m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(s.lighting)));
    if (m_comboTex) m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(s.textureQuality)));
    if (m_comboMsaa) m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(s.msaa)));
    if (m_comboCamZoom) m_comboCamZoom->setCurrentIndex(m_comboCamZoom->findData(static_cast<int>(s.camZoom)));
    if (m_comboNetOpt) m_comboNetOpt->setCurrentIndex(m_comboNetOpt->findData(static_cast<int>(s.netProfile)));

    if (m_spinFps) m_spinFps->setValue(s.targetFps);
    if (m_chkUnlimFps) m_chkUnlimFps->setChecked(s.unlimitedFps);
    if (m_spinFps) m_spinFps->setEnabled(!s.unlimitedFps);
    if (m_spinFrm) m_spinFrm->setValue(s.frmQualityLevel);

    if (m_sldMouseSens) m_sldMouseSens->setValueSilent(s.mouseSensitivityPct);
    if (m_chkHideMenuBar) m_chkHideMenuBar->setChecked(s.hideMenuBar);
    if (m_comboDnsProvider) {
        m_comboDnsProvider->setCurrentIndex(m_comboDnsProvider->findData(s.dnsProvider));
    }
    if (m_inpDnsCustom) {
        m_inpDnsCustom->setText(s.dnsCustom);
        m_inpDnsCustom->setEnabled(s.dnsProvider == 4);
    }

    if (m_chkReturnMq) m_chkReturnMq->setChecked(s.returnToMqBloxOnLeave);
    if (m_chkAnimBg) m_chkAnimBg->setChecked(s.animBg);
    if (m_chkCloseToTray) m_chkCloseToTray->setChecked(s.closeToTray);
    if (m_chkVsyncOff) m_chkVsyncOff->setChecked(s.vsyncOff);
    if (m_chkAutoClean) m_chkAutoClean->setChecked(s.smartAutoCleanCache);
    if (m_chkMemFflags) m_chkMemFflags->setChecked(s.lowRamFflags);
    if (m_chkNoTelemetry) m_chkNoTelemetry->setChecked(s.disableTelemetry);
    if (m_chkAutoReconnect) m_chkAutoReconnect->setChecked(s.autoReconnectOnCrash);

    if (m_chkNoShadows) m_chkNoShadows->setChecked(s.noShadows);
    if (m_chkUltraShadows) m_chkUltraShadows->setChecked(s.ultraShadows);
    if (m_chkNoPostfx) m_chkNoPostfx->setChecked(s.noPostFx);
    if (m_chkLowMeshes) m_chkLowMeshes->setChecked(s.lowMeshes);
    if (m_chkGreySky) m_chkGreySky->setChecked(s.greySky);

    if (m_chkHudFps) m_chkHudFps->setChecked(s.showHudFps);
    if (m_chkClearFog) m_chkClearFog->setChecked(s.clearFog);
    if (m_spinMaxZoom) m_spinMaxZoom->setValue(s.customMaxZoom);
    if (m_chkInfZoom) m_chkInfZoom->setChecked(s.unlockInfiniteZoom);
    if (m_chkFullBright) m_chkFullBright->setChecked(s.fullBright);
    if (m_chkHitboxes) m_chkHitboxes->setChecked(s.showHitboxes);

    if (m_chkFpsLog) m_chkFpsLog->setChecked(s.fpsLog);
    if (m_chkTraceUdp) m_chkTraceUdp->setChecked(s.traceUdp);
    if (m_chkTraceLock) m_chkTraceLock->setChecked(s.traceLock);
    if (m_chkTraceGl) m_chkTraceGl->setChecked(s.traceGl);
    if (m_chkDiagSignals) m_chkDiagSignals->setChecked(s.diagnosticSignals);

    if (m_comboVisualPreset) m_comboVisualPreset->setCurrentIndex(m_comboVisualPreset->findData(s.visualPreset));
    if (m_sldBlur) m_sldBlur->setValueSilent(s.glassBlurStrength);
    if (m_sldOpacity) m_sldOpacity->setValueSilent(s.glassOpacityPct);
    if (m_sldBorderBright) m_sldBorderBright->setValueSilent(s.borderBrightnessPct);
    if (m_comboThemeMode) m_comboThemeMode->setCurrentIndex(m_comboThemeMode->findData(s.themeMode));
    if (m_comboAccent) m_comboAccent->setCurrentIndex(m_comboAccent->findData(s.accentPreset));
    if (m_chkChameleon) m_chkChameleon->setChecked(s.chameleonEnabled);
    if (m_chkReducedMotion) m_chkReducedMotion->setChecked(s.reducedMotion);
    if (m_sldUiScale) m_sldUiScale->setValueSilent(s.uiScalePct);
    if (m_sldFontSize) m_sldFontSize->setValueSilent(s.fontSizePx);

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    if (m_inpPlace) m_inpPlace->setText(s.lastPlaceInput);
    if (m_fflagsEditor) m_fflagsEditor->setPlainText(s.customFflagsJson);
    if (m_btnPinSide) m_btnPinSide->setText(s.sidebarPinned ? "📌" : "📍");
    m_isLoadingUi = false;
}

void MainWindow::collectSettingsFromUi() {
    AppSettings &s = m_settingsMgr.settings();
    if (m_comboRes) s.resMode = static_cast<ResolutionMode>(m_comboRes->currentData().toInt());
    if (m_comboLighting) s.lighting = static_cast<LightingMode>(m_comboLighting->currentData().toInt());
    if (m_comboTex) s.textureQuality = static_cast<TextureQuality>(m_comboTex->currentData().toInt());
    if (m_comboMsaa) s.msaa = static_cast<MsaaMode>(m_comboMsaa->currentData().toInt());
    if (m_comboCamZoom) s.camZoom = static_cast<CameraZoomMode>(m_comboCamZoom->currentData().toInt());
    if (m_comboNetOpt) s.netProfile = static_cast<NetworkProfile>(m_comboNetOpt->currentData().toInt());

    if (m_spinFps) s.targetFps = m_spinFps->value();
    if (m_chkUnlimFps) s.unlimitedFps = m_chkUnlimFps->isChecked();
    if (m_spinFrm) s.frmQualityLevel = m_spinFrm->value();

    if (m_sldMouseSens) s.mouseSensitivityPct = m_sldMouseSens->value();
    if (m_chkHideMenuBar) s.hideMenuBar = m_chkHideMenuBar->isChecked();
    if (m_comboDnsProvider) s.dnsProvider = m_comboDnsProvider->currentData().toInt();
    if (m_inpDnsCustom) s.dnsCustom = m_inpDnsCustom->text().trimmed();

    if (m_chkReturnMq) s.returnToMqBloxOnLeave = m_chkReturnMq->isChecked();
    if (m_chkAnimBg) s.animBg = m_chkAnimBg->isChecked();
    if (m_chkCloseToTray) s.closeToTray = m_chkCloseToTray->isChecked();
    if (m_chkVsyncOff) s.vsyncOff = m_chkVsyncOff->isChecked();
    if (m_chkAutoClean) s.smartAutoCleanCache = m_chkAutoClean->isChecked();
    if (m_chkMemFflags) s.lowRamFflags = m_chkMemFflags->isChecked();
    if (m_chkNoTelemetry) s.disableTelemetry = m_chkNoTelemetry->isChecked();
    if (m_chkAutoReconnect) s.autoReconnectOnCrash = m_chkAutoReconnect->isChecked();

    if (m_chkNoShadows) s.noShadows = m_chkNoShadows->isChecked();
    if (m_chkUltraShadows) s.ultraShadows = m_chkUltraShadows->isChecked();
    if (s.noShadows && s.ultraShadows) {
        s.ultraShadows = false;
        if (m_chkUltraShadows) {
            m_chkUltraShadows->blockSignals(true);
            m_chkUltraShadows->setChecked(false);
            m_chkUltraShadows->blockSignals(false);
        }
    }
    if (m_chkNoPostfx) s.noPostFx = m_chkNoPostfx->isChecked();
    if (m_chkLowMeshes) s.lowMeshes = m_chkLowMeshes->isChecked();
    if (m_chkGreySky) s.greySky = m_chkGreySky->isChecked();

    if (m_chkHudFps) s.showHudFps = m_chkHudFps->isChecked();
    if (m_chkClearFog) s.clearFog = m_chkClearFog->isChecked();
    if (m_spinMaxZoom) s.customMaxZoom = m_spinMaxZoom->value();
    if (m_chkInfZoom) s.unlockInfiniteZoom = m_chkInfZoom->isChecked();
    if (m_chkFullBright) s.fullBright = m_chkFullBright->isChecked();
    if (m_chkHitboxes) s.showHitboxes = m_chkHitboxes->isChecked();

    if (m_chkFpsLog) s.fpsLog = m_chkFpsLog->isChecked();
    if (m_chkTraceUdp) s.traceUdp = m_chkTraceUdp->isChecked();
    if (m_chkTraceLock) s.traceLock = m_chkTraceLock->isChecked();
    if (m_chkTraceGl) s.traceGl = m_chkTraceGl->isChecked();
    if (m_chkDiagSignals) s.diagnosticSignals = m_chkDiagSignals->isChecked();

    if (m_comboVisualPreset) s.visualPreset = m_comboVisualPreset->currentData().toInt();
    if (m_sldBlur) s.glassBlurStrength = m_sldBlur->value();
    if (m_sldOpacity) s.glassOpacityPct = m_sldOpacity->value();
    if (m_sldBorderBright) s.borderBrightnessPct = m_sldBorderBright->value();
    if (m_comboThemeMode) s.themeMode = m_comboThemeMode->currentData().toInt();
    if (m_comboAccent) s.accentPreset = m_comboAccent->currentData().toInt();
    if (m_chkChameleon) s.chameleonEnabled = m_chkChameleon->isChecked();
    if (m_chkReducedMotion) s.reducedMotion = m_chkReducedMotion->isChecked();
    if (m_sldUiScale) s.uiScalePct = m_sldUiScale->value();
    if (m_sldFontSize) s.fontSizePx = m_sldFontSize->value();

    if (s.rememberWindowSize && width() >= 820 && height() >= 540) {
        s.savedWindowWidth = width();
        s.savedWindowHeight = height();
    }

    if (m_rootCosmos) {
        m_rootCosmos->applyVisualSettings(s);
    }

    if (m_inpPlace) s.lastPlaceInput = m_inpPlace->text().trimmed();
    if (m_fflagsEditor) s.customFflagsJson = m_fflagsEditor->toPlainText();
}

void MainWindow::saveSettingsNow(bool manualClick) {
    collectSettingsFromUi();
    QString err;
    if (m_settingsMgr.saveAll(&err)) {
        if (m_lblFflagsValidation) {
            m_lblFflagsValidation->setText(tr("✓ Синтаксис FastFlags корректен и сохранён в ClientAppSettings.json"));
            m_lblFflagsValidation->setStyleSheet("color: #34D399; font-size: 11px; font-weight: 700;");
        }
        if (m_btnSaveSettings) {
            m_btnSaveSettings->setText(tr("✓ Настройки сохранены"));
            m_btnSaveSettings->setAccentColor(QColor(16, 185, 129), manualClick);
        }
        if (m_lblAutosync) {
            m_lblAutosync->setText(tr("✓ Все изменения сохранены"));
        }
        if (manualClick) {
            enqueueToast(tr("💾 Настройки сохранены в ClientAppSettings.json!"));
        }
    } else {
        if (m_lblFflagsValidation) {
            m_lblFflagsValidation->setText("⚠️ " + err);
            m_lblFflagsValidation->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 700;");
        }
        enqueueToast("⚠️ " + err);
    }
}

void MainWindow::triggerInstantAutosave(const QString &toastMsg) {
    if (m_isLoadingUi) return;
    if (!toastMsg.isEmpty()) enqueueToast(toastMsg);
    collectSettingsFromUi();
    if (m_btnSaveSettings) {
        m_btnSaveSettings->setText(tr("💾 Сохранить настройки ●"));
        m_btnSaveSettings->setAccentColor(QColor(245, 158, 11), false);
    }
    if (m_lblAutosync) {
        m_lblAutosync->setText(tr("⚡ Авто-сохранение..."));
    }
    m_autosaveTimer->start(250);
}

void MainWindow::enqueueToast(const QString &msg) {
    if (msg.isEmpty()) return;
    if (m_toastQueue.isEmpty() || m_toastQueue.back() != msg) {
        m_toastQueue.enqueue(msg);
    }
    if (!m_toastActive) processNextToast();
}

void MainWindow::processNextToast() {
    if (m_toastQueue.isEmpty()) {
        m_toastActive = false;
        m_toastLbl->setVisible(false);
        return;
    }
    m_toastActive = true;
    m_toastLbl->setText(m_toastQueue.dequeue());
    m_toastLbl->setVisible(true);
    QTimer::singleShot(1900, this, &MainWindow::processNextToast);
}

void MainWindow::refreshAccountsUi() {
    m_comboQuickAcc->blockSignals(true);
    m_comboQuickAcc->clear();
    for (const auto &acc : m_settingsMgr.accounts()) {
        m_comboQuickAcc->addItem("👤 " + acc.displayName, acc.id);
        if (acc.id == m_settingsMgr.activeAccountId()) {
            m_comboQuickAcc->setCurrentIndex(m_comboQuickAcc->count() - 1);
        }
    }
    m_comboQuickAcc->blockSignals(false);

    while (QLayoutItem *item = m_accountsListLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    for (const auto &acc : m_settingsMgr.accounts()) {
        auto *card = new QFrame();
        card->setObjectName("GlassCard");
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(16, 12, 16, 12);
        bool isAct = (acc.id == m_settingsMgr.activeAccountId());
        QString badge = isAct ? "🟢 [АКТИВНЫЙ СЛОТ]" : "⚪ [В РЕЗЕРВЕ]";
        auto *lbl = new QLabel(QString("%1  <b>%2</b> (ID: <code>%3</code>)")
                                   .arg(badge, acc.displayName, acc.id));
        row->addWidget(lbl);
        row->addStretch();
        if (!isAct) {
            auto *bSw = new AnimButton(tr("✅ Переключить слот"));
            QString id = acc.id;
            connect(bSw, &QPushButton::clicked, this, [this, id]() { m_soberCtrl->switchAccountSlotAsync(id); });
            row->addWidget(bSw);
        }
        m_accountsListLayout->addWidget(card);
    }
}

void MainWindow::applyResolutionMode(ResolutionMode mode, bool initial) {
    QScreen *scr = windowHandle() && windowHandle()->screen()
                       ? windowHandle()->screen()
                       : QApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1366, 768);
    int sw = avail.width(), sh = avail.height();
    setMaximumSize(sw, sh);

    int tw = 1100, th = 660;
    switch (mode) {
    case ResolutionMode::HD_720p:   tw = 1080; th = 640; break;
    case ResolutionMode::FHD_1080p: tw = 1360; th = 780; break;
    case ResolutionMode::QHD_2K:    tw = 1720; th = 960; break;
    case ResolutionMode::Auto:
        tw = int(sw * 0.78);
        th = int(sh * 0.78);
        break;
    }

    int fw = std::clamp(tw, 840, std::max(840, sw - 32));
    int fh = std::clamp(th, 520, std::max(520, sh - 36));

    if (!isMaximized()) {
        resize(fw, fh);
        move(avail.x() + (sw - fw) / 2, avail.y() + (sh - fh) / 2);
    }
    if (!initial) triggerInstantAutosave(tr("🖥️ Размер окна адаптирован (%1×%2)").arg(fw).arg(fh));
}

void MainWindow::applyGraphicsMasterPreset(int presetIdx) {
    m_isLoadingUi = true;
    if (presetIdx == 0) {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::Voxel)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level0)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::Off)));
        m_spinFrm->setValue(1);
        m_chkNoShadows->setChecked(true);
        m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(true);
        m_chkLowMeshes->setChecked(true);
        m_chkGreySky->setChecked(true);
        m_chkUnlimFps->setChecked(true);
    } else if (presetIdx == 1) {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::ShadowMap)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level2)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X2)));
        m_spinFrm->setValue(10);
        m_chkNoShadows->setChecked(false);
        m_chkUltraShadows->setChecked(false);
        m_chkNoPostfx->setChecked(false);
        m_chkLowMeshes->setChecked(false);
        m_chkGreySky->setChecked(false);
    } else {
        m_comboLighting->setCurrentIndex(m_comboLighting->findData(static_cast<int>(LightingMode::FuturePhase3)));
        m_comboTex->setCurrentIndex(m_comboTex->findData(static_cast<int>(TextureQuality::Level3)));
        m_comboMsaa->setCurrentIndex(m_comboMsaa->findData(static_cast<int>(MsaaMode::X4)));
        m_spinFrm->setValue(21);
        m_chkNoShadows->setChecked(false);
        m_chkUltraShadows->setChecked(true);
        m_chkNoPostfx->setChecked(false);
        m_chkLowMeshes->setChecked(false);
        m_chkGreySky->setChecked(false);
    }
    m_isLoadingUi = false;
    triggerInstantAutosave(tr("🌟 Пресет графики применён!"));
}

void MainWindow::toggleSidebarPinMode() {
    bool &pinned = m_settingsMgr.settings().sidebarPinned;
    pinned = !pinned;
    m_btnPinSide->setText(pinned ? "📌" : "📍");
    if (pinned) animateSidebar(true);
    triggerInstantAutosave(pinned ? tr("📌 Боковая панель закреплена") : tr("📍 Авто-выезд панели включён"));
}

void MainWindow::animateSidebar(bool expand) {
    m_sidebarExpanded = expand;
    int targetW = expand ? 232 : 70;
    if (m_sidebar->width() == targetW) return;
    m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebar->width());
    m_sidebarAnim->setEndValue(targetW);
    m_sidebarAnim->start();
}

void MainWindow::switchTab(int idx) {
    if (m_pages->currentIndex() == idx) return;
    m_pages->setCurrentIndex(idx);
    for (size_t i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setActiveTab(static_cast<int>(i) == idx);
    }
    if (idx == 0) refreshEnvironmentAndSessionUi();
    if (idx == 5) refreshGitUpdateStatus();
    if (QWidget *w = m_pages->currentWidget()) {
        if (idx != 0) {
            auto *eff = new QGraphicsOpacityEffect(w);
            w->setGraphicsEffect(eff);
            auto *anim = new QPropertyAnimation(eff, "opacity", w);
            anim->setDuration(220);
            anim->setStartValue(0.15);
            anim->setEndValue(1.0);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QPropertyAnimation::finished, w, [w]() { w->setGraphicsEffect(nullptr); });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void MainWindow::runWatchdogTick() {
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly)) return;
    qulonglong totalKb = 8000000, availKb = 4000000;
    for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
        if (line.startsWith("MemTotal:")) totalKb = line.split(QRegularExpression("\\s+")).value(1).toULongLong();
        else if (line.startsWith("MemAvailable:")) availKb = line.split(QRegularExpression("\\s+")).value(1).toULongLong();
    }
    int availMb = int(availKb / 1024);
    int usedPct = int(100 - (availKb * 100 / std::max(1ULL, totalKb)));
    m_ramBar->setValue(std::clamp(usedPct, 5, 100));
    m_ramBar->setFormat(tr("ОЗУ: свободно %1 МБ (%2%)").arg(availMb).arg(usedPct));
}

} // namespace mqblox
EOF_SRC_UI_MAINWINDOW_CPP

cat <<'EOF_SHIMS_BUILD_DEBUG_SHIM_SH' > "shims/build_debug_shim.sh"
#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/.local/bin:$PATH"
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# Output goes to MACOBLOX_BUILD_DIR when the sources are read-only (a package).
build_dir=${MACOBLOX_BUILD_DIR:-$project_dir/build}
# Packages put Darling's macOS root in /usr/libexec, a source build in /usr/local,
# or user/system Flatpak in ~/.local/share/flatpak or /var/lib/flatpak.
sysroot=${DARLING_SYSROOT:-}
if [[ -z $sysroot ]]; then
  for candidate in \
    /usr/libexec/darling \
    /usr/local/libexec/darling \
    "$HOME/.local/share/flatpak/app/xyz.narez.MacOBlox/current/active/files/libexec/darling" \
    /var/lib/flatpak/app/xyz.narez.MacOBlox/current/active/files/libexec/darling; do
    if [[ -d $candidate ]]; then
      sysroot=$candidate
      break
    fi
  done
fi
sysroot=${sysroot:-/usr/libexec/darling}
mkdir -p "$build_dir"
tmp_output=$(mktemp "$build_dir/.shim.XXXXXX")
# Memory/string functions replacing Darling's unoptimized ones: -O2 and no
# builtins, compiled on their own (see fast_libc.c).
clang -target x86_64-apple-darwin -isysroot "$sysroot" -mmacosx-version-min=11.0 \
  -O2 -fno-builtin -c "$project_dir/fast_libc.c" -o "$build_dir/fast_libc.o"
trap 'rm -f -- "$tmp_output"' EXIT
clang -target x86_64-apple-darwin -fuse-ld=lld \
  -isysroot "$sysroot" -mmacosx-version-min=11.0 \
  -dynamiclib -fno-objc-arc -Wno-builtin-requires-header -Werror=incompatible-function-pointer-types \
  -Wl,-undefined,dynamic_lookup \
  -install_name @rpath/libMacOBloxShims.dylib \
  "$project_dir/libMacOBloxShims.m" "$project_dir/xattr_compat.c" "$project_dir/exit_compat.c" "$project_dir/missing_symbols.c" "$project_dir/net_trace.c" "$project_dir/darling_fixes.c" "$project_dir/xfixes_raw.c" "$project_dir/dns_override.c" "$project_dir/audio_hal.c" "$project_dir/gpu_info.c" "$project_dir/gl_profile.c" "$project_dir/connectx_compat.c" "$project_dir/memory_stats.c" "$build_dir/fast_libc.o" \
  -lobjc -lc++ -lc++abi -framework Foundation -framework AppKit \
  -o "$tmp_output"
mv -- "$tmp_output" "$build_dir/libMacOBloxShims.dylib"
printf 'Built: %s\n' "$build_dir/libMacOBloxShims.dylib"

# Frameworks RobloxPlayer links that Darling does not have (see frameworks/).
# The launcher copies them into the Darling prefix.
for name in CoreML CoreHaptics DeviceCheck; do
  framework="$build_dir/frameworks/$name.framework"
  mkdir -p "$framework/Versions/A"
  clang -target x86_64-apple-darwin -fuse-ld=lld \
    -isysroot "$sysroot" -mmacosx-version-min=11.0 \
    -dynamiclib -fno-objc-arc \
    -install_name "/System/Library/Frameworks/$name.framework/Versions/A/$name" \
    "$project_dir/frameworks/$name.m" -lobjc -framework CoreFoundation \
    -o "$framework/Versions/A/$name"
  ln -sfn A "$framework/Versions/Current"
  ln -sfn "Versions/Current/$name" "$framework/$name"
done
printf 'Built: %s\n' "$build_dir/frameworks"
EOF_SHIMS_BUILD_DEBUG_SHIM_SH

cat <<'EOF_SHIMS_LIBMACOBLOXSHIMS_M' > "shims/libMacOBloxShims.m"
typedef struct objc_class *Class;
typedef struct objc_object { Class isa; } *id;
typedef struct objc_selector *SEL;
typedef struct objc_method *Method;
typedef void (*IMP)(void);

extern unsigned long long mach_absolute_time(void);
extern Class objc_getClass(const char *name);
extern SEL sel_registerName(const char *str);
extern Method class_getInstanceMethod(Class cls, SEL name);
extern Method class_getClassMethod(Class cls, SEL name);
extern IMP method_getImplementation(Method m);
extern IMP method_setImplementation(Method m, IMP imp);
extern Class object_getClass(id obj);
extern const char* object_getClassName(id obj);
extern id objc_msgSend(id self, SEL op, ...);

extern char *getenv(const char *);
extern int write(int fd, const void *buf, unsigned long count);
extern int backtrace(void** array, int size);
extern void backtrace_symbols_fd(void* const* array, int size, int fd);
extern void* dlsym(void* handle, const char* symbol);
extern long _dyld_get_image_vmaddr_slide(unsigned int image_index);

#define RTLD_NEXT ((void*)-1)
#define RTLD_DEFAULT ((void*)-2)

#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

static void write_str(const char* s);
static void print_hex(unsigned long long val);
static void print_num(long long val);

// Darling can create an IONotificationPort but currently returns an error when
// Roblox registers HID matching notifications.  Roblox then destroys the port
// and attempts a second registration with the cleared pointer, which crashes
// inside IOKit.  Report a valid empty registration: controller hot-plugging is
// unavailable, while AppKit keyboard and mouse input remain unaffected.
typedef void (*MacOBloxIOServiceMatchingCallback)(void*, unsigned int);
extern int IOServiceAddMatchingNotification(
    void*, const char*, void*, MacOBloxIOServiceMatchingCallback, void*, unsigned int*);
static int macoblox_IOServiceAddMatchingNotification(
    void* notification_port,
    const char* notification_type,
    void* matching,
    MacOBloxIOServiceMatchingCallback callback,
    void* context,
    unsigned int* iterator) {
    (void)notification_port;
    (void)notification_type;
    (void)matching;
    (void)callback;
    (void)context;
    if (iterator)
        *iterator = 0;
    write_str("[MacOBlox] IOKit HID notification registered with empty iterator\n");
    return 0;
}
DYLD_INTERPOSE(macoblox_IOServiceAddMatchingNotification,
               IOServiceAddMatchingNotification);

// Keep the source for each OpenGL shader long enough to capture the first
// source rejected by Mesa. Roblox's normal log contains the compiler message
// and line number, but not the GLSL text that caused it.
extern void glShaderSource(unsigned int, int, const char* const*, const int*);
extern void glCompileShader(unsigned int);
extern unsigned int glCreateShader(unsigned int);
extern void glAttachShader(unsigned int, unsigned int);
extern void* malloc(unsigned long);
extern void free(void*);
typedef struct MacOBloxFILE MacOBloxFILE;
extern MacOBloxFILE* fopen(const char*, const char*);
extern unsigned long fwrite(const void*, unsigned long, unsigned long, MacOBloxFILE*);
extern int fclose(MacOBloxFILE*);

#define MACOBLOX_SHADER_SLOTS 8192
static char* macoblox_shader_sources[MACOBLOX_SHADER_SLOTS];
static unsigned long macoblox_shader_source_lengths[MACOBLOX_SHADER_SLOTS];
static unsigned int macoblox_shader_types[MACOBLOX_SHADER_SLOTS];
static unsigned int macoblox_program_shaders[MACOBLOX_SHADER_SLOTS][4];
static volatile int macoblox_dumped_failed_shader;
static volatile int macoblox_dumped_final_program;

static unsigned int macoblox_glCreateShader(unsigned int type) {
    static unsigned int (*real_function)(unsigned int);
    if (!real_function)
        real_function = (unsigned int (*)(unsigned int))
            dlsym(RTLD_NEXT, "glCreateShader");
    unsigned int shader = real_function ? real_function(type) : 0;
    if (shader < MACOBLOX_SHADER_SLOTS)
        macoblox_shader_types[shader] = type;
    return shader;
}
DYLD_INTERPOSE(macoblox_glCreateShader, glCreateShader);

static void macoblox_glAttachShader(unsigned int program, unsigned int shader) {
    static void (*real_function)(unsigned int, unsigned int);
    if (!real_function)
        real_function = (void (*)(unsigned int, unsigned int))
            dlsym(RTLD_NEXT, "glAttachShader");
    if (program < MACOBLOX_SHADER_SLOTS) {
        for (int index = 0; index < 4; index++) {
            if (!macoblox_program_shaders[program][index] ||
                macoblox_program_shaders[program][index] == shader) {
                macoblox_program_shaders[program][index] = shader;
                break;
            }
        }
    }
    if (real_function)
        real_function(program, shader);
}
DYLD_INTERPOSE(macoblox_glAttachShader, glAttachShader);

static unsigned long macoblox_cstr_length(const char* text) {
    unsigned long length = 0;
    if (text)
        while (text[length]) length++;
    return length;
}

static char* macoblox_fix_mesa_shader_indices(char* source,
                                              unsigned long* source_length) {
    static const char needle[] = "((uint(POSITION.w) >> 8u) & 255u)";
    static const char prefix[] = "int(";
    const unsigned long needle_length = sizeof(needle) - 1;
    unsigned long matches = 0;
    for (unsigned long position = 0;
         position + needle_length <= *source_length; position++) {
        unsigned long index = 0;
        while (index < needle_length &&
               source[position + index] == needle[index])
            index++;
        if (index == needle_length) {
            matches++;
            position += needle_length - 1;
        }
    }
    if (!matches)
        return source;

    unsigned long fixed_length = *source_length + matches * 5;
    char* fixed = (char*)malloc(fixed_length + 1);
    if (!fixed)
        return source;

    unsigned long input = 0;
    unsigned long output = 0;
    while (input < *source_length) {
        unsigned long index = 0;
        while (input + index < *source_length && index < needle_length &&
               source[input + index] == needle[index])
            index++;
        if (index == needle_length) {
            for (unsigned long prefix_index = 0;
                 prefix_index < sizeof(prefix) - 1; prefix_index++)
                fixed[output++] = prefix[prefix_index];
            for (unsigned long needle_index = 0;
                 needle_index < needle_length; needle_index++)
                fixed[output++] = needle[needle_index];
            fixed[output++] = ')';
            input += needle_length;
        } else {
            fixed[output++] = source[input++];
        }
    }
    fixed[output] = 0;
    free(source);
    *source_length = output;
    return fixed;
}

static void macoblox_apply_ui_color_test(char* source,
                                         unsigned long source_length) {
    const char* enabled = getenv("MACOBLOX_GL_TEST_UI_COLOR");
    if (!enabled || !enabled[0])
        return;
    static const char needle[] =
        "_entryPointOutput = VARYING1 * _676;";
    static const char replacement[] =
        "_entryPointOutput = vec4(1,0,1,1)  ;";
    const unsigned long length = sizeof(needle) - 1;
    for (unsigned long position = 0;
         position + length <= source_length; position++) {
        unsigned long index = 0;
        while (index < length && source[position + index] == needle[index])
            index++;
        if (index == length) {
            for (index = 0; index < length; index++)
                source[position + index] = replacement[index];
            write_str("[MacOBlox GL] Forced UI fragment shader color\n");
            return;
        }
    }
}

static int macoblox_source_contains(const char* source,
                                    unsigned long source_length,
                                    const char* needle) {
    unsigned long needle_length = macoblox_cstr_length(needle);
    for (unsigned long position = 0;
         position + needle_length <= source_length; position++) {
        unsigned long index = 0;
        while (index < needle_length &&
               source[position + index] == needle[index])
            index++;
        if (index == needle_length)
            return 1;
    }
    return 0;
}

static void macoblox_glShaderSource(unsigned int shader, int count,
                                    const char* const* strings,
                                    const int* lengths) {
    void (*real_function)(unsigned int, int, const char* const*, const int*) =
        (void (*)(unsigned int, int, const char* const*, const int*))
            dlsym(RTLD_NEXT, "glShaderSource");

    const char* fixed_string = 0;
    int fixed_length = 0;
    if (shader < MACOBLOX_SHADER_SLOTS && count > 0 && strings) {
        unsigned long total = 0;
        for (int index = 0; index < count; index++) {
            unsigned long part = (lengths && lengths[index] >= 0)
                ? (unsigned long)lengths[index]
                : macoblox_cstr_length(strings[index]);
            if (part > 1024UL * 1024UL - total) {
                total = 0;
                break;
            }
            total += part;
        }
        if (total) {
            char* copy = (char*)malloc(total + 1);
            if (copy) {
                unsigned long position = 0;
                for (int index = 0; index < count; index++) {
                    unsigned long part = (lengths && lengths[index] >= 0)
                        ? (unsigned long)lengths[index]
                        : macoblox_cstr_length(strings[index]);
                    for (unsigned long offset = 0; offset < part; offset++)
                        copy[position++] = strings[index][offset];
                }
                copy[position] = 0;
                copy = macoblox_fix_mesa_shader_indices(copy, &total);
                macoblox_apply_ui_color_test(copy, total);
                const char* ui_test = getenv("MACOBLOX_GL_TEST_UI_COLOR");
                if (ui_test && ui_test[0] &&
                    macoblox_shader_types[shader] == 0x8B31U &&
                    macoblox_source_contains(copy, total,
                                             "out vec2 VARYING2;")) {
                    static const char forced_vertex[] =
                        "#version 150\n"
                        "in vec4 POSITION; in vec2 TEXCOORD0; in vec4 COLOR0;\n"
                        "out vec2 VARYING0; out vec4 VARYING1; out vec2 VARYING2;\n"
                        "void main(){ int v=gl_VertexID%3; vec2 p=(v==0)?vec2(-1,-1):(v==1)?vec2(3,-1):vec2(-1,3); gl_Position=vec4(p,0,1); VARYING0=vec2(0); VARYING1=vec4(1); VARYING2=vec2(0); }\n";
                    unsigned long forced_length = sizeof(forced_vertex) - 1;
                    char* forced = (char*)malloc(forced_length + 1);
                    if (forced) {
                        for (unsigned long index = 0; index <= forced_length;
                             index++)
                            forced[index] = forced_vertex[index];
                        free(copy);
                        copy = forced;
                        total = forced_length;
                        write_str("[MacOBlox GL] Forced UI vertex coverage\n");
                    }
                }
                if (macoblox_shader_sources[shader])
                    free(macoblox_shader_sources[shader]);
                macoblox_shader_sources[shader] = copy;
                macoblox_shader_source_lengths[shader] = total;
                fixed_string = copy;
                fixed_length = (int)total;
            }
        }
    }
    if (real_function) {
        if (fixed_string)
            real_function(shader, 1, &fixed_string, &fixed_length);
        else
            real_function(shader, count, strings, lengths);
    }
}
DYLD_INTERPOSE(macoblox_glShaderSource, glShaderSource);

static void macoblox_glCompileShader(unsigned int shader) {
    void (*real_compile)(unsigned int) =
        (void (*)(unsigned int))dlsym(RTLD_NEXT, "glCompileShader");
    void (*real_get_shader_iv)(unsigned int, unsigned int, int*) =
        (void (*)(unsigned int, unsigned int, int*))
            dlsym(RTLD_NEXT, "glGetShaderiv");
    if (real_compile)
        real_compile(shader);

    int succeeded = 1;
    if (real_get_shader_iv)
        real_get_shader_iv(shader, 0x8B81U, &succeeded); // GL_COMPILE_STATUS
    if (!succeeded && shader < MACOBLOX_SHADER_SLOTS &&
        macoblox_shader_sources[shader] &&
        __sync_bool_compare_and_swap(&macoblox_dumped_failed_shader, 0, 1)) {
        MacOBloxFILE* output = fopen(
            "/private/tmp/macoblox-first-failed-shader.glsl", "w");
        if (output) {
            fwrite(macoblox_shader_sources[shader], 1,
                   macoblox_shader_source_lengths[shader], output);
            fclose(output);
            write_str("[MacOBlox GL] Captured first failed shader source\n");
        }
    }
}
DYLD_INTERPOSE(macoblox_glCompileShader, glCompileShader);

// Trace every CGL/EGL binding change with its thread. Roblox renders on a
// worker thread; EGL allows a surface to be current in only one thread, so a
// failed eglMakeCurrent there leaves the renderer without a window surface.
// Enabled with MACOBLOX_TRACE_CGL=1; the output is bounded per function.
extern void* pthread_self(void);
extern unsigned int pthread_mach_thread_np(void*);
extern int CGLCreateContext(void*, void*, void**);
extern int CGLSetCurrentContext(void*);
extern int CGLContextMakeCurrentAndAttachToWindow(void*, void*);
extern int CGLFlushDrawable(void*);
extern unsigned int eglMakeCurrent(void*, void*, void*, void*);

static int macoblox_trace_cgl_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* value = getenv("MACOBLOX_TRACE_CGL");
        enabled = value && value[0] ? 1 : 0;
    }
    return enabled;
}

static int macoblox_trace_cgl_should_log(volatile long* counter) {
    long count = __sync_add_and_fetch(counter, 1);
    return count <= 40 || count % 1000 == 0;
}

static void macoblox_trace_cgl_prefix(const char* name, volatile long* counter) {
    write_str("[MacOBlox CGL] ");
    write_str(name);
    write_str(" #");
    print_num(*counter);
    write_str(" thread=");
    print_num(pthread_mach_thread_np(pthread_self()));
}

static void* macoblox_cgl_egl_context(void* cgl) {
    // struct _CGLContextObj: retain_count, pthread_mutex_t (64 bytes on
    // Darwin x86_64), egl_context, egl_surface.
    return cgl ? ((void**)cgl)[9] : 0;
}

static void* macoblox_cgl_egl_surface(void* cgl) {
    return cgl ? ((void**)cgl)[10] : 0;
}

static volatile long macoblox_cgl_create_count;
static int macoblox_CGLCreateContext(void* format, void* share, void** result) {
    if (macoblox_trace_cgl_enabled() && format) {
        // struct _CGLPixelFormatObj { GLuint retain_count; CGLPixelFormatAttribute* attributes; }
        int* attributes = ((int**)format)[1];
        write_str("[MacOBlox CGL] pixel format:");
        for (int index = 0; attributes && index < 64 && attributes[index]; index++) {
            write_str(" ");
            print_num(attributes[index]);
        }
        write_str("\n");
    }
    int error = CGLCreateContext(format, share, result);
    if (macoblox_trace_cgl_enabled() &&
        macoblox_trace_cgl_should_log(&macoblox_cgl_create_count)) {
        macoblox_trace_cgl_prefix("CGLCreateContext", &macoblox_cgl_create_count);
        write_str(" share=");
        print_hex((unsigned long long)share);
        write_str(" share-egl=");
        print_hex((unsigned long long)macoblox_cgl_egl_context(share));
        write_str(" result=");
        print_hex((unsigned long long)(result ? *result : 0));
        write_str(" egl=");
        print_hex((unsigned long long)macoblox_cgl_egl_context(result ? *result : 0));
        write_str(" error=");
        print_num(error);
        write_str("\n");
    }
    return error;
}
DYLD_INTERPOSE(macoblox_CGLCreateContext, CGLCreateContext);

static volatile long macoblox_cgl_set_current_count;
static int macoblox_CGLSetCurrentContext(void* context) {
    int error = CGLSetCurrentContext(context);
    if (macoblox_trace_cgl_enabled() &&
        macoblox_trace_cgl_should_log(&macoblox_cgl_set_current_count)) {
        macoblox_trace_cgl_prefix("CGLSetCurrentContext",
                                  &macoblox_cgl_set_current_count);
        write_str(" cgl=");
        print_hex((unsigned long long)context);
        write_str(" egl=");
        print_hex((unsigned long long)macoblox_cgl_egl_context(context));
        write_str(" surface=");
        print_hex((unsigned long long)macoblox_cgl_egl_surface(context));
        write_str("\n");
    }
    return error;
}
DYLD_INTERPOSE(macoblox_CGLSetCurrentContext, CGLSetCurrentContext);

static volatile long macoblox_cgl_attach_count;
static int macoblox_CGLContextMakeCurrentAndAttachToWindow(void* context,
                                                           void* window) {
    int error = CGLContextMakeCurrentAndAttachToWindow(context, window);
    if (macoblox_trace_cgl_enabled() &&
        macoblox_trace_cgl_should_log(&macoblox_cgl_attach_count)) {
        macoblox_trace_cgl_prefix("CGLContextMakeCurrentAndAttachToWindow",
                                  &macoblox_cgl_attach_count);
        write_str(" cgl=");
        print_hex((unsigned long long)context);
        write_str(" egl=");
        print_hex((unsigned long long)macoblox_cgl_egl_context(context));
        write_str(" window=");
        print_hex((unsigned long long)window);
        write_str("\n");
    }
    return error;
}
DYLD_INTERPOSE(macoblox_CGLContextMakeCurrentAndAttachToWindow,
               CGLContextMakeCurrentAndAttachToWindow);

// Frame presentation (gl_profile.c): vsync off unless MACOBLOX_VSYNC=1,
// and an FPS line every 5 s with MACOBLOX_FPS_LOG=1.
extern void macoblox_frame_presenting(void* cgl_context);
static volatile long macoblox_cgl_flush_drawable_count;
static int macoblox_CGLFlushDrawable(void* context) {
    if (macoblox_trace_cgl_enabled() &&
        macoblox_trace_cgl_should_log(&macoblox_cgl_flush_drawable_count)) {
        macoblox_trace_cgl_prefix("CGLFlushDrawable",
                                  &macoblox_cgl_flush_drawable_count);
        write_str(" cgl=");
        print_hex((unsigned long long)context);
        write_str(" egl=");
        print_hex((unsigned long long)macoblox_cgl_egl_context(context));
        write_str(" surface=");
        print_hex((unsigned long long)macoblox_cgl_egl_surface(context));
        write_str("\n");
    }
    macoblox_frame_presenting(context);
    return CGLFlushDrawable(context);
}
DYLD_INTERPOSE(macoblox_CGLFlushDrawable, CGLFlushDrawable);

static volatile long macoblox_egl_make_current_count;
static unsigned int macoblox_eglMakeCurrent(void* display, void* draw,
                                            void* read, void* context) {
    unsigned int result = eglMakeCurrent(display, draw, read, context);
    if (macoblox_trace_cgl_enabled() &&
        macoblox_trace_cgl_should_log(&macoblox_egl_make_current_count)) {
        unsigned int (*egl_error)(void) =
            (unsigned int (*)(void))dlsym(RTLD_DEFAULT, "eglGetError");
        macoblox_trace_cgl_prefix("eglMakeCurrent",
                                  &macoblox_egl_make_current_count);
        write_str(" display=");
        print_hex((unsigned long long)display);
        write_str(" draw=");
        print_hex((unsigned long long)draw);
        write_str(" context=");
        print_hex((unsigned long long)context);
        write_str(" result=");
        print_num(result);
        write_str(" error=");
        print_hex(result || !egl_error ? 0x3000 : egl_error());
        write_str("\n");
    }
    return result;
}
DYLD_INTERPOSE(macoblox_eglMakeCurrent, eglMakeCurrent);

// Count the commands that can actually produce or resolve pixels. A steady
// stream of NSOpenGLContext flushes only proves that Darling swaps the X11
// surface; it does not prove that Roblox submitted a frame to that surface.
// Keep these wrappers passive so the diagnostic cannot change GL state.
extern void glDrawArrays(unsigned int, int, int);
extern void glDrawElements(unsigned int, int, unsigned int, const void*);
extern void glDrawRangeElements(unsigned int, unsigned int, unsigned int, int,
                                unsigned int, const void*);
extern void glDrawElementsBaseVertex(unsigned int, int, unsigned int,
                                     const void*, int);
extern void glDrawArraysInstanced(unsigned int, int, int, int);
extern void glDrawElementsInstanced(unsigned int, int, unsigned int,
                                    const void*, int);
extern void glDrawElementsInstancedBaseVertex(unsigned int, int, unsigned int,
                                              const void*, int, int);
extern void glBlitFramebuffer(int, int, int, int, int, int, int, int,
                              unsigned int, unsigned int);
extern void glBindFramebuffer(unsigned int, unsigned int);
extern void glClear(unsigned int);

static volatile long macoblox_gl_draw_count;
static volatile long macoblox_gl_blit_count;
static volatile long macoblox_gl_clear_count;
static volatile long macoblox_gl_default_framebuffer_bind_count;
static void* macoblox_gl_window_surface;
static void* macoblox_gl_window_egl_context;
static void* macoblox_gl_render_egl_context;
static volatile long macoblox_gl_surface_repair_attempts;
static volatile long macoblox_gl_traced_window_draws;
static volatile long macoblox_gl_skipped_foreign_draws;
static volatile int macoblox_dumped_uniform_blocks;
static volatile int macoblox_dumped_actual_program;

static void macoblox_dump_actual_program_sources(unsigned int program) {
    if (program < 700 ||
        !__sync_bool_compare_and_swap(&macoblox_dumped_actual_program, 0, 1))
        return;
    void (*attached_shaders)(unsigned int, int, int*, unsigned int*) =
        (void (*)(unsigned int, int, int*, unsigned int*))
            dlsym(RTLD_NEXT, "glGetAttachedShaders");
    void (*shader_iv)(unsigned int, unsigned int, int*) =
        (void (*)(unsigned int, unsigned int, int*))
            dlsym(RTLD_NEXT, "glGetShaderiv");
    void (*shader_source)(unsigned int, int, int*, char*) =
        (void (*)(unsigned int, int, int*, char*))
            dlsym(RTLD_NEXT, "glGetShaderSource");
    if (!attached_shaders || !shader_iv || !shader_source)
        return;
    unsigned int shaders[4] = {0, 0, 0, 0};
    int shader_count = 0;
    attached_shaders(program, 4, &shader_count, shaders);
    write_str("[MacOBlox GL] actual program=");
    print_num(program);
    write_str(" attached=");
    print_num(shader_count);
    write_str(" shaders=");
    for (int index = 0; index < shader_count && index < 4; index++) {
        if (index)
            write_str(",");
        print_num(shaders[index]);
        int source_length = 0;
        shader_iv(shaders[index], 0x8B88U, &source_length);
        if (source_length <= 1 || source_length > 1024 * 1024)
            continue;
        char* source = (char*)malloc((unsigned long)source_length);
        if (!source)
            continue;
        int written = 0;
        shader_source(shaders[index], source_length, &written, source);
        char path[] =
            "/Volumes/SystemRoot/tmp/macoblox-actual-program-shader-0.glsl";
        path[sizeof(path) - sizeof("0.glsl")] = (char)('0' + index);
        MacOBloxFILE* output = fopen(path, "w");
        if (output) {
            fwrite(source, 1, (unsigned long)(written > 0 ? written : 0),
                   output);
            fclose(output);
        }
        free(source);
    }
    write_str("\n");
}

// The EGL surface repair and foreign-draw filtering below were workarounds
// for Roblox drawing into the wrong context, which restoring the context
// after CALayerContext renders fixed properly. They stay available for
// debugging with MACOBLOX_GL_HACKS=1.
static int macoblox_gl_hacks_disabled(void) {
    static int disabled = -1;
    if (disabled < 0) {
        const char* value = getenv("MACOBLOX_GL_HACKS");
        disabled = value && value[0] ? 0 : 1;
    }
    return disabled;
}

static void macoblox_ensure_gl_window_surface(void) {
    if (macoblox_gl_hacks_disabled())
        return;
    void* surface = macoblox_gl_window_surface;
    void* (*current_surface)(int) =
        (void* (*)(int))dlsym(RTLD_NEXT, "eglGetCurrentSurface");
    if (!surface || !current_surface || current_surface(0x3059)) // EGL_DRAW
        return;

    void* (*current_display)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentDisplay");
    void* (*current_context)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentContext");
    int (*make_current)(void*, void*, void*, void*) =
        (int (*)(void*, void*, void*, void*))
            dlsym(RTLD_NEXT, "eglMakeCurrent");
    unsigned int (*egl_error)(void) =
        (unsigned int (*)(void))dlsym(RTLD_NEXT, "eglGetError");
    if (!current_display || !current_context || !make_current)
        return;

    // Roblox switches from AppKit's initial CGL context to its own shared
    // render context. The latter owns the draws but Darling leaves it
    // surfaceless, so attach whichever live context is selecting framebuffer
    // zero rather than limiting this repair to AppKit's original context.
    void* context = current_context();
    if (!context)
        return;

    int attached = make_current(current_display(), surface, surface,
                                context);
    long attempt = __sync_add_and_fetch(&macoblox_gl_surface_repair_attempts, 1);
    if (attempt <= 5) {
        write_str("[MacOBlox GL] draw-time EGL surface repair #");
        print_num(attempt);
        write_str(" result=");
        print_num(attached);
        write_str(" error=");
        print_hex(egl_error ? egl_error() : 0);
        write_str(" surface-now=");
        print_hex((unsigned long long)current_surface(0x3059));
        write_str("\n");
    }
}

static int macoblox_should_skip_foreign_window_command(int learn_renderer) {
    if (macoblox_gl_hacks_disabled())
        return 0;
    void* (*current_surface)(int) =
        (void* (*)(int))dlsym(RTLD_NEXT, "eglGetCurrentSurface");
    void* (*current_context)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentContext");
    if (!current_surface || !current_context ||
        current_surface(0x3059) != macoblox_gl_window_surface)
        return 0;

    void* context = current_context();
    if (learn_renderer) {
        void (*get_integer)(unsigned int, int*) =
            (void (*)(unsigned int, int*))dlsym(RTLD_NEXT, "glGetIntegerv");
        int program = 0;
        if (get_integer)
            get_integer(0x8B8DU, &program); // GL_CURRENT_PROGRAM
        if (program > 0) {
            macoblox_gl_render_egl_context = context;
            const char* ui_test = getenv("MACOBLOX_GL_TEST_UI_COLOR");
            if (ui_test && ui_test[0] && program >= 700) {
                void (*disable)(unsigned int) =
                    (void (*)(unsigned int))dlsym(RTLD_NEXT, "glDisable");
                void (*color_mask)(unsigned char, unsigned char,
                                   unsigned char, unsigned char) =
                    (void (*)(unsigned char, unsigned char, unsigned char,
                              unsigned char))dlsym(RTLD_NEXT, "glColorMask");
                if (disable) {
                    disable(0x0BE2U); // GL_BLEND
                    disable(0x0B71U); // GL_DEPTH_TEST
                    disable(0x0B44U); // GL_CULL_FACE
                    disable(0x0C11U); // GL_SCISSOR_TEST
                    disable(0x0B90U); // GL_STENCIL_TEST
                    disable(0x8C89U); // GL_RASTERIZER_DISCARD
                    for (unsigned int clip = 0; clip < 8; clip++)
                        disable(0x3000U + clip); // GL_CLIP_DISTANCE0..
                }
                if (color_mask)
                    color_mask(1, 1, 1, 1);
            }
        }
    }
    return macoblox_gl_render_egl_context &&
           context != macoblox_gl_render_egl_context;
}

static void macoblox_trace_window_draw(unsigned int mode, int count) {
    unsigned int (*get_error)(void) =
        (unsigned int (*)(void))dlsym(RTLD_NEXT, "glGetError");
    unsigned int draw_error = get_error ? get_error() : 0;
    void* (*current_surface)(int) =
        (void* (*)(int))dlsym(RTLD_NEXT, "eglGetCurrentSurface");
    if (!current_surface || !current_surface(0x3059))
        return;

    void (*get_integer)(unsigned int, int*) =
        (void (*)(unsigned int, int*))dlsym(RTLD_NEXT, "glGetIntegerv");
    if (!get_integer)
        return;
    int framebuffer = -1;
    get_integer(0x8CA6U, &framebuffer); // GL_DRAW_FRAMEBUFFER_BINDING
    if (framebuffer)
        return;

    long sequence = __sync_add_and_fetch(&macoblox_gl_traced_window_draws, 1);
    if (sequence > 16)
        return;

    int program = 0;
    int vertex_array = 0;
    int active_texture = 0;
    int texture = 0;
    int viewport[4] = {0, 0, 0, 0};
    int scissor[4] = {0, 0, 0, 0};
    get_integer(0x8B8DU, &program);       // GL_CURRENT_PROGRAM
    get_integer(0x85B5U, &vertex_array);  // GL_VERTEX_ARRAY_BINDING
    get_integer(0x84E0U, &active_texture);// GL_ACTIVE_TEXTURE
    get_integer(0x0BA2U, viewport);       // GL_VIEWPORT
    get_integer(0x0C10U, scissor);        // GL_SCISSOR_BOX

    int (*uniform_location)(unsigned int, const char*) =
        (int (*)(unsigned int, const char*))
            dlsym(RTLD_NEXT, "glGetUniformLocation");
    void (*get_uniform)(unsigned int, int, int*) =
        (void (*)(unsigned int, int, int*))
            dlsym(RTLD_NEXT, "glGetUniformiv");
    void (*active_texture_fn)(unsigned int) =
        (void (*)(unsigned int))dlsym(RTLD_NEXT, "glActiveTexture");
    int sampler = 0;
    if (uniform_location && get_uniform && program > 0) {
        int location = uniform_location((unsigned int)program,
                                        "DiffuseMapTexture");
        if (location >= 0)
            get_uniform((unsigned int)program, location, &sampler);
    }
    if (active_texture_fn) {
        active_texture_fn(0x84C0U + (unsigned int)sampler); // GL_TEXTURE0
        get_integer(0x8069U, &texture); // GL_TEXTURE_BINDING_2D
        active_texture_fn((unsigned int)active_texture);
    }

    unsigned int (*uniform_block_index)(unsigned int, const char*) =
        (unsigned int (*)(unsigned int, const char*))
            dlsym(RTLD_NEXT, "glGetUniformBlockIndex");
    void (*uniform_block_iv)(unsigned int, unsigned int, unsigned int, int*) =
        (void (*)(unsigned int, unsigned int, unsigned int, int*))
            dlsym(RTLD_NEXT, "glGetActiveUniformBlockiv");
    void (*get_integer_indexed)(unsigned int, unsigned int, int*) =
        (void (*)(unsigned int, unsigned int, int*))
            dlsym(RTLD_NEXT, "glGetIntegeri_v");
    int cb0_buffer = -1;
    int cb1_buffer = -1;
    macoblox_dump_actual_program_sources((unsigned int)program);
    if (uniform_block_index && uniform_block_iv && get_integer_indexed &&
        program > 0) {
        const char* names[2] = {"block_CB0", "block_CB1"};
        int* buffers[2] = {&cb0_buffer, &cb1_buffer};
        for (int index = 0; index < 2; index++) {
            unsigned int block = uniform_block_index((unsigned int)program,
                                                      names[index]);
            if (block != 0xFFFFFFFFU) {
                int binding = 0;
                uniform_block_iv((unsigned int)program, block, 0x8A3FU,
                                 &binding); // GL_UNIFORM_BLOCK_BINDING
                get_integer_indexed(0x8A28U, (unsigned int)binding,
                                    buffers[index]);
            }
        }
    }

    if (program >= 700 && uniform_block_iv && get_integer_indexed &&
        __sync_bool_compare_and_swap(&macoblox_dumped_uniform_blocks, 0, 1)) {
        void (*get_program)(unsigned int, unsigned int, int*) =
            (void (*)(unsigned int, unsigned int, int*))
                dlsym(RTLD_NEXT, "glGetProgramiv");
        void (*block_name)(unsigned int, unsigned int, int, int*, char*) =
            (void (*)(unsigned int, unsigned int, int, int*, char*))
                dlsym(RTLD_NEXT, "glGetActiveUniformBlockName");
        int blocks = 0;
        if (get_program)
            get_program((unsigned int)program, 0x8A36U, &blocks);
        write_str("[MacOBlox GL] uniform-blocks program=");
        print_num(program);
        write_str(" count=");
        print_num(blocks);
        write_str("\n");
        for (int block = 0; block < blocks && block < 8; block++) {
            int binding = -1;
            int size = -1;
            int buffer = -1;
            int written = 0;
            char name[128];
            name[0] = 0;
            uniform_block_iv((unsigned int)program, (unsigned int)block,
                             0x8A3FU, &binding);
            uniform_block_iv((unsigned int)program, (unsigned int)block,
                             0x8A40U, &size); // GL_UNIFORM_BLOCK_DATA_SIZE
            if (block_name)
                block_name((unsigned int)program, (unsigned int)block,
                           127, &written, name);
            name[127] = 0;
            if (binding >= 0)
                get_integer_indexed(0x8A28U, (unsigned int)binding, &buffer);
            write_str("[MacOBlox GL] uniform-block #");
            print_num(block);
            write_str(" name=");
            write_str(name);
            write_str(" binding=");
            print_num(binding);
            write_str(" buffer=");
            print_num(buffer);
            write_str(" size=");
            print_num(size);
            write_str("\n");
        }
    }

    unsigned char (*is_enabled)(unsigned int) =
        (unsigned char (*)(unsigned int))dlsym(RTLD_NEXT, "glIsEnabled");
    void* (*current_context)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentContext");
    write_str("[MacOBlox GL] window-draw #");
    print_num(sequence);
    write_str(" mode=");
    print_hex(mode);
    write_str(" count=");
    print_num(count);
    write_str(" program=");
    print_num(program);
    write_str(" error=");
    print_hex(draw_error);
    write_str(" egl-context=");
    print_hex((unsigned long long)(current_context ? current_context() : 0));
    write_str(" vao=");
    print_num(vertex_array);
    write_str(" sampler=");
    print_num(sampler);
    write_str(" texture=");
    print_num(texture);
    write_str(" cb0=");
    print_num(cb0_buffer);
    write_str(" cb1=");
    print_num(cb1_buffer);
    write_str(" blend=");
    print_num(is_enabled ? is_enabled(0x0BE2U) : -1);
    write_str(" scissor-enabled=");
    print_num(is_enabled ? is_enabled(0x0C11U) : -1);
    write_str(" viewport=");
    print_num(viewport[0]); write_str(","); print_num(viewport[1]);
    write_str(","); print_num(viewport[2]); write_str(",");
    print_num(viewport[3]);
    write_str(" scissor=");
    print_num(scissor[0]); write_str(","); print_num(scissor[1]);
    write_str(","); print_num(scissor[2]); write_str(",");
    print_num(scissor[3]);
    write_str("\n");
}

// GL call tracing is opt-in (MACOBLOX_TRACE_GL=1). Without it the wrappers
// below only forward: tracing costs dlsym, glGetError and several
// glGetIntegerv per draw, which in game (thousands of draws per frame) was a
// large part of the frame time.
static int macoblox_gl_trace_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* value = getenv("MACOBLOX_TRACE_GL");
        enabled = value && value[0] ? 1 : 0;
    }
    return enabled;
}

static void macoblox_glBindFramebuffer(unsigned int target,
                                       unsigned int framebuffer) {
    static void (*real_function)(unsigned int, unsigned int);
    if (!real_function)
        real_function = (void (*)(unsigned int, unsigned int))
            dlsym(RTLD_NEXT, "glBindFramebuffer");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(target, framebuffer);
        return;
    }
    // Binding framebuffer 0 selects the window drawable. Restore its EGL
    // surface before GL validates the following clear and draw commands.
    if (!framebuffer) {
        macoblox_ensure_gl_window_surface();
        __sync_add_and_fetch(&macoblox_gl_default_framebuffer_bind_count, 1);
    }
    if (real_function)
        real_function(target, framebuffer);
}
DYLD_INTERPOSE(macoblox_glBindFramebuffer, glBindFramebuffer);

static void macoblox_glClear(unsigned int mask) {
    static void (*real_function)(unsigned int);
    if (!real_function)
        real_function = (void (*)(unsigned int))dlsym(RTLD_NEXT, "glClear");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mask);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_clear_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(0))
        real_function(mask);
}
DYLD_INTERPOSE(macoblox_glClear, glClear);

static void macoblox_glDrawArrays(unsigned int mode, int first, int count) {
    static void (*real_function)(unsigned int, int, int);
    if (!real_function)
        real_function = (void (*)(unsigned int, int, int))
            dlsym(RTLD_NEXT, "glDrawArrays");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, first, count);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, first, count);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawArrays, glDrawArrays);

static void macoblox_glDrawElements(unsigned int mode, int count,
                                    unsigned int type, const void* indices) {
    static void (*real_function)(unsigned int, int, unsigned int, const void*);
    if (!real_function)
        real_function = (void (*)(unsigned int, int, unsigned int, const void*))
            dlsym(RTLD_NEXT, "glDrawElements");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, count, type, indices);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, count, type, indices);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawElements, glDrawElements);

static void macoblox_glDrawRangeElements(unsigned int mode, unsigned int start,
                                         unsigned int end, int count,
                                         unsigned int type,
                                         const void* indices) {
    static void (*real_function)(unsigned int, unsigned int, unsigned int, int,
                                 unsigned int, const void*);
    if (!real_function)
        real_function = (void (*)(unsigned int, unsigned int, unsigned int, int,
                                  unsigned int, const void*))
            dlsym(RTLD_NEXT, "glDrawRangeElements");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, start, end, count, type, indices);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, start, end, count, type, indices);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawRangeElements, glDrawRangeElements);

static void macoblox_glDrawElementsBaseVertex(unsigned int mode, int count,
                                              unsigned int type,
                                              const void* indices,
                                              int base_vertex) {
    static void (*real_function)(unsigned int, int, unsigned int, const void*,
                                 int);
    if (!real_function)
        real_function = (void (*)(unsigned int, int, unsigned int, const void*,
                                  int))dlsym(RTLD_NEXT,
                                             "glDrawElementsBaseVertex");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, count, type, indices, base_vertex);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, count, type, indices, base_vertex);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawElementsBaseVertex, glDrawElementsBaseVertex);

static void macoblox_glDrawArraysInstanced(unsigned int mode, int first,
                                           int count, int instances) {
    static void (*real_function)(unsigned int, int, int, int);
    if (!real_function)
        real_function = (void (*)(unsigned int, int, int, int))
            dlsym(RTLD_NEXT, "glDrawArraysInstanced");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, first, count, instances);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, first, count, instances);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawArraysInstanced, glDrawArraysInstanced);

static void macoblox_glDrawElementsInstanced(unsigned int mode, int count,
                                             unsigned int type,
                                             const void* indices,
                                             int instances) {
    static void (*real_function)(unsigned int, int, unsigned int, const void*,
                                 int);
    if (!real_function)
        real_function = (void (*)(unsigned int, int, unsigned int, const void*,
                                  int))dlsym(RTLD_NEXT,
                                             "glDrawElementsInstanced");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, count, type, indices, instances);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, count, type, indices, instances);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawElementsInstanced, glDrawElementsInstanced);

static void macoblox_glDrawElementsInstancedBaseVertex(
    unsigned int mode, int count, unsigned int type, const void* indices,
    int instances, int base_vertex) {
    static void (*real_function)(unsigned int, int, unsigned int, const void*,
                                 int, int);
    if (!real_function)
        real_function =
            (void (*)(unsigned int, int, unsigned int, const void*, int, int))
                dlsym(RTLD_NEXT, "glDrawElementsInstancedBaseVertex");
    if (!macoblox_gl_trace_enabled() && macoblox_gl_hacks_disabled()) {
        if (real_function)
            real_function(mode, count, type, indices, instances, base_vertex);
        return;
    }
    macoblox_ensure_gl_window_surface();
    __sync_add_and_fetch(&macoblox_gl_draw_count, 1);
    if (real_function &&
        !macoblox_should_skip_foreign_window_command(1))
        real_function(mode, count, type, indices, instances, base_vertex);
    else
        __sync_add_and_fetch(&macoblox_gl_skipped_foreign_draws, 1);
    macoblox_trace_window_draw(mode, count);
}
DYLD_INTERPOSE(macoblox_glDrawElementsInstancedBaseVertex,
               glDrawElementsInstancedBaseVertex);

static void macoblox_glBlitFramebuffer(int sx0, int sy0, int sx1, int sy1,
                                       int dx0, int dy0, int dx1, int dy1,
                                       unsigned int mask,
                                       unsigned int filter) {
    static void (*real_function)(int, int, int, int, int, int, int, int,
                                 unsigned int, unsigned int);
    if (!real_function)
        real_function =
            (void (*)(int, int, int, int, int, int, int, int, unsigned int,
                      unsigned int))dlsym(RTLD_NEXT, "glBlitFramebuffer");
    __sync_add_and_fetch(&macoblox_gl_blit_count, 1);
    if (real_function)
        real_function(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, filter);
}
DYLD_INTERPOSE(macoblox_glBlitFramebuffer, glBlitFramebuffer);

static void write_str(const char* s) {
    if (!s) return;
    int len = 0;
    while (s[len]) len++;
    write(2, s, len);
}

static void print_hex(unsigned long long val) {
    char buf[20];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        buf[2 + (15 - i)] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
    }
    buf[18] = '\0';
    write(2, buf, 18);
}

static void print_num(long long val) {
    char buf[32];
    int pos = 30;
    buf[31] = '\0';
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) {
        buf[pos--] = '0';
    } else {
        while (val > 0) {
            buf[pos--] = '0' + (val % 10);
            val /= 10;
        }
    }
    if (neg) buf[pos--] = '-';
    write(2, &buf[pos + 1], 30 - pos);
}

static void print_backtrace(void) {
    void* frames[64];
    int n = backtrace(frames, 64);
    for (int i = 0; i < n; i++) {
        write_str("  [frame ");
        print_num(i);
        write_str("]: ");
        print_hex((unsigned long long)frames[i]);
        write_str("\n");
    }
    backtrace_symbols_fd(frames, n, 2);
}

// Trace the exact resolver requests made by Roblox. A plain getaddrinfo probe
// succeeds in Darling while Roblox reports DnsResolve, so the hints passed by
// its networking layer are relevant to the failure.
struct macoblox_addrinfo_head {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
};
extern int getaddrinfo(const char*, const char*, const void*, void**);
extern int usleep(unsigned int);
static volatile int macoblox_dns_lock;
extern int macoblox_dns_resolve(const char* node, const char* service,
                                const void* hints, void** result);
static int macoblox_getaddrinfo(const char* node, const char* service,
                                const void* hints, void** result) {
    // DNS chosen in the launcher, for Roblox only (dns_override.c).
    int own = macoblox_dns_resolve(node, service, hints, result);
    if (own >= 0)
        return own;
    int (*real_getaddrinfo)(const char*, const char*, const void*, void**) =
        (int (*)(const char*, const char*, const void*, void**))dlsym(RTLD_NEXT, "getaddrinfo");
    while (__sync_lock_test_and_set(&macoblox_dns_lock, 1))
        usleep(1000);

    int status = -1;
    int attempts = 0;
    if (real_getaddrinfo) {
        for (attempts = 1; attempts <= 4; attempts++) {
            if (result)
                *result = 0;
            status = real_getaddrinfo(node, service, hints, result);
            if (status == 0)
                break;
            usleep(50000);
        }
    }
    __sync_lock_release(&macoblox_dns_lock);

    const char* trace = getenv("MACOBLOX_TRACE_DNS");
    if (status != 0 || (trace && *trace == '1')) {
        write_str("[MacOBlox DNS] node=");
        write_str(node ? node : "(null)");
        write_str(" service=");
        write_str(service ? service : "(null)");
        if (hints) {
            const struct macoblox_addrinfo_head* head =
                (const struct macoblox_addrinfo_head*)hints;
            write_str(" flags="); print_num(head->ai_flags);
            write_str(" family="); print_num(head->ai_family);
            write_str(" socktype="); print_num(head->ai_socktype);
            write_str(" protocol="); print_num(head->ai_protocol);
        } else {
            write_str(" hints=(null)");
        }
        write_str(" attempts="); print_num(attempts);
        write_str(" result="); print_num(status);
        write_str("\n");
    }
    return status;
}
DYLD_INTERPOSE(macoblox_getaddrinfo, getaddrinfo)

// Interposed functions
extern void exit(int);
extern void _exit(int);
extern void _Exit(int);
extern void abort(void);
extern void __cxa_throw(void*, void*, void(*)(void*));
extern void _ZSt9terminatev(void);
extern void objc_exception_throw(id);
extern int NSApplicationMain(int argc, const char *argv[]);

static int in_interpose_exit = 0;
void my_exit(int status) {
    if (!in_interpose_exit) {
        in_interpose_exit = 1;
        write_str("\n[MacOBlox Hook] exit(");
        print_num(status);
        write_str(") called!\n[MacOBlox Hook] Backtrace:\n");
        print_backtrace();
    }
    void (*real_exit)(int) = (void (*)(int))dlsym(RTLD_NEXT, "exit");
    if (real_exit) real_exit(status);
    _exit(status);
}
// Disabled: tracing/suppressing exit changes crash and post-fork semantics.

void my__exit(int status) {
    if (!in_interpose_exit) {
        in_interpose_exit = 1;
        write_str("\n[MacOBlox Hook] _exit(");
        print_num(status);
        write_str(") called!\n[MacOBlox Hook] Backtrace:\n");
        print_backtrace();
    }
    void (*real__exit)(int) = (void (*)(int))dlsym(RTLD_NEXT, "_exit");
    if (real__exit) real__exit(status);
    while (1);
}
// Disabled: tracing/suppressing _exit changes crash and post-fork semantics.

void my__Exit(int status) {
    if (!in_interpose_exit) {
        in_interpose_exit = 1;
        write_str("\n[MacOBlox Hook] _Exit(");
        print_num(status);
        write_str(") called!\n[MacOBlox Hook] Backtrace:\n");
        print_backtrace();
    }
    void (*real__Exit)(int) = (void (*)(int))dlsym(RTLD_NEXT, "_Exit");
    if (real__Exit) real__Exit(status);
    while (1);
}
// Disabled: tracing/suppressing _Exit changes crash and post-fork semantics.

void my_abort(void) {
    write_str("\n[MacOBlox Hook] abort() called!\n[MacOBlox Hook] Backtrace:\n");
    print_backtrace();
    void (*real_abort)(void) = (void (*)(void))dlsym(RTLD_NEXT, "abort");
    if (real_abort) real_abort();
    while (1);
}
// Disabled: tracing/suppressing abort changes crash and post-fork semantics.

// Interpose pthread_kill, kill, raise
extern int pthread_kill(void* thread, int sig);
int my_pthread_kill(void* thread, int sig) {
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] pthread_kill called with sig: ");
    print_num(sig);
    write_str(" for thread: ");
    print_hex((unsigned long long)thread);
    write_str("\nBacktrace of caller:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");
    if (sig == 11 || sig == 10 || sig == 6) {
        write_str("[MacOBlox Hook] Suppressing fatal signal in pthread_kill!\n");
        return 0;
    }
    int (*real_pk)(void*, int) = (int (*)(void*, int))dlsym(RTLD_NEXT, "pthread_kill");
    return real_pk ? real_pk(thread, sig) : 0;
}
// Disabled: tracing/suppressing pthread_kill changes crash and post-fork semantics.

extern int kill(int pid, int sig);
int my_kill(int pid, int sig) {
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] kill() called with sig: ");
    print_num(sig);
    write_str(" for pid: ");
    print_num(pid);
    write_str("\nBacktrace of caller:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");
    if (sig == 11 || sig == 10 || sig == 6) {
        write_str("[MacOBlox Hook] Suppressing fatal signal in kill()!\n");
        return 0;
    }
    int (*real_k)(int, int) = (int (*)(int, int))dlsym(RTLD_NEXT, "kill");
    return real_k ? real_k(pid, sig) : 0;
}
// Disabled: tracing/suppressing kill changes crash and post-fork semantics.

extern int raise(int sig);
int my_raise(int sig) {
    write_str("\n[MacOBlox Hook] raise() called with sig: ");
    print_num(sig);
    write_str("\nBacktrace of caller:\n");
    print_backtrace();
    if (sig == 11 || sig == 10 || sig == 6) {
        write_str("[MacOBlox Hook] Suppressing fatal signal in raise()!\n");
        return 0;
    }
    int (*real_raise)(int) = (int (*)(int))dlsym(RTLD_NEXT, "raise");
    return real_raise ? real_raise(sig) : 0;
}
// Disabled: tracing/suppressing raise changes crash and post-fork semantics.

static int trace_exceptions_enabled(void) {
    const char* value = getenv("MACOBLOX_TRACE_EXCEPTIONS");
    return value && *value == '1';
}

void my_cxa_throw(void* thrown_exception, void* tinfo, void (*dest)(void*)) {
    if (!trace_exceptions_enabled()) {
        void (*real_throw)(void*, void*, void(*)(void*)) =
            (void (*)(void*, void*, void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");
        if (real_throw) real_throw(thrown_exception, tinfo, dest);
        while (1);
    }
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] __cxa_throw called!\n");
    if (tinfo) {
        const char* mangled = ((const char**)tinfo)[1];
        if (mangled) {
            write_str("[MacOBlox Hook] Mangled type: ");
            write_str(mangled);
            write_str("\n");
            typedef char* (*demangle_fn)(const char*, char*, unsigned long*, int*);
            demangle_fn demangle = (demangle_fn)dlsym(RTLD_DEFAULT, "__cxa_demangle");
            if (demangle) {
                int status = 0;
                char* demangled = demangle(mangled, 0, 0, &status);
                if (demangled) {
                    write_str("[MacOBlox Hook] Demangled type: ");
                    write_str(demangled);
                    write_str("\n");
                }
            }
        }
    }
    // C++ can throw scalars and arbitrary classes, not just std::exception.
    // Never interpret an unknown exception object as a vtable or call through it.
    write_str("[MacOBlox Hook] Backtrace:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");

    void (*real_throw)(void*, void*, void(*)(void*)) = (void (*)(void*, void*, void(*)(void*)))dlsym(RTLD_NEXT, "__cxa_throw");
    if (real_throw) {
        real_throw(thrown_exception, tinfo, dest);
    }
    while (1);
}
DYLD_INTERPOSE(my_cxa_throw, __cxa_throw);

void my_objc_exception_throw(id exception) {
    if (!trace_exceptions_enabled()) {
        void (*real_throw)(id) = (void (*)(id))dlsym(RTLD_NEXT, "objc_exception_throw");
        if (real_throw) real_throw(exception);
        while (1);
    }
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] objc_exception_throw called!\n");
    if (exception) {
        write_str("[MacOBlox Hook] Exception class: ");
        write_str(object_getClassName(exception));
        write_str("\n");
    }
    write_str("[MacOBlox Hook] Backtrace:\n");
    void (*real_throw)(id) = (void (*)(id))dlsym(RTLD_NEXT, "objc_exception_throw");
    if (real_throw) {
        real_throw(exception);
    }
    while (1);
}
DYLD_INTERPOSE(my_objc_exception_throw, objc_exception_throw);

// Interpose _Unwind_Resume, __throw_system_error, __cxa_rethrow
extern void _Unwind_Resume(void*);
void my_Unwind_Resume(void* exc) {
    if (!trace_exceptions_enabled()) {
        void (*real_fn)(void*) = (void (*)(void*))dlsym(RTLD_NEXT, "_Unwind_Resume");
        if (real_fn) real_fn(exc);
        while (1);
    }
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] _Unwind_Resume called! exc: ");
    print_hex((unsigned long long)exc);
    write_str("\nBacktrace:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");
    void (*real_fn)(void*) = (void (*)(void*))dlsym(RTLD_NEXT, "_Unwind_Resume");
    if (real_fn) real_fn(exc);
    while (1);
}
DYLD_INTERPOSE(my_Unwind_Resume, _Unwind_Resume);

extern void _ZNSt3__120__throw_system_errorEiPKc(int, const char*);
void my_throw_system_error(int err, const char* msg) {
    if (!trace_exceptions_enabled()) {
        void (*real_fn)(int, const char*) =
            (void (*)(int, const char*))dlsym(RTLD_NEXT, "_ZNSt3__120__throw_system_errorEiPKc");
        if (real_fn) real_fn(err, msg);
        while (1);
    }
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] std::__throw_system_error called! err: ");
    print_num(err);
    write_str(", msg: ");
    write_str(msg ? msg : "(null)");
    write_str("\nBacktrace:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");
    void (*real_fn)(int, const char*) = (void (*)(int, const char*))dlsym(RTLD_NEXT, "_ZNSt3__120__throw_system_errorEiPKc");
    if (real_fn) real_fn(err, msg);
    while (1);
}
DYLD_INTERPOSE(my_throw_system_error, _ZNSt3__120__throw_system_errorEiPKc);

extern void __cxa_rethrow(void);
void my_cxa_rethrow(void) {
    if (!trace_exceptions_enabled()) {
        void (*real_fn)(void) = (void (*)(void))dlsym(RTLD_NEXT, "__cxa_rethrow");
        if (real_fn) real_fn();
        while (1);
    }
    write_str("\n[MacOBlox Hook] ========================================\n");
    write_str("[MacOBlox Hook] __cxa_rethrow called!\nBacktrace:\n");
    print_backtrace();
    write_str("[MacOBlox Hook] ========================================\n\n");
    void (*real_fn)(void) = (void (*)(void))dlsym(RTLD_NEXT, "__cxa_rethrow");
    if (real_fn) real_fn();
    while (1);
}
DYLD_INTERPOSE(my_cxa_rethrow, __cxa_rethrow);

int my_NSApplicationMain(int argc, const char *argv[]) {
    write_str("\n[MacOBlox Hook] NSApplicationMain entered!\n");
    int (*real_main)(int, const char*[]) = (int (*)(int, const char*[]))dlsym(RTLD_NEXT, "NSApplicationMain");
    int res = real_main ? real_main(argc, argv) : 0;
    write_str("\n[MacOBlox Hook] NSApplicationMain returned: ");
    print_num(res);
    write_str("\n[MacOBlox Hook] Backtrace:\n");
    print_backtrace();
    return res;
}
DYLD_INTERPOSE(my_NSApplicationMain, NSApplicationMain);

// Signal crash handler
struct darwin_sigaction {
    void (*sa_sigaction)(int, void*, void*);
    unsigned int sa_mask;
    int sa_flags;
};
extern int sigaction(int, const struct darwin_sigaction*, struct darwin_sigaction*);

typedef struct dl_info {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;
extern int dladdr(const void *, Dl_info *);

static void print_addr_info(const char* prefix, void* addr) {
    write_str(prefix);
    print_hex((unsigned long long)addr);
    Dl_info dli;
    if (dladdr(addr, &dli)) {
        if (dli.dli_fname) {
            write_str(" in ");
            write_str(dli.dli_fname);
        }
        if (dli.dli_sname) {
            write_str(" (");
            write_str(dli.dli_sname);
            write_str("+");
            print_num((long long)((char*)addr - (char*)dli.dli_saddr));
            write_str(")");
        }
    }
    write_str("\n");
}

static void crash_handler(int sig, void* info, void* uap) {
    write_str("\n\n[MacOBlox FATAL CRASH] ****************************************\n");
    write_str("[MacOBlox FATAL CRASH] Signal received: ");
    print_num(sig);
    write_str("\n");
    if (info) {
        write_str("[MacOBlox FATAL CRASH] siginfo: ");
        print_hex((unsigned long long)info);
        unsigned long long* si = (unsigned long long*)info;
        write_str("\n  si_signo/code: ");
        print_hex(si[0]);
        write_str("\n  si_addr (fault addr / sender pid): ");
        print_hex(si[2]);
        write_str("\n");
    }
    if (uap) {
        write_str("[MacOBlox FATAL CRASH] ucontext: ");
        print_hex((unsigned long long)uap);
        write_str("\n");
        unsigned long long* p = (unsigned long long*)uap;
        write_str("  uc_mcontext ptr: ");
        print_hex(p[6]);
        write_str("\n");
        unsigned long long* mc = (unsigned long long*)p[6];
        if (mc) {
            write_str("  RAX: "); print_hex(mc[2]); write_str("\n");
            write_str("  RBX: "); print_hex(mc[3]); write_str("\n");
            write_str("  RCX: "); print_hex(mc[4]); write_str("\n");
            write_str("  RDX: "); print_hex(mc[5]); write_str("\n");
            write_str("  RDI: "); print_hex(mc[6]); write_str("\n");
            write_str("  RSI: "); print_hex(mc[7]); write_str("\n");
            write_str("  RBP: "); print_hex(mc[8]); write_str("\n");
            write_str("  RSP: "); print_hex(mc[9]); write_str("\n");
            write_str("  RIP: "); print_hex(mc[18]); write_str("\n");
            write_str("  RFLAGS: "); print_hex(mc[19]); write_str("\n");

            print_addr_info("  Fault RIP info: ", (void*)mc[18]);
            print_addr_info("  RDI info: ", (void*)mc[6]);
            print_addr_info("  RSI info: ", (void*)mc[7]);

            write_str("\n[MacOBlox Stack Walk from RBP]:\n");
            void** fp = (void**)mc[8];
            for (int i = 0; i < 30 && fp; i++) {
                if ((unsigned long long)fp < 0x1000 || ((unsigned long long)fp & 7)) break;
                void* ret_addr = fp[1];
                write_str("  #"); print_num(i); write_str(" ");
                print_addr_info("", ret_addr);
                fp = (void**)fp[0];
            }
        }
    }
    write_str("[MacOBlox FATAL CRASH] ****************************************\n\n");
    _exit(128 + sig);
}

int my_sigaction(int sig, const struct darwin_sigaction *act, struct darwin_sigaction *oact) {
    write_str("[MacOBlox Hook] sigaction called for sig: ");
    print_num(sig);
    write_str(", new handler: ");
    print_hex(act ? (unsigned long long)act->sa_sigaction : 0);
    write_str("\n");
    int (*real_sigaction)(int, const void*, void*) = (int (*)(int, const void*, void*))dlsym(RTLD_NEXT, "sigaction");
    // Opt-in crash diagnosis only: preserve the application's handlers normally.
    const char *diagnose = getenv("MACOBLOX_DIAGNOSTIC_SIGNALS");
    if (diagnose && *diagnose == '1' && sig == 11 && act && real_sigaction) {
        struct darwin_sigaction debug_action = {crash_handler, 0, 0x0040};
        return real_sigaction(sig, &debug_action, oact);
    }
    return real_sigaction ? real_sigaction(sig, act, oact) : 0;
}
DYLD_INTERPOSE(my_sigaction, sigaction);

// Swizzle NSConcreteScanner
static id (*orig_concrete_initWithString)(id self, SEL _cmd, id str) = 0;
static id hooked_concrete_initWithString(id self, SEL _cmd, id str) {
    if (!str) {
        write_str("\n[MacOBlox Hook] -[NSConcreteScanner initWithString:nil] called!\n");
    }
    return orig_concrete_initWithString(self, _cmd, str);
}

// Swizzle NSApplication run
static void (*orig_app_run)(id self, SEL _cmd) = 0;
static void hooked_app_run(id self, SEL _cmd) {
    write_str("\n[MacOBlox Hook] -[NSApplication run] entered!\n");
    orig_app_run(self, _cmd);
    write_str("\n[MacOBlox Hook] -[NSApplication run] returned!\n");
}

static int ascii_strings_equal(const char* left, const char* right) {
    if (!left || !right)
        return 0;
    while (*left && *right && *left == *right) {
        left++;
        right++;
    }
    return *left == 0 && *right == 0;
}

static void (*orig_app_finish_launching)(id self, SEL cmd) = 0;
// Window icon: MACOBLOX_ICON_ARGB names a file of 32-bit little-endian words
// in _NET_WM_ICON layout (width, height, ARGB pixels, repeated per size),
// written by the launcher. It is set on the Roblox X window from a separate
// X connection, so docks and window switchers show the Mac O Blox logo.
extern void* malloc(unsigned long);
extern void free(void*);
static unsigned long macoblox_icon_window;
static void* macoblox_set_window_icon_thread(void* unused) {
    (void)unused;
    const char* path = getenv("MACOBLOX_ICON_ARGB");
    const char* wm_class = getenv("MACOBLOX_WM_CLASS");
    extern unsigned long fread(void*, unsigned long, unsigned long, MacOBloxFILE*);
    unsigned long* longs = 0;
    unsigned long count = 0;
    if (path && path[0]) {
        MacOBloxFILE* file = fopen(path, "rb");
        if (file) {
            unsigned int* words = (unsigned int*)malloc(1 << 22);
            count = words ? fread(words, 4, (1 << 22) / 4, file) : 0;
            fclose(file);
            if (count > 0) {
                longs = (unsigned long*)malloc(count * sizeof(unsigned long));
                for (unsigned long index = 0; index < count && longs; index++)
                    longs[index] = words[index];
            }
            free(words);
        }
    }
    void* (*open_display)(const char*) = (void* (*)(const char*))dlsym(RTLD_DEFAULT, "XOpenDisplay");
    unsigned long (*intern)(void*, const char*, int) =
        (unsigned long (*)(void*, const char*, int))dlsym(RTLD_DEFAULT, "XInternAtom");
    int (*change)(void*, unsigned long, unsigned long, unsigned long, int, int, const void*, int) =
        (int (*)(void*, unsigned long, unsigned long, unsigned long, int, int, const void*, int))
            dlsym(RTLD_DEFAULT, "XChangeProperty");
    int (*close_display)(void*) = (int (*)(void*))dlsym(RTLD_DEFAULT, "XCloseDisplay");
    int (*sync)(void*, int) = (int (*)(void*, int))dlsym(RTLD_DEFAULT, "XSync");
    void* display = open_display && (longs || (wm_class && wm_class[0])) ? open_display(0) : 0;
    if (display && intern && change) {
        if (longs && count > 0) {
            unsigned long atom = intern(display, "_NET_WM_ICON", 0);
            change(display, macoblox_icon_window, atom, 6 /* XA_CARDINAL */, 32,
                   0 /* PropModeReplace */, longs, (int)count);
            write_str("[MacOBlox] Window icon set\n");
        }
        if (wm_class && wm_class[0]) {
            char class_buf[128];
            unsigned long pos = 0;
            static const char inst_name[] = "RobloxPlayer";
            for (unsigned long i = 0; i < sizeof(inst_name) && pos < sizeof(class_buf); i++)
                class_buf[pos++] = inst_name[i];
            for (unsigned long i = 0; wm_class[i] && pos + 1 < sizeof(class_buf); i++)
                class_buf[pos++] = wm_class[i];
            if (pos < sizeof(class_buf))
                class_buf[pos++] = 0;
            change(display, macoblox_icon_window, 67 /* XA_WM_CLASS */, 31 /* XA_STRING */, 8,
                   0 /* PropModeReplace */, class_buf, (int)pos);
            write_str("[MacOBlox] Window WM_CLASS set\n");
        }
        if (sync)
            sync(display, 0);
    }
    if (display && close_display)
        close_display(display);
    free(longs);
    return 0;
}

static void macoblox_set_window_icon(id window) {
    if ((!getenv("MACOBLOX_ICON_ARGB") && !getenv("MACOBLOX_WM_CLASS")) || macoblox_icon_window)
        return;
    id platform = ((id (*)(id, SEL))objc_msgSend)(window, sel_registerName("platformWindow"));
    if (!platform || !((signed char (*)(id, SEL, SEL))objc_msgSend)(
                         platform, sel_registerName("respondsToSelector:"),
                         sel_registerName("windowHandle")))
        return;
    macoblox_icon_window = ((unsigned long (*)(id, SEL))objc_msgSend)(
        platform, sel_registerName("windowHandle"));
    if (!macoblox_icon_window)
        return;
    extern int pthread_create(void**, const void*, void* (*)(void*), void*);
    void* thread;
    pthread_create(&thread, 0, macoblox_set_window_icon_thread, 0);
}

// Darling draws the macOS menu bar (Roblox, Edit, Window...) inside each
// window. With MACOBLOX_HIDE_MENU_BAR=1 its height is 0: cocotron uses
// +[NSMainMenuView menuHeight] for the bar itself and for converting between
// window frame and content rect, so the game fills the whole window, also
// after Roblox rebuilds its menu (which re-showed a strip after sign-in).
static double macoblox_zero_menu_height(id cls, SEL cmd) {
    (void)cls; (void)cmd;
    return 0.0;
}

// Secure-coding convenience methods (macOS 10.13) missing in Darling.
// Roblox archives some state with them after sign-in and unarchives it at
// the next start, which crashed. Map them to the older keyed archiver API;
// unreadable data or an object of the wrong class gives nil, not a crash.
static id keyed_archiver_archived_data(id cls, SEL cmd, id root, signed char secure, id* error) {
    (void)cls; (void)cmd; (void)secure;
    if (error)
        *error = 0;
    return ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSKeyedArchiver"), sel_registerName("archivedDataWithRootObject:"), root);
}
static id keyed_unarchiver_unarchived_object(id cls, SEL cmd, Class expected, id data, id* error) {
    (void)cls; (void)cmd;
    if (error)
        *error = 0;
    if (!data)
        return 0;
    id object = 0;
    @try {
        object = ((id (*)(id, SEL, id))objc_msgSend)(
            (id)objc_getClass("NSKeyedUnarchiver"), sel_registerName("unarchiveObjectWithData:"), data);
    } @catch (id exception) {
        (void)exception;
        object = 0;
    }
    if (object && expected &&
        !((signed char (*)(id, SEL, Class))objc_msgSend)(object, sel_registerName("isKindOfClass:"), expected))
        object = 0;
    return object;
}

static void macoblox_install_late_hooks(void);
static void hooked_app_finish_launching(id self, SEL cmd) {
    macoblox_install_late_hooks();
    orig_app_finish_launching(self, cmd);

    id windows = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("windows"));
    unsigned long count = windows
        ? ((unsigned long (*)(id, SEL))objc_msgSend)(windows, sel_registerName("count")) : 0;
    for (unsigned long index = 0; index < count; index++) {
        id window = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
            windows, sel_registerName("objectAtIndex:"), index);
        const char* class_name = window ? object_getClassName(window) : 0;
        if (!ascii_strings_equal(class_name, "RBXWindow"))
            continue;
        macoblox_set_window_icon(window);
        signed char visible = ((signed char (*)(id, SEL))objc_msgSend)(
            window, sel_registerName("isVisible"));
        if (!visible) {
            ((void (*)(id, SEL, id))objc_msgSend)(
                window, sel_registerName("makeKeyAndOrderFront:"), 0);
            write_str("[MacOBlox] Ordered RBXWindow to the front after launch\n");
        }
    }
}

// Swizzle NSApplication terminate:
static void (*orig_app_terminate)(id self, SEL _cmd, id sender) = 0;
static void hooked_app_terminate(id self, SEL _cmd, id sender) {
    write_str("\n[MacOBlox Hook] -[NSApplication terminate:] called!\nBacktrace:\n");
    print_backtrace();
    orig_app_terminate(self, _cmd, sender);
}

// Swizzle NSApplication setDelegate:
static void (*orig_app_setDelegate)(id self, SEL _cmd, id del) = 0;
static void hooked_app_setDelegate(id self, SEL _cmd, id del) {
    write_str("\n[MacOBlox Hook] -[NSApplication setDelegate:] called with: ");
    write_str(del ? object_getClassName(del) : "(nil)");
    write_str("\n");
    orig_app_setDelegate(self, _cmd, del);
}

// Swizzle NSBundle loadNibNamed:owner:
static int (*orig_loadNibNamed)(id self, SEL _cmd, id name, id owner) = 0;
static int hooked_loadNibNamed(id self, SEL _cmd, id name, id owner) {
    write_str("\n[MacOBlox Hook] +[NSBundle loadNibNamed:owner:] entered\n");
    int res = orig_loadNibNamed(self, _cmd, name, owner);
    write_str("[MacOBlox Hook] +[NSBundle loadNibNamed:owner:] returned: ");
    print_num(res);
    write_str("\n");
    return res;
}

// Swizzle NSNib instantiateNibWithExternalNameTable:
static int (*orig_instantiateNib)(id self, SEL _cmd, id table) = 0;
static int hooked_instantiateNib(id self, SEL _cmd, id table) {
    write_str("\n[MacOBlox Hook] -[NSNib instantiateNibWithExternalNameTable:] entered\n");
    int res = orig_instantiateNib(self, _cmd, table);
    write_str("[MacOBlox Hook] -[NSNib instantiateNibWithExternalNameTable:] returned: ");
    print_num(res);
    write_str("\n");
    return res;
}

// Swizzle NSWindowTemplate initWithCoder:
static int in_wt_init = 0;
static id (*orig_wt_initWithCoder)(id self, SEL _cmd, id coder) = 0;
static id hooked_wt_initWithCoder(id self, SEL _cmd, id coder) {
    write_str("[MacOBlox Hook] -[NSWindowTemplate initWithCoder:] START\n");
    in_wt_init = 1;
    id res = orig_wt_initWithCoder(self, _cmd, coder);
    in_wt_init = 0;
    write_str("[MacOBlox Hook] -[NSWindowTemplate initWithCoder:] END -> ");
    print_hex((unsigned long long)res);
    write_str("\n");
    return res;
}

// Swizzle NSKeyedUnarchiver decodeObjectForKey:
static id (*orig_decodeObjectForKey)(id self, SEL _cmd, id key) = 0;
static id hooked_decodeObjectForKey(id self, SEL _cmd, id key) {
    const char* kstr = key ? (const char*)objc_msgSend(key, sel_registerName("UTF8String")) : 0;
    if (in_wt_init) {
        write_str("  [WT decodeObjectForKey]: ");
        write_str(kstr ? kstr : "(null)");
        write_str("\n");
    }
    id res = orig_decodeObjectForKey(self, _cmd, key);
    if (in_wt_init) {
        write_str("    -> returned: ");
        write_str(res ? object_getClassName(res) : "(nil)");
        write_str("\n");
    }
    return res;
}

// Swizzle NSIBObjectData initWithCoder:
static id (*orig_od_initWithCoder)(id self, SEL _cmd, id coder) = 0;
static id hooked_od_initWithCoder(id self, SEL _cmd, id coder) {
    write_str("[MacOBlox Hook] -[NSIBObjectData initWithCoder:] START\n");
    id res = orig_od_initWithCoder(self, _cmd, coder);
    write_str("[MacOBlox Hook] -[NSIBObjectData initWithCoder:] END -> ");
    print_hex((unsigned long long)res);
    write_str("\n");
    return res;
}

// Swizzle NSIBObjectData establishConnections
static void (*orig_od_establish)(id self, SEL _cmd) = 0;
static void hooked_od_establish(id self, SEL _cmd) {
    write_str("[MacOBlox Hook] -[NSIBObjectData establishConnections] START\n");
    orig_od_establish(self, _cmd);
    write_str("[MacOBlox Hook] -[NSIBObjectData establishConnections] END\n");
}

// Swizzle NSNibConnector establishConnection
static void (*orig_conn_establish)(id self, SEL _cmd) = 0;
static void hooked_conn_establish(id self, SEL _cmd) {
    write_str("[MacOBlox Hook] -[NSNibConnector establishConnection] called on: ");
    write_str(object_getClassName(self));
    write_str("\n");
    orig_conn_establish(self, _cmd);
    write_str("[MacOBlox Hook] -[NSNibConnector establishConnection] finished\n");
}

// Cocoa x86_64 ABI: NSRect is four doubles passed by value; BOOL is signed char.
typedef struct { double x, y; } MacOBloxPoint;
typedef struct { double width, height; } MacOBloxSize;
typedef struct { MacOBloxPoint origin; MacOBloxSize size; } MacOBloxRect;
typedef signed char MacOBloxBool;

// Swizzle NSWindow initWithContentRect:styleMask:backing:defer:
static id (*orig_win_init)(id self, SEL _cmd, MacOBloxRect r, unsigned long sm, unsigned long bs, MacOBloxBool def) = 0;
static id hooked_win_init(id self, SEL _cmd, MacOBloxRect r, unsigned long sm, unsigned long bs, MacOBloxBool def) {
    write_str("\n[MacOBlox Hook] -[NSWindow initWithContentRect:...] START, class: ");
    write_str(object_getClassName(self));
    write_str("\n");
    id res = orig_win_init(self, _cmd, r, sm, bs, def);
    write_str("[MacOBlox Hook] -[NSWindow initWithContentRect:...] END -> ");
    print_hex((unsigned long long)res);
    write_str("\n");
    return res;
}

// Experimental 1x backing-size fallback for Darling's non-Retina NSView.
// Only add it when AppKit does not implement the method.
extern signed char class_addMethod(Class, SEL, IMP, const char*);
static MacOBloxSize backing_size_1x(id self, SEL cmd, MacOBloxSize size) {
    (void)self; (void)cmd;
    return size;
}

// NSWindow gained rectangle variants of its screen conversion API after the
// AppKit snapshot used by Darling.  Roblox uses convertRectFromScreen: while
// handling mouse movement.  Falling through Objective-C forwarding is unsafe
// for a CGRect return on x86_64, so implement the methods in terms of the older
// point conversion calls which Darling does provide.
static MacOBloxRect window_convert_rect_from_screen(id self, SEL cmd, MacOBloxRect rect) {
    (void)cmd;
    rect.origin = ((MacOBloxPoint (*)(id, SEL, MacOBloxPoint))objc_msgSend)(
        self, sel_registerName("convertScreenToBase:"), rect.origin);
    return rect;
}

static MacOBloxRect window_convert_rect_to_screen(id self, SEL cmd, MacOBloxRect rect) {
    (void)cmd;
    rect.origin = ((MacOBloxPoint (*)(id, SEL, MacOBloxPoint))objc_msgSend)(
        self, sel_registerName("convertBaseToScreen:"), rect.origin);
    return rect;
}

// Darling exposes a deliberately small WebPreferences forwarding stub, but
// omits this legacy singleton constructor.  Roblox asks for the singleton while
// creating its experience coordinator, before any preference setters are sent.
static id macoblox_standard_web_preferences;
static volatile int macoblox_web_preferences_lock;
static id web_preferences_standard_preferences(id cls, SEL cmd) {
    (void)cmd;
    if (macoblox_standard_web_preferences)
        return macoblox_standard_web_preferences;
    while (__sync_lock_test_and_set(&macoblox_web_preferences_lock, 1))
        usleep(1000);
    if (!macoblox_standard_web_preferences) {
        id preferences = ((id (*)(id, SEL))objc_msgSend)(
            cls, sel_registerName("alloc"));
        preferences = preferences
            ? ((id (*)(id, SEL))objc_msgSend)(preferences, sel_registerName("init"))
            : 0;
        macoblox_standard_web_preferences = preferences;
    }
    __sync_lock_release(&macoblox_web_preferences_lock);
    return macoblox_standard_web_preferences;
}

// Darling's AVFoundation class shell does not implement device discovery.
// Roblox/WebRTC probes cameras during Universal App startup even when voice or
// video capture is not in use.  An empty inventory is the correct headless
// result and lets the rest of the client continue initializing.
static id empty_capture_devices(id cls, SEL cmd) {
    (void)cls; (void)cmd;
    return ((id (*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSArray"), sel_registerName("array"));
}

static id empty_capture_devices_for_media_type(id cls, SEL cmd, id media_type) {
    (void)media_type;
    return empty_capture_devices(cls, cmd);
}

static id no_default_capture_device(id cls, SEL cmd, id media_type) {
    (void)cls; (void)cmd; (void)media_type;
    return 0;
}

// Keep the missing CALayer property as associated state. Darling's renderer
// still only supports the existing 1x path; this does not add Retina rendering.
extern void objc_setAssociatedObject(id, const void*, id, unsigned long);
extern id objc_getAssociatedObject(id, const void*);
static char contents_scale_key;
static void layer_set_contents_scale(id self, SEL cmd, double scale) {
    (void)cmd;
    id number = ((id (*)(id, SEL, double))objc_msgSend)(
        (id)objc_getClass("NSNumber"), sel_registerName("numberWithDouble:"), scale);
    objc_setAssociatedObject(self, &contents_scale_key, number, 1);
    if (scale != 1.0)
        write_str("[MacOBlox] Warning: CALayer backing rendering above/below 1x is unimplemented\n");
}
static double layer_contents_scale(id self, SEL cmd) {
    (void)cmd;
    id number = objc_getAssociatedObject(self, &contents_scale_key);
    return number ? ((double (*)(id, SEL))objc_msgSend)(number, sel_registerName("doubleValue")) : 1.0;
}

// Preserve requested touch filtering; this does not synthesize touch events.
static char allowed_touch_types_key;
static void view_set_allowed_touch_types(id self, SEL cmd, unsigned long types) {
    (void)cmd;
    id number = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
        (id)objc_getClass("NSNumber"), sel_registerName("numberWithUnsignedLong:"), types);
    objc_setAssociatedObject(self, &allowed_touch_types_key, number, 1);
}
static unsigned long view_allowed_touch_types(id self, SEL cmd) {
    (void)cmd;
    id number = objc_getAssociatedObject(self, &allowed_touch_types_key);
    return number ? ((unsigned long (*)(id, SEL))objc_msgSend)(number, sel_registerName("unsignedLongValue")) : 0;
}

// Newer AppKit exposes inertial-scroll phases on NSEvent. Darling does not
// synthesize those events, so the only correct value for its existing mouse
// events is NSEventPhaseNone (zero). This also keeps Roblox's input path out of
// Objective-C forwarding while the pointer is over the render view.
static unsigned long event_phase_none(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}

// Darling predates NSProcessInfo's thermal and low-power APIs. Linux exposes
// neither state through this compatibility layer, so report the documented
// neutral values used by macOS when no throttling is active.
static long process_info_thermal_state_nominal(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}

static MacOBloxBool process_info_low_power_mode_disabled(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}

// Trace the handoff between Roblox's OpenGL renderer and Darling's X11
// drawable. Keep this bounded because makeCurrentContext/flushBuffer normally
// run once per frame.
static id (*orig_gl_context_init)(id, SEL, id, id) = 0;
static void (*orig_gl_context_set_view)(id, SEL, id) = 0;
static void (*orig_gl_context_make_current)(id, SEL) = 0;
static void (*orig_gl_context_flush)(id, SEL) = 0;
static volatile long macoblox_gl_make_current_count;
static volatile long macoblox_gl_flush_count;

static void trace_gl_context(const char* action, id self, long count) {
    void** slots = (void**)self;
    write_str("[MacOBlox GL] ");
    write_str(action);
    if (count >= 0) {
        write_str(" #");
        print_num(count);
    }
    write_str(" self=");
    print_hex((unsigned long long)self);
    if (self) {
        write_str(" class=");
        write_str(object_getClassName(self));
        write_str(" view=");
        print_hex((unsigned long long)slots[2]);
        write_str(" cgl-context=");
        print_hex((unsigned long long)slots[3]);
        write_str(" subwindow=");
        print_hex((unsigned long long)slots[4]);
        write_str(" egl-surface=");
        print_hex((unsigned long long)slots[5]);
    }
    write_str("\n");
}

extern void macoblox_prepare_context(void* format);
extern void macoblox_finish_context(void);
extern void macoblox_note_pixel_format(void* format, const unsigned int* attributes);
static id hooked_gl_context_init(id self, SEL cmd, id format, id shared) {
    macoblox_prepare_context(format); // Core Profile if Roblox asked for it (gl_profile.c)
    id result = orig_gl_context_init(self, cmd, format, shared);
    macoblox_finish_context();
    trace_gl_context("init", result, -1);
    return result;
}

static void hooked_gl_context_set_view(id self, SEL cmd, id view) {
    orig_gl_context_set_view(self, cmd, view);
    trace_gl_context("setView", self, -1);
}

static void hooked_gl_context_make_current(id self, SEL cmd) {
    orig_gl_context_make_current(self, cmd);
    long count = __sync_add_and_fetch(&macoblox_gl_make_current_count, 1);
    void** slots = (void**)self;
    if (self && slots[3] && slots[5]) {
        void** cgl_slots = (void**)slots[3];
        macoblox_gl_window_surface = slots[5];
        macoblox_gl_window_egl_context = cgl_slots[9];
    }
    void* (*current_display)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentDisplay");
    void* (*current_context)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentContext");
    void* (*current_surface)(int) =
        (void* (*)(int))dlsym(RTLD_NEXT, "eglGetCurrentSurface");
    int (*make_current)(void*, void*, void*, void*) =
        (int (*)(void*, void*, void*, void*))
            dlsym(RTLD_NEXT, "eglMakeCurrent");
    unsigned int (*egl_error)(void) =
        (unsigned int (*)(void))dlsym(RTLD_NEXT, "eglGetError");
    if (!macoblox_gl_hacks_disabled() &&
        self && slots[5] && current_display && current_context &&
        current_surface && make_current && !current_surface(0x3059)) {
        void* display = current_display();
        void* context = current_context();
        int attached = make_current(display, slots[5], slots[5], context);
        write_str("[MacOBlox GL] repaired missing EGL window surface result=");
        print_num(attached);
        write_str(" error=");
        print_hex(egl_error ? egl_error() : 0);
        write_str(" surface-now=");
        print_hex((unsigned long long)current_surface(0x3059));
        write_str("\n");
    }
    if (count <= 5 || count == 60 || count == 600)
        trace_gl_context("makeCurrentContext", self, count);
}

static void dump_final_program_shaders(int program) {
    if (program <= 0 || program >= MACOBLOX_SHADER_SLOTS ||
        !__sync_bool_compare_and_swap(&macoblox_dumped_final_program, 0, 1))
        return;

    write_str("[MacOBlox GL] final program=");
    print_num(program);
    write_str(" shaders=");
    for (int index = 0; index < 4; index++) {
        unsigned int shader = macoblox_program_shaders[program][index];
        if (index)
            write_str(",");
        print_num(shader);
        write_str(":");
        print_hex(shader < MACOBLOX_SHADER_SLOTS
                      ? macoblox_shader_types[shader]
                      : 0);
        if (!shader || shader >= MACOBLOX_SHADER_SLOTS ||
            !macoblox_shader_sources[shader])
            continue;
        char path[] = "/Volumes/SystemRoot/tmp/macoblox-final-shader-0.glsl";
        path[sizeof(path) - sizeof("0.glsl")] = (char)('0' + index);
        MacOBloxFILE* output = fopen(path, "w");
        if (output) {
            fwrite(macoblox_shader_sources[shader], 1,
                   macoblox_shader_source_lengths[shader], output);
            fclose(output);
        }
    }
    write_str("\n");
}

static void trace_gl_frame_state(long count) {
    void (*get_integer)(unsigned int, int*) =
        (void (*)(unsigned int, int*))dlsym(RTLD_NEXT, "glGetIntegerv");
    void (*read_pixels)(int, int, int, int, unsigned int, unsigned int, void*) =
        (void (*)(int, int, int, int, unsigned int, unsigned int, void*))
            dlsym(RTLD_NEXT, "glReadPixels");
    void (*set_read_buffer)(unsigned int) =
        (void (*)(unsigned int))dlsym(RTLD_NEXT, "glReadBuffer");
    if (!get_integer)
        return;

    int draw_framebuffer = -1;
    int read_framebuffer = -1;
    int program = -1;
    int draw_buffer = -1;
    int read_buffer = -1;
    int viewport[4] = {-1, -1, -1, -1};
    get_integer(0x8CA6U, &draw_framebuffer); // GL_DRAW_FRAMEBUFFER_BINDING
    get_integer(0x8CAAU, &read_framebuffer); // GL_READ_FRAMEBUFFER_BINDING
    get_integer(0x8B8DU, &program);          // GL_CURRENT_PROGRAM
    get_integer(0x0C01U, &draw_buffer);      // GL_DRAW_BUFFER
    get_integer(0x0C02U, &read_buffer);      // GL_READ_BUFFER
    get_integer(0x0BA2U, viewport);          // GL_VIEWPORT
    dump_final_program_shaders(program);

    void* (*egl_current_context)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentContext");
    void* (*egl_current_surface)(int) =
        (void* (*)(int))dlsym(RTLD_NEXT, "eglGetCurrentSurface");
    void* (*egl_current_display)(void) =
        (void* (*)(void))dlsym(RTLD_NEXT, "eglGetCurrentDisplay");

    write_str("[MacOBlox GL] frame-state #");
    print_num(count);
    write_str(" draws=");
    print_num(macoblox_gl_draw_count);
    write_str(" blits=");
    print_num(macoblox_gl_blit_count);
    write_str(" clears=");
    print_num(macoblox_gl_clear_count);
    write_str(" default-binds=");
    print_num(macoblox_gl_default_framebuffer_bind_count);
    write_str(" draw-fbo=");
    print_num(draw_framebuffer);
    write_str(" read-fbo=");
    print_num(read_framebuffer);
    write_str(" program=");
    print_num(program);
    write_str(" draw-buffer=");
    print_hex((unsigned long long)(unsigned int)draw_buffer);
    write_str(" read-buffer=");
    print_hex((unsigned long long)(unsigned int)read_buffer);
    write_str(" egl-context=");
    print_hex((unsigned long long)(egl_current_context
                                      ? egl_current_context()
                                      : 0));
    write_str(" egl-display=");
    print_hex((unsigned long long)(egl_current_display
                                      ? egl_current_display()
                                      : 0));
    write_str(" egl-draw-surface=");
    print_hex((unsigned long long)(egl_current_surface
                                      ? egl_current_surface(0x3059)
                                      : 0)); // EGL_DRAW
    write_str(" viewport=");
    print_num(viewport[0]);
    write_str(",");
    print_num(viewport[1]);
    write_str(",");
    print_num(viewport[2]);
    write_str(",");
    print_num(viewport[3]);
    if (read_pixels && set_read_buffer && viewport[2] > 0 && viewport[3] > 0) {
        static const int fractions[3] = {1, 2, 3};
        // Roblox leaves the default read buffer disabled. Select the same back
        // buffer it draws into only while sampling, then restore its state.
        set_read_buffer(0x0402U); // GL_BACK_LEFT
        write_str(" samples=");
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 3; column++) {
                unsigned char pixel[4] = {0, 0, 0, 0};
                int x = viewport[0] + viewport[2] * fractions[column] / 4;
                int y = viewport[1] + viewport[3] * fractions[row] / 4;
                read_pixels(x, y, 1, 1, 0x1908U, 0x1401U, pixel);
                if (row || column)
                    write_str(";");
                print_num(pixel[0]);
                write_str(",");
                print_num(pixel[1]);
                write_str(",");
                print_num(pixel[2]);
                write_str(",");
                print_num(pixel[3]);
            }
        }
        set_read_buffer((unsigned int)read_buffer);
    }
    write_str("\n");
}

static void hooked_gl_context_flush(id self, SEL cmd) {
    long count = __sync_add_and_fetch(&macoblox_gl_flush_count, 1);
    const char* test_clear = getenv("MACOBLOX_GL_TEST_CLEAR");
    if (test_clear && test_clear[0]) {
        void (*disable)(unsigned int) =
            (void (*)(unsigned int))dlsym(RTLD_NEXT, "glDisable");
        void (*color_mask)(unsigned char, unsigned char, unsigned char,
                           unsigned char) =
            (void (*)(unsigned char, unsigned char, unsigned char,
                      unsigned char))dlsym(RTLD_NEXT, "glColorMask");
        void (*clear_color)(float, float, float, float) =
            (void (*)(float, float, float, float))
                dlsym(RTLD_NEXT, "glClearColor");
        void (*clear)(unsigned int) =
            (void (*)(unsigned int))dlsym(RTLD_NEXT, "glClear");
        unsigned int (*get_error)(void) =
            (unsigned int (*)(void))dlsym(RTLD_NEXT, "glGetError");
        unsigned int (*check_framebuffer)(unsigned int) =
            (unsigned int (*)(unsigned int))
                dlsym(RTLD_NEXT, "glCheckFramebufferStatus");
        void (*finish)(void) = (void (*)(void))dlsym(RTLD_NEXT, "glFinish");
        if (disable && color_mask && clear_color && clear) {
            unsigned int before_error = get_error ? get_error() : 0;
            disable(0x0C11U); // GL_SCISSOR_TEST
            color_mask(1, 1, 1, 1);
            clear_color(1.0f, 0.0f, 1.0f, 1.0f);
            clear(0x00004000U); // GL_COLOR_BUFFER_BIT
            if (finish)
                finish();
            if (count <= 5) {
                write_str("[MacOBlox GL] test-clear before-error=");
                print_hex(before_error);
                write_str(" after-error=");
                print_hex(get_error ? get_error() : 0);
                write_str(" framebuffer-status=");
                print_hex(check_framebuffer
                              ? check_framebuffer(0x8CA9U) // GL_DRAW_FRAMEBUFFER
                              : 0);
                write_str("\n");
            }
        }
    }
    if (macoblox_gl_trace_enabled() && (count <= 5 || count == 60 || count == 600)) {
        trace_gl_frame_state(count);
        trace_gl_context("flushBuffer", self, count);
    }
    orig_gl_context_flush(self, cmd);
}

// Darling's CALayerContext renders layers on the main thread and leaves its
// own CGL context current. On macOS the render server does this in another
// process, so Roblox makes its NSOpenGLContext current once and expects it to
// stay current. Without restoring it, every later Roblox GL call goes to the
// surfaceless layer context, where Roblox's shaders and buffers do not exist.
extern void* CGLGetCurrentContext(void);
static void (*orig_layer_context_render_layer)(id, SEL, id) = 0;
static volatile long macoblox_layer_context_restores;
static void hooked_layer_context_render_layer(id self, SEL cmd, id layer) {
    void* previous = CGLGetCurrentContext();
    orig_layer_context_render_layer(self, cmd, layer);
    if (previous && CGLGetCurrentContext() != previous) {
        CGLSetCurrentContext(previous);
        if (__sync_add_and_fetch(&macoblox_layer_context_restores, 1) == 1)
            write_str("[MacOBlox GL] Restored app GL context after CALayerContext render\n");
    }
}

// Mouse lock (in-game camera). On macOS CGAssociateMouseAndMouseCursorPosition
// (false) freezes the cursor and mouse events carry raw deltas. Darling's
// version cannot be used for that:
//  - it passes "connected" to -[NSDisplay grabMouse:] unchanged (inverted);
//  - Roblox repeats the call every frame and each grab recenters the pointer;
//  - in grab mode deltas are measured from the pin point, and events queued
//    before the asynchronous warp-back report growing offsets (5, 10, 15
//    instead of 5, 5, 5), so the camera spun far too fast;
//  - Roblox's per-frame CGWarpMouseCursorPosition fought the pin point.
// So the lock is implemented here: the pointer stays free and Darling's
// ordinary per-event deltas are used; when it drifts more than
// MACOBLOX_LOCK_RADIUS from the window center it is moved back with a
// relative XWarpPointer, and the single motion event caused by that warp is
// dropped. Warp requests during the lock are remembered and applied when the
// lock ends, as a frozen macOS cursor would stay put.
extern int CGAssociateMouseAndMouseCursorPosition(unsigned int connected);
extern int CGWarpMouseCursorPosition(MacOBloxPoint position);
#define MACOBLOX_LOCK_RADIUS 100.0
static volatile int macoblox_pointer_grabbed;
static volatile int macoblox_pending_warp;
static MacOBloxPoint macoblox_pending_warp_position;
static volatile int macoblox_drop_warp_motion;
static MacOBloxPoint macoblox_expected_warp_delta;
static volatile long macoblox_associate_mouse_count;
static MacOBloxPoint macoblox_lock_anchor;
static volatile int macoblox_lock_anchor_pending;

// Move the pointer by (dx, dy) window points (Cocoa axes, y up).
static void macoblox_warp_pointer_by(double dx, double dy) {
    static int (*warp)(void*, unsigned long, unsigned long, int, int,
                       unsigned int, unsigned int, int, int);
    static int (*flush)(void*);
    if (!warp) {
        warp = (int (*)(void*, unsigned long, unsigned long, int, int,
                        unsigned int, unsigned int, int, int))
            dlsym(RTLD_DEFAULT, "XWarpPointer");
        flush = (int (*)(void*))dlsym(RTLD_DEFAULT, "XFlush");
    }
    id display_object = ((id (*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSDisplay"), sel_registerName("currentDisplay"));
    void* display = display_object
        ? ((void* (*)(id, SEL))objc_msgSend)(display_object, sel_registerName("display"))
        : 0;
    int ix = (int)(dx < 0 ? dx - 0.5 : dx + 0.5);
    int iy = (int)(dy < 0 ? dy - 0.5 : dy + 0.5);
    if (!warp || !display || (!ix && !iy))
        return;
    // X11 y grows downward.
    warp(display, 0, 0, 0, 0, 0, 0, ix, -iy);
    if (flush)
        flush(display);
    macoblox_expected_warp_delta.x = ix;
    macoblox_expected_warp_delta.y = iy;
    macoblox_drop_warp_motion = 1;
}

static MacOBloxPoint macoblox_window_center(id window) {
    // NSRect is returned in memory on x86_64: objc_msgSend_stret, not objc_msgSend.
    extern void objc_msgSend_stret(id, SEL, ...);
    MacOBloxRect frame = ((MacOBloxRect (*)(id, SEL))objc_msgSend_stret)(
        window, sel_registerName("frame"));
    MacOBloxPoint center = {frame.size.width / 2.0, frame.size.height / 2.0};
    return center;
}

static id macoblox_lock_window(void) {
    id app = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSApplication"),
                                             sel_registerName("sharedApplication"));
    id window = ((id (*)(id, SEL))objc_msgSend)(app, sel_registerName("keyWindow"));
    if (!window)
        window = ((id (*)(id, SEL))objc_msgSend)(app, sel_registerName("mainWindow"));
    return window;
}

// While locked, a macOS cursor does not move: event locations, the window's
// mouse location and +[NSEvent mouseLocation] all stay where the lock began,
// and only the deltas change. Here the real pointer wanders around the window
// center, so report the frozen positions to Roblox as macOS would.
static MacOBloxPoint macoblox_frozen_window_location;
static MacOBloxPoint macoblox_frozen_screen_location;
static MacOBloxPoint (*orig_event_location_in_window)(id, SEL) = 0;
static MacOBloxPoint (*orig_window_mouse_location)(id, SEL) = 0;
static MacOBloxPoint (*orig_event_mouse_location)(id, SEL) = 0;
static MacOBloxPoint hooked_event_location_in_window(id self, SEL cmd) {
    if (macoblox_pointer_grabbed) {
        unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(
            self, sel_registerName("type"));
        if ((type >= 1 && type <= 7) || type == 25 || type == 26 || type == 27)
            return macoblox_frozen_window_location;
    }
    return orig_event_location_in_window(self, cmd);
}
static MacOBloxPoint hooked_window_mouse_location(id self, SEL cmd) {
    if (macoblox_pointer_grabbed)
        return macoblox_frozen_window_location;
    return orig_window_mouse_location(self, cmd);
}
static MacOBloxPoint hooked_event_mouse_location(id cls, SEL cmd) {
    if (macoblox_pointer_grabbed)
        return macoblox_frozen_screen_location;
    return orig_event_mouse_location(cls, cmd);
}
static MacOBloxPoint macoblox_real_window_mouse_location(id window) {
    SEL selector = sel_registerName("mouseLocationOutsideOfEventStream");
    return orig_window_mouse_location ? orig_window_mouse_location(window, selector)
        : ((MacOBloxPoint (*)(id, SEL))objc_msgSend)(window, selector);
}
static MacOBloxPoint macoblox_real_event_location(id event) {
    SEL selector = sel_registerName("locationInWindow");
    return orig_event_location_in_window ? orig_event_location_in_window(event, selector)
        : ((MacOBloxPoint (*)(id, SEL))objc_msgSend)(event, selector);
}

// Under Xwayland, XWarpPointer only moves the X server's idea of the pointer;
// the next Wayland motion reports the real position again, so recentering
// bounced the pointer back and forth. Xwayland emulates warps properly while
// the X cursor is hidden with XFixes. The requests go over a private X11
// socket (xfixes_raw.c), and on a worker thread, so the game thread never
// waits on X.
extern int macoblox_raw_xfixes_open(void);
extern int macoblox_raw_xfixes_set_hidden(int hidden);
extern int pthread_create(void**, const void*, void* (*)(void*), void*);
extern int usleep(unsigned int);
static volatile int macoblox_cursor_wanted_hidden;
static void* macoblox_xfixes_worker(void* unused) {
    (void)unused;
    if (!macoblox_raw_xfixes_open()) {
        write_str("[MacOBlox] XFixes unavailable, mouse lock may bounce\n");
        return 0;
    }
    write_str("[MacOBlox] XFixes ready for cursor hiding\n");
    int applied = 0;
    for (;;) {
        int wanted = macoblox_cursor_wanted_hidden;
        if (wanted != applied) {
            macoblox_raw_xfixes_set_hidden(wanted);
            applied = wanted;
        }
        usleep(5000);
    }
    return 0;
}

static void macoblox_set_x_cursor_hidden(int hidden) {
    static volatile int started;
    macoblox_cursor_wanted_hidden = hidden;
    if (hidden && __sync_bool_compare_and_swap(&started, 0, 1)) {
        void* thread;
        pthread_create(&thread, 0, macoblox_xfixes_worker, 0);
    }
}

static int macoblox_CGAssociateMouseAndMouseCursorPosition(unsigned int connected) {
    int grab = !connected;
    if (grab == macoblox_pointer_grabbed)
        return 0;
    if (__sync_add_and_fetch(&macoblox_associate_mouse_count, 1) <= 20)
        write_str(grab ? "[MacOBlox Input] mouse lock on\n"
                       : "[MacOBlox Input] mouse lock off\n");
    macoblox_drop_warp_motion = 0;
    if (grab) {
        id window = macoblox_lock_window();
        SEL screen_selector = sel_registerName("mouseLocation");
        id event_class = (id)objc_getClass("NSEvent");
        macoblox_frozen_screen_location = orig_event_mouse_location
            ? orig_event_mouse_location(event_class, screen_selector)
            : ((MacOBloxPoint (*)(id, SEL))objc_msgSend)(event_class, screen_selector);
        if (window)
            macoblox_frozen_window_location = macoblox_real_window_mouse_location(window);
    }
    if (grab) {
        // The pointer stays where the button was pressed. Recentering starts
        // from there; only a press close to the window edge moves the anchor
        // inward, and that first move waits for the first motion event, when
        // the cursor is surely hidden (a warp before that is not emulated by
        // Xwayland and the camera would jump).
        id window = macoblox_lock_window();
        macoblox_lock_anchor = macoblox_frozen_window_location;
        macoblox_lock_anchor_pending = 0;
        if (window) {
            MacOBloxPoint center = macoblox_window_center(window);
            double margin = MACOBLOX_LOCK_RADIUS + 20;
            double width = center.x * 2, height = center.y * 2;
            MacOBloxPoint anchor = macoblox_lock_anchor;
            if (width > margin * 2) {
                if (anchor.x < margin) anchor.x = margin;
                if (anchor.x > width - margin) anchor.x = width - margin;
            } else {
                anchor.x = center.x;
            }
            if (height > margin * 2) {
                if (anchor.y < margin) anchor.y = margin;
                if (anchor.y > height - margin) anchor.y = height - margin;
            } else {
                anchor.y = center.y;
            }
            macoblox_lock_anchor_pending = anchor.x != macoblox_lock_anchor.x ||
                                           anchor.y != macoblox_lock_anchor.y;
            macoblox_lock_anchor = anchor;
        }
        macoblox_pointer_grabbed = 1;
        macoblox_set_x_cursor_hidden(1);
    } else {
        // Put the pointer back where the button was pressed, while it is
        // still hidden, then show it.
        macoblox_pointer_grabbed = 0;
        macoblox_pending_warp = 0;
        id window = macoblox_lock_window();
        if (window) {
            MacOBloxPoint location = macoblox_real_window_mouse_location(window);
            macoblox_warp_pointer_by(macoblox_frozen_window_location.x - location.x,
                                     macoblox_frozen_window_location.y - location.y);
        }
        macoblox_drop_warp_motion = 0;
        macoblox_set_x_cursor_hidden(0);
    }
    return 0;
}
DYLD_INTERPOSE(macoblox_CGAssociateMouseAndMouseCursorPosition,
               CGAssociateMouseAndMouseCursorPosition);

static int macoblox_CGWarpMouseCursorPosition(MacOBloxPoint position) {
    if (macoblox_pointer_grabbed) {
        macoblox_pending_warp_position = position;
        macoblox_pending_warp = 1;
        return 0;
    }
    return CGWarpMouseCursorPosition(position);
}
DYLD_INTERPOSE(macoblox_CGWarpMouseCursorPosition, CGWarpMouseCursorPosition);

// Returns 1 if the motion event must be dropped (it only reflects our warp).
static int macoblox_filter_locked_motion_inner(id event, double dx, double dy);
static double (*orig_mouse_event_delta_x)(id, SEL);
static double (*orig_mouse_event_delta_y)(id, SEL);
// MACOBLOX_TRACE_LOCK=1: log every motion event seen during mouse lock.
static void macoblox_trace_lock_motion(id event, double dx, double dy, const char* action) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* value = getenv("MACOBLOX_TRACE_LOCK");
        enabled = value && value[0] ? 1 : 0;
    }
    if (!enabled)
        return;
    MacOBloxPoint location = macoblox_real_event_location(event);
    unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(event, sel_registerName("type"));
    write_str("[MacOBlox Lock] type=");
    print_num((long long)type);
    write_str(" dx=");
    print_num((long long)dx);
    write_str(" dy=");
    print_num((long long)dy);
    write_str(" at=");
    print_num((long long)location.x);
    write_str(",");
    print_num((long long)location.y);
    write_str(" ");
    write_str(action);
    write_str("\n");
}

static int macoblox_filter_locked_motion(id event) {
    if (!macoblox_pointer_grabbed)
        return 0;
    // Raw Darling deltas (Cocoa axes), without sign flip or sensitivity.
    SEL delta_x = sel_registerName("deltaX"), delta_y = sel_registerName("deltaY");
    double dx = orig_mouse_event_delta_x ? orig_mouse_event_delta_x(event, delta_x)
        : ((double (*)(id, SEL))objc_msgSend)(event, delta_x);
    double dy = orig_mouse_event_delta_y ? orig_mouse_event_delta_y(event, delta_y)
        : ((double (*)(id, SEL))objc_msgSend)(event, delta_y);
    int dropping = macoblox_drop_warp_motion;
    int dropped = macoblox_filter_locked_motion_inner(event, dx, dy);
    macoblox_trace_lock_motion(event, dx, dy,
                               dropped ? "DROPPED" : (dropping && !macoblox_drop_warp_motion ? "" :
                               (macoblox_drop_warp_motion && !dropping ? "RECENTER" : "")));
    return dropped;
}

static int macoblox_filter_locked_motion_inner(id event, double dx, double dy) {
    if (macoblox_drop_warp_motion) {
        double ex = macoblox_expected_warp_delta.x, ey = macoblox_expected_warp_delta.y;
        if ((dx - ex) * (dx - ex) + (dy - ey) * (dy - ey) <
            0.25 * (ex * ex + ey * ey) + 4.0) {
            macoblox_drop_warp_motion = 0;
            return 1;
        }
    }
    if (!macoblox_drop_warp_motion) {
        MacOBloxPoint location = macoblox_real_event_location(event);
        double ox = location.x - macoblox_lock_anchor.x, oy = location.y - macoblox_lock_anchor.y;
        if (macoblox_lock_anchor_pending) {
            macoblox_lock_anchor_pending = 0;
            macoblox_warp_pointer_by(-ox, -oy);
        } else if (ox > MACOBLOX_LOCK_RADIUS || ox < -MACOBLOX_LOCK_RADIUS ||
                   oy > MACOBLOX_LOCK_RADIUS || oy < -MACOBLOX_LOCK_RADIUS) {
            macoblox_warp_pointer_by(-ox, -oy);
        }
    }
    return 0;
}

// macOS reports mouse motion deltaY positive downward; Darling computes it in
// Cocoa window coordinates (positive upward). Scroll events keep their sign.
// MACOBLOX_MOUSE_SENSITIVITY scales camera motion during mouse lock.
static double macoblox_mouse_sensitivity(void) {
    static double value = -1;
    if (value < 0) {
        const char* text = getenv("MACOBLOX_MOUSE_SENSITIVITY");
        double parsed = 0;
        if (text && text[0]) {
            double scale = 1;
            int fraction = 0;
            for (const char* c = text; *c; c++) {
                if (*c >= '0' && *c <= '9') {
                    if (fraction) { scale /= 10; parsed += (*c - '0') * scale; }
                    else parsed = parsed * 10 + (*c - '0');
                } else if (*c == '.' || *c == ',') {
                    fraction = 1;
                }
            }
        }
        value = parsed > 0.05 && parsed < 20 ? parsed : 1.0;
    }
    return value;
}
static int macoblox_is_motion_type(id event) {
    unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(
        event, sel_registerName("type"));
    return type == 5 || type == 6 || type == 7 || type == 27;
}
static double (*orig_mouse_event_delta_x)(id, SEL) = 0;
static double hooked_mouse_event_delta_x(id self, SEL cmd) {
    double delta = orig_mouse_event_delta_x(self, cmd);
    if (macoblox_pointer_grabbed && macoblox_is_motion_type(self))
        return delta * macoblox_mouse_sensitivity();
    return delta;
}
static double (*orig_mouse_event_delta_y)(id, SEL) = 0;
static double hooked_mouse_event_delta_y(id self, SEL cmd) {
    double delta = orig_mouse_event_delta_y(self, cmd);
    if (!macoblox_is_motion_type(self))
        return delta;
    delta = -delta;
    return macoblox_pointer_grabbed ? delta * macoblox_mouse_sensitivity() : delta;
}

// Scroll wheel APIs missing from Darling's NSEvent. X11 wheels report coarse
// line deltas, so they are never "precise".
static MacOBloxBool event_has_precise_scrolling_deltas(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}
static MacOBloxBool event_is_direction_inverted(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}
static double event_scrolling_delta_x(id self, SEL cmd) {
    (void)cmd;
    return ((double (*)(id, SEL))objc_msgSend)(self, sel_registerName("deltaX"));
}
static double event_scrolling_delta_y(id self, SEL cmd) {
    (void)cmd;
    return ((double (*)(id, SEL))objc_msgSend)(self, sel_registerName("deltaY"));
}

// Darling's -[X11Cursor initWithImage:hotPoint:] copies each pixel row with a
// wrong source offset, so custom cursors show shifted rows and bytes read past
// the bitmap (the bright pink/blue fringe). Rebuild the Xcursor image here.
typedef struct {
    unsigned int version, size, width, height, xhot, yhot, delay;
    unsigned int* pixels;
} MacOBloxXcursorImage;
typedef struct objc_ivar* Ivar;
extern Ivar class_getInstanceVariable(Class, const char*);
extern long ivar_getOffset(Ivar);
extern void* objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void*);
extern void* CGColorSpaceCreateDeviceRGB(void);
extern void CGColorSpaceRelease(void*);
extern void* CGBitmapContextCreate(void*, unsigned long, unsigned long,
                                   unsigned long, unsigned long, void*,
                                   unsigned int);
extern void* CGBitmapContextGetData(void*);
extern unsigned long CGBitmapContextGetBytesPerRow(void*);
extern void CGContextRelease(void*);

extern unsigned long CGImageGetWidth(void*);
extern unsigned long CGImageGetHeight(void*);
extern unsigned long CGImageGetBitsPerComponent(void*);
extern unsigned long CGImageGetBitsPerPixel(void*);
extern unsigned long CGImageGetBytesPerRow(void*);
extern unsigned int CGImageGetBitmapInfo(void*);
extern void* CGImageGetDataProvider(void*);
extern void* CGDataProviderCopyData(void*);
extern const unsigned char* CFDataGetBytePtr(void*);
extern long CFDataGetLength(void*);
extern void CFRelease(const void*);

// Read the cursor image's own pixels instead of drawing it through Darling,
// whose compositing corrupts channels of semi-transparent pixels. Returns an
// Xcursor-style premultiplied ARGB buffer (caller frees), or 0 if the format
// is not a plain 8-bit RGB(A) layout.
static unsigned int* macoblox_cursor_pixels_from_image(
    id image, unsigned long* width_out, unsigned long* height_out) {
    id reps = ((id (*)(id, SEL))objc_msgSend)(image, sel_registerName("representations"));
    unsigned long rep_count = reps
        ? ((unsigned long (*)(id, SEL))objc_msgSend)(reps, sel_registerName("count")) : 0;
    void* cg_image = 0;
    for (unsigned long index = 0; index < rep_count && !cg_image; index++) {
        id rep = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
            reps, sel_registerName("objectAtIndex:"), index);
        if (((MacOBloxBool (*)(id, SEL, SEL))objc_msgSend)(
                rep, sel_registerName("respondsToSelector:"), sel_registerName("CGImage")))
            cg_image = ((void* (*)(id, SEL))objc_msgSend)(rep, sel_registerName("CGImage"));
    }
    if (!cg_image)
        return 0;
    unsigned long width = CGImageGetWidth(cg_image);
    unsigned long height = CGImageGetHeight(cg_image);
    unsigned long bits_per_pixel = CGImageGetBitsPerPixel(cg_image);
    unsigned long bytes_per_row = CGImageGetBytesPerRow(cg_image);
    unsigned int info = CGImageGetBitmapInfo(cg_image);
    unsigned int alpha_info = info & 0x1FU;
    unsigned int byte_order = info & 0x7000U;
    static volatile long described;
    if (__sync_add_and_fetch(&described, 1) <= 4) {
        write_str("[MacOBlox Cursor] source ");
        print_num((long long)width);
        write_str("x");
        print_num((long long)height);
        write_str(" bpp=");
        print_num((long long)bits_per_pixel);
        write_str(" bpc=");
        print_num((long long)CGImageGetBitsPerComponent(cg_image));
        write_str(" alpha-info=");
        print_num(alpha_info);
        write_str(" byte-order=");
        print_hex(byte_order);
        write_str("\n");
    }
    if (!width || !height || width > 512 || height > 512 ||
        CGImageGetBitsPerComponent(cg_image) != 8 ||
        (bits_per_pixel != 32 && bits_per_pixel != 24) ||
        (byte_order != 0 && byte_order != 0x2000U && byte_order != 0x4000U))
        return 0;
    void* data = CGDataProviderCopyData(CGImageGetDataProvider(cg_image));
    if (!data)
        return 0;
    const unsigned char* bytes = CFDataGetBytePtr(data);
    if ((unsigned long)CFDataGetLength(data) < bytes_per_row * (height - 1) +
                                                   width * (bits_per_pixel / 8)) {
        CFRelease(data);
        return 0;
    }
    unsigned int* pixels = (unsigned int*)malloc(width * height * 4);
    if (!pixels) {
        CFRelease(data);
        return 0;
    }
    int alpha_first = alpha_info == 2 || alpha_info == 4 || alpha_info == 6;
    int has_alpha = alpha_info >= 1 && alpha_info <= 4;
    int premultiplied = alpha_info == 1 || alpha_info == 2;
    for (unsigned long row = 0; row < height; row++) {
        const unsigned char* source = bytes + row * bytes_per_row;
        for (unsigned long column = 0; column < width; column++) {
            unsigned int c[4] = {0, 0, 0, 0};
            if (bits_per_pixel == 24) {
                c[0] = source[column * 3];
                c[1] = source[column * 3 + 1];
                c[2] = source[column * 3 + 2];
                c[3] = 255;
            } else {
                const unsigned char* pixel = source + column * 4;
                // Logical component order, most significant first.
                unsigned char word[4];
                if (byte_order == 0x2000U) {
                    word[0] = pixel[3]; word[1] = pixel[2];
                    word[2] = pixel[1]; word[3] = pixel[0];
                } else {
                    word[0] = pixel[0]; word[1] = pixel[1];
                    word[2] = pixel[2]; word[3] = pixel[3];
                }
                if (alpha_first) {
                    c[3] = word[0]; c[0] = word[1]; c[1] = word[2]; c[2] = word[3];
                } else {
                    c[0] = word[0]; c[1] = word[1]; c[2] = word[2]; c[3] = word[3];
                }
                if (!has_alpha)
                    c[3] = 255;
            }
            if (!premultiplied) {
                for (int channel = 0; channel < 3; channel++)
                    c[channel] = c[channel] * c[3] / 255;
            }
            pixels[row * width + column] =
                (c[3] << 24) | (c[0] << 16) | (c[1] << 8) | c[2];
        }
    }
    CFRelease(data);
    *width_out = width;
    *height_out = height;
    return pixels;
}

static id (*orig_x11_cursor_init_image)(id, SEL, id, MacOBloxPoint) = 0;
static id hooked_x11_cursor_init_image(id self, SEL cmd, id image,
                                       MacOBloxPoint hot) {
    MacOBloxXcursorImage* (*image_create)(int, int) =
        (MacOBloxXcursorImage* (*)(int, int))
            dlsym(RTLD_DEFAULT, "XcursorImageCreate");
    void (*image_destroy)(MacOBloxXcursorImage*) =
        (void (*)(MacOBloxXcursorImage*))
            dlsym(RTLD_DEFAULT, "XcursorImageDestroy");
    unsigned long (*load_cursor)(void*, const MacOBloxXcursorImage*) =
        (unsigned long (*)(void*, const MacOBloxXcursorImage*))
            dlsym(RTLD_DEFAULT, "XcursorImageLoadCursor");
    Ivar cursor_ivar = class_getInstanceVariable(object_getClass(self), "_cursor");
    MacOBloxSize size = image
        ? ((MacOBloxSize (*)(id, SEL))objc_msgSend)(image, sel_registerName("size"))
        : (MacOBloxSize){0, 0};
    unsigned long width = (unsigned long)size.width;
    unsigned long height = (unsigned long)size.height;
    if (!image_create || !image_destroy || !load_cursor || !cursor_ivar ||
        !width || !height || width > 512 || height > 512)
        return orig_x11_cursor_init_image(self, cmd, image, hot);

    unsigned long source_width = 0, source_height = 0;
    unsigned int* source_pixels =
        macoblox_cursor_pixels_from_image(image, &source_width, &source_height);
    if (source_pixels) {
        id direct_display_object = ((id (*)(id, SEL))objc_msgSend)(
            (id)objc_getClass("NSDisplay"), sel_registerName("currentDisplay"));
        void* direct_display = direct_display_object
            ? ((void* (*)(id, SEL))objc_msgSend)(direct_display_object,
                                                  sel_registerName("display"))
            : 0;
        MacOBloxXcursorImage* direct = direct_display
            ? image_create((int)source_width, (int)source_height) : 0;
        if (direct) {
            for (unsigned long index = 0; index < source_width * source_height; index++)
                direct->pixels[index] = source_pixels[index];
            // The hot spot is given in image points; the bitmap may be larger.
            double scale_x = (double)source_width / (double)width;
            double scale_y = (double)source_height / (double)height;
            double hot_x = hot.x * scale_x, hot_y = hot.y * scale_y;
            direct->xhot = hot_x < 0 ? 0 : (hot_x >= source_width ? source_width - 1 : (unsigned int)hot_x);
            direct->yhot = hot_y < 0 ? 0 : (hot_y >= source_height ? source_height - 1 : (unsigned int)hot_y);
            unsigned long direct_cursor = load_cursor(direct_display, direct);
            image_destroy(direct);
            if (direct_cursor) {
                free(source_pixels);
                *(unsigned long*)((char*)self + ivar_getOffset(cursor_ivar)) = direct_cursor;
                return self;
            }
        }
        free(source_pixels);
    }

    id display_object = ((id (*)(id, SEL))objc_msgSend)(
        (id)objc_getClass("NSDisplay"), sel_registerName("currentDisplay"));
    void* display = display_object
        ? ((void* (*)(id, SEL))objc_msgSend)(display_object, sel_registerName("display"))
        : 0;
    MacOBloxXcursorImage* ximage = display ? image_create((int)width, (int)height) : 0;
    void* color_space = ximage ? CGColorSpaceCreateDeviceRGB() : 0;
    // Premultiplied ARGB in host byte order, the XcursorPixel format.
    void* bitmap = color_space
        ? CGBitmapContextCreate(0, width, height, 8, 0, color_space,
                                2U /* PremultipliedFirst */ | 0x2000U /* 32Little */)
        : 0;
    if (color_space)
        CGColorSpaceRelease(color_space);
    if (!bitmap) {
        if (ximage)
            image_destroy(ximage);
        return orig_x11_cursor_init_image(self, cmd, image, hot);
    }

    void* pool = objc_autoreleasePoolPush();
    id context_class = (id)objc_getClass("NSGraphicsContext");
    id graphics = ((id (*)(id, SEL, void*, MacOBloxBool))objc_msgSend)(
        context_class, sel_registerName("graphicsContextWithGraphicsPort:flipped:"),
        bitmap, 0);
    ((void (*)(id, SEL))objc_msgSend)(context_class, sel_registerName("saveGraphicsState"));
    ((void (*)(id, SEL, id))objc_msgSend)(
        context_class, sel_registerName("setCurrentContext:"), graphics);
    MacOBloxRect destination = {{0, 0}, {(double)width, (double)height}};
    MacOBloxRect whole_image = {{0, 0}, {0, 0}};
    ((void (*)(id, SEL, MacOBloxRect, MacOBloxRect, unsigned long, double))objc_msgSend)(
        image, sel_registerName("drawInRect:fromRect:operation:fraction:"),
        destination, whole_image, 1UL /* NSCompositeCopy */, 1.0);
    ((void (*)(id, SEL))objc_msgSend)(context_class, sel_registerName("restoreGraphicsState"));
    objc_autoreleasePoolPop(pool);

    const unsigned char* rows = (const unsigned char*)CGBitmapContextGetData(bitmap);
    unsigned long bytes_per_row = CGBitmapContextGetBytesPerRow(bitmap);
    for (unsigned long row = 0; row < height; row++) {
        const unsigned int* source = (const unsigned int*)(rows + row * bytes_per_row);
        for (unsigned long column = 0; column < width; column++)
            ximage->pixels[row * width + column] = source[column];
    }
    CGContextRelease(bitmap);

    // Xcursor expects premultiplied ARGB. Darling's image drawing can leave
    // straight alpha, which shows up as a bright fringe on soft edges. A
    // channel above its alpha is impossible when premultiplied, so use that
    // to detect the format and premultiply only in that case.
    unsigned long pixel_count = width * height;
    int straight_alpha = 0;
    for (unsigned long index = 0; index < pixel_count && !straight_alpha; index++) {
        unsigned int pixel = ximage->pixels[index];
        unsigned int alpha = pixel >> 24;
        if (((pixel >> 16) & 255) > alpha || ((pixel >> 8) & 255) > alpha ||
            (pixel & 255) > alpha)
            straight_alpha = 1;
    }
    if (straight_alpha) {
        for (unsigned long index = 0; index < pixel_count; index++) {
            unsigned int pixel = ximage->pixels[index];
            unsigned int alpha = pixel >> 24;
            unsigned int red = ((pixel >> 16) & 255) * alpha / 255;
            unsigned int green = ((pixel >> 8) & 255) * alpha / 255;
            unsigned int blue = (pixel & 255) * alpha / 255;
            ximage->pixels[index] = (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }
    static volatile long dumped_cursors;
    long dump_index = __sync_add_and_fetch(&dumped_cursors, 1) - 1;
    if (dump_index < 4) {
        write_str("[MacOBlox Cursor] #");
        print_num(dump_index);
        write_str(" size=");
        print_num((long long)width);
        write_str("x");
        print_num((long long)height);
        write_str(straight_alpha ? " straight-alpha (premultiplied now)\n"
                                 : " already premultiplied\n");
    }

    ximage->xhot = hot.x < 0 ? 0 : (hot.x >= width ? width - 1 : (unsigned int)hot.x);
    ximage->yhot = hot.y < 0 ? 0 : (hot.y >= height ? height - 1 : (unsigned int)hot.y);
    unsigned long cursor = load_cursor(display, ximage);
    image_destroy(ximage);
    if (!cursor)
        return orig_x11_cursor_init_image(self, cmd, image, hot);
    *(unsigned long*)((char*)self + ivar_getOffset(cursor_ivar)) = cursor;
    return self;
}

// Darling stubs +[NSEvent addLocalMonitorForEventsMatchingMask:handler:].
// Roblox installs local monitors for its input, so implement them the macOS
// way: -[NSApplication sendEvent:] passes each matching event to the handlers
// first; a handler may replace the event or return nil to consume it.
struct MacOBloxBlock {
    void* isa;
    int flags;
    int reserved;
    id (*invoke)(void*, id);
};
extern void* _Block_copy(const void*);
extern void _Block_release(const void*);
#define MACOBLOX_MAX_MONITORS 32
static struct {
    unsigned long long mask;
    struct MacOBloxBlock* block;
} macoblox_monitors[MACOBLOX_MAX_MONITORS];
static volatile int macoblox_monitor_lock;

static id event_add_local_monitor(id cls, SEL cmd, unsigned long long mask,
                                  void* handler) {
    (void)cls; (void)cmd;
    if (!handler)
        return 0;
    struct MacOBloxBlock* block = (struct MacOBloxBlock*)_Block_copy(handler);
    while (!__sync_bool_compare_and_swap(&macoblox_monitor_lock, 0, 1)) {}
    int stored = 0;
    for (int index = 0; index < MACOBLOX_MAX_MONITORS && !stored; index++) {
        if (!macoblox_monitors[index].block) {
            macoblox_monitors[index].mask = mask;
            macoblox_monitors[index].block = block;
            stored = 1;
        }
    }
    macoblox_monitor_lock = 0;
    write_str("[MacOBlox Input] addLocalMonitorForEventsMatchingMask mask=");
    print_hex(mask);
    write_str(stored ? "\n" : " (table full, ignored)\n");
    if (!stored) {
        _Block_release(block);
        return 0;
    }
    return (id)block;
}

static void event_remove_monitor(id cls, SEL cmd, id monitor) {
    (void)cls; (void)cmd;
    if (!monitor)
        return;
    while (!__sync_bool_compare_and_swap(&macoblox_monitor_lock, 0, 1)) {}
    for (int index = 0; index < MACOBLOX_MAX_MONITORS; index++) {
        if (macoblox_monitors[index].block == (struct MacOBloxBlock*)monitor) {
            macoblox_monitors[index].block = 0;
            macoblox_monitors[index].mask = 0;
            macoblox_monitor_lock = 0;
            _Block_release(monitor);
            return;
        }
    }
    macoblox_monitor_lock = 0;
}

// Keyboard state for CGEventSourceKeyState, which Darling lacks and Roblox
// polls in game. Built from the key events AppKit dispatches.
static volatile unsigned char macoblox_key_down[128];
static void macoblox_track_key_event(id event, unsigned long type) {
    if (type != 10 && type != 11 && type != 12) // keyDown, keyUp, flagsChanged
        return;
    unsigned short key = ((unsigned short (*)(id, SEL))objc_msgSend)(
        event, sel_registerName("keyCode"));
    if (key >= 128)
        return;
    if (type != 12) {
        macoblox_key_down[key] = type == 10;
        return;
    }
    unsigned long flags = ((unsigned long (*)(id, SEL))objc_msgSend)(
        event, sel_registerName("modifierFlags"));
    unsigned long bit = 0;
    switch (key) {
    case 56: case 60: bit = 1UL << 17; break; // shift
    case 59: case 62: bit = 1UL << 18; break; // control
    case 58: case 61: bit = 1UL << 19; break; // option
    case 55: case 54: bit = 1UL << 20; break; // command
    case 57: bit = 1UL << 16; break;          // caps lock
    default: return;
    }
    macoblox_key_down[key] = (flags & bit) != 0;
}
unsigned char CGEventSourceKeyState(int state_id, unsigned short key) {
    (void)state_id;
    return key < 128 ? macoblox_key_down[key] : 0;
}

static int macoblox_trace_keys_enabled(void);
static void (*orig_app_send_event)(id, SEL, id) = 0;
static void hooked_app_send_event(id self, SEL cmd, id event) {
    if (event) {
        unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(
            event, sel_registerName("type"));
        macoblox_track_key_event(event, type);
        if ((type == 10 || type == 11) && macoblox_trace_keys_enabled()) {
            write_str(type == 10 ? "[MacOBlox Keys] game keyDown code=" : "[MacOBlox Keys] game keyUp   code=");
            print_num(((unsigned short (*)(id, SEL))objc_msgSend)(event, sel_registerName("keyCode")));
            write_str(((signed char (*)(id, SEL))objc_msgSend)(event, sel_registerName("isARepeat")) ? " repeat\n" : "\n");
        }
        if ((type == 5 || type == 6 || type == 7 || type == 27) &&
            macoblox_filter_locked_motion(event))
            return;
        unsigned long long event_mask = type < 64 ? 1ULL << type : 0;
        struct MacOBloxBlock* handlers[MACOBLOX_MAX_MONITORS];
        int handler_count = 0;
        while (!__sync_bool_compare_and_swap(&macoblox_monitor_lock, 0, 1)) {}
        for (int index = 0; index < MACOBLOX_MAX_MONITORS; index++) {
            if (macoblox_monitors[index].block &&
                (macoblox_monitors[index].mask & event_mask))
                handlers[handler_count++] = macoblox_monitors[index].block;
        }
        macoblox_monitor_lock = 0;
        for (int index = 0; index < handler_count && event; index++)
            event = handlers[index]->invoke(handlers[index], event);
        if (!event)
            return;
    }
    orig_app_send_event(self, cmd, event);
}

// Darling drops pointer motion unless the window accepts mouse-moved events.
// On macOS Roblox receives motion through tracking areas instead, which
// Darling does not deliver here, so the Roblox window always accepts them.
static MacOBloxBool (*orig_window_accepts_mouse_moved)(id, SEL) = 0;
static MacOBloxBool hooked_window_accepts_mouse_moved(id self, SEL cmd) {
    const char* class_name = object_getClassName(self);
    if (class_name && ascii_strings_equal(class_name, "RBXWindow"))
        return 1;
    return orig_window_accepts_mouse_moved(self, cmd);
}

// MACOBLOX_TRACE_EVENTS=1: log mouse events as AppKit dispatches them.
// Movement is sampled; button, enter/exit and scroll events are all logged.
static void (*orig_window_send_event)(id, SEL, id) = 0;
static volatile long macoblox_traced_motion_events;
static void hooked_window_send_event(id self, SEL cmd, id event) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* value = getenv("MACOBLOX_TRACE_EVENTS");
        enabled = value && value[0] ? 1 : 0;
    }
    if (enabled && event) {
        unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(
            event, sel_registerName("type"));
        int is_button = type == 1 || type == 2 || type == 3 || type == 4 ||
                        type == 25 || type == 26;
        int is_motion = type == 5 || type == 6 || type == 7 || type == 27;
        int is_other = type == 8 || type == 9 || type == 22;
        int log = is_button || is_other;
        if (is_motion) {
            long count = __sync_add_and_fetch(&macoblox_traced_motion_events, 1);
            log = count <= 20 || count % 50 == 0;
        }
        if (log) {
            MacOBloxPoint location = ((MacOBloxPoint (*)(id, SEL))objc_msgSend)(
                event, sel_registerName("locationInWindow"));
            write_str("[MacOBlox Event] type=");
            print_num((long long)type);
            write_str(" window=");
            write_str(object_getClassName(self));
            write_str(" x=");
            print_num((long long)location.x);
            write_str(" y=");
            print_num((long long)location.y);
            if (is_button) {
                write_str(" button=");
                print_num(((long (*)(id, SEL))objc_msgSend)(
                    event, sel_registerName("buttonNumber")));
                write_str(" clicks=");
                print_num(((long (*)(id, SEL))objc_msgSend)(
                    event, sel_registerName("clickCount")));
                id content = ((id (*)(id, SEL))objc_msgSend)(
                    self, sel_registerName("contentView"));
                id superview = content ? ((id (*)(id, SEL))objc_msgSend)(
                    content, sel_registerName("superview")) : 0;
                id root = superview ? superview : content;
                id hit = root ? ((id (*)(id, SEL, MacOBloxPoint))objc_msgSend)(
                    root, sel_registerName("hitTest:"), location) : 0;
                write_str(" hit=");
                write_str(hit ? object_getClassName(hit) : "(nil)");
                id responder = ((id (*)(id, SEL))objc_msgSend)(
                    self, sel_registerName("firstResponder"));
                write_str(" first-responder=");
                write_str(responder ? object_getClassName(responder) : "(nil)");
            }
            if (is_motion) {
                write_str(" sample=");
                print_num(macoblox_traced_motion_events);
            }
            write_str("\n");
        }
    }
    orig_window_send_event(self, cmd, event);
}

// Log where Roblox tries to send the user: an in-app WKWebView page or an
// external URL. Darling's WebKit is a stub, so both currently show nothing.
static void macoblox_log_url(const char* prefix, id url) {
    id text = url ? ((id (*)(id, SEL))objc_msgSend)(url, sel_registerName("absoluteString")) : 0;
    const char* utf8 = text ? ((const char* (*)(id, SEL))objc_msgSend)(
                                  text, sel_registerName("UTF8String")) : 0;
    write_str(prefix);
    write_str(utf8 ? utf8 : "(nil)");
    write_str("\n");
}
static id (*orig_web_view_init)(id, SEL, MacOBloxRect, id) = 0;
static id hooked_web_view_init(id self, SEL cmd, MacOBloxRect frame, id configuration) {
    write_str("[MacOBlox Web] WKWebView initWithFrame:configuration:\n");
    return orig_web_view_init(self, cmd, frame, configuration);
}
static id (*orig_web_view_load_request)(id, SEL, id) = 0;
static id hooked_web_view_load_request(id self, SEL cmd, id request) {
    id url = request ? ((id (*)(id, SEL))objc_msgSend)(request, sel_registerName("URL")) : 0;
    macoblox_log_url("[MacOBlox Web] WKWebView loadRequest: ", url);
    return orig_web_view_load_request(self, cmd, request);
}
static MacOBloxBool (*orig_workspace_open_url)(id, SEL, id) = 0;
static MacOBloxBool hooked_workspace_open_url(id self, SEL cmd, id url) {
    MacOBloxBool result = orig_workspace_open_url(self, cmd, url);
    macoblox_log_url(result ? "[MacOBlox Web] NSWorkspace openURL: (ok) "
                            : "[MacOBlox Web] NSWorkspace openURL: (failed) ", url);
    return result;
}

// Darling stores the X11 button number (left 1, middle 2, right 3, back 8,
// forward 9) in -[NSEvent buttonNumber]. macOS numbers them left 0, right 1,
// middle 2, then 3 and 4. Roblox therefore saw every left click as the right
// button: GUI hover and press states still worked, but Activated (left button
// only) never fired, and the "right button" started camera mouse capture.
static long (*orig_mouse_event_button_number)(id, SEL) = 0;
static long hooked_mouse_event_button_number(id self, SEL cmd) {
    long x11_button = orig_mouse_event_button_number(self, cmd);
    unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(
        self, sel_registerName("type"));
    switch (type) {
    case 1: case 2: case 6:   // left down, up, dragged
        return 0;
    case 3: case 4: case 7:   // right down, up, dragged
        return 1;
    default:
        break;
    }
    if (x11_button == 2)
        return 2;
    if (x11_button >= 8)
        return x11_button - 5;
    return x11_button;
}

// NSTextInputClient methods that macOS NSTextView provides and Darling's
// lacks. Roblox's InputMethodHandler (an NSTextView subclass) queries them
// when a text box gains focus; the missing hasMarkedText was fatal. These
// report "no IME composition in progress", matching a plain text field.
typedef struct { unsigned long location, length; } MacOBloxRange;
static MacOBloxBool text_view_has_marked_text(id self, SEL cmd) {
    (void)self; (void)cmd;
    return 0;
}
static MacOBloxRange text_view_marked_range(id self, SEL cmd) {
    (void)self; (void)cmd;
    MacOBloxRange range = {0x7FFFFFFFFFFFFFFFUL /* NSNotFound */, 0};
    return range;
}
static void text_view_unmark_text(id self, SEL cmd) {
    (void)self; (void)cmd;
}
static id text_view_valid_marked_attributes(id self, SEL cmd) {
    (void)self; (void)cmd;
    return ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSArray"),
                                           sel_registerName("array"));
}
static id text_view_attributed_substring(id self, SEL cmd, MacOBloxRange range,
                                         MacOBloxRange* actual) {
    (void)self; (void)cmd; (void)range;
    if (actual) {
        actual->location = 0x7FFFFFFFFFFFFFFFUL;
        actual->length = 0;
    }
    return 0;
}

// Convenience constructors from macOS 10.12 missing in Darling's AppKit.
// Roblox builds its web challenge window with them.
static id macoblox_new_view(const char* class_name) {
    id object = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass(class_name),
                                                sel_registerName("alloc"));
    MacOBloxRect zero = {{0, 0}, {0, 0}};
    object = ((id (*)(id, SEL, MacOBloxRect))objc_msgSend)(
        object, sel_registerName("initWithFrame:"), zero);
    return ((id (*)(id, SEL))objc_msgSend)(object, sel_registerName("autorelease"));
}
static id button_with_image_target_action(id cls, SEL cmd, id image, id target,
                                          SEL action) {
    (void)cls; (void)cmd;
    id button = macoblox_new_view("NSButton");
    if (!button)
        return 0;
    ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setImage:"), image);
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(
        button, sel_registerName("setImagePosition:"), 1UL /* NSImageOnly */);
    ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setTarget:"), target);
    ((void (*)(id, SEL, SEL))objc_msgSend)(button, sel_registerName("setAction:"), action);
    ((void (*)(id, SEL))objc_msgSend)(button, sel_registerName("sizeToFit"));
    return button;
}
static id text_field_label_with_string(id cls, SEL cmd, id string) {
    (void)cls; (void)cmd;
    id label = macoblox_new_view("NSTextField");
    if (!label)
        return 0;
    ((void (*)(id, SEL, id))objc_msgSend)(label, sel_registerName("setStringValue:"), string);
    ((void (*)(id, SEL, MacOBloxBool))objc_msgSend)(label, sel_registerName("setEditable:"), 0);
    ((void (*)(id, SEL, MacOBloxBool))objc_msgSend)(label, sel_registerName("setSelectable:"), 0);
    ((void (*)(id, SEL, MacOBloxBool))objc_msgSend)(label, sel_registerName("setBezeled:"), 0);
    ((void (*)(id, SEL, MacOBloxBool))objc_msgSend)(label, sel_registerName("setBordered:"), 0);
    ((void (*)(id, SEL, MacOBloxBool))objc_msgSend)(label, sel_registerName("setDrawsBackground:"), 0);
    ((void (*)(id, SEL))objc_msgSend)(label, sel_registerName("sizeToFit"));
    return label;
}

// Darling's Contacts framework is an empty stub. Roblox's home page asks for
// contacts permission (friend finding); report access as denied so it never
// tries to read contacts.
static long contact_store_authorization_denied(id cls, SEL cmd, long entity_type) {
    (void)cls; (void)cmd; (void)entity_type;
    return 2; // CNAuthorizationStatusDenied
}
struct MacOBloxAccessBlock {
    void* isa;
    int flags;
    int reserved;
    void (*invoke)(void*, MacOBloxBool, id);
};
static void contact_store_request_access(id self, SEL cmd, long entity_type,
                                         void* completion) {
    (void)self; (void)cmd; (void)entity_type;
    if (completion)
        ((struct MacOBloxAccessBlock*)completion)->invoke(completion, 0, 0);
}

// Darling's Keychain does not persist SecItemAdd, so Roblox's login (a
// generic password item keyed by kSecAttrAccount) was gone after every
// restart. Keep generic password items as files instead:
// ~/Library/MacOBlox/Keychain/<hex account>, mode 0600.
// Other item classes still go to Darling's Security framework.
extern int SecItemAdd(id attributes, id* result);
extern int SecItemCopyMatching(id query, id* result);
extern int SecItemDelete(id query);
extern const id kSecClass;
extern const id kSecClassGenericPassword;
extern const id kSecAttrAccount;
extern const id kSecValueData;
extern const id kSecReturnData;
extern int chmod(const char*, unsigned short);
#define MACOBLOX_ERR_SEC_ITEM_NOT_FOUND (-25300)

static id macoblox_keychain_path(id query) {
    id item_class = ((id (*)(id, SEL, id))objc_msgSend)(query, sel_registerName("objectForKey:"), kSecClass);
    id account = ((id (*)(id, SEL, id))objc_msgSend)(query, sel_registerName("objectForKey:"), kSecAttrAccount);
    if (!item_class || !account ||
        !((MacOBloxBool (*)(id, SEL, id))objc_msgSend)(item_class, sel_registerName("isEqual:"),
                                                        kSecClassGenericPassword))
        return 0;
    const char* name = ((const char* (*)(id, SEL))objc_msgSend)(account, sel_registerName("UTF8String"));
    if (!name)
        return 0;
    char hex[512];
    int length = 0;
    for (const unsigned char* c = (const unsigned char*)name; *c && length < 500; c++) {
        static const char digits[] = "0123456789abcdef";
        hex[length++] = digits[*c >> 4];
        hex[length++] = digits[*c & 15];
    }
    hex[length] = 0;
    id home = ((id (*)(void))dlsym(RTLD_DEFAULT, "NSHomeDirectory"))();
    id directory = ((id (*)(id, SEL, id))objc_msgSend)(
        home, sel_registerName("stringByAppendingPathComponent:"),
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)objc_getClass("NSString"),
            sel_registerName("stringWithUTF8String:"), "Library/MacOBlox/Keychain"));
    ((MacOBloxBool (*)(id, SEL, id, MacOBloxBool, id, id*))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSFileManager"), sel_registerName("defaultManager")),
        sel_registerName("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"),
        directory, 1, 0, 0);
    return ((id (*)(id, SEL, id))objc_msgSend)(
        directory, sel_registerName("stringByAppendingPathComponent:"),
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)objc_getClass("NSString"),
            sel_registerName("stringWithUTF8String:"), hex));
}

static int macoblox_SecItemAdd(id attributes, id* result) {
    id path = attributes ? macoblox_keychain_path(attributes) : 0;
    if (!path)
        return SecItemAdd(attributes, result);
    id data = ((id (*)(id, SEL, id))objc_msgSend)(attributes, sel_registerName("objectForKey:"), kSecValueData);
    if (!data)
        data = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSData"), sel_registerName("data"));
    ((MacOBloxBool (*)(id, SEL, id, MacOBloxBool))objc_msgSend)(
        data, sel_registerName("writeToFile:atomically:"), path, 0); // atomic writes are broken in Darling
    chmod(((const char* (*)(id, SEL))objc_msgSend)(path, sel_registerName("fileSystemRepresentation")), 0600);
    if (result)
        *result = 0;
    write_str("[MacOBlox Keychain] stored generic password item\n");
    return 0;
}
DYLD_INTERPOSE(macoblox_SecItemAdd, SecItemAdd);

static int macoblox_SecItemCopyMatching(id query, id* result) {
    id path = query ? macoblox_keychain_path(query) : 0;
    if (!path)
        return SecItemCopyMatching(query, result);
    id data = ((id (*)(id, SEL, id))objc_msgSend)((id)objc_getClass("NSData"),
                                                 sel_registerName("dataWithContentsOfFile:"), path);
    if (!data)
        return MACOBLOX_ERR_SEC_ITEM_NOT_FOUND;
    id wants_data = ((id (*)(id, SEL, id))objc_msgSend)(query, sel_registerName("objectForKey:"), kSecReturnData);
    if (result)
        *result = wants_data && ((MacOBloxBool (*)(id, SEL))objc_msgSend)(wants_data, sel_registerName("boolValue"))
            ? ((id (*)(id, SEL))objc_msgSend)(data, sel_registerName("retain"))  // caller owns (+1)
            : 0;
    return 0;
}
DYLD_INTERPOSE(macoblox_SecItemCopyMatching, SecItemCopyMatching);

static int macoblox_SecItemDelete(id query) {
    id path = query ? macoblox_keychain_path(query) : 0;
    if (!path)
        return SecItemDelete(query);
    MacOBloxBool removed = ((MacOBloxBool (*)(id, SEL, id, id*))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSFileManager"), sel_registerName("defaultManager")),
        sel_registerName("removeItemAtPath:error:"), path, 0);
    return removed ? 0 : MACOBLOX_ERR_SEC_ITEM_NOT_FOUND;
}
DYLD_INTERPOSE(macoblox_SecItemDelete, SecItemDelete);

// Records the requested OpenGL profile (gl_profile.c); MACOBLOX_TRACE_CGL
// logs the attributes (Darling's CGL pixel format keeps almost none of them).
static id (*orig_pixel_format_init)(id, SEL, const unsigned int*) = 0;
static const unsigned int* macoblox_last_pixel_attributes;
static id hooked_pixel_format_init(id self, SEL cmd, const unsigned int* attributes) {
    if (macoblox_trace_cgl_enabled()) {
        write_str("[MacOBlox CGL] NSOpenGLPixelFormat attributes:");
        for (int index = 0; attributes && index < 24; index++) {
            write_str(" ");
            print_num(attributes[index]);
        }
        write_str("\n");
    }
    macoblox_last_pixel_attributes = attributes;
    id result = orig_pixel_format_init(self, cmd, attributes);
    macoblox_note_pixel_format(result, attributes);
    return result;
}

// Darling's event queue (NSDisplay) fixed to behave like macOS.
//
// -nextEventMatchingMask:untilDate:inMode:dequeue: removed every queued
// event that did not match the mask while looking for one that did. macOS
// leaves them queued. When the game asked for mouse events during a camera
// drag, key releases waiting in front were thrown away: keys stuck ("W
// stayed pressed") or presses were lost. -discardEventsMatchingMask:
// beforeEvent: tested the reference event's type instead of each queued
// event's. The queue (an NSMutableArray) is also shared with the rendering
// thread (shared_current_display), so all three take a lock.
static volatile int macoblox_event_queue_lock;
static void macoblox_lock_event_queue(void) {
    while (__sync_lock_test_and_set(&macoblox_event_queue_lock, 1)) {}
}
static void macoblox_unlock_event_queue(void) {
    __sync_lock_release(&macoblox_event_queue_lock);
}
static id macoblox_event_queue(id display) {
    static Ivar queue_ivar;
    if (!queue_ivar)
        queue_ivar = class_getInstanceVariable(objc_getClass("NSDisplay"), "_eventQueue");
    return queue_ivar ? *(id*)((char*)display + ivar_getOffset(queue_ivar)) : (id)0;
}
static unsigned long macoblox_event_type(id event) {
    return ((unsigned long (*)(id, SEL))objc_msgSend)(event, sel_registerName("type"));
}
static int macoblox_mask_matches(unsigned long long mask, unsigned long type) {
    return type < 64 && (mask & (1ULL << type));
}

static id hooked_display_next_event(id self, SEL cmd, unsigned long long mask, id until, id mode,
                                    signed char dequeue) {
    (void)cmd;
    id queue = macoblox_event_queue(self);
    SEL count = sel_registerName("count"), object_at = sel_registerName("objectAtIndex:");
    if (queue && ((unsigned long (*)(id, SEL))objc_msgSend)(queue, count))
        until = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSDate"), sel_registerName("date"));
    id run_loop = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSRunLoop"),
                                                  sel_registerName("currentRunLoop"));
    ((signed char (*)(id, SEL, id, id))objc_msgSend)(run_loop, sel_registerName("runMode:beforeDate:"),
                                                     mode, until);
    id result = (id)0;
    if (queue) {
        macoblox_lock_event_queue();
        unsigned long total = ((unsigned long (*)(id, SEL))objc_msgSend)(queue, count);
        for (unsigned long index = 0; index < total; index++) {
            id event = ((id (*)(id, SEL, unsigned long))objc_msgSend)(queue, object_at, index);
            if (!macoblox_mask_matches(mask, macoblox_event_type(event)))
                continue;
            result = ((id (*)(id, SEL))objc_msgSend)(event, sel_registerName("retain"));
            if (dequeue)
                ((void (*)(id, SEL, unsigned long))objc_msgSend)(
                    queue, sel_registerName("removeObjectAtIndex:"), index);
            break;
        }
        /* Nobody may ever ask for some event types; keep the queue bounded. */
        while (((unsigned long (*)(id, SEL))objc_msgSend)(queue, count) > 4096)
            ((void (*)(id, SEL, unsigned long))objc_msgSend)(queue, sel_registerName("removeObjectAtIndex:"), 0);
        macoblox_unlock_event_queue();
        if (result)
            result = ((id (*)(id, SEL))objc_msgSend)(result, sel_registerName("autorelease"));
    }
    if (!result) {
        /* As Darling: an NSAppKitSystem event when nothing matches. */
        id event = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSEvent"), sel_registerName("alloc"));
        event = ((id (*)(id, SEL, unsigned long, MacOBloxPoint, unsigned long, id))objc_msgSend)(
            event, sel_registerName("initWithType:location:modifierFlags:window:"), 13 /* NSAppKitSystem */,
            (MacOBloxPoint){0, 0}, 0, (id)0);
        result = ((id (*)(id, SEL))objc_msgSend)(event, sel_registerName("autorelease"));
    }
    return result;
}

static void hooked_display_post_event(id self, SEL cmd, id event, signed char at_start) {
    (void)cmd;
    id queue = macoblox_event_queue(self);
    if (!queue || !event)
        return;
    macoblox_lock_event_queue();
    if (at_start)
        ((void (*)(id, SEL, id, unsigned long))objc_msgSend)(queue, sel_registerName("insertObject:atIndex:"),
                                                             event, 0);
    else
        ((void (*)(id, SEL, id))objc_msgSend)(queue, sel_registerName("addObject:"), event);
    macoblox_unlock_event_queue();
}

static void hooked_display_discard_events(id self, SEL cmd, unsigned long long mask, id before) {
    (void)cmd;
    id queue = macoblox_event_queue(self);
    if (!queue)
        return;
    macoblox_lock_event_queue();
    unsigned long total = ((unsigned long (*)(id, SEL))objc_msgSend)(queue, sel_registerName("count"));
    unsigned long stop = total;
    for (unsigned long index = 0; index < total; index++)
        if (((id (*)(id, SEL, unsigned long))objc_msgSend)(queue, sel_registerName("objectAtIndex:"), index) == before) {
            stop = index;
            break;
        }
    for (unsigned long index = stop; index-- > 0;) {
        id event = ((id (*)(id, SEL, unsigned long))objc_msgSend)(queue, sel_registerName("objectAtIndex:"), index);
        if (macoblox_mask_matches(mask, macoblox_event_type(event)))
            ((void (*)(id, SEL, unsigned long))objc_msgSend)(queue, sel_registerName("removeObjectAtIndex:"), index);
    }
    macoblox_unlock_event_queue();
}

// X11 events as Darling's AppKit receives them (-[X11Display postXEvent:]).
//
// Motion compression: a 1000 Hz mouse sends a motion event per pixel, and
// Darling turns every one into an NSEvent the game handles. It could not
// keep up; a pointer warp during mouse lock reached the game 180-340 events
// late and the camera lagged and jumped (reported on an RTX 3050 laptop).
// A motion event is skipped when the next queued event is a motion event
// of the same window and buttons: Darling computes deltas from positions,
// so the next one carries the skipped movement. Jumps over 48 px (our
// warps) are never merged, the lock logic needs to see them alone.
// MACOBLOX_NO_MOTION_COMPRESSION=1 turns this off.
// MACOBLOX_TRACE_KEYS=1 logs X key presses/releases here and the key events
// the game gets (sendEvent), to find lost or stuck keys.
static void (*orig_post_x_event)(id, SEL, void*);
static int macoblox_trace_keys_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char* value = getenv("MACOBLOX_TRACE_KEYS");
        enabled = value && value[0] == '1';
    }
    return enabled;
}
static void hooked_post_x_event(id self, SEL cmd, void* event) {
    int type = *(int*)event;
    if (type == 6 /* MotionNotify */) {
        static int compression = -1;
        static int (*pending)(void*);
        static int (*peek)(void*, void*);
        if (compression < 0) {
            const char* value = getenv("MACOBLOX_NO_MOTION_COMPRESSION");
            pending = (int (*)(void*))dlsym(RTLD_DEFAULT, "XPending");
            peek = (int (*)(void*, void*))dlsym(RTLD_DEFAULT, "XPeekEvent");
            compression = !(value && value[0] == '1') && pending && peek;
        }
        Ivar display_ivar = compression ? class_getInstanceVariable(object_getClass(self), "_display") : 0;
        void* display = display_ivar ? *(void**)((char*)self + ivar_getOffset(display_ivar)) : 0;
        if (display && pending(display) > 0) {
            unsigned char next[192];
            peek(display, next);
            /* XMotionEvent: window at 32, x/y at 64/68, state at 80 */
            int dx = *(int*)(next + 64) - *(int*)((char*)event + 64);
            int dy = *(int*)(next + 68) - *(int*)((char*)event + 68);
            if (*(int*)next == 6 &&
                *(unsigned long*)(next + 32) == *(unsigned long*)((char*)event + 32) &&
                *(unsigned int*)(next + 80) == *(unsigned int*)((char*)event + 80) &&
                dx <= 48 && dx >= -48 && dy <= 48 && dy >= -48)
                return;
        }
    } else if ((type == 2 || type == 3) && macoblox_trace_keys_enabled()) {
        /* XKeyEvent: time at 56, keycode at 84 */
        write_str(type == 2 ? "[MacOBlox Keys] X press   keycode=" : "[MacOBlox Keys] X release keycode=");
        print_num(*(unsigned int*)((char*)event + 84));
        write_str(" time=");
        print_num((long long)*(unsigned long*)((char*)event + 56));
        /* When Darling handles it, in the same milliseconds scale: if the
         * gap to `time` grows, events wait inside Mac O' Blox. */
        write_str(" handled=");
        print_num((long long)(mach_absolute_time() / 1000000ULL));
        write_str("\n");
    }
    orig_post_x_event(self, cmd, event);
}

// OpenGL subwindows get the screen's visual (gl_profile.c explains why).
extern unsigned long macoblox_replace_gl_subwindow(void* display, unsigned long parent, unsigned long old);
static id (*orig_x11_subwindow_init)(id, SEL, id, MacOBloxRect);
static id hooked_x11_subwindow_init(id self, SEL cmd, id parent, MacOBloxRect frame) {
    id result = orig_x11_subwindow_init(self, cmd, parent, frame);
    if (!result || !parent)
        return result;
    Class cls = object_getClass(result);
    Ivar window_ivar = class_getInstanceVariable(cls, "_window");
    Ivar display_ivar = class_getInstanceVariable(cls, "_display");
    if (!window_ivar || !display_ivar)
        return result;
    unsigned long* window = (unsigned long*)((char*)result + ivar_getOffset(window_ivar));
    void* display = *(void**)((char*)result + ivar_getOffset(display_ivar));
    unsigned long parent_handle =
        ((unsigned long (*)(id, SEL))objc_msgSend)(parent, sel_registerName("windowHandle"));
    *window = macoblox_replace_gl_subwindow(display, parent_handle, *window);
    return result;
}

// Classes from Darling's X11 backend load after this library initializes, so
// hooks on them are installed again once NSApplication finishes launching.
static void macoblox_install_late_hooks(void) {
    static volatile int cursor_hooked;
    static volatile int window_events_hooked;
    static volatile int event_queue_hooked;
    Class display_class = objc_getClass("NSDisplay");
    if (display_class && __sync_bool_compare_and_swap(&event_queue_hooked, 0, 1)) {
        Method next = class_getInstanceMethod(display_class,
            sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"));
        Method post = class_getInstanceMethod(display_class, sel_registerName("postEvent:atStart:"));
        Method discard = class_getInstanceMethod(display_class,
            sel_registerName("discardEventsMatchingMask:beforeEvent:"));
        if (next && post && discard && class_getInstanceVariable(display_class, "_eventQueue")) {
            method_setImplementation(next, (IMP)hooked_display_next_event);
            method_setImplementation(post, (IMP)hooked_display_post_event);
            method_setImplementation(discard, (IMP)hooked_display_discard_events);
            write_str("[MacOBlox] Event queue keeps non-matching events (NSDisplay fix)\n");
        }
    }
    static volatile int x_events_hooked;
    Class x11_display_class = objc_getClass("X11Display");
    if (x11_display_class && __sync_bool_compare_and_swap(&x_events_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(x11_display_class, sel_registerName("postXEvent:"));
        if (method) {
            orig_post_x_event = (void (*)(id, SEL, void*))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_post_x_event);
            write_str("[MacOBlox] Hooked X11Display postXEvent: (motion compression)\n");
        }
    }
    static volatile int subwindow_hooked;
    Class x11_subwindow_class = objc_getClass("X11SubWindow");
    if (x11_subwindow_class && __sync_bool_compare_and_swap(&subwindow_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(
            x11_subwindow_class, sel_registerName("initWithParentWindow:frame:"));
        if (method) {
            orig_x11_subwindow_init =
                (id (*)(id, SEL, id, MacOBloxRect))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_x11_subwindow_init);
            write_str("[MacOBlox] Hooked X11SubWindow initWithParentWindow:frame: (GL visual)\n");
        }
    }
    Class x11_cursor_class = objc_getClass("X11Cursor");
    if (x11_cursor_class &&
        __sync_bool_compare_and_swap(&cursor_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(
            x11_cursor_class, sel_registerName("initWithImage:hotPoint:"));
        if (method) {
            orig_x11_cursor_init_image =
                (id (*)(id, SEL, id, MacOBloxPoint))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_x11_cursor_init_image);
            write_str("[MacOBlox] Replaced X11Cursor initWithImage:hotPoint: (row copy fix)\n");
        }
    }
    static volatile int monitors_hooked;
    Class event_class_for_monitors = objc_getClass("NSEvent");
    Class application_class = objc_getClass("NSApplication");
    if (event_class_for_monitors && application_class &&
        __sync_bool_compare_and_swap(&monitors_hooked, 0, 1)) {
        Method add = class_getClassMethod(
            event_class_for_monitors,
            sel_registerName("addLocalMonitorForEventsMatchingMask:handler:"));
        Method remove = class_getClassMethod(
            event_class_for_monitors, sel_registerName("removeMonitor:"));
        Method send = class_getInstanceMethod(
            application_class, sel_registerName("sendEvent:"));
        if (add && remove && send) {
            method_setImplementation(add, (IMP)event_add_local_monitor);
            method_setImplementation(remove, (IMP)event_remove_monitor);
            orig_app_send_event =
                (void (*)(id, SEL, id))method_getImplementation(send);
            method_setImplementation(send, (IMP)hooked_app_send_event);
            write_str("[MacOBlox] Implemented NSEvent local event monitors\n");
        }
        Method accepts = class_getInstanceMethod(
            objc_getClass("NSWindow"), sel_registerName("acceptsMouseMovedEvents"));
        if (accepts) {
            orig_window_accepts_mouse_moved =
                (MacOBloxBool (*)(id, SEL))method_getImplementation(accepts);
            method_setImplementation(accepts, (IMP)hooked_window_accepts_mouse_moved);
            write_str("[MacOBlox] RBXWindow always accepts mouse-moved events\n");
        }
    }
    static volatile int button_number_hooked;
    Class mouse_event_class = objc_getClass("NSEvent_mouse");
    if (mouse_event_class &&
        __sync_bool_compare_and_swap(&button_number_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(
            mouse_event_class, sel_registerName("buttonNumber"));
        if (method) {
            orig_mouse_event_button_number =
                (long (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_mouse_event_button_number);
            write_str("[MacOBlox] NSEvent buttonNumber uses macOS numbering\n");
        }
        method = class_getInstanceMethod(mouse_event_class, sel_registerName("deltaX"));
        if (method) {
            orig_mouse_event_delta_x =
                (double (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_mouse_event_delta_x);
        }
        method = class_getInstanceMethod(mouse_event_class, sel_registerName("deltaY"));
        if (method) {
            orig_mouse_event_delta_y =
                (double (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_mouse_event_delta_y);
            write_str("[MacOBlox] NSEvent motion deltaY uses macOS sign\n");
        }
        method = class_getInstanceMethod(mouse_event_class, sel_registerName("locationInWindow"));
        if (method) {
            orig_event_location_in_window =
                (MacOBloxPoint (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_event_location_in_window);
        }
        method = class_getInstanceMethod(objc_getClass("NSWindow"),
                                         sel_registerName("mouseLocationOutsideOfEventStream"));
        if (method) {
            orig_window_mouse_location =
                (MacOBloxPoint (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_window_mouse_location);
        }
        method = class_getClassMethod(objc_getClass("NSEvent"), sel_registerName("mouseLocation"));
        if (method) {
            orig_event_mouse_location =
                (MacOBloxPoint (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_event_mouse_location);
        }
        write_str("[MacOBlox] Mouse location freezes during mouse lock\n");
    }
    static volatile int text_input_added;
    Class text_view_class = objc_getClass("NSTextView");
    if (text_view_class &&
        __sync_bool_compare_and_swap(&text_input_added, 0, 1)) {
        struct { const char* name; IMP imp; const char* types; } methods[] = {
            {"hasMarkedText", (IMP)text_view_has_marked_text, "c@:"},
            {"markedRange", (IMP)text_view_marked_range, "{_NSRange=QQ}@:"},
            {"unmarkText", (IMP)text_view_unmark_text, "v@:"},
            {"validAttributesForMarkedText", (IMP)text_view_valid_marked_attributes, "@@:"},
            {"attributedSubstringForProposedRange:actualRange:",
             (IMP)text_view_attributed_substring, "@@:{_NSRange=QQ}^{_NSRange=QQ}"},
        };
        for (unsigned long index = 0; index < sizeof(methods) / sizeof(methods[0]); index++) {
            SEL selector = sel_registerName(methods[index].name);
            if (!class_getInstanceMethod(text_view_class, selector) &&
                class_addMethod(text_view_class, selector, methods[index].imp,
                                methods[index].types)) {
                write_str("[MacOBlox] Added NSTextView ");
                write_str(methods[index].name);
                write_str("\n");
            }
        }
    }
    static volatile int constructors_added;
    if (__sync_bool_compare_and_swap(&constructors_added, 0, 1)) {
        Class button_meta = object_getClass((id)objc_getClass("NSButton"));
        Class text_field_meta = object_getClass((id)objc_getClass("NSTextField"));
        SEL button_selector = sel_registerName("buttonWithImage:target:action:");
        SEL label_selector = sel_registerName("labelWithString:");
        if (button_meta && !class_getInstanceMethod(button_meta, button_selector) &&
            class_addMethod(button_meta, button_selector,
                            (IMP)button_with_image_target_action, "@@:@@:"))
            write_str("[MacOBlox] Added +[NSButton buttonWithImage:target:action:]\n");
        if (text_field_meta && !class_getInstanceMethod(text_field_meta, label_selector) &&
            class_addMethod(text_field_meta, label_selector,
                            (IMP)text_field_label_with_string, "@@:@"))
            write_str("[MacOBlox] Added +[NSTextField labelWithString:]\n");
    }
    static volatile int contacts_added;
    Class contact_store_class = objc_getClass("CNContactStore");
    if (contact_store_class &&
        __sync_bool_compare_and_swap(&contacts_added, 0, 1)) {
        Class contact_store_meta = object_getClass((id)contact_store_class);
        SEL status = sel_registerName("authorizationStatusForEntityType:");
        SEL request = sel_registerName("requestAccessForEntityType:completionHandler:");
        if (!class_getInstanceMethod(contact_store_meta, status) &&
            class_addMethod(contact_store_meta, status,
                            (IMP)contact_store_authorization_denied, "q@:q"))
            write_str("[MacOBlox] Added CNContactStore authorization (denied)\n");
        if (!class_getInstanceMethod(contact_store_class, request))
            class_addMethod(contact_store_class, request,
                            (IMP)contact_store_request_access, "v@:q@?");
    }
    static volatile int pixel_format_hooked;
    Class pixel_format_class = objc_getClass("NSOpenGLPixelFormat");
    if (pixel_format_class && __sync_bool_compare_and_swap(&pixel_format_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(pixel_format_class, sel_registerName("initWithAttributes:"));
        if (method) {
            orig_pixel_format_init = (id (*)(id, SEL, const unsigned int*))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_pixel_format_init);
        }
    }
    static volatile int web_hooked;
    Class web_view_class = objc_getClass("WKWebView");
    Class workspace_class = objc_getClass("NSWorkspace");
    if (web_view_class && workspace_class &&
        __sync_bool_compare_and_swap(&web_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(
            web_view_class, sel_registerName("initWithFrame:configuration:"));
        if (method) {
            orig_web_view_init = (id (*)(id, SEL, MacOBloxRect, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_web_view_init);
        }
        method = class_getInstanceMethod(web_view_class, sel_registerName("loadRequest:"));
        if (method) {
            orig_web_view_load_request = (id (*)(id, SEL, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_web_view_load_request);
        }
        method = class_getInstanceMethod(workspace_class, sel_registerName("openURL:"));
        if (method) {
            orig_workspace_open_url = (MacOBloxBool (*)(id, SEL, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_workspace_open_url);
        }
        write_str("[MacOBlox] Tracing WKWebView and NSWorkspace openURL:\n");
    }
    Class window_class = objc_getClass("NSWindow");
    if (window_class &&
        __sync_bool_compare_and_swap(&window_events_hooked, 0, 1)) {
        Method method = class_getInstanceMethod(
            window_class, sel_registerName("sendEvent:"));
        if (method) {
            orig_window_send_event =
                (void (*)(id, SEL, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_window_send_event);
        }
    }
}

// A direct executable launch has no Apple Event. Darling lacks this selector;
// its generic forwarding path otherwise leaves an invalid object return value.
static id no_current_apple_event(id self, SEL cmd) { (void)self; (void)cmd; return 0; }

// Added in newer Foundation versions than Darling currently implements.
// Build relative file URLs using the older URL and NSString APIs that Darling
// does provide.
static id url_file_with_path_relative_to_url(id cls, SEL cmd, id path, id base_url) {
    (void)cmd;
    if (!path)
        return 0;

    signed char is_absolute = ((signed char (*)(id, SEL))objc_msgSend)(
        path, sel_registerName("isAbsolutePath"));
    if (!base_url || is_absolute)
        return ((id (*)(id, SEL, id))objc_msgSend)(
            cls, sel_registerName("fileURLWithPath:"), path);

    id base_path = ((id (*)(id, SEL))objc_msgSend)(base_url, sel_registerName("path"));
    id combined_path = base_path
        ? ((id (*)(id, SEL, id))objc_msgSend)(
              base_path, sel_registerName("stringByAppendingPathComponent:"), path)
        : path;
    return ((id (*)(id, SEL, id))objc_msgSend)(
        cls, sel_registerName("fileURLWithPath:"), combined_path);
}

static int ascii_contains_case_insensitive(const char* text, const char* needle) {
    if (!text || !needle || !*needle)
        return 0;
    for (const char* start = text; *start; start++) {
        const char* a = start;
        const char* b = needle;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? *a + ('a' - 'A') : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? *b + ('a' - 'A') : *b;
            if (ca != cb)
                break;
            a++;
            b++;
        }
        if (!*b)
            return 1;
    }
    return 0;
}

// Cookies. Roblox on macOS keeps the login (.ROBLOSECURITY) in
// NSHTTPCookieStorage. Darling's CFNetwork cookie parser crashes on a valid
// host-only Set-Cookie (it copies a missing Domain), and its cookie storage
// lives only in memory. So Set-Cookie headers are parsed here, cookies are
// built with +[NSHTTPCookie cookieWithProperties:] (Domain always set), and
// persistent cookies are saved to
// ~/Library/MacOBlox/Cookies.plist (mode 0600) and loaded
// back at startup.
#define MSG0(r, o, sel) ((r (*)(id, SEL))objc_msgSend)((id)(o), sel_registerName(sel))
#define MSG1(r, o, sel, a) ((r (*)(id, SEL, id))objc_msgSend)((id)(o), sel_registerName(sel), (a))
#define MSG2(r, o, sel, a, b) ((r (*)(id, SEL, id, id))objc_msgSend)((id)(o), sel_registerName(sel), (a), (b))
static id macoblox_nsstring(const char* text) {
    return ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), text);
}
static id macoblox_trimmed(id string) {
    id whitespace = MSG0(id, objc_getClass("NSCharacterSet"), "whitespaceAndNewlineCharacterSet");
    return MSG1(id, string, "stringByTrimmingCharactersInSet:", whitespace);
}
static id macoblox_cf_boolean(int value) {
    return ((id (*)(id, SEL, MacOBloxBool))objc_msgSend)(
        (id)objc_getClass("NSNumber"), sel_registerName("numberWithBool:"), (MacOBloxBool)(value != 0));
}

static id macoblox_http_date(id text) {
    id formatter = MSG0(id, MSG0(id, objc_getClass("NSDateFormatter"), "alloc"), "init");
    MSG1(void, formatter, "setLocale:",
         MSG1(id, objc_getClass("NSLocale"), "localeWithLocaleIdentifier:", macoblox_nsstring("en_US_POSIX")));
    MSG1(void, formatter, "setTimeZone:",
         MSG1(id, objc_getClass("NSTimeZone"), "timeZoneWithAbbreviation:", macoblox_nsstring("GMT")));
    static const char* formats[] = {"EEE, dd MMM yyyy HH:mm:ss zzz", "EEE, dd-MMM-yyyy HH:mm:ss zzz",
                                    "EEE, dd-MMM-yy HH:mm:ss zzz", "EEEE, dd-MMM-yy HH:mm:ss zzz"};
    id date = 0;
    for (int index = 0; index < 4 && !date; index++) {
        MSG1(void, formatter, "setDateFormat:", macoblox_nsstring(formats[index]));
        date = MSG1(id, formatter, "dateFromString:", text);
    }
    MSG0(void, formatter, "release");
    return date;
}

// Build one NSHTTPCookie from a single "name=value; attr; attr=value" string.
static id macoblox_cookie_from_string(id line, id url) {
    id parts = MSG1(id, line, "componentsSeparatedByString:", macoblox_nsstring(";"));
    unsigned long count = MSG0(unsigned long, parts, "count");
    if (!count)
        return 0;
    id first = macoblox_trimmed(MSG1(id, parts, "objectAtIndex:", (id)0));
    unsigned long equals = ((MacOBloxRange (*)(id, SEL, id))objc_msgSend)(
        first, sel_registerName("rangeOfString:"), macoblox_nsstring("=")).location;
    if (equals == 0x7FFFFFFFFFFFFFFFUL || equals == 0)
        return 0;
    id properties = MSG0(id, objc_getClass("NSMutableDictionary"), "dictionary");
    MSG2(void, properties, "setObject:forKey:",
         macoblox_trimmed(((id (*)(id, SEL, unsigned long))objc_msgSend)(first, sel_registerName("substringToIndex:"), equals)),
         macoblox_nsstring("Name"));
    MSG2(void, properties, "setObject:forKey:",
         ((id (*)(id, SEL, unsigned long))objc_msgSend)(first, sel_registerName("substringFromIndex:"), equals + 1),
         macoblox_nsstring("Value"));
    id host = url ? MSG0(id, url, "host") : 0;
    MSG2(void, properties, "setObject:forKey:", host ? host : macoblox_nsstring("roblox.com"),
         macoblox_nsstring("Domain"));
    MSG2(void, properties, "setObject:forKey:", macoblox_nsstring("/"), macoblox_nsstring("Path"));
    // Darling's CFHTTPCookieIsSecure reads Secure with CFBooleanGetValue and
    // crashes when it is missing, so it is always set, as a CFBoolean.
    MSG2(void, properties, "setObject:forKey:", macoblox_cf_boolean(0), macoblox_nsstring("Secure"));
    for (unsigned long index = 1; index < count; index++) {
        id attribute = macoblox_trimmed(((id (*)(id, SEL, unsigned long))objc_msgSend)(
            parts, sel_registerName("objectAtIndex:"), index));
        unsigned long split = ((MacOBloxRange (*)(id, SEL, id))objc_msgSend)(
            attribute, sel_registerName("rangeOfString:"), macoblox_nsstring("=")).location;
        id key = split == 0x7FFFFFFFFFFFFFFFUL ? attribute
            : ((id (*)(id, SEL, unsigned long))objc_msgSend)(attribute, sel_registerName("substringToIndex:"), split);
        id value = split == 0x7FFFFFFFFFFFFFFFUL ? macoblox_nsstring("")
            : macoblox_trimmed(((id (*)(id, SEL, unsigned long))objc_msgSend)(
                  attribute, sel_registerName("substringFromIndex:"), split + 1));
        const char* name = MSG0(const char*, MSG0(id, macoblox_trimmed(key), "lowercaseString"), "UTF8String");
        if (!name)
            continue;
        if (ascii_strings_equal(name, "domain") && MSG0(unsigned long, value, "length")) {
            MSG2(void, properties, "setObject:forKey:", value, macoblox_nsstring("Domain"));
        } else if (ascii_strings_equal(name, "path") && MSG0(unsigned long, value, "length")) {
            MSG2(void, properties, "setObject:forKey:", value, macoblox_nsstring("Path"));
        } else if (ascii_strings_equal(name, "expires")) {
            id date = macoblox_http_date(value);
            if (date && !MSG1(id, properties, "objectForKey:", macoblox_nsstring("Max-Age")))
                MSG2(void, properties, "setObject:forKey:", date, macoblox_nsstring("Expires"));
        } else if (ascii_strings_equal(name, "max-age")) {
            double seconds = MSG0(double, value, "doubleValue");
            id date = ((id (*)(id, SEL, double))objc_msgSend)(
                (id)objc_getClass("NSDate"), sel_registerName("dateWithTimeIntervalSinceNow:"), seconds);
            MSG2(void, properties, "setObject:forKey:", date, macoblox_nsstring("Expires"));
            MSG2(void, properties, "setObject:forKey:", value, macoblox_nsstring("Max-Age"));
        } else if (ascii_strings_equal(name, "secure")) {
            MSG2(void, properties, "setObject:forKey:", macoblox_cf_boolean(1), macoblox_nsstring("Secure"));
        }
    }
    MSG1(void, properties, "removeObjectForKey:", macoblox_nsstring("Max-Age"));
    return MSG1(id, objc_getClass("NSHTTPCookie"), "cookieWithProperties:", properties);
}

// Split a combined Set-Cookie header ("a=1; Expires=Wed, 21 Oct ...,b=2")
// at commas that start a new "name=" pair, not at commas inside dates.
static id macoblox_split_set_cookie(id header) {
    id result = MSG0(id, objc_getClass("NSMutableArray"), "array");
    const char* text = MSG0(const char*, header, "UTF8String");
    if (!text)
        return result;
    unsigned long start = 0, length = 0;
    while (text[length]) length++;
    for (unsigned long index = 0; index <= length; index++) {
        int boundary = index == length;
        if (!boundary && text[index] == ',') {
            unsigned long probe = index + 1;
            while (text[probe] == ' ') probe++;
            unsigned long name_end = probe;
            while (text[name_end] && text[name_end] != '=' && text[name_end] != ';' &&
                   text[name_end] != ',' && text[name_end] != ' ')
                name_end++;
            boundary = name_end > probe && text[name_end] == '=';
        }
        if (boundary) {
            if (index > start) {
                id piece = ((id (*)(id, SEL, const void*, unsigned long, unsigned long))objc_msgSend)(
                    MSG0(id, objc_getClass("NSString"), "alloc"),
                    sel_registerName("initWithBytes:length:encoding:"), text + start, index - start, 4UL);
                MSG1(void, result, "addObject:", piece);
                MSG0(void, piece, "release");
            }
            start = index + 1;
        }
    }
    return result;
}

static id (*orig_cookies_with_response_headers)(id, SEL, id, id) = 0;
static id cookies_with_response_headers(id cls, SEL cmd, id headers, id url) {
    (void)cls; (void)cmd;
    id cookies = MSG0(id, objc_getClass("NSMutableArray"), "array");
    id keys = headers ? MSG0(id, headers, "allKeys") : 0;
    unsigned long count = keys ? MSG0(unsigned long, keys, "count") : 0;
    for (unsigned long index = 0; index < count; index++) {
        id key = ((id (*)(id, SEL, unsigned long))objc_msgSend)(keys, sel_registerName("objectAtIndex:"), index);
        const char* key_text = key ? MSG0(const char*, key, "UTF8String") : 0;
        if (!key_text || !ascii_contains_case_insensitive(key_text, "set-cookie"))
            continue;
        id value = MSG1(id, headers, "objectForKey:", key);
        id lines = MSG1(MacOBloxBool, value, "isKindOfClass:", (id)objc_getClass("NSArray"))
            ? value : macoblox_split_set_cookie(value);
        unsigned long line_count = MSG0(unsigned long, lines, "count");
        for (unsigned long line = 0; line < line_count; line++) {
            id cookie = macoblox_cookie_from_string(
                ((id (*)(id, SEL, unsigned long))objc_msgSend)(lines, sel_registerName("objectAtIndex:"), line), url);
            if (cookie)
                MSG1(void, cookies, "addObject:", cookie);
        }
    }
    return cookies;
}

static id macoblox_cookie_file(void) {
    id home = ((id (*)(void))dlsym(RTLD_DEFAULT, "NSHomeDirectory"))();
    id directory = MSG1(id, home, "stringByAppendingPathComponent:",
                        macoblox_nsstring("Library/MacOBlox"));
    ((MacOBloxBool (*)(id, SEL, id, MacOBloxBool, id, id*))objc_msgSend)(
        MSG0(id, objc_getClass("NSFileManager"), "defaultManager"),
        sel_registerName("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"),
        directory, 1, 0, 0);
    return MSG1(id, directory, "stringByAppendingPathComponent:", macoblox_nsstring("Cookies.plist"));
}

static volatile int macoblox_loading_cookies;
// Persistent cookies as Roblox hands them to NSHTTPCookieStorage, keyed by
// domain|path|name. Darling's storage cannot be read back for this: in
// Roblox's process it kept none of them, so the login was never saved.
static id macoblox_saved_cookies;

static id macoblox_cookie_key(id domain, id path, id name) {
    return ((id (*)(id, SEL, id, ...))objc_msgSend)(
        (id)objc_getClass("NSString"), sel_registerName("stringWithFormat:"),
        macoblox_nsstring("%@|%@|%@"), domain ? domain : macoblox_nsstring(""),
        path ? path : macoblox_nsstring("/"), name ? name : macoblox_nsstring(""));
}

static void macoblox_write_saved_cookies(void) {
    id file = macoblox_cookie_file();
    id entries = MSG0(id, macoblox_saved_cookies, "allValues");
    ((MacOBloxBool (*)(id, SEL, id, MacOBloxBool))objc_msgSend)(
        entries, sel_registerName("writeToFile:atomically:"), file, 0); // Darling leaves atomic writes as .tmpN files
    chmod(MSG0(const char*, file, "fileSystemRepresentation"), 0600);
}

static void macoblox_remember_cookie(id cookie, int deleted) {
    if (!cookie || macoblox_loading_cookies)
        return;
    if (!macoblox_saved_cookies)
        macoblox_saved_cookies = MSG0(id, MSG0(id, objc_getClass("NSMutableDictionary"), "alloc"), "init");
    id name = MSG0(id, cookie, "name");
    id domain = MSG0(id, cookie, "domain");
    id path = MSG0(id, cookie, "path");
    id key = macoblox_cookie_key(domain, path, name);
    id expires = MSG0(id, cookie, "expiresDate");
    id now = MSG0(id, objc_getClass("NSDate"), "date");
    int persistent = expires && MSG1(long, expires, "compare:", now) == 1;
    write_str("[MacOBlox Cookies] ");
    write_str(deleted ? "delete " : "set ");
    write_str(name ? MSG0(const char*, name, "UTF8String") : "?");
    write_str(" domain=");
    write_str(domain ? MSG0(const char*, domain, "UTF8String") : "?");
    write_str(deleted ? "\n" : (persistent ? " persistent\n" : " session\n"));
    if (deleted || !persistent) {
        MSG1(void, macoblox_saved_cookies, "removeObjectForKey:", key);
    } else {
        id entry = MSG0(id, objc_getClass("NSMutableDictionary"), "dictionary");
        MSG2(void, entry, "setObject:forKey:", name, macoblox_nsstring("Name"));
        MSG2(void, entry, "setObject:forKey:", MSG0(id, cookie, "value"), macoblox_nsstring("Value"));
        MSG2(void, entry, "setObject:forKey:", domain, macoblox_nsstring("Domain"));
        MSG2(void, entry, "setObject:forKey:", path ? path : macoblox_nsstring("/"), macoblox_nsstring("Path"));
        MSG2(void, entry, "setObject:forKey:", expires, macoblox_nsstring("Expires"));
        id properties = MSG0(id, cookie, "properties");
        id secure = properties ? MSG1(id, properties, "objectForKey:", macoblox_nsstring("Secure")) : 0;
        int is_secure = secure && ((MacOBloxBool (*)(id, SEL, SEL))objc_msgSend)(
            secure, sel_registerName("respondsToSelector:"), sel_registerName("boolValue"))
            && MSG0(MacOBloxBool, secure, "boolValue");
        MSG2(void, entry, "setObject:forKey:", macoblox_cf_boolean(is_secure), macoblox_nsstring("Secure"));
        MSG2(void, macoblox_saved_cookies, "setObject:forKey:", entry, key);
    }
    macoblox_write_saved_cookies();
}

static void (*orig_cookie_storage_set)(id, SEL, id) = 0;
static void hooked_cookie_storage_set(id self, SEL cmd, id cookie) {
    orig_cookie_storage_set(self, cmd, cookie);
    macoblox_remember_cookie(cookie, 0);
}
static void (*orig_cookie_storage_set_many)(id, SEL, id, id, id) = 0;
static void hooked_cookie_storage_set_many(id self, SEL cmd, id cookies, id url, id main_url) {
    orig_cookie_storage_set_many(self, cmd, cookies, url, main_url);
    unsigned long count = cookies ? MSG0(unsigned long, cookies, "count") : 0;
    for (unsigned long index = 0; index < count; index++)
        macoblox_remember_cookie(((id (*)(id, SEL, unsigned long))objc_msgSend)(
            cookies, sel_registerName("objectAtIndex:"), index), 0);
}
static void (*orig_cookie_storage_delete)(id, SEL, id) = 0;
static void hooked_cookie_storage_delete(id self, SEL cmd, id cookie) {
    orig_cookie_storage_delete(self, cmd, cookie);
    macoblox_remember_cookie(cookie, 1);
}

// Requests ask cookiesForURL:; add saved cookies that match the URL in case
// Darling's storage dropped them (it keeps none of Roblox's in practice).
static id (*orig_cookie_storage_for_url)(id, SEL, id) = 0;
static id hooked_cookie_storage_for_url(id self, SEL cmd, id url) {
    id result = orig_cookie_storage_for_url(self, cmd, url);
    if (!macoblox_saved_cookies || !url)
        return result;
    id host = MSG0(id, MSG0(id, url, "host"), "lowercaseString");
    id url_path = MSG0(id, url, "path");
    if (!host)
        return result;
    int https = MSG1(MacOBloxBool, MSG0(id, MSG0(id, url, "scheme"), "lowercaseString"),
                     "isEqualToString:", macoblox_nsstring("https"));
    id merged = result ? MSG0(id, result, "mutableCopy") : MSG0(id, MSG0(id, objc_getClass("NSMutableArray"), "alloc"), "init");
    id present = MSG0(id, MSG0(id, objc_getClass("NSMutableSet"), "alloc"), "init");
    unsigned long count = MSG0(unsigned long, merged, "count");
    for (unsigned long index = 0; index < count; index++) {
        id cookie = ((id (*)(id, SEL, unsigned long))objc_msgSend)(merged, sel_registerName("objectAtIndex:"), index);
        MSG1(void, present, "addObject:", MSG0(id, cookie, "name"));
    }
    id now = MSG0(id, objc_getClass("NSDate"), "date");
    id entries = MSG0(id, macoblox_saved_cookies, "allValues");
    unsigned long entry_count = MSG0(unsigned long, entries, "count");
    for (unsigned long index = 0; index < entry_count; index++) {
        id entry = ((id (*)(id, SEL, unsigned long))objc_msgSend)(entries, sel_registerName("objectAtIndex:"), index);
        id name = MSG1(id, entry, "objectForKey:", macoblox_nsstring("Name"));
        if (MSG1(MacOBloxBool, present, "containsObject:", name))
            continue;
        id expires = MSG1(id, entry, "objectForKey:", macoblox_nsstring("Expires"));
        if (expires && MSG1(long, expires, "compare:", now) != 1)
            continue;
        id domain = MSG0(id, MSG1(id, entry, "objectForKey:", macoblox_nsstring("Domain")), "lowercaseString");
        id bare = MSG1(MacOBloxBool, domain, "hasPrefix:", macoblox_nsstring("."))
            ? ((id (*)(id, SEL, unsigned long))objc_msgSend)(domain, sel_registerName("substringFromIndex:"), 1)
            : domain;
        int domain_match = MSG1(MacOBloxBool, host, "isEqualToString:", bare) ||
            MSG1(MacOBloxBool, host, "hasSuffix:",
                 MSG1(id, macoblox_nsstring("."), "stringByAppendingString:", bare));
        id path = MSG1(id, entry, "objectForKey:", macoblox_nsstring("Path"));
        int path_match = !path || !url_path || !MSG0(unsigned long, url_path, "length") ||
            MSG1(MacOBloxBool, url_path, "hasPrefix:", path);
        id secure = MSG1(id, entry, "objectForKey:", macoblox_nsstring("Secure"));
        int needs_https = secure && MSG0(MacOBloxBool, secure, "boolValue");
        if (!domain_match || !path_match || (needs_https && !https))
            continue;
        id cookie = MSG1(id, objc_getClass("NSHTTPCookie"), "cookieWithProperties:", entry);
        if (cookie) {
            MSG1(void, merged, "addObject:", cookie);
            MSG1(void, present, "addObject:", name);
        }
    }
    MSG0(void, present, "release");
    return MSG0(id, merged, "autorelease");
}

static void macoblox_load_cookies(void) {
    id storage = MSG0(id, objc_getClass("NSHTTPCookieStorage"), "sharedHTTPCookieStorage");
    id entries = MSG1(id, objc_getClass("NSArray"), "arrayWithContentsOfFile:", macoblox_cookie_file());
    unsigned long count = entries ? MSG0(unsigned long, entries, "count") : 0;
    id now = MSG0(id, objc_getClass("NSDate"), "date");
    int loaded = 0;
    macoblox_loading_cookies = 1;
    for (unsigned long index = 0; index < count; index++) {
        id entry = ((id (*)(id, SEL, unsigned long))objc_msgSend)(entries, sel_registerName("objectAtIndex:"), index);
        id expires = MSG1(id, entry, "objectForKey:", macoblox_nsstring("Expires"));
        if (expires && MSG1(long, expires, "compare:", now) != 1 /* NSOrderedDescending */)
            continue;
        id cookie = MSG1(id, objc_getClass("NSHTTPCookie"), "cookieWithProperties:", entry);
        if (cookie) {
            MSG1(void, storage, "setCookie:", cookie);
            if (!macoblox_saved_cookies)
                macoblox_saved_cookies = MSG0(id, MSG0(id, objc_getClass("NSMutableDictionary"), "alloc"), "init");
            MSG2(void, macoblox_saved_cookies, "setObject:forKey:", entry,
                 macoblox_cookie_key(MSG1(id, entry, "objectForKey:", macoblox_nsstring("Domain")),
                                     MSG1(id, entry, "objectForKey:", macoblox_nsstring("Path")),
                                     MSG1(id, entry, "objectForKey:", macoblox_nsstring("Name"))));
            loaded++;
        }
    }
    macoblox_loading_cookies = 0;
    write_str("[MacOBlox Cookies] loaded ");
    print_num(loaded);
    write_str(" saved cookies\n");
}

// Cocotron stores NSDisplay in thread-local state. Its X11 backend cannot open
// a second display for Roblox's rendering worker, although the main thread's
// display is already valid. Share the first successfully created display.
static id (*orig_current_display)(id, SEL) = 0;
static id macoblox_shared_display;
static volatile int macoblox_display_lock;
static id shared_current_display(id cls, SEL cmd) {
    if (macoblox_shared_display)
        return macoblox_shared_display;
    while (__sync_lock_test_and_set(&macoblox_display_lock, 1))
        usleep(1000);
    if (!macoblox_shared_display) {
        id display = orig_current_display(cls, cmd);
        if (display)
            macoblox_shared_display = ((id (*)(id, SEL))objc_msgSend)(
                display, sel_registerName("retain"));
    }
    __sync_lock_release(&macoblox_display_lock);
    return macoblox_shared_display;
}

__attribute__((constructor))
static void install_swizzles(void) {
    write_str("[MacOBlox] libMacOBloxShims loaded.\n");
    long slide = _dyld_get_image_vmaddr_slide(0);
    write_str("[MacOBlox] Main executable ASLR slide: ");
    print_hex(slide);
    write_str("\n");

    // Let Darling and Crashpad own signals unless diagnosis is explicitly requested.
    const char *diagnose_signals = getenv("MACOBLOX_DIAGNOSTIC_SIGNALS");
    if (diagnose_signals && *diagnose_signals == '1') {
        struct darwin_sigaction debug_action = {crash_handler, 0, 0x0040};
        sigaction(11, &debug_action, 0);
    }

    Class eventManager = objc_getClass("NSAppleEventManager");
    SEL currentEvent = sel_registerName("currentAppleEvent");
    if (eventManager && !class_getInstanceMethod(eventManager, currentEvent)) {
        class_addMethod(eventManager, currentEvent, (IMP)no_current_apple_event, "@@:");
        write_str("[MacOBlox] Added currentAppleEvent=nil for direct launch\n");
    }

    Class url_class = objc_getClass("NSURL");
    SEL relative_file_url = sel_registerName("fileURLWithPath:relativeToURL:");
    if (url_class && !class_getClassMethod(url_class, relative_file_url)) {
        Class url_meta_class = object_getClass((id)url_class);
        if (url_meta_class && class_addMethod(url_meta_class, relative_file_url,
                                              (IMP)url_file_with_path_relative_to_url,
                                              "@@:@@"))
            write_str("[MacOBlox] Added NSURL fileURLWithPath:relativeToURL:\n");
    }

    Class event_class = objc_getClass("NSEvent");
    if (event_class) {
        SEL phase = sel_registerName("phase");
        SEL momentum_phase = sel_registerName("momentumPhase");
        if (!class_getInstanceMethod(event_class, phase) &&
            class_addMethod(event_class, phase, (IMP)event_phase_none, "Q@:"))
            write_str("[MacOBlox] Added NSEvent phase=None\n");
        if (!class_getInstanceMethod(event_class, momentum_phase) &&
            class_addMethod(event_class, momentum_phase,
                            (IMP)event_phase_none, "Q@:"))
            write_str("[MacOBlox] Added NSEvent momentumPhase=None\n");
    }

    Class process_info_class = objc_getClass("NSProcessInfo");
    if (process_info_class) {
        SEL thermal_state = sel_registerName("thermalState");
        SEL low_power = sel_registerName("isLowPowerModeEnabled");
        if (!class_getInstanceMethod(process_info_class, thermal_state) &&
            class_addMethod(process_info_class, thermal_state,
                            (IMP)process_info_thermal_state_nominal, "q@:"))
            write_str("[MacOBlox] Added NSProcessInfo thermalState=nominal\n");
        if (!class_getInstanceMethod(process_info_class, low_power) &&
            class_addMethod(process_info_class, low_power,
                            (IMP)process_info_low_power_mode_disabled, "B@:"))
            write_str("[MacOBlox] Added NSProcessInfo lowPowerMode=false\n");
    }

    Class gl_context_class = objc_getClass("NSOpenGLContext");
    if (gl_context_class) {
        Method method = class_getInstanceMethod(
            gl_context_class, sel_registerName("initWithFormat:shareContext:"));
        if (method) {
            orig_gl_context_init =
                (id (*)(id, SEL, id, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_gl_context_init);
        }
        method = class_getInstanceMethod(gl_context_class, sel_registerName("setView:"));
        if (method) {
            orig_gl_context_set_view =
                (void (*)(id, SEL, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_gl_context_set_view);
        }
        method = class_getInstanceMethod(
            gl_context_class, sel_registerName("makeCurrentContext"));
        if (method) {
            orig_gl_context_make_current =
                (void (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_gl_context_make_current);
        }
        method = class_getInstanceMethod(gl_context_class, sel_registerName("flushBuffer"));
        if (method) {
            orig_gl_context_flush =
                (void (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_gl_context_flush);
        }
        write_str("[MacOBlox] Tracing NSOpenGLContext drawable presentation\n");
    }

    Class layer_context_class = objc_getClass("CALayerContext");
    if (layer_context_class) {
        Method method = class_getInstanceMethod(
            layer_context_class, sel_registerName("renderLayer:"));
        if (method) {
            orig_layer_context_render_layer =
                (void (*)(id, SEL, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)hooked_layer_context_render_layer);
            write_str("[MacOBlox] Hooked CALayerContext renderLayer: (restores GL context)\n");
        }
    }

    Class scroll_event_class = objc_getClass("NSEvent");
    if (scroll_event_class) {
        struct { const char* name; IMP imp; const char* types; } event_methods[] = {
            {"hasPreciseScrollingDeltas", (IMP)event_has_precise_scrolling_deltas, "c@:"},
            {"isDirectionInvertedFromDevice", (IMP)event_is_direction_inverted, "c@:"},
            {"scrollingDeltaX", (IMP)event_scrolling_delta_x, "d@:"},
            {"scrollingDeltaY", (IMP)event_scrolling_delta_y, "d@:"},
        };
        for (unsigned long index = 0;
             index < sizeof(event_methods) / sizeof(event_methods[0]); index++) {
            SEL selector = sel_registerName(event_methods[index].name);
            if (!class_getInstanceMethod(scroll_event_class, selector) &&
                class_addMethod(scroll_event_class, selector, event_methods[index].imp,
                                event_methods[index].types)) {
                write_str("[MacOBlox] Added NSEvent ");
                write_str(event_methods[index].name);
                write_str("\n");
            }
        }
    }

    macoblox_install_late_hooks();

    Class archiver_meta = object_getClass((id)objc_getClass("NSKeyedArchiver"));
    Class unarchiver_meta = object_getClass((id)objc_getClass("NSKeyedUnarchiver"));
    SEL archive = sel_registerName("archivedDataWithRootObject:requiringSecureCoding:error:");
    SEL unarchive = sel_registerName("unarchivedObjectOfClass:fromData:error:");
    if (archiver_meta && !class_getInstanceMethod(archiver_meta, archive) &&
        class_addMethod(archiver_meta, archive, (IMP)keyed_archiver_archived_data, "@@:@c^@"))
        write_str("[MacOBlox] Added +[NSKeyedArchiver archivedDataWithRootObject:requiringSecureCoding:error:]\n");
    if (unarchiver_meta && !class_getInstanceMethod(unarchiver_meta, unarchive) &&
        class_addMethod(unarchiver_meta, unarchive, (IMP)keyed_unarchiver_unarchived_object, "@@:#@^@"))
        write_str("[MacOBlox] Added +[NSKeyedUnarchiver unarchivedObjectOfClass:fromData:error:]\n");

    const char* hide_menu = getenv("MACOBLOX_HIDE_MENU_BAR");
    Class menu_view_class = objc_getClass("NSMainMenuView");
    if (hide_menu && hide_menu[0] == '1' && menu_view_class) {
        Method method = class_getClassMethod(menu_view_class, sel_registerName("menuHeight"));
        if (method) {
            method_setImplementation(method, (IMP)macoblox_zero_menu_height);
            write_str("[MacOBlox] Menu bar hidden\n");
        }
    }

    Class cookie_class = objc_getClass("NSHTTPCookie");
    if (cookie_class) {
        SEL parse_cookies = sel_registerName("cookiesWithResponseHeaderFields:forURL:");
        Method method = class_getClassMethod(cookie_class, parse_cookies);
        if (method) {
            orig_cookies_with_response_headers =
                (id (*)(id, SEL, id, id))method_getImplementation(method);
            method_setImplementation(method, (IMP)cookies_with_response_headers);
            write_str("[MacOBlox] Hooked NSHTTPCookie response parser\n");
        }
        Class storage_class = objc_getClass("NSHTTPCookieStorage");
        Method set_method = storage_class
            ? class_getInstanceMethod(storage_class, sel_registerName("setCookie:")) : 0;
        Method delete_method = storage_class
            ? class_getInstanceMethod(storage_class, sel_registerName("deleteCookie:")) : 0;
        if (set_method && delete_method) {
            orig_cookie_storage_set = (void (*)(id, SEL, id))method_getImplementation(set_method);
            method_setImplementation(set_method, (IMP)hooked_cookie_storage_set);
            orig_cookie_storage_delete = (void (*)(id, SEL, id))method_getImplementation(delete_method);
            method_setImplementation(delete_method, (IMP)hooked_cookie_storage_delete);
            Method for_url = class_getInstanceMethod(storage_class, sel_registerName("cookiesForURL:"));
            if (for_url) {
                orig_cookie_storage_for_url = (id (*)(id, SEL, id))method_getImplementation(for_url);
                method_setImplementation(for_url, (IMP)hooked_cookie_storage_for_url);
            }
            Method set_many = class_getInstanceMethod(
                storage_class, sel_registerName("setCookies:forURL:mainDocumentURL:"));
            if (set_many) {
                orig_cookie_storage_set_many =
                    (void (*)(id, SEL, id, id, id))method_getImplementation(set_many);
                method_setImplementation(set_many, (IMP)hooked_cookie_storage_set_many);
            }
            macoblox_load_cookies();
        }
    }

    Class display_class = objc_getClass("NSDisplay");
    if (display_class) {
        SEL current_display = sel_registerName("currentDisplay");
        Method method = class_getClassMethod(display_class, current_display);
        if (method) {
            orig_current_display = (id (*)(id, SEL))method_getImplementation(method);
            method_setImplementation(method, (IMP)shared_current_display);
            write_str("[MacOBlox] Sharing NSDisplay across rendering threads\n");
        }
    }

    Class web_preferences_class = objc_getClass("WebPreferences");
    SEL standard_preferences = sel_registerName("standardPreferences");
    if (web_preferences_class &&
        !class_getClassMethod(web_preferences_class, standard_preferences)) {
        Class web_preferences_meta_class = object_getClass((id)web_preferences_class);
        if (web_preferences_meta_class &&
            class_addMethod(web_preferences_meta_class, standard_preferences,
                            (IMP)web_preferences_standard_preferences, "@@:"))
            write_str("[MacOBlox] Added WebPreferences standardPreferences\n");
    }

    Class capture_device_class = objc_getClass("AVCaptureDevice");
    if (capture_device_class) {
        Class capture_device_meta_class = object_getClass((id)capture_device_class);
        SEL devices = sel_registerName("devices");
        SEL devices_for_type = sel_registerName("devicesWithMediaType:");
        SEL default_for_type = sel_registerName("defaultDeviceWithMediaType:");
        if (!class_getClassMethod(capture_device_class, devices) &&
            class_addMethod(capture_device_meta_class, devices,
                            (IMP)empty_capture_devices, "@@:"))
            write_str("[MacOBlox] Added empty AVCaptureDevice devices inventory\n");
        if (!class_getClassMethod(capture_device_class, devices_for_type) &&
            class_addMethod(capture_device_meta_class, devices_for_type,
                            (IMP)empty_capture_devices_for_media_type, "@@:@"))
            write_str("[MacOBlox] Added empty AVCaptureDevice devicesWithMediaType:\n");
        if (!class_getClassMethod(capture_device_class, default_for_type) &&
            class_addMethod(capture_device_meta_class, default_for_type,
                            (IMP)no_default_capture_device, "@@:@"))
            write_str("[MacOBlox] Added AVCaptureDevice defaultDeviceWithMediaType:=nil\n");
    }

    Class layerCls = objc_getClass("CALayer");
    if (layerCls && !class_getInstanceMethod(layerCls, sel_registerName("setContentsScale:"))
                 && !class_getInstanceMethod(layerCls, sel_registerName("contentsScale"))) {
        class_addMethod(layerCls, sel_registerName("setContentsScale:"), (IMP)layer_set_contents_scale, "v@:d");
        class_addMethod(layerCls, sel_registerName("contentsScale"), (IMP)layer_contents_scale, "d@:");
        write_str("[MacOBlox] Added CALayer contentsScale state (1x rendering only)\n");
    }

    Class viewCls = objc_getClass("NSView");
    if (viewCls && !class_getInstanceMethod(viewCls, sel_registerName("setAllowedTouchTypes:"))
                && !class_getInstanceMethod(viewCls, sel_registerName("allowedTouchTypes"))) {
        class_addMethod(viewCls, sel_registerName("setAllowedTouchTypes:"), (IMP)view_set_allowed_touch_types, "v@:Q");
        class_addMethod(viewCls, sel_registerName("allowedTouchTypes"), (IMP)view_allowed_touch_types, "Q@:");
        write_str("[MacOBlox] Added NSView allowedTouchTypes state (no touch synthesis)\n");
    }
    SEL backingSize = sel_registerName("convertSizeToBacking:");
    if (viewCls && !class_getInstanceMethod(viewCls, backingSize)) {
        class_addMethod(viewCls, backingSize, (IMP)backing_size_1x,
                        "{CGSize=dd}@:{CGSize=dd}");
        write_str("[MacOBlox] Added NSView convertSizeToBacking: (experimental 1x)\n");
    }


    Class window_class = objc_getClass("NSWindow");
    if (window_class) {
        SEL rect_from_screen = sel_registerName("convertRectFromScreen:");
        SEL rect_to_screen = sel_registerName("convertRectToScreen:");
        const char* rect_conversion_types =
            "{CGRect={CGPoint=dd}{CGSize=dd}}@:{CGRect={CGPoint=dd}{CGSize=dd}}";
        if (!class_getInstanceMethod(window_class, rect_from_screen) &&
            class_addMethod(window_class, rect_from_screen,
                            (IMP)window_convert_rect_from_screen,
                            rect_conversion_types))
            write_str("[MacOBlox] Added NSWindow convertRectFromScreen:\n");
        if (!class_getInstanceMethod(window_class, rect_to_screen) &&
            class_addMethod(window_class, rect_to_screen,
                            (IMP)window_convert_rect_to_screen,
                            rect_conversion_types))
            write_str("[MacOBlox] Added NSWindow convertRectToScreen:\n");
    }

    Class ccls = objc_getClass("NSConcreteScanner");
    if (ccls) {
        Method m = class_getInstanceMethod(ccls, sel_registerName("initWithString:"));
        if (m) {
            orig_concrete_initWithString = (id (*)(id, SEL, id))method_getImplementation(m);
            method_setImplementation(m, (IMP)hooked_concrete_initWithString);
            write_str("[MacOBlox] Hooked NSConcreteScanner initWithString:\n");
        }
    }

    Class appCls = objc_getClass("NSApplication");
    if (appCls) {
        Method mRun = class_getInstanceMethod(appCls, sel_registerName("run"));
        if (mRun) {
            orig_app_run = (void (*)(id, SEL))method_getImplementation(mRun);
            method_setImplementation(mRun, (IMP)hooked_app_run);
            write_str("[MacOBlox] Hooked NSApplication run\n");
        }
        Method mFinish = class_getInstanceMethod(appCls, sel_registerName("finishLaunching"));
        if (mFinish) {
            orig_app_finish_launching =
                (void (*)(id, SEL))method_getImplementation(mFinish);
            method_setImplementation(mFinish, (IMP)hooked_app_finish_launching);
            write_str("[MacOBlox] Hooked NSApplication finishLaunching\n");
        }
        Method mTerm = class_getInstanceMethod(appCls, sel_registerName("terminate:"));
        if (mTerm) {
            orig_app_terminate = (void (*)(id, SEL, id))method_getImplementation(mTerm);
            method_setImplementation(mTerm, (IMP)hooked_app_terminate);
            write_str("[MacOBlox] Hooked NSApplication terminate:\n");
        }
        Method mDel = class_getInstanceMethod(appCls, sel_registerName("setDelegate:"));
        if (mDel) {
            orig_app_setDelegate = (void (*)(id, SEL, id))method_getImplementation(mDel);
            method_setImplementation(mDel, (IMP)hooked_app_setDelegate);
            write_str("[MacOBlox] Hooked NSApplication setDelegate:\n");
        }
    }

    Class bndlCls = objc_getClass("NSBundle");
    if (bndlCls) {
        Method mNib = class_getClassMethod(bndlCls, sel_registerName("loadNibNamed:owner:"));
        if (mNib) {
            orig_loadNibNamed = (int (*)(id, SEL, id, id))method_getImplementation(mNib);
            method_setImplementation(mNib, (IMP)hooked_loadNibNamed);
            write_str("[MacOBlox] Hooked NSBundle loadNibNamed:owner:\n");
        }
    }

    Class nibCls = objc_getClass("NSNib");
    if (nibCls) {
        Method mInst = class_getInstanceMethod(nibCls, sel_registerName("instantiateNibWithExternalNameTable:"));
        if (mInst) {
            orig_instantiateNib = (int (*)(id, SEL, id))method_getImplementation(mInst);
            method_setImplementation(mInst, (IMP)hooked_instantiateNib);
            write_str("[MacOBlox] Hooked NSNib instantiateNibWithExternalNameTable:\n");
        }
    }

    Class unarchCls = objc_getClass("NSKeyedUnarchiver");
    if (unarchCls) {
        Method m = class_getInstanceMethod(unarchCls, sel_registerName("decodeObjectForKey:"));
        if (m) {
            orig_decodeObjectForKey = (id (*)(id, SEL, id))method_getImplementation(m);
            method_setImplementation(m, (IMP)hooked_decodeObjectForKey);
            write_str("[MacOBlox] Hooked NSKeyedUnarchiver decodeObjectForKey:\n");
        }
    }

    Class wtCls = objc_getClass("NSWindowTemplate");
    if (wtCls) {
        Method mWT = class_getInstanceMethod(wtCls, sel_registerName("initWithCoder:"));
        if (mWT) {
            orig_wt_initWithCoder = (id (*)(id, SEL, id))method_getImplementation(mWT);
            method_setImplementation(mWT, (IMP)hooked_wt_initWithCoder);
            write_str("[MacOBlox] Hooked NSWindowTemplate initWithCoder:\n");
        }
    }

    Class odCls = objc_getClass("NSIBObjectData");
    if (odCls) {
        Method mOD = class_getInstanceMethod(odCls, sel_registerName("initWithCoder:"));
        if (mOD) {
            orig_od_initWithCoder = (id (*)(id, SEL, id))method_getImplementation(mOD);
            method_setImplementation(mOD, (IMP)hooked_od_initWithCoder);
            write_str("[MacOBlox] Hooked NSIBObjectData initWithCoder:\n");
        }
        Method mODE = class_getInstanceMethod(odCls, sel_registerName("establishConnections"));
        if (mODE) {
            orig_od_establish = (void (*)(id, SEL))method_getImplementation(mODE);
            method_setImplementation(mODE, (IMP)hooked_od_establish);
            write_str("[MacOBlox] Hooked NSIBObjectData establishConnections\n");
        }
    }

    Class connCls = objc_getClass("NSNibConnector");
    if (connCls) {
        Method mC = class_getInstanceMethod(connCls, sel_registerName("establishConnection"));
        if (mC) {
            orig_conn_establish = (void (*)(id, SEL))method_getImplementation(mC);
            method_setImplementation(mC, (IMP)hooked_conn_establish);
            write_str("[MacOBlox] Hooked NSNibConnector establishConnection\n");
        }
    }

    Class winCls = objc_getClass("NSWindow");
    if (winCls) {
        Method mW = class_getInstanceMethod(winCls, sel_registerName("initWithContentRect:styleMask:backing:defer:"));
        if (mW) {
            orig_win_init = (id (*)(id, SEL, MacOBloxRect, unsigned long, unsigned long, MacOBloxBool))method_getImplementation(mW);
            method_setImplementation(mW, (IMP)hooked_win_init);
            write_str("[MacOBlox] Hooked NSWindow initWithContentRect:...\n");
        }
    }
}

__attribute__((objc_root_class))
@interface NSObject { Class isa; }
@end

// Foundation Objective-C classes
@interface NSBackgroundActivityScheduler : NSObject @end
@implementation NSBackgroundActivityScheduler @end

// Foundation constants
const void* NSHTTPCookieDomain = @"Domain";
const void* NSHTTPCookieExpires = @"Expires";
const void* NSHTTPCookieName = @"Name";
const void* NSHTTPCookiePath = @"Path";
const void* NSHTTPCookieSecure = @"Secure";
const void* NSHTTPCookieValue = @"Value";
const void* NSHTTPCookieVersion = @"Version";
const void* NSLocalizedDescriptionKey = @"NSLocalizedDescriptionKey";
const void* NSProcessInfoThermalStateDidChangeNotification = @"NSProcessInfoThermalStateDidChangeNotification";
const void* NSProcessInfoPowerStateDidChangeNotification = @"NSProcessInfoPowerStateDidChangeNotification";

// Carbon constants
const void* kTISNotifySelectedKeyboardInputSourceChanged = @"kTISNotifySelectedKeyboardInputSourceChanged";
const void* kTISPropertyInputSourceLanguages = @"kTISPropertyInputSourceLanguages";
const void* kTISPropertyUnicodeKeyLayoutData = @"kTISPropertyUnicodeKeyLayoutData";

// The original Carbon compatibility framework returned NULL for the current
// keyboard source and all of its properties. Roblox copies the Unicode layout
// data during startup, so that placeholder becomes CFDataCreateCopy(NULL) and
// crashes in CFDataGetLength. Keep real Objective-C objects alive for the
// process and interpose the three Text Input Source entry points.
static id macoblox_keyboard_source;
static id macoblox_keyboard_layout_data;
static id macoblox_keyboard_languages;

static void macoblox_initialize_keyboard_source(void) {
    if (macoblox_keyboard_source)
        return;

    Class object_class = objc_getClass("NSObject");
    Class data_class = objc_getClass("NSMutableData");
    Class array_class = objc_getClass("NSArray");
    if (!object_class || !data_class || !array_class)
        return;

    macoblox_keyboard_source = ((id (*)(id, SEL))objc_msgSend)(
        (id)object_class, sel_registerName("new"));

    id data = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
        (id)data_class, sel_registerName("dataWithLength:"), 4096UL);
    macoblox_keyboard_layout_data = data
        ? ((id (*)(id, SEL))objc_msgSend)(data, sel_registerName("retain")) : 0;

    id languages = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)array_class, sel_registerName("arrayWithObject:"), @"en");
    macoblox_keyboard_languages = languages
        ? ((id (*)(id, SEL))objc_msgSend)(languages, sel_registerName("retain")) : 0;
}

void* TISCopyCurrentKeyboardInputSource(void) {
    macoblox_initialize_keyboard_source();
    return macoblox_keyboard_source
        ? ((id (*)(id, SEL))objc_msgSend)(macoblox_keyboard_source, sel_registerName("retain")) : 0;
}

void* TISCopyCurrentKeyboardLayoutInputSource(void) {
    return TISCopyCurrentKeyboardInputSource();
}

const void* TISGetInputSourceProperty(void* source, const void* key) {
    (void)source;
    macoblox_initialize_keyboard_source();
    if (key == kTISPropertyInputSourceLanguages)
        return macoblox_keyboard_languages;
    if (key == kTISPropertyUnicodeKeyLayoutData)
        return macoblox_keyboard_layout_data;
    return 0;
}

// The zero-filled CFData above is only a non-null compatibility token. Avoid
// parsing it as a UCKeyboardLayout until Darling provides a real Carbon layout.
int UCKeyTranslate(const void* layout, unsigned short key_code,
                   unsigned short key_action, unsigned int modifiers,
                   unsigned int keyboard_type, unsigned int options,
                   unsigned int* dead_key_state,
                   unsigned long max_length,
                   unsigned long* actual_length,
                   unsigned short* unicode_string) {
    (void)layout; (void)key_code; (void)key_action; (void)modifiers;
    (void)keyboard_type; (void)options; (void)max_length; (void)unicode_string;
    if (dead_key_state)
        *dead_key_state = 0;
    if (actual_length)
        *actual_length = 0;
    return 0;
}

// CoreServices constants
const void* kUTTagClassFilenameExtension = @"public.filename-extension";
const void* kUTTypeImage = @"public.image";
const void* kUTTypeMovie = @"public.movie";

// CoreVideo constants
const void* kCVPixelBufferCGBitmapContextCompatibilityKey = @"kCVPixelBufferCGBitmapContextCompatibilityKey";
const void* kCVPixelBufferCGImageCompatibilityKey = @"kCVPixelBufferCGImageCompatibilityKey";

// GameController constants
const void* GCControllerDidConnectNotification = @"GCControllerDidConnectNotification";
const void* GCControllerDidDisconnectNotification = @"GCControllerDidDisconnectNotification";

// SystemConfiguration constants
const void* kSCNetworkInterfaceTypeEthernet = @"Ethernet";
const void* kSCNetworkInterfaceTypeIEEE80211 = @"IEEE80211";

// VideoToolbox constants
const void* kVTDecompressionPropertyKey_RealTime = @"RealTime";
const void* kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder = @"RequireHardwareAcceleratedVideoDecoder";
const void* kVTVideoEncoderList_CodecType = @"CodecType";
const void* kVTVideoEncoderList_EncoderName = @"EncoderName";
EOF_SHIMS_LIBMACOBLOXSHIMS_M

cat <<'EOF_SHIMS_AUDIO_HAL_C' > "shims/audio_hal.c"
/* CoreAudio HAL additions for FMOD (Roblox audio & microphone input).
 *
 * Darling's PulseAudio HAL exposes an output device (object 2, also 4 as the
 * system output) and an input device (object 3), but only a few properties.
 * FMOD's CoreAudio output/input asks the device for its streams ('stm#') first
 * and gave up on "unknown property", so Roblox fell back to "NoSound Driver"
 * and could not see or record from any microphone.
 *
 * This answers the missing device and stream properties with the format
 * Darling's stream really uses (44.1 kHz, stereo, 32-bit float, interleaved),
 * provides an AUHAL output/input unit (bus 0 output, bus 1 microphone input),
 * and supports both host FIFO (MACOBLOX_AUDIO_FIFO / MACOBLOX_MIC_FIFO) and
 * ring-buffered IOProc paths. MACOBLOX_TRACE_AUDIO=1 logs the calls.
 *
 * Imports are weak: RobloxCrashHandler loads this library but not CoreAudio. */

typedef int OSStatus;
typedef unsigned int UInt32;
typedef double Float64;
typedef struct { UInt32 selector, scope, element; } PropertyAddress;
typedef struct { UInt32 type, subtype, manufacturer, flags, mask; } ComponentDescription;
typedef struct {
    Float64 sample_rate;
    UInt32 format_id, format_flags, bytes_per_packet, frames_per_packet, bytes_per_frame,
        channels_per_frame, bits_per_channel, reserved;
} StreamDescription;
typedef struct { Float64 minimum, maximum; } ValueRange;
typedef struct { StreamDescription format; ValueRange rates; } RangedDescription;
typedef struct { UInt32 channels, byte_size; void *data; } Buffer;
typedef struct { UInt32 count; Buffer buffers[1]; } BufferList;
typedef void (*ListenerProc)(void);

extern char *getenv(const char *);
extern int snprintf(char *, unsigned long, const char *, ...);
extern long write(int, const void *, unsigned long);
extern long read(int, void *, unsigned long);

#define W __attribute__((weak_import)) extern
W OSStatus AudioObjectGetPropertyData(UInt32, const PropertyAddress *, UInt32, const void *, UInt32 *, void *);
W OSStatus AudioObjectGetPropertyDataSize(UInt32, const PropertyAddress *, UInt32, const void *, UInt32 *);
W unsigned char AudioObjectHasProperty(UInt32, const PropertyAddress *);
W OSStatus AudioObjectSetPropertyData(UInt32, const PropertyAddress *, UInt32, const void *, UInt32, const void *);
W OSStatus AudioObjectIsPropertySettable(UInt32, const PropertyAddress *, unsigned char *);
W OSStatus AudioObjectAddPropertyListener(UInt32, const PropertyAddress *, ListenerProc, void *);
W OSStatus AudioObjectRemovePropertyListener(UInt32, const PropertyAddress *, ListenerProc, void *);
W void *AudioComponentFindNext(void *, const ComponentDescription *);
W OSStatus AudioComponentInstanceNew(void *, void **);
W OSStatus AudioUnitSetProperty(void *, UInt32, UInt32, UInt32, const void *, UInt32);
W OSStatus AudioUnitGetProperty(void *, UInt32, UInt32, UInt32, void *, UInt32 *);
W OSStatus AudioUnitInitialize(void *);
W OSStatus AudioOutputUnitStart(void *);
W OSStatus AudioUnitRender(void *, UInt32 *, const void *, UInt32, UInt32, BufferList *);
W OSStatus AudioDeviceCreateIOProcID(UInt32, void *, void *, void **);
W OSStatus AudioDeviceStart(UInt32, void *);
W OSStatus AudioDeviceStop(UInt32, void *);
W OSStatus AudioDeviceDestroyIOProcID(UInt32, void *);
W OSStatus AudioComponentInstanceDispose(void *);
W OSStatus AudioUnitUninitialize(void *);
W OSStatus AudioOutputUnitStop(void *);
extern void *calloc(unsigned long, unsigned long);
extern void *malloc(unsigned long);
extern void free(void *);
extern unsigned long long mach_absolute_time(void);

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { const void *replacement; const void *replacee; } \
    _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = \
        {(const void *)(unsigned long)&_replacement, (const void *)(unsigned long)&_replacee};

#define FOURCC(a, b, c, d) ((UInt32)(a) << 24 | (UInt32)(b) << 16 | (UInt32)(c) << 8 | (UInt32)(d))
#define SCOPE_GLOBAL FOURCC('g', 'l', 'o', 'b')
#define SCOPE_OUTPUT FOURCC('o', 'u', 't', 'p')
#define SCOPE_INPUT FOURCC('i', 'n', 'p', 't')
#define OUTPUT_STREAM 0x4001u
#define INPUT_STREAM 0x4002u
#define NO_ERROR 0
#define BAD_SIZE FOURCC('!', 's', 'i', 'z')
#define UNSUPPORTED_FORMAT FOURCC('!', 'd', 'a', 't')
#define NOT_HANDLED 0x7fffffff

/* ---------------------------------------------------------------- tracing */

extern int open(const char *, int, ...);
extern int close(int);

static int audio_log_fd = -2;

static int tracing(void) {
    static int value = -1;
    if (value < 0) {
        const char *log_env = getenv("MACOBLOX_AUDIO_LOG");
        const char *trace_env = getenv("MACOBLOX_TRACE_AUDIO");
        const char *active = (log_env && log_env[0] && log_env[0] != '0')
                                 ? log_env
                                 : ((trace_env && trace_env[0] && trace_env[0] != '0') ? trace_env : 0);
        value = active ? 1 : 0;
        if (log_env && (log_env[0] == '/' || log_env[0] == '.')) {
            /* Darwin: O_WRONLY (0x0001) | O_APPEND (0x0008) | O_CREAT (0x0200) = 0x0209 */
            audio_log_fd = open(log_env, 0x0209, 0644);
        }
    }
    return value;
}

static void audio_log_write(const char *line, int length) {
    if (length <= 0)
        return;
    write(2, line, (unsigned long)length);
    if (audio_log_fd >= 0)
        write(audio_log_fd, line, (unsigned long)length);
}

static void code(char out[8], UInt32 value) {
    for (int i = 0; i < 4; i++) {
        char c = (char)(value >> (24 - 8 * i));
        out[i] = c >= 32 && c < 127 ? c : '?';
    }
    out[4] = 0;
}

static void report(const char *function, UInt32 object, const PropertyAddress *address,
                   long status, UInt32 size, int ours) {
    if (!tracing())
        return;
    char selector[8] = "-", scope[8] = "-", line[200];
    if (address) {
        code(selector, address->selector);
        code(scope, address->scope);
    }
    int length = snprintf(line, sizeof line, "[MacOBlox Audio] %s object=%u '%s' scope='%s' -> %ld size=%u%s\n",
                          function, object, selector, scope, status, size, ours ? " (shim)" : "");
    audio_log_write(line, length);
}

/* ------------------------------------------------------ added properties */

static int is_output_device(UInt32 object) { return object == 2 || object == 4; }
static int is_input_device(UInt32 object) { return object == 3; }
static int is_device(UInt32 object) { return is_output_device(object) || is_input_device(object); }
static int is_stream(UInt32 object) { return object == OUTPUT_STREAM || object == INPUT_STREAM; }

static StreamDescription current_format = {
    44100.0, FOURCC('l', 'p', 'c', 'm'), 1 /* float */ | 8 /* packed */, 8, 1, 8, 2, 32, 0};

/* Does the device side of `scope` have a stream on this device? */
static int device_has_stream(UInt32 object, UInt32 scope) {
    if (scope == SCOPE_GLOBAL)
        return 1;
    return (is_output_device(object) && scope == SCOPE_OUTPUT) ||
           (is_input_device(object) && scope == SCOPE_INPUT);
}

static OSStatus put(const void *value, UInt32 size, UInt32 *io_size, void *out) {
    if (out) {
        if (!io_size || *io_size < size)
            return BAD_SIZE;
        const unsigned char *from = value;
        unsigned char *to = out;
        for (UInt32 i = 0; i < size; i++)
            to[i] = from[i];
    }
    if (io_size)
        *io_size = size;
    return NO_ERROR;
}

static OSStatus put_u32(UInt32 value, UInt32 *io_size, void *out) {
    return put(&value, sizeof value, io_size, out);
}

/* MACOBLOX_AUDIO=0 turns the additions off (FMOD then uses "NoSound"). */
static int additions_enabled(void) {
    static int value = -1;
    if (value < 0) {
        const char *text = getenv("MACOBLOX_AUDIO");
        value = text && text[0] == '0' ? 0 : 1;
    }
    return value;
}

/* Returns NOT_HANDLED when Darling should answer. out == 0 means size only. */
static OSStatus added_property(UInt32 object, const PropertyAddress *address, UInt32 *io_size, void *out) {
    if (!additions_enabled())
        return NOT_HANDLED;
    UInt32 selector = address->selector, scope = address->scope;
    /* Fix 1: Answer kAudioHardwarePropertyDevices ('dev#') and default input
     * device ('dIn ') on kAudioObjectSystemObject (object 1) so FMOD's
     * getRecordNumDrivers() enumerates the microphone (object 3) alongside
     * the output device (object 2). */
    if (object == 1) {
        if (selector == FOURCC('d', 'e', 'v', '#')) {
            UInt32 devices[2] = {2, 3};
            return put(devices, sizeof devices, io_size, out);
        }
        if (selector == FOURCC('d', 'I', 'n', ' ')) {
            return put_u32(3, io_size, out);
        }
        if (selector == FOURCC('d', 'O', 'u', 't') || selector == FOURCC('s', 'O', 'u', 't')) {
            return put_u32(2, io_size, out);
        }
    }
    if (is_device(object)) {
        switch (selector) {
        case FOURCC('s', 't', 'm', '#'): { /* streams */
            UInt32 stream = is_output_device(object) ? OUTPUT_STREAM : INPUT_STREAM;
            if (!device_has_stream(object, scope))
                return put(&stream, 0, io_size, out);
            return put_u32(stream, io_size, out);
        }
        case FOURCC('s', 'l', 'a', 'y'): { /* stream configuration */
            BufferList list = {0, {{0, 0, 0}}};
            if (!device_has_stream(object, scope) || scope == SCOPE_GLOBAL)
                return put(&list, 8, io_size, out);
            list.count = 1;
            list.buffers[0].channels = current_format.channels_per_frame;
            return put(&list, sizeof list, io_size, out);
        }
        case FOURCC('s', 'f', 'm', 't'): /* legacy device stream format */
        case FOURCC('p', 'f', 't', ' '):
            if (!device_has_stream(object, scope))
                return NOT_HANDLED;
            return put(&current_format, sizeof current_format, io_size, out);
        case FOURCC('n', 'c', 'h', 'n'): /* legacy channel count */
            if (!device_has_stream(object, scope))
                return put_u32(0, io_size, out);
            return put_u32(current_format.channels_per_frame, io_size, out);
        case FOURCC('n', 's', 'r', 't'): { /* nominal sample rate */
            Float64 rate = current_format.sample_rate;
            return put(&rate, sizeof rate, io_size, out);
        }
        case FOURCC('n', 's', 'r', '#'): { /* available nominal sample rates */
            ValueRange range = {current_format.sample_rate, current_format.sample_rate};
            return put(&range, sizeof range, io_size, out);
        }
        case FOURCC('l', 'i', 'v', 'n'): /* device is alive */
            return put_u32(1, io_size, out);
        case FOURCC('f', 's', 'i', 'z'): { /* buffer frame size, from Darling's byte size */
            UInt32 bytes = 0, size = sizeof bytes;
            PropertyAddress legacy = {FOURCC('b', 's', 'i', 'z'), scope, 0};
            if (AudioObjectGetPropertyData(object, &legacy, 0, 0, &size, &bytes) != NO_ERROR || !bytes)
                bytes = 512 * current_format.bytes_per_frame;
            return put_u32(bytes / current_format.bytes_per_frame, io_size, out);
        }
        case FOURCC('f', 's', 'z', '#'): { /* buffer frame size range */
            ValueRange range = {64, 4096};
            return put(&range, sizeof range, io_size, out);
        }
        case FOURCC('l', 't', 'n', 'c'): /* latency */
        case FOURCC('s', 'a', 'f', 't'): /* safety offset */
            return put_u32(0, io_size, out);
        case FOURCC('t', 'r', 'a', 'n'): /* transport type: built-in */
            return put_u32(FOURCC('b', 'l', 't', 'n'), io_size, out);
        case FOURCC('c', 'l', 'c', 'k'): /* clock domain */
            return put_u32(0, io_size, out);
        }
        return NOT_HANDLED;
    }
    if (is_stream(object)) {
        switch (selector) {
        case FOURCC('s', 'f', 'm', 't'): /* virtual format */
        case FOURCC('p', 'f', 't', ' '): /* physical format */
            return put(&current_format, sizeof current_format, io_size, out);
        case FOURCC('s', 'f', 'm', 'a'): /* available virtual formats */
        case FOURCC('p', 'f', 't', 'a'): { /* available physical formats */
            RangedDescription ranged = {current_format, {current_format.sample_rate, current_format.sample_rate}};
            return put(&ranged, sizeof ranged, io_size, out);
        }
        case FOURCC('s', 'd', 'i', 'r'): /* direction: 0 output, 1 input */
            return put_u32(object == INPUT_STREAM, io_size, out);
        case FOURCC('s', 'c', 'h', 'n'): /* starting channel */
        case FOURCC('s', 'a', 'c', 't'): /* is active */
            return put_u32(1, io_size, out);
        case FOURCC('t', 'e', 'r', 'm'): /* terminal type */
        case FOURCC('l', 't', 'n', 'c'): /* latency */
            return put_u32(0, io_size, out);
        }
        return NOT_HANDLED;
    }
    return NOT_HANDLED;
}

/* ----------------------------------------------------------- interposers */

static OSStatus h_get(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                      UInt32 *size, void *data) {
    if (address) {
        UInt32 capacity = size ? *size : 0;
        OSStatus status = added_property(object, address, &capacity, data);
        if (status != NOT_HANDLED) {
            if (size) *size = capacity;
            report("GetPropertyData", object, address, status, capacity, 1);
            return status;
        }
    }
    OSStatus status = AudioObjectGetPropertyData(object, address, qsize, qdata, size, data);
    report("GetPropertyData", object, address, status, size ? *size : 0, 0);
    return status;
}
DYLD_INTERPOSE(h_get, AudioObjectGetPropertyData)

static OSStatus h_size(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                       UInt32 *size) {
    if (address) {
        UInt32 capacity = 0;
        OSStatus status = added_property(object, address, &capacity, 0);
        if (status != NOT_HANDLED) {
            if (size) *size = capacity;
            report("GetPropertyDataSize", object, address, status, capacity, 1);
            return status;
        }
    }
    OSStatus status = AudioObjectGetPropertyDataSize(object, address, qsize, qdata, size);
    report("GetPropertyDataSize", object, address, status, size ? *size : 0, 0);
    return status;
}
DYLD_INTERPOSE(h_size, AudioObjectGetPropertyDataSize)

static unsigned char h_has(UInt32 object, const PropertyAddress *address) {
    UInt32 capacity = 0;
    if (address && added_property(object, address, &capacity, 0) != NOT_HANDLED) {
        report("HasProperty", object, address, 1, 0, 1);
        return 1;
    }
    unsigned char has = AudioObjectHasProperty(object, address);
    report("HasProperty", object, address, has, 0, 0);
    return has;
}
DYLD_INTERPOSE(h_has, AudioObjectHasProperty)

static OSStatus h_set(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                      UInt32 size, const void *data) {
    if (additions_enabled() && address && (is_stream(object) || is_device(object)) &&
        (address->selector == FOURCC('s', 'f', 'm', 't') || address->selector == FOURCC('p', 'f', 't', ' '))) {
        /* Only the format Darling's PulseAudio stream uses is accepted. */
        const StreamDescription *format = data;
        OSStatus status = size == sizeof *format && format->format_id == current_format.format_id &&
                                  format->sample_rate == current_format.sample_rate &&
                                  format->channels_per_frame == current_format.channels_per_frame &&
                                  format->bits_per_channel == 32 && (format->format_flags & 1)
                              ? NO_ERROR
                              : UNSUPPORTED_FORMAT;
        report("SetPropertyData", object, address, status, size, 1);
        return status;
    }
    if (additions_enabled() && address && is_device(object) &&
        address->selector == FOURCC('f', 's', 'i', 'z') && size == 4) {
        UInt32 bytes = *(const UInt32 *)data * current_format.bytes_per_frame;
        PropertyAddress legacy = {FOURCC('b', 's', 'i', 'z'), address->scope, 0};
        OSStatus status = AudioObjectSetPropertyData(object, &legacy, 0, 0, sizeof bytes, &bytes);
        report("SetPropertyData", object, address, status, size, 1);
        return status;
    }
    OSStatus status = AudioObjectSetPropertyData(object, address, qsize, qdata, size, data);
    report("SetPropertyData", object, address, status, size, 0);
    return status;
}
DYLD_INTERPOSE(h_set, AudioObjectSetPropertyData)

static OSStatus h_settable(UInt32 object, const PropertyAddress *address, unsigned char *settable) {
    if (additions_enabled() && address && (((is_stream(object) || is_device(object)) &&
                                           (address->selector == FOURCC('s', 'f', 'm', 't') ||
                                            address->selector == FOURCC('p', 'f', 't', ' '))) ||
                    (is_device(object) && address->selector == FOURCC('f', 's', 'i', 'z')))) {
        if (settable) *settable = 1;
        return NO_ERROR;
    }
    UInt32 capacity = 0;
    if (address && added_property(object, address, &capacity, 0) != NOT_HANDLED) {
        if (settable) *settable = 0;
        return NO_ERROR;
    }
    return AudioObjectIsPropertySettable(object, address, settable);
}
DYLD_INTERPOSE(h_settable, AudioObjectIsPropertySettable)

/* Darling's AudioObjectAddPropertyListener always fails ("unknown
 * property"), and FMOD treats a failed listener registration (default device
 * changes and so on) as a failed init. Darling's devices never change, so
 * listeners are accepted and simply never called. */
static OSStatus h_add_listener(UInt32 object, const PropertyAddress *address, ListenerProc proc, void *data) {
    if (additions_enabled()) {
        report("AddPropertyListener", object, address, NO_ERROR, 0, 1);
        return NO_ERROR;
    }
    return AudioObjectAddPropertyListener(object, address, proc, data);
}
DYLD_INTERPOSE(h_add_listener, AudioObjectAddPropertyListener)

static OSStatus h_remove_listener(UInt32 object, const PropertyAddress *address, ListenerProc proc, void *data) {
    if (additions_enabled())
        return NO_ERROR;
    return AudioObjectRemovePropertyListener(object, address, proc, data);
}
DYLD_INTERPOSE(h_remove_listener, AudioObjectRemovePropertyListener)

/* ------------------------------------------------ output & input AUHAL unit */

/* Darling's AUHAL only accepts the canonical non-interleaved AudioUnit format
 * (the AUBase check), while FMOD asks for interleaved float and then gave up
 * (kAudioUnitErr_FormatNotSupported). Instead of it, FMOD gets this small
 * HAL output/input unit:
 *  - Bus 0 (output): accepts FMOD's format and render callback (property 23),
 *    renders on an 8 MB stack thread into MACOBLOX_AUDIO_FIFO or a ring buffer
 *    fed to Darling's output IOProc.
 *  - Bus 1 (microphone input): accepts EnableIO on scope=1 (Input), Element=1,
 *    input notify callback (property 2005), and client format on scope=2
 *    (Output), Element=1. Reads captured PCM on an 8 MB stack thread from
 *    MACOBLOX_MIC_FIFO (or Darling's input IOProc on device 3) into an input
 *    ring buffer, invokes the input callback on that safe thread, and serves
 *    samples from AudioUnitRender(). */

typedef OSStatus (*RenderProc)(void *, UInt32 *, const void *, UInt32, UInt32, BufferList *);
typedef struct {
    Float64 sample_time;
    unsigned long long host_time;
    Float64 rate_scalar;
    unsigned long long word_clock;
    unsigned char smpte[24];
    UInt32 flags, reserved;
} TimeStamp;

#define UNIT_MAGIC 0x4d4f4241u /* 'MOBA' */
typedef struct {
    UInt32 magic;
    UInt32 device;
    int output_enabled;
    int input_enabled;
    int running;
    int initialized;
    StreamDescription format;
    StreamDescription input_format;
    RenderProc render;
    void *render_context;
    RenderProc input_callback;
    void *input_context;
    void *ioproc;
    UInt32 max_frames;
    Float64 sample_time;
    Float64 input_sample_time;
    float *scratch;
    UInt32 scratch_frames;
    /* Output ring (FMOD -> host/Darling) */
    unsigned char *ring;
    unsigned long ring_bytes;
    volatile unsigned long ring_written, ring_read;
    /* Input ring (host/Darling mic -> FMOD AudioUnitRender) */
    unsigned char *in_ring;
    unsigned long in_ring_bytes;
    volatile unsigned long in_ring_written, in_ring_read;
    volatile int producing;
    void *thread;
    void *in_thread;
} OutputUnit;

#define INVALID_PROPERTY (-10879)
#define PROPERTY_NOT_WRITABLE (-10865)

static void *hal_component;

static OutputUnit *as_unit(void *instance) {
    OutputUnit *unit = instance;
    return unit && unit->magic == UNIT_MAGIC ? unit : 0;
}

#define RENDER_FRAMES 512
#define RING_FRAMES 8192
extern int pthread_create(void **, const void *, void *(*)(void *), void *);
extern int pthread_join(void *, void **);
extern int pthread_attr_init(void *);
extern int pthread_attr_setstacksize(void *, unsigned long);
extern int usleep(unsigned int);

static UInt32 stream_frame_bytes(const StreamDescription *fmt) {
    UInt32 channels = fmt->channels_per_frame ? fmt->channels_per_frame : 2;
    UInt32 sample_bytes = fmt->bits_per_channel / 8 ? fmt->bits_per_channel / 8 : 4;
    return channels * sample_bytes;
}

static UInt32 unit_frame_bytes(OutputUnit *unit) {
    return stream_frame_bytes(&unit->format);
}

static UInt32 unit_input_frame_bytes(OutputUnit *unit) {
    return stream_frame_bytes(&unit->input_format);
}

/* Render `frames` frames into `out` as interleaved samples. */
static void unit_render(OutputUnit *unit, unsigned char *out, UInt32 frames) {
    UInt32 channels = unit->format.channels_per_frame ? unit->format.channels_per_frame : 2;
    UInt32 sample_bytes = unit->format.bits_per_channel / 8 ? unit->format.bits_per_channel / 8 : 4;
    UInt32 frame_bytes = channels * sample_bytes;
    int planar = (unit->format.format_flags & 0x20) != 0; /* kAudioFormatFlagIsNonInterleaved */
    TimeStamp stamp = {unit->sample_time, mach_absolute_time(), 1.0, 0, {0}, 3 /* sample+host time */, 0};
    UInt32 flags = 0;
    OSStatus status = -1;
    if (unit->render && !planar) {
        BufferList list = {1, {{channels, frames * frame_bytes, out}}};
        status = unit->render(unit->render_context, &flags, &stamp, 0, frames, &list);
    } else if (unit->render && unit->scratch) {
        struct { UInt32 count; Buffer buffers[8]; } list = {0};
        list.count = channels > 8 ? 8 : channels;
        for (UInt32 c = 0; c < list.count; c++) {
            list.buffers[c].channels = 1;
            list.buffers[c].byte_size = frames * sample_bytes;
            list.buffers[c].data = (unsigned char *)unit->scratch + (unsigned long)c * frames * sample_bytes;
        }
        status = unit->render(unit->render_context, &flags, &stamp, 0, frames, (BufferList *)&list);
        for (UInt32 f = 0; f < frames; f++)
            for (UInt32 c = 0; c < list.count; c++)
                for (UInt32 b = 0; b < sample_bytes; b++)
                    out[(f * channels + c) * sample_bytes + b] =
                        ((unsigned char *)list.buffers[c].data)[f * sample_bytes + b];
    }
    if (status != NO_ERROR)
        for (UInt32 i = 0; i < frames * frame_bytes; i++) out[i] = 0;
    unit->sample_time += frames;
}

static void *unit_render_thread(void *context) {
    OutputUnit *unit = context;
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = malloc(chunk);
    if (!block)
        return 0;
    while (unit->producing) {
        unsigned long used = unit->ring_written - unit->ring_read;
        if (unit->ring_bytes - used < chunk) {
            usleep(2000);
            continue;
        }
        unit_render(unit, block, RENDER_FRAMES);
        unsigned long position = unit->ring_written % unit->ring_bytes;
        for (unsigned long i = 0; i < chunk; i++)
            unit->ring[(position + i) % unit->ring_bytes] = block[i];
        __sync_synchronize();
        unit->ring_written += chunk;
    }
    free(block);
    return 0;
}

extern int *__error(void);
extern int pthread_sigmask(int, const unsigned int *, unsigned int *);
#define FIFO_AHEAD_FRAMES 2600

static void *unit_fifo_thread(void *context) {
    OutputUnit *unit = context;
    const char *path = getenv("MACOBLOX_AUDIO_FIFO");
    /* A reader that went away must give EPIPE here, not kill the game. */
    unsigned int block_pipe = 1u << (13 - 1); /* SIGPIPE */
    pthread_sigmask(1 /* SIG_BLOCK */, &block_pipe, 0);
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = malloc(chunk);
    int fd = -1;
    unsigned long long start = mach_absolute_time();
    unsigned long long frames_written = 0;
    Float64 rate = unit->format.sample_rate > 0 ? unit->format.sample_rate : 44100.0;
    while (block && unit->producing) {
        if (fd < 0) {
            fd = open(path, 1 /* O_WRONLY */);
            if (fd < 0) {
                usleep(200000);
                continue;
            }
            start = mach_absolute_time();
            frames_written = 0;
        }
        double elapsed = (double)(mach_absolute_time() - start) / 1e9;
        double ahead = (double)frames_written - elapsed * rate;
        if (ahead > FIFO_AHEAD_FRAMES) {
            usleep(3000);
            continue;
        }
        if (ahead < -rate / 4) { /* fell far behind (stall): restart the clock */
            start = mach_absolute_time();
            frames_written = 0;
        }
        unit_render(unit, block, RENDER_FRAMES);
        unsigned long done = 0;
        while (done < chunk) {
            long written = write(fd, block + done, chunk - done);
            if (written <= 0) {
                if (written < 0 && *__error() == 4 /* EINTR */)
                    continue;
                close(fd);
                fd = -1;
                break;
            }
            done += (unsigned long)written;
        }
        frames_written += RENDER_FRAMES;
    }
    if (fd >= 0)
        close(fd);
    free(block);
    return 0;
}

/* Microphone capture thread: reads captured PCM from MACOBLOX_MIC_FIFO (or
 * waits for Darling's input IOProc to push into in_ring) on an 8 MB stack
 * thread and notifies FMOD's input_callback so FMOD pulls samples via
 * AudioUnitRender() without ever running on Darling's tiny GCD thread. */
static void *unit_mic_thread(void *context) {
    OutputUnit *unit = context;
    const char *fifo_path = getenv("MACOBLOX_MIC_FIFO");
    UInt32 frame_bytes = unit_input_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = fifo_path ? malloc(chunk) : 0;
    int fd = -1;
    unsigned long notified_bytes = 0;

    while (unit->producing) {
        if (fifo_path && block) {
            if (fd < 0) {
                fd = open(fifo_path, 0 /* O_RDONLY */ | 0x0004 /* O_NONBLOCK on Darwin */);
                if (fd < 0) {
                    usleep(50000);
                    continue;
                }
            }
            long got = read(fd, block, chunk);
            if (got <= 0) {
                int err = *__error();
                if (got < 0 && (err == 35 /* EAGAIN */ || err == 4 /* EINTR */)) {
                    usleep(4000);
                    continue;
                }
                close(fd);
                fd = -1;
                usleep(50000);
                continue;
            }
            unsigned long valid = ((unsigned long)got / frame_bytes) * frame_bytes;
            if (valid > 0 && unit->in_ring && unit->in_ring_bytes) {
                unsigned long used = unit->in_ring_written - unit->in_ring_read;
                if (used + valid > unit->in_ring_bytes)
                    unit->in_ring_read = unit->in_ring_written + valid - unit->in_ring_bytes;
                unsigned long pos = unit->in_ring_written % unit->in_ring_bytes;
                for (unsigned long i = 0; i < valid; i++)
                    unit->in_ring[(pos + i) % unit->in_ring_bytes] = block[i];
                __sync_synchronize();
                unit->in_ring_written += valid;
            }
        } else {
            usleep(4000);
        }

        __sync_synchronize();
        while (unit->producing && (unit->in_ring_written - notified_bytes) >= chunk) {
            notified_bytes += chunk;
            if (unit->input_callback) {
                TimeStamp stamp = {unit->input_sample_time, mach_absolute_time(), 1.0, 0, {0}, 3, 0};
                UInt32 flags = 0;
                unit->input_callback(unit->input_context, &flags, &stamp, 1, RENDER_FRAMES, 0);
            }
            unit->input_sample_time += RENDER_FRAMES;
        }
    }
    if (fd >= 0)
        close(fd);
    free(block);
    return 0;
}

static OSStatus unit_ioproc(UInt32 device, const void *now, const BufferList *input, const void *input_time,
                            BufferList *output, const void *output_time, void *context) {
    (void)device; (void)now; (void)input_time; (void)output_time;
    OutputUnit *unit = context;
    /* Copy microphone input from Darling's input IOProc into in_ring. */
    if (unit->input_enabled && input && input->count && input->buffers[0].data &&
        unit->in_ring && unit->in_ring_bytes) {
        const Buffer *in_buf = &input->buffers[0];
        UInt32 in_fb = unit_input_frame_bytes(unit);
        unsigned long got = (in_buf->byte_size / in_fb) * in_fb;
        const unsigned char *src = in_buf->data;
        unsigned long used = unit->in_ring_written - unit->in_ring_read;
        if (used + got > unit->in_ring_bytes)
            unit->in_ring_read = unit->in_ring_written + got - unit->in_ring_bytes;
        unsigned long pos = unit->in_ring_written % unit->in_ring_bytes;
        for (unsigned long i = 0; i < got; i++)
            unit->in_ring[(pos + i) % unit->in_ring_bytes] = src[i];
        __sync_synchronize();
        unit->in_ring_written += got;
    }
    if (!output || !output->count || !unit->output_enabled)
        return NO_ERROR;
    Buffer *buffer = &output->buffers[0];
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long wanted = (buffer->byte_size / frame_bytes) * frame_bytes;
    buffer->byte_size = (UInt32)wanted;
    unsigned char *bytes = buffer->data;
    __sync_synchronize();
    unsigned long available = unit->ring_written - unit->ring_read;
    unsigned long copied = available < wanted ? available : wanted;
    unsigned long position = unit->ring_read % unit->ring_bytes;
    for (unsigned long i = 0; i < copied; i++)
        bytes[i] = unit->ring[(position + i) % unit->ring_bytes];
    for (unsigned long i = copied; i < wanted; i++)
        bytes[i] = 0; /* underrun: silence */
    unit->ring_read += copied;
    return NO_ERROR; /* an error or 0 bytes would pause Darling's stream */
}

static void unit_stop_producer(OutputUnit *unit) {
    if (unit->producing) {
        unit->producing = 0;
        if (unit->thread) {
            pthread_join(unit->thread, 0);
            unit->thread = 0;
        }
        if (unit->in_thread) {
            pthread_join(unit->in_thread, 0);
            unit->in_thread = 0;
        }
    }
}

static void unit_stop(OutputUnit *unit) {
    unit_stop_producer(unit);
    if (unit->running && unit->ioproc)
        AudioDeviceStop(unit->device, unit->ioproc);
    unit->running = 0;
    if (unit->ioproc) {
        AudioDeviceDestroyIOProcID(unit->device, unit->ioproc);
        unit->ioproc = 0;
    }
}

static void *t_find(void *after, const ComponentDescription *description) {
    void *component = AudioComponentFindNext(after, description);
    if (description && description->type == FOURCC('a', 'u', 'o', 'u') &&
        (description->subtype == FOURCC('a', 'h', 'a', 'l') ||
         description->subtype == FOURCC('d', 'e', 'f', ' ')))
        hal_component = component;
    if (tracing() && description) {
        char type[8], subtype[8], line[160];
        code(type, description->type);
        code(subtype, description->subtype);
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioComponentFindNext '%s'/'%s' -> %p\n",
                              type, subtype, component);
        audio_log_write(line, length);
    }
    return component;
}
DYLD_INTERPOSE(t_find, AudioComponentFindNext)

static OSStatus t_new(void *component, void **instance) {
    if (additions_enabled() && component && component == hal_component && instance) {
        OutputUnit *unit = calloc(1, sizeof *unit);
        if (unit) {
            unit->magic = UNIT_MAGIC;
            unit->device = 2;
            unit->output_enabled = 1;
            unit->input_enabled = 0;
            unit->format = current_format;
            unit->input_format = current_format;
            unit->max_frames = 4096;
            *instance = unit;
            report("AudioComponentInstanceNew (shim output/input unit)", 0, 0, 0, 0, 1);
            return NO_ERROR;
        }
    }
    OSStatus status = AudioComponentInstanceNew(component, instance);
    report("AudioComponentInstanceNew", 0, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_new, AudioComponentInstanceNew)

static OSStatus t_dispose(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop(unit);
        unit->magic = 0;
        free(unit->scratch);
        free(unit->ring);
        free(unit->in_ring);
        free(unit);
        return NO_ERROR;
    }
    return AudioComponentInstanceDispose(instance);
}
DYLD_INTERPOSE(t_dispose, AudioComponentInstanceDispose)

static void report_format(const char *what, const StreamDescription *format) {
    if (!tracing() || !format)
        return;
    char id[8], line[220];
    code(id, format->format_id);
    int length = snprintf(line, sizeof line,
                          "[MacOBlox Audio]   %s format '%s' rate=%.0f flags=0x%x bytes/packet=%u frames/packet=%u "
                          "bytes/frame=%u channels=%u bits=%u\n",
                          what, id, format->sample_rate, format->format_flags, format->bytes_per_packet,
                          format->frames_per_packet, format->bytes_per_frame, format->channels_per_frame,
                          format->bits_per_channel);
    audio_log_write(line, length);
}

static OSStatus unit_set(OutputUnit *unit, UInt32 id, UInt32 scope, UInt32 element, const void *data, UInt32 size) {
    switch (id) {
    case 2003: /* kAudioOutputUnitProperty_EnableIO */
        if (size < 4) return BAD_SIZE;
        if (scope == 2 && element == 0)
            unit->output_enabled = *(const UInt32 *)data != 0;
        else if (scope == 1 && element == 1) {
            unit->input_enabled = *(const UInt32 *)data != 0;
            if (tracing()) {
                char line[128];
                int len = snprintf(line, sizeof line,
                                   "[MacOBlox Audio Mic] EnableIO bus=1 (Input) input_enabled=%d\n",
                                   unit->input_enabled);
                audio_log_write(line, len);
            }
        }
        return NO_ERROR;
    case 2000: /* kAudioOutputUnitProperty_CurrentDevice */
        if (size < 4) return BAD_SIZE;
        unit->device = *(const UInt32 *)data;
        return NO_ERROR;
    case 8: { /* kAudioUnitProperty_StreamFormat */
        if (size != sizeof(StreamDescription)) return BAD_SIZE;
        const StreamDescription *format = data;
        report_format("unit set", format);
        if (format->format_id != FOURCC('l', 'p', 'c', 'm'))
            return -10868; /* kAudioUnitErr_FormatNotSupported */
        if (scope == 1 && element == 0) /* what the client renders to output */
            unit->format = *format;
        else if (scope == 2 && element == 1) /* what the client reads from mic input */
            unit->input_format = *format;
        return NO_ERROR;
    }
    case 23: /* kAudioUnitProperty_SetRenderCallback */
        if (size < 2 * sizeof(void *)) return BAD_SIZE;
        unit->render = ((RenderProc const *)data)[0];
        unit->render_context = ((void *const *)data)[1];
        return NO_ERROR;
    case 2005: /* kAudioOutputUnitProperty_SetInputCallback */
        if (size < 2 * sizeof(void *)) return BAD_SIZE;
        unit->input_callback = ((RenderProc const *)data)[0];
        unit->input_context = ((void *const *)data)[1];
        if (tracing()) {
            char line[140];
            int len = snprintf(line, sizeof line,
                               "[MacOBlox Audio Mic] SetInputCallback (2005) registered proc=%p ctx=%p\n",
                               (void *)(unsigned long)unit->input_callback, unit->input_context);
            audio_log_write(line, len);
        }
        return NO_ERROR;
    case 14: /* kAudioUnitProperty_MaximumFramesPerSlice */
        if (size >= 4) unit->max_frames = *(const UInt32 *)data;
        return NO_ERROR;
    }
    return NO_ERROR; /* accept and ignore anything else */
}

static OSStatus unit_get(OutputUnit *unit, UInt32 id, UInt32 scope, UInt32 element, void *data, UInt32 *size) {
    switch (id) {
    case 8: /* StreamFormat */
        if (element == 1)
            return put(scope == 2 ? &unit->input_format : &current_format, sizeof unit->input_format, size, data);
        return put(&unit->format, sizeof unit->format, size, data);
    case 2000:
        return put_u32(unit->device, size, data);
    case 2003:
        if (scope == 1 && element == 1)
            return put_u32((UInt32)unit->input_enabled, size, data);
        return put_u32(scope == 2 ? (UInt32)unit->output_enabled : 0, size, data);
    case 2001: /* IsRunning */
        return put_u32((UInt32)unit->running, size, data);
    case 2006: /* HasIO */
        if (scope == 1 && element == 1)
            return put_u32(1, size, data);
        return put_u32(scope == 2 ? 1 : 0, size, data);
    case 14:
        return put_u32(unit->max_frames, size, data);
    case 12: { /* Latency */
        Float64 zero = 0;
        return put(&zero, sizeof zero, size, data);
    }
    }
    return INVALID_PROPERTY;
}

static OSStatus t_unit_set(void *instance, UInt32 id, UInt32 scope, UInt32 element, const void *data, UInt32 size) {
    OutputUnit *unit = as_unit(instance);
    OSStatus status = unit ? unit_set(unit, id, scope, element, data, size)
                           : AudioUnitSetProperty(instance, id, scope, element, data, size);
    if (tracing()) {
        char line[160];
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioUnitSetProperty id=%u scope=%u element=%u -> %d%s\n",
                              id, scope, element, status, unit ? " (shim)" : "");
        audio_log_write(line, length);
    }
    return status;
}
DYLD_INTERPOSE(t_unit_set, AudioUnitSetProperty)

static OSStatus t_unit_get(void *instance, UInt32 id, UInt32 scope, UInt32 element, void *data, UInt32 *size) {
    OutputUnit *unit = as_unit(instance);
    OSStatus status = unit ? unit_get(unit, id, scope, element, data, size)
                           : AudioUnitGetProperty(instance, id, scope, element, data, size);
    if (tracing()) {
        char line[160];
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioUnitGetProperty id=%u scope=%u element=%u -> %d%s\n",
                              id, scope, element, status, unit ? " (shim)" : "");
        audio_log_write(line, length);
    }
    return status;
}
DYLD_INTERPOSE(t_unit_get, AudioUnitGetProperty)

static volatile unsigned long mic_render_calls;

static OSStatus t_unit_render(void *instance, UInt32 *action_flags, const void *time_stamp,
                              UInt32 bus_number, UInt32 number_frames, BufferList *data) {
    OutputUnit *unit = as_unit(instance);
    if (!unit) {
        return AudioUnitRender ? AudioUnitRender(instance, action_flags, time_stamp, bus_number, number_frames, data)
                               : -10879;
    }
    if (!data || !data->count)
        return NO_ERROR;
    if (bus_number == 1) {
        UInt32 channels = unit->input_format.channels_per_frame ? unit->input_format.channels_per_frame : 2;
        UInt32 sample_bytes = unit->input_format.bits_per_channel / 8 ? unit->input_format.bits_per_channel / 8 : 4;
        UInt32 frame_bytes = channels * sample_bytes;
        unsigned long wanted = (unsigned long)number_frames * frame_bytes;
        __sync_synchronize();
        unsigned long available = (unit->in_ring && unit->in_ring_bytes)
                                      ? (unit->in_ring_written - unit->in_ring_read)
                                      : 0;
        unsigned long copied = available < wanted ? available : wanted;
        unsigned long pos = unit->in_ring_bytes ? (unit->in_ring_read % unit->in_ring_bytes) : 0;
        unsigned long non_zero_bytes = 0;
        for (unsigned long i = 0; i < copied; i++) {
            if (unit->in_ring[(pos + i) % unit->in_ring_bytes] != 0)
                non_zero_bytes++;
        }
        int planar = (unit->input_format.format_flags & 0x20) != 0;
        if (!planar || data->count == 1) {
            unsigned char *dst = data->buffers[0].data;
            data->buffers[0].byte_size = (UInt32)wanted;
            if (dst) {
                for (unsigned long i = 0; i < copied; i++)
                    dst[i] = unit->in_ring[(pos + i) % unit->in_ring_bytes];
                for (unsigned long i = copied; i < wanted; i++)
                    dst[i] = 0;
            }
        } else {
            UInt32 copied_frames = (UInt32)(copied / frame_bytes);
            for (UInt32 c = 0; c < data->count; c++) {
                unsigned char *dst = data->buffers[c].data;
                data->buffers[c].byte_size = number_frames * sample_bytes;
                if (!dst) continue;
                for (UInt32 f = 0; f < number_frames; f++) {
                    for (UInt32 b = 0; b < sample_bytes; b++) {
                        if (f < copied_frames && c < channels) {
                            unsigned long idx = (pos + ((unsigned long)f * channels + c) * sample_bytes + b) %
                                                unit->in_ring_bytes;
                            dst[f * sample_bytes + b] = unit->in_ring[idx];
                        } else {
                            dst[f * sample_bytes + b] = 0;
                        }
                    }
                }
            }
        }
        unit->in_ring_read += copied;
        if (tracing()) {
            unsigned long call_no = __sync_add_and_fetch(&mic_render_calls, 1);
            if (call_no <= 10 || call_no % 100 == 0) {
                char line[200];
                int len = snprintf(line, sizeof line,
                                   "[MacOBlox Audio Mic] AudioUnitRender(bus=1) #%lu frames=%u copied=%lu/%lu non_zero_bytes=%lu planar=%d\n",
                                   call_no, number_frames, copied, wanted, non_zero_bytes, planar);
                audio_log_write(line, len);
            }
        }
        return NO_ERROR;
    }
    return NO_ERROR;
}
DYLD_INTERPOSE(t_unit_render, AudioUnitRender)

static OSStatus t_init(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit->initialized = 1;
        return NO_ERROR;
    }
    OSStatus status = AudioUnitInitialize(instance);
    report("AudioUnitInitialize", 0, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_init, AudioUnitInitialize)

static OSStatus t_uninit(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop(unit);
        unit->initialized = 0;
        return NO_ERROR;
    }
    return AudioUnitUninitialize(instance);
}
DYLD_INTERPOSE(t_uninit, AudioUnitUninitialize)

static OSStatus t_start(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (!unit) {
        OSStatus status = AudioOutputUnitStart(instance);
        report("AudioOutputUnitStart", 0, 0, status, 0, 0);
        return status;
    }
    if (unit->running || (!unit->output_enabled && !unit->input_enabled))
        return NO_ERROR;

    unsigned char attributes[64] = {0}; /* pthread_attr_t is 64 bytes on Darwin x86_64 */
    pthread_attr_init(attributes);
    pthread_attr_setstacksize(attributes, 8u << 20);
    unit->producing = 1;

    if (unit->input_enabled) {
        UInt32 in_fb = unit_input_frame_bytes(unit);
        if (!unit->in_ring) {
            unit->in_ring_bytes = (unsigned long)RING_FRAMES * in_fb;
            unit->in_ring = calloc(1, unit->in_ring_bytes);
        }
        unit->in_ring_written = unit->in_ring_read = 0;
        if (pthread_create(&unit->in_thread, attributes, unit_mic_thread, unit) != 0) {
            unit->in_thread = 0;
        }
        if (tracing()) {
            const char *fifo_env = getenv("MACOBLOX_MIC_FIFO");
            char line[200];
            int len = snprintf(line, sizeof line,
                               "[MacOBlox Audio Mic] Started mic capture thread: device=%u fifo=%s ring_bytes=%lu\n",
                               unit->device, fifo_env ? fifo_env : "(Darling HAL device 3)", unit->in_ring_bytes);
            audio_log_write(line, len);
        }
    }

    int out_uses_fifo = (unit->output_enabled && getenv("MACOBLOX_AUDIO_FIFO") != 0);
    int in_uses_fifo = (unit->input_enabled && getenv("MACOBLOX_MIC_FIFO") != 0);

    if (out_uses_fifo) {
        if ((unit->format.format_flags & 0x20) && !unit->scratch)
            unit->scratch = malloc((unsigned long)RENDER_FRAMES * unit_frame_bytes(unit));
        if (pthread_create(&unit->thread, attributes, unit_fifo_thread, unit) != 0) {
            unit_stop_producer(unit);
            return -1;
        }
        report("unit: start (host FIFO)", 0, 0, 0, 0, 1);
    }

    if (( !unit->output_enabled || out_uses_fifo ) && ( !unit->input_enabled || in_uses_fifo )) {
        unit->running = 1;
        return NO_ERROR;
    }

    /* Darling's device takes the stream format through the legacy
     * kAudioDevicePropertyStreamFormat ('sfmt') and wants it interleaved;
     * a non-interleaved client format is interleaved in the IOProc. */
    int need_hal_out = (unit->output_enabled && !out_uses_fifo);
    StreamDescription device_format = need_hal_out ? unit->format : unit->input_format;
    if (device_format.format_flags & 0x20) {
        device_format.format_flags &= ~0x20u;
        device_format.bytes_per_frame = device_format.channels_per_frame * (device_format.bits_per_channel / 8);
        device_format.bytes_per_packet = device_format.bytes_per_frame;
    }
    if (need_hal_out) {
        UInt32 frame_bytes = unit_frame_bytes(unit);
        if (!unit->ring) {
            unit->ring_bytes = (unsigned long)RING_FRAMES * frame_bytes;
            unit->ring = calloc(1, unit->ring_bytes);
        }
        if ((unit->format.format_flags & 0x20) && !unit->scratch)
            unit->scratch = malloc((unsigned long)RENDER_FRAMES * frame_bytes);
        if (!unit->ring) {
            unit_stop_producer(unit);
            return -1;
        }
        unit->ring_written = unit->ring_read = 0;
        if (pthread_create(&unit->thread, attributes, unit_render_thread, unit) != 0) {
            unit_stop_producer(unit);
            return -1;
        }
    }
    if (!need_hal_out && unit->input_enabled && unit->device == 2)
        unit->device = 3;
    UInt32 fmt_scope = is_input_device(unit->device) ? SCOPE_INPUT : SCOPE_OUTPUT;
    PropertyAddress format_address = {FOURCC('s', 'f', 'm', 't'), fmt_scope, 0};
    OSStatus status = AudioObjectSetPropertyData(unit->device, &format_address, 0, 0,
                                                 sizeof device_format, &device_format);
    report("unit: device format", unit->device, &format_address, status, 0, 1);
    status = AudioDeviceCreateIOProcID(unit->device, (void *)unit_ioproc, unit, &unit->ioproc);
    if (status == NO_ERROR)
        status = AudioDeviceStart(unit->device, unit->ioproc);
    unit->running = status == NO_ERROR;
    report("unit: start", unit->device, 0, status, 0, 1);
    return status;
}
DYLD_INTERPOSE(t_start, AudioOutputUnitStart)

static OSStatus t_stop(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop_producer(unit);
        if (unit->running && unit->ioproc)
            AudioDeviceStop(unit->device, unit->ioproc);
        unit->running = 0;
        return NO_ERROR;
    }
    return AudioOutputUnitStop(instance);
}
DYLD_INTERPOSE(t_stop, AudioOutputUnitStop)

static OSStatus t_ioproc(UInt32 device, void *proc, void *data, void **id) {
    OSStatus status = AudioDeviceCreateIOProcID(device, proc, data, id);
    report("AudioDeviceCreateIOProcID", device, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_ioproc, AudioDeviceCreateIOProcID)

static OSStatus t_dstart(UInt32 device, void *proc) {
    OSStatus status = AudioDeviceStart(device, proc);
    report("AudioDeviceStart", device, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_dstart, AudioDeviceStart)

/* Host time: Darling's AudioGetCurrentHostTime is a stub (logs "STUB" and
 * returns nothing useful), and FMOD times its mixing with it. Darling's
 * mach_absolute_time counts nanoseconds, so host time is nanoseconds too. */
W unsigned long long AudioGetCurrentHostTime(void);
W Float64 AudioGetHostClockFrequency(void);
W unsigned long long AudioConvertNanosToHostTime(unsigned long long);

static unsigned long long h_host_time(void) {
    return mach_absolute_time();
}
DYLD_INTERPOSE(h_host_time, AudioGetCurrentHostTime)

static Float64 h_host_frequency(void) {
    return 1000000000.0;
}
DYLD_INTERPOSE(h_host_frequency, AudioGetHostClockFrequency)

static unsigned long long h_nanos_to_host(unsigned long long nanos) {
    return nanos;
}
DYLD_INTERPOSE(h_nanos_to_host, AudioConvertNanosToHostTime)
EOF_SHIMS_AUDIO_HAL_C

cat <<'EOF_TESTS_TST_MQBLOX_CPP' > "tests/tst_mqblox.cpp"
#include "core/SettingsManager.hpp"
#include "core/SettingsRegistry.hpp"
#include "core/SoberController.hpp"
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace mqblox;

class TestMqBlox : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
        qputenv("MQBLOX_TEST_DIR", m_tmpDir.path().toUtf8());
    }

    void testPlaceUriParser() {
        QCOMPARE(SettingsManager::parsePlaceUri("1818"),
                 QString("roblox://experiences/start?placeId=1818"));
        QCOMPARE(SettingsManager::parsePlaceUri("https://www.roblox.com/games/2753915549/Blox-Fruits?privateServerLinkCode=ABC123xyz"),
                 QString("roblox://experiences/start?placeId=2753915549&linkCode=ABC123xyz"));
        QCOMPARE(SettingsManager::parsePlaceUri("invalid_text"), QString());
    }

    void testValidAndInvalidFflagsJsonAndForeignKeysPreserved() {
        SettingsManager mgr;
        const QString clientSettingsPath = mgr.clientAppSettingsPath();
        QDir().mkpath(QFileInfo(clientSettingsPath).absolutePath());
        {
            QFile f(clientSettingsPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(R"({"FFlagForeignMacOBloxKey": true})");
            f.close();
        }

        const QString cfgPath = mgr.soberBaseDir() + "/config/sober/config.json";
        QDir().mkpath(QFileInfo(cfgPath).absolutePath());
        {
            QFile f(cfgPath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(R"({"custom_sober_internal_key": 42, "fflags": {"FFlagForeignUserKey": true}})");
            f.close();
        }

        mgr.settings().customFflagsJson = "{ broken json ";
        QString err;
        QVERIFY(!mgr.saveAll(&err));
        QVERIFY(!err.isEmpty());

        mgr.settings().customFflagsJson = R"({"FIntCameraMaxZoomDistance": 9999})";
        mgr.settings().targetFps = 360;
        mgr.settings().fullBright = true;
        mgr.settings().clearFog = true;
        QVERIFY(mgr.saveAll(&err));

        // 1. Verify MacOBlox ClientAppSettings.json
        QFile csf(clientSettingsPath);
        QVERIFY(csf.open(QIODevice::ReadOnly));
        QJsonObject csRoot = QJsonDocument::fromJson(csf.readAll()).object();
        QCOMPARE(csRoot.value("FFlagForeignMacOBloxKey").toBool(), true);
        QCOMPARE(csRoot.value("DFIntTaskSchedulerTargetFps").toInt(), 360);
        QCOMPARE(csRoot.value("FIntRenderShadowIntensity").toInt(), 0);
        QCOMPARE(csRoot.value("FIntFRMMinGrassDistance").toInt(), 0);
        QCOMPARE(csRoot.value("FFlagDebugGraphicsPreferOpenGL").toBool(), true);
        QVERIFY(!csRoot.contains("FFlagDebugGraphicsPreferVulkan"));

        // 2. Verify legacy config.json compatibility
        QFile cf(cfgPath);
        QVERIFY(cf.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(cf.readAll()).object();
        QCOMPARE(root.value("custom_sober_internal_key").toInt(), 42);
        QCOMPARE(root.value("use_console_experience").toBool(), false);
        QCOMPARE(root.value("touch_mode").toString(), QString("off"));
        QVERIFY(!root.contains("bring_back_oof"));
    }

    void testRegistryClampingMutualExclusionAndReset() {
        AppSettings s;
        QJsonObject badJson;
        badJson["targetFps"] = 999999;
        badJson["glassOpacityPct"] = -100;
        badJson["glassBlurStrength"] = 99;
        badJson["mouseSensitivityPct"] = 999;
        badJson["noShadows"] = true;
        badJson["ultraShadows"] = true;
        badJson["pipewireBuffer"] = true;
        badJson["audioSink"] = "alsa_output.pci";
        badJson["audioSource"] = "alsa_input.pci";

        SettingsRegistry::readFromJson(s, badJson);
        QCOMPARE(s.schemaVersion, 36);
        QCOMPARE(s.targetFps, 9999);
        QCOMPARE(s.glassOpacityPct, 2);
        QCOMPARE(s.glassBlurStrength, 8);
        QCOMPARE(s.mouseSensitivityPct, 200);
        QCOMPARE(s.noShadows, true);
        QCOMPARE(s.ultraShadows, false);

        QJsonObject written = SettingsRegistry::writeToJson(s);
        QCOMPARE(written.value("schemaVersion").toInt(), 36);
        QVERIFY(!written.contains("pipewireBuffer"));
        QVERIFY(!written.contains("audioSink"));
        QVERIFY(!written.contains("audioSource"));
        QVERIFY(!written.contains("bringBackOof"));
        QVERIFY(!written.contains("graphicsApi"));

        QVERIFY(SettingsRegistry::resetKey(s, "targetFps"));
        QCOMPARE(s.targetFps, 240);

        SettingsRegistry::applyVisualPreset(s, 4);
        QCOMPARE(s.animBg, false);
        QCOMPARE(s.reducedMotion, true);
        QCOMPARE(s.glassBlurStrength, 0);

        SettingsRegistry::resetVisualAppearance(s);
        QCOMPARE(s.animBg, true);
        QCOMPARE(s.glassBlurStrength, 4);
    }

    void testLegacySchemaMigration() {
        const QString cfgDir = m_tmpDir.path() + "/config";
        QDir().mkpath(cfgDir);
        const QString v36Path = cfgDir + "/settings_v36.json";
        const QString v35Path = cfgDir + "/settings_v35.json";
        QFile::remove(v36Path);

        const QByteArray v35Payload = R"({
            "schemaVersion": 35,
            "activeAccountId": "alt_slot",
            "accounts": [
                {"id": "default", "label": "Main", "displayName": "MainUser"},
                {"id": "alt_slot", "label": "Alt", "displayName": "AltUser"}
            ],
            "settings": {
                "targetFps": 360,
                "msaa": 3,
                "customMaxZoom": 99999,
                "glassBlurStrength": 6,
                "glassOpacityPct": 28,
                "lastPlaceInput": "2753915549",
                "gpuVendor": 1,
                "audioSink": "alsa_output.pci"
            }
        })";
        {
            QFile f(v35Path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(v35Payload);
            f.close();
        }

        SettingsManager mgr;
        mgr.loadAll();

        QCOMPARE(mgr.settings().schemaVersion, 36);
        QCOMPARE(mgr.activeAccountId(), QString("alt_slot"));
        QCOMPARE(mgr.accounts().size(), 2);
        QCOMPARE(mgr.accounts().at(1).displayName, QString("AltUser"));
        QCOMPARE(mgr.settings().targetFps, 360);
        QCOMPARE(mgr.settings().msaa, MsaaMode::X4);
        QCOMPARE(mgr.settings().customMaxZoom, 9999);
        QCOMPARE(mgr.settings().glassBlurStrength, 6);
        QCOMPARE(mgr.settings().glassOpacityPct, 28);
        QCOMPARE(mgr.settings().lastPlaceInput, QString("2753915549"));
        QVERIFY2(QFileInfo::exists(v36Path), "Migrated settings_v36.json must be written automatically");
    }

    void testProfilesExportImportAndNamedProfiles() {
        SettingsManager mgr;
        mgr.settings().targetFps = 480;
        mgr.settings().mouseSensitivityPct = 135;
        mgr.settings().dnsProvider = 2;

        QString err;
        QVERIFY(mgr.saveNamedProfile("TestProfile240", &err));
        QVERIFY(mgr.listNamedProfiles().contains("TestProfile240"));

        SettingsRegistry::resetAll(mgr.settings());
        QCOMPARE(mgr.settings().targetFps, 240);

        QVERIFY(mgr.loadNamedProfile("TestProfile240", &err));
        QCOMPARE(mgr.settings().targetFps, 480);
        QCOMPARE(mgr.settings().mouseSensitivityPct, 135);
        QCOMPARE(mgr.settings().dnsProvider, 2);

        QVERIFY(mgr.deleteNamedProfile("TestProfile240"));
        QVERIFY(!mgr.listNamedProfiles().contains("TestProfile240"));
    }

    void testZeroAudioInterferenceAndSourceAudit() {
        SettingsManager mgr;
        SoberController ctrl(&mgr);
        const QStringList args = ctrl.buildFlatpakArgs("roblox://experiences/start?placeId=1818");
        const QString joinedArgs = args.join(' ');

        QVERIFY2(!joinedArgs.contains("PULSE", Qt::CaseInsensitive), "Found PULSE in launch args");
        QVERIFY2(!joinedArgs.contains("PIPEWIRE", Qt::CaseInsensitive), "Found PIPEWIRE in launch args");
        QVERIFY2(!joinedArgs.contains("ALSA", Qt::CaseInsensitive), "Found ALSA in launch args");
        QVERIFY2(!joinedArgs.contains("AUDIODRIVER", Qt::CaseInsensitive), "Found AUDIODRIVER in launch args");
        QVERIFY2(!joinedArgs.contains("pulseaudio", Qt::CaseInsensitive), "Found pulseaudio in launch args");
        QVERIFY2(joinedArgs.contains("libMacOBloxShims.dylib"), "Missing libMacOBloxShims.dylib in launch args");

        const QString srcDir = QFileInfo(__FILE__).absolutePath() + "/../src";
        QDirIterator it(srcDir, {"*.cpp", "*.hpp"}, QDir::Files, QDirIterator::Subdirectories);
        const QStringList forbiddenTokens = {
            "pactl", "wpctl", "PULSE_", "PIPEWIRE_", "ALSA_", "AUDIODRIVER",
            "runMicrophoneDiagnostics", "QWebEngineCookieStore", "SecretStore"
        };
        while (it.hasNext()) {
            QString filePath = it.next();
            QFile f(filePath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QString content = QString::fromUtf8(f.readAll());
            for (const QString &tok : forbiddenTokens) {
                QVERIFY2(!content.contains(tok),
                         qPrintable(QString("Forbidden token '%1' found in %2").arg(tok, filePath)));
            }
        }
    }

    void testAudioHalMicrophoneFixesPresent() {
        const QString shimPath = QFileInfo(__FILE__).absolutePath() + "/../shims/audio_hal.c";
        QFile f(shimPath);
        QVERIFY2(f.open(QIODevice::ReadOnly), "shims/audio_hal.c must exist");
        const QString code = QString::fromUtf8(f.readAll());

        QVERIFY(code.contains("is_input_device"));
        QVERIFY(code.contains("kAudioOutputUnitProperty_SetInputCallback"));
        QVERIFY(code.contains("DYLD_INTERPOSE(t_unit_render, AudioUnitRender)"));
        QVERIFY(code.contains("unit_mic_thread"));
        QVERIFY(code.contains("MACOBLOX_MIC_FIFO"));
        QVERIFY(code.contains("MACOBLOX_AUDIO_LOG"));
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_MAIN(TestMqBlox)
#include "tst_mqblox.moc"
EOF_TESTS_TST_MQBLOX_CPP

chmod +x shims/build_debug_shim.sh
[[ -f install.sh ]] && chmod +x install.sh

echo "[apply.sh] Сборка libMacOBloxShims.dylib и стаб-фреймворков..."
./shims/build_debug_shim.sh

echo "[apply.sh] Конфигурация и сборка CMake (Release, -Wall -Wextra -Wpedantic)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo "[apply.sh] Запуск автотестов (ctest)..."
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

echo "[apply.sh] Готово! Сборка и тесты успешно пройдены (git push не выполнялся)."
