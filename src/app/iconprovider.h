#pragma once

#include <QQuickImageProvider>

// Serves the bundled monochrome SVG icons tinted to any colour:
//   image://icon/<name>?<color>      e.g. image://icon/search?#6b6a66
// Icons use stroke="currentColor", which is replaced before rendering, so
// they follow the QML theme instead of the desktop icon theme.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
