#include "icon_images.h"
#include <QColor>
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

namespace open_icue {
namespace {
constexpr int DEFAULT_ICON_SIZE = 64;
constexpr int MAX_ICON_SIZE = 1024;
constexpr int MAX_SVG_DEPTH = 64;

void applyRoles(QDomElement element, const QMap<QString, QColor>& palette, int depth = 0) {
    if (depth > MAX_SVG_DEPTH)
        return;
    for (const auto& attribute : {QString("fill"), QString("stroke"), QString("stop")}) {
        const auto role = element.attribute("data-" + attribute + "-role");
        if (palette.contains(role))
            element.setAttribute(attribute == "stop" ? "stop-color" : attribute,
                                 palette.value(role).name());
    }
    for (auto child = element.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
        applyRoles(child, palette, depth + 1);
}
} // namespace

QImage IconImages::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const QUrl url("image://icons/" + id);
    const auto path = url.path().mid(1);
    static const QRegularExpression validPath("^(lighting|assignments|controls)/[a-z0-9-]{1,80}$");
    if (!validPath.match(path).hasMatch())
        return {};
    QFile file(":/app/icons/" + path + ".svg");
    if (!file.exists())
        file.setFileName(":/app/icons/placeholder.svg");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QDomDocument document;
    if (!document.setContent(file.readAll()) || document.documentElement().tagName() != "svg")
        return {};
    QMap<QString, QColor> palette{{"base", QColor("#DCE4EF")},
                                  {"accent", QColor("#67C5FF")},
                                  {"secondary", QColor("#DA8EFF")},
                                  {"highlight", QColor("#FFD66B")}};
    const QUrlQuery query(url);
    for (auto it = palette.begin(); it != palette.end(); ++it) {
        const QColor replacement(query.queryItemValue(it.key(), QUrl::FullyDecoded));
        if (replacement.isValid())
            it.value() = replacement;
    }
    applyRoles(document.documentElement(), palette);
    QSvgRenderer renderer(document.toByteArray());
    if (!renderer.isValid())
        return {};
    const QSize outputSize(
        requestedSize.width() > 0 ? std::min(requestedSize.width(), MAX_ICON_SIZE)
                                  : DEFAULT_ICON_SIZE,
        requestedSize.height() > 0 ? std::min(requestedSize.height(), MAX_ICON_SIZE)
                                   : DEFAULT_ICON_SIZE);
    QImage image(outputSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    if (size)
        *size = outputSize;
    return image;
}
} // namespace open_icue
