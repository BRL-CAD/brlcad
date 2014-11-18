#include "cadaccordian.h"
#include "cadapp.h"

CADViewControls::CADViewControls(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    info_view = new QWidget(this);
    info_view->setMinimumHeight(100);
    for(int i = 1; i < 8; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(info_view);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
}

CADViewControls::~CADViewControls()
{
    delete tpalette;
    delete info_view;
}

CADInstanceEdit::CADInstanceEdit(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    info_view = new QWidget(this);
    info_view->setMinimumHeight(100);
    for(int i = 1; i < 4; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(info_view);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
}

CADInstanceEdit::~CADInstanceEdit()
{
    delete tpalette;
    delete info_view;
}


CADPrimitiveEdit::CADPrimitiveEdit(QWidget *pparent)
    : QWidget(pparent)
{
    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->setSpacing(0);
    mlayout->setContentsMargins(0,0,0,0);
    tpalette = new QToolPalette(this);
    shape_properties = new QWidget(this);
    shape_properties->setMinimumHeight(100);
    for(int i = 1; i < 15; i++) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("tool controls ");
	obj_label.append(QString::number(i));
	QPushButton *obj_control = new QPushButton(obj_label);
	QToolPaletteElement *el = new QToolPaletteElement(0, obj_icon, obj_control);
	tpalette->addElement(el);
    }
    mlayout->addWidget(tpalette);
    mlayout->addWidget(shape_properties);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(mlayout);
}

CADPrimitiveEdit::~CADPrimitiveEdit()
{
    delete tpalette;
    delete shape_properties;
}

CADAccordian::CADAccordian(QWidget *pparent)
    : QAccordianWidget(pparent)
{
    view_ctrls = new CADViewControls(this);
    instance_ctrls = new CADInstanceEdit(this);
    primitive_ctrls = new CADPrimitiveEdit(this);

    stdpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 1, 0);
    stdpropview = new CADAttributesView(this, 1);
    stdpropview->setModel(stdpropmodel);

    userpropmodel = new CADAttributesModel(0, DBI_NULL, RT_DIR_NULL, 0, 1);
    userpropview = new CADAttributesView(this, 0);
    userpropview->setModel(userpropmodel);

    view_obj = new QAccordianObject(this, view_ctrls, "View Controls");
    this->addObject(view_obj);
    instance_obj = new QAccordianObject(this, instance_ctrls, "Instance Editing");
    this->addObject(instance_obj);
    primitive_obj = new QAccordianObject(this, primitive_ctrls, "Primitive Editing");
    this->addObject(primitive_obj);
    stdprop_obj = new QAccordianObject(this, stdpropview, "Standard Attributes");
    this->addObject(stdprop_obj);
    userprop_obj = new QAccordianObject(this, userpropview, "User Attributes");
    this->addObject(userprop_obj);
}

// TODO - this is wrong.  Need to use an event filter for this - child widgets
// get events first and by the time we get here we don't see anything
void CADAccordian::mousePressEvent(QMouseEvent *e)
{
    if (view_ctrls->geometry().contains(e->pos())) {
	((CADApp *)qApp)->interaction_mode = 0;
    }

    if (instance_ctrls->geometry().contains(e->pos())) {
	((CADApp *)qApp)->interaction_mode = 1;
    }

    if (primitive_ctrls->geometry().contains(e->pos())) {
	((CADApp *)qApp)->interaction_mode = 2;
    }

    if (stdpropview->geometry().contains(e->pos())) {
	((CADApp *)qApp)->interaction_mode = 2;
    }

    if (userpropview->geometry().contains(e->pos())) {
	((CADApp *)qApp)->interaction_mode = 2;
    }

    CADTreeView *tview = (CADTreeView *)(((CADApp *)qApp)->cadtreeview);
    CADTreeModel *tmodel = (CADTreeModel *)(tview->model());

    tmodel->update_selected_node_relationships(tview->selected());

}

CADAccordian::~CADAccordian()
{
    delete view_ctrls;
    delete instance_ctrls;
    delete primitive_ctrls;
    delete stdpropmodel;
    delete stdpropview;
    delete userpropmodel;
    delete userpropview;
}


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

