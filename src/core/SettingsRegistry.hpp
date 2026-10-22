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
        s.schemaVersion = 37;
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
        if (s.optMode.isEmpty()) s.optMode = "performance";
        if (s.customFflagsJson.trimmed().isEmpty()) s.customFflagsJson = "{}";
        s.noclipCamWalls = false;
        s.wallhopPhysics = false;
    }

    static QJsonObject writeToJson(const AppSettings &s) {
        QJsonObject obj;
        obj["schemaVersion"] = 37;
        for (const auto &d : all()) {
            QVariant val = d.getter(s);
            if (d.type == SettingValueType::Bool) {
                obj[d.key] = val.toBool();
            } else if (d.type == SettingValueType::Int) {
                obj[d.key] = std::clamp(val.toInt(), d.minInt, d.maxInt);
            } else if (d.type == SettingValueType::String) {
                obj[d.key] = val.toString();
            }
        }
        obj["noclipCamWalls"] = false;
        obj["wallhopPhysics"] = false;
        return obj;
    }

private:
    static QVector<SettingDescriptor> buildRegistry() {
        AppSettings def;
        QVector<SettingDescriptor> reg;

        auto addBool = [&](const char *key, SettingCategory cat, const char *title, const char *desc,
                           bool restart, bool AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                QString::fromUtf8(key), SettingValueType::Bool, cat,
                QString::fromUtf8(title), QString::fromUtf8(desc), restart,
                def.*member, 0, 1,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member](AppSettings &s, const QVariant &v) { s.*member = v.toBool(); }
            });
        };

        auto addInt = [&](const char *key, SettingCategory cat, const char *title, const char *desc,
                          bool restart, int minV, int maxV, int AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                QString::fromUtf8(key), SettingValueType::Int, cat,
                QString::fromUtf8(title), QString::fromUtf8(desc), restart,
                def.*member, minV, maxV,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member, minV, maxV](AppSettings &s, const QVariant &v) {
                    s.*member = std::clamp(v.toInt(), minV, maxV);
                }
            });
        };

        auto addString = [&](const char *key, SettingCategory cat, const char *title, const char *desc,
                             bool restart, QString AppSettings::*member) {
            reg.push_back(SettingDescriptor{
                QString::fromUtf8(key), SettingValueType::String, cat,
                QString::fromUtf8(title), QString::fromUtf8(desc), restart,
                def.*member, 0, 0,
                [member](const AppSettings &s) -> QVariant { return s.*member; },
                [member](AppSettings &s, const QVariant &v) { s.*member = v.toString(); }
            });
        };

        auto addEnum = [&](const char *key, SettingCategory cat, const char *title, const char *desc,
                           bool restart, int minV, int maxV, auto member) {
            using EnumT = std::decay_t<decltype(def.*member)>;
            reg.push_back(SettingDescriptor{
                QString::fromUtf8(key), SettingValueType::Int, cat,
                QString::fromUtf8(title), QString::fromUtf8(desc), restart,
                static_cast<int>(def.*member), minV, maxV,
                [member](const AppSettings &s) -> QVariant { return static_cast<int>(s.*member); },
                [member, minV, maxV](AppSettings &s, const QVariant &v) {
                    s.*member = static_cast<EnumT>(std::clamp(v.toInt(), minV, maxV));
                }
            });
        };

        addEnum("resMode", SettingCategory::System, "Разрешение рендера", "Масштаб окна игры", true, 0, 3, &AppSettings::resMode);
        addEnum("gpuVendor", SettingCategory::System, "Профиль видеокарты", "Оптимизация драйвера AMD/NVIDIA/Intel", true, 0, 3, &AppSettings::gpuVendor);
        addEnum("cpuVendor", SettingCategory::System, "Профиль процессора", "Планировщик потоков CPU", true, 0, 2, &AppSettings::cpuVendor);
        addEnum("windowDriver", SettingCategory::System, "Оконный протокол", "Wayland или X11 (XWayland)", true, 0, 2, &AppSettings::windowDriver);

        addBool("returnToMqBloxOnLeave", SettingCategory::System, "Возвращать в MqBlox при выходе", "Разворачивать лаунчер после закрытия плейса", false, &AppSettings::returnToMqBloxOnLeave);
        addBool("pauseWebInGame", SettingCategory::System, "Замораживать браузер в игре", "0% нагрузки от браузера во время игры", false, &AppSettings::pauseWebInGame);
        addBool("animBg", SettingCategory::Appearance, "Анимированный видеофон", "Плавное вращение чёрной дыры", false, &AppSettings::animBg);
        addBool("closeToTray", SettingCategory::System, "Сворачивать в трей при закрытии", "Оставлять лаунчер в системном лотке", false, &AppSettings::closeToTray);
        addBool("glThread", SettingCategory::System, "Многопоточный OpenGL/Vulkan", "mesa_glthread и RADV_PERFTEST=gpl", true, &AppSettings::glThread);
        addBool("asyncShaders", SettingCategory::System, "Асинхронный кэш шейдеров", "Убирает микрофризы при подгрузке эффектов", true, &AppSettings::asyncShaders);
        addBool("vsyncOff", SettingCategory::System, "Отключить VSync драйвера", "vblank_mode=0", true, &AppSettings::vsyncOff);
        addBool("pipewireBuffer", SettingCategory::System, "Нативный аудиорежим Sober", "Звуком и микрофоном управляет сам Sober в настройках игры", true, &AppSettings::pipewireBuffer);
        addBool("mallocTrim", SettingCategory::System, "Сжатие неиспользуемой ОЗУ", "Автоматический возврат памяти системе", true, &AppSettings::mallocTrim);
        addBool("smartAutoCleanCache", SettingCategory::System, "Автоочистка старого кэша", "Очистка временных файлов", false, &AppSettings::smartAutoCleanCache);
        addBool("lowRamFflags", SettingCategory::System, "Режим экономии ОЗУ (Low-RAM)", "Уменьшает буферы текстур и света", true, &AppSettings::lowRamFflags);
        addBool("disableTelemetry", SettingCategory::System, "Отключить телеметрию Roblox", "Блокирует отправку аналитики", true, &AppSettings::disableTelemetry);
        addBool("enableGamemode", SettingCategory::System, "Включить Feral GameMode", "Приоритет планировщика ядра Linux", true, &AppSettings::enableGamemode);
        addBool("autoLaunchFromBrowser", SettingCategory::System, "Перехват ссылок roblox://", "Автоматический вход с сайта Roblox", false, &AppSettings::autoLaunchFromBrowser);
        addBool("autoReconnectOnCrash", SettingCategory::System, "Автоперезаход при вылете", "Повторный запуск при сбое клиента", false, &AppSettings::autoReconnectOnCrash);
        addBool("hideSoberDesktop", SettingCategory::System, "Скрыть дубликат ярлыка Sober", "Оставить в меню только иконку MqBlox", false, &AppSettings::hideSoberDesktop);
        addBool("sidebarPinned", SettingCategory::Layout, "Закрепить боковую панель", "Не сворачивать меню при уводе мыши", false, &AppSettings::sidebarPinned);

        addEnum("graphicsApi", SettingCategory::Graphics, "Графический API", "Vulkan или OpenGL", true, 0, 1, &AppSettings::graphicsApi);
        addString("optMode", SettingCategory::Graphics, "Профиль оптимизации Sober", "performance / balanced / quality", true, &AppSettings::optMode);
        addEnum("lighting", SettingCategory::Graphics, "Технология освещения", "Voxel, ShadowMap или Future Phase 3", true, 0, 3, &AppSettings::lighting);
        addEnum("textureQuality", SettingCategory::Graphics, "Качество текстур", "Уровень детализации 0..3", true, 0, 4, &AppSettings::textureQuality);
        addEnum("msaa", SettingCategory::Graphics, "Сглаживание MSAA", "Off / 2x / 4x / 8x", true, 0, 3, &AppSettings::msaa);
        addInt("targetFps", SettingCategory::Graphics, "Целевой лимит FPS", "Максимальная частота кадров (30..9999)", true, 30, 9999, &AppSettings::targetFps);
        addBool("unlimitedFps", SettingCategory::Graphics, "Безлимитный FPS (9999)", "Снять ограничение кадров полностью", true, &AppSettings::unlimitedFps);
        addInt("frmQualityLevel", SettingCategory::Graphics, "Уровень графики FRM", "Ступень рендера (0..21)", true, 0, 21, &AppSettings::frmQualityLevel);

        addBool("noShadows", SettingCategory::Graphics, "Отключить все тени", "Повышает FPS и осветляет карту", true, &AppSettings::noShadows);
        addBool("ultraShadows", SettingCategory::Graphics, "Ультра-чёткие тени", "Максимальная детализация теней", true, &AppSettings::ultraShadows);
        addBool("noPostFx", SettingCategory::Graphics, "Отключить PostFX", "Убирает Bloom, Blur и солнечные лучи", true, &AppSettings::noPostFx);
        addBool("lowMeshes", SettingCategory::Graphics, "Упростить дальние меши", "Быстрое переключение LOD", true, &AppSettings::lowMeshes);
        addBool("greySky", SettingCategory::Graphics, "Нейтральное серое небо", "Убирает текстуры облаков", true, &AppSettings::greySky);
        addBool("bringBackOof", SettingCategory::Graphics, "Классический звук Oof", "Возвращает старый звук смерти", true, &AppSettings::bringBackOof);
        addBool("discordRpc", SettingCategory::Graphics, "Статус в Discord (Rich Presence)", "Показывать игру в профиле Discord", true, &AppSettings::discordRpc);
        addBool("serverRegionIndicator", SettingCategory::Graphics, "Индикатор региона сервера", "Показывать локацию сервера", true, &AppSettings::serverRegionIndicator);
        addBool("gamepadSupport", SettingCategory::Graphics, "Режим консоли (Console UI)", "Включает интерфейс Xbox/PS (выключите для обычного ПК-режима)", true, &AppSettings::gamepadSupport);
        addBool("enableHiDpi", SettingCategory::Graphics, "Масштабирование HiDPI в игре", "Чёткий интерфейс на 2K/4K экранах", true, &AppSettings::enableHiDpi);
        addString("customFflagsJson", SettingCategory::Graphics, "Пользовательские FastFlags (JSON)", "Собственный JSON-объект флагов", true, &AppSettings::customFflagsJson);

        addEnum("camZoom", SettingCategory::Tweaks, "Пресет дистанции камеры", "Быстрый выбор дальности обзора", true, 0, 3, &AppSettings::camZoom);
        addEnum("netProfile", SettingCategory::Tweaks, "Сетевой пакетный профиль", "MTU 1396 или стандартный", true, 0, 1, &AppSettings::netProfile);
        addBool("showHudFps", SettingCategory::Tweaks, "Счётчик FPS на экране", "Индикатор кадров и GALLIUM_HUD", true, &AppSettings::showHudFps);
        addBool("clearFog", SettingCategory::Tweaks, "Убрать 3D-траву на ландшафте", "Чистый обзор поверхности", true, &AppSettings::clearFog);
        addInt("customMaxZoom", SettingCategory::Tweaks, "Максимальная дистанция зума", "Лимит отдаления камеры в студах", true, 128, 100000, &AppSettings::customMaxZoom);
        addBool("unlockInfiniteZoom", SettingCategory::Tweaks, "Разблокировать отдаление камеры", "Применяет пользовательский лимит студов", true, &AppSettings::unlockInfiniteZoom);
        addBool("fullBright", SettingCategory::Tweaks, "Высветление локаций (FullBright)", "Voxel-рендер без теней и темноты", true, &AppSettings::fullBright);
        addBool("showHitboxes", SettingCategory::Tweaks, "Упростить геометрию объектов (LOD 0)", "Низкополигональные модели", true, &AppSettings::showHitboxes);

        addInt("glassBlurStrength", SettingCategory::Appearance, "Сила размытия фона", "Радиус размытия подложки (0..8)", false, 0, 8, &AppSettings::glassBlurStrength);
        addInt("glassOpacityPct", SettingCategory::Appearance, "Непрозрачность панелей (%)", "Плотность карточек (2..60%)", false, 2, 60, &AppSettings::glassOpacityPct);
        addInt("borderThicknessPx", SettingCategory::Appearance, "Толщина рамки (×0.1 px)", "5..25 (10 = 1.0px)", false, 5, 25, &AppSettings::borderThicknessPx);
        addInt("borderBrightnessPct", SettingCategory::Appearance, "Яркость рамок (%)", "Интенсивность свечения граней (20..180%)", false, 20, 180, &AppSettings::borderBrightnessPct);
        addInt("glassSaturationPct", SettingCategory::Appearance, "Насыщенность подложки (%)", "Сочность цвета под стеклом (80..220%)", false, 80, 220, &AppSettings::glassSaturationPct);
        addInt("specularStrengthPct", SettingCategory::Appearance, "Сила бликов стекла (%)", "Яркость верхнего блика и курсора (0..200%)", false, 0, 200, &AppSettings::specularStrengthPct);
        addInt("shadowStrengthPct", SettingCategory::Appearance, "Сила внешних теней (%)", "Глубина тени под карточками (0..200%)", false, 0, 200, &AppSettings::shadowStrengthPct);
        addInt("cornerRadiusMode", SettingCategory::Appearance, "Радиус скругления", "0 = Компактный, 1 = Стандартный, 2 = Круглый", false, 0, 2, &AppSettings::cornerRadiusMode);
        addInt("glassStyleMode", SettingCategory::Appearance, "Стиль стекла", "0 = Прозрачное, 1 = Матовое, 2 = Тонированное", false, 0, 2, &AppSettings::glassStyleMode);

        addInt("themeMode", SettingCategory::Appearance, "Цветовая тема", "0 = Тёмная, 1 = Светлая, 2 = Авто", false, 0, 2, &AppSettings::themeMode);
        addInt("accentPreset", SettingCategory::Appearance, "Палитра акцента", "0 = Голубой, 1 = Золотой, 2 = Изумрудный, 3 = Фиолетовый, 4 = Свой HEX", false, 0, 4, &AppSettings::accentPreset);
        addString("customAccentHex", SettingCategory::Appearance, "Свой HEX-цвет акцента", "Например #38BDF8", false, &AppSettings::customAccentHex);
        addBool("chameleonEnabled", SettingCategory::Appearance, "Эффект Хамелеона", "Адаптация цвета рамок под видеофон", false, &AppSettings::chameleonEnabled);
        addInt("chameleonSpeedPct", SettingCategory::Appearance, "Скорость Хамелеона (%)", "Скорость перетекания оттенков (5..100%)", false, 5, 100, &AppSettings::chameleonSpeedPct);
        addInt("chameleonStrengthPct", SettingCategory::Appearance, "Сила Хамелеона (%)", "Насыщенность адаптивного цвета (20..150%)", false, 20, 150, &AppSettings::chameleonStrengthPct);
        addInt("borderColorSource", SettingCategory::Appearance, "Источник цвета рамки", "0 = Из видео, 1 = Акцент, 2 = Фиксированный", false, 0, 2, &AppSettings::borderColorSource);
        addInt("textColorMode", SettingCategory::Appearance, "Цвет текста", "0 = Авто по контрасту, 1 = Светлый, 2 = Тёмный", false, 0, 2, &AppSettings::textColorMode);

        addInt("backdropType", SettingCategory::Appearance, "Тип заднего фона", "0 = Видео, 1 = Картинка, 2 = Градиент, 3 = Свой файл", false, 0, 3, &AppSettings::backdropType);
        addString("customBackdropPath", SettingCategory::Appearance, "Путь к своему фону", "Локальный файл .mp4 / .png / .jpg", false, &AppSettings::customBackdropPath);
        addInt("backdropDimPct", SettingCategory::Appearance, "Затемнение фона (%)", "Дополнительное затемнение фона (0..85%)", false, 0, 85, &AppSettings::backdropDimPct);
        addInt("videoFpsLimit", SettingCategory::Appearance, "Лимит FPS видеофона", "15, 24 или 30 FPS", false, 15, 60, &AppSettings::videoFpsLimit);
        addBool("pauseOnFocusLoss", SettingCategory::Appearance, "Пауза фона вне фокуса", "Останавливать видео при потере фокуса", false, &AppSettings::pauseOnFocusLoss);

        addBool("animationsEnabled", SettingCategory::Appearance, "Анимации интерфейса", "Плавные переходы", false, &AppSettings::animationsEnabled);
        addInt("animSpeedPct", SettingCategory::Appearance, "Скорость анимаций (%)", "50..200%", false, 50, 200, &AppSettings::animSpeedPct);
        addInt("animStyle", SettingCategory::Appearance, "Стиль анимаций", "0 = Плавные, 1 = Пружинные, 2 = Без анимаций", false, 0, 2, &AppSettings::animStyle);
        addBool("cursorGlowFollow", SettingCategory::Appearance, "Следование блика за курсором", "Подсветка под мышью", false, &AppSettings::cursorGlowFollow);
        addBool("reducedMotion", SettingCategory::Accessibility, "Режим «Меньше движения»", "Отключает пружины и блик курсора", false, &AppSettings::reducedMotion);

        addInt("uiScalePct", SettingCategory::Layout, "Масштаб интерфейса (%)", "80..150%", false, 80, 150, &AppSettings::uiScalePct);
        addInt("fontSizePx", SettingCategory::Layout, "Размер шрифта (px)", "11..17 px", false, 11, 17, &AppSettings::fontSizePx);
        addInt("fontBoldness", SettingCategory::Layout, "Начертание шрифта", "0 = Обычное, 1 = Полужирное", false, 0, 1, &AppSettings::fontBoldness);
        addInt("uiDensity", SettingCategory::Layout, "Плотность макета", "0 = Компактно, 1 = Обычно, 2 = Просторно", false, 0, 2, &AppSettings::uiDensity);

        addInt("sidebarPosition", SettingCategory::Layout, "Положение панели навигации", "0 = Слева, 1 = Сверху", false, 0, 1, &AppSettings::sidebarPosition);
        addInt("sidebarWidthPx", SettingCategory::Layout, "Ширина боковой панели (px)", "160..280 px", false, 160, 280, &AppSettings::sidebarWidthPx);
        addBool("sidebarShowLabels", SettingCategory::Layout, "Подписи у иконок меню", "Показывать названия вкладок", false, &AppSettings::sidebarShowLabels);
        addBool("rememberWindowSize", SettingCategory::Layout, "Запоминать размер окна", "Сохранять ширину и высоту", false, &AppSettings::rememberWindowSize);
        addInt("savedWindowWidth", SettingCategory::Layout, "Сохранённая ширина окна", "820..3840", false, 820, 3840, &AppSettings::savedWindowWidth);
        addInt("savedWindowHeight", SettingCategory::Layout, "Сохранённая высота окна", "540..2160", false, 540, 2160, &AppSettings::savedWindowHeight);
        addBool("startMinimized", SettingCategory::Layout, "Запуск свёрнутым в трей", "Не показывать окно при старте", false, &AppSettings::startMinimized);
        addBool("alwaysOnTop", SettingCategory::Layout, "Всегда поверх других окон", "Доступно в сессии X11", false, &AppSettings::alwaysOnTop);

        addBool("highContrast", SettingCategory::Accessibility, "Высокий контраст", "Усиленная читаемость текста", false, &AppSettings::highContrast);
        addBool("reduceTransparency", SettingCategory::Accessibility, "Уменьшить прозрачность", "Плотный фон карточек", false, &AppSettings::reduceTransparency);
        addInt("effectsQuality", SettingCategory::Accessibility, "Качество оптических эффектов", "0 = Низкое, 1 = Среднее, 2 = Высокое, 3 = Авто", false, 0, 3, &AppSettings::effectsQuality);
        addBool("disableBlurLowGpu", SettingCategory::Accessibility, "Отключить размытие (для слабых GPU)", "Без фильтра размытия подложки", false, &AppSettings::disableBlurLowGpu);
        addInt("visualPreset", SettingCategory::Appearance, "Пресет оформления", "0 = Liquid Glass, 1 = Матовое стекло, 2 = Минимализм, 3 = Контраст, 4 = Экономный", false, 0, 4, &AppSettings::visualPreset);

        addString("lastPlaceInput", SettingCategory::General, "Последний Place ID / Ссылка", "Сохранённый адрес сервера", false, &AppSettings::lastPlaceInput);
        return reg;
    }
};

} // namespace mqblox
