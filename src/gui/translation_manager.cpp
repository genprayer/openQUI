#include "translation_manager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <stdexcept>

namespace open_icue {
namespace {
constexpr qint64 MAX_UI_SETTINGS_BYTES = 4096;
constexpr auto SETTINGS_FILENAME = "ui-settings.json";
const QStringList SUPPORTED_LANGUAGES{"de", "en", "ru"};
QString translateMessagePart(const QString& source) {
    static const QRegularExpression rejectedCommand(
        "^HID command (0x[0-9a-fA-F]+) rejected: status (0x[0-9a-fA-F]+)$");
    const auto match = rejectedCommand.match(source);
    if (match.hasMatch())
        return QCoreApplication::translate("OpenICUE", "HID command %1 rejected: status %2")
            .arg(match.captured(1), match.captured(2));
    return QCoreApplication::translate("OpenICUE", source.toUtf8().constData());
}
QString systemLanguage() {
    switch (QLocale::system().language()) {
    case QLocale::German:
        return "de";
    case QLocale::Russian:
        return "ru";
    default:
        return "en";
    }
}
} // namespace

QString translateOperationMessage(const QString& source) {
    const auto translated = translateMessagePart(source);
    if (translated != source)
        return translated;
    const QString rollbackPrefix = "DPI rollback failed: ";
    const QString restorationPrefix = "mode restoration failed: ";
    auto parts = source.split("; ");
    for (auto& part : parts) {
        if (part.startsWith(rollbackPrefix))
            part = QCoreApplication::translate("OpenICUE", "DPI rollback failed: %1")
                       .arg(translateMessagePart(part.mid(rollbackPrefix.size())));
        else if (part.startsWith(restorationPrefix))
            part = QCoreApplication::translate("OpenICUE", "mode restoration failed: %1")
                       .arg(translateMessagePart(part.mid(restorationPrefix.size())));
        else
            part = translateMessagePart(part);
    }
    return parts.join("; ");
}

TranslationManager::TranslationManager(const QString& stateDirectory,
                                       const QString& requestedLanguage, QObject* parent)
    : QObject(parent), settingsPath_(QDir(stateDirectory).filePath(SETTINGS_FILENAME)) {
    const auto saved = readLanguage();
    const auto startupError = error_;
    const auto code = requestedLanguage.isEmpty() ? (saved.isEmpty() ? systemLanguage() : saved)
                                                  : requestedLanguage;
    if (!applyLanguage(code, false))
        throw std::runtime_error(error_.toStdString());
    error_ = startupError;
}

TranslationManager::~TranslationManager() {
    if (translator_)
        QCoreApplication::removeTranslator(translator_.get());
}

QVariantList TranslationManager::languages() const {
    return {QVariantMap{{"code", "de"}, {"name", "Deutsch"}},
            QVariantMap{{"code", "en"}, {"name", "English"}},
            QVariantMap{{"code", "ru"}, {"name", QString::fromUtf8("Русский")}}};
}

QString TranslationManager::error() const {
    return QCoreApplication::translate("OpenICUE", error_.toUtf8().constData());
}

QString TranslationManager::readLanguage() {
    QFile file(settingsPath_);
    if (!file.exists())
        return {};
    if (file.size() > MAX_UI_SETTINGS_BYTES || !file.open(QIODevice::ReadOnly)) {
        error_ = "Cannot read UI language settings";
        return {};
    }
    QJsonParseError parseError;
    const auto settings = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto code = settings.object().value("language").toString();
    if (parseError.error != QJsonParseError::NoError || !settings.isObject() ||
        settings.object().value("schemaVersion").toInt() != 1 ||
        !SUPPORTED_LANGUAGES.contains(code)) {
        error_ = "UI language settings are invalid; the original file was preserved";
        return {};
    }
    return code;
}

bool TranslationManager::applyLanguage(const QString& code, bool shouldPersist) {
    error_.clear();
    const auto reject = [this](const QString& message) {
        error_ = message;
        emit errorChanged();
        return false;
    };
    if (!SUPPORTED_LANGUAGES.contains(code))
        return reject("Choose German, English or Russian");
    auto candidate = std::make_unique<QTranslator>();
    if (!candidate->load(":/i18n/open-icue_" + code + ".qm"))
        return reject("The requested translation is unavailable");
    if (shouldPersist) {
        QSaveFile file(settingsPath_);
        const auto bytes =
            QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"language", code}}).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return reject("Cannot save UI language settings");
    }
    if (translator_)
        QCoreApplication::removeTranslator(translator_.get());
    translator_ = std::move(candidate);
    QCoreApplication::installTranslator(translator_.get());
    languageCode_ = code;
    emit languageChanged();
    emit errorChanged();
    return true;
}

bool TranslationManager::setLanguage(const QString& code) {
    return applyLanguage(code, true);
}
} // namespace open_icue
