#ifndef CAD_VIEW_H
#define CAD_VIEW_H

#include <QImage>
#include <QObject>
#include <QGLWidget>

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADView : public QWidget
{
    Q_OBJECT

    public:
	CADView(QWidget *pparent = 0, int type = 0);
	~CADView();

    private:
	QGLWidget *canvas;
};

#endif /*CAD_VIEW_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

