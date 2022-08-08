/*                     W I D G E T . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file view_widget.h
 *
 */

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include "ged.h"

class CADViewSelecter : public QWidget
{
    Q_OBJECT

    public:
	CADViewSelecter(QWidget *p = 0);
	~CADViewSelecter();

	QRadioButton *use_pnt_select_button;
	QRadioButton *use_rect_select_button;

	QCheckBox *use_ray_test_ckbx;
	QCheckBox *select_all_depth_ckbx;

	QRadioButton *erase_from_scene_button;
	QRadioButton *add_to_group_button;
	QRadioButton *rm_from_group_button;
	//QComboBox *current_group;
	QListWidget *group_contents;
	//QPushButton *add_new_group;
	//QPushButton *rm_group;

	QPushButton *draw_selections;
	QPushButton *erase_selections;

	bool erase_obj_bbox();
	bool erase_obj_ray();

	bool add_obj_bbox();
	bool add_obj_ray();

	bool rm_obj_bbox();
	bool rm_obj_ray();

    signals:
	void view_changed(unsigned long long);

    public slots:
	void enable_groups(bool);
    	void disable_groups(bool);
	void enable_raytrace_opt(bool);
    	void disable_raytrace_opt(bool);
	void enable_useall_opt(bool);
    	void disable_useall_opt(bool);
	void do_view_update(unsigned long long);
	void do_draw_selections();
	void do_erase_selections();

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	bool enabled = true;
	fastf_t px = -FLT_MAX;
	fastf_t py = -FLT_MAX;
	fastf_t vx = -FLT_MAX;
	fastf_t vy = -FLT_MAX;
	int scnt = 0;
	struct bv_scene_obj **sset = NULL;
	unsigned long long ohash = 0;
	unsigned long long omhash = 0;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
