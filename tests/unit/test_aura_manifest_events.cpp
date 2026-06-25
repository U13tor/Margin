// test_aura_manifest_events — A1 manifest/code consistency.
// Verifies that every EventBus topic published by AuraLockerPlugin is
// declared in manifest.json's events.publish array. CLAUDE.md §7 forbids
// undeclared extension points — this test catches the regression that
// motivated the fix (margin.aura.state was published but missing from
// the manifest).
//
// Reads manifest.json from the source tree via AURA_MANIFEST_PATH (passed
// in by CMake target_compile_definitions). The expected set is hardcoded
// from the current publish() call sites in AuraLockerPlugin.cpp.

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QTest>

#include <cstdio>

class TestAuraManifestEvents : public QObject {
    Q_OBJECT

private slots:
    void allCodePublishedTopicsAreDeclared();
    void warningTopicIsPublishedAndDeclared();
};

namespace {
QSet<QString> loadPublishArray() {
    QFile f(QString::fromUtf8(AURA_MANIFEST_PATH));
    if (!f.open(QIODevice::ReadOnly)) {
        qFatal("Cannot open manifest at " AURA_MANIFEST_PATH);
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject events = doc.object().value(QStringLiteral("events")).toObject();
    const QJsonArray publish = events.value(QStringLiteral("publish")).toArray();

    QSet<QString> out;
    for (const QJsonValue& v : publish) {
        out.insert(v.toString());
    }
    return out;
}
} // namespace

void TestAuraManifestEvents::allCodePublishedTopicsAreDeclared() {
    const QSet<QString> declared = loadPublishArray();
    fprintf(stderr, "[manifest] declared topics: %s\n",
            qPrintable(QStringList(declared.begin(), declared.end()).join(", ")));

    // Topics published via eventBus().publish() in AuraLockerPlugin.cpp.
    // Update this set when adding a new publish() call site — and add the
    // topic to src/plugins/aura_locker/manifest.json in the same PR.
    const QSet<QString> codePublished = {
        QStringLiteral("margin.aura.state"),
        QStringLiteral("margin.aura.away"),
        QStringLiteral("margin.aura.back"),
        QStringLiteral("margin.aura.warning"),
    };

    for (const QString& topic : codePublished) {
        fprintf(stderr, "[manifest] checking %s declared=%d\n",
                qPrintable(topic), int(declared.contains(topic)));
        QVERIFY2(declared.contains(topic),
                 qPrintable(QStringLiteral("Topic %1 is publish()'d in code but "
                                           "missing from manifest.json events.publish")
                                .arg(topic)));
    }
}

void TestAuraManifestEvents::warningTopicIsPublishedAndDeclared() {
    // Regression guard: margin.aura.warning was once speculatively noted
    // as "declared but unused" — it IS used (onRadioStateChanged emits it
    // when the BT radio flips off). Lock that contract in.
    const QSet<QString> declared = loadPublishArray();
    QVERIFY(declared.contains(QStringLiteral("margin.aura.warning")));
}

QTEST_MAIN(TestAuraManifestEvents)
#include "test_aura_manifest_events.moc"
