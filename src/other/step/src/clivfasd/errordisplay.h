/*
 *  ErrorDisplay - a HBox that provides a label, followed by a 
 *	framed stringdisplay, followed by a pushbutton.
 *	Call ErrorMsg(const char *msg) to display an error, when the user
 *	pushes the button the error is cleared.
 *
 */
#ifndef errordisplay_h
#define errordisplay_h

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>

#include <mybutton.h>

#include <strdisplay.h>  // ivfasd/strdisplay.h"
#include <variousmessage.h>  // ivfasd/variousmessage.h"

#include <IV-2_6/_enter.h>

class ErrorDisplay : public HBox 
{
public:
    ErrorDisplay(const char *labelText = "Error Messages:",
	const char *buttonText = " Clear ", const char *initStr = "\0");
    virtual ~ErrorDisplay();
    void ErrorMsg(const char *msg)	{ errorDisp->Message(msg); };
    void Update();
private:
    StringDisplay *errorDisp;
    ButtonState *errorButSt;
    MyButton *errorButton;
};

inline ErrorDisplay::ErrorDisplay(const char *labelText,
		const char *buttonText, const char *initStr)
{
    errorButSt = new ButtonState(0);
    errorButSt->Attach(this);
    errorButton = new MyPushButton(buttonText, errorButSt, 1);
    errorDisp = new StringDisplay(errorButSt, initStr);

    Insert(new BoldMessage(labelText));
    Insert(new HGlue(round(.05*inch), 0, 0));
    Insert(new Frame(errorDisp));
    Insert(new HGlue(round(.05*inch), 0, 0));
    Insert(errorButton);
};

#include <IV-2_6/_leave.h>

#endif
