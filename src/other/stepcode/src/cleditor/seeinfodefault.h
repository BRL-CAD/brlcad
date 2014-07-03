#ifndef seeinfodefault_h
#define seeinfodefault_h

/*
* NIST STEP Editor Class Library
* cleditor/seeinfodefault.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

// this is a default seeinfo that does nothing... thus it is not
//  dependent on a user interface toolkit

#include <sc_export.h>

class MgrNode;
class DisplayNode;
class DisplayNodelist;
#include <sdai.h>
//class SDAI_Application_instance;

#include <editordefines.h>

class SC_EDITOR_EXPORT seeInfo : public DisplayNode {
    public:
        seeInfo( MgrNode * node,
                 SDAI_Application_instance * se,
                 DisplayNodeList * dnl, displayStateEnum displaySt = mappedWrite );

        void * GetSEE()      {
            return see;
        }
};

inline seeInfo::seeInfo( MgrNode * node, SDAI_Application_instance * se,
                         DisplayNodeList * dnl, displayStateEnum displaySt ) {
    mn = node;
    see = 0;
    displayState = displaySt;
    dnl->Append( this );
}

#endif
