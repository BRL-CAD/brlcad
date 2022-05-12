/*                         Q E L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file QEll.cpp
 *
 */

#include "common.h"
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QGroupBox>
#include "../../../app.h"
#include "QEll.h"

QEll::QEll()
    : QWidget()
{
    // TODO - in an ideal world the "default" values would be set
    // and updated in response to view changes (if widget is continually
    // visible) or when it becomes visible...
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0, 0, 0);
    VSET(ell.a, 100, 0, 0);
    VSET(ell.b, 0, 200, 0);
    VSET(ell.c, 0, 0, 300);


    QVBoxLayout *l = new QVBoxLayout;

    QLabel *ell_name_label = new QLabel("Object name:");
    l->addWidget(ell_name_label);
    ell_name = new QLineEdit();
    l->addWidget(ell_name);

    QGroupBox *abox = new QGroupBox("Elements");
    QVBoxLayout *abl = new QVBoxLayout;
    abl->setAlignment(Qt::AlignTop);
    O_pnt = new QCheckBox("O:");
    abl->addWidget(O_pnt);
    A_axis = new QCheckBox("A:");
    abl->addWidget(A_axis);
    B_axis = new QCheckBox("B:");
    abl->addWidget(B_axis);
    C_axis = new QCheckBox("C:");
    abl->addWidget(C_axis);
    abox->setLayout(abl);
    l->addWidget(abox);

    QGroupBox *ac_box = new QGroupBox("Actions");
    QVBoxLayout *acl = new QVBoxLayout;
    write_edit = new QPushButton("Apply");
    acl->addWidget(write_edit);
    make_sph = new QPushButton("Make sph");
    acl->addWidget(make_sph);
    reset_values = new QPushButton("Reset");
    acl->addWidget(reset_values);
    ac_box->setLayout(acl);
    l->addWidget(ac_box);


    l->setAlignment(Qt::AlignTop);
    this->setLayout(l);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);


    QObject::connect(ell_name, &QLineEdit::textEdited, this, &QEll::update_viewobj_name);
}

QEll::~QEll()
{
    if (p)
	bv_obj_put(p);
    bu_vls_free(&oname);
}

void
QEll::read_from_db()
{
    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return;
    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;
    struct db_i *dbip = gedp->dbip;
    if (!dbip)
	return;

    // Both of the conditions we are checking for here are normal - the former
    // indicates we are creating a new object, and the latter indicates we are
    // going to replace an existing object of another type with an ell.
    //
    // TODO - in the latter case we probably want to indicate visually in some
    // fashion that an application of the write would be more than an update to
    // existing object parameters - maybe by changing the color and/or label of
    // the write button.
    if (!dp || dp->d_minor_type != DB5_MINORTYPE_BRLCAD_ELL)
	return;

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0)
	return;
    struct rt_ell_internal *ellp = (struct rt_ell_internal *)intern.idb_ptr;
    RT_ELL_CK_MAGIC(ellp);
    VMOVE(ell.v, ellp->v);
    VMOVE(ell.a, ellp->a);
    VMOVE(ell.b, ellp->b);
    VMOVE(ell.c, ellp->c);
    rt_db_free_internal(&intern);

    // We have pulled new data from disk - let the wireframe know
    update_obj_wireframe();
}

void
QEll::write_to_db()
{
    if (!bu_vls_strlen(&oname))
	return;

    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return;
    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;
    struct db_i *dbip = gedp->dbip;
    if (!dbip)
	return;

    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_ptr = &ell;
    intern.idb_meth = &OBJ[intern.idb_type];

    dp = db_lookup(dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);

    if (dp == RT_DIR_NULL)
	dp = db_diradd(dbip, bu_vls_cstr(&oname), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);

    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	return;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	return;
    }

    rt_db_free_internal(&intern);

    emit db_updated();
}

void
QEll::update_obj_wireframe()
{
    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return;
    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;
    struct bview *v = gedp->ged_gvp;
    if (!v)
	return;

    // Make the object, if we've not already done so
    if (!p)
	p = bv_obj_get(v, BV_VIEW_OBJS);

    // Clear any old wireframes, labels, etc.
    bv_obj_reset(p);

    // Use whatever view is current to drive the update
    p->s_v = v;

    // Set up the rt_db_internal and trigger the plotting routine with the
    // current ell parameters
    struct rt_db_internal intern = RT_DB_INTERNAL_INIT_ZERO;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_ptr = &ell;
    intern.idb_meth = &OBJ[intern.idb_type];
    if (!intern.idb_meth->ft_plot)
	return;
    struct bn_tol *tol = &gedp->ged_wdbp->wdb_tol;
    struct bg_tess_tol *ttol = &gedp->ged_wdbp->wdb_ttol;
    intern.idb_meth->ft_plot(&p->s_vlist, &intern, ttol, tol, p->s_v);

    // At least for now, mimic the MGED behavior and make editing wireframes white
    const char *wcolor = "255/255/255";
    const char *av[2] = {wcolor, NULL};
    struct bu_color cval;
    bu_opt_color(NULL, 1, (const char **)&av[0], (void *)&cval);
    bu_color_to_rgb_chars(&cval, p->s_color);

    // When editing, we show the labels
    if (intern.idb_meth->ft_labels)
	intern.idb_meth->ft_labels(p, &intern, p->s_v);

    // TODO - we should be able to set UP or DOWN on the various labels
    // when their respective controls are enabled/disabled...

    emit view_updated(&v);
}

void
QEll::update_viewobj_name(const QString &)
{
    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return;
    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp)
	return;
    struct bview *v = gedp->ged_gvp;
    if (!v)
	return;

    // Make the view object, if we've not already done so
    if (!p)
	p = bv_obj_get(v, BV_VIEW_OBJS);

    // Make sure the view object names match whatever the dialog says
    // is the current (proposed) name for the written object
    bu_vls_trunc(&oname, 0);
    if (ell_name->placeholderText().length())
	bu_vls_sprintf(&oname, "%s", ell_name->placeholderText().toLocal8Bit().data());
    if (ell_name->text().length())
	bu_vls_sprintf(&oname, "%s", ell_name->text().toLocal8Bit().data());
    if (!bu_vls_strlen(&oname))
	return;
    bu_vls_sprintf(&p->s_name, "%s:%s", bu_vls_cstr(&v->gv_name), bu_vls_cstr(&oname));
    bu_vls_sprintf(&p->s_uuid, "%s:%s", bu_vls_cstr(&v->gv_name), bu_vls_cstr(&oname));

    // Update the directory pointer to reflect the name.  If there is a change,
    // and that change points us to a new object, we need to read the info from
    // that object
    struct directory *ndp = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
    if (ndp != dp) {
	dp = ndp;
	if (dp)
	    read_from_db();
    }
}

bool
QEll::eventFilter(QObject *, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease ||   e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove) {
	bu_log("ell mouse event\n");
    }
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
