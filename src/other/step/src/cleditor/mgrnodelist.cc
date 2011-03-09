
/*
* NIST STEP Editor Class Library
* cleditor/mgrnodelist.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: mgrnodelist.cc,v 3.0.1.2 1997/11/05 22:11:39 sauderd DP3.1 $ */ 

#include <mgrnode.h>
#include <mgrnodelist.h>
#include <dispnode.h>
#include <dispnodelist.h>


MgrNodeList::MgrNodeList(stateEnum type) : GenNodeList(new MgrNode())
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNodeList::MgrNodeList()\n";
    listType = type;
    ((MgrNode *)head)->currState = type;
}

void MgrNodeList::Remove(GenericNode *node)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNodeList::Remove()\n";
    GenNodeList::Remove(node);
// DON'T DO THIS    ((MgrNode *)node)->currState = noStateSE;
}

	// deletes node from its previous list & appends...
	// actually it puts it at the front of the list.
void MgrNodeList::Append(GenericNode *node)
{
    InsertBefore(node, head);
}

		// deletes newNode from its previous list & inserts after
		//	existNode
void MgrNodeList::InsertAfter(GenericNode *newNode, 
				     GenericNode *existNode)
{
    if(newNode->next != 0){	// remove the node from its previous list
	newNode->Remove();
    }
    GenNodeList::InsertAfter(newNode, existNode);
// DON'T DO THIS    ((MgrNode *)newNode)->currState = listType;
}

		// deletes newNode from its previous list & inserts before
		//	existNode
void MgrNodeList::InsertBefore(GenericNode *newNode, 
				      GenericNode *existNode)
{
    if(newNode->next != 0){	// remove the node from its previous 
	newNode->Remove();	//	state list
    }
    GenNodeList::InsertBefore(newNode, existNode);
// DON'T DO THIS!!    ((MgrNode *)newNode)->currState = listType;
}

MgrNode *MgrNodeList::FindFileId(int fileId)
{
    MgrNode *mn = (MgrNode *)head->next;
    while(mn != head)
    {
	if(mn->GetFileId() == fileId)
	    return mn;
	mn = (MgrNode *)mn->next;
    }
    return (MgrNode *)0;
}

