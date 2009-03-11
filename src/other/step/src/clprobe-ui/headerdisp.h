#ifndef headerdisp_h
#define headerdisp_h

/*
* NIST Data Probe Class Library
* clprobe-ui/headerdisp.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: headerdisp.h,v 3.0.1.1 1997/11/05 23:01:05 sauderd DP3.1 $ */

#define iv2_6_compatible

#include <IV-2_6/InterViews/dialog.h>
#include <IV-2_6/InterViews/frame.h>
#include <mystrbrowser.h>  // ivfasd/mystrbrowser.h
#include <streditor2.h>
#include <instmgr.h>

class Probe;
class HeaderEntityEditor;

#include <mybutton.h>

#include <IV-2_6/_enter.h>

//#define HD_FORWARD_SEARCH '\023'
//#define HD_BACKWARD_SEARCH '\022'

#define HD_MODIFY_ACTION 1
#define HD_INFO_ACTION 2

class HeaderDisplay : public Frame 
{
public:
    HeaderDisplay(InstMgr *him, Probe *probe, int pinBool = 1);
    virtual ~HeaderDisplay();
	// overrule scene's virtual function to do nothing
    void Insert(Interactor *) { };

    void Update();
    virtual int ExecuteCommand(int v);

    InstMgr *HeaderIM() { return headerIM; }
    void HeaderIM(InstMgr *him) { headerIM = him; }

    boolean IsMapped ();

private:
    void CreateButtons(int pinBool);

protected:

    Probe *dp;
    InstMgr *headerIM;
    HeaderEntityEditor *hee1, *hee2, *hee3;

    ButtonState *buttonsButSt;		// subject set by save & abort buttons

    MyPushButton *saveCompleteBut;	// sets buttonsButSt
    MyPushButton *saveIncompleteBut;	// sets buttonsButSt
    MyPushButton *cancelBut;		// sets buttonsButSt

    ButtonState *pinTogButSt;		// subject set by pinTogBut
    PinCheckBox *pinTogBut;		// toggle pin(stay mapped)/unmap with
					// save, cancel, and delete buttons
};

#include <IV-2_6/_leave.h>

#endif
