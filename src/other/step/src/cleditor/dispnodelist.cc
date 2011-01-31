
/*
* NIST STEP Editor Class Library
* cleditor/dispnodelist.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: dispnodelist.cc,v 3.0.1.2 1997/11/05 22:11:40 sauderd DP3.1 $ */ 

#include <gennode.h>
#include <gennodelist.h>

#include <mgrnode.h>
#include <mgrnodelist.h>
#include <dispnode.h>
#include <dispnodelist.h>


void DisplayNodeList::Remove(GenericNode *node)
{
    GenNodeList::Remove(node);
// DON'T DO THIS    ((DisplayNode *)node)->displayState = noMapState;
}

	// deletes node from its previous list & appends
	// actually it puts it at the front of the list.
void DisplayNodeList::Append(GenericNode *node)
{
    InsertBefore(node, head);
}

		// deletes newNode from its previous list & inserts after
		//	existNode
void DisplayNodeList::InsertAfter(GenericNode *newNode, 
					 GenericNode *existNode)
{
    if(newNode->next != 0){	// remove the node from its previous 
	newNode->Remove();	//	display state list
    }
    GenNodeList::InsertAfter(newNode, existNode);
// DON'T DO THIS    ((DisplayNode *)newNode)->displayState = listType;
}

		// deletes newNode from its previous list & inserts before
		//	existNode
void DisplayNodeList::InsertBefore(GenericNode *newNode,
					  GenericNode *existNode)
{
    if(newNode->next != 0){	// remove the node from its previous 
	newNode->Remove();	//	display state list
    }
    GenNodeList::InsertBefore(newNode, existNode);
// DON'T DO THIS!!!   ((DisplayNode *)newNode)->displayState = listType;
}

