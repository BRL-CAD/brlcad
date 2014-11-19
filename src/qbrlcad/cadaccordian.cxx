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


bool EditStateFilter::eventFilter(QObject *target, QEvent *e)
{
    int interaction_mode = ((CADApp *)qApp)->interaction_mode;
    CADAccordian *accordian = ((CADApp *)qApp)->cadaccordian;
    if (e->type() == QEvent::MouseButtonPress) {
	QMouseEvent *me = (QMouseEvent *)e;
	QWidget *target_widget = static_cast<QWidget *>(target);
	QPoint mpos = target_widget->mapTo(accordian, me->pos());

	QPoint view_ctrls_topleft = accordian->view_ctrls->mapTo(accordian, accordian->view_ctrls->geometry().topLeft());
	QPoint view_ctrls_bottomright = accordian->view_ctrls->mapTo(accordian, accordian->view_ctrls->geometry().bottomRight());
	QRect view_ctrls_rect(view_ctrls_topleft, view_ctrls_bottomright);

	QPoint instance_ctrls_topleft = accordian->instance_ctrls->mapTo(accordian, accordian->instance_ctrls->geometry().topLeft());
	QPoint instance_ctrls_bottomright = accordian->instance_ctrls->mapTo(accordian, accordian->instance_ctrls->geometry().bottomRight());
	QRect instance_ctrls_rect(instance_ctrls_topleft, instance_ctrls_bottomright);

	QPoint primitive_ctrls_topleft = accordian->primitive_ctrls->mapTo(accordian, accordian->primitive_ctrls->geometry().topLeft());
	QPoint primitive_ctrls_bottomright = accordian->primitive_ctrls->mapTo(accordian, accordian->primitive_ctrls->geometry().bottomRight());
	QRect primitive_ctrls_rect(primitive_ctrls_topleft, primitive_ctrls_bottomright);

	QPoint stdpropview_topleft = accordian->stdpropview->mapTo(accordian, accordian->stdpropview->geometry().topLeft());
	QPoint stdpropview_bottomright = accordian->stdpropview->mapTo(accordian, accordian->stdpropview->geometry().bottomRight());
	QRect stdpropview_rect(stdpropview_topleft, stdpropview_bottomright);

	QPoint userpropview_topleft = accordian->userpropview->mapTo(accordian, accordian->userpropview->geometry().topLeft());
	QPoint userpropview_bottomright = accordian->userpropview->mapTo(accordian, accordian->userpropview->geometry().bottomRight());
	QRect userpropview_rect(userpropview_topleft, userpropview_bottomright);

	if (view_ctrls_rect.contains(mpos)) {
	    ((CADApp *)qApp)->interaction_mode = 0;
	}
	if (instance_ctrls_rect.contains(mpos)) {
	    ((CADApp *)qApp)->interaction_mode = 1;
	}

	if (primitive_ctrls_rect.contains(mpos)) {
	    ((CADApp *)qApp)->interaction_mode = 2;
	}

	if (stdpropview_rect.contains(mpos)) {
	    ((CADApp *)qApp)->interaction_mode = 2;
	}

	if (userpropview_rect.contains(mpos)) {
	    ((CADApp *)qApp)->interaction_mode = 2;
	}

	CADTreeView *tview = (CADTreeView *)(((CADApp *)qApp)->cadtreeview);
	CADTreeModel *tmodel = (CADTreeModel *)(tview->model());

	if (interaction_mode != ((CADApp *)qApp)->interaction_mode) {
	    tmodel->update_selected_node_relationships(tview->selected());
	}
    }
    return QObject::eventFilter(target, e);
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

    QList<QWidget*> list = this->findChildren<QWidget *>();
    EditStateFilter *efilter = new EditStateFilter();
    foreach(QWidget *w, list) {
	w->installEventFilter(efilter);
    }

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

