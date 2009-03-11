
// ~sauderd/src/iv/hfiles/intstreditor2sub.h

/*
 * IntStringEditorSub - Interactive editor for character strings based
 *			on IntStringEditor.  This one is sociable with
 *			a supervisor.  In addition to supplying the 
 *			supervisor with the terminating char it also 
 *			returns a pointer to itself.
 *			It does this by redefining SetSubject() to 
 *			communicate with the supervisor supplying the 
 *			fields contained in class SubordinateInfo.
 *
 * RealStringEditorSub - Same as above except for real numbers and based 
 *			on RealStringEditor.
 */

#ifndef intstreditorsub_h
#define intstreditorsub_h

#include <InterViews/event.h>
#include <intstreditor.h>
#include <subinfo.h>

#include <IV-2_6/_enter.h>

///////////////////////////////////////////////////////////////////////////////
//
// IntStringEditorSub class definition
//
///////////////////////////////////////////////////////////////////////////////

class IntStringEditorSub : public IntStringEditor {
public:
    IntStringEditorSub(ButtonState*, const char* sample, 
			const char* donechars = SEDone, int width = 0);
    IntStringEditorSub(
        const char* name, ButtonState*, const char*, const char* = SEDone,
	int width = 0
    );
    ~IntStringEditorSub();

    virtual void SetSubject(int c);

protected:
    SubordinateInfo *subInfo;

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// RealStringEditorSub class definition
//
///////////////////////////////////////////////////////////////////////////////

class RealStringEditorSub : public RealStringEditor {
public:
    RealStringEditorSub(ButtonState*, const char* sample, 
			const char* donechars = SEDone, int width = 0);
    RealStringEditorSub(
        const char* name, ButtonState*, const char*, const char* = SEDone,
	int width = 0
    );
    virtual ~RealStringEditorSub();

    virtual void SetSubject(int c);

protected:
//    virtual boolean HandleChar(char);
    SubordinateInfo *subInfo;

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// IntStringEditorSub class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline void IntStringEditorSub::Init()
{
    SetClassName("IntStringEditorSub");
    subInfo = new SubordinateInfo(0, this);
}

inline IntStringEditorSub::IntStringEditorSub (
    ButtonState* s, const char* samp, const char* donechars, int width
) : IntStringEditor(s, samp, donechars, width)
{
    Init();
}

inline IntStringEditorSub::IntStringEditorSub (
    const char* name, ButtonState* s, const char* samp, const char* donechars,
    int width
) : IntStringEditor( s, samp, donechars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// RealStringEditorSub class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline void RealStringEditorSub::Init()
{
    SetClassName("RealStringEditorSub");
    subInfo = new SubordinateInfo(0, this);
}

inline RealStringEditorSub::RealStringEditorSub (
    ButtonState* s, const char* samp, const char* donechars, int width
) : RealStringEditor(s, samp, donechars, width)
{
    Init();
}

inline RealStringEditorSub::RealStringEditorSub (
    const char* name, ButtonState* s, const char* samp, const char* donechars,
    int width
) : RealStringEditor( s, samp, donechars, width)
{
    SetInstance(name);
    Init();
}

#include <IV-2_6/_leave.h>

#endif
