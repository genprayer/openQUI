#pragma once
#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <memory>

namespace open_icue {
/** Translate operation and recovery messages while preserving protocol codes and device IDs. */
[[nodiscard]] QString translateOperationMessage(const QString& source);
/** Own embedded Qt translations and an independent, atomic UI language preference. */
class TranslationManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString languageCode READ languageCode NOTIFY languageChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  public:
    explicit TranslationManager(const QString& stateDirectory,
                                const QString& requestedLanguage = {}, QObject* parent = nullptr);
    ~TranslationManager() override;
    [[nodiscard]] QString languageCode() const {
        return languageCode_;
    }
    [[nodiscard]] QVariantList languages() const;
    [[nodiscard]] QString error() const;
    /** Switch through the same committed preference path used by the visible language selector. */
    Q_INVOKABLE bool setLanguage(const QString& code);
  signals:
    void languageChanged();
    void errorChanged();

  private:
    [[nodiscard]] QString readLanguage();
    bool applyLanguage(const QString& code, bool shouldPersist);
    QString settingsPath_, languageCode_, error_;
    std::unique_ptr<QTranslator> translator_;
};
} // namespace open_icue
