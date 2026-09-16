#include "EngineConfig.h"
#include <QFile>

namespace Margin::Plugins::LlamaPet {

Result<void, QString> EngineConfig::validate(const EngineConfig& cfg) {
    // 1. 推理引擎验证
    const QString& engine = cfg.inference.engine;
    if (engine != QStringLiteral("llama.cpp") &&
        engine != QStringLiteral("ollama") &&
        engine != QStringLiteral("openai_compatible")) {
        return Result<void, QString>::err(
            QStringLiteral("不支持的推理引擎类型: %1").arg(engine));
    }

    const QString& url = cfg.inference.endpointUrl;
    if (!url.startsWith(QStringLiteral("http://")) &&
        !url.startsWith(QStringLiteral("https://"))) {
        return Result<void, QString>::err(
            QStringLiteral("推理服务地址必须以 http:// 或 https:// 开头"));
    }

    if (cfg.inference.pollIntervalMs < 200 || cfg.inference.pollIntervalMs > 10000) {
        return Result<void, QString>::err(
            QStringLiteral("采样轮询间隔必须在 200ms 至 10000ms 之间"));
    }

    // 2. 硬件阈值验证: warn < danger，且阈值归一化后在 0.1 - 1.0 之间
    float warn = cfg.hardware.vramWarnThreshold;
    if (warn > 1.0f) warn /= 100.0f;
    float danger = cfg.hardware.vramDangerThreshold;
    if (danger > 1.0f) danger /= 100.0f;

    if (warn < 0.1f || warn > 0.99f) {
        return Result<void, QString>::err(
            QStringLiteral("显存警告阈值必须在 0.1 至 0.99 之间"));
    }
    if (danger < 0.1f || danger > 1.0f) {
        return Result<void, QString>::err(
            QStringLiteral("显存危急阈值必须在 0.1 至 1.0 之间"));
    }
    if (warn >= danger) {
        return Result<void, QString>::err(
            QStringLiteral("显存警告阈值必须严格小于危急阈值 (warn < danger)"));
    }

    // 3. 快捷键验证
    const QString hotkey = cfg.shortcuts.toggleClickThrough.trimmed();
    if (hotkey.isEmpty()) {
        return Result<void, QString>::err(QStringLiteral("快捷键配置不能为空"));
    }
    const QString lowerHotkey = hotkey.toLower().remove(QLatin1Char(' '));
    if (lowerHotkey.contains(QStringLiteral("ctrl+shift+p"))) {
        return Result<void, QString>::err(
            QStringLiteral("严禁使用 Ctrl+Shift+P，避免吞掉 IDE 命令面板"));
    }

    return Result<void, QString>::ok();
}

EngineConfig EngineConfig::fromJson(const QJsonObject& obj) {
    EngineConfig cfg;
    if (obj.contains(QStringLiteral("inference")) && obj[QStringLiteral("inference")].isObject()) {
        const QJsonObject inf = obj[QStringLiteral("inference")].toObject();
        if (inf.contains(QStringLiteral("engine"))) cfg.inference.engine = inf[QStringLiteral("engine")].toString();
        if (inf.contains(QStringLiteral("endpoint_url"))) cfg.inference.endpointUrl = inf[QStringLiteral("endpoint_url")].toString();
        if (inf.contains(QStringLiteral("api_key"))) cfg.inference.apiKey = inf[QStringLiteral("api_key")].toString();
        if (inf.contains(QStringLiteral("poll_interval_ms"))) cfg.inference.pollIntervalMs = inf[QStringLiteral("poll_interval_ms")].toInt(1000);
    }
    if (obj.contains(QStringLiteral("hardware")) && obj[QStringLiteral("hardware")].isObject()) {
        const QJsonObject hw = obj[QStringLiteral("hardware")].toObject();
        if (hw.contains(QStringLiteral("backend"))) cfg.hardware.backend = hw[QStringLiteral("backend")].toString();
        if (hw.contains(QStringLiteral("gpu_index"))) cfg.hardware.gpuIndex = static_cast<quint32>(hw[QStringLiteral("gpu_index")].toInt(0));
        if (hw.contains(QStringLiteral("vram_danger_threshold"))) cfg.hardware.vramDangerThreshold = static_cast<float>(hw[QStringLiteral("vram_danger_threshold")].toDouble(0.96));
        if (hw.contains(QStringLiteral("vram_warn_threshold"))) cfg.hardware.vramWarnThreshold = static_cast<float>(hw[QStringLiteral("vram_warn_threshold")].toDouble(0.90));
    }
    if (obj.contains(QStringLiteral("appearance")) && obj[QStringLiteral("appearance")].isObject()) {
        const QJsonObject app = obj[QStringLiteral("appearance")].toObject();
        if (app.contains(QStringLiteral("default_mode"))) cfg.appearance.defaultMode = app[QStringLiteral("default_mode")].toString();
        if (app.contains(QStringLiteral("always_on_top"))) cfg.appearance.alwaysOnTop = app[QStringLiteral("always_on_top")].toBool(true);
        if (app.contains(QStringLiteral("click_through"))) cfg.appearance.clickThrough = app[QStringLiteral("click_through")].toBool(false);
        if (app.contains(QStringLiteral("auto_dock_hide"))) cfg.appearance.autoDockHide = app[QStringLiteral("auto_dock_hide")].toBool(true);
        if (app.contains(QStringLiteral("opacity"))) cfg.appearance.opacity = static_cast<float>(app[QStringLiteral("opacity")].toDouble(0.90));
        if (app.contains(QStringLiteral("scale"))) cfg.appearance.scale = static_cast<float>(app[QStringLiteral("scale")].toDouble(1.0));
    }
    if (obj.contains(QStringLiteral("shortcuts")) && obj[QStringLiteral("shortcuts")].isObject()) {
        const QJsonObject sc = obj[QStringLiteral("shortcuts")].toObject();
        if (sc.contains(QStringLiteral("toggle_click_through"))) cfg.shortcuts.toggleClickThrough = sc[QStringLiteral("toggle_click_through")].toString();
    }
    return cfg;
}

QJsonObject EngineConfig::toJson() const {
    QJsonObject obj;
    QJsonObject inf;
    inf[QStringLiteral("engine")] = inference.engine;
    inf[QStringLiteral("endpoint_url")] = inference.endpointUrl;
    inf[QStringLiteral("api_key")] = inference.apiKey;
    inf[QStringLiteral("poll_interval_ms")] = inference.pollIntervalMs;
    obj[QStringLiteral("inference")] = inf;

    QJsonObject hw;
    hw[QStringLiteral("backend")] = hardware.backend;
    hw[QStringLiteral("gpu_index")] = static_cast<int>(hardware.gpuIndex);
    hw[QStringLiteral("vram_danger_threshold")] = hardware.vramDangerThreshold;
    hw[QStringLiteral("vram_warn_threshold")] = hardware.vramWarnThreshold;
    obj[QStringLiteral("hardware")] = hw;

    QJsonObject app;
    app[QStringLiteral("default_mode")] = appearance.defaultMode;
    app[QStringLiteral("always_on_top")] = appearance.alwaysOnTop;
    app[QStringLiteral("click_through")] = appearance.clickThrough;
    app[QStringLiteral("auto_dock_hide")] = appearance.autoDockHide;
    app[QStringLiteral("opacity")] = appearance.opacity;
    app[QStringLiteral("scale")] = appearance.scale;
    obj[QStringLiteral("appearance")] = app;

    QJsonObject sc;
    sc[QStringLiteral("toggle_click_through")] = shortcuts.toggleClickThrough;
    obj[QStringLiteral("shortcuts")] = sc;

    return obj;
}

std::optional<EngineConfig> EngineConfig::loadFromLlamaPetToml(const QString& customPath) {
    QString tomlPath = customPath;
    if (tomlPath.isEmpty()) {
        const QString appData = qEnvironmentVariable("APPDATA");
        if (!appData.isEmpty()) {
            tomlPath = appData + QStringLiteral("/LlamaPet/config/config.toml");
        }
    }

    if (tomlPath.isEmpty()) {
        return std::nullopt;
    }

    QFile file(tomlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    EngineConfig cfg;
    QString currentSection;

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line.mid(1, line.length() - 2).trimmed().toLower();
            continue;
        }

        int eqIdx = line.indexOf(QLatin1Char('='));
        if (eqIdx < 0) {
            continue;
        }

        QString key = line.left(eqIdx).trimmed().toLower();
        QString val = line.mid(eqIdx + 1).trimmed();
        if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.length() >= 2) {
            val = val.mid(1, val.length() - 2);
        }

        if (currentSection == QStringLiteral("inference")) {
            if (key == QStringLiteral("engine")) cfg.inference.engine = val;
            else if (key == QStringLiteral("endpoint_url")) cfg.inference.endpointUrl = val;
            else if (key == QStringLiteral("api_key")) cfg.inference.apiKey = val;
            else if (key == QStringLiteral("poll_interval_ms")) cfg.inference.pollIntervalMs = val.toInt();
        } else if (currentSection == QStringLiteral("hardware")) {
            if (key == QStringLiteral("backend")) cfg.hardware.backend = val;
            else if (key == QStringLiteral("gpu_index")) cfg.hardware.gpuIndex = static_cast<quint32>(val.toUInt());
            else if (key == QStringLiteral("vram_danger_threshold")) cfg.hardware.vramDangerThreshold = val.toFloat();
            else if (key == QStringLiteral("vram_warn_threshold")) cfg.hardware.vramWarnThreshold = val.toFloat();
        } else if (currentSection == QStringLiteral("appearance")) {
            if (key == QStringLiteral("default_mode")) cfg.appearance.defaultMode = val;
            else if (key == QStringLiteral("always_on_top")) cfg.appearance.alwaysOnTop = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            else if (key == QStringLiteral("click_through")) cfg.appearance.clickThrough = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            else if (key == QStringLiteral("auto_dock_hide")) cfg.appearance.autoDockHide = (val.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            else if (key == QStringLiteral("opacity")) cfg.appearance.opacity = val.toFloat();
            else if (key == QStringLiteral("scale")) cfg.appearance.scale = val.toFloat();
        } else if (currentSection == QStringLiteral("shortcuts")) {
            if (key == QStringLiteral("toggle_click_through")) cfg.shortcuts.toggleClickThrough = val;
        }
    }

    return cfg;
}

} // namespace Margin::Plugins::LlamaPet
