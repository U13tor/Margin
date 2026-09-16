#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>

class QQmlEngine;

namespace Margin {
class HostServices;
}

namespace Margin::Plugins::LlamaPet {

class TelemetryService;
class DockBehavior;

class FloatingWindows : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString skinJson READ skinJson CONSTANT)

public:
    enum class Form {
        MiniPet = 0,
        Dock = 1
    };
    Q_ENUM(Form)

    QString skinJson() const;

    explicit FloatingWindows(QObject* parent = nullptr);
    ~FloatingWindows() override;

    bool initialize(QQmlEngine* engine, Margin::HostServices* host,
                    TelemetryService* telemetry, DockBehavior* dockBehavior);
    void cleanup();

    Q_INVOKABLE void startSystemMove(QQuickWindow* win);
    Q_INVOKABLE void switchForm(int formIndex);
    Q_INVOKABLE void handleMouseEnter(QQuickWindow* win);
    Q_INVOKABLE void handleMouseLeave(QQuickWindow* win);
    Q_INVOKABLE void showContextMenu(QQuickWindow* win);

    void showCurrent();
    void hideCurrent();
    void toggleVisibility();

    void setClickThrough(bool enabled);
    bool isClickThrough() const { return m_clickThrough; }

    Form currentForm() const { return m_currentForm; }
    QQuickWindow* currentWindow() const;
    QQuickWindow* miniPetWindow() const { return m_miniPetWindow; }
    QQuickWindow* dockWindow() const { return m_dockWindow; }

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void formChanged(Form newForm);
    void clickThroughChanged(bool enabled);
    void visibilityChanged(bool visible);

private:
    QQuickWindow* createWindowFromQml(QQmlEngine* engine, const QString& qmlUrl,
                                      const QSize& size);

    Margin::HostServices* m_host{nullptr};
    TelemetryService* m_telemetry{nullptr};
    DockBehavior* m_dockBehavior{nullptr};

    QPointer<QQuickWindow> m_miniPetWindow;
    QPointer<QQuickWindow> m_dockWindow;

    Form m_currentForm{Form::MiniPet};
    bool m_clickThrough{false};
    bool m_userVisible{true};
};

} // namespace Margin::Plugins::LlamaPet
