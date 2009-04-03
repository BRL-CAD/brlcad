
#ifndef seestreditors_h
#define seestreditors_h

/*
* NIST Data Probe Class Library
* clprobe-ui/seestreditors.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: seestreditors.h,v 3.0.1.1 1997/11/05 23:01:09 sauderd DP3.1 $ */

/*
 * seeStringEditor - Interactive editor for strings based on StringEditor2Sub.
 *			This object adds checking for two character terminating
 *			characters such a ^X^S or ^X S.
 *
 * seeIntEditor - Interactive editor for strings of integers based
 *			on IntStringEditorSub.  This object adds checking for 
 *			two character terminating characters such a ^X^S or 
 *			^X S.
 *
 * seeRealEditor - Same as above except for real numbers and based 
 *			on RealStringEditorSub.
 *
 * seeEntityEditor -    Interactive editor for character strings based on
 *			on StringEditor2.  This object accepts only entity
 *			references (i.e. #number). This is
 *			done by redefining InsertValidChar(char c).  It 
 *			performs type checking when a terminating char is
 *			typed by redefining TypeCheck().
 *
 * seeEntAggrEditor - Interactive editor for character strings based
 *			on EntityStringEditor.  This one is sociable with
 *			a supervisor.  In addition to supplying the 
 *			supervisor with the terminating char it also 
 *			returns a pointer to itself.
 *			It does this by redefining SetSubject() to 
 *			communicate with the supervisor supplying the 
 *			fields contained in class SubordinateInfo.
 *
 */

#include <seedefines.h>		// ivfasd/intstreditorsub.h
#include <streditor2sub.h>	// ivfasd/streditor2sub.h
#include <intstreditorsub.h>	// ivfasd/intstreditor2sub.h

int CheckTermChar(char c, const char lastChar, const char *done);
#define SETYPE_ENTITY 7
#define SETYPE_ENTITY_AGGR 8

#include <IV-2_6/_enter.h>

///////////////////////////////////////////////////////////////////////////////
//
// seeStringEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeStringEditor : public StringEditor2Sub {
public:
    seeStringEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeStringEditor(const char* name, ButtonState* bs, const char* sample,
		    int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeStringEditor();
    virtual void Handle(Event&);
    virtual void NewEdit();
    virtual int TypeCheck() { return SETYPE_STRING; }

protected:
    boolean CheckForCmdChars(char c);
    int TermChar(char c);

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeIntEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeIntEditor : public IntStringEditorSub {
public:
    seeIntEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeIntEditor(const char* name, ButtonState* bs, const char* sample,
		 int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeIntEditor();
    virtual void Handle(Event&);
    virtual void NewEdit();
    virtual int TypeCheck() { return SETYPE_SIGNED_INTEGER; }

protected:
    boolean CheckForCmdChars(char c);
    int TermChar(char c);

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeIntAggrEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeIntAggrEditor : public StringEditor2 {
protected:
    SubordinateInfo *subInfo;
public:
    seeIntAggrEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeIntAggrEditor(const char* name, ButtonState* bs, const char* sample, 
		    int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeIntAggrEditor();

	// returns SETYPE_ for success
	// otherwise returns zero.
    virtual int TypeCheck();
    virtual void SetSubject(int c);
    virtual void Handle(Event&);
    virtual void NewEdit();

protected:
    virtual boolean InsertValidChar(char c);
    boolean CheckForCmdChars(char c);
    int TermChar(char c);
private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeRealEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeRealEditor : public RealStringEditorSub {
public:
    seeRealEditor(ButtonState* bs, const char* sample, int width = 0,
		  const char *singleTermChars = seeTermChars);
    seeRealEditor(const char* name, ButtonState* bs, const char* sample,
		  int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeRealEditor();
    virtual void Handle(Event&);
    virtual void NewEdit();
    virtual int TypeCheck() { return SETYPE_REAL; }

protected:
    boolean CheckForCmdChars(char c);
    int TermChar(char c);

private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeRealAggrEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeRealAggrEditor : public StringEditor2 {
protected:
    SubordinateInfo *subInfo;
public:
    seeRealAggrEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeRealAggrEditor(const char* name, ButtonState* bs, const char* sample, 
		    int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeRealAggrEditor();

	// returns SETYPE_ for success
	// otherwise returns zero.
    virtual int TypeCheck();
    virtual void SetSubject(int c);
    virtual void Handle(Event&);
    virtual void NewEdit();

protected:
    virtual boolean InsertValidChar(char c);
    boolean CheckForCmdChars(char c);
    int TermChar(char c);
private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeEntityEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeEntityEditor : public StringEditor2 {
protected:
    SubordinateInfo *subInfo;
public:
    seeEntityEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeEntityEditor(const char* name, ButtonState* bs, const char* sample, 
		    int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeEntityEditor();

	// returns SETYPE_ENTITY for success
	// otherwise returns zero.
    virtual int TypeCheck();
    virtual void SetSubject(int c);
    virtual void Handle(Event&);
    virtual void NewEdit();

protected:
    virtual boolean InsertValidChar(char c);
    boolean CheckForCmdChars(char c);
    int TermChar(char c);
private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeEntAggrEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class seeEntAggrEditor : public StringEditor2 {
protected:
    SubordinateInfo *subInfo;
public:
    seeEntAggrEditor(ButtonState* bs, const char* sample, int width = 0, 
		    const char *singleTermChars = seeTermChars);
    seeEntAggrEditor(const char* name, ButtonState* bs, const char* sample, 
		    int width = 0, const char *singleTermChars = seeTermChars);
    virtual ~seeEntAggrEditor();

	// returns SETYPE_ENTITY
	// otherwise returns zero.
    virtual int TypeCheck();
    virtual void SetSubject(int c);
    virtual void Handle(Event&);
    virtual void NewEdit();

protected:
    virtual boolean InsertValidChar(char c);
    boolean CheckForCmdChars(char c);
    int TermChar(char c);
private:
    void Init();
};

///////////////////////////////////////////////////////////////////////////////
//
// seeStringEditor class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeStringEditor::Init()
{
    SetClassName("seeStringEditor");
}

inline seeStringEditor::seeStringEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : StringEditor2Sub(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeStringEditor::seeStringEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : StringEditor2Sub(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeIntEditor class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeIntEditor::Init()
{
    SetClassName("seeIntEditor");
}

inline seeIntEditor::seeIntEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : IntStringEditorSub(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeIntEditor::seeIntEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : IntStringEditorSub(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeIntAggrEditor inline functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeIntAggrEditor::Init()
{
    SetClassName("seeIntAggrEditor");
    subInfo = new SubordinateInfo(0, this);
}

inline seeIntAggrEditor::seeIntAggrEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeIntAggrEditor::seeIntAggrEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeRealEditor class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeRealEditor::Init()
{
    SetClassName("seeRealEditor");
}

inline seeRealEditor::seeRealEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars)
 : RealStringEditorSub(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeRealEditor::seeRealEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : RealStringEditorSub(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeRealAggrEditor class inline member functions
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// seeRealAggrEditor inline functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeRealAggrEditor::Init()
{
    SetClassName("seeRealAggrEditor");
    subInfo = new SubordinateInfo(0, this);
}

inline seeRealAggrEditor::seeRealAggrEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeRealAggrEditor::seeRealAggrEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeEntityEditor inline functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeEntityEditor::Init()
{
    SetClassName("seeEntityEditor");
    subInfo = new SubordinateInfo(0, this);
}

inline seeEntityEditor::seeEntityEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeEntityEditor::seeEntityEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

///////////////////////////////////////////////////////////////////////////////
//
// seeEntAggrEditor inline functions
//
///////////////////////////////////////////////////////////////////////////////

inline void seeEntAggrEditor::Init()
{
    SetClassName("seeEntAggrEditor");
    subInfo = new SubordinateInfo(0, this);
}

inline seeEntAggrEditor::seeEntAggrEditor (
    ButtonState* bs, const char* Sample, int width, const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    Init();
}

inline seeEntAggrEditor::seeEntAggrEditor (
    const char* name, ButtonState* bs, const char* Sample, int width,
    const char *singleTermChars
) : StringEditor2(bs, Sample, singleTermChars, width)
{
    SetInstance(name);
    Init();
}

#include <IV-2_6/_leave.h>

#endif
