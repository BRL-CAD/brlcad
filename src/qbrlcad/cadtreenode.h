#ifndef CAD_TREENODE_H
#define CAD_TREENODE_H

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

class CADTreeNode
{
public:
    CADTreeNode(struct directory *in_dp = RT_DIR_NULL, CADTreeNode *aParent=0);
    CADTreeNode(QString dp_name, CADTreeNode *aParent=0);
    ~CADTreeNode();
    QVariant data() const;

    QString name;
    struct directory *node_dp;
    CADTreeNode *parent;
    QList<CADTreeNode*> children;
};

#endif //CAD_TREENODE_H

