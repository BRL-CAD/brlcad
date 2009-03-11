
#ifndef setypedisp_h
#define setypedisp_h

/*
* NIST Data Probe Class Library
* clprobe-ui/setypedisp.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: setypedisp.h,v 3.0.1.1 1997/11/05 23:01:11 sauderd DP3.1 $ */

#include <IV-2_6/InterViews/frame.h>
#include <mystrbrowser.h>  // ivfasd/mystrbrowser.h
#include <streditor2.h>

#include <mybutton.h>

#include <IV-2_6/_enter.h>

#define TLD_FORWARD_SEARCH '\023'
#define TLD_BACKWARD_SEARCH '\022'
#define TLD_CONSIDER_CASE '\000'
#define TLD_IGNORE_CASE '\001'

#define TLD_CREATE_ACTION 1
#define TLD_INFO_ACTION 2

#define TLD_VSPACE (round(.15*inch))
#define TLD_HSPACE (round(.15*inch))

class seTypeListDisplay : public Frame 
{
public:
    seTypeListDisplay();
	// overrule scene's virtual function to do nothing
    void Insert(Interactor *) { };

    void Update();

// operations on the list
    void Browse();
    virtual void Insert(const char*, int index);
    virtual void Append(const char*);
    virtual void Remove(int index);
    virtual void ReplaceText(const char* s, int index);

    void Bold(int index);

    int Index(const char*);
    char* String(int);
    int Count();
    void Clear();

    void Select(int index);
    void Unselect(int index);
    void UnselectAll();
    int Selection(int selindex = 0);
    int SelectionIndex(int index);
    int Selections();
    boolean Selected(int index);

private:
    void CreateButtons();

private:
    MyStringBrowser *entityTypeList;
    ButtonState *entityTypeListButSt;
    
    StringEditor2 *searchBuf;
    ButtonState *searchBufButSt;
    ButtonState *searchButtonsButSt;
    MyRadioButton *forwardSearchBut;
    MyRadioButton *backwardSearchBut;
    ButtonState *caseButtonButSt;
    MyCheckBox *caseSearchBut;

    ButtonState *buttonsButSt;

    MyPushButton *createBut;
    MyPushButton *infoBut;
};

///////////////////////////////////////////////////////////////////////////////

inline void seTypeListDisplay::Browse()
{
    entityTypeList->Browse();
}

inline void seTypeListDisplay::Bold(int index)
{
    entityTypeList->Bold(index);
}

inline int seTypeListDisplay::Index(const char* s)
{
    return entityTypeList->Index(s);
}

inline char *seTypeListDisplay::String(int index)
{
    return entityTypeList->String(index);
}

inline int seTypeListDisplay::Count()
{
    return entityTypeList->Count();
}

inline void seTypeListDisplay::Clear()
{
    entityTypeList->Clear();
}

inline void seTypeListDisplay::Select(int index)
{
    entityTypeList->Select(index);
}

inline void seTypeListDisplay::Unselect(int index)
{
    entityTypeList->Unselect(index);
}

inline void seTypeListDisplay::UnselectAll()
{
    entityTypeList->UnselectAll();
}

inline int seTypeListDisplay::Selection(int selindex)
{
    return entityTypeList->Selection(selindex);
}

inline int seTypeListDisplay::SelectionIndex(int index)
{
    return entityTypeList->SelectionIndex(index);
}

inline int seTypeListDisplay::Selections()
{
    return entityTypeList->Selections();
}

inline boolean seTypeListDisplay::Selected(int index)
{
    return entityTypeList->Selected(index);
}

#include <IV-2_6/_leave.h>

#endif
