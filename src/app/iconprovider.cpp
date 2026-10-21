#include "iconprovider.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSvgRenderer>

static QJsonObject iconTable()
{
    static const QJsonObject table = [] {
        QFile f(QStringLiteral(":/icons.json"));
        f.open(QIODevice::ReadOnly);
        return QJsonDocument::fromJson(f.readAll()).object();
    }();
    return table;
}

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int q = id.indexOf(QLatin1Char('?'));
    const QString name = q < 0 ? id : id.left(q);
    const QString color = q < 0 ? QStringLiteral("#000000") : id.mid(q + 1);

    // The logo is a full-colour SVG; everything else is a stroke icon from icons.json.
    QByteArray svg;
    if (name == QLatin1String("logo")) {
        QFile f(QStringLiteral(":/theme/orbital.svg"));
        if (f.open(QIODevice::ReadOnly))
            svg = f.readAll();
    } else {
        svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
              "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
            + iconTable().value(name).toString().toUtf8() + "</svg>";
        svg.replace("currentColor", color.toUtf8());
    }

    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    if (renderer.isValid()) {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
    }
    if (size)
        *size = target;
    return image;
}
