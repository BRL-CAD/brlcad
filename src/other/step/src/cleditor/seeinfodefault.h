#ifndef seeinfodefault_h
#define seeinfodefault_h

/*
* NIST STEP Editor Class Library
* cleditor/schemaheader.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

// this is a default seeinfo that does nothing... thus it is not
//	dependent on a user interface toolkit

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

class MgrNode;
class DisplayNode;
class DisplayNodelist;
#include <sdai.h>
//class SCLP23(Application_instance);

#include <editordefines.h>

class seeInfo : public DisplayNode
{
public:
    seeInfo(MgrNode *node, 
	    SCLP23(Application_instance) *se,
	    DisplayNodeList *dnl, displayStateEnum displaySt = mappedWrite);

    void *GetSEE()		{ return see; }
};

inline seeInfo::seeInfo(MgrNode *node, SCLP23(Application_instance) *se,
		 DisplayNodeList *dnl, displayStateEnum displaySt)
{
    mn = node; 
    see = 0;
    displayState = displaySt; 
    dnl->Append(this);
}

#endif
