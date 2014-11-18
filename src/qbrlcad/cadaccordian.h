#ifndef CADACCORDIAN_H
#define CADACCORDIAN_H
#include "cadtreemodel.h"
#include "cadattributes.h"
#include "QToolPalette.h"
#include "QAccordianWidget.h"

class CADViewControls : public QWidget
{
    Q_OBJECT

    public:
	CADViewControls(QWidget *pparent = 0);
	~CADViewControls();

    private:
	QToolPalette *tpalette;
	QWidget *info_view;
};

class CADInstanceEdit : public QWidget
{
    Q_OBJECT

    public:
	CADInstanceEdit(QWidget *pparent = 0);
	~CADInstanceEdit();

    private:
	QToolPalette *tpalette;
	QWidget *info_view;
};

class CADPrimitiveEdit : public QWidget
{
    Q_OBJECT

    public:
	CADPrimitiveEdit(QWidget *pparent = 0);
	~CADPrimitiveEdit();

    private:
	QToolPalette *tpalette;
	QWidget *shape_properties;
};

class CADAccordian : public QAccordianWidget
{
    Q_OBJECT
    public:
	CADAccordian(QWidget *pparent);
	~CADAccordian();

	void mousePressEvent(QMouseEvent *e);
	CADViewControls *view_ctrls;
	QAccordianObject *view_obj;
	CADInstanceEdit *instance_ctrls;
	QAccordianObject *instance_obj;
	CADPrimitiveEdit *primitive_ctrls;
	QAccordianObject *primitive_obj;
	CADAttributesModel *stdpropmodel;
	CADAttributesView *stdpropview;
	QAccordianObject *stdprop_obj;
	CADAttributesModel *userpropmodel;
	CADAttributesView *userpropview;
	QAccordianObject *userprop_obj;
};

#endif /* CADACCORDIAN_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

