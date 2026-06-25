// test_rhythm_manifest_events — manifest/code consistency for the Rhythm
// plugin's EventBus surface (mirrors test_aura_manifest_events / A17).
//
// CLAUDE.md §7 forbids undeclared extension points — this test catches the
// regression class "topic publish()'d in code but missing from manifest.json".
// Reads manifest.json from the source tree via RHYTHM_MANIFEST_PATH (CMake
// target_compile_definitions). The expected set is hardcoded from the
// publishEvent() call sites in RhythmPlugin.cpp.
//
// Coverage:
//   - all code publish() topics are declared in manifest.json
//   - subscribe("margin.aura.*") topics are declared in events.subscribe
//   - permissions array contains the two Rhythm needs (input-monitor,
//     notification-show). M4-C13a removed overlay-fullscreen — the
//     break overlay is now a standalone Qt.Tool Window, not an
//     OverlayContributor, so the permission is no longer required.
//   - ui_contributions contains TrayMenuContributor + DashboardTabContributor.
//     OverlayContributor was removed in M4-C13a for the same reason.

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QTest>

#include <cstdio>

class TestRhythmManifestEvents : public QObject {
    Q_OBJECT

private slots:
    void allCodePublishedTopicsAreDeclared();
    void allCodeSubscribedTopicsAreDeclared();
    void permissionsAreDeclared();
    void uiContributionsAreDeclared();

private:
    static QJsonObject loadManifest() {
        QFile f(QString::fromUtf8(RHYTHM_MANIFEST_PATH));
        if (!f.open(QIODevice::ReadOnly)) {
            qFatal("Cannot open manifest at " RHYTHM_MANIFEST_PATH);
        }
        return QJsonDocument::fromJson(f.readAll()).object();
    }
    static QSet<QString> toStringSet(const QJsonArray& arr) {
        QSet<QString> out;
        for (const QJsonValue& v : arr) out.insert(v.toString());
        return out;
    }
};

void TestRhythmManifestEvents::allCodePublishedTopicsAreDeclared() {
    const QJsonObject m = loadManifest();
    const QSet<QString> declared = toStringSet(
        m.value("events").toObject().value("publish").toArray());
    fprintf(stderr, "[publish] declared: %s\n",
            qPrintable(QStringList(declared.begin(), declared.end()).join(", ")));

    // Topics publish()'d by RhythmPlugin::publishEvent(). Update this set
    // when adding a new publish call site — and add the topic to manifest
    // events.publish in the same PR.
    const QSet<QString> codePublished = {
        QStringLiteral("margin.rhythm.break_due"),
        QStringLiteral("margin.rhythm.break_started"),
        QStringLiteral("margin.rhythm.break_dismissed"),
    };
    for (const QString& t : codePublished) {
        QVERIFY2(declared.contains(t),
                 qPrintable(QStringLiteral("Topic %1 publish()'d in code but "
                                           "missing from manifest.json events.publish").arg(t)));
    }
}

void TestRhythmManifestEvents::allCodeSubscribedTopicsAreDeclared() {
    const QJsonObject m = loadManifest();
    const QSet<QString> declared = toStringSet(
        m.value("events").toObject().value("subscribe").toArray());
    fprintf(stderr, "[subscribe] declared: %s\n",
            qPrintable(QStringList(declared.begin(), declared.end()).join(", ")));

    // Topics subscribed in RhythmPlugin::onLoad. C5 added both.
    const QSet<QString> codeSubscribed = {
        QStringLiteral("margin.aura.away"),
        QStringLiteral("margin.aura.back"),
    };
    for (const QString& t : codeSubscribed) {
        QVERIFY2(declared.contains(t),
                 qPrintable(QStringLiteral("Topic %1 subscribe()'d in code but "
                                           "missing from manifest.json events.subscribe").arg(t)));
    }
}

void TestRhythmManifestEvents::permissionsAreDeclared() {
    const QJsonObject m = loadManifest();
    const QSet<QString> perms = toStringSet(m.value("permissions").toArray());
    fprintf(stderr, "[permissions] declared: %s\n",
            qPrintable(QStringList(perms.begin(), perms.end()).join(", ")));

    // input-monitor: idle pause (C2)
    // notification-show: toast UI (C3)
    // M4-C13a: overlay-fullscreen removed — standalone Qt.Tool window
    // replaces the OverlayContributor path.
    QVERIFY(perms.contains(QStringLiteral("input-monitor")));
    QVERIFY(perms.contains(QStringLiteral("notification-show")));
    QVERIFY(!perms.contains(QStringLiteral("overlay-fullscreen")));
}

void TestRhythmManifestEvents::uiContributionsAreDeclared() {
    const QJsonObject m = loadManifest();
    const QSet<QString> contribs = toStringSet(m.value("ui_contributions").toArray());
    fprintf(stderr, "[ui_contributions] declared: %s\n",
            qPrintable(QStringList(contribs.begin(), contribs.end()).join(", ")));

    QVERIFY(contribs.contains(QStringLiteral("TrayMenuContributor")));
    QVERIFY(contribs.contains(QStringLiteral("DashboardTabContributor")));
    // M4-C13a: OverlayContributor removed — break overlay is now a
    // standalone Window owned by RhythmPlugin, not a host-polled overlay.
    QVERIFY(!contribs.contains(QStringLiteral("OverlayContributor")));
}

QTEST_MAIN(TestRhythmManifestEvents)
#include "test_rhythm_manifest_events.moc"
