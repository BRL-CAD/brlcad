/*                       Q S K E T C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file qsketch.cpp
 *
 * Standalone BRL-CAD sketch editor demo using libqtcad.
 *
 * Usage:
 *   qsketch  <file.g>  <sketch_name>
 *
 * Opens (or creates) a sketch primitive named <sketch_name> in the
 * given .g database file and presents an interactive editing UI built
 * with libqtcad widgets.  The editing backend uses the librt
 * ECMD_SKETCH_* API (rt/rt_ecmds.h) via the QgSketchFilter event
 * filters provided in libqtcad.
 *
 * UI layout
 * ---------
 *  Left panel  — toolbar with editing-mode buttons
 *  Centre      — QgView (software render) showing the sketch face-on
 *  Right panel — vertex table (multi-select) + segment table
 *  Bottom bar  — status line + live UV cursor display
 *
 * Editing modes
 * -------------
 *  Pick Vertex  (P) — left-click to select nearest vertex
 *  Move Vertex  (M) — left-drag to move selected vertex
 *  Add Vertex   (A) — left-click to add a vertex at cursor
 *  Pick Segment (S) — left-click to select nearest segment
 *  Move Segment (G) — left-drag to move selected segment
 *  Add Line     (L) — click two vertices then press Enter to create a
 *                      line segment between them
 *  Add Arc      (R) — start, end, radius dialog
 *  Add Bezier   (B) — click control points, press Enter to commit
 *  Flip Arc     (C) — toggle center_is_left on selected CARC (arc complement)
 *  Set R        (I) — drag to resize selected arc's radius interactively
 *
 * Multi-select
 * ------------
 *  Shift-click / Ctrl-click rows in the Vertices table, then press the
 *  "Move Selected…" button to shift all selected vertices by a UV delta
 *  (uses ECMD_SKETCH_MOVE_VERTEX_LIST).
 *
 * View
 * ----
 *  F / View → Fit to View — fit view scale to sketch bounds.
 *
 * Keyboard shortcuts:
 *  Ctrl+S  — save to .g file
 *  Ctrl+Z  — undo (single level)
 *  Escape  — cancel current operation / return to idle
 *  Delete  — delete selected vertex or segment
 *  F       — fit view to sketch bounds
 *  C       — flip arc complement (toggle center_is_left)
 *  I       — interactive arc radius drag mode
 */

#include "common.h"

#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/tol.h"
#include "bv.h"
#include "bv/util.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/primitives/sketch.h"
#include "rt/rt_ecmds.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>

#include "qtcad/QgSignalFlags.h"
#include "qtcad/QgSketchFilter.h"
#include "qtcad/QgSW.h"
#include "qtcad/QgView.h"

/* ------------------------------------------------------------------ */
/* Helpers: create a minimal in-plane sketch                          */
/* ------------------------------------------------------------------ */

/*
 * Create an empty sketch at the origin, with the u_vec along +X and
 * v_vec along +Y, so it lies in the XY plane.
 */
static struct directory *
sketch_create_empty(struct db_i *dbip, const char *name)
{
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    if (!wdbp)
	return NULL;

    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V,     0.0, 0.0, 0.0);
    VSET(skt->u_vec, 1.0, 0.0, 0.0);
    VSET(skt->v_vec, 0.0, 1.0, 0.0);
    skt->vert_count = 0;
    skt->verts      = NULL;
    skt->curve.count   = 0;
    skt->curve.segment = NULL;
    skt->curve.reverse = NULL;

    wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0);
    wdb_close(wdbp);

    return db_lookup(dbip, name, LOOKUP_QUIET);
}

/* ------------------------------------------------------------------ */
/* Wireframe refresh                                                   */
/* ------------------------------------------------------------------ */

/*
 * Draw the sketch wireframe directly via ft_plot into the swrast dm.
 * This is the custom draw callback registered with QgView_SW: it bypasses
 * the scene-object machinery and renders the sketch vlist directly,
 * which is both simpler and faster for a single-primitive editor.
 */
struct qsketch_draw_ctx {
    struct rt_edit *es;
    struct bv_grid_state *grid;
};

static void
sketch_draw_custom(struct bview *v, void *udata)
{
    struct qsketch_draw_ctx *ctx = (struct qsketch_draw_ctx *)udata;
    if (!ctx || !ctx->es) return;

    struct dm *dmp = (struct dm *)v->dmp;
    if (!dmp) return;

    /* The caller (QgSW::paintEvent) already called dm_draw_begin before
     * invoking us via dm_draw_objs; we just issue draw commands here. */

    /* ---- grid (optional) ---- */
    if (ctx->grid && ctx->grid->draw) {
	dm_draw_grid(dmp, ctx->grid, v->gv_scale, v->gv_model2view, 1.0);
    }

    /* ---- sketch wireframe ---- */
    const struct rt_sketch_internal *skt =
	(const struct rt_sketch_internal *)ctx->es->es_int.idb_ptr;
    if (skt && skt->vert_count > 0 && skt->curve.count > 0) {
	struct bu_list vlist;
	BU_LIST_INIT(&vlist);

	struct bg_tess_tol ttol;
	ttol.magic = BG_TESS_TOL_MAGIC;
	ttol.abs   = 0.0;
	ttol.rel   = 0.01;
	ttol.norm  = 0.0;

	struct bn_tol tol = BN_TOL_INIT_TOL;
	OBJ[ctx->es->es_int.idb_type].ft_plot(
	    &vlist, &ctx->es->es_int, &ttol, &tol, v);

	dm_set_fg(dmp, 255, 255, 0, 1, 1.0);  /* yellow wireframe */
	dm_draw_vlist(dmp, (struct bv_vlist *)&vlist);
	bv_vlist_cleanup(&vlist);
    }
}

/*
 * Write the current edit state back to the database and redisplay.
 *
 * This is called after every edit operation so the view shows the
 * latest geometry.  In a production qged integration the wireframe
 * would instead be refreshed from the in-memory es_int directly via
 * EDOBJ.ft_plot without touching the database; for this demo the
 * write-then-read round-trip keeps things simple.
 */
static int
sketch_write_to_db(struct rt_edit *es, struct db_i *dbip, struct directory *dp)
{
    if (!es || !dbip || !dp)
	return BRLCAD_ERROR;

    return rt_db_put_internal(dp, dbip, &es->es_int);
}

/* ------------------------------------------------------------------ */
/* Vertex / segment table helpers                                      */
/* ------------------------------------------------------------------ */

static void
populate_vertex_table(QTableWidget *tw, const struct rt_sketch_internal *skt)
{
    tw->setRowCount(0);
    if (!skt)
	return;

    tw->setRowCount((int)skt->vert_count);
    for (size_t i = 0; i < skt->vert_count; i++) {
	char buf[64];
	snprintf(buf, sizeof(buf), "%zu", i);
	tw->setItem((int)i, 0, new QTableWidgetItem(buf));
	snprintf(buf, sizeof(buf), "%.4g", skt->verts[i][0]);
	tw->setItem((int)i, 1, new QTableWidgetItem(buf));
	snprintf(buf, sizeof(buf), "%.4g", skt->verts[i][1]);
	tw->setItem((int)i, 2, new QTableWidgetItem(buf));
    }
}

static void
populate_segment_table(QTableWidget *tw, const struct rt_sketch_internal *skt)
{
    tw->setRowCount(0);
    if (!skt)
	return;

    tw->setRowCount((int)skt->curve.count);
    for (size_t i = 0; i < skt->curve.count; i++) {
	void *seg = skt->curve.segment[i];
	if (!seg) continue;

	char buf[256] = "";
	const char *type_str = "?";

	uint32_t magic = *(uint32_t *)seg;
	if (magic == CURVE_LSEG_MAGIC) {
	    struct line_seg *ls = (struct line_seg *)seg;
	    type_str = "Line";
	    snprintf(buf, sizeof(buf), "%d → %d", ls->start, ls->end);
	} else if (magic == CURVE_CARC_MAGIC) {
	    struct carc_seg *cs = (struct carc_seg *)seg;
	    if (cs->radius < 0.0) {
		type_str = "Circle";
		snprintf(buf, sizeof(buf), "centre=%d r=%.4g",
			 cs->end, -cs->radius);
	    } else {
		type_str = "Arc";
		snprintf(buf, sizeof(buf), "%d→%d r=%.4g",
			 cs->start, cs->end, cs->radius);
	    }
	} else if (magic == CURVE_BEZIER_MAGIC) {
	    struct bezier_seg *bs = (struct bezier_seg *)seg;
	    type_str = "Bezier";
	    std::string pts;
	    for (int k = 0; k <= bs->degree; k++) {
		if (k) pts += '-';
		pts += std::to_string(bs->ctl_points[k]);
	    }
	    snprintf(buf, sizeof(buf), "deg=%d [%s]", bs->degree, pts.c_str());
	} else if (magic == CURVE_NURB_MAGIC) {
	    struct nurb_seg *ns = (struct nurb_seg *)seg;
	    type_str = "NURB";
	    snprintf(buf, sizeof(buf), "order=%d c_size=%d",
		     ns->order, ns->c_size);
	}

	char idx[22];
	snprintf(idx, sizeof(idx), "%zu", i);
	tw->setItem((int)i, 0, new QTableWidgetItem(idx));
	tw->setItem((int)i, 1, new QTableWidgetItem(type_str));
	tw->setItem((int)i, 2, new QTableWidgetItem(buf));
    }
}

/* ------------------------------------------------------------------ */
/* QSketchEditWindow                                                   */
/* ------------------------------------------------------------------ */

/**
 * Main editing window for the qsketch demo.
 *
 * Owns a QgView and the rt_edit context, manages filter switching,
 * and keeps the vertex/segment tables in sync with the sketch data.
 */
class QSketchEditWindow : public QMainWindow
{
    Q_OBJECT

public:
    QSketchEditWindow(struct db_i *dbip,
		      struct directory *dp,
		      QWidget *parent = NULL);
    ~QSketchEditWindow();

    /* Render the sketch view and save a composite screenshot.
     * Grabs the Qt window chrome and overlays the swrast DM viewport render,
     * giving a picture that shows both the UI and the sketch wireframe. */
    void screenshot_to_file(const QString &filename) {
	if (!m_view) return;

	/* Step 1: Grab the full Qt window (UI chrome) */
	QPixmap winpix = grab();

	/* Step 2: Get the DM viewport image from swrast */
	QImage dm_img;
	m_view->get_viewport_image(dm_img);

	if (!dm_img.isNull()) {
	    /* Compute where the QgView viewport sits inside this window */
	    QPoint vp_offset = m_view->mapTo(this, QPoint(0, 0));
	    QSize  vp_size   = m_view->size();

	    /* Paint the DM image into the window pixmap */
	    QPainter p(&winpix);
	    p.drawImage(QRect(vp_offset, vp_size),
		       dm_img.convertToFormat(QImage::Format_RGB32));
	    p.end();
	}

	winpix.save(filename);
    }

private slots:
    void on_sketch_changed();
    void on_save();
    void on_undo();
    void on_mode_pick_vertex();
    void on_mode_move_vertex();
    void on_mode_add_vertex();
    void on_mode_pick_segment();
    void on_mode_move_segment();
    void on_mode_add_line();
    void on_mode_add_arc();
    void on_mode_add_bezier();
    void on_delete_selected();
    void on_mode_toggle_arc_orient();
    void on_mode_arc_radius();
    void on_mode_set_tangency();
    void on_move_selected_vertices();
    void on_fit_view();
    void on_describe_segments();
    void on_sketch_plane();
    void on_toggle_grid(bool checked);
    void on_grid_settings();
    void on_mode_chain_vertex();
    void on_goto_uv();
    void on_toggle_segment_reverse();

protected:
    void keyPressEvent(QKeyEvent *ev) override;

private:
    void install_filter(QgSketchFilter *f);
    void clear_filter();
    void refresh_tables();
    void refresh_view();
    void set_status(const QString &msg);

    /* Edit-state helpers */
    struct rt_sketch_internal *sketch() const {
	if (!m_es) return NULL;
	return (struct rt_sketch_internal *)m_es->es_int.idb_ptr;
    }
    struct rt_sketch_edit *sketch_edit() const {
	if (!m_es) return NULL;
	return (struct rt_sketch_edit *)m_es->ipe_ptr;
    }

    /* Enum for line/bezier multi-click accumulation */
    enum CreateMode { NONE, LINE, BEZIER };

    /* ---- data ---- */
    struct db_i          *m_dbip = NULL;
    struct directory     *m_dp   = NULL;
    struct bview         *m_bv   = NULL;
    struct rt_edit       *m_es   = NULL;
    struct bn_tol         m_tol;
    qsketch_draw_ctx      m_draw_ctx;

    /* ---- Qt UI ---- */
    QgView          *m_view   = NULL;
    QTableWidget    *m_vtable = NULL;  /* vertex table */
    QTableWidget    *m_stable = NULL;  /* segment table */
    QLabel          *m_status = NULL;
    QLabel          *m_cursor_label = NULL;  /* live UV cursor readout */

    /* ---- active filter ---- */
    QgSketchFilter  *m_active_filter  = NULL;
    QgSketchCursorTracker *m_tracker  = NULL;  /* always-on UV tracker */

    /* ---- grid state ---- */
    QAction         *m_grid_action    = NULL;  /* checkable grid toggle */

    /* ---- multi-click segment creation ---- */
    CreateMode       m_create_mode = NONE;
    std::vector<int> m_pending_verts;   /* vertex indices gathered so far */
};


/* ---------- constructor ---------- */

QSketchEditWindow::QSketchEditWindow(struct db_i *dbip,
				     struct directory *dp,
				     QWidget *parent)
    : QMainWindow(parent), m_dbip(dbip), m_dp(dp)
{
    setWindowTitle(QString("qsketch — %1").arg(dp->d_namep));
    resize(1200, 700);

    /* ---- tolerance ---- */
    BN_TOL_INIT(&m_tol);

    /* ---- bview ---- */
    BU_GET(m_bv, struct bview);
    bv_init(m_bv, NULL);

    /* Look along -Z toward +Z (top view, sketch in XY plane face-on).
     * az=0, el=90 gives view +X→right, +Y→up which matches the sketch
     * u_vec (1,0,0) and v_vec (0,1,0) defaults. */
    VSET(m_bv->gv_aet, 0.0, 90.0, 0.0);
    bv_mat_aet(m_bv);
    m_bv->gv_scale  = 250.0;
    m_bv->gv_size   = 2.0 * m_bv->gv_scale;
    m_bv->gv_isize  = 1.0 / m_bv->gv_size;
    bv_update(m_bv);
    bu_vls_sprintf(&m_bv->gv_name, "qsketch");
    m_bv->gv_width  = 700;
    m_bv->gv_height = 700;

    /* ---- rt_edit ---- */
    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);
    m_es = rt_edit_create(&fp, dbip, &m_tol, m_bv);
    db_free_full_path(&fp);

    if (!m_es) {
	bu_log("qsketch: rt_edit_create failed\n");
	return;
    }
    m_es->local2base = 1.0;  /* mm database */
    m_es->base2local = 1.0;
    m_es->mv_context = 1;

    /* Take an initial checkpoint for undo */
    rt_edit_checkpoint(m_es);

    /* ----  QgView ---- */
    m_view = new QgView(this, QgView_SW);
    m_view->set_view(m_bv);

    /* Register our custom draw callback so ft_plot renders the sketch */
    m_draw_ctx.es   = m_es;
    m_draw_ctx.grid = &m_bv->gv_s->gv_grid;
    m_view->set_draw_custom(sketch_draw_custom, &m_draw_ctx);

    /* ---- vertex table — allow multi-row selection ---- */
    m_vtable = new QTableWidget(this);
    m_vtable->setColumnCount(3);
    m_vtable->setHorizontalHeaderLabels({"#", "U", "V"});
    m_vtable->horizontalHeader()->setStretchLastSection(true);
    m_vtable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_vtable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vtable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vtable->setMinimumWidth(200);

    /* ---- segment table ---- */
    m_stable = new QTableWidget(this);
    m_stable->setColumnCount(3);
    m_stable->setHorizontalHeaderLabels({"#", "Type", "Params"});
    m_stable->horizontalHeader()->setStretchLastSection(true);
    m_stable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stable->setMinimumWidth(200);

    /* ---- right panel (tables + Move Sel button) ---- */
    QSplitter *right_split = new QSplitter(Qt::Vertical, this);
    {
	QGroupBox *vg = new QGroupBox("Vertices");
	QVBoxLayout *vl = new QVBoxLayout(vg);
	vl->addWidget(m_vtable);
	QPushButton *mv_sel_btn = new QPushButton("Move Selected…");
	mv_sel_btn->setToolTip(
	    "Move all selected vertices by a UV delta.\n"
	    "Select rows in the Vertices table using Shift-click / Ctrl-click.");
	connect(mv_sel_btn, &QPushButton::clicked,
		this, &QSketchEditWindow::on_move_selected_vertices);
	vl->addWidget(mv_sel_btn);
	right_split->addWidget(vg);
    }
    {
	QGroupBox *sg = new QGroupBox("Segments");
	QVBoxLayout *sl = new QVBoxLayout(sg);
	sl->addWidget(m_stable);
	right_split->addWidget(sg);
    }
    right_split->setMinimumWidth(210);

    /* ---- central splitter (view | tables) ---- */
    QSplitter *main_split = new QSplitter(Qt::Horizontal, this);
    main_split->addWidget(m_view);
    main_split->addWidget(right_split);
    main_split->setSizes({700, 250});
    setCentralWidget(main_split);

    /* ---- toolbar ---- */
    QToolBar *tb = addToolBar("Edit Modes");
    tb->setOrientation(Qt::Vertical);
    addToolBar(Qt::LeftToolBarArea, tb);

    auto mkbtn = [&](const QString &label, const char *tip, auto slot) {
	QPushButton *b = new QPushButton(label, this);
	b->setToolTip(tip);
	connect(b, &QPushButton::clicked, this, slot);
	QWidgetAction *wa = new QWidgetAction(tb);
	wa->setDefaultWidget(b);
	tb->addAction(wa);
    };

    mkbtn("Pick V",  "Pick Vertex (P)",   &QSketchEditWindow::on_mode_pick_vertex);
    mkbtn("Move V",  "Move Vertex (M)",   &QSketchEditWindow::on_mode_move_vertex);
    mkbtn("Add V",   "Add Vertex (A)",    &QSketchEditWindow::on_mode_add_vertex);
    tb->addSeparator();
    mkbtn("Pick S",  "Pick Segment (S)",  &QSketchEditWindow::on_mode_pick_segment);
    mkbtn("Move S",  "Move Segment (G)",  &QSketchEditWindow::on_mode_move_segment);
    tb->addSeparator();
    mkbtn("Line",    "Add Line (L)",      &QSketchEditWindow::on_mode_add_line);
    mkbtn("Arc",     "Add Arc (R)",       &QSketchEditWindow::on_mode_add_arc);
    mkbtn("Bezier",  "Add Bezier (B)",    &QSketchEditWindow::on_mode_add_bezier);
    tb->addSeparator();
    mkbtn("Flip Arc","Toggle arc complement — flips center_is_left on selected CARC (C)",
	  &QSketchEditWindow::on_mode_toggle_arc_orient);
    mkbtn("Set R",   "Drag to set arc radius — drag on selected CARC (I)",
	  &QSketchEditWindow::on_mode_arc_radius);
    mkbtn("Tangent", "Set Arc Tangency: click adjacent segment (T)",
	  &QSketchEditWindow::on_mode_set_tangency);
    tb->addSeparator();
    mkbtn("Delete",  "Delete selected (Del)", &QSketchEditWindow::on_delete_selected);

    /* ---- menu bar ---- */
    QMenu *file_menu = menuBar()->addMenu("&File");
    file_menu->addAction("&Save\tCtrl+S", this,
			  &QSketchEditWindow::on_save);
    file_menu->addSeparator();
    file_menu->addAction("&Quit\tCtrl+Q", this, &QWidget::close);

    QMenu *edit_menu = menuBar()->addMenu("&Edit");
    edit_menu->addAction("&Undo\tCtrl+Z", this,
			  &QSketchEditWindow::on_undo);
    edit_menu->addAction("&Delete Selected\tDel", this,
			  &QSketchEditWindow::on_delete_selected);
    edit_menu->addAction("&Go To UV…\tCtrl+G", this,
			  &QSketchEditWindow::on_goto_uv);
    edit_menu->addSeparator();
    edit_menu->addAction("&Flip Arc Complement\tC", this,
			  &QSketchEditWindow::on_mode_toggle_arc_orient);
    edit_menu->addAction("Set Arc &Radius (drag)\tI", this,
			  &QSketchEditWindow::on_mode_arc_radius);
    edit_menu->addAction("Set Arc &Tangency\tT", this,
			  &QSketchEditWindow::on_mode_set_tangency);
    edit_menu->addAction("Toggle Segment Re&verse\tV", this,
			  &QSketchEditWindow::on_toggle_segment_reverse);
    edit_menu->addSeparator();
    edit_menu->addAction("&Chain Segment\tN", this,
			  &QSketchEditWindow::on_mode_chain_vertex);
    edit_menu->addSeparator();
    edit_menu->addAction("Sketch &Plane…", this,
			  &QSketchEditWindow::on_sketch_plane);

    QMenu *view_menu = menuBar()->addMenu("&View");
    view_menu->addAction("Fit to &View\tF", this,
			  &QSketchEditWindow::on_fit_view);
    view_menu->addSeparator();
    m_grid_action = view_menu->addAction("Show &Grid\tH");
    m_grid_action->setCheckable(true);
    m_grid_action->setChecked(false);
    connect(m_grid_action, &QAction::toggled,
	    this, &QSketchEditWindow::on_toggle_grid);
    view_menu->addAction("Grid &Settings…", this,
			  &QSketchEditWindow::on_grid_settings);

    QMenu *debug_menu = menuBar()->addMenu("&Debug");
    debug_menu->addAction("&Describe All Segments", this,
			   &QSketchEditWindow::on_describe_segments);

    /* ---- status bar ---- */
    m_status = new QLabel("Ready", this);
    statusBar()->addWidget(m_status, 1);

    /* Live UV cursor readout on the right side of the status bar */
    m_cursor_label = new QLabel("U: —    V: —", this);
    m_cursor_label->setMinimumWidth(180);
    m_cursor_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_cursor_label);

    /* Cursor tracker — always installed, never consumes events */
    m_tracker = new QgSketchCursorTracker();
    m_tracker->v  = m_bv;
    m_tracker->es = m_es;
    m_view->add_event_filter(m_tracker);
    connect(m_tracker, &QgSketchCursorTracker::uv_moved,
	    this, [this](double u, double vv) {
		m_cursor_label->setText(
		    QString("U: %1   V: %2")
			.arg(u,  0, 'f', 4)
			.arg(vv, 0, 'f', 4));
	    });

    /* ---- initial display ---- */
    refresh_tables();
    /* Fit view and draw the sketch on first show */
    QTimer::singleShot(0, this, [this]() {
	on_fit_view();
    });
    set_status("Sketch loaded — choose an edit mode from the toolbar.");
}

QSketchEditWindow::~QSketchEditWindow()
{
    if (m_tracker) {
	m_view->clear_event_filter(m_tracker);
	delete m_tracker;
    }
    clear_filter();
    if (m_es)
	rt_edit_destroy(m_es);
    if (m_bv) {
	bv_free(m_bv);
	BU_PUT(m_bv, struct bview);
    }
}

/* ---------- filter management ---------- */

void
QSketchEditWindow::install_filter(QgSketchFilter *f)
{
    clear_filter();
    if (!f) return;

    f->v  = m_bv;
    f->es = m_es;

    connect(f, &QgSketchFilter::view_updated,
	    m_view, &QgView::need_update);
    connect(f, &QgSketchFilter::sketch_changed,
	    this,  &QSketchEditWindow::on_sketch_changed);

    m_view->add_event_filter(f);
    m_active_filter = f;
}

void
QSketchEditWindow::clear_filter()
{
    if (!m_active_filter) return;

    m_view->clear_event_filter(m_active_filter);
    disconnect(m_active_filter, nullptr, this, nullptr);
    disconnect(m_active_filter, nullptr, m_view, nullptr);
    delete m_active_filter;
    m_active_filter = NULL;
}

/* ---------- view / table refresh ---------- */

void
QSketchEditWindow::refresh_tables()
{
    populate_vertex_table(m_vtable, sketch());
    populate_segment_table(m_stable, sketch());

    /* Highlight currently selected vertex row */
    struct rt_sketch_edit *se = sketch_edit();
    if (se && se->curr_vert >= 0
	    && se->curr_vert < m_vtable->rowCount())
	m_vtable->selectRow(se->curr_vert);

    if (se && se->curr_seg >= 0
	    && se->curr_seg < m_stable->rowCount())
	m_stable->selectRow(se->curr_seg);
}

void
QSketchEditWindow::refresh_view()
{
    /* Write edited sketch back to the DB so the view can re-read it */
    sketch_write_to_db(m_es, m_dbip, m_dp);
    m_view->need_update(QG_VIEW_REFRESH);
}

void
QSketchEditWindow::set_status(const QString &msg)
{
    m_status->setText(msg);
}

/* ---------- slots ---------- */

void
QSketchEditWindow::on_sketch_changed()
{
    refresh_tables();
    refresh_view();
}

void
QSketchEditWindow::on_save()
{
    if (!m_es || !m_dbip || !m_dp) {
	QMessageBox::warning(this, "Save", "No sketch open.");
	return;
    }
    int ret = sketch_write_to_db(m_es, m_dbip, m_dp);
    if (ret == BRLCAD_OK) {
	rt_edit_checkpoint(m_es);   /* update undo baseline */
	set_status("Saved.");
    } else {
	QMessageBox::critical(this, "Save", "Failed to write sketch to database.");
    }
}

void
QSketchEditWindow::on_undo()
{
    if (!m_es) return;
    if (rt_edit_revert(m_es) == BRLCAD_OK) {
	on_sketch_changed();
	set_status("Reverted to last checkpoint.");
    } else {
	set_status("Nothing to undo.");
    }
}

/* ---- mode buttons ---- */

void QSketchEditWindow::on_mode_pick_vertex()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    install_filter(new QgSketchPickVertexFilter());
    set_status("Pick Vertex: click on or near a vertex to select it.");
}

void QSketchEditWindow::on_mode_move_vertex()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    install_filter(new QgSketchMoveVertexFilter());
    set_status("Move Vertex: hold left button and drag to reposition selected vertex.");
}

void QSketchEditWindow::on_mode_add_vertex()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    install_filter(new QgSketchAddVertexFilter());
    set_status("Add Vertex: left-click to place a new vertex.");
}

void QSketchEditWindow::on_mode_pick_segment()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    install_filter(new QgSketchPickSegmentFilter());
    set_status("Pick Segment: click near a segment to select it.");
}

void QSketchEditWindow::on_mode_move_segment()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    install_filter(new QgSketchMoveSegmentFilter());
    set_status("Move Segment: hold left button and drag to translate selected segment.");
}

/* ---- Add Line: pick two vertices then press Enter ---- */

void QSketchEditWindow::on_mode_add_line()
{
    m_create_mode = LINE;
    m_pending_verts.clear();

    /* Install an AddVertex filter to place/select vertices */
    auto *f = new QgSketchAddVertexFilter();
    install_filter(f);

    /* Connect a one-shot handler to accumulate vertex picks */
    connect(f, &QgSketchFilter::sketch_changed, this, [this]() {
	if (m_create_mode != LINE) return;

	struct rt_sketch_edit *se = sketch_edit();
	if (!se || se->curr_vert < 0) return;

	int vi = se->curr_vert;

	/* Avoid duplicates */
	for (int x : m_pending_verts)
	    if (x == vi) return;

	m_pending_verts.push_back(vi);
	set_status(QString("Add Line: vertex %1 picked (%2/2). "
			   "Pick another or press Enter to finish.")
			   .arg(vi).arg((int)m_pending_verts.size()));

	if ((int)m_pending_verts.size() >= 2)
	    set_status("Add Line: 2 vertices selected — press Enter to create line.");
    });

    set_status("Add Line: click to place/select vertex 1.");
}

/* ---- Add Arc ---- */

void QSketchEditWindow::on_mode_add_arc()
{
    m_create_mode = NONE;
    m_pending_verts.clear();
    clear_filter();

    /* Collect parameters via a dialog */
    QDialog dlg(this);
    dlg.setWindowTitle("Add Circular Arc");
    QFormLayout *fl = new QFormLayout(&dlg);

    QSpinBox *sb_start = new QSpinBox(&dlg);
    sb_start->setRange(0, 9999);
    QSpinBox *sb_end = new QSpinBox(&dlg);
    sb_end->setRange(0, 9999);
    QDoubleSpinBox *sb_r = new QDoubleSpinBox(&dlg);
    sb_r->setRange(-9999, 9999);
    sb_r->setDecimals(4);
    sb_r->setValue(10.0);
    sb_r->setToolTip("Radius in mm.  Negative = full circle (end is centre).");
    QCheckBox *cb_left = new QCheckBox("Centre to left", &dlg);
    QCheckBox *cb_cw   = new QCheckBox("Clockwise",      &dlg);

    fl->addRow("Start vertex index:", sb_start);
    fl->addRow("End vertex index:",   sb_end);
    fl->addRow("Radius (mm):",        sb_r);
    fl->addRow("",                    cb_left);
    fl->addRow("",                    cb_cw);

    QDialogButtonBox *bb = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
	set_status("Arc creation cancelled.");
	return;
    }

    if (!m_es) return;
    rt_edit_checkpoint(m_es);

    m_es->e_para[0] = (fastf_t)sb_start->value();
    m_es->e_para[1] = (fastf_t)sb_end->value();
    m_es->e_para[2] = sb_r->value();
    m_es->e_para[3] = cb_left->isChecked() ? 1.0 : 0.0;
    m_es->e_para[4] = cb_cw->isChecked()   ? 1.0 : 0.0;
    m_es->e_inpara  = 5;

    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
	    ECMD_SKETCH_APPEND_ARC);
    rt_edit_process(m_es);

    on_sketch_changed();
    set_status("Arc added.");
}

/* ---- Add Bezier ---- */

void QSketchEditWindow::on_mode_add_bezier()
{
    m_create_mode = BEZIER;
    m_pending_verts.clear();
    clear_filter();

    /* Install an AddVertex filter; accumulate until Enter */
    auto *f = new QgSketchAddVertexFilter();
    install_filter(f);

    connect(f, &QgSketchFilter::sketch_changed, this, [this]() {
	if (m_create_mode != BEZIER) return;

	struct rt_sketch_edit *se = sketch_edit();
	if (!se || se->curr_vert < 0) return;

	int vi = se->curr_vert;
	for (int x : m_pending_verts)
	    if (x == vi) return;

	m_pending_verts.push_back(vi);
	set_status(QString("Add Bezier: %1 control point(s) — "
			   "click more or press Enter to commit.")
			   .arg((int)m_pending_verts.size()));
    });

    set_status("Add Bezier: click to place control points, then press Enter.");
}

/* ---- Delete ---- */

void QSketchEditWindow::on_delete_selected()
{
    if (!m_es) return;
    struct rt_sketch_edit *se = sketch_edit();
    if (!se) return;

    if (se->curr_seg >= 0) {
	rt_edit_checkpoint(m_es);
	m_es->e_inpara = 0;
	EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
		ECMD_SKETCH_DELETE_SEGMENT);
	rt_edit_process(m_es);
	on_sketch_changed();
	set_status("Segment deleted.");
	return;
    }

    if (se->curr_vert >= 0) {
	rt_edit_checkpoint(m_es);
	m_es->e_inpara = 0;
	EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
		ECMD_SKETCH_DELETE_VERTEX);
	rt_edit_process(m_es);
	on_sketch_changed();
	set_status("Vertex deleted (only if unreferenced by any segment).");
	return;
    }

    set_status("Nothing selected to delete.");
}

/* ---- Arc complement (toggle center_is_left) ---- */

void QSketchEditWindow::on_mode_toggle_arc_orient()
{
    if (!m_es) return;
    struct rt_sketch_edit *se = sketch_edit();
    if (!se || se->curr_seg < 0) {
	set_status("Flip Arc: no segment selected. Pick a segment first (S key).");
	return;
    }

    const struct rt_sketch_internal *skt = sketch();
    if (!skt) return;

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC) {
	set_status("Flip Arc: selected segment is not an arc.");
	return;
    }

    const struct carc_seg *cs = (const struct carc_seg *)seg;
    if (cs->radius < 0.0) {
	set_status("Flip Arc: cannot flip a full-circle arc.");
	return;
    }

    rt_edit_checkpoint(m_es);
    m_es->e_inpara = 0;
    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
	    ECMD_SKETCH_TOGGLE_ARC_ORIENT);
    rt_edit_process(m_es);
    on_sketch_changed();
    set_status(QString("Arc %1 orientation flipped.")
		       .arg(se->curr_seg));
}

/* ---- Interactive arc radius drag ---- */

void QSketchEditWindow::on_mode_arc_radius()
{
    m_create_mode = NONE;
    m_pending_verts.clear();

    struct rt_sketch_edit *se = sketch_edit();
    if (!se || se->curr_seg < 0) {
	set_status("Set Arc Radius: no segment selected. Pick a segment first (S key).");
	return;
    }

    const struct rt_sketch_internal *skt = sketch();
    if (!skt) return;

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC) {
	set_status("Set Arc Radius: selected segment is not an arc.");
	return;
    }

    rt_edit_checkpoint(m_es);
    install_filter(new QgSketchArcRadiusFilter());
    set_status("Set Arc Radius: drag to resize the selected arc.");
}

/* ---- Move multiple selected vertices ---- */

void QSketchEditWindow::on_move_selected_vertices()
{
    if (!m_es) return;

    QList<QTableWidgetItem *> selected = m_vtable->selectedItems();
    if (selected.isEmpty()) {
	set_status("Move Selected: no vertices selected in the Vertices table.");
	return;
    }

    /* Collect unique row indices */
    QSet<int> rows;
    for (auto *item : selected)
	rows.insert(item->row());

    /* Ask for UV delta */
    QDialog dlg(this);
    dlg.setWindowTitle("Move Selected Vertices");
    QFormLayout *fl = new QFormLayout(&dlg);
    QDoubleSpinBox *sb_du = new QDoubleSpinBox(&dlg);
    sb_du->setRange(-99999.0, 99999.0);
    sb_du->setDecimals(4);
    sb_du->setSuffix(" mm");
    QDoubleSpinBox *sb_dv = new QDoubleSpinBox(&dlg);
    sb_dv->setRange(-99999.0, 99999.0);
    sb_dv->setDecimals(4);
    sb_dv->setSuffix(" mm");
    fl->addRow("\xce\x94U:", sb_du);
    fl->addRow("\xce\x94V:", sb_dv);
    QDialogButtonBox *bb = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
	return;

    rt_edit_checkpoint(m_es);

    /* Pack e_para: [0]=dU, [1]=dV, [2..]=vertex indices */
    int idx = 0;
    m_es->e_para[idx++] = (fastf_t)sb_du->value();
    m_es->e_para[idx++] = (fastf_t)sb_dv->value();
    for (int r : rows) {
	if (idx >= RT_EDIT_MAXPARA) break;
	m_es->e_para[idx++] = (fastf_t)r;
    }
    m_es->e_inpara = idx;

    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
	    ECMD_SKETCH_MOVE_VERTEX_LIST);
    rt_edit_process(m_es);
    on_sketch_changed();
    set_status(QString("Moved %1 vertex/vertices by (%2, %3) mm.")
		       .arg(rows.size())
		       .arg(sb_du->value(), 0, 'f', 4)
		       .arg(sb_dv->value(), 0, 'f', 4));
}

/* ---- Fit view to sketch bounds ---- */

void QSketchEditWindow::on_fit_view()
{
    const struct rt_sketch_internal *skt = sketch();
    if (!skt || skt->vert_count == 0) {
	set_status("Fit View: no vertices to fit.");
	return;
    }

    fastf_t u_min = skt->verts[0][0], u_max = skt->verts[0][0];
    fastf_t v_min = skt->verts[0][1], v_max = skt->verts[0][1];
    for (size_t i = 1; i < skt->vert_count; i++) {
	fastf_t u  = skt->verts[i][0];
	fastf_t vv = skt->verts[i][1];
	if (u  < u_min) u_min = u;
	if (u  > u_max) u_max = u;
	if (vv < v_min) v_min = vv;
	if (vv > v_max) v_max = vv;
    }

    fastf_t span_u = u_max - u_min;
    fastf_t span_v = v_max - v_min;
    fastf_t span = (span_u > span_v) ? span_u : span_v;
    span *= 1.25;  /* 25% padding */
    if (span < 1.0) span = 1.0;

    /* Centre view on sketch centroid in world space */
    const struct rt_sketch_internal *skt2 = skt;
    fastf_t cx = (u_min + u_max) * 0.5;
    fastf_t cy = (v_min + v_max) * 0.5;
    point_t world_center;
    world_center[0] = skt2->V[0] + cx * skt2->u_vec[0] + cy * skt2->v_vec[0];
    world_center[1] = skt2->V[1] + cx * skt2->u_vec[1] + cy * skt2->v_vec[1];
    world_center[2] = skt2->V[2] + cx * skt2->u_vec[2] + cy * skt2->v_vec[2];
    MAT_IDN(m_bv->gv_center);
    MAT_DELTAS_VEC_NEG(m_bv->gv_center, world_center);

    m_bv->gv_scale = span * 0.5;
    m_bv->gv_size  = span;
    m_bv->gv_isize = 1.0 / span;
    bv_update(m_bv);

    m_view->need_update(QG_VIEW_REFRESH);
    set_status("View fitted to sketch bounds.");
}

/* ---- Describe all segments (diagnostic dump) ---- */

void QSketchEditWindow::on_describe_segments()
{
    const struct rt_sketch_internal *skt = sketch();
    if (!skt) {
	QMessageBox::warning(this, "Describe Segments", "No sketch loaded.");
	return;
    }

    QString out;
    out += QString("Sketch: %1 vertex/vertices, %2 segment(s)\n\n")
		   .arg((uint)skt->vert_count)
		   .arg((uint)skt->curve.count);

    out += "Vertices:\n";
    for (size_t i = 0; i < skt->vert_count; i++) {
	out += QString("  V[%1] = (%2, %3)\n")
		       .arg((uint)i)
		       .arg(skt->verts[i][0], 0, 'g', 6)
		       .arg(skt->verts[i][1], 0, 'g', 6);
    }

    out += "\nSegments:\n";
    for (size_t i = 0; i < skt->curve.count; i++) {
	void *seg = skt->curve.segment[i];
	if (!seg) {
	    out += QString("  S[%1] = NULL\n").arg((uint)i);
	    continue;
	}
	uint32_t magic = *(uint32_t *)seg;
	int rev = skt->curve.reverse ? skt->curve.reverse[i] : 0;

	if (magic == CURVE_LSEG_MAGIC) {
	    struct line_seg *ls = (struct line_seg *)seg;
	    out += QString("  S[%1] Line: V[%2] \xe2\x86\x92 V[%3]%4\n")
			   .arg((uint)i).arg(ls->start).arg(ls->end)
			   .arg(rev ? " (reversed)" : "");
	} else if (magic == CURVE_CARC_MAGIC) {
	    struct carc_seg *cs = (struct carc_seg *)seg;
	    if (cs->radius < 0.0) {
		out += QString("  S[%1] Circle: centre=V[%2] r=%3%4\n")
			       .arg((uint)i).arg(cs->end)
			       .arg(-cs->radius, 0, 'g', 6)
			       .arg(rev ? " (reversed)" : "");
	    } else {
		out += QString("  S[%1] Arc: V[%2]\xe2\x86\x92V[%3] r=%4  CIL=%5  CW=%6%7\n")
			       .arg((uint)i)
			       .arg(cs->start).arg(cs->end)
			       .arg(cs->radius, 0, 'g', 6)
			       .arg(cs->center_is_left ? "yes" : "no")
			       .arg(cs->orientation ? "yes" : "no")
			       .arg(rev ? " (reversed)" : "");
	    }
	} else if (magic == CURVE_BEZIER_MAGIC) {
	    struct bezier_seg *bs = (struct bezier_seg *)seg;
	    QString pts;
	    for (int k = 0; k <= bs->degree; k++) {
		if (k) pts += " - ";
		pts += QString("V[%1]").arg(bs->ctl_points[k]);
	    }
	    out += QString("  S[%1] Bezier deg=%2: %3%4\n")
			   .arg((uint)i).arg(bs->degree).arg(pts)
			   .arg(rev ? " (reversed)" : "");
	} else if (magic == CURVE_NURB_MAGIC) {
	    struct nurb_seg *ns = (struct nurb_seg *)seg;
	    out += QString("  S[%1] NURB: order=%2 c_size=%3 k_size=%4%5\n")
			   .arg((uint)i)
			   .arg(ns->order).arg(ns->c_size).arg(ns->k.k_size)
			   .arg(rev ? " (reversed)" : "");
	} else {
	    out += QString("  S[%1] Unknown magic 0x%2\n")
			   .arg((uint)i).arg(magic, 0, 16);
	}
    }

    /* Show in a scrollable message box */
    QDialog dlg(this);
    dlg.setWindowTitle("Segment Description");
    dlg.resize(600, 400);
    QVBoxLayout *vl = new QVBoxLayout(&dlg);
    QScrollArea *sa = new QScrollArea(&dlg);
    QLabel *lbl = new QLabel(out, sa);
    lbl->setFont(QFont("Courier New", 9));
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lbl->setWordWrap(false);
    sa->setWidget(lbl);
    sa->setWidgetResizable(true);
    vl->addWidget(sa);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

/* ---- Arc tangency ---- */

void QSketchEditWindow::on_mode_set_tangency()
{
    m_create_mode = NONE;
    m_pending_verts.clear();

    struct rt_sketch_edit *se = sketch_edit();
    if (!se || se->curr_seg < 0) {
	set_status("Set Tangency: no arc selected. Pick a segment first (S key).");
	return;
    }

    const struct rt_sketch_internal *skt = sketch();
    if (!skt) return;

    void *seg = skt->curve.segment[se->curr_seg];
    if (!seg || *(uint32_t *)seg != CURVE_CARC_MAGIC) {
	set_status("Set Tangency: selected segment is not an arc.");
	return;
    }
    if (((const struct carc_seg *)seg)->radius < 0.0) {
	set_status("Set Tangency: cannot set tangency on a full-circle arc.");
	return;
    }

    /* Ask for optional tangency angle */
    bool ok = true;
    double angle_deg = QInputDialog::getDouble(this, "Set Arc Tangency",
	    "Tangency angle offset (degrees, 0 = smooth join):",
	    0.0, -360.0, 360.0, 1, &ok);
    if (!ok)
	return;

    QgSketchSetTangencyFilter *f = new QgSketchSetTangencyFilter();
    f->tangency_angle = angle_deg * M_PI / 180.0;
    install_filter(f);

    connect(f, &QgSketchFilter::sketch_changed,
	    this, &QSketchEditWindow::on_sketch_changed);

    set_status(QString("Set Tangency: click adjacent segment (angle offset %1°).")
		       .arg(angle_deg, 0, 'f', 1));
}

/* ---- Grid toggle ---- */

void QSketchEditWindow::on_toggle_grid(bool checked)
{
    if (!m_bv) return;
    m_bv->gv_s->gv_grid.draw = checked ? 1 : 0;
    m_view->need_update(QG_VIEW_REFRESH);
    set_status(checked ? "Grid enabled." : "Grid disabled.");
}

/* ---- Grid settings dialog ---- */

void QSketchEditWindow::on_grid_settings()
{
    if (!m_bv) return;

    struct bv_grid_state *gs = &m_bv->gv_s->gv_grid;

    QDialog dlg(this);
    dlg.setWindowTitle("Grid Settings");
    QFormLayout *fl = new QFormLayout(&dlg);

    QDoubleSpinBox *sb_res_h = new QDoubleSpinBox(&dlg);
    sb_res_h->setRange(0.001, 99999.0);
    sb_res_h->setDecimals(3);
    sb_res_h->setValue(gs->res_h);
    sb_res_h->setSuffix(" mm");

    QDoubleSpinBox *sb_res_v = new QDoubleSpinBox(&dlg);
    sb_res_v->setRange(0.001, 99999.0);
    sb_res_v->setDecimals(3);
    sb_res_v->setValue(gs->res_v);
    sb_res_v->setSuffix(" mm");

    QDoubleSpinBox *sb_major_h = new QDoubleSpinBox(&dlg);
    sb_major_h->setRange(1.0, 1000.0);
    sb_major_h->setDecimals(0);
    sb_major_h->setValue(gs->res_major_h);

    QDoubleSpinBox *sb_major_v = new QDoubleSpinBox(&dlg);
    sb_major_v->setRange(1.0, 1000.0);
    sb_major_v->setDecimals(0);
    sb_major_v->setValue(gs->res_major_v);

    /* Anchor point — exposed in this dialog (was previously omitted) */
    QDoubleSpinBox *sb_anchor_u = new QDoubleSpinBox(&dlg);
    sb_anchor_u->setRange(-99999.0, 99999.0);
    sb_anchor_u->setDecimals(3);
    sb_anchor_u->setValue(gs->anchor[0]);
    sb_anchor_u->setSuffix(" mm");

    QDoubleSpinBox *sb_anchor_v = new QDoubleSpinBox(&dlg);
    sb_anchor_v->setRange(-99999.0, 99999.0);
    sb_anchor_v->setDecimals(3);
    sb_anchor_v->setValue(gs->anchor[1]);
    sb_anchor_v->setSuffix(" mm");

    /* Row widget for anchor U+V side by side */
    QWidget *anchor_row = new QWidget(&dlg);
    {
	QHBoxLayout *hl = new QHBoxLayout(anchor_row);
	hl->setContentsMargins(0, 0, 0, 0);
	hl->addWidget(sb_anchor_u);
	hl->addWidget(new QLabel("V:"));
	hl->addWidget(sb_anchor_v);
    }

    QCheckBox *cb_snap = new QCheckBox("Snap to grid", &dlg);
    cb_snap->setChecked(gs->snap);

    QCheckBox *cb_adaptive = new QCheckBox("Adaptive spacing", &dlg);
    cb_adaptive->setChecked(gs->adaptive);

    fl->addRow("H spacing:", sb_res_h);
    fl->addRow("V spacing:", sb_res_v);
    fl->addRow("Major H every:", sb_major_h);
    fl->addRow("Major V every:", sb_major_v);
    fl->addRow("Anchor U:", anchor_row);
    fl->addRow(cb_snap);
    fl->addRow(cb_adaptive);

    QDialogButtonBox *bb = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
	return;

    gs->res_h       = (fastf_t)sb_res_h->value();
    gs->res_v       = (fastf_t)sb_res_v->value();
    gs->res_major_h = (int)sb_major_h->value();
    gs->res_major_v = (int)sb_major_v->value();
    gs->anchor[0]   = (fastf_t)sb_anchor_u->value();
    gs->anchor[1]   = (fastf_t)sb_anchor_v->value();
    gs->snap        = cb_snap->isChecked() ? 1 : 0;
    gs->adaptive    = cb_adaptive->isChecked() ? 1 : 0;

    m_view->need_update(QG_VIEW_REFRESH);
    set_status(QString("Grid: H=%1 V=%2 mm  anchor=(%3,%4)  snap=%5.")
		       .arg(gs->res_h, 0, 'f', 3)
		       .arg(gs->res_v, 0, 'f', 3)
		       .arg(gs->anchor[0], 0, 'f', 3)
		       .arg(gs->anchor[1], 0, 'f', 3)
		       .arg(gs->snap ? "on" : "off"));
}

/* ---- Sketch plane dialog ---- */

void QSketchEditWindow::on_sketch_plane()
{
    if (!m_es) return;
    const struct rt_sketch_internal *skt = sketch();
    if (!skt) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Sketch Plane (V, A, B)");
    QFormLayout *fl = new QFormLayout(&dlg);

    auto mkxyz = [&](const char *label, const vect_t v)
	    -> std::array<QDoubleSpinBox *, 3>
    {
	std::array<QDoubleSpinBox *, 3> sb;
	QWidget *row = new QWidget(&dlg);
	QHBoxLayout *hl = new QHBoxLayout(row);
	hl->setContentsMargins(0, 0, 0, 0);
	for (int k = 0; k < 3; k++) {
	    sb[k] = new QDoubleSpinBox(&dlg);
	    sb[k]->setRange(-99999.0, 99999.0);
	    sb[k]->setDecimals(4);
	    sb[k]->setValue(v[k]);
	    hl->addWidget(sb[k]);
	}
	fl->addRow(label, row);
	return sb;
    };

    auto sb_V = mkxyz("V (origin):", skt->V);
    auto sb_A = mkxyz("A (u_vec):", skt->u_vec);
    auto sb_B = mkxyz("B (v_vec):", skt->v_vec);

    QDialogButtonBox *bb = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
	return;

    rt_edit_checkpoint(m_es);
    for (int k = 0; k < 3; k++) m_es->e_para[k  ] = sb_V[k]->value();
    for (int k = 0; k < 3; k++) m_es->e_para[k+3] = sb_A[k]->value();
    for (int k = 0; k < 3; k++) m_es->e_para[k+6] = sb_B[k]->value();
    m_es->e_inpara = 9;

    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
	    ECMD_SKETCH_SET_PLANE);
    rt_edit_process(m_es);
    on_sketch_changed();
    set_status("Sketch plane updated.");
}

/* ---- Segment chaining (N key) ---- */

void QSketchEditWindow::on_mode_chain_vertex()
{
    /* Chain mode: if there are pending vertices (end of last LINE or BEZIER
     * creation), keep the last accumulated vertex as the start of the next
     * segment.  If not in creation mode, or the pending list is empty,
     * simply report what the user should do.
     *
     * NOTE: only LINE and BEZIER use m_pending_verts for multi-click
     * accumulation.  ARC creation uses a dialog and does not accumulate
     * pending vertices, so it is not chainable in the same way. If new
     * multi-click creation modes are added (e.g. a polyline mode), add
     * them to the condition below. */

    if ((m_create_mode == LINE || m_create_mode == BEZIER)
	    && !m_pending_verts.empty()) {
	/* Keep only the last vertex; user continues drawing from it */
	int last = m_pending_verts.back();
	m_pending_verts.clear();
	m_pending_verts.push_back(last);
	set_status(QString("Chain: continuing from V[%1]. "
			   "Click more vertices, Enter to commit.")
			   .arg(last));
    } else if (m_create_mode == LINE || m_create_mode == BEZIER) {
	/* In a multi-click mode but no vertex yet accumulated — nothing to chain */
	m_pending_verts.clear();
	m_create_mode = NONE;
	clear_filter();
	set_status("Chain: cancelled (no prior vertex to chain from).");
    } else {
	/* Activate Add-Vertex mode so the user can start a new unconnected
	 * contour from scratch. */
	m_create_mode = NONE;
	m_pending_verts.clear();
	on_mode_add_vertex();
	set_status("Chain: add a new start vertex for the next contour.");
    }
}

/* ---- Go To UV (editable coordinate entry) ---- */

void QSketchEditWindow::on_goto_uv()
{
    if (!m_es) return;

    struct rt_sketch_edit *se = sketch_edit();

    /* If a vertex is selected, show its current coords as defaults */
    const struct rt_sketch_internal *skt = sketch();
    double default_u = 0.0, default_v = 0.0;
    if (se && se->curr_vert >= 0
	    && skt && (size_t)se->curr_vert < skt->vert_count) {
	default_u = skt->verts[se->curr_vert][0];
	default_v = skt->verts[se->curr_vert][1];
    }

    QDialog dlg(this);
    dlg.setWindowTitle(se && se->curr_vert >= 0
		       ? QString("Move V[%1] to UV").arg(se->curr_vert)
		       : "Go To UV (no vertex selected — will be used for next add)");
    QFormLayout *fl = new QFormLayout(&dlg);

    QDoubleSpinBox *sb_u = new QDoubleSpinBox(&dlg);
    sb_u->setRange(-99999.0, 99999.0);
    sb_u->setDecimals(4);
    sb_u->setValue(default_u);
    sb_u->setSuffix(" mm");

    QDoubleSpinBox *sb_v = new QDoubleSpinBox(&dlg);
    sb_v->setRange(-99999.0, 99999.0);
    sb_v->setDecimals(4);
    sb_v->setValue(default_v);
    sb_v->setSuffix(" mm");

    fl->addRow("U:", sb_u);
    fl->addRow("V:", sb_v);

    QDialogButtonBox *bb = new QDialogButtonBox(
	    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
	return;

    fastf_t u  = (fastf_t)sb_u->value();
    fastf_t vv = (fastf_t)sb_v->value();

    if (se && se->curr_vert >= 0) {
	/* Move the selected vertex */
	rt_edit_checkpoint(m_es);
	fastf_t scale = (m_es->local2base > 0.0) ? (1.0 / m_es->local2base) : 1.0;
	m_es->e_para[0] = u  * scale;
	m_es->e_para[1] = vv * scale;
	m_es->e_inpara  = 2;
	EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
		ECMD_SKETCH_MOVE_VERTEX);
	rt_edit_process(m_es);
	on_sketch_changed();
	set_status(QString("V[%1] moved to (%2, %3) mm.")
			   .arg(se->curr_vert)
			   .arg(u,  0, 'f', 4)
			   .arg(vv, 0, 'f', 4));
    } else {
	/* No vertex selected — create a new one at the given UV */
	rt_edit_checkpoint(m_es);
	fastf_t scale = (m_es->local2base > 0.0) ? (1.0 / m_es->local2base) : 1.0;
	m_es->e_para[0] = u  * scale;
	m_es->e_para[1] = vv * scale;
	m_es->e_inpara  = 2;
	EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
		ECMD_SKETCH_ADD_VERTEX);
	rt_edit_process(m_es);
	on_sketch_changed();
	set_status(QString("New vertex added at (%1, %2) mm.")
			   .arg(u,  0, 'f', 4)
			   .arg(vv, 0, 'f', 4));
    }
}

/* ---- Toggle segment reverse flag ---- */

void QSketchEditWindow::on_toggle_segment_reverse()
{
    if (!m_es) return;
    struct rt_sketch_edit *se = sketch_edit();
    if (!se || se->curr_seg < 0) {
	set_status("Toggle Reverse: no segment selected. Pick a segment first (S key).");
	return;
    }

    rt_edit_checkpoint(m_es);
    m_es->e_inpara = 0;
    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
	    ECMD_SKETCH_TOGGLE_SEGMENT_REVERSE);
    rt_edit_process(m_es);

    const struct rt_sketch_internal *skt = sketch();
    bool now_reversed = skt && skt->curve.reverse
	    && (size_t)se->curr_seg < skt->curve.count
	    && skt->curve.reverse[se->curr_seg] != 0;

    on_sketch_changed();
    set_status(QString("Segment %1 reverse flag: %2.")
		       .arg(se->curr_seg)
		       .arg(now_reversed ? "ON" : "OFF"));
}

void
QSketchEditWindow::keyPressEvent(QKeyEvent *ev)
{
    switch (ev->key()) {
	case Qt::Key_P:
	    on_mode_pick_vertex();
	    break;
	case Qt::Key_M:
	    on_mode_move_vertex();
	    break;
	case Qt::Key_A:
	    on_mode_add_vertex();
	    break;
	case Qt::Key_S:
	    if (ev->modifiers() & Qt::ControlModifier)
		on_save();
	    else
		on_mode_pick_segment();
	    break;
	case Qt::Key_G:
	    if (ev->modifiers() & Qt::ControlModifier)
		on_goto_uv();
	    else
		on_mode_move_segment();
	    break;
	case Qt::Key_L:
	    on_mode_add_line();
	    break;
	case Qt::Key_R:
	    on_mode_add_arc();
	    break;
	case Qt::Key_B:
	    on_mode_add_bezier();
	    break;
	case Qt::Key_C:
	    on_mode_toggle_arc_orient();
	    break;
	case Qt::Key_I:
	    on_mode_arc_radius();
	    break;
	case Qt::Key_T:
	    on_mode_set_tangency();
	    break;
	case Qt::Key_N:
	    on_mode_chain_vertex();
	    break;
	case Qt::Key_V:
	    on_toggle_segment_reverse();
	    break;
	case Qt::Key_H:
	    if (m_grid_action)
		m_grid_action->toggle();
	    break;
	case Qt::Key_F:
	    on_fit_view();
	    break;
	case Qt::Key_Z:
	    if (ev->modifiers() & Qt::ControlModifier)
		on_undo();
	    break;
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
	    on_delete_selected();
	    break;

	case Qt::Key_Return:
	case Qt::Key_Enter:
	    /* Commit pending line / bezier creation */
	    if (m_create_mode == LINE && (int)m_pending_verts.size() >= 2) {
		if (m_es) {
		    rt_edit_checkpoint(m_es);
		    m_es->e_para[0] = (fastf_t)m_pending_verts[0];
		    m_es->e_para[1] = (fastf_t)m_pending_verts[1];
		    m_es->e_inpara  = 2;
		    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
			    ECMD_SKETCH_APPEND_LINE);
		    rt_edit_process(m_es);
		    on_sketch_changed();
		    set_status(QString("Line added (%1 → %2).")
				       .arg(m_pending_verts[0])
				       .arg(m_pending_verts[1]));
		}
		m_create_mode = NONE;
		m_pending_verts.clear();
		clear_filter();
	    } else if (m_create_mode == BEZIER
		       && (int)m_pending_verts.size() >= 2) {
		if (m_es) {
		    rt_edit_checkpoint(m_es);
		    int n = (int)m_pending_verts.size();
		    for (int i = 0; i < n; i++)
			m_es->e_para[i] = (fastf_t)m_pending_verts[i];
		    m_es->e_inpara = n;
		    EDOBJ[m_es->es_int.idb_type].ft_set_edit_mode(m_es,
			    ECMD_SKETCH_APPEND_BEZIER);
		    rt_edit_process(m_es);
		    on_sketch_changed();
		    set_status(QString("Bezier added (degree %1).")
				       .arg(n - 1));
		}
		m_create_mode = NONE;
		m_pending_verts.clear();
		clear_filter();
	    }
	    break;

	case Qt::Key_Escape:
	    m_create_mode = NONE;
	    m_pending_verts.clear();
	    clear_filter();
	    set_status("Idle.");
	    break;

	default:
	    QMainWindow::keyPressEvent(ev);
	    break;
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Pre-built demo sketch creation helpers                              */
/* ------------------------------------------------------------------ */

/*
 * Helper: add a vertex to the sketch's vert array, return its index.
 */
static int
skt_add_vert(struct rt_sketch_internal *skt, fastf_t u, fastf_t vv)
{
    int vi = (int)skt->vert_count;
    skt->verts = (point2d_t *)bu_realloc(skt->verts,
					  (vi + 1) * sizeof(point2d_t),
					  "skt verts");
    skt->verts[vi][0] = u;
    skt->verts[vi][1] = vv;
    skt->vert_count++;
    return vi;
}

static void
skt_add_line(struct rt_sketch_internal *skt, int s, int e)
{
    struct line_seg *ls;
    BU_ALLOC(ls, struct line_seg);
    ls->magic = CURVE_LSEG_MAGIC;
    ls->start = s; ls->end = e;
    int ci = (int)skt->curve.count;
    skt->curve.segment = (void **)bu_realloc(skt->curve.segment,
					      (ci + 1) * sizeof(void *),
					      "skt segs");
    skt->curve.reverse = (int *)bu_realloc(skt->curve.reverse,
					   (ci + 1) * sizeof(int),
					   "skt reverse");
    skt->curve.segment[ci] = ls;
    skt->curve.reverse[ci] = 0;
    skt->curve.count++;
}

static void
skt_add_arc(struct rt_sketch_internal *skt, int s, int e,
	    fastf_t r, int center_is_left, int orientation)
{
    struct carc_seg *cs;
    BU_ALLOC(cs, struct carc_seg);
    cs->magic         = CURVE_CARC_MAGIC;
    cs->start         = s; cs->end = e;
    cs->radius        = r;
    cs->center        = -1;
    cs->center_is_left = center_is_left;
    cs->orientation   = orientation;
    int ci = (int)skt->curve.count;
    skt->curve.segment = (void **)bu_realloc(skt->curve.segment,
					      (ci + 1) * sizeof(void *),
					      "skt segs");
    skt->curve.reverse = (int *)bu_realloc(skt->curve.reverse,
					   (ci + 1) * sizeof(int),
					   "skt reverse");
    skt->curve.segment[ci] = cs;
    skt->curve.reverse[ci] = 0;
    skt->curve.count++;
}

static void
skt_add_circle(struct rt_sketch_internal *skt, int start, int centre, fastf_t r)
{
    /* Full circle: radius < 0, end vertex is centre */
    struct carc_seg *cs;
    BU_ALLOC(cs, struct carc_seg);
    cs->magic  = CURVE_CARC_MAGIC;
    cs->start  = start;
    cs->end    = centre;
    cs->radius = -r;
    cs->center = -1;
    cs->center_is_left = 0;
    cs->orientation    = 0;
    int ci = (int)skt->curve.count;
    skt->curve.segment = (void **)bu_realloc(skt->curve.segment,
					      (ci + 1) * sizeof(void *),
					      "skt segs");
    skt->curve.reverse = (int *)bu_realloc(skt->curve.reverse,
					   (ci + 1) * sizeof(int),
					   "skt reverse");
    skt->curve.segment[ci] = cs;
    skt->curve.reverse[ci] = 0;
    skt->curve.count++;
}

/* Allocate and initialise an rt_sketch_internal in the XY plane at origin.
 * wdb_export() takes ownership of this pointer and will free it; callers
 * MUST NOT free it themselves after calling wdb_export(). */
static struct rt_sketch_internal *
skt_new(void)
{
    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    memset(skt, 0, sizeof(*skt));
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V,     0.0, 0.0, 0.0);
    VSET(skt->u_vec, 1.0, 0.0, 0.0);
    VSET(skt->v_vec, 0.0, 1.0, 0.0);
    return skt;
}

static struct directory *
sketch_create_box(struct rt_wdb *wdbp, const char *name, fastf_t w, fastf_t h)
{
    struct rt_sketch_internal *skt = skt_new();
    fastf_t hw = w * 0.5, hh = h * 0.5;
    int v0 = skt_add_vert(skt, -hw, -hh);
    int v1 = skt_add_vert(skt,  hw, -hh);
    int v2 = skt_add_vert(skt,  hw,  hh);
    int v3 = skt_add_vert(skt, -hw,  hh);
    skt_add_line(skt, v0, v1);
    skt_add_line(skt, v1, v2);
    skt_add_line(skt, v2, v3);
    skt_add_line(skt, v3, v0);
    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

static struct directory *
sketch_create_rounded_rect(struct rt_wdb *wdbp, const char *name,
			   fastf_t w, fastf_t h, fastf_t r)
{
    struct rt_sketch_internal *skt = skt_new();
    if (r > w * 0.5) r = w * 0.5;
    if (r > h * 0.5) r = h * 0.5;
    fastf_t hw = w * 0.5, hh = h * 0.5;
    /* Bottom edge */
    int bl = skt_add_vert(skt, -hw + r, -hh);
    int br = skt_add_vert(skt,  hw - r, -hh);
    /* Right edge */
    int rb = skt_add_vert(skt,  hw, -hh + r);
    int rt = skt_add_vert(skt,  hw,  hh - r);
    /* Top edge */
    int tr = skt_add_vert(skt,  hw - r,  hh);
    int tl = skt_add_vert(skt, -hw + r,  hh);
    /* Left edge */
    int lt = skt_add_vert(skt, -hw,  hh - r);
    int lb = skt_add_vert(skt, -hw, -hh + r);
    /* Lines */
    skt_add_line(skt, bl, br);
    skt_add_line(skt, rb, rt);
    skt_add_line(skt, tr, tl);
    skt_add_line(skt, lt, lb);
    /* Corner arcs (radius = r, CCW, centre to the left) */
    skt_add_arc(skt, br, rb, r, 1, 0);
    skt_add_arc(skt, rt, tr, r, 1, 0);
    skt_add_arc(skt, tl, lt, r, 1, 0);
    skt_add_arc(skt, lb, bl, r, 1, 0);

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

/* Circle + inner concentric circle (donut cross section) */
static struct directory *
sketch_create_annulus(struct rt_wdb *wdbp, const char *name,
		      fastf_t r_outer, fastf_t r_inner)
{
    struct rt_sketch_internal *skt = skt_new();
    int v_out_s = skt_add_vert(skt,  r_outer, 0.0);
    int v_out_c = skt_add_vert(skt,  0.0,     0.0);
    skt_add_circle(skt, v_out_s, v_out_c, r_outer);
    int v_in_s  = skt_add_vert(skt,  r_inner, 0.0);
    skt_add_circle(skt, v_in_s, v_out_c, r_inner);

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

/* Simple C-shape (outer arc + inner arc + two closing lines) */
static struct directory *
sketch_create_C_shape(struct rt_wdb *wdbp, const char *name, fastf_t r)
{
    struct rt_sketch_internal *skt = skt_new();
    fastf_t ang0 = -150.0 * M_PI / 180.0;
    fastf_t ang1 =  150.0 * M_PI / 180.0;
    fastf_t r2   = r * 0.65;

    int vo0 = skt_add_vert(skt, r  * cos(ang0), r  * sin(ang0));
    int vo1 = skt_add_vert(skt, r  * cos(ang1), r  * sin(ang1));
    int vi0 = skt_add_vert(skt, r2 * cos(ang0), r2 * sin(ang0));
    int vi1 = skt_add_vert(skt, r2 * cos(ang1), r2 * sin(ang1));

    skt_add_arc( skt, vo0, vo1, r,  0, 0);
    skt_add_line(skt, vo1, vi1);
    skt_add_arc( skt, vi1, vi0, r2, 0, 1);
    skt_add_line(skt, vi0, vo0);

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

/* Gear-tooth profile (N teeth around a circle) */
static struct directory *
sketch_create_gear(struct rt_wdb *wdbp, const char *name,
		   fastf_t r_base, fastf_t r_tooth, int nteeth)
{
    struct rt_sketch_internal *skt = skt_new();

    for (int t = 0; t < nteeth; t++) {
	fastf_t a0 = 2.0 * M_PI * t / (fastf_t)nteeth;
	fastf_t a1 = 2.0 * M_PI * (t + 0.4) / (fastf_t)nteeth;
	fastf_t a2 = 2.0 * M_PI * (t + 0.6) / (fastf_t)nteeth;
	fastf_t a3 = 2.0 * M_PI * (t + 1.0) / (fastf_t)nteeth;

	int vb0 = skt_add_vert(skt, r_base  * cos(a0), r_base  * sin(a0));
	int vb1 = skt_add_vert(skt, r_base  * cos(a1), r_base  * sin(a1));
	int vt0 = skt_add_vert(skt, r_tooth * cos(a1), r_tooth * sin(a1));
	int vt1 = skt_add_vert(skt, r_tooth * cos(a2), r_tooth * sin(a2));
	int vb2 = skt_add_vert(skt, r_base  * cos(a2), r_base  * sin(a2));
	int vb3 = skt_add_vert(skt, r_base  * cos(a3), r_base  * sin(a3));

	fastf_t arc_r = r_base * 2.0 * sin(M_PI * 0.4 / (fastf_t)nteeth) * 0.6
			+ r_base * 0.05;

	skt_add_arc( skt, vb0, vb1, arc_r, 0, 0);
	skt_add_line(skt, vb1, vt0);
	skt_add_line(skt, vt0, vt1);
	skt_add_line(skt, vt1, vb2);
	skt_add_arc( skt, vb2, vb3, arc_r, 0, 0);
    }

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

/* Multi-contour: outer square + inner circle */
static struct directory *
sketch_create_multi_contour(struct rt_wdb *wdbp, const char *name)
{
    struct rt_sketch_internal *skt = skt_new();
    fastf_t s = 80.0, r = 25.0;
    int v0  = skt_add_vert(skt, -s, -s);
    int v1  = skt_add_vert(skt,  s, -s);
    int v2  = skt_add_vert(skt,  s,  s);
    int v3  = skt_add_vert(skt, -s,  s);
    skt_add_line(skt, v0, v1);
    skt_add_line(skt, v1, v2);
    skt_add_line(skt, v2, v3);
    skt_add_line(skt, v3, v0);
    int vc  = skt_add_vert(skt,  r,   0.0);
    int vcc = skt_add_vert(skt,  0.0, 0.0);
    skt_add_circle(skt, vc, vcc, r);

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0) == 0
           ? db_lookup(wdbp->dbip, name, LOOKUP_QUIET) : NULL;
}

/* Create all demo sketches; wdbp must be open and owned by caller.
 * Returns a vector of (sketch-name, directory*) pairs. */
static std::vector<std::pair<std::string, struct directory *>>
create_demo_sketches(struct rt_wdb *wdbp)
{
    std::vector<std::pair<std::string, struct directory *>> out;
    struct directory *dp;

    if ((dp = sketch_create_box(wdbp, "box", 150.0, 100.0)))
	out.push_back({"box", dp});

    if ((dp = sketch_create_rounded_rect(wdbp, "rounded_rect", 150.0, 100.0, 20.0)))
	out.push_back({"rounded_rect", dp});

    if ((dp = sketch_create_annulus(wdbp, "annulus", 60.0, 35.0)))
	out.push_back({"annulus", dp});

    if ((dp = sketch_create_C_shape(wdbp, "C_shape", 60.0)))
	out.push_back({"C_shape", dp});

    if ((dp = sketch_create_gear(wdbp, "gear", 60.0, 80.0, 8)))
	out.push_back({"gear", dp});

    if ((dp = sketch_create_multi_contour(wdbp, "multi_contour")))
	out.push_back({"multi_contour", dp});

    return out;
}

#include "qsketch.moc"

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    /* Handle --demo-sketches before creating QApplication (no display needed) */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "--demo-sketches")) {
	if (argc < 3) bu_exit(1, "qsketch: --demo-sketches requires <file.g>\n");
	const char *g_file = argv[2];

	/* Create/open the .g file; get a wdb for writing */
	struct rt_wdb *wdbp = wdb_fopen(g_file);
	if (!wdbp) bu_exit(1, "qsketch: cannot open/create '%s'\n", g_file);

	auto demos = create_demo_sketches(wdbp);
	bu_log("qsketch: created %zu demo sketches in '%s':\n",
	       demos.size(), g_file);
	for (auto &p : demos)
	    bu_log("  %s\n", p.first.c_str());
	/* wdb_close will also close the underlying dbip */
	wdb_close(wdbp);
	return 0;
    }

    /* All remaining modes need a display / QApplication */
    QApplication app(argc, argv);

    /* Parse remaining command line:
     *   qsketch <file.g> <sketch_name>
     *   qsketch --screenshot <outfile.png> <file.g> <sketch_name>
     */
    const char *screenshot_file = NULL;
    int arg_start = 1;
    if (argc > 1 && BU_STR_EQUAL(argv[1], "--screenshot")) {
	if (argc < 3) bu_exit(1, "qsketch: --screenshot requires an output file\n");
	screenshot_file = argv[2];
	arg_start = 3;
    }

    if ((argc - arg_start) != 2) {
	bu_exit(1,
	    "Usage: qsketch [--screenshot <out.png>] <file.g> <sketch_name>\n"
	    "       qsketch --demo-sketches <file.g>\n"
	    "\n"
	    "Opens or creates a sketch primitive in the given .g file\n"
	    "and presents an interactive Qt editing window.\n"
	    "\n"
	    "--screenshot <out.png>  Save a screenshot and exit (headless capture)\n"
	    "--demo-sketches         Create pre-built demo sketches and exit (no display)\n");
    }

    const char *g_file  = argv[arg_start];
    const char *sk_name = argv[arg_start + 1];

    /* ---- open / create database ---- */
    struct db_i *dbip = db_open(g_file, DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	struct rt_wdb *wdbp = wdb_fopen(g_file);
	if (!wdbp)
	    bu_exit(1, "qsketch: cannot open or create '%s'\n", g_file);
	wdb_close(wdbp);
	dbip = db_open(g_file, DB_OPEN_READWRITE);
    }
    if (dbip == DBI_NULL)
	bu_exit(1, "qsketch: failed to open '%s'\n", g_file);

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "qsketch: db_dirbuild failed\n");

    struct directory *dp = db_lookup(dbip, sk_name, LOOKUP_QUIET);
    if (!dp) {
	bu_log("qsketch: '%s' not found — creating empty sketch.\n", sk_name);
	dp = sketch_create_empty(dbip, sk_name);
	if (!dp)
	    bu_exit(1, "qsketch: failed to create sketch '%s'\n", sk_name);
    }

    if (dp->d_minor_type != ID_SKETCH) {
	bu_exit(1, "qsketch: '%s' exists but is not a sketch (type %d)\n",
		sk_name, dp->d_minor_type);
    }

    QSketchEditWindow win(dbip, dp);
    win.show();

    if (screenshot_file) {
	/* Headless screenshot: render directly from swrast DM buffer.
	 * Wait for the initial fit-view timer (singleShot(0)) to fire and
	 * for Qt to finish laying out the window before we capture. */
	static const int SCREENSHOT_DELAY_MS = 500;
	const char *ssfile = screenshot_file;
	QTimer::singleShot(SCREENSHOT_DELAY_MS, &app, [&app, &win, ssfile]() {
	    QCoreApplication::processEvents();
	    win.screenshot_to_file(QString::fromUtf8(ssfile));
	    bu_log("qsketch: screenshot saved to '%s'\n", ssfile);
	    app.quit();
	});
    }

    int ret = app.exec();

    db_close(dbip);
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
