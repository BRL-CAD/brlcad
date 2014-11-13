#ifndef CAD_USER_PROPERTIES_H
#define CAD_USER_PROPERTIES_H

#include <QVariant>
#include <QString>
#include <QList>
#include <QImage>
#include <QAbstractItemModel>
#include <QObject>
#include <QTreeView>
#include <QModelIndex>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QUrl>

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADUserPropertiesNode
{
public:
    CADUserPropertiesNode(CADUserPropertiesNode *aParent=0);
    ~CADUserPropertiesNode();

    QString name;
    QString value;
    int attr_type;

    CADUserPropertiesNode *parent;
    QList<CADUserPropertiesNode*> children;
};

class CADUserPropertiesModel : public QAbstractItemModel
{
    Q_OBJECT

    public:  // "standard" custom tree model functions
	explicit CADUserPropertiesModel(QObject *parent = 0, struct db_i *dbip = DBI_NULL, struct directory *dp = RT_DIR_NULL);
	~CADUserPropertiesModel();

	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &child) const;
	int columnCount(const QModelIndex &child) const;

	void setRootNode(CADUserPropertiesNode *root);
	CADUserPropertiesNode* rootNode();

	bool hasChildren(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	CADUserPropertiesNode *m_root;
	QModelIndex NodeIndex(CADUserPropertiesNode *node) const;
	CADUserPropertiesNode *IndexNode(const QModelIndex &index) const;

    public:  // BRL-CAD specific operations
	int update(struct db_i *new_dbip, struct directory *dp);

    public slots:
	void refresh(const QModelIndex &idx);

    protected:

	int NodeRow(CADUserPropertiesNode *node) const;
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

    private:
	CADUserPropertiesNode *add_attribute(const char *name, const char *value, CADUserPropertiesNode *curr_node);
	struct db_i *current_dbip;
	struct directory *current_dp;
	struct bu_attribute_value_set *avs;
};

class GUserPropertyDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
	GUserPropertyDelegate(QWidget *pparent = 0) : QStyledItemDelegate(pparent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class CADUserPropertiesView : public QTreeView
{
    Q_OBJECT

    public:
	CADUserPropertiesView(QWidget *pparent);
	~CADUserPropertiesView() {};
};

#endif /*CAD_USER_PROPERTIES_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

