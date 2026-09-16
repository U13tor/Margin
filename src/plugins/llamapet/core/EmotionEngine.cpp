#include "EmotionEngine.h"

namespace Margin::Plugins::LlamaPet {

EmotionEngine::EmotionEngine()
    : m_current(Emotion::Idle),
      m_oomRecoveryTicks(0),
      m_offlineFailTicks(0),
      m_happyCacheTicks(0) {
}

Emotion EmotionEngine::tick(const EmotionInputs& input) {
    // 1. PanickingOom 判定与滞回 (优先级 1)
    // 进入：vram_percent >= vram_danger 立即全局触发
    // 退出：vram_percent < vram_warn 持续 >= 2 周期
    bool inOom = false;
    if (m_current == Emotion::PanickingOom) {
        if (input.vramPercent < input.vramWarn) {
            m_oomRecoveryTicks++;
            if (m_oomRecoveryTicks >= 2) {
                m_oomRecoveryTicks = 0;
                inOom = false;
            } else {
                inOom = true;
            }
        } else {
            m_oomRecoveryTicks = 0;
            inOom = true;
        }
    } else {
        m_oomRecoveryTicks = 0;
        inOom = (input.vramPercent >= input.vramDanger);
    }

    if (inOom) {
        m_current = Emotion::PanickingOom;
        return m_current;
    }

    // 2. ConfusedOffline 判定与滞回 (优先级 2)
    // 进入：连续 >= 2 周期连接失败
    // 退出：任一次连接成功立即退出
    bool isOffline = false;
    if (!input.connected) {
        m_offlineFailTicks++;
        isOffline = (m_offlineFailTicks >= 2);
    } else {
        m_offlineFailTicks = 0;
        isOffline = false;
    }

    if (isOffline) {
        m_current = Emotion::ConfusedOffline;
        return m_current;
    }

    // 3. 工作态 Prefill (优先级 3a: any_prefill)
    if (input.anyPrefill) {
        m_happyCacheTicks = 0;
        m_current = Emotion::EatingPrefill;
        return m_current;
    }

    // 4. 工作态 Decoding / HappyCache (优先级 3b / 4: any_decoding)
    if (input.anyDecoding) {
        float hitRate = input.cacheHitRate.value_or(0.0f);
        if (m_current == Emotion::HappyCache) {
            // 滞回退出条件：KV 复用率 < 60%
            if (hitRate < 60.0f) {
                m_happyCacheTicks = 0;
                m_current = Emotion::SpittingTokens;
            } else {
                m_current = Emotion::HappyCache;
            }
        } else {
            // 进入条件：KV 复用率 >= 70% 且持续 2 个周期
            if (hitRate >= 70.0f) {
                m_happyCacheTicks++;
                if (m_happyCacheTicks >= 2) {
                    m_current = Emotion::HappyCache;
                } else {
                    m_current = Emotion::SpittingTokens;
                }
            } else {
                m_happyCacheTicks = 0;
                m_current = Emotion::SpittingTokens;
            }
        }
        return m_current;
    }

    // 无工作任务时，重置 HappyCache 计数器
    m_happyCacheTicks = 0;

    // 5. 空闲待命与浅度睡眠 (优先级 5 & 6)
    if (input.idleTicks >= 300) {
        m_current = Emotion::Sleeping;
    } else {
        m_current = Emotion::Idle;
    }

    return m_current;
}

QString dialogFor(Emotion emotion, std::optional<float> tps) {
    switch (emotion) {
    case Emotion::Sleeping:
        return QStringLiteral("呼.. 模型浅睡中");
    case Emotion::Idle:
        return QStringLiteral("随时准备推理！");
    case Emotion::EatingPrefill:
        return QStringLiteral("正在吞入代码...");
    case Emotion::SpittingTokens:
        if (tps.has_value()) {
            return QStringLiteral("全力生成中 (%1 t/s)").arg(QString::number(*tps, 'f', 1));
        } else {
            return QStringLiteral("全力生成中");
        }
    case Emotion::HappyCache:
        return QStringLiteral("✨ KV 缓存复用达成！");
    case Emotion::PanickingOom:
        return QStringLiteral("🚨 显存快撑爆啦！要掉速！");
    case Emotion::ConfusedOffline:
        return QStringLiteral("未检测到本地服务");
    }
    return QString{};
}

} // namespace Margin::Plugins::LlamaPet
