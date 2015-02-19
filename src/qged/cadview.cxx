#include "cadview.h"
#include "cadapp.h"

CADView::CADView(QWidget *pparent, int type)
    : QWidget(pparent)
{
    Q_UNUSED(type);
    canvas = new QGLWidget(this);
}

CADView::~CADView()
{
    delete canvas;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

