#ifndef probe_h
#define probe_h

/*
* NIST Data Probe Class Library
* clprobe-ui/probe.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: probe.h,v 1.1 1998/02/27 17:52:00 sauderd Exp $ */

class Probe;

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/menu.h>
#include <IV-2_6/InterViews/viewport.h>
#include <mystrbrowser.h>  // ivfasd/mystrbrowser.h

// SCL
#include <sdai.h>

#include <mycontrol.h>
#include <mymenu.h>
#include <errordisplay.h>  // ivfasd/errordisplay.h
#include <seinstdisp.h> // "./seinstdisp.h"
#include <setypedisp.h> // "./setypedisp.h"
#include <stepenteditor.h> // "./stepenteditor.h"
#include <stepentdescriptor.h> // "./stepenteditor.h"
#include <headerdisp.h>
#include <instmgr.h>
#include <Registry.h>
#include <STEPfile.h>
#include <cmdmgr.h>

#include <sclfilechooser.h>

#include <IV-2_6/_enter.h>

class ListItemChooser;

///////////////////////////////////////////////////////////////////////////////
//
// Class Probe definition
//
///////////////////////////////////////////////////////////////////////////////

class Probe : public MonoScene
{
private:
    int readUsingTC;
    InstMgr *instMgr;
    Registry *registry;
    STEPfile *fileMgr;
    CmdMgr *cmdMgr;
    sclFileChooser* fileChooser;
    HeaderDisplay *headerDisplay;

    seInstListDisplay *entityInstanceList;
    seTypeListDisplay *entityTypeList;

    MyMenuBar *functionsMenuBar;
    MyMenu *functionsMenu;

    MyMenuBar *schemaMgmtMenuBar;
    MyMenu *schemaMgmtMenu;

    MyMenuBar *instanceMgmtMenuBar;
    MyMenu *instanceMgmtMenu;

    MyMenuBar *fileMgmtMenuBar;
    MyMenu *fileMgmtMenu;

    ErrorDisplay *errorDisp;
	// Auto Save happens after every AutoSaveCount loops in Probe::Run()
    static int autoSaveCount;
    static char * autoSaveFile;	// This is the Auto Save File.

    Interactor *b;

public:

    Probe();
    ~Probe();
    void Run();

    int AutoSave();
    int ChooseAutoSaveFile();
    void WriteBackupFileCmd()	     { AutoSave(); }
    void ChooseBackupFileCmd() { ChooseAutoSaveFile(); }

    void ErrorMsg(const char *msg)	{ errorDisp->ErrorMsg(msg); };

    InstMgr *GetInstMgr()	{ return instMgr; }
    void AssignFileId(STEPentity *se);

//NEW (below)
    void UpdateCmdList(int index, char cmd, int cmdCol);
    void ExecuteInstanceCmds();
    void ExecuteSaveComplete();
    void ExecuteSaveIncomplete();
    void ExecuteDelete();
    void ExecuteCancel();
    void ExecuteReplicate();
    void ExecuteModify();
    void ExecuteView();
    void ExecuteClose();
    void ExecuteUnmark();

//NEW (above)

	// SEE mgmt functions
    void seeSaveComplete(StepEntityEditor *see);
    void seeSaveIncomplete(StepEntityEditor *see);
    void seeCancel(StepEntityEditor *see);
    void seeDelete(StepEntityEditor *see);
    void seeReplicate(StepEntityEditor *see);
    void seeOpenSED(StepEntityEditor *see);

    int seeEdit(StepEntityEditor *see);
    int seeEntityEdit(StepEntityEditor *see);
    int seeEnumEdit(StepEntityEditor *see);
    int	seeSelectEdit(StepEntityEditor *see);

    int seeSelectMark(StepEntityEditor *see);
//    int seeList(StepEntityEditor *see);
    void seeToggle(StepEntityEditor *see, int code);

	// command options called from the STEPentity display list
    boolean sedlSaveComplete(MgrNode *mn, int index);
    boolean sedlSaveIncomplete(MgrNode *mn, int index);
    void sedlCancel(MgrNode *mn, int index);
    void sedlDelete(MgrNode *mn, int index);
    void sedlReplicate(MgrNode *mn, int index);
    void sedlModify(MgrNode *mn, int index);
    void sedlView(MgrNode *mn, int index);
    void sedlClose(MgrNode *mn, int index);
    void sedlUnmark(MgrNode *mn, int index);
/* 	these are missing because they act on an STEPattribute not a STEPentity
    void sedlEdit(int index);
    void sedlChooseMark(int index);
    void sedlList(int index);
*/
				// place objects into the world
    Interactor *InsertInteractor (Interactor * i, Alignment align = Center,
				  Interactor * alignTo = 0);
    void	RemoveInteractor (Interactor* i);

    void	InsertHeaderEditor ();
    void	RemoveHeaderEditor ();

    void	InsertSED (StepEntityDescriptor * sed);
    void	RemoveSED (StepEntityDescriptor * sed);

    void	InsertSEE (StepEntityEditor * i);
    void	RemoveSEE (StepEntityEditor* i);
    void	RemoveSeeIfNotPinned (StepEntityEditor* see, MgrNode *mn);
    void	SetProbeCursor(Cursor *c);

    boolean IsMapped ();

    void ReadFile();
    void AppendFile();
    void WriteFile();
				// functions_menu
    void Quit() { exit(0); };
    void Refresh() {  };
				// schema_mgmt_menu
    void ListEntityTypesCmd();
    void DescribeEntityCmd();
    void SelectSchemaCmd();
				// instance_mgmt_menu
    void CreateInstanceCmd();
    void ModifyInstanceCmd();
    void ViewInstanceCmd();
    void CloseInstanceCmd();
    void SaveComplInstanceCmd();
    void SaveIncomplInstanceCmd();
    void ReplicateInstanceCmd();
    void UnmarkInstanceCmd();
    void DeleteInstanceCmd();
    void PrintInstanceCmd();
    void SelectInstanceCmd();
				// file_mgmt_menu
    void ReadExchangeFileCmd();
    void ReadExchangeFileCmdPreTC();
    void AppendExchangeFileCmd();
    void WriteExchangeFileCmd();
    
    void ReadWorkingFileCmd(); 
    void ReadWorkingFileCmdPreTC();
    void AppendWorkingFileCmd();
    void WriteWorkingFileCmd();

    void VerifyInstsCmd();
    void RemoveDelInstsCmd();
    void ClearInstanceListCmd();
    void EditHeaderInstancesCmd();
    void DoNothing() {}
protected:
    void WriteInstance(int index);
    void ListEntityInstances();
    StepEntityEditor *GetStepEntityEditor(STEPentity *se, 
					 int modifyBool = 1, int pinBool = 1);
    StepEntityEditor *GetStepEntityEditor(MgrNode *mn,
					 int modifyBool = 1, int pinBool = 1);

    void BuildSubtypeList(ListItemChooser *lic, EntityDescriptor *ed);

private:
    void Init();
    void InitEntityInstanceList();
    void InitEntityTypeList();
    void InitMenus();

    Interactor *ProbeBody();
    Interactor *SEEHelpBody();
    MyPulldownMenu* MakePulldown( const char* name, struct DPMenuInfo* item);

};

#include <IV-2_6/_leave.h>

#endif

