
#ifndef seinstdisp_h
#define seinstdisp_h

/*
* NIST Data Probe Class Library
* clprobe-ui/seinstdisp.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: seinstdisp.h,v 3.0.1.1 1997/11/05 23:01:10 sauderd DP3.1 $ */

#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/frame.h>
#include <mystrbrowser.h>  // ivfasd/mystrbrowser.h
#include <streditor2.h>
#include <instcmdbufdisp.h>

#include <mybutton.h>

#include <IV-2_6/_enter.h>

// forward is ^s - backward is ^r
#define ILD_FORWARD_SEARCH '\023'
#define ILD_BACKWARD_SEARCH '\022'
#define ILD_CONSIDER_CASE '\000'
#define ILD_IGNORE_CASE '\001'

#define ILD_COMPLETE_ACTION 1
#define ILD_INCOMPLETE_ACTION 2
#define ILD_REPLICATE_ACTION 3

#define ILD_EXECUTE_ACTION 4
#define ILD_UNMARK_ACTION 5
#define ILD_DELETE_ACTION 6

#define ILD_VIEW_ACTION 7
#define ILD_MODIFY_ACTION 8
#define ILD_CLOSE_ACTION 9

#define ILD_VSPACE (round(.15*inch))
#define ILD_HSPACE (round(.15*inch))

class seInstListDisplay : public Frame 
{
public:
    seInstListDisplay();
	// overrule scene's virtual function to do nothing
    void Insert(Interactor *) { };

    void Update();

// operations on the list
    void Browse();
    virtual void Insert(const char*, int index);
    virtual int Append(const char*);	// returns the index where appended
    virtual void Remove(int index);
    virtual void ReplaceText(const char* s, int index);
    InstCmdBufDisp *GetInstCmdBuf() { return entityInstanceList; }
    char* CmdString(int index, char *str, int strSize)
	{ return entityInstanceList->CmdString(index, str, strSize); }
    void AdvanceSelection(int index)
	{ entityInstanceList->AdvanceSelection(index); }

    void WriteCmdChar(char ch, int cmdColIndex, int rowIndex)
	{ entityInstanceList->WriteChar(ch, cmdColIndex, rowIndex); }
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
    void CheckButtons();

private:
    void CreateButtons();

private:
//    MyStringBrowser *entityInstanceList;
    InstCmdBufDisp *entityInstanceList;
    ButtonState *entityInstanceListButSt;
    
    StringEditor2 *searchBuf;
    ButtonState *searchBufButSt;
    ButtonState *searchButtonsButSt;
    MyRadioButton *forwardSearchBut;
    MyRadioButton *backwardSearchBut;
    ButtonState *caseButtonButSt;
    MyCheckBox *caseSearchBut;

    ButtonState *buttonsButSt;

    MyPushButton *completeBut;
    MyPushButton *incompleteBut;
    MyPushButton *replicateBut;

    MyPushButton *executeBut;
    MyPushButton *unmarkBut;
    MyPushButton *deleteBut;

    MyPushButton *viewBut;
    MyPushButton *modifyBut;
    MyPushButton *closeBut;
};

///////////////////////////////////////////////////////////////////////////////

inline void seInstListDisplay::Browse()
{
    entityInstanceList->Browse();
}

inline int seInstListDisplay::Index(const char* s)
{
    return entityInstanceList->Index(s);
}

inline char *seInstListDisplay::String(int index)
{
    return entityInstanceList->String(index);
}

inline int seInstListDisplay::Count()
{
    return entityInstanceList->Count();
}

inline void seInstListDisplay::Clear()
{
    entityInstanceList->Clear();
}

inline void seInstListDisplay::Select(int index)
{
    entityInstanceList->Select(index);
}

inline void seInstListDisplay::Unselect(int index)
{
    entityInstanceList->Unselect(index);
}

inline void seInstListDisplay::UnselectAll()
{
    entityInstanceList->UnselectAll();
}

inline int seInstListDisplay::Selection(int selindex)
{
    return entityInstanceList->Selection(selindex);
}

inline int seInstListDisplay::SelectionIndex(int index)
{
    return entityInstanceList->SelectionIndex(index);
}

inline int seInstListDisplay::Selections()
{
    return entityInstanceList->Selections();
}

inline boolean seInstListDisplay::Selected(int index)
{
    return entityInstanceList->Selected(index);
}

#include <IV-2_6/_leave.h>

#endif
