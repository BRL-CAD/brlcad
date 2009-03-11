#ifndef mybuttonstate_h
#define	mybuttonstate_h

#include <IV-2_6/InterViews/button.h>

#include <IV-2_6/_enter.h>

/*
///////////////////////////////////////////////////////////////////////////////
// This class was created to avoid all views being notified when the 
// ButtonState value is nulled.
///////////////////////////////////////////////////////////////////////////////
*/

class MyButtonState : public ButtonState {
  public:
    MyButtonState () {
	value = nil;
	Reference();
    }
    MyButtonState (int v) {
	value = (void*)v;
	Reference();
    }
    MyButtonState (void* v) {
	value = v;
	Reference();
    }
    virtual ~MyButtonState () { }; 

    void SetNull(void *val = 0, int notify = 0) {
	value = val;
	if (notify) Notify();
    }
};

#include <IV-2_6/_leave.h>

#endif
