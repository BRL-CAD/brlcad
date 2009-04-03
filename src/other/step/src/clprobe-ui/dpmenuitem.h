#ifndef dpmenuitem_h
#define dpmenuitem_h

/*
* NIST Data Probe Class Library
* clprobe-ui/dpmenuitem.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: dpmenuitem.h,v 3.0.1.2 1997/11/05 23:01:04 sauderd DP3.1 $ */

/*
 * DPMenuItem - a MyMenuItem that when chosen calls the function
 * 			 pointed to by func which you must define and
 *			 pass to the constructor.
 */

#include <IV-2_6/InterViews/message.h>
#include <probe.h>

#include <IV-2_6/_enter.h>

typedef void (Probe::*DPMenuFunc)();

struct DPMenuInfo {
    const char* lbl;
    DPMenuFunc func;
};

class DPMenuItem : public MyMenuItem {
public:
    DPMenuItem(const char* lbl, DPMenuFunc funcPtr, Probe *dp);

    virtual void Do();
private:
    Probe *owner;
    DPMenuFunc func;
};

inline DPMenuItem::DPMenuItem (const char* lbl, DPMenuFunc funcPtr, 
				Probe *dp) 
   : MyMenuItem(new Message(lbl, Left, 2, hfil, 0))
//				Probe *dp) : MyMenuItem((Interactor*) nil)
{
//   Insert(new Message(lbl, Center, 2, 3));
//   Insert(new Message(lbl, Center, 2, hfil, 0));
   func = funcPtr;
   owner = dp;
}

#include <IV-2_6/_leave.h>

#endif
