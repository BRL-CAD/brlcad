#ifndef CAD_TREEMODEL_H
#define CAD_TREEMODEL_H

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QtCore>
#undef Success
#include <QAbstractItemModel>
#undef Success
#include <QObject>
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#ifndef Q_MOC_RUN
#include "bu/file.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADTreeNode;

class CADTreeModel : public QAbstractItemModel
{
    Q_OBJECT

    public:
	explicit CADTreeModel(QObject *parent = 0);
	~CADTreeModel();

	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;
	int rowCount(const QModelIndex &child) const;
	int columnCount(const QModelIndex &child) const;

	QVariant data(const QModelIndex &index, int role) const;

	void setRootNode(CADTreeNode *root);
	CADTreeNode* rootNode();

	int populate(struct db_i *new_dbip);
	bool hasChildren(const QModelIndex &parent) const;

    public slots:
	void refresh();

    protected:

	CADTreeNode *m_root;
	QModelIndex NodeIndex(CADTreeNode *node) const;
	CADTreeNode *IndexNode(const QModelIndex &index) const;
	int NodeRow(CADTreeNode *node) const;
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

    private:
	struct db_i *current_dbip;
};

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

