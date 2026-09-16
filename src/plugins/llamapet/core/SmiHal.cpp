#include "SmiHal.h"

#include <QProcess>
#include <QStringList>

namespace Margin::Plugins::LlamaPet {

namespace {

float parseF32OrDefault(const QString& str) {
    const QString s = str.trimmed();
    if (s.contains(QStringLiteral("N/A"), Qt::CaseInsensitive) ||
        s.contains(QStringLiteral("Not Supported"), Qt::CaseInsensitive)) {
        return 0.0f;
    }
    bool ok = false;
    float v = s.toFloat(&ok);
    return ok ? std::max(0.0f, v) : 0.0f;
}

quint32 parseU32OrDefault(const QString& str) {
    const QString s = str.trimmed();
    if (s.contains(QStringLiteral("N/A"), Qt::CaseInsensitive) ||
        s.contains(QStringLiteral("Not Supported"), Qt::CaseInsensitive)) {
        return 0;
    }
    bool ok = false;
    uint v = s.toUInt(&ok);
    return ok ? static_cast<quint32>(v) : 0;
}

} // namespace

Result<GpuMetrics, QString> parseSmiCsv(const QString& input) {
    const QStringList lines = input.split(QLatin1Char('\n'));
    QString targetLine;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            targetLine = trimmed;
            break;
        }
    }

    if (targetLine.isEmpty()) {
        return Result<GpuMetrics, QString>::err(QStringLiteral("nvidia-smi 输出内容为空"));
    }

    const QStringList rawParts = targetLine.split(QLatin1Char(','));
    if (rawParts.size() < 7) {
        return Result<GpuMetrics, QString>::err(
            QStringLiteral("nvidia-smi CSV 列数不足 7 列 (实际 %1 列): %2")
                .arg(rawParts.size())
                .arg(targetLine));
    }

    QStringList parts;
    parts.reserve(rawParts.size());
    for (const QString& p : rawParts) {
        parts.append(p.trimmed());
    }

    GpuMetrics m;
    m.deviceName = parts[0];
    m.vramUsedMb = parseF32OrDefault(parts[1]);
    m.vramTotalMb = parseF32OrDefault(parts[2]);
    m.tempC = parseU32OrDefault(parts[3]);
    m.powerW = parseF32OrDefault(parts[4]);
    m.powerLimitW = parseF32OrDefault(parts[5]);
    m.gpuUtil = parseU32OrDefault(parts[6]);

    return Result<GpuMetrics, QString>::ok(m);
}

SmiHal::SmiHal(quint32 gpuIndex) : m_gpuIndex(gpuIndex) {
}

bool SmiHal::isAvailable() const {
    // 探测一次 nvidia-smi 是否可执行
    QProcess proc;
#if defined(Q_OS_WIN)
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    proc.start(QStringLiteral("nvidia-smi"), {QStringLiteral("-h")});
    if (!proc.waitForFinished(1000)) {
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

Result<GpuMetrics, QString> SmiHal::pollMetrics() {
    QProcess proc;
#if defined(Q_OS_WIN)
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    QStringList args;
    args << QStringLiteral("--id=%1").arg(m_gpuIndex);
    args << QStringLiteral("--query-gpu=name,memory.used,memory.total,temperature.gpu,power.draw,power.limit,utilization.gpu");
    args << QStringLiteral("--format=csv,noheader,nounits");

    proc.start(QStringLiteral("nvidia-smi"), args);
    if (!proc.waitForFinished(2000)) {
        proc.kill();
        return Result<GpuMetrics, QString>::err(QStringLiteral("nvidia-smi 采集超时 (超过 2 秒)"));
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError());
        return Result<GpuMetrics, QString>::err(
            QStringLiteral("nvidia-smi 命令执行失败: %1").arg(err));
    }

    const QString stdoutContent = QString::fromUtf8(proc.readAllStandardOutput());
    return parseSmiCsv(stdoutContent);
}

} // namespace Margin::Plugins::LlamaPet
