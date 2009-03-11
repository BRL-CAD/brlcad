
#ifndef instmgr_h
#define instmgr_h

/*
* NIST STEP Editor Class Library
* cleditor/instmgr.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: instmgr.h,v 3.0.1.4 1997/11/05 22:11:42 sauderd DP3.1 $ */ 

///// future? ////////////////
// InstMgr can maintain an undo list for the last operation
//	performed on a node 
// InstMgr can have a startUndo() and endUndo() so it knows when to
//	start a new undo list and delete the old undo list. 
/////////////////////

/*
#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif
*/

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

typedef unsigned boolean;
extern char * EntityClassName ( char *);

// IT IS VERY IMPORTANT THAT THE ORDER OF THE FOLLOWING INCLUDE FILES
// BE PRESERVED

#include <gennode.h>
#include <gennodelist.h>
//#include <gennode.inline.h>
#include <gennodearray.h>

#include <mgrnode.h>
#include <mgrnodelist.h>

#include <dispnode.h>
#include <dispnodelist.h>

#include <mgrnodearray.h>

class InstMgr
{
protected:
    int maxFileId;
    int _ownsInstances; // if true will delete instances inside destructor

    MgrNodeArray *master;	// master array of all MgrNodes made up of
			// complete, incomplete, new, delete MgrNodes lists
			// this corresponds to the display list object by index
    MgrNodeArraySorted *sortedMaster;	// master array sorted by fileId
//    StateList *master; // this will be an sorted array of ptrs to MgrNodes

public:
    InstMgr(int ownsInstances = 0);
    virtual ~InstMgr();

// MASTER LIST OPERATIONS
    int InstanceCount()   		{ return master->Count(); }

    int OwnsInstances() { return _ownsInstances; }
    void OwnsInstances(int ownsInstances) { _ownsInstances = ownsInstances; }

    void ClearInstances(); //clears instance lists but doesn't delete instances
    void DeleteInstances(); // deletes the instances (ignores _ownsInstances)

    Severity VerifyInstances(ErrorDescriptor& e);

    // DAS PORT possible BUG two funct's below may create a temp for the cast
    MgrNode * GetMgrNode(int index)   
        { return (MgrNode *) *GetGenNode (index); }
    GenericNode** GetGenNode(int index)   
	{ return &(*master) [index]; }

    MgrNode *FindFileId(int fileId);
	// get the index into display list given a SCLP23(Application_instance)
	//  called by see initiated functions
    int GetIndex(SCLP23(Application_instance) *se);
    int GetIndex(MgrNode *mn);
    int VerifyEntity(int fileId, const char *expectedType);

//    void Append(MgrNode *node);
    MgrNode *Append(SCLP23(Application_instance) *se, stateEnum listState);
		// deletes node from master list structure 
    void Delete(MgrNode *node);
    void Delete(SCLP23(Application_instance) *se);

    void ChangeState(MgrNode *node, stateEnum listState);

    int MaxFileId()		{ return maxFileId; }
    int NextFileId()		{ return maxFileId = maxFileId +1; }
    int EntityKeywordCount(const char* name);

    SCLP23(Application_instance) *GetApplication_instance(int index);
    SCLP23(Application_instance) *
			    GetApplication_instance(const char* entityKeyword, 
						    int starting_index =0);
    SCLP23(Application_instance) *GetApplication_instance(MgrNode *node) 
				   { return node->GetApplication_instance(); };

    void *GetSEE(int index);
    void *GetSEE(MgrNode *node) { return node->SEE(); };

    void PrintSortedFileIds();

/*
#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
*/

    // OBSOLETE
    SCLP23(Application_instance) *GetSTEPentity(int index);
    SCLP23(Application_instance) *GetSTEPentity(const char* entityKeyword, 
			      int starting_index =0);
    SCLP23(Application_instance) *GetSTEPentity(MgrNode *node) 
				{ return node->GetApplication_instance(); };

};

#endif
