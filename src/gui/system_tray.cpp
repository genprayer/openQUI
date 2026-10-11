#include "system_tray.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPixmap>

namespace open_icue {
SystemTray::SystemTray(ApplicationModel* model, QQuickWindow* window, QObject* parent)
    : QObject(parent), model_(model), window_(window) {
    showAction_ = menu_.addAction(QString());
    menu_.addSeparator();
    quitAction_ = menu_.addAction(QString());
    icon_.setContextMenu(&menu_);
    connect(showAction_, &QAction::triggered, this, [this] {
        window_->show();
        window_->raise();
        window_->requestActivate();
    });
    connect(quitAction_, &QAction::triggered, QCoreApplication::instance(),
            &QCoreApplication::quit);
    connect(&icon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    window_->show();
                    window_->raise();
                    window_->requestActivate();
                }
            });
    connect(model_, &ApplicationModel::stateChanged, this, &SystemTray::refresh);
    refresh();
    if (QSystemTrayIcon::isSystemTrayAvailable())
        icon_.show();
}
void SystemTray::refresh() {
    showAction_->setText(QCoreApplication::translate("OpenICUE", "Fenster öffnen"));
    quitAction_->setText(QCoreApplication::translate("OpenICUE", "Beenden"));
    QStringList lines{"OpenQUI"};
    int selectedLevel = -1;
    int lowestLevel = -1;
    for (const auto& value : model_->devices()) {
        const auto device = value.toMap();
        const auto state = device.value("telemetry").toMap();
        if (!state.value("hasBattery").toBool() || !state.value("isVerified").toBool())
            continue;
        const int level = state.value("batteryPercent").toInt();
        lowestLevel = lowestLevel < 0 ? level : std::min(lowestLevel, level);
        auto line = device.value("name").toString() + ": " + QString::number(level) + "%";
        if (state.value("isCharging").toBool())
            line += " · " + QCoreApplication::translate("OpenICUE", "Wird geladen");
        lines.append(line);
        if (device.value("id").toString() == model_->selectedModelId())
            selectedLevel = level;
    }
    if (selectedLevel < 0)
        selectedLevel = lowestLevel;
    icon_.setToolTip(lines.join('\n'));
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QIcon(":/app/branding/open-cue-gui.svg").paint(&painter, QRect(0, 0, 64, 64));
    if (selectedLevel >= 0) {
        painter.setPen(Qt::white);
        painter.setBrush(QColor("#202020"));
        painter.drawRoundedRect(QRect(12, 37, 50, 24), 5, 5);
        auto font = painter.font();
        font.setPixelSize(19);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRect(12, 37, 50, 24), Qt::AlignCenter, QString::number(selectedLevel));
    }
    painter.end();
    icon_.setIcon(QIcon(pixmap));
}
} // namespace open_icue
