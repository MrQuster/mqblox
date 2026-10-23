# Changelog / История изменений

Все значимые изменения проекта **MqBlox × MacOBlox** документируются в этом файле.

## [36.4.0] — 2026-10-24

### Устранение критических зависаний и задержек запуска (Zero-Hang & Freeze Elimination)
- **Устранение фатального графического конфликта (Vulkan vs OpenGL)**:
  - Исправлена критическая ошибка в `SettingsManager.cpp`, из-за которой выбор графического бэкенда Vulkan записывал в `ClientAppSettings.json` флаг `"FFlagDebugGraphicsDisableOpenGL": true`.
  - Так как клиент Roblox для macOS (Mach-O) не имеет нативного бэкенда Vulkan и слинкован исключительно с OpenGL и Metal, отключение OpenGL приводило к полному отсутствию рабочего рендерера в Darling и вечному зависанию клиента при запуске.
  - Закреплены постоянные флаги `FFlagDebugGraphicsPreferOpenGL=true` и `FFlagDebugGraphicsDisableOpenGL=false`. При выборе режима Vulkan на хосте активируется драйвер Gallium Zink (OpenGL поверх Vulkan), сохраняя совместимость с клиентом.
- **Сторожевой таймер запуска (35s Launch Watchdog)**:
  - В `DarlingController` добавлен сторожевой таймер на 35 секунд: если контейнер Darling или `darlingserver` зависает на этапе инициализации `launchd`, лаунчер принудительно очищает зависшие процессы и сокеты, сбрасывает статус в меню и выводит информативное уведомление, предотвращая вечное состояние «● В ИГРЕ».
- **Неблокирующее завершение сессии (`stopRobloxAsync`)**:
  - Убран блокирующий вызов `waitForFinished(1200)` из основного потока GUI; завершение и очистка процессов переведены на асинхронный однократный таймер.
- **Асинхронная сборка заглушек фреймворков**:
  - Компиляция стабов `CoreML`, `CoreHaptics`, `DeviceCheck` через `clang` перенесена из главного потока интерфейса в фоновый рабочий поток через `QtConcurrent::run`.

### Оптимизация для 4-ядерных CPU и GPU AMD Radeon (Hardware & CPU Tuning)
- **Умное определение гибридной графики (Single vs Multi-GPU)**:
  - Добавлен метод `hasMultipleGpus()`. Переменная `DRI_PRIME=1` теперь экспортируется только при наличии нескольких видеокарт, предотвращая задержки согласования DRI3 на системах с одним дискретным GPU (например, AMD Radeon R9 M360).
- **Балансировка потоков на 4-ядерных процессорах (Intel Core i5-2400S)**:
  - Переменная `mesa_glthread=true` теперь включается только на процессорах с 6 и более потоками, исключая троттлинг и конкуренцию за процессорное время с `darlingserver`.
- **Оптимизация FastFlags при пониженном потреблении памяти (`lowRamFflags`)**:
  - Добавлено принудительное отключение процедурной травы (`FIntFRMMinGrassDistance = 0`, `FIntRenderGrassDetailStrands = 0`) и ограничение задач композитора текстур (`DFIntTextureCompositorActiveJobs = 1`), что предотвращает сбои компиляции шейдеров `GrassVSUnified` и существенно повышает FPS на архитектурах GCN 1.0.
- **Кэширование DNS-запросов и ускорение DoT**:
  - Встроен потокобезопасный LRU-кэш DNS-ответов (`m_dnsCache`) в `DarlingController`, сокращающий задержку повторных сетевых запросов Roblox до 0 мс и предотвращающий насыщение пула потоков. Таймаут DoT ограничен до 120 мс с мгновенным переключением на системный резолвер.
- **Оптимизация дерева виджетов и приостановка в фоне**:
  - Устранено ресурсоемкое рекурсивное сканирование 300+ виджетов в `findBackdropTon618` благодаря статическому кэшированию указателя (`QPointer`).
  - Добавлена обработка `changeEvent(WindowStateChange)`: при сворачивании окна опрос `/proc/meminfo` и фоновые обновления виджетов полностью приостанавливаются.
- **Защита от бесконечного цикла перезаходов при сбоях**:
  - Введен лимит авто-переподключений: максимум 3 попытки за 45 секунд, что исключает циклические перезапуски и зависание операционной системы.

## [36.3.0] — 2026-10-24

### Полный редизайн интерфейса (Clean Minimalist Dark UI)
- **Отказ от фоновых видеодекодеров и тяжелого Liquid Glass**:
  - Полностью удалены `ffmpeg` subprocess, циклы `popen()`, буферы тяжелой многопроходной свертки размытия и видеопотоки черной дыры.
  - Нагрузка на CPU и GPU в состоянии покоя снижена до **0%**.
- **Новая минималистичная эстетика (Raycast / Linear / Modern macOS)**:
  - Глубокая матовая темно-графитовая палитра (`#0A0D16`, карточки `#101623` с четкой 1-пиксельной рамкой `rgba(255, 255, 255, 0.08)`).
  - Высококонтрастная типографика с идеальной читаемостью на экранах любой плотности (Wayland / X11).
  - Сохранены и отполированы тактильные физические spring-анимации нажатий, наведения и переключения табов (60 FPS, нативный легковесный векторный рендеринг Qt).
  - Полное соответствие XDG Base Directory Specification (`~/.config/mqblox`, `~/.local/share/mqblox`, `~/.cache/mqblox`).

### Аппаратная поддержка всего спектра железа (Hardware Optimization)
- **Универсальная поддержка GPU (NVIDIA, AMD, Intel, Asahi/ARM, LLVMpipe)**:
  - Автоматическое определение видеочипов через прямой парсинг PCI Vendor ID в `/sys/class/drm/card*` с фоллбэком на `lspci`.
  - **NVIDIA**: активация `__NV_PRIME_RENDER_OFFLOAD=1`, `__GLX_VENDOR_LIBRARY_NAME=nvidia`, `__VK_LAYER_NV_optimus=NVIDIA_only` для ноутбуков с гибридной графикой и десктопов.
  - **AMD Radeon**: включение `DRI_PRIME=1`, компилятора шейдеров `RADV_PERFTEST=aco` для минимального frametime джиттера и `mesa_glthread=true`.
  - **Intel**: включение многопоточного диспетчера OpenGL `mesa_glthread=true` и драйвера `iris`/`zink`.
- **Многопоточность CPU**:
  - Автоматическая передача `MACOBLOX_CPU_THREADS` на основе `QThread::idealThreadCount()`.
- **Пакетная интеграция**:
  - Добавлена опция `--purge` в `install.sh` для полной очистки пользовательских каталогов, поддержка `update-mime-database`.
  - Тестовый набор расширен до **17 тестов (100% PASS)**.

## [36.2.0] — 2026-10-23

### Безопасность и изоляция (Security & Isolation)
- **Изоляция IPC сокета одиночного экземпляра**:
  - Имя сокета IPC теперь динамически изолировано по UID пользователя (`mqblox_ipc_<uid>`), предотвращая конфликты и перехват при многопользовательской работе в системе.
  - Установлен режим `QLocalServer::UserAccessOption` (права доступа 0600), исключающий неавторизованное подключение других локальных пользователей.
- **Защита от шелл-инъекций и корректное экранирование**:
  - В `Ton618GlWidget::decoderLoop` устранена потенциальная уязвимость интерполяции путей командной строки `ffmpeg` через `popen` с безопасным одинарным квотированием путей.
  - Автоматическое создание каталога `~/.local/share/mqblox/videos` перед конвертацией пользовательских видеофонов.

### Производительность и оптимизация Qt6 (Performance & Optimization)
- **Оптимизация анимационного движка Liquid Glass (60 FPS)**:
  - Устранены непрерывные аллокации `QList<QWidget*>` и `QList<QCheckBox*>` в куче каждые 16 мс при работе тикера физики с переходом на прямую итерацию по `QHash`.
  - Предотвращено накопление дублирующихся слотов `destroyed` при смене вкладок с помощью флага `Qt::UniqueConnection`.
  - Исправлен жизненный цикл анимации `switchTab` (предотвращение dangling pointer при быстром переключении вкладок).
  - Устранена утечка контекстного меню трея (`QMenu` теперь корректно привязан к родительскому `MainWindow`).
  - Корректная обработка отображения журнала Git в бинарных сборках без локального репозитория.

### Рантайм Darling и сборка (Darling Runtime & Build)
- **Быстрый DNS-резолвинг без задержек**:
  - Таймаут попытки DoT сокращён до 250 мс с мгновенным автоматическим фоллбэком на локальный системный DNS хоста через `::getaddrinfo`, устраняя сетевые задержки при старте сессий Roblox.
  - При распаковке `RobloxPlayer.app` гарантированно выставляются биты исполнения `0755` на бинарник `RobloxPlayer`.
- **Оптимизация компиляции (LTO & -O2)**:
  - В `CMakeLists.txt` включена межпроцедурная оптимизация (LTO / IPO) для релизных сборок и флаг `-pipe`.
  - В `shims/build_debug_shim.sh` активирована оптимизация `-O2` при линковке `libMacOBloxShims.dylib`.

## [36.1.0] — 2026-10-23

### Добавлено (Added)
- **Выбор графического бэкенда (OpenGL 4.1 Core vs Vulkan)**:
  - В настройках графики добавлен выпадающий список: `OpenGL 4.1 Core (CGL / gl_profile.c)` и `Vulkan (Native / RADV / Zink)`.
  - Взаимно исключающие FastFlags движка Roblox (`FFlagDebugGraphicsPreferVulkan`, `FFlagDebugGraphicsDisableVulkan`, `FFlagDebugGraphicsPreferOpenGL`, `FFlagDebugGraphicsDisableOpenGL`).
  - При выборе Vulkan лаунчер активирует переменные окружения `MACOBLOX_GRAPHICS_BACKEND=vulkan`, `MESA_LOADER_DRIVER_OVERRIDE=zink`, `GALLIUM_DRIVER=zink` для высокопроизводительного рендера через Vulkan (Mesa RADV для AMD, ANV для Intel, NVK для Nvidia).
  - В `PKGBUILD` и `install.sh` добавлен пакет `vulkan-icd-loader`.
- **Устранение 5-секундных сетевых лагов запуска (DNS Sinkhole)**:
  - Локальный DNS-резолвер в `DarlingController` теперь мгновенно (за 0 мс) возвращает `NXDOMAIN` для доменов телеметрии (`pulsar.roblox.com`, `ecsv2.roblox.com`, `ephemeralcounters.api.roblox.com`), устраняя 5000 мс зависание потоков HTTP cURL при старте.
- **Очистка отчёта диагностики от ложных предупреждений**:
  - В фильтр `isBenignNoise` добавлены штатные информационные логи Roblox (`setStartupThrottle`, `preRenderJob is null`, `SignalRCoreError`, `LuauForge`, `multiget-playability-status`, `Wrap-deformer`).
  - Уточнена формулировка статуса сессии: *"🟢 Сессия активна (Cookies.plist сохранён в Darling; токен защищён и не считывается лаунчером)"*.
- **Автономная команда AI-агентов с сохранением между сессиями**:
  - Создан `GEMINI.md`, правила и спецификации 6 агентов (`code-reviewer`, `security-auditor`, `darling-specialist`, `qa-tester`, `ui-ux-reviewer`, `system-packager`).
  - Добавлено правило обязательного пуша обновлений в Git/GitHub после каждого цикла доработок.

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
