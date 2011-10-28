#ifndef stepenteditor_h
#define stepenteditor_h

/*
* NIST Data Probe Class Library
* clprobe-ui/stepenteditor.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: stepenteditor.h,v 3.0.1.1 1997/11/05 23:01:12 sauderd DP3.1 $ */

#include <ctype.h>	// to get iscntrl()

#include <instmgr.h>
// #include <schema.h>
#include <sdai.h>
#include <STEPattribute.h>

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/streditor.h>
#include <label.h>

#include <mybutton.h>
#include <streditor2sub.h>	// "~sauderd/src/iv/hfiles/streditor2sub.h"

#include <seedefines.h>
#include <mymessage.h>
#include <strdisplay.h>

class Probe;
class MyButtonState;

#include <IV-2_6/_enter.h>

//////////////////////////////////////////////////////////////////////////////
//
//  This is a horizontal box object that represents a STEPattribute in a 
//  STEPentity (a row in the StepEntityEditor's (SEE's) list of STEPattributes)
//  It's a Horizontal box containing a myLabel (attr name), StringEditor 
//  (editable attribute field), and another myLabel (attr type).
//  Note that these rows correspond to STEPattributes for a STEPentity;
//  it does not mean that when the values are changed the STEPentity's 
//  STEPattribute values are changed.  The value for the attribute in
//  this object can be saved complete, saved incomplete, or aborted by the
//  StepEntityEditor.
//
//////////////////////////////////////////////////////////////////////////////

class seeAttrRow : public HBox
{
public:
    seeAttrRow(STEPattribute * sAttr, InstMgr *im, ButtonState *b, 
	       int nameWidth, int editWidth, int typeWidth);
    virtual ~seeAttrRow();

    const char * GetAttrName();
    const char * GetAttrTypeName();
    BASE_TYPE GetType();
	       // return aName with brackets around it if attribute is optional
    char * IndicateOptionality(const char *aName);
    enum Severity Validate(InstMgr *instMgr, int setMark = 1, 
		      enum Severity setMarkAtSev = SEVERITY_INCOMPLETE);
    enum Severity SaveAttr(InstMgr *instMgr, 
		      enum Severity setMarkAtSev = SEVERITY_INCOMPLETE);
    void UndoChanges();

    char *StrAttrAssignVal(const char *s);
    char *StrEditorVal(std::string &s);

	// return private class members
    STEPattribute *StepAttr()	{ return stepAttr; }
    MyMessage	  *StatusMarker() { return statusMarker; }
    myLabel 	  *NameField()	{ return nameField; };
    StringEditor2 *EditField()	{ return editField; };
    myLabel 	  *TypeField()	{ return typeField; };
    ButtonState	  *ButtonSt()	{ return butSt; };
    seeAttrRow	  *Next()	{ return next; };
    seeAttrRow	  *Previous()	{ return previous; };

	// get or set the text in the editable field
    const char *GetEditFieldText()		 { return editField->Text(); };
    void 	SetEditFieldText(const char *str) { editField->Message(str); };

	// initiate an edit for the attribute editable field and get
	//	or set the terminating char for an edit
    void Edit()			  { butSt->SetValue(0); editField->NewEdit();};
    int GetTerminatingChar()	  { int v; butSt->GetValue(v); return (v); };
    void SetTerminatingChar(int v) { butSt->SetValue(v); };

    boolean IsMapped ();

private:
    friend class seeAttrRowList;

//   StringDisplay *statusMarker; // a marker for indicating invalid data, etc.
    MyMessage *statusMarker; // a marker for indicating invalid data, etc.
    myLabel        *nameField;	// label for STEPattribute name
    StringEditor2 *editField;	// editable (Step attribute) field
    myLabel        *typeField;	// label for STEPattribute type
    ButtonState  *butSt;	// set by editField to char terminating edit

    seeAttrRow *next;		// next row in SEE's list of STEPattributes
    seeAttrRow *previous;	// previous row in SEE's list of STEPattributes
    STEPattribute *stepAttr;	// STEPattribute represented by this row
};

//////////////////////////////////////////////////////////////////////////////
//
//  This is a list of seeAttrRow's (i.e. a list of editable STEPattribute's)
//  in the order returned by STEPentity's NextAttribute() for a single 
//  STEPentity.  It has the idea of a current row to indicate, for instance,
//  where the previous edit was done or where the current edit is being done.
//
//////////////////////////////////////////////////////////////////////////////

class seeAttrRowList 
{
public:
    seeAttrRowList();
    virtual ~seeAttrRowList();

	// access members in the list.
    seeAttrRow *Head()		{ return head; };
    seeAttrRow *LastRow()	{ return (head ? (head->Previous()) : nil); };
    seeAttrRow *CurRow()	{ return currentRow; };
    seeAttrRow *NextRow();	// following current row
    seeAttrRow *PreviousRow();	// preceding current row

	// set the current row in the list by index, returns index or 0 (error)
    int 	SetCurRow(int index);
	// set the current row in the list to existingRow,
	// returns the row object -- does not check if existingRow is in list.
    seeAttrRow *SetCurRow(seeAttrRow *existingRow);

	// return the number of members (rows) in the list
    int RowCount()               { return rowCount; };

	// Find() returns the index in the list (found) if found 
	// otherwise zero (not found)
    int Find(seeAttrRow *r);
	// returns the row object containing the editable attribute field
    seeAttrRow *FindEditField(Interactor *targetStrEd);

	// Append() and Insert() add a row object to the end of the list.
    void Append(seeAttrRow *newRow)     { InsertAfter(newRow, LastRow()); };
    void Insert(seeAttrRow *newRow)     { InsertAfter(newRow, LastRow()); };
	// insert a row object into the list before or after existingRow.
	// returns the index of the new object in the list or zero (error)
    int InsertAfter(seeAttrRow *newRow, seeAttrRow *existingRow);
    int InsertBefore(seeAttrRow *newRow, seeAttrRow *existingRow);
/*
    int InsertAfter(seeAttrRow *newRow, seeAttrRow *existingRow = LastRow());
    int InsertBefore(seeAttrRow *newRow, seeAttrRow *existingRow = Head());

    int InsertAfter(seeAttrRow *newRow, 
		   seeAttrRow *existingRow = (head ? (head->previous) : nil) );
    int InsertBefore(seeAttrRow *newRow, seeAttrRow *existingRow = head);
*/
	// causes the editable attribute fields in the list to ignore events.
    void ComponentsIgnore();
    void ComponentsListen();
private:
    seeAttrRow *head;
    seeAttrRow *currentRow;
    int rowCount;	// number of objects (rows/attributes) in the list
};

inline seeAttrRow *seeAttrRowList::PreviousRow() 
		{ return (currentRow ? (currentRow->Previous()) : nil); };

inline seeAttrRow *seeAttrRowList::NextRow() 
		{ return (currentRow ? (currentRow->Next()) : nil); };

inline seeAttrRow *seeAttrRowList::SetCurRow(seeAttrRow *row) 
		{ if(currentRow){
		    currentRow->NameField()->Highlight(false);
		    currentRow->TypeField()->Highlight(false);
		    currentRow = row; 
		    currentRow->NameField()->Highlight(true);
		    currentRow->TypeField()->Highlight(true);
		    return (currentRow);
		  } else return 0;
		};

//////////////////////////////////////////////////////////////////////////////
//
//  This is a vertical box object that represents a STEPentity.  The vertical
//  box has inserted (in order top to bottom) the following objects : 
//		1) STEPentity name
//		2) step file id
//		3) label field for this instance of STEPentity
//		4) a seeAttrRow for each attribute in a STEPentity in the 
//			order returned by STEPentity::NextAttribute().
//			these objects are accessed through attrRowList (a list
//			of seeAttrRows)
//		5) a button for saving completed modifications
//		6) a button for saving incompleted modifications
//		7) a button for aborting modifications
//
//  Note that this corresponds to a STEPentity; it does not mean that 
//  when it's attribute values are changed the STEPentity's values are 
//  changed.  The modified values for the attributes in this object can
//  be saved complete, saved incomplete, or aborted by the StepEntityEditor.
//
//////////////////////////////////////////////////////////////////////////////

class StepEntityEditor : public VBox
{
public:
    StepEntityEditor(MgrNode *mn, InstMgr *im, int pinBool = 1, 
		     int modifyBool = 1);
    StepEntityEditor(STEPentity *se, int pinBool = 1, 
		     int modifyBool = 1);
    virtual ~StepEntityEditor();

    boolean IsMapped ();
    
    virtual void Handle(Event& e);	// handle down events to grab focus
	// initiate editing starting at currentRow (attribute) in attrRowList
    virtual int Edit();
	// append a seeAttrRow (HBox representing an attribute) to end of 
	// seeAttrRowList and insert seeAttrRow into end of StepEntityEditor 
	// (VBox representing a STEPentity)
    void NotifyUser(const char *msg, int numBellRings = 1);

    void SetAttrTypeMessage(seeAttrRow *row);
    enum Severity ValidateAttr(seeAttrRow *row, 
			  enum Severity setMarkAtSev = SEVERITY_INCOMPLETE, 
			  int notifyUser = 1);
    enum Severity Validate();
    boolean SaveComplete();
    boolean SaveIncomplete();
    void UndoChanges();

    void Insert(STEPattribute *attr, int nameWidth, int editWidth, int typeWidth);
//DAS    seeAttrRowList *AttrRowList()     { return attrRowList; };
    seeAttrRowList *&AttrRowList()     { return attrRowList; };

    int Pinned();
    void SetPinned(int pinnedBool = 1);
    virtual void ComponentsIgnore();
    virtual void ComponentsListen();
    virtual void PressButton(int butCode);
    virtual void DisableSave() { saveCompleteBut->Disable();
			 saveIncompleteBut->Disable(); };
    virtual void EnableSave() { saveCompleteBut->Enable();
			saveIncompleteBut->Enable(); };
//  int GetTerminatingChar()  { int v; editorsButSt->GetValue(v); return (v);};
	// Called when a subject (buttonsButSt or editorsButSt) is set to
	// a NEW value.  StepEntityEditor is 'Subject::Attach()ed' to the
	// two subjects except during the scope of StepEntityEditor::Edit()
	// when it is 'Subject::Detach()ed'.  This means that it's called
	// when (inside a SEE) a button is pushed or an attribute field is
	// changed (and terminated by a terminating char) outside of
	// the scope of StepEntityEditor::Edit().  It does the appropriate
	// action associated with the button or the term char for an attribute
	// edit.  
    void Update();
    ButtonState *GetButtonState() { return buttonsButSt; };
    STEPentity *GetStepEntity() { return stepEnt; };
    MgrNode *GetMgrNode() { return mgrNode; };

    void SetProbe(Probe *p) { dp = p; }
    Probe *GetProbe() { return dp;}

    void SetInstMgr(InstMgr *im) { instMgr = im; }
    InstMgr *GetInstMgr() { return instMgr;}

protected:
    StepEntityEditor() { }

    void EditRows();

    virtual void CreateHeading();
    virtual void CreateButtonsAndStates(int modifyBool = 1, int pinBool = 1);
    virtual void InsertButtons();
    void CreateInsertAttributeEditors();
	// checks editorsButSt & assigns new currentRow (in attrRowList)
	// being edited based on terminating char of edit
    virtual int ExecuteTermChar();
//    int AssignCurRow();
	// checks buttonsButSt & does appropriate action if a button was pushed
    int CheckButtons();
    virtual int ExecuteCommand(int v);

protected: 
	 // list of contained seeAttrRow's (HBox's) representing attributes
    seeAttrRowList *attrRowList; // members set editorsButSt

    MyButtonState *editorsButSt;	// subject set by editable attr fields
    ButtonState *buttonsButSt;		// subject set by save & abort buttons
    MyPushButton *saveCompleteBut;	// sets buttonsButSt
    MyPushButton *saveIncompleteBut;	// sets buttonsButSt
    MyPushButton *cancelBut;		// sets buttonsButSt
    MyPushButton *deleteBut;		// sets buttonsButSt

    MyPushButton *replicateBut;		// sets buttonsButSt
    MyPushButton *editBut;		// sets buttonsButSt
    MyPushButton *markBut;		// sets buttonsButSt
    MyPushButton *listBut;		// sets buttonsButSt

    MyPushButton *sedBut;		// sets buttonsButSt
    MyPushButton *attrInfoBut;		// sets buttonsButSt

    int modifyTogVal;	// used to remember the previous modify/view state
    ButtonState *modifyTogButSt;	// subject set by modifyTogBut
    ModifyViewCheckBox *modifyTogBut;		// toggle modify/view of window

    ButtonState *pinTogButSt;		// subject set by pinTogBut
    PinCheckBox *pinTogBut;		// toggle pin(stay mapped)/unmap with
					// save, cancel, and delete buttons
    Probe *dp;
    InstMgr *instMgr;
    MgrNode *mgrNode;		// MgrNode containing stepEnt
    STEPentity *stepEnt;	// represented STEPentity
    StringEditor2Sub *label;	// label field associated with this STEPentity
    StringDisplay *messageDisp;
    ButtonState *dispButSt;
};

inline int StepEntityEditor::Pinned()
{
    int v;
    pinTogButSt->GetValue(v);
    return (v == SEE_PIN);
}

inline void StepEntityEditor::SetPinned(int pinnedBool)
{
    if(pinnedBool)
	pinTogButSt->SetValue(SEE_PIN);
    else
	pinTogButSt->SetValue(SEE_NOT_PIN);
}


class HeaderEntityEditor : public StepEntityEditor
{
public:
    HeaderEntityEditor(MgrNode *mn, InstMgr *im, int pinBool = 1, 
		     int modifyBool = 1);
    virtual ~HeaderEntityEditor();

    void ComponentsIgnore();
    void ComponentsListen();
    void PressButton(int butCode);
	// Called when a subject (buttonsButSt or editorsButSt) is set to
	// a NEW value.  StepEntityEditor is 'Subject::Attach()ed' to the
	// two subjects except during the scope of StepEntityEditor::Edit()
	// when it is 'Subject::Detach()ed'.  This means that it's called
	// when (inside a SEE) a button is pushed or an attribute field is
	// changed (and terminated by a terminating char) outside of
	// the scope of StepEntityEditor::Edit().  It does the appropriate
	// action associated with the button or the term char for an attribute
	// edit.  
    void Update();

protected:
    void CreateHeading();
    void CreateButtonsAndStates(int modifyBool = 1, int pinBool = 1);
    void InsertButtons();
	// checks buttonsButSt & does appropriate action if a button was pushed
    int ExecuteCommand(int v);
};

#include <IV-2_6/_leave.h>

#endif
