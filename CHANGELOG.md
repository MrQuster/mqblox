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
