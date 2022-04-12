#include "qtcad/Application.h"
#include "qtcad/GeometryItem.h"
#include "bv/util.h"
#include "bu/env.h"
#include "ged/commands.h"
#include "brlcad/rt/db5.h"
#include "brlcad/rt/db_internal.h"
#include "brlcad/rt/nongeom.h"
#include "brlcad/rt/geom.h"
#include "brlcad/rt/db_io.h"

#include "qtcad/QgUtil.h"

#include "ged.h"

#define G_STANDARD_OBJ 0 /** @brief no region or air flags set, or not a comb */
#define G_REGION 1       /** @brief region flag set, no air flag set */
#define G_AIR 2          /** @brief non-zero aircode set, no non-zero region_id set */
#define G_AIR_REGION 3   /** @brief both non-zero aircode and non-zero region_id set (error case) */
#define G_ASSEMBLY 4     /** @brief a comb object above one or more regions */

namespace qtcad {

GeometryItem::GeometryItem(struct db_i *dbip, struct directory *dp, mat_t transform, db_op_t op, int rowNumber, GeometryItem *parent) : itemDb(dbip), itemDirectoryPointer(dp), itemOp(op), itemRowNumber(rowNumber), itemParent(parent)
{
    for (int i = 0; i < ELEMENTS_PER_MAT; i++) {
        itemTransform[i] = transform[i];
    }
    switch (itemOp) {
    case DB_OP_UNION:
        opText = QString("u");
        break;
    case DB_OP_SUBTRACT:
        opText = QString("-");
        break;
    case DB_OP_INTERSECT:
        opText = QString("+");
        break;
    default:
        break;
    }

    itemIcon = determineIcon();
}

GeometryItem::~GeometryItem()
{
    children.clear();
    itemChildrenLoaded = 0;
}

QImage GeometryItem::icon()
{
    return itemIcon;
}

void GeometryItem::appendChild(GeometryItem *child)
{
    children.push_back(child);
}

GeometryItem *GeometryItem::child(int row)
{
    if (row < 0 || row >= (int) children.size()) {
        return nullptr;
    }
    checkChildrenLoaded();
    return children[row];
}

int GeometryItem::childCount()
{
    checkChildrenLoaded();
    return children.size();
}

void GeometryItem::checkChildrenLoaded()
{
    if (!itemChildrenLoaded) {
        loadChildren(0, 1);
        itemChildrenLoaded = 1;
    }
}

int GeometryItem::columnCount() const
{
    return 1;
}

QVariant GeometryItem::data(int column) const
{
    if (column < 0 || column >= 1) {
        return QVariant();
    }

    if (opText != nullptr) {
        return QString("%1 %2").arg(opText, itemDirectoryPointer->d_namep);
    }

    return itemDirectoryPointer->d_namep;
}

void GeometryItem::leafFunc(union tree *comb_leaf, int tree_op, int level, int maxLevel)
{
    struct directory *dp = db_lookup(itemDb, comb_leaf->tr_l.tl_name, LOOKUP_QUIET);
    db_op_t op = DB_OP_UNION;
    if (tree_op == OP_SUBTRACT) {
        op = DB_OP_SUBTRACT;
    }
    if (tree_op == OP_INTERSECT) {
        op = DB_OP_INTERSECT;
    }

    mat_t transformMatrix = MAT_INIT_IDN;
    std::string dp_name(comb_leaf->tr_l.tl_name);
    if (comb_leaf->tr_l.tl_mat) {
        MAT_COPY(transformMatrix, comb_leaf->tr_l.tl_mat);
    }

    GeometryItem *child = new GeometryItem(itemDb, dp, transformMatrix, op, children.size(), this);
    children.push_back(child);
    child->loadChildren(level, maxLevel);
}

void GeometryItem::parseComb(struct rt_comb_internal *comb, union tree *comb_tree, int op, int level, int maxLevel)
{
    switch (comb_tree->tr_op) {
    case OP_DB_LEAF:
        leafFunc(comb_tree, op, level, maxLevel);
        break;
    case OP_UNION:
    case OP_INTERSECT:
    case OP_SUBTRACT:
    case OP_XOR:
        parseComb(comb, comb_tree->tr_b.tb_left, OP_UNION, level, maxLevel);
        parseComb(comb, comb_tree->tr_b.tb_right, comb_tree->tr_op, level, maxLevel);
        break;
    default:
        bu_log("db_tree_opleaf: bad op %d\n", comb_tree->tr_op);
        bu_bomb("db_tree_opleaf: bad op\n");
        break;
    }
}


int GeometryItem::loadChildren(int level, int maxLevel)
{
    if (itemDirectoryPointer->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
        if ((level + 1) > maxLevel) {
            return 0;
        }
        struct rt_db_internal intern;
        RT_DB_INTERNAL_INIT(&intern);
        if (rt_db_get_internal(&intern, itemDirectoryPointer, itemDb, NULL, &rt_uniresource) < 0) return 0;
        if (intern.idb_type != ID_COMBINATION) {
            bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n", itemDirectoryPointer->d_namep);
            itemDirectoryPointer->d_flags &= ~RT_DIR_COMB;
            rt_db_free_internal(&intern);
            return 0;
        }
        struct rt_comb_internal *comb = (struct rt_comb_internal *) intern.idb_ptr;
        parseComb(comb, comb->tree, OP_UNION, level+1, maxLevel);
        rt_db_free_internal(&intern);

        return 1;
    }

    return 0;
}

int GeometryItem::getArbType()
{
    int type;
    const struct bn_tol arb_tol = BG_TOL_INIT;
    struct rt_db_internal intern;
    if (rt_db_get_internal(&intern, itemDirectoryPointer, itemDb, (fastf_t *) NULL, &rt_uniresource) < 0) return 0;
    type = rt_arb_std_type(&intern, &arb_tol);
    rt_db_free_internal(&intern);
    return type;
}

int GeometryItem::getCombType()
{
    struct bu_attribute_value_set avs;
    int region_flag = 0;
    int air_flag = 0;
    int region_id_flag = 0;
    int assembly_flag = 0;
    if (itemDirectoryPointer->d_flags & RT_DIR_REGION) {
        region_flag = 1;
    }

    bu_avs_init_empty(&avs);
    (void) db5_get_attributes(itemDb, &avs, itemDirectoryPointer);
    const char *airval = bu_avs_get(&avs, "aircode");
    if (airval && !BU_STR_EQUAL(airval, "0")) {
        air_flag = 1;
    }
    const char *region_id = bu_avs_get(&avs, "region_id");
    if (region_id && !BU_STR_EQUAL(region_id, "0")) {
        region_id_flag = 1;
    }

    if (!region_flag && !air_flag) {
        assembly_flag = 1; // This is probably not right but I don't want to parse the whole structure to find it
    }

    if (region_flag && !air_flag) return G_REGION;
    if (!region_id_flag && air_flag) return G_AIR;
    if (region_id_flag && air_flag) return G_AIR_REGION;
    if (assembly_flag) return G_ASSEMBLY;

    return G_STANDARD_OBJ;
}

QImage GeometryItem::determineIcon()
{
    int type;
    switch (itemDirectoryPointer->d_minor_type) {
    case DB5_MINORTYPE_BRLCAD_TOR:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_TOR");
        break;
    case DB5_MINORTYPE_BRLCAD_TGC:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_TGC");
        break;
    case DB5_MINORTYPE_BRLCAD_ELL:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ELL");
        break;
    case DB5_MINORTYPE_BRLCAD_ARB8:
        type = getArbType();
        switch (type) {
        case 4:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARB4");
            break;
        case 5:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARB5");
            break;
        case 6:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARB6");
            break;
        case 7:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARB7");
            break;
        case 8:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARB8");
            break;
        default:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_OTHER");
            break;
        }
        break;
    case DB5_MINORTYPE_BRLCAD_ARS:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARS");
        break;
    case DB5_MINORTYPE_BRLCAD_HALF:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_HALF");
        break;
    case DB5_MINORTYPE_BRLCAD_REC:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_REC");
        break;
    case DB5_MINORTYPE_BRLCAD_POLY:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_POLY");
        break;
    case DB5_MINORTYPE_BRLCAD_BSPLINE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_BSPLINE");
        break;
    case DB5_MINORTYPE_BRLCAD_SPH:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_SPH");
        break;
    case DB5_MINORTYPE_BRLCAD_NMG:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_NMG");
        break;
    case DB5_MINORTYPE_BRLCAD_EBM:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_EBM");
        break;
    case DB5_MINORTYPE_BRLCAD_VOL:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_VOL");
        break;
    case DB5_MINORTYPE_BRLCAD_ARBN:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ARBN");
        break;
    case DB5_MINORTYPE_BRLCAD_PIPE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_PIPE");
        break;
    case DB5_MINORTYPE_BRLCAD_PARTICLE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_PARTICLE");
        break;
    case DB5_MINORTYPE_BRLCAD_RPC:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_RPC");
        break;
    case DB5_MINORTYPE_BRLCAD_RHC:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_RHC");
        break;
    case DB5_MINORTYPE_BRLCAD_EPA:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_EPA");
        break;
    case DB5_MINORTYPE_BRLCAD_EHY:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_EHY");
        break;
    case DB5_MINORTYPE_BRLCAD_ETO:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ETO");
        break;
    case DB5_MINORTYPE_BRLCAD_GRIP:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_GRIP");
        break;
    case DB5_MINORTYPE_BRLCAD_JOINT:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_JOINT");
        break;
    case DB5_MINORTYPE_BRLCAD_HF:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_HF");
        break;
    case DB5_MINORTYPE_BRLCAD_DSP:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_DSP");
        break;
    case DB5_MINORTYPE_BRLCAD_SKETCH:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_SKETCH");
        break;
    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_EXTRUDE");
        break;
    case DB5_MINORTYPE_BRLCAD_SUBMODEL:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_SUBMODEL");
        break;
    case DB5_MINORTYPE_BRLCAD_CLINE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_CLINE");
        break;
    case DB5_MINORTYPE_BRLCAD_BOT:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_BOT");
        break;
    case DB5_MINORTYPE_BRLCAD_COMBINATION:
        type = getCombType();
        switch (type) {
        case G_REGION:
            return Application::getImageCache()->getImage("G_REGION");
            break;
        case G_AIR:
            return Application::getImageCache()->getImage("return");
            break;
        case G_AIR_REGION:
            return Application::getImageCache()->getImage("G_AIR_REGION");
            break;
        case G_ASSEMBLY:
            return Application::getImageCache()->getImage("G_ASSEMBLY");
            break;
        default:
            return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_OTHER");
            break;
        }
        break;
    case DB5_MINORTYPE_BRLCAD_SUPERELL:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_SUPERELL");
        break;
    case DB5_MINORTYPE_BRLCAD_METABALL:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_METABALL");
        break;
    case DB5_MINORTYPE_BRLCAD_BREP:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_BREP");
        break;
    case DB5_MINORTYPE_BRLCAD_HYP:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_HYP");
        break;
    case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_CONSTRAINT");
        break;
    case DB5_MINORTYPE_BRLCAD_REVOLVE:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_REVOLVE");
        break;
    case DB5_MINORTYPE_BRLCAD_ANNOT:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_ANNOT");
        break;
    case DB5_MINORTYPE_BRLCAD_HRT:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_HRT");
        break;
    default:
        return Application::getImageCache()->getImage("DB5_MINORTYPE_BRLCAD_OTHER");
        break;
    }

}

} // namespace qtcad
