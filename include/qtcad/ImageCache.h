#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include "qtcad/defines.h"
#include "bu/hash.h"

#include <QImage>
#include <QMap>

namespace qtcad {

class QTCAD_EXPORT ImageCache
{
  public:
    ImageCache();

    QImage getImage(const QString name);
    void setImage(const QString name, QImage image);
    void loadImage(QString name, QString filename);

  private:
    QMap<QString, QImage> images;
};

}
#endif // IMAGECACHE_H
