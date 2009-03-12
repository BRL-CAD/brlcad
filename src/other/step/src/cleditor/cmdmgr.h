#ifndef cmdmgr_h
#define cmdmgr_h

/*
* NIST STEP Editor Class Library
* cleditor/cmdmgr.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: cmdmgr.h,v 3.0.1.2 1997/11/05 22:11:41 sauderd DP3.1 $ */ 

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <gennode.h>
#include <gennodelist.h>
//#include <gennode.inline.h>
#include <gennodearray.h>

#include <editordefines.h>
#include <mgrnode.h>
#include <mgrnodelist.h>
#include <dispnode.h>
#include <dispnodelist.h>
#include <SingleLinkList.h>

typedef unsigned short BOOLEAN;

//#define NUM_CMDMGR_CMDS 9
	// this is the number of columns that contain cmds (as opposed
	// to state info)
#define NUM_CMD_COLUMNS 3

// **** each of CMD_CHAR must be a unique char.
#define SAVE_COMPLETE_CMD_CHAR		's'
#define SAVE_COMPLETE_CMD_COL		 0
#define SAVE_COMPLETE_STATE_CHAR	' '
#define SAVE_COMPLETE_STATE_COL		 4

#define SAVE_INCOMPLETE_CMD_CHAR	'i'
#define SAVE_INCOMPLETE_CMD_COL		 0
#define SAVE_INCOMPLETE_STATE_CHAR	'I'
#define SAVE_INCOMPLETE_STATE_COL	 4

// backup to last save
//#define CANCEL_CMD_CHAR			'c'
//#define CANCEL_CMD_COL			 0

#define NEW_STATE_CHAR			'N'
#define NEW_STATE_COL			 4

#define DELETE_CMD_CHAR			'd'
#define DELETE_CMD_COL			 0
#define DELETE_STATE_CHAR		'D'
#define DELETE_STATE_COL		 4

	// close will try to save it to its previous status
#define CLOSE_CMD_CHAR			'c'
#define CLOSE_CMD_COL			 2

#define MODIFY_CMD_CHAR			'm'
#define MODIFY_CMD_COL			 2
#define MODIFY_STATE_CHAR 		'M'
#define MODIFY_STATE_COL		 3

#define VIEW_CMD_CHAR	 		'v'
#define VIEW_CMD_COL			 2
#define VIEW_STATE_CHAR			'V'
#define VIEW_STATE_COL			 3

#define REPLICATE_CMD_CHAR 		'r'
#define REPLICATE_CMD_COL		 1

#define EXECUTE_CMD_CHAR		'x'
#define EXECUTE_CMD_COL			 5

#define UNMARK_CMD_CHAR			'u'
#define UNMARK_CMD_COL			 5

///////////////////////////////////////////////////////////////////////////////

class ReplicateLinkNode : public SingleLinkNode {
  private:
  protected:
    MgrNode * _repNode;
  public:
    ReplicateLinkNode() { _repNode = 0; }
    ~ReplicateLinkNode() { }

    char *ClassName () { return "ReplicateLinkNode"; }

    MgrNode *ReplicateNode() { return _repNode; }
    void ReplicateNode(MgrNode *rn) { _repNode = rn; }
};

class ReplicateList : public SingleLinkList {
  private:
  protected:
  public:
    ReplicateList()  { }
    ~ReplicateList() { }

    virtual SingleLinkNode * NewNode () { return new ReplicateLinkNode; }

    BOOLEAN IsOnList(MgrNode *mn);
    ReplicateLinkNode *FindNode(MgrNode *mn);
    
    ReplicateLinkNode * AddNode (MgrNode * rn) { 
	ReplicateLinkNode *node = (ReplicateLinkNode *) NewNode();
	node->ReplicateNode(rn);
	SingleLinkList::AppendNode(node);
	return node;
    }    

    BOOLEAN Remove(ReplicateLinkNode *rln);
    BOOLEAN Remove(MgrNode *rn);

    char *ClassName () { return "ReplicateList"; }
};

///////////////////////////////////////////////////////////////////////////////

class CmdMgr 
{
protected:
    MgrNodeList *completeList;
    MgrNodeList *incompleteList;
    MgrNodeList *cancelList;
    MgrNodeList *deleteList;

    DisplayNodeList *mappedWriteList;
    DisplayNodeList *mappedViewList;
    DisplayNodeList *closeList;

    ReplicateList *replicateList;
  public:

    CmdMgr();

// STATE LIST OPERATIONS
    MgrNode     *GetHead(stateEnum listType);
    DisplayNode *GetHead(displayStateEnum listType);
    ReplicateLinkNode	*GetReplicateHead()
	{ return (ReplicateLinkNode *)(replicateList->GetHead()); }

    void ClearEntries(stateEnum listType);
    void ClearEntries(displayStateEnum listType);
    void ClearReplicateEntries() { replicateList->Empty(); } 
    ReplicateList *RepList() { return replicateList; } 

			// searches current list for fileId
    MgrNode *StateFindFileId(stateEnum s, int fileId);
			// returns stateNext or statePrev member variables
			// i.e. next or previous node on curr state list

    int SaveCompleteCmdList(MgrNode *mn)
	{ return mn->ChangeList(completeList); }
    int SaveIncompleteCmdList(MgrNode *mn)
	{ return mn->ChangeList(incompleteList); }
    int CancelCmdList(MgrNode *mn)
	{ return mn->ChangeList(cancelList); }
    int DeleteCmdList(MgrNode *mn)
	{ return mn->ChangeList(deleteList); }
    int ModifyCmdList(MgrNode *mn)
	{ return mn->ChangeList(mappedWriteList); }
    int ViewCmdList(MgrNode *mn)
	{ return mn->ChangeList(mappedViewList); }
    int CloseCmdList(MgrNode *mn)
	{ return ( ( mn->DisplayState() == mappedWrite ) || 
		   ( mn->DisplayState() == mappedView ) ) ?
		       mn->ChangeList(closeList) : 0;
	}
/*
//	{ if(mn->DisplayState() == mappedWrite || 
//	     mn->DisplayState() == mappedView)
//	     return mn->ChangeList(closeList); 
//	  else return 0;
//	}
*/
    int ReplicateCmdList(MgrNode *mn);

    void ClearInstances();
protected:

};

#endif
