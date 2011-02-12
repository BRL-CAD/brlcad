
/*
* NIST STEP Editor Class Library
* cleditor/mgrnode.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: mgrnode.cc,v 3.0.1.3 1997/11/05 22:11:37 sauderd DP3.1 $ */ 

#include <mgrnode.h>
#include <mgrnodelist.h>
#include <dispnode.h>
#include <dispnodelist.h>

#include <instmgr.h>
//#include <STEPentity.h>
#include <sdai.h>

#include <iostream>

void *MgrNode::SEE()
{
    return (di ? di->SEE() : 0);
}

int MgrNode::GetFileId()
{
    return (se ? se->GetFileId() : -1);
}

void MgrNode::Remove()
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::Remove()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    GenericNode::Remove();
// DON'T DO THIS!!    currState = noStateSE;
}

	// searches current list for fileId
MgrNode *MgrNode::StateFindFileId(int fileId)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::StateFindFileId()\n";
    MgrNode *startNode = this;
    if(startNode->GetFileId() == fileId) return this;
    else
    {
		// mn is really a MgrNode
	MgrNode *mn = (MgrNode *)(startNode->Next());
	while(mn != startNode)
	{
	    if( mn->GetFileId() == fileId)
		return (MgrNode *)mn;
	    mn = ((MgrNode *)mn->Next());
	}
	return (MgrNode *)0;
    }
}

MgrNode::~MgrNode()
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::~MgrNode()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    if(se)
	delete se;
    if(di)
	delete di;
//    GenericNode::Remove(); // this is called by default.
}

///////////////////// class MgrNode Display Functions /////////////////////////

displayStateEnum MgrNode::DisplayState() 
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::DisplayState()\n";
    return (di ? di->DisplayState() : noMapState);
}

int MgrNode::IsDisplayState(displayStateEnum ds)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::IsDisplayState()\n";
    return (di ? di->DisplayListMember(ds) : 0);
}

GenericNode *MgrNode::NextDisplay()
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::NextDisplay()\n";
//    return (di ? ((DisplayNode *)di->Next()) : (DisplayNode *)0);
    if(di)
    {
//	GenericNode *dn = di->Next();
//	return (DisplayNode *)dn;
//    	return (DisplayNode *)(di->Next());
    	return di->Next();
    }
    else
	return 0;
}

GenericNode *MgrNode::PrevDisplay()
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::PrevDisplay()\n";
//    return (di ? ((DisplayNode *)di->Prev()) : 0);
    if(di)
	return di->Prev();
    else
	return 0;
}

// STATE LIST OPERATIONS

// deletes from previous cmd list & puts on cmd list cmdList
int MgrNode::ChangeList(DisplayNodeList *cmdList)
{
    if(!di)
	di = new class DisplayNode(this);
    return di->ChangeList(cmdList);
}

// deletes from previous cmd list & puts on cmd list cmdList
int MgrNode::ChangeList(MgrNodeList *cmdList)
{
    Remove();
    cmdList->Append(this);
    return 1;
}

int MgrNode::ChangeState(displayStateEnum s)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::ChangeState()\n";
    if(di)
    {
	return di->ChangeState(s);
    }
    return 0;
}

int MgrNode::ChangeState(stateEnum s)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::ChangeState()\n";
    currState = s;
     // for now, later need to type check somehow and return success or failure
    return 1;
}

void MgrNode::Init(SCLP23(Application_instance) *s,
			  stateEnum listState,
			  MgrNodeList *list)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::Init()\n";
    se = s;
    arrayIndex = -1;
    di = 0;
    currState = listState;
    if(list)
    {
	list->Append(this);
    }
}

	// used for sentinel node on lists of MgrNodes
MgrNode::MgrNode()
{ 
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::MgrNode()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    Init(0, noStateSE, 0);
}

MgrNode::MgrNode(SCLP23(Application_instance) *StepEntPtr)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::MgrNode()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    Init(StepEntPtr, noStateSE, 0);
}

	// 'listState' ==
	//	completeSE - if reading valid exchange file
	//	incompleteSE or completeSE - if reading working session file
	//	newSE - if instance is created by user using editor (probe)
MgrNode::MgrNode(SCLP23(Application_instance) *StepEntPtr, stateEnum listState)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::MgrNode()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    Init(StepEntPtr, listState, 0);
}
	// 'listState' ==
	//	completeSE - if reading valid exchange file
	//	incompleteSE or completeSE - if reading working session file
	//	newSE - if instance is created by user using editor (probe)
MgrNode::MgrNode(SCLP23(Application_instance) *StepEntPtr, stateEnum listState, MgrNodeList *list)
{
//    if(debug_level >= PrintFunctionTrace)
//	cout << "MgrNode::MgrNode()\n";
//    if(debug_level >= PrintValues)
//	cout << "MgrNode::this : '" << this << "'\n";
    Init(StepEntPtr, listState, list);

}
