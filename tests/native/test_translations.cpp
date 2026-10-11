/** Verify compiled language coverage, preference persistence and profile isolation. */
#include "gui/application_model.h"
#include "gui/translation_manager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class NativeTranslationTest : public QObject {
    Q_OBJECT
  private slots:
    void completeCompiledCatalog_data() {
        QTest::addColumn<QString>("language");
        QTest::newRow("de") << QString("de");
        QTest::newRow("en") << QString("en");
        QTest::newRow("ru") << QString("ru");
    }
    void completeCompiledCatalog() {
        QFETCH(QString, language);
        QTemporaryDir state;
        QVERIFY(state.isValid());
        open_icue::TranslationManager translations(state.path(), language);
        QFile catalog(QString::fromUtf8(OPENQUI_SOURCE_DIR) + "/translations/catalog.json");
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const auto messages =
            QJsonDocument::fromJson(catalog.readAll()).object().value("messages").toObject();
        QVERIFY(messages.size() >= 300);
        for (auto it = messages.constBegin(); it != messages.constEnd(); ++it) {
            const auto translated =
                QCoreApplication::translate("OpenICUE", it.key().toUtf8().constData());
            QCOMPARE(translated, it.value().toObject().value(language).toString());
        }
    }
    void languagePreferenceSurvivesRestartAndRejectsUnsupportedChoice() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        open_icue::ApplicationModel model(state.path());
        const auto original = model.document();
        {
            open_icue::TranslationManager translations(state.path(), "en");
            QVERIFY(translations.setLanguage("ru"));
            QCOMPARE(translations.languageCode(), QString("ru"));
            QFile saved(state.path() + "/ui-settings.json");
            QVERIFY(saved.open(QIODevice::ReadOnly));
            const auto bytes = saved.readAll();
            QVERIFY(!translations.setLanguage("fr"));
            QCOMPARE(translations.languageCode(), QString("ru"));
            QVERIFY(translations.error().contains(QString::fromUtf8("Выберите")));
            saved.seek(0);
            QCOMPARE(saved.readAll(), bytes);
            QCOMPARE(model.document(), original);
        }
        open_icue::TranslationManager restarted(state.path());
        QCOMPARE(restarted.languageCode(), QString("ru"));
        QCOMPARE(model.document(), original);
    }
    void operationRecoveryMessagesFollowLanguageAndPreserveProtocolCodes() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        open_icue::TranslationManager translations(state.path(), "ru");
        const QString source = "Firmware DPI readback does not match the requested value; "
                               "DPI rollback failed: HID command 0x01 rejected: status 0x05; "
                               "mode restoration failed: Firmware mode restoration failed";
        const auto russian = open_icue::translateOperationMessage(source);
        QVERIFY(russian.contains(QString::fromUtf8("Считанный DPI")));
        QVERIFY(russian.contains(QString::fromUtf8("Не удалось восстановить предыдущий DPI")));
        QVERIFY(russian.contains(QString::fromUtf8("Команда HID 0x01 отклонена: статус 0x05")));
        QVERIFY(!russian.contains("failed"));
        QVERIFY(translations.setLanguage("de"));
        const auto german = open_icue::translateOperationMessage(source);
        QVERIFY(german.contains("DPI-Rollback"));
        QVERIFY(german.contains("0x01"));
        QVERIFY(german.contains("0x05"));
        QVERIFY(german != russian);
        QVERIFY(translations.setLanguage("en"));
        QCOMPARE(open_icue::translateOperationMessage(source), source);
    }
    void nativeValidationNoticeIsTranslatedAfterLanguageSwitch() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        open_icue::ApplicationModel model(state.path());
        open_icue::TranslationManager translations(state.path(), "ru");
        model.selectDevice("k70_core_rgb");
        const auto original = model.document();
        const QVariantMap layer{
            {"type", "GradientLightingTypeData.lightings.cue.corsair.com"},
            {"zones", QStringList{"A"}},
            {"colors", QVariantList{"#ff0000", "#00ff00"}},
            {"duration", 2.0},
            {"isLooping", true},
            {"transitions", QVariantList{QVariantMap{{"time", 0.25}, {"color", "#ff0000"}},
                                         QVariantMap{{"time", 1.0}, {"color", "#00ff00"}}}}};
        model.setDocumentValue("lightingLayers", QVariantList{layer});
        QVERIFY(!model.lastOperation().value("isOk").toBool());
        QCOMPARE(model.document(), original);
        QVERIFY(model.notice().contains(QString::fromUtf8("Переходы подсветки")));
        QVERIFY(translations.setLanguage("en"));
        QCOMPARE(model.notice(), QString("Lighting transitions must span the complete timeline"));
    }
    void corruptPreferenceIsPreservedAndUsesExplicitFallback() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        QFile settings(state.path() + "/ui-settings.json");
        QVERIFY(settings.open(QIODevice::WriteOnly));
        const QByteArray original("{\"language\":");
        QCOMPARE(settings.write(original), original.size());
        settings.close();
        open_icue::TranslationManager translations(state.path(), "en");
        QCOMPARE(translations.languageCode(), QString("en"));
        QVERIFY(translations.error().contains("invalid"));
        QVERIFY(settings.open(QIODevice::ReadOnly));
        QCOMPARE(settings.readAll(), original);
    }
    void failedPreferenceSaveDoesNotChangeLanguage() {
        QTemporaryDir state;
        QVERIFY(state.isValid());
        open_icue::TranslationManager translations(state.path(), "de");
        QVERIFY(QDir().mkpath(state.path() + "/ui-settings.json"));
        QVERIFY(!translations.setLanguage("en"));
        QCOMPARE(translations.languageCode(), QString("de"));
        QVERIFY(!translations.error().isEmpty());
        QCOMPARE(QCoreApplication::translate("OpenICUE", "Sprache"), QString("Sprache"));
    }
};
QTEST_GUILESS_MAIN(NativeTranslationTest)
#include "test_translations.moc"
