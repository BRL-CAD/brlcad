#include "cadtreemodel.h"
#include "cadtreeview.h"

#include <iostream>
#include <QPainter>
#include <qmath.h>
#include "raytrace.h"
#include "cadapp.h"

HIDDEN int
get_arb_type(struct directory *dp, struct db_i *dbip)
{
    int type;
    const struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return 0;
    type = rt_arb_std_type(&intern, &arb_tol);
    rt_db_free_internal(&intern);
    return type;
}

HIDDEN int
get_comb_type(struct directory *dp, struct db_i *dbip)
{
    struct bu_attribute_value_set avs;
    int region_flag = 0;
    int air_flag = 0;
    int assembly_flag = 0;
    if (dp->d_flags & RT_DIR_REGION) {
	region_flag = 1;
    }

    bu_avs_init_empty(&avs);
    (void)db5_get_attributes(dbip, &avs, dp);
    const char *airval = bu_avs_get(&avs, "aircode");
    if (airval && !BU_STR_EQUAL(airval, "0")) {
	air_flag = 1;
    }

    const char *assembly_search = "-depth 0 -type comb -above -type region ! -type region";
    int search_results = db_search(NULL, DB_SEARCH_QUIET, assembly_search, 1, &dp, dbip);
    //std::cout << "search results(" << dp->d_namep << "): " << search_results << "\n";
    if (search_results) assembly_flag = 1;

    if (region_flag && !air_flag) return 2;
    if (!region_flag && air_flag) return 3;
    if (region_flag && air_flag) return 4;
    if (assembly_flag) return 5;

    return 0;
}

void GObjectDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
        QString text = index.data().toString();
	int bool_op = index.data(BoolInternalRole).toInt();
	int type;
	switch (bool_op) {
	    case OP_UNION:
		text.prepend("u ");
		break;
	    case OP_INTERSECT:
		text.prepend("  + ");
		break;
	    case OP_SUBTRACT:
		text.prepend("  - ");
		break;
	}
	text.prepend(" ");
	QImage raw_type_icon;
	struct db_i *dbip = ((CADApp *)qApp)->dbip();
	if (dbip != DBI_NULL) {
	    struct directory *dp = db_lookup(dbip, index.data().toString().toLocal8Bit(), LOOKUP_QUIET);
	    if (dp != RT_DIR_NULL) {
		switch(dp->d_minor_type) {
		    case DB5_MINORTYPE_BRLCAD_TOR:
			raw_type_icon.load(":/images/primitives/tor.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_TGC:
			raw_type_icon.load(":/images/primitives/tgc.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_ELL:
			raw_type_icon.load(":/images/primitives/ell.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_ARB8:
			type = get_arb_type(dp, dbip);
			switch (type) {
			    case 4:
				raw_type_icon.load(":/images/primitives/arb4.png");
				break;
			    case 5:
				raw_type_icon.load(":/images/primitives/arb5.png");
				break;
			    case 6:
				raw_type_icon.load(":/images/primitives/arb6.png");
				break;
			    case 7:
				raw_type_icon.load(":/images/primitives/arb7.png");
				break;
			    case 8:
				raw_type_icon.load(":/images/primitives/arb8.png");
				break;
			    default:
				raw_type_icon.load(":/images/primitives/other.png");
				break;
			}
			break;
		    case DB5_MINORTYPE_BRLCAD_ARS:
			raw_type_icon.load(":/images/primitives/ars.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_HALF:
			raw_type_icon.load(":/images/primitives/half.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_REC:
			raw_type_icon.load(":/images/primitives/tgc.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_POLY:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_BSPLINE:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_SPH:
			raw_type_icon.load(":/images/primitives/sph.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_NMG:
			raw_type_icon.load(":/images/primitives/nmg.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_EBM:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_VOL:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_ARBN:
			raw_type_icon.load(":/images/primitives/arbn.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_PIPE:
			raw_type_icon.load(":/images/primitives/pipe.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_PARTICLE:
			raw_type_icon.load(":/images/primitives/part.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_RPC:
			raw_type_icon.load(":/images/primitives/rpc.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_RHC:
			raw_type_icon.load(":/images/primitives/rhc.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_EPA:
			raw_type_icon.load(":/images/primitives/epa.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_EHY:
			raw_type_icon.load(":/images/primitives/ehy.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_ETO:
			raw_type_icon.load(":/images/primitives/eto.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_GRIP:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_JOINT:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_HF:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_DSP:
			raw_type_icon.load(":/images/primitives/dsp.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_SKETCH:
			raw_type_icon.load(":/images/primitives/sketch.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
			raw_type_icon.load(":/images/primitives/extrude.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_SUBMODEL:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_CLINE:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_BOT:
			raw_type_icon.load(":/images/primitives/bot.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_COMBINATION:
			type = get_comb_type(dp, dbip);
			switch (type) {
			    case 2:
				raw_type_icon.load(":/images/primitives/region.png");
				break;
			    case 3:
				raw_type_icon.load(":/images/primitives/air.png");
				break;
			    case 4:
				raw_type_icon.load(":/images/primitives/airregion.png");
				break;
			    case 5:
				raw_type_icon.load(":/images/primitives/assembly.png");
				break;
			    default:
				raw_type_icon.load(":/images/primitives/comb.png");
				break;
			}
			break;
		    case DB5_MINORTYPE_BRLCAD_SUPERELL:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_METABALL:
			raw_type_icon.load(":/images/primitives/metaball.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_BREP:
			raw_type_icon.load(":/images/primitives/brep.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_HYP:
			raw_type_icon.load(":/images/primitives/hyp.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_REVOLVE:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_ANNOTATION:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    case DB5_MINORTYPE_BRLCAD_HRT:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		    default:
			raw_type_icon.load(":/images/primitives/other.png");
			break;
		}
	    }
	}

        // rect with width proportional to value
        //QRect rect(option.rect.adjusted(4,4,-4,-4));

        // draw the value bar
        //painter->fillRect(rect, QBrush(QColor("steelblue")));
	//
        // draw text label

	QImage type_icon = raw_type_icon.scaledToHeight(option.rect.height()-2);
	QRect image_rect = type_icon.rect();
	image_rect.moveTo(option.rect.topLeft());
	QRect text_rect(type_icon.rect().topRight(), option.rect.bottomRight());
	text_rect.moveTo(image_rect.topRight());
	image_rect.translate(0, 1);
        painter->drawImage(image_rect, type_icon);

        painter->drawText(text_rect, text, QTextOption(Qt::AlignLeft));
}

QSize GObjectDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    QSize bool_size;
    int bool_op = index.data(BoolInternalRole).toInt();
    switch (bool_op) {
	case OP_UNION:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, " u ");
	    break;
	case OP_INTERSECT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   + ");
	    break;
	case OP_SUBTRACT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   - ");
	    break;
    }
    int item_width = name_size.rwidth() + bool_size.rwidth() + 18;
    int item_height = name_size.rheight();
    return QSize(item_width, item_height);
}


CADTreeView::CADTreeView(QWidget *pparent) : QTreeView(pparent)
{
}

void CADTreeView::header_state()
{
    header()->setStretchLastSection(true);
    int header_length = header()->length();
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Fixed);
    resizeColumnToContents(0);
    int column_width = columnWidth(0);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    if (column_width > header_length) {
	header()->setStretchLastSection(false);
    } else {
	header()->setStretchLastSection(true);
    }
}

void CADTreeView::resizeEvent(QResizeEvent *)
{
    header_state();
}

void CADTreeView::tree_column_size(const QModelIndex &)
{
    header_state();
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

