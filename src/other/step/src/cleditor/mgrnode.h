#ifndef mgrnode_h
#define mgrnode_h

/*
* NIST STEP Editor Class Library
* cleditor/mgrnode.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: mgrnode.h,v 3.0.1.4 1997/11/05 22:11:37 sauderd DP3.1 $ */ 

/*
#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif
*/

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

class GenericNode;
class DisplayNode;
#include <sdai.h>
//class SCLP23(Application_instance);

#include <gennode.h>
#include <gennodelist.h>
//#include <gennode.inline.h>

#include <editordefines.h>

class InstMgr;

//////////////////////////////////////////////////////////////////////////////
// class MgrNode
// If you delete this object, it deletes the SCLP23(Application_instance) it represents,
// the DisplayNode, and removes itself from any list it is in.
//////////////////////////////////////////////////////////////////////////////

class MgrNode : public GenericNode
{
    friend class GenNodeList;
    friend class MgrNodeList;
    friend class InstMgr;
    
protected:
	// currState, next, prev implement several lists
	// based on currState:
	// currState = (completeSE, incompleteSE, newSE, or deleteSE)
	// every node will be on one of the four lists implemented by these:
    stateEnum currState;

	// SCLP23(Application_instance) this node is representing info for
    SCLP23(Application_instance) *se;
	// this is the index (in the InstMgr master array) of the ptr to
	//   this node.
    int arrayIndex;

	// display info (SEE, etc) for this node
    DisplayNode *di;

public:
	// used for sentinel node on lists of MgrNodes
    MgrNode();
    MgrNode(SCLP23(Application_instance) *se);
	// 'listState' ==
	//	completeSE - if reading valid exchange file
	//	incompleteSE or completeSE - if reading working session file
	//	newSE - if instance is created by user using editor (probe)
    MgrNode(SCLP23(Application_instance) *se, stateEnum listState);
    MgrNode(SCLP23(Application_instance) *se, stateEnum listState, MgrNodeList *list);
    virtual ~MgrNode();

// STATE LIST OPERATIONS
    int MgrNodeListMember(stateEnum s)	{ return (currState == s); }
    stateEnum  CurrState()		{ return  currState;	    }
		// returns next or prev member variables
		// i.e. next or previous node on curr state list
			// searches current list for fileId
    MgrNode *StateFindFileId(int fileId);

			// deletes from previous cmd list, 
			//	& puts on cmd list cmdList
    int ChangeList(MgrNodeList *cmdList);
    int ChangeState(stateEnum s);

	// Removes from current list. 
	// 	Called before adding to diff list or when destructor is called.
    void Remove();

// DISPLAY LIST OPERATIONS
    void *SEE();

    displayStateEnum DisplayState();
    int IsDisplayState(displayStateEnum ds);

		// returns next or prev member variables
		// i.e. next or previous node on display state list
    GenericNode *NextDisplay();
    GenericNode *PrevDisplay();

			// deletes from previous cmd list, 
			//	& puts on cmd list cmdList
    int ChangeList(DisplayNodeList *cmdList);
			// deletes from previous display list, assigns ds to 
			//	displayState & puts on list dsList
    int ChangeState(displayStateEnum ds);

// 	might not want these three? since it won't actually map them?
    void MapModifiable(DisplayNodeList *dnList);
    void MapViewable(DisplayNodeList *dnList);
    void UnMap(DisplayNodeList *dnList);

// ACCESS FUNCTIONS
    int GetFileId();
    SCLP23(Application_instance) *GetApplication_instance() { return se; }
    DisplayNode *&displayNode() { return di; }
    int ArrayIndex()		{ return arrayIndex; }
    void ArrayIndex(int index)	{ arrayIndex = index; }

/*
#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
*/

    // OBSOLETE
    SCLP23(Application_instance) *GetSTEPentity()	{ return se; }
protected:

private:
    void Init(SCLP23(Application_instance) *s, stateEnum listState, MgrNodeList *list);
};

//////////////////////////////////////////////////////////////////////////////
// class MgrNode inline functions
// these functions don't rely on any inline functions (its own or
//	other classes) that aren't in this file except for Generic functions
//////////////////////////////////////////////////////////////////////////////

#endif
