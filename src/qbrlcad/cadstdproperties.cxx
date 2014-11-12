#include <QPainter>
#include <QString>
#include "cadstdproperties.h"
#include "cadapp.h"
#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"

// *********** Node **************

CADStdPropertiesNode::CADStdPropertiesNode(CADStdPropertiesNode *aParent)
: parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
}

CADStdPropertiesNode::~CADStdPropertiesNode()
{
    qDeleteAll(children);
}

// *********** Model **************

CADStdPropertiesModel::CADStdPropertiesModel(QObject *parentobj, struct db_i *dbip, struct directory *dp)
    : QAbstractItemModel(parentobj)
{
    int i = 0;
    current_dbip = dbip;
    current_dp = dp;
    m_root = new CADStdPropertiesNode();
    BU_GET(avs, struct bu_attribute_value_set);
    bu_avs_init_empty(avs);
    while (i != ATTR_NULL) {
	add_attribute(db5_standard_attribute(i), "", m_root, i);
	i++;
    }
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	update(dbip, dp);
    }
}

CADStdPropertiesModel::~CADStdPropertiesModel()
{
    delete m_root;
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

QVariant
CADStdPropertiesModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (section == 0) return QString("Property");
    if (section == 1) return QString("Value");
    return QVariant();
}

QVariant
CADStdPropertiesModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    CADStdPropertiesNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole && idx.column() == 0) return QVariant(curr_node->name);
    if (role == Qt::DisplayRole && idx.column() == 1) return QVariant(curr_node->value);
    //if (role == DirectoryInternalRole) return QVariant::fromValue((void *)(curr_node->node_dp));
    return QVariant();
}

bool
CADStdPropertiesModel::setData(const QModelIndex & idx, const QVariant & value, int role)
{
    if (!idx.isValid()) return false;
    QVector<int> roles;
    bool ret = false;
    CADStdPropertiesNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole) {
	curr_node->name = value.toString();
	roles.append(Qt::DisplayRole);
	ret = true;
    }
    if (ret) emit dataChanged(idx, idx, roles);
    return ret;
}

void CADStdPropertiesModel::setRootNode(CADStdPropertiesNode *root)
{
    m_root = root;
    beginResetModel();
    endResetModel();
}

QModelIndex CADStdPropertiesModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (hasIndex(row, column, parent_idx)) {
	CADStdPropertiesNode *cnode = IndexNode(parent_idx)->children.at(row);
	return createIndex(row, column, cnode);
    }
    return QModelIndex();
}

QModelIndex CADStdPropertiesModel::parent(const QModelIndex &child) const
{
    CADStdPropertiesNode *pnode = IndexNode(child)->parent;
    if (pnode == m_root) return QModelIndex();
    return createIndex(NodeRow(pnode), 0, pnode);
}

int CADStdPropertiesModel::rowCount(const QModelIndex &parent_idx) const
{
    return IndexNode(parent_idx)->children.count();
}

int CADStdPropertiesModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 2;
}

QModelIndex CADStdPropertiesModel::NodeIndex(CADStdPropertiesNode *node) const
{
    if (node == m_root) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

CADStdPropertiesNode * CADStdPropertiesModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	return static_cast<CADStdPropertiesNode *>(idx.internalPointer());
    }
    return m_root;
}

int CADStdPropertiesModel::NodeRow(CADStdPropertiesNode *node) const
{
    return node->parent->children.indexOf(node);
}


int
attr_children(const char *attr)
{
    if (BU_STR_EQUAL(attr, "color")) return 3;
    return 0;
}


bool CADStdPropertiesModel::canFetchMore(const QModelIndex &idx) const
{
    CADStdPropertiesNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return false;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

CADStdPropertiesNode *
CADStdPropertiesModel::add_attribute(const char *name, const char *value, CADStdPropertiesNode *curr_node, int type)
{
    CADStdPropertiesNode *new_node = new CADStdPropertiesNode(curr_node);
    new_node->name = name;
    new_node->value = value;
    new_node->attr_type = type;
    QModelIndex idx = NodeIndex(new_node);
    return new_node;
}

void
CADStdPropertiesModel::add_Children(const char *name, CADStdPropertiesNode *curr_node)
{
    if (BU_STR_EQUAL(name, "color")) {
	QString val(bu_avs_get(avs, name));
	QStringList vals = val.split(QRegExp("/"));
	(void)add_attribute("r", vals.at(0).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_attribute("g", vals.at(1).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_attribute("b", vals.at(2).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	return;
    }
    (void)add_attribute(name, bu_avs_get(avs, name), curr_node, db5_standardize_attribute(name));
}


void CADStdPropertiesModel::fetchMore(const QModelIndex &idx)
{
    CADStdPropertiesNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	add_Children(curr_node->name.toLocal8Bit(),curr_node);
	endInsertRows();
    }
}

bool CADStdPropertiesModel::hasChildren(const QModelIndex &idx) const
{
    CADStdPropertiesNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

int CADStdPropertiesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dp = new_dp;
    current_dbip = new_dbip;
    if (current_dbip != DBI_NULL && current_dp != RT_DIR_NULL) {
	QMap<QString, CADStdPropertiesNode*> standard_nodes;
	int i = 0;
	m_root = new CADStdPropertiesNode();
	beginResetModel();
	struct bu_attribute_value_pair *avpp;
	for (BU_AVS_FOR(avpp, avs)) {
	    bu_avs_remove(avs, avpp->name);
	}
	(void)db5_get_attributes(current_dbip, avs, current_dp);
	while (i != ATTR_NULL) {
	    standard_nodes.insert(db5_standard_attribute(i), add_attribute(db5_standard_attribute(i), "", m_root, i));
	    i++;
	}
	for (BU_AVS_FOR(avpp, avs)) {
	    if (db5_is_standard_attribute(avpp->name)) {
		if (standard_nodes.find(avpp->name) != standard_nodes.end()) {
		    QString new_value(avpp->value);
		    CADStdPropertiesNode *snode = standard_nodes.find(avpp->name).value();
		    snode->value = new_value;
		} else {
		    add_attribute(avpp->name, avpp->value, m_root, db5_standardize_attribute(avpp->name));
		}
	    }
	}
	endResetModel();
    } else {
	m_root = new CADStdPropertiesNode();
	beginResetModel();
	endResetModel();
    }
    return 0;
}

void
CADStdPropertiesModel::refresh(const QModelIndex &idx)
{
    current_dbip = ((CADApp *)qApp)->dbip();
    current_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
    update(current_dbip, current_dp);
}

// *********** View **************
void GStdPropertyDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString text = index.data().toString();
    painter->drawText(option.rect, text, QTextOption(Qt::AlignLeft));
}

QSize GStdPropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    return name_size;
}


CADStdPropertiesView::CADStdPropertiesView(QWidget *pparent) : QTreeView(pparent)
{
    //this->setContextMenuPolicy(Qt::CustomContextMenu);
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

