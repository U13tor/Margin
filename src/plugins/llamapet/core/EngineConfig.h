#pragma once

#include "Margin/Result.h"

#include <QJsonObject>
#include <QString>

namespace Margin::Plugins::LlamaPet {

struct EngineConfig {
    struct Inference {
        QString engine{QStringLiteral("llama.cpp")};
        QString endpointUrl{QStringLiteral("http://127.0.0.1:8080")};
        QString apiKey{};
        int pollIntervalMs{1000};
    } inference;

    struct Hardware {
        QString backend{QStringLiteral("auto")}; // "auto", "nvml", "nvidia-smi"
        quint32 gpuIndex{0};
        float vramDangerThreshold{0.96f};
        float vramWarnThreshold{0.90f};
    } hardware;

    struct Appearance {
        QString defaultMode{QStringLiteral("dock")};
        bool alwaysOnTop{true};
        bool clickThrough{false};
        bool autoDockHide{true};
        float opacity{0.90f};
        float scale{1.0f};
    } appearance;

    struct Shortcuts {
        QString toggleClickThrough{QStringLiteral("Ctrl+Alt+P")};
    } shortcuts;

    static Result<void, QString> validate(const EngineConfig& cfg);
    static EngineConfig fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;
    static std::optional<EngineConfig> loadFromLlamaPetToml(const QString& customPath = QString{});
};

} // namespace Margin::Plugins::LlamaPet
