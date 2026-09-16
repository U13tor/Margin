#pragma once

#include "core/TelemetryTypes.h"

namespace Margin::Plugins::LlamaPet::health {

quint32 score(const GpuMetrics& m, bool connected);
StatusLevel statusLevel(quint32 score, const GpuMetrics& m, float danger, float warn, bool connected);
StatusLevel statusLevel(quint32 score, const GpuMetrics& m, float danger, float warn);

// 辅助纯函数（供单测与底层复用）
quint32 score(float vramPercent, quint32 tempC, float powerW, float powerLimitW, bool connected);
StatusLevel statusLevel(quint32 score, float vramPercent, quint32 tempC, float danger, float warn, bool connected);

} // namespace Margin::Plugins::LlamaPet::health
