#pragma once
#include "application_model.h"
#include <QMenu>
#include <QQuickWindow>
#include <QSystemTrayIcon>

namespace open_icue {
/** A native StatusNotifier tray icon with device-reported battery readings. */
class SystemTray final : public QObject {
    Q_OBJECT
  public:
    SystemTray(ApplicationModel* model, QQuickWindow* window, QObject* parent = nullptr);
    [[nodiscard]] bool isVisible() const {
        return icon_.isVisible();
    }

  private:
    void refresh();
    ApplicationModel* model_;
    QQuickWindow* window_;
    QSystemTrayIcon icon_;
    QMenu menu_;
    QAction* showAction_;
    QAction* quitAction_;
};
} // namespace open_icue
