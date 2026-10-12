#pragma once
#include "devices/spatial_effects.h"
#include <optional>

#include <QElapsedTimer>
#include <QImage>
#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QRegion>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace open_icue {

/** Interactive device contours with click, additive selection and rectangle selection. */
class DeviceSelection : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries WRITE setEntries NOTIFY entriesChanged)
    Q_PROPERTY(QSizeF canvasSize READ canvasSize WRITE setCanvasSize NOTIFY geometryChanged)
    Q_PROPERTY(int viewIndex READ viewIndex WRITE setViewIndex NOTIFY geometryChanged)
    Q_PROPERTY(
        QStringList selectedIds READ selectedIds WRITE setSelectedIds NOTIFY selectionChanged)
    Q_PROPERTY(
        bool isMultiSelection READ isMultiSelection WRITE setIsMultiSelection NOTIFY modeChanged)
    Q_PROPERTY(
        QVariantList previewLayers READ previewLayers WRITE setPreviewLayers NOTIFY previewChanged)
    Q_PROPERTY(QVariantMap frameColors READ frameColors WRITE setFrameColors NOTIFY previewChanged)
    Q_PROPERTY(QString hoveredLabel READ hoveredLabel NOTIFY hoverChanged)
    Q_PROPERTY(bool isDragging READ isDragging NOTIFY gestureChanged)
    Q_PROPERTY(QRectF selectionRect READ selectionRect NOTIFY gestureChanged)
  public:
    explicit DeviceSelection(QQuickItem* parent = nullptr);
    [[nodiscard]] QVariantList entries() const {
        return entries_;
    }
    [[nodiscard]] QSizeF canvasSize() const {
        return canvasSize_;
    }
    [[nodiscard]] int viewIndex() const {
        return viewIndex_;
    }
    [[nodiscard]] QStringList selectedIds() const {
        return selectedIds_;
    }
    [[nodiscard]] bool isMultiSelection() const {
        return isMultiSelection_;
    }
    [[nodiscard]] QVariantList previewLayers() const {
        return previewLayers_;
    }
    [[nodiscard]] QVariantMap frameColors() const {
        return frameColors_;
    }
    [[nodiscard]] QString hoveredLabel() const {
        return hoveredLabel_;
    }
    [[nodiscard]] bool isDragging() const {
        return isDragging_;
    }
    [[nodiscard]] QRectF selectionRect() const {
        return selectionRect_;
    }
    void setEntries(const QVariantList& entries);
    void setCanvasSize(QSizeF size);
    void setViewIndex(int index);
    void setSelectedIds(const QStringList& ids);
    void setIsMultiSelection(bool value);
    void setPreviewLayers(const QVariantList& layers);
    void setFrameColors(const QVariantMap& colors);
    /** Geometry readback in item coordinates, without changing selection. */
    Q_INVOKABLE QRectF keyRect(const QString& id) const;
    Q_INVOKABLE QPointF keyCenter(const QString& id) const;
    /** Local preview color; never reports a hardware LED value. */
    Q_INVOKABLE QColor previewColor(const QString& id) const;
    void paint(QPainter* painter) override;
  signals:
    void entriesChanged();
    void geometryChanged();
    void selectionChanged();
    void modeChanged();
    void previewChanged();
    void hoverChanged();
    void gestureChanged();
    void selectionRequested(const QStringList& ids);

  protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;

  private:
    struct Contour {
        QString id, label;
        QPainterPath path;
        QImage mask;
        QRect maskBounds;
        QRegion alphaRegion;
    };
    void rebuildContours();
    [[nodiscard]] QPointF toCanvas(QPointF point) const;
    [[nodiscard]] QString hit(QPointF point) const;
    [[nodiscard]] QStringList rectangleIds() const;
    [[nodiscard]] QColor colorFor(const Contour& contour) const;
    QVariantList entries_, previewLayers_;
    std::optional<SpatialEffects> spatialEffects_;
    QVariantMap frameColors_;
    QList<Contour> contours_;
    QSizeF canvasSize_{2956, 1120};
    QStringList selectedIds_, pressSelection_;
    QString hoveredLabel_;
    QPointF pressPoint_;
    QRectF selectionRect_;
    int viewIndex_ = 0;
    bool isMultiSelection_ = false, isPressed_ = false, isDragging_ = false, isAdditive_ = false;
    QTimer previewTimer_;
    QElapsedTimer previewClock_;
};

} // namespace open_icue
