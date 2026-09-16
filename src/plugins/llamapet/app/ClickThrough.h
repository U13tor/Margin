#pragma once

#include <QQuickWindow>

namespace Margin::Plugins::LlamaPet {

class ClickThrough {
public:
    static bool setClickThrough(QQuickWindow* window, bool enabled);
    static bool isClickThrough(QQuickWindow* window);
};

} // namespace Margin::Plugins::LlamaPet
