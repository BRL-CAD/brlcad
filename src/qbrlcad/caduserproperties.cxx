#include <QPainter>
#include <QString>
#include "caduserproperties.h"
#include "cadapp.h"
#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"

// *********** Node **************

CADUserPropertiesNode::CADUserPropertiesNode(CADUserPropertiesNode *aParent)
: parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
}

CADUserPropertiesNode::~CADUserPropertiesNode()
{
    qDeleteAll(children);
}

// *********** Model **************

CADUserPropertiesModel::CADUserPropertiesModel(QObject *parentobj, struct db_i *dbip, struct directory *dp)
    : QAbstractItemModel(parentobj)
{
    current_dbip = dbip;
    current_dp = dp;
    m_root = new CADUserPropertiesNode();
    BU_GET(avs, struct bu_attribute_value_set);
    bu_avs_init_empty(avs);
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	update(dbip, dp);
    }
}

CADUserPropertiesModel::~CADUserPropertiesModel()
{
    delete m_root;
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

QVariant
CADUserPropertiesModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (section == 0) return QString("Property");
    if (section == 1) return QString("Value");
    return QVariant();
}

QVariant
CADUserPropertiesModel::data(const QModelIndex & idx, int role) const
{
    if (!idx.isValid()) return QVariant();
    CADUserPropertiesNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole && idx.column() == 0) return QVariant(curr_node->name);
    if (role == Qt::DisplayRole && idx.column() == 1) return QVariant(curr_node->value);
    //if (role == DirectoryInternalRole) return QVariant::fromValue((void *)(curr_node->node_dp));
    return QVariant();
}

bool
CADUserPropertiesModel::setData(const QModelIndex & idx, const QVariant & value, int role)
{
    if (!idx.isValid()) return false;
    QVector<int> roles;
    bool ret = false;
    CADUserPropertiesNode *curr_node = IndexNode(idx);
    if (role == Qt::DisplayRole) {
	curr_node->name = value.toString();
	roles.append(Qt::DisplayRole);
	ret = true;
    }
    if (ret) emit dataChanged(idx, idx, roles);
    return ret;
}

void CADUserPropertiesModel::setRootNode(CADUserPropertiesNode *root)
{
    m_root = root;
    beginResetModel();
    endResetModel();
}

QModelIndex CADUserPropertiesModel::index(int row, int column, const QModelIndex &parent_idx) const
{
    if (hasIndex(row, column, parent_idx)) {
	CADUserPropertiesNode *cnode = IndexNode(parent_idx)->children.at(row);
	return createIndex(row, column, cnode);
    }
    return QModelIndex();
}

QModelIndex CADUserPropertiesModel::parent(const QModelIndex &child) const
{
    CADUserPropertiesNode *pnode = IndexNode(child)->parent;
    if (pnode == m_root) return QModelIndex();
    return createIndex(NodeRow(pnode), 0, pnode);
}

int CADUserPropertiesModel::rowCount(const QModelIndex &parent_idx) const
{
    return IndexNode(parent_idx)->children.count();
}

int CADUserPropertiesModel::columnCount(const QModelIndex &parent_idx) const
{
    Q_UNUSED(parent_idx);
    return 2;
}

QModelIndex CADUserPropertiesModel::NodeIndex(CADUserPropertiesNode *node) const
{
    if (node == m_root) return QModelIndex();
    return createIndex(NodeRow(node), 0, node);
}

CADUserPropertiesNode * CADUserPropertiesModel::IndexNode(const QModelIndex &idx) const
{
    if (idx.isValid()) {
	return static_cast<CADUserPropertiesNode *>(idx.internalPointer());
    }
    return m_root;
}

int CADUserPropertiesModel::NodeRow(CADUserPropertiesNode *node) const
{
    return node->parent->children.indexOf(node);
}

bool CADUserPropertiesModel::canFetchMore(const QModelIndex &idx) const
{
    Q_UNUSED(idx);
    return false;
}

CADUserPropertiesNode *
CADUserPropertiesModel::add_attribute(const char *name, const char *value, CADUserPropertiesNode *curr_node)
{
    CADUserPropertiesNode *new_node = new CADUserPropertiesNode(curr_node);
    new_node->name = name;
    new_node->value = value;
    QModelIndex idx = NodeIndex(new_node);
    return new_node;
}

void CADUserPropertiesModel::fetchMore(const QModelIndex &idx)
{
    Q_UNUSED(idx);
    return;
}

bool CADUserPropertiesModel::hasChildren(const QModelIndex &idx) const
{
    CADUserPropertiesNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    return false;
}

int CADUserPropertiesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dp = new_dp;
    current_dbip = new_dbip;
    if (current_dbip != DBI_NULL && current_dp != RT_DIR_NULL) {
	m_root = new CADUserPropertiesNode();
	beginResetModel();
	struct bu_attribute_value_pair *avpp;
	bu_avs_free(avs);
	bu_avs_init_empty(avs);
	(void)db5_get_attributes(current_dbip, avs, current_dp);
	for (BU_AVS_FOR(avpp, avs)) {
	    if (!db5_is_standard_attribute(avpp->name)) {
		add_attribute(avpp->name, avpp->value, m_root);
	    }
	}
	endResetModel();
    } else {
	m_root = new CADUserPropertiesNode();
	beginResetModel();
	endResetModel();
    }
    return 0;
}

void
CADUserPropertiesModel::refresh(const QModelIndex &idx)
{
    current_dbip = ((CADApp *)qApp)->dbip();
    current_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
    update(current_dbip, current_dp);
}

// *********** View **************
void GUserPropertyDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString text = index.data().toString();
    painter->drawText(option.rect, text, QTextOption(Qt::AlignLeft));
}

QSize GUserPropertyDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    return name_size;
}


CADUserPropertiesView::CADUserPropertiesView(QWidget *pparent) : QTreeView(pparent)
{
    //this->setContextMenuPolicy(Qt::CustomContextMenu);
    setRootIsDecorated(false);
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

