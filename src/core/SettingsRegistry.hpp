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

        // Graphics & FastFlags
        addEnum("graphicsBackend", SettingCategory::Graphics, "Графический бэкенд", "[MacOBlox] 0=OpenGL 4.1 Core (CGL), 1=Vulkan (Native/RADV/Zink)", true, 0, 1, &AppSettings::graphicsBackend);
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
