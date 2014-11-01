#include "cadtreenode.h"

CADTreeNode::CADTreeNode(struct directory *in_dp, CADTreeNode *aParent)
    : node_dp(in_dp), parent(aParent)
{
    if(parent) {
        parent->children.append(this);
    }
}

CADTreeNode::CADTreeNode(QString dp_name, CADTreeNode *aParent)
    : name(dp_name), parent(aParent)
{
    node_dp = RT_DIR_NULL;
    if(parent) {
        parent->children.append(this);
    }
}


CADTreeNode::~CADTreeNode()
{
    qDeleteAll(children);
}

QVariant CADTreeNode::data() const {
    //return QString(node_dp->d_namep);
    return name;
}

