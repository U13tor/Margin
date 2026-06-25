// Logger service interface — verbatim from docs/05-host-services.md §2.
// Impl: host/services/Logger.cpp.

#pragma once

#include <QString>

namespace Margin {

class Logger {
public:
    enum class Level { Debug, Info, Warn, Error, Fatal };

    virtual void log(Level level, const QString& tag,
                     const QString& message) = 0;

    virtual ~Logger() = default;

    void debug(const QString& tag, const QString& msg) { log(Level::Debug, tag, msg); }
    void info(const QString& tag, const QString& msg)  { log(Level::Info,  tag, msg); }
    void warn(const QString& tag, const QString& msg)  { log(Level::Warn,  tag, msg); }
    void error(const QString& tag, const QString& msg) { log(Level::Error, tag, msg); }
    void fatal(const QString& tag, const QString& msg) { log(Level::Fatal, tag, msg); }
};

} // namespace Margin
