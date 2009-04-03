
/*
* NIST STEP Editor Class Library
* cleditor/dispnode.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: dispnode.cc,v 3.0.1.2 1997/11/05 22:11:39 sauderd DP3.1 $ */ 

#include <gennode.h>
#include <gennodelist.h>
//#include <gennode.inline.h>

#include <dispnode.h>
#include <dispnodelist.h>

// define this to be the name of the display object
class StepEntityEditor;

// This function needs to be defined outside the SCL libraries.  It needs to do
// two things:
// 1) unmap the StepEntityEditor window if it is mapped.
// 2) delete the StepEntityEditor window
// To see an example of this function used with the Data Probe look in
// ../clprobe-ui/StepEntEditor.cc  Look at DeleteSEE() and ~StepEntityEditor().
extern void DeleteSEE(StepEntityEditor *se);

DisplayNode::~DisplayNode()
{
    Remove();
    if(see)
    {
	DeleteSEE((StepEntityEditor *)see);
//DAS PORT need the cast from void*	DeleteSEE(see);
    }
}

void DisplayNode::Remove()
{
    GenericNode::Remove();
// DON'T DO THIS!!    displayState = noMapState;
}

int DisplayNode::ChangeState(displayStateEnum s)
{
    displayState = s;
    return 1;
}

int DisplayNode::ChangeList(DisplayNodeList *cmdList)
{
    Remove();
    cmdList->Append(this);
    return 1;
}
