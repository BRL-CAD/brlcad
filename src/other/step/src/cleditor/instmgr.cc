
/*
* NIST STEP Editor Class Library
* cleditor/instmgr.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: instmgr.cc,v 3.0.1.5 1997/11/05 22:11:42 sauderd DP3.1 $ */ 

//////////////////////////////////////////////////////////////////////////////
//
// InstMgr member functions
//
//////////////////////////////////////////////////////////////////////////////

#include <sdai.h>
//#include <STEPentity.h>
#include <instmgr.h>

///////////////////////////////////////////////////////////////////////////////
//	debug_level >= 2 => tells when a command is chosen
//	debug_level >= 3 => prints values within functions:
///////////////////////////////////////////////////////////////////////////////
static int debug_level = 2;
	// if debug_level is greater than this number then
	// function names get printed out when executed
//static int PrintFunctionTrace = 2;
	// if debug_level is greater than this number then
	// values within functions get printed out
//static int PrintValues = 3;


///////////////////////////////////////////////////////////////////////////////

void 
InstMgr::PrintSortedFileIds()
{
    MgrNode *mn = 0;
    int count = InstanceCount();
    int i = 0;
    for(i = 0; i < count; i++)
      {
	mn = (MgrNode *)((*sortedMaster)[i]);
	cout << i << " " << mn->GetFileId() << endl;
      }
}

InstMgr::InstMgr(int ownsInstances)
 : maxFileId(-1), _ownsInstances(ownsInstances)
{
    master = new MgrNodeArray();
    sortedMaster = new MgrNodeArraySorted();
}

InstMgr::~InstMgr()
{
    if(_ownsInstances)
    {
	master->DeleteEntries();
    }
}

///////////////////////////////////////////////////////////////////////////////

void InstMgr::ClearInstances()
{
//    delete master;
//    master = new MgrNodeArray();
    master->ClearEntries();
    sortedMaster->ClearEntries();
    maxFileId = -1;
}

void InstMgr::DeleteInstances()
{
//    delete master;
//    master = new MgrNodeArray();
    master->DeleteEntries();
    sortedMaster->ClearEntries();
    maxFileId = -1;
}

///////////////////////////////////////////////////////////////////////////////

/**************************************************
 description:
 returns:
    SEVERITY_NULL:        if all instances are complete
    SEVERITY_INCOMPLETE:  if at least one instance is missing a required attribute
    SEVERITY_WARNING:     
    SEVERITY_INPUT_ERROR: if at least one instance can not be fetched
                          from the instance list.
**************************************************/

enum Severity
InstMgr::VerifyInstances(ErrorDescriptor& err) 
{
    int errorCount = 0;
    char errbuf[BUFSIZ];
    
    int n = InstanceCount();
    MgrNode* mn;
    SCLP23(Application_instance)* se;
    enum Severity rval = SEVERITY_NULL;
    
    //for each instance on the list,
    //check the MgrNode state.
    //if the state is complete then do nothing
    //if the state is not complete,
    //   then check if it is valid
    //   if it is valid then increment the warning count,
    //      and set the rval to SEVERITY_INCOMPLETE
    //   if it is not valid, then increment the error count 
    //      and set the rval to 

    for (int i = 0; i < n; ++i) 
    {
	mn = GetMgrNode(i);
	if (!mn) 
	{
	    ++errorCount;
	    if (errorCount == 1) 
		sprintf(errbuf,
		"VerifyInstances: Unable to verify the following instances: node %d", 
			i);
	    else
		sprintf(errbuf,", node %d",i);

	    err.AppendToDetailMsg(errbuf);
	    rval = SEVERITY_INPUT_ERROR;
	    err.GreaterSeverity(SEVERITY_INPUT_ERROR);
	    continue;
	}
	if (debug_level > 3)
	  cerr << "In VerifyInstances:  " 
	    << "new MgrNode for " << mn->GetFileId() << " with state " 
	       << mn->CurrState () << endl;
	if (!mn->MgrNodeListMember(completeSE)) 
	{
	    se = mn->GetApplication_instance();
	    if (se->ValidLevel(&err,this,0) < SEVERITY_USERMSG)
	    {
		if (rval > SEVERITY_INCOMPLETE) 
		    rval = SEVERITY_INCOMPLETE;
		++errorCount;
		if (errorCount == 1)
		    sprintf(errbuf,
			    "VerifyInstances: Unable to verify the following instances: #%d",
			    se->StepFileId());
		else 
		    sprintf(errbuf,", #%d",se->StepFileId());
		err.AppendToDetailMsg(errbuf);
	    }
	}
    }
    if (errorCount) 
    {
	sprintf(errbuf,
		"VerifyInstances: %d invalid instances in list.\n", 
		errorCount);
	err.AppendToUserMsg(errbuf);
	err.AppendToDetailMsg(".\n");
	err.GreaterSeverity(SEVERITY_INCOMPLETE);
    }

    return rval;
}

///////////////////////////////////////////////////////////////////////////////

MgrNode *InstMgr::FindFileId(int fileId)
{
    int index = sortedMaster->MgrNodeIndex(fileId);
    if(index >= 0)
	return (MgrNode *)(*sortedMaster)[index];
    else
	return (MgrNode *)0;
}

///////////////////////////////////////////////////////////////////////////////

	// get the index into display list given a SCLP23(Application_instance)
	//  called by see initiated functions
int InstMgr::GetIndex(MgrNode *mn)
{
    return mn->ArrayIndex();
}

///////////////////////////////////////////////////////////////////////////////

int InstMgr::GetIndex(SCLP23(Application_instance) *se)
{
    int fileId = se->StepFileId();
    return sortedMaster->MgrNodeIndex(fileId);
}

///////////////////////////////////////////////////////////////////////////////

int InstMgr::VerifyEntity(int fileId, const char *expectedType)
{
    MgrNode *mn = FindFileId(fileId);
    if(mn) 
    {
	if(!strcmp(expectedType, mn->GetApplication_instance()->EntityName()))
	    return 2;	// types match
	else
	    return 1;	// possible mismatch depending on descendants
    }
    else
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//   Append instance to the list of instances.  Checks the file id and 
//   sets it if 1) it is not set already or 2) it already exists in the list.

MgrNode *InstMgr::Append(SCLP23(Application_instance) *se, stateEnum listState)
{
    if (debug_level > 3)
	cout << "#" << se->StepFileId() << " append node to InstMgr" << endl;

    MgrNode *mn =0;

    if (se->StepFileId () == 0)  // no id assigned
	se->StepFileId( NextFileId () ); // assign a file id

    if (mn = FindFileId (se->StepFileId())) // if id already in list
	// and it's because instance is already in list
	if (GetApplication_instance (mn) == se)  
	    return 0;  // return 0 or mn?
	else se->StepFileId( NextFileId () ); // otherwise assign a new file id

    // update the maxFileId if needed
    if (se->StepFileId() > MaxFileId()) maxFileId = se->StepFileId();

    mn = new MgrNode(se, listState);

    if (debug_level > 3)
      cerr << "new MgrNode for " << mn->GetFileId() << " with state " 
	   << mn->CurrState () << endl;
    if(listState == noStateSE)
	cout << "append to InstMgr **ERROR ** node #" << se->StepFileId() << 
		" doesn't have state information" << endl;
    master->Append(mn);
    sortedMaster->Insert(mn);
//PrintSortedFileIds();
    return mn;
}

///////////////////////////////////////////////////////////////////////////////

void InstMgr::Delete(MgrNode *node)
{
	// delete the node from its current state list
    node->Remove();

    int index;    

	// get the index of the node in the sorted master array
    index = sortedMaster->MgrNodeIndex(node->GetFileId());
	// remove the node from the sorted master array
    sortedMaster->Remove(index);

	// get the index into the master array by ptr arithmetic
    index = node->ArrayIndex();
    master->Remove(index);

    delete node;
}

///////////////////////////////////////////////////////////////////////////////

void InstMgr::Delete(SCLP23(Application_instance) *se)
{
   Delete (FindFileId (se->StepFileId()));
}

///////////////////////////////////////////////////////////////////////////////

void InstMgr::ChangeState(MgrNode *node, stateEnum listState)
{
    switch(listState){
	case completeSE:
		if (debug_level > 3)
		    cout << "#" << node->GetApplication_instance()->StepFileId() << 
		    " change node to InstMgr's complete list\n";
		node->ChangeState(listState);
		break;
	case incompleteSE:
		if (debug_level > 3)
		    cout << "#" << node->GetApplication_instance()->StepFileId() << 
		    " change node to InstMgr's incomplete list\n";
		node->ChangeState(listState);
		break;
	case newSE:
		if (debug_level > 3)
		    cout << "#" << node->GetApplication_instance()->StepFileId() << 
		    " change node to InstMgr's new list\n";
		node->ChangeState(listState);
		break;
	case deleteSE:
		if (debug_level > 3)
		    cout << "#" << node->GetApplication_instance()->StepFileId() << 
		    " change node to InstMgr's delete list\n";
		node->ChangeState(listState);
		break;
	case noStateSE:
		cout << "#" << node->GetApplication_instance()->StepFileId() << 
		"ERROR can't change this node state\n";
		node->Remove();
		break;
    }
}

///////////////////////////////////////////////////////////////////////////////

/**************************************************
 description:
    This function returns an integer value indicating
    the number of instances with the given name appearing
    on the instance manager.
**************************************************/
int 
InstMgr::EntityKeywordCount(const char* name) 
{
    int count = 0;
    MgrNode* node;
    SCLP23(Application_instance)* se;
    int n = InstanceCount();
    for (int j = 0; j<n; ++j) 
      {
	  node = GetMgrNode(j);
	  se = node->GetApplication_instance();
	  if (!strcmp(se->EntityName(),
		      PrettyTmpName(name)))
	      ++count;
      }
    return count;
}

///////////////////////////////////////////////////////////////////////////////

SCLP23(Application_instance) *
InstMgr::GetApplication_instance(int index)
{
    MgrNode *mn = (MgrNode*)(*master)[index];
    if(mn)
	return mn->GetApplication_instance(); 
    else
	return 0;
}

SCLP23(Application_instance) *
InstMgr::GetSTEPentity(int index)
{
    MgrNode *mn = (MgrNode*)(*master)[index];
    if(mn)
	return mn->GetApplication_instance(); 
    else
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

/**************************************************
 description:
    This function returns the first entity (by index)
    on the instance manager, which has the given
    entity name. It starts its search at starting_index,
    and returns ENTITY_NULL if a match does not occur 
    before the last index is reached. This function 
    does not wrap around to search indices before the
    starting_index.
**************************************************/
SCLP23(Application_instance)*
InstMgr::GetApplication_instance(const char* entityKeyword, int starting_index)
{
    MgrNode *node;
    SCLP23(Application_instance) *se;
    
    int count = InstanceCount();
    for (int j = starting_index; j<count; ++j) 
      {
	  node = GetMgrNode(j);
	  se = node->GetApplication_instance();
	  if (!strcmp(se->EntityName(),
		      PrettyTmpName(entityKeyword)))
	      return se;
      }
    return ENTITY_NULL;
}

SCLP23(Application_instance)*
InstMgr::GetSTEPentity(const char* entityKeyword, int starting_index) 
{
    MgrNode *node;
    SCLP23(Application_instance) *se;
    
    int count = InstanceCount();
    for (int j = starting_index; j<count; ++j) 
      {
	  node = GetMgrNode(j);
	  se = node->GetApplication_instance();
	  if (!strcmp(se->EntityName(),
		      PrettyTmpName(entityKeyword)))
	      return se;
      }
    return ENTITY_NULL;
}

///////////////////////////////////////////////////////////////////////////////

void *
InstMgr::GetSEE(int index)
{
    MgrNode *mn = (MgrNode*)(*master)[index];
    if(mn)
	return mn->SEE(); 
    else
	return 0;
}
