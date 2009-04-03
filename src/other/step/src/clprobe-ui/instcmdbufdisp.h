
/*
* NIST Data Probe Class Library
* clprobe-ui/instcmdbufdisp.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: instcmdbufdisp.h,v 3.0.1.1 1997/11/05 23:01:06 sauderd DP3.1 $ */

/*
 * InstCmdBufDisp - an object that let's you manage a list.  Meaning that you
 * can mark an entry on the list for:
 *    1) v(iew) or m(odify)
 *    2) r(eplicate) or d(elete)
 *    3) p(rint)
 *    4) c(hoose)
 *    5) h(ide)
 *    6) s(ave complete)
 *    7) i(save incomplete)
 */

#ifndef instcmdbufdisp_h
#define instcmdbufdisp_h

#include <cmdmgr.h>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/interactor.h>
#include <mystrbrowser.h>
#include <listmgrcore.h>

#include <IV-2_6/_enter.h>

///////////////////////////////////////////////////////////////////////////////
//
// InstCmdBufDisp class
//
///////////////////////////////////////////////////////////////////////////////

class InstCmdBufDisp : public ListMgrCore {
public:
    InstCmdBufDisp(ButtonState* bs, int Rows, int Cols, 
		const char* Done = mySBDone);
    ~InstCmdBufDisp(){};

    void AdvanceSelection(int index);
    virtual void DoCommand(lmCommand lm, int index);
    virtual void Choose();
protected:

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// InstCmdBufDisp inline member functions
//
///////////////////////////////////////////////////////////////////////////////

              //////////////////////////////////////////
#endif
