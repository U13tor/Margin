#include "ClickThrough.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Margin::Plugins::LlamaPet {

bool ClickThrough::setClickThrough(QQuickWindow* window, bool enabled) {
    if (!window) return false;
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return false;

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
    } else {
        // 关键纪律：禁用时严格同时双清 WS_EX_TRANSPARENT 与 WS_EX_LAYERED 两标志
        exStyle &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
#else
    return false;
#endif
}

bool ClickThrough::isClickThrough(QQuickWindow* window) {
    if (!window) return false;
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return false;
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    return (exStyle & WS_EX_TRANSPARENT) != 0;
#else
    return false;
#endif
}

} // namespace Margin::Plugins::LlamaPet
