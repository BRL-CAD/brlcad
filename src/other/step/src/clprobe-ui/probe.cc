
/*
* NIST Data Probe Class Library
* clprobe-ui/probe.cc
* February, 1994
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: probe.cc,v 3.0.1.6 1997/11/05 23:01:07 sauderd DP3.2 $ */

#include <sclprefixes.h>

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

//#define EXCHANGE_FILE_DIR "\0"
#define EXCHANGE_FILE_DIR "~pdevel/src/probe/data"
#define WORKING_FILE_DIR "~pdevel/src/probe/data"
#define MAX_PATH_LEN 80
#define MAX_PATH_AND_FILE_LEN 120
#define vspace .25
#define hspace .5
#define VERSION  "Data Probe Version 3.2"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream.h>
#include <fstream.h>

//#include "interactor.h"
//#include "canvas.h"
//#include "event.h"

/*
// in order to include xcanvas.h you will need to have a -I option 
// for /usr/local/include because <X11/Xlib.h> from /usr/local/include/
// gets included from InterViews/include/IV-X11/Xlib.h
#include <IV-X11/xcanvas.h>
#include <InterViews/window.h>
// xcanvas.h and window.h were included so I could write the 
// function IsMapped()
*/

#include <InterViews/canvas.h>
#include <InterViews/window.h>

#include <InterViews/bitmap.h>
#include <InterViews/cursor.h>
#include <InterViews/event.h>
#include <InterViews/font.h>

#include <IV-2_6/InterViews/banner.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/menu.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/viewport.h>
#include <IV-2_6/InterViews/world.h>
#include <OS/string.h>

#include <listitemchooser.h>
#include <sclfilechooser.h>
#include <dirobj.h>
#include <dpmenuitem.h>

#include <ExpDict.h>
#include <Registry.h>
#undef Action

#include <probe.h>
#include <stepentdescriptor.h>
#include <InterViews/style.h>

PulldownMenu* MakePulldown2( const char* name, struct DPMenuInfo* item);

// declared and defined in libInterViews/cursor.c
// function that creates it is called when World is created
extern Cursor* hourglass;

extern void     SchemaInit (Registry &);

extern Probe *dp;
extern Interactor* AddScroller (Interactor* grid);
extern World *world;

int Probe::autoSaveCount = 0;
char * Probe::autoSaveFile = 0;	// This is the Auto Save File.

///////////////////////////////////////////////////////////////////////////////
//	debug_level >= 2 => tells when a command is chosen
//	debug_level >= 3 => prints values within functions:
//	   e.g. 1) entity type list when read
//		2) entity instance list when read
///////////////////////////////////////////////////////////////////////////////
static int debug_level = 1;
	// if debug_level is greater than this number then
	// function names get printed out when executed
static int PrintFunctionTrace = 2;
	// if debug_level is greater than this number then
	// values within functions get printed out
static int PrintValues = 3;
static const char RingBell = '\007';   // <CNTRL> G

static DPMenuInfo quit_menu[] = {
//   { "Refresh",  &Probe::Refresh},
   { "Quit    ", &Probe::Quit},
   { 0 }
};

static DPMenuInfo schema_mgmt_menu[] = {
   { "List of Entities", &Probe::ListEntityTypesCmd},
   { "Describe Entity ", &Probe::DescribeEntityCmd},
//   { "Select Schema   ", &Probe::SelectSchemaCmd},
   { 0 }
};

static DPMenuInfo instance_mgmt_menu[] = {
   { "Create                  ", Probe::CreateInstanceCmd},
   { "Modify                  ", Probe::ModifyInstanceCmd},
   { "View                    ", Probe::ViewInstanceCmd},
   { "Close                   ", Probe::CloseInstanceCmd},
   { "Save Complete           ", Probe::SaveComplInstanceCmd},
   { "Save Incomplete         ", Probe::SaveIncomplInstanceCmd},
   { "Replicate               ", Probe::ReplicateInstanceCmd},
   { "Unmark                  ", Probe::UnmarkInstanceCmd},
   { "Delete                  ", Probe::DeleteInstanceCmd},
//   { "Print                   ", Probe::PrintInstanceCmd},
   { "Select from List        ", Probe::SelectInstanceCmd},
//   { "Incomplete Instance List", Probe::IncompleteInstanceCmd},
   { 0 }
};

static DPMenuInfo file_mgmt_menu[] = {
   { "Read Exchange File  ", Probe::ReadExchangeFileCmd   },
   { "read Exchange File (pre-TC) ", Probe::ReadExchangeFileCmdPreTC   },
   { "Append From Exchange File", Probe::AppendExchangeFileCmd },
   { "Write Exchange File ", Probe::WriteExchangeFileCmd  },
   { "                            ", Probe::DoNothing   },
   { "Read Working File   ", Probe::ReadWorkingFileCmd    },
   { "Read Working File (pre-TC)  ", Probe::ReadWorkingFileCmdPreTC    },
   { "Append From Working File ", Probe::AppendWorkingFileCmd  },
   { "Write Working File  ", Probe::WriteWorkingFileCmd   },
   { "                            ", Probe::DoNothing   },
   { "Choose Backup File",  &Probe::ChooseBackupFileCmd},
   { "Write Backup File",  &Probe::WriteBackupFileCmd},
   { "                            ", Probe::DoNothing   },
   { "Verify Instances    ", Probe::VerifyInstsCmd   },
   { "Remove Deleted Instances    ", Probe::RemoveDelInstsCmd   },
   { "Clear Entity Instance List  ", Probe::ClearInstanceListCmd   },
   { "                            ", Probe::DoNothing   },
   { "Edit Header Instances  ", Probe::EditHeaderInstancesCmd   },
   { 0 }
};

void Probe::AssignFileId(STEPentity *se)
{
    int fId = instMgr->MaxFileId() + 1;
    se->STEPfile_id = (fId > 0) ? fId : 1; 
}

///////////////////////////////////////////////////////////////////////////////
//
//   Constructors, Destructors, Init and Builder functions, and Run.
//
///////////////////////////////////////////////////////////////////////////////

void Probe::Run() {
    if(debug_level >= PrintFunctionTrace) cout << "Probe::Run()\n";
    Event e;
    static int saveCount = 0;

    do {
        Read(e);
	if(e.leftmouse)
	    cout << "left\n";
	else if(e.middlemouse)
	    cout << "middle\n";
	else if(e.rightmouse)
	    cout << "right\n";
	switch (e.eventType)
	{
	  case MotionEvent:
	    cout << "Motion Event\n\n";
	    break;
	  case DownEvent:
	    cout << "Down Event\n\n";
	    break;
	  case UpEvent:
	    cout << "Up Event\n\n";
	    break;
	  case KeyEvent:
	    cout << "Key Event\n\n";
	    break;
	  case EnterEvent:
	    cout << "Enter Event\n\n";
	    break;
	  case LeaveEvent:
	    cout << "Leave Event\n\n";
	    break;
	  case FocusInEvent:
	    cout << "Focus In Event\n\n";
	    break;
	  case FocusOutEvent:
	    cout << "Focus Out Event\n\n";
	    break;
	  default:
	    cout << "? Event\n\n";
	    break;
	}
	e.target->Handle(e);
	if(saveCount++ >= autoSaveCount)
	{
	    AutoSave();
	    saveCount = 0;
	}
    } while (e.target != nil);
}
	//////////////////////////////////

int Probe::ChooseAutoSaveFile()
{
    static Interactor *root;

    static MyFileChooser* writeChooser;

//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;

    const char * restrict = worldPtr->GetAttribute("Restrict");

    if(!writeChooser){
	char* dir = 0;
	const int maxPath = MAX_PATH_LEN+2;
	char dirAlt[maxPath];

	if(getcwd(dirAlt, maxPath)) dir = dirAlt;
	else dir = "~";
	int rows = 10, cols = 24;
	writeChooser = new MyFileChooser(
				"Please select a BACKUP file: ", 
				" ", 
				(const char *)dir, rows, cols, " Select ");
//IVBUG	writeChooser->SetName("Write Backup File Selector");
//IVBUG	writeChooser->SetIconName("WBFS");
	root = InsertInteractor(writeChooser, Center);
	if(restrict[0] == '1')
	    writeChooser->Restrict(1);
    }
    else {
	root = InsertInteractor(writeChooser, Center);
//	writeChooser->Map(root, true);
    }
    writeChooser->UpdateDir();
    writeChooser->SelectFile();
    if(writeChooser->Accept()){
	autoSaveFile = (char *)writeChooser->Choice();
//	writeChooser->Unmap(root);    //    RemoveInteractor(writeChooser);
	RemoveInteractor(writeChooser);
	return 1;
    }
    RemoveInteractor(writeChooser);
//    writeChooser->Unmap(root);    //    RemoveInteractor(writeChooser);
    return 0;
}

int Probe::AutoSave()
{
    cout << "auto saving\n";
//    ostream* out  = new ostream(autoSaveFile, io_writeonly, 
//				a_create);
    ofstream* out  = new ofstream(autoSaveFile);
//    if(out->is_open() && out->writable())
    if((!(*out)) == 0)
    {
	Severity sev = fileMgr->WriteWorkingFile(*out);
	if (sev < SEVERITY_NULL) 
	  {
	      ErrorMsg(fileMgr->Error().UserMsg());
	      cerr << fileMgr->Error().DetailMsg();
	      fileMgr->Error().ClearErrorMsg();
	  }
	else 
	  {
	      char asStr[BUFSIZ];
	      asStr[0] = '\0';
	      sprintf(asStr, 
		 "Successfully wrote backup file to: %s", autoSaveFile);
	      ErrorMsg(asStr);
	  }	
	delete out;
	return 1;
    }
    else
    {
	delete out;
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
	char asStr[BUFSIZ];
	asStr[0] = '\0';
	sprintf(asStr, "Backup file: %s could not be opened for writing.",
		autoSaveFile);
	ErrorMsg(asStr);
	if(ChooseAutoSaveFile())
	{
	    return AutoSave();
	}
	else {
	    ErrorMsg("Skipping Backup for now.");
	    return 0;
	}
	
    }
}
	//////////////////////////////////

Probe::Probe() 
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::Probe()\n";

    readUsingTC = 1; // read using technical corrigendum
    instMgr = new InstMgr();
    registry = new Registry(SchemaInit);
    fileMgr = new STEPfile(*registry, *instMgr);
    cmdMgr = new CmdMgr;
    headerDisplay = 0;

//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;

    const char * restrict = worldPtr->GetAttribute("Restrict");

//    static char* dir = EXCHANGE_FILE_DIR;
    char* dir = 0;
    const int maxPath = MAX_PATH_LEN+2;
    char dirAlt[maxPath];

    if(getcwd(dirAlt, maxPath)) dir = dirAlt;
    else dir = "~";
    int rows = 10, cols = 24;
    fileChooser = new sclFileChooser("Read Exchange File", 
				"Please select an exchange file: ", 
				 (const char *)dir, rows, cols, " Execute ");
//				 Center, restrict);
    if(restrict[0] == '1')
	fileChooser->Restrict(1);
//IVBUG    fileChooser->SetName(" File Chooser");
//IVBUG    fileChooser->SetIconName("FileChooser");

    Init();

    b = ProbeBody();
    Insert(b);
}
	//////////////////////////////////

Probe::~Probe()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::~Probe()\n";

    delete instMgr;
    delete registry;
    
    delete entityInstanceList;

    delete entityTypeList;

    delete functionsMenuBar;
    delete functionsMenu;

    delete schemaMgmtMenuBar;
    delete schemaMgmtMenu;

    delete instanceMgmtMenuBar;
    delete instanceMgmtMenu;

    delete fileMgmtMenuBar;
    delete fileMgmtMenu;

    delete errorDisp;
}
	//////////////////////////////////

void Probe::InitMenus()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::InitMenus()\n";

    functionsMenuBar = new MyMenuBar;
    functionsMenu = MakePulldown("  Quit  ", quit_menu);
    functionsMenuBar->Include(functionsMenu);

    schemaMgmtMenuBar = new MyMenuBar;
    schemaMgmtMenu = MakePulldown("  Schema Mgmt  ", schema_mgmt_menu);
    schemaMgmtMenuBar->Include(schemaMgmtMenu);

    instanceMgmtMenuBar = new MyMenuBar;
    instanceMgmtMenu = 
		MakePulldown("  Instance Mgmt  ", instance_mgmt_menu);
    instanceMgmtMenuBar->Include(instanceMgmtMenu);

    fileMgmtMenuBar = new MyMenuBar;
    fileMgmtMenu = MakePulldown("  File Management  ", file_mgmt_menu);
//    fileMgmtMenuBar = new MenuBar;
//    fileMgmtMenu = MakePulldown2("  File Management  ", file_mgmt_menu);

    fileMgmtMenuBar->Include(fileMgmtMenu);
}
	//////////////////////////////////
extern const int defaultCmdSize;
extern lmCommand defaultCmds[];

void Probe::InitEntityInstanceList()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::InitEntityInstanceList()\n";
//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;


    defaultCmds[0] = lmCommand('s', SAVE_COMPLETE_CMD_COL);
    defaultCmds[1] = lmCommand('S', SAVE_COMPLETE_CMD_COL);
    defaultCmds[2] = lmCommand('i', SAVE_INCOMPLETE_CMD_COL);
    defaultCmds[3] = lmCommand('I', SAVE_INCOMPLETE_CMD_COL);
    defaultCmds[4] = lmCommand('m', MODIFY_CMD_COL);
    defaultCmds[5] = lmCommand('M', MODIFY_CMD_COL);
    defaultCmds[6] = lmCommand('v', VIEW_CMD_COL);
    defaultCmds[7] = lmCommand('V', VIEW_CMD_COL);
    defaultCmds[8] = lmCommand('d', DELETE_CMD_COL);
    defaultCmds[9] = lmCommand('D', DELETE_CMD_COL);
    defaultCmds[10] = lmCommand('c', CLOSE_CMD_COL);
    defaultCmds[11] = lmCommand('C', CLOSE_CMD_COL);
    defaultCmds[12] = lmCommand('r', REPLICATE_CMD_COL);
    defaultCmds[13] = lmCommand('R', REPLICATE_CMD_COL);
    defaultCmds[14] = lmCommand('x', EXECUTE_CMD_COL);
    defaultCmds[15] = lmCommand('X', EXECUTE_CMD_COL);
    defaultCmds[16] = lmCommand('u', UNMARK_CMD_COL);
    defaultCmds[17] = lmCommand('U', UNMARK_CMD_COL);

    entityInstanceList = new seInstListDisplay();
//    entityInstanceList->SetInitSize();
    worldPtr->InsertToplevel(entityInstanceList, this, 0, 0, 
			  6 /* 6 is BottomLeft */);
//    entityInstanceList->SendResize(0, 19, 623, 802);

    const char * initEfile = worldPtr->GetAttribute("ExchangeFile");
    const char * initWfile = worldPtr->GetAttribute("WorkFile");
    if(initWfile && initEfile)
    {
	cerr << "** Error ** Please supply only one initial file.";
	fflush(stderr);
	exit(-1);
    }
    else if(initWfile)
    {
	Severity sev = fileMgr -> ReadWorkingFile (initWfile);
	if (sev < SEVERITY_NULL) 
	{
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	    ErrorMsg(fileMgr->Error().UserMsg());
	    cerr << fileMgr->Error().DetailMsg();
	    fileMgr->Error().ClearErrorMsg();
	}
	ListEntityInstances();
    }
    else if(initEfile)
    {
	Severity sev = fileMgr -> ReadExchangeFile (initEfile);
	if (sev < SEVERITY_NULL) 
	{
	    // read NOT successful 
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	    ErrorMsg(fileMgr->Error().UserMsg());
	    cerr << fileMgr->Error().DetailMsg();
	    fileMgr->Error().ClearErrorMsg();
	}
	ListEntityInstances();
    }
}
	//////////////////////////////////

void Probe::InitEntityTypeList()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::InitEntityTypeList()\n";
//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;

//    Coord l, b;

    entityTypeList = new seTypeListDisplay();

    Shape *s = entityTypeList->GetShape();
//    worldPtr->Align(BottomRight, s->width, s->height, l, b);
//    worldPtr->InsertToplevel(entityTypeList, this, l, b, BottomRight);
    worldPtr->InsertToplevel(entityTypeList, this, (int)(worldPtr->width()), 0, 
			  8 /* 8 is BottomRight */);
}
	//////////////////////////////////

void Probe::Init()
{
//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;


    if(debug_level >= PrintFunctionTrace) cout << "Probe::Init()\n";
//IVBUG    SetName("Data Probe");
//IVBUG    SetIconName("Data Probe");

    errorDisp = new ErrorDisplay("Messages:", "Clear");

    const char *freqStr = worldPtr->GetAttribute("AutoSaveFreq");
    if(sscanf((char *)freqStr, "%d", &autoSaveCount) < 1)
    {
	cout << "error no initialized number for saving.\n";
	autoSaveCount = 30;
    }
    autoSaveFile = (char *)worldPtr->GetAttribute("AutoSaveFile");

    static char auto_save_file[MAX_PATH_AND_FILE_LEN+2];
    auto_save_file[0] = '\0';
    if(!autoSaveFile)
    {
	char *saveFileDir;
	const int maxPath = MAX_PATH_LEN+2;
	if(getcwd(auto_save_file, maxPath)) 
	{
	    strcat(auto_save_file, "/");
	}
	else 
	{
	    saveFileDir = (char *)worldPtr->GetAttribute("AutoSaveFileDir");
	    DirObj saveDir(saveFileDir);
	    strcpy(auto_save_file, saveDir.Normalize(saveFileDir));
	}
	char *saveFileName = (char *)worldPtr->GetAttribute("AutoSaveFileName");
	strcat(auto_save_file, saveFileName);
	autoSaveFile = auto_save_file;
    }

    cout << VERSION << endl;

    InitMenus();
    InitEntityInstanceList();
    InitEntityTypeList();
}
	//////////////////////////////////
PulldownMenu* MakePulldown2( const char* name, struct DPMenuInfo* item)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::MakePulldown()\n";
    Message * m = new Message(name, Center);

//    MyPulldownMenu* menu = new MyPulldownMenu(m);
    PulldownMenu* menu = new PulldownMenu(m);

    for (DPMenuInfo* i = item; i->lbl != 0; i++) {
//	menu->Include(new DPMenuItem(i->lbl, i->func, this));
	menu->Include(new MenuItem( (const char *)i->lbl ) );
    }
    return menu;
}

MyPulldownMenu* Probe::MakePulldown( const char* name, struct DPMenuInfo* item)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::MakePulldown()\n";
    Message * m = new Message(name, Center);

    MyPulldownMenu* menu = new MyPulldownMenu(m);

    for (DPMenuInfo* i = item; i->lbl != 0; i++) {
	menu->Include(new DPMenuItem(i->lbl, i->func, this));
    }
    return menu;
}
	//////////////////////////////////

Interactor *Probe::SEEHelpBody()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::SEEHelpBody()\n";
    return new MarginFrame(
	new VBox (
	    new HBox(
		new HGlue,
		new BoldMessage(
		"Key Bindings & Button Codes for Step Entity Editor Windows:"),
		new HGlue
	    ),
	    new VGlue (round(.1*inch), round(.1*inch), 0),
	    new Message(
		"Press a button or type control-x followed by the appropriate key below to execute a command."),
	    new VGlue (round(.1*inch), round(.1*inch), 0),
	    new HBox(
		new VBox(
		    new BoldMessage("closes existing window"),
		    new HBox(
			new VBox(
			    new BoldMessage("s"),
			    new BoldMessage("i"),
			    new BoldMessage("c"),
			    new BoldMessage("d")
			),
			new VBox(
			    new Message(" - save complete"),
			    new Message(" - save incomplete"),
			    new Message(" - cancel edits"),
			    new Message(" - delete STEP entity")
			)
		    )
		),
		new HGlue(round(.1*inch)),
		new VBox(
		    new BoldMessage("opens a new window"),
		    new HBox(
			new VBox(
			    new BoldMessage("r"),
			    new BoldMessage("e"),
			    new Message(" "),
			    new BoldMessage("t")
			),
			new VBox(
			    new Message(" - replicate instance"),
			    new Message(" - edit existing instance/"),
			    new Message("   create and edit new instance"),
			    new Message(" - describe STEP entity type")
			)
		    )
		),
		new HGlue(round(.1*inch)),
		new VBox(
		    new BoldMessage("get value from list"),
		    new HBox(
			new VBox(
			    new BoldMessage("m"),
			    new BoldMessage("l"),
			    new Message(" "),
			    new BoldMessage("a")
			),
			new VBox(
			    new Message(
			       " - select marked instance from instance list"),
			    new Message(
				" - pop up list of valid attribute values"),
			    new Message(" "),
			    new Message(" - describe attribute type")
			)
		    )
		)
	    )
	), 5
    );
}

///////////////////////////////////////////////////////////////////////////////

Interactor *Probe::ProbeBody ()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::ProbeBody()\n";
    static char schemaName[BUFSIZ];
    int count = 0, length = 0;
    const SchemaDescriptor *schemaDesc;

    registry -> ResetSchemas ();

    while (schemaDesc = registry -> NextSchema () ) {
//	printf("found schema '%s'\n",schemaDesc->Name());
	if(debug_level >= PrintValues) cout << schemaDesc->Name() << "\n";
	if(count)
	    strcat(schemaName, ", ");
	count++;
	strcat(schemaName, schemaDesc->Name());
    }
    // DAR - temp fix so that main win won't be too wide:
    schemaName[82] = '\0';
//    printf("found schema '%s' : %d\n",schemaName, strlen(schemaName));

  return new Frame(
    new VBox(
	new VBox(
	    new VGlue (round(.1 * inch), 0),
	    new HBox(
		new HGlue,
		new Banner(""," Data Probe ",""),
		new HGlue
	    ),
	    new VGlue (round(.01 * inch), 0),
	    new HBox(
		new HGlue,
//		     (strlen(schemaName) > 40)?new Message(schemaName, Center):
//		     new Banner("", schemaName, ""),
		new Banner("", schemaName, ""),
		new HGlue
	    ),
	    new VGlue (round(.1 * inch), 0)
	),
	new HBorder,
	new HBox (
		new HGlue (round(.1 * inch), 0),
		new Frame(functionsMenuBar),
		new HGlue (round(hspace * inch), 0),
		new Frame(fileMgmtMenuBar),
		new HGlue (round(.1 * inch), 0)
/*
	    new HBox (
		new HGlue (round(.1 * inch), 0),
		new Frame(functionsMenuBar),
		new HGlue (round(hspace * inch), 0),
		new Frame(schemaMgmtMenuBar)
	    ),
	    new HBox (
//		new HGlue (round(hspace * inch), 0),
//		new Frame(instanceMgmtMenuBar),
		new HGlue (round(hspace * inch), 0),
		new Frame(fileMgmtMenuBar),
		new HGlue (round(.1 * inch), 0)
	    )
*/
	),
	new HBorder,
	SEEHelpBody(),
	new HBorder,
	new MarginFrame(errorDisp, 5)
    )
  );
}

void Probe::SetProbeCursor(Cursor *c)
{
    SetCursor(c);
    entityInstanceList->SetCursor(c);
    entityTypeList->SetCursor(c);
}

///////////////////////////////////////////////////////////////////////////////
//
//  Insert objects into world etc.
//
///////////////////////////////////////////////////////////////////////////////

void Probe::InsertSEE (StepEntityEditor * see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::InsertSEE()\n";

    Frame* framedSEE = new Frame(see, 2);
//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;

//    Coord x, y;

//    Align(Center, 0, 0, x, y);
//    GetRelative(x, y, worldPtr);
//    worldPtr->InsertTransient(framedSEE, this, x, y, Center);
//    worldPtr->InsertTransient(framedSEE, this);
    char iconName[128];
    char winName[128];
    iconName[0] = '\0';
    winName[0] = '\0';

    const char *seName = see->GetStepEntity()->EntityName();
    if(seName){
	sprintf(iconName, "#%d %s", see->GetStepEntity()->STEPfile_id, 
		seName);
    } else
	sprintf(iconName, "#%d", see->GetStepEntity()->STEPfile_id);
    sprintf(winName, "dp - %s", iconName);
   
//    framedSEE->SetName("Data Probe STEP entity");
//IVBUG    framedSEE->SetName(winName);
//IVBUG    framedSEE->SetIconName(iconName);

    worldPtr->InsertToplevel(framedSEE, this);

/*
    if(framedSEE->AttributeIsSet("iconName"))
	cout << "icon Name is set.\n";
    else
	cout << "icon Name is NOT set.\n";
    cout << "attribute Icon Name: '" << framedSEE->GetAttribute("iconName")
	<< "'\n";
    Style *st = framedSEE->GetTopLevelWindow()->style();
    if(st)
    {
	if(st->value_is_on("iconName"))
	{
	    cout << "managed window's icon Name is set.\n";
	    String str;
	    st->find_attribute("iconName", str);
	    cout << "attribute Icon Name: '" << str.string() << "'\n";
	}
	else
	{
	    cout << "managed window's icon Name is NOT set.\n";
	    st->attribute("iconName", "Dave");
	    String str;
	    st->find_attribute("iconName", str);
	    cout << "attribute Icon Name: '" << str.string() << "'\n";
	}
    }
    else
	cout << "managed window's icon Name is NOT set.\n";
//    framedSEE->GetTopLevelWindow()->iconify();
//    framedSEE->GetTopLevelWindow()->icon_bitmap();
*/

//SetIconName(iconName);
//    worldPtr->InsertToplevel(framedSEE, this);
}

	//////////////////////////////////

void Probe::RemoveSEE (StepEntityEditor* see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::RemoveSEE()\n";

//    World *worldPtr = GetWorld();
    World *worldPtr = ::world;

    Frame* framedSEE = (Frame*) see->Parent();

    int ismapped = see->IsMapped();
    if(ismapped)
    {
	worldPtr->Remove(framedSEE);
	framedSEE->Remove(see);
	delete framedSEE;
    }
//    SetSEEDelete(see);
}

void Probe::InsertSED (StepEntityDescriptor * sed)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::InsertSED()\n";

    Frame* framedSED = new Frame(sed, 2);
//    World* worldPtr = GetWorld();
    World *worldPtr = ::world;

//    Coord x, y;

//    Align(Center, 0, 0, x, y);
//    GetRelative(x, y, worldPtr);
//    worldPtr->InsertTransient(framedSEE, this, x, y, Center);
//    worldPtr->InsertTransient(framedSEE, this);
    char iconName[128];
    char winName[128];
    iconName[0] = '\0';
    winName[0] = '\0';

    const char *edName = sed->EntityDesc()->Name();
    if(edName){
	sprintf(iconName, "%s", edName);
    }
    sprintf(winName, "dp - %s", iconName);
   
//    framedSED->SetName("Data Probe STEP entity");
//IVBUG    framedSED->SetName(winName);
//IVBUG    framedSED->SetIconName(iconName);
    worldPtr->InsertToplevel(framedSED, this);
}

	//////////////////////////////////

void Probe::RemoveSED (StepEntityDescriptor * sed)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::RemoveSED()\n";

//    World* worldPtr = GetWorld();
    World *worldPtr = ::world;

    Frame* framedSED = (Frame*) sed->Parent();

    int ismapped = sed->IsMapped();
    if(ismapped)
    {
	worldPtr->Remove(framedSED);
	framedSED->Remove(sed);
	delete framedSED;
    }
}

	//////////////////////////////////

Interactor *Probe::InsertInteractor (Interactor * i, Alignment align, 
				     Interactor * alignTo)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::InsertInteractor()\n";
//    World* worldPtr = GetWorld();
    World *worldPtr = ::world;


    Frame* framedInteractor = new Frame(i, 2);
    Coord x, y;

/*
    if(IsMapped()){
	Align(BottomCenter, i->GetShape()->width, i->GetShape()->height, x, y);
	GetRelative(x, y, worldPtr);
	worldPtr->InsertTransient(framedInteractor, this, x, y, align);
    } else {
	x = y = 0;
	worldPtr->InsertTransient(framedInteractor, this, x, y, align);
    }
*/
    if(alignTo){
	alignTo->Align(align, i->GetShape()->width, i->GetShape()->height, 
			x, y);
//	cout << "align X: " << x << "   align Y: " << y << "\n";
	alignTo->GetRelative(x, y, worldPtr);
//	cout << "relative X: " << x << "   relative Y: " << y << "\n";
//	worldPtr->InsertTransient(framedInteractor, this, x, y, align);
	worldPtr->InsertTransient(framedInteractor, alignTo, x, y, align);
    } else {
	x = (int)(worldPtr->width()/2);
	y = (int)(worldPtr->height()/2);
	worldPtr->InsertTransient(framedInteractor, this, x, y, align);
    }
    return(framedInteractor);
}

	//////////////////////////////////

void Probe::RemoveInteractor (Interactor* i)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::RemoveInteractor()\n";
//    World* worldPtr = GetWorld();
    World *worldPtr = ::world;

    Frame* framedInteractor = (Frame*) i->Parent();

    worldPtr->Remove(framedInteractor);
    framedInteractor->Remove(i);
    delete framedInteractor;
}

///////////////////////////////////////////////////////////////////////////////
//
//  StepEntityEditor management functions
//
///////////////////////////////////////////////////////////////////////////////

StepEntityEditor *Probe::GetStepEntityEditor(MgrNode *mn,
					int modifyBool, int pinBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::GetStepEntityEditor()\n";
    StepEntityEditor *tempSEE;

    tempSEE = new StepEntityEditor(mn, instMgr, modifyBool, pinBool);
//    tempSEE->SetInstMgr(instMgr);
    tempSEE->SetProbe(this);

    Font* f = new Font("*fixed-medium-r-normal--13*");
    tempSEE->GetShape()->hunits = f->Width("n");
    tempSEE->GetShape()->vunits = f->Height();

    return tempSEE;
}
	//////////////////////////////////

StepEntityEditor *Probe::GetStepEntityEditor(STEPentity *se, 
					int modifyBool, int pinBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::GetStepEntityEditor()\n";
    StepEntityEditor *tempSEE;

    tempSEE = new StepEntityEditor(se, modifyBool, pinBool);
    tempSEE->SetInstMgr(instMgr);
    tempSEE->SetProbe(this);
    Font* f = new Font("*fixed-medium-r-normal--13*");
    tempSEE->GetShape()->hunits = f->Width("n");
    tempSEE->GetShape()->vunits = f->Height();

    return tempSEE;
}

///////////////////////////////////////////////////////////////////////////////
//
// command options called from the StepEntityEditor
//
///////////////////////////////////////////////////////////////////////////////

void Probe::RemoveSeeIfNotPinned(StepEntityEditor* see, MgrNode *mn)
{
    if( ( (mn->DisplayState() == mappedWrite) || 
          (mn->DisplayState() == mappedView) 
	) && !see->Pinned() )
    {
	RemoveSEE(see);
	int index = mn->ArrayIndex();
	entityInstanceList->WriteCmdChar(' ', MODIFY_STATE_COL, index);
// not sure we want to do this.  If this is done, would need to do more than 
// this. i.e. if a 'v', 'm', or 'c' happens to be here, would need to remove 
// the entity from the appropriate 'cmd-to-do' list in the CmdMgr.
//	entityInstanceList->WriteCmdChar(' ', MODIFY_CMD_COL, index);
	mn->ChangeState(notMapped);
    }
}
	//////////////////////////////////

// boolean i.e. true = 1 and false = 0
boolean Probe::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/* // this doesn't work
    if(canvas)
    {
	int bool = canvas->window()->is_mapped();
	return bool;
    }
    else
	return false;
*/
}
	//////////////////////////////////

void Probe::seeSaveComplete(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeSaveComplete()\n";
/*    ErrorMsg("STEPentity saved complete.\n"); */

    MgrNode *mn = see->GetMgrNode();
    int index = mn->ArrayIndex();
    mn->ChangeState(completeSE);

// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
// 	no it doesn't not now - DAS
    mn->Remove(); 	// remove from list of intended actions for CmdMgr
   // this writes over the: 'd' for delete, 's' for save, or 'i' for incomplete
    entityInstanceList->WriteCmdChar(' ', SAVE_COMPLETE_CMD_COL, index);

    RemoveSeeIfNotPinned(see, mn);

    SCLstring seText;
    see->GetStepEntity()->STEPwrite(seText);
    entityInstanceList->ReplaceText(seText.chars(), index) ;

    entityInstanceList->WriteCmdChar(SAVE_COMPLETE_STATE_CHAR,
	SAVE_COMPLETE_STATE_COL, index);	// write the state char
}
	//////////////////////////////////

void Probe::seeSaveIncomplete(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::seeSaveIncomplete()\n";
/*    ErrorMsg("STEPentity saved incomplete.\n");*/

    MgrNode *mn = see->GetMgrNode();
    int index = mn->ArrayIndex();
    mn->ChangeState(incompleteSE);
// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
// 	no it doesn't not now - DAS
    mn->Remove(); 	// remove from list of intended actions for CmdMgr
   // this writes over the: 'd' for delete, 's' for save, or 'i' for incomplete
    entityInstanceList->WriteCmdChar(' ', SAVE_INCOMPLETE_CMD_COL, index);

    RemoveSeeIfNotPinned(see, mn);

    SCLstring seText;
    see->GetStepEntity()->STEPwrite(seText);
    entityInstanceList->ReplaceText(seText.chars(), index);

    entityInstanceList->WriteCmdChar(SAVE_INCOMPLETE_STATE_CHAR,
	SAVE_INCOMPLETE_STATE_COL, index);	// write the state char
}
	//////////////////////////////////

void Probe::seeCancel(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeCancel()\n";
/*    ErrorMsg("STEPentity canceled edit.");*/

    MgrNode *mn = see->GetMgrNode();
    RemoveSeeIfNotPinned(see, mn);
}
	//////////////////////////////////

void Probe::seeDelete(StepEntityEditor *see) 
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeDelete()\n";
/*    ErrorMsg("STEPentity delete.");*/

    MgrNode *mn = see->GetMgrNode();
    int index = mn->ArrayIndex();
    mn->ChangeState(deleteSE);
// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
// 	no it doesn't not now - DAS
    mn->Remove(); 	// remove from list of intended actions for CmdMgr
   // this writes over the: 'd' for delete, 's' for save, or 'i' for incomplete
    entityInstanceList->WriteCmdChar(' ', DELETE_CMD_COL, index);

    RemoveSeeIfNotPinned(see, mn);

    entityInstanceList->WriteCmdChar(DELETE_STATE_CHAR, DELETE_STATE_COL, 
				     index);	// write the state char
}
	//////////////////////////////////

void Probe::seeOpenSED(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeOpenSED()\n";
/*    ErrorMsg("SEE Open SED.");*/
    StepEntityDescriptor *sed = 
	new StepEntityDescriptor( see->GetStepEntity()->eDesc );
/*	new StepEntityDescriptor( see->GetStepEntity()->EntityDescriptor );*/
		
    InsertSED(sed);
}

void Probe::seeReplicate(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeReplicate()\n";
/*    ErrorMsg("SEE replicate.");*/
    STEPentity *seExist = see->GetStepEntity();
/*    EntityDescriptor *ed = see->GetStepEntity()->EntityDescriptor;*/
    EntityDescriptor *ed = see->GetStepEntity()->eDesc;
//    STEPentity *se = ed->NewSTEPentity();
    STEPentity *seNew;
//    if(seNew = ed->NewSTEPentity()){
    if(seNew = seExist->Replicate()){
	SCLstring instanceInfo;

		// BUG remove the next line when it is generated by the core
	AssignFileId(seNew);
		// this will probably be replaced with an InstanceMgr function.
		// also this probably doesn't make sense until the se can be
		// marked as incomplete since it doesn't have any values yet
	MgrNode *mn = instMgr->Append(seNew, newSE);
	if(!mn->displayNode())
	    mn->displayNode() = new DisplayNode(mn);
	DisplayNode *dn = mn->displayNode();
	dn->SEE(GetStepEntityEditor(mn, 0));
	see = (StepEntityEditor *)dn->SEE();

	seNew->STEPwrite(instanceInfo);
	int index = entityInstanceList->Append( instanceInfo.chars() );
	mn->ArrayIndex(index);
	entityInstanceList->WriteCmdChar(NEW_STATE_CHAR, 
					 NEW_STATE_COL, index);
	entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
					 MODIFY_STATE_COL, index);

	mn->ChangeState(mappedWrite);
	InsertSEE(see);
//	see->Validate();
	see->Edit();
    } else {
	ErrorMsg("Could not replicate selected instance.");
    }
}

	//////////////////////////////////
void Probe::BuildSubtypeList(ListItemChooser *lic, EntityDescriptor *ed)
{
//    EntityDescriptor *ed = 
//	(EntityDescriptor *)td->NonRefTypeDescriptor();
//    int choiceCount = 0;
    const char *entityTypeName = ed->Name();

    // add the entity type referenced by the attribute to choices
    // available to be created if it is not an abstract entity
    if(((EntityDescriptor *)ed)->AbstractEntity().asInt() == LFalse)
    {
	lic->Append(entityTypeName);
//	choiceCount++;
    }

    // add the 1st generation subtypes of the attribute's ref entity
    const EntityDescriptorList *subtypeList = 
				&( ((EntityDescriptor *)ed)->Subtypes());
    EntityDescLinkNode *subtypePtr = 
				(EntityDescLinkNode *)subtypeList->GetHead();
//    EntityDescLinkNode *subtypePtr = subtypeList->GetHead();
    EntityDescriptor *entDesc = 0;
    while( subtypePtr != 0)
    {
	entDesc = subtypePtr->EntityDesc();
	BuildSubtypeList(lic, entDesc);
/*
	LOGICAL logi = entDesc->AbstractEntity().asInt();
//	if(entDesc->AbstractEntity().asInt() == F)
	if(logi == F)
	{
	    lic->Append(entDesc->Name());
//	    choiceCount++;
	}
*/
	subtypePtr = (EntityDescLinkNode *)subtypePtr->NextNode();
    }
}

int Probe::seeEdit(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeEdit()\n";
/*    ErrorMsg("SEE edit.");*/

    seeAttrRow *attrRow = see->AttrRowList()->CurRow();
/*    const AttrDescriptor *ad = attrRow->StepAttr()->AttrDescriptor;*/
    const AttrDescriptor *ad = attrRow->StepAttr()->aDesc;

    switch(ad->NonRefType())
    {
      case ENUM_TYPE:
      case LOGICAL_TYPE:
      case BOOLEAN_TYPE:
	  return seeEnumEdit(see);
      case ENTITY_TYPE:
	  return seeEntityEdit(see);
      case SELECT_TYPE:
	  return seeSelectEdit(see);
      default:
	  see->NotifyUser("Attribute base type needs to be ENUM or ENTITY", 1);
	  return 0;
    }
}

int Probe::seeSelectEdit(StepEntityEditor *see)
{
    int rval = 0;
    seeAttrRow *attrRow = see->AttrRowList()->CurRow();
    STEPattribute *sa = attrRow->StepAttr();
    const TypeDescriptor *td = sa->ReferentType();
    if (td && td->NonRefType() == SELECT_TYPE)
    {
      ListItemChooser *lic = 
	new ListItemChooser(new ButtonState(0),
			      "Choose the underlying type for the SELECT",
			      8, 24, " Choose ");

	td = td->NonRefTypeDescriptor();
	SelectTypeDescriptor *std = (SelectTypeDescriptor *)td;

		// the TypeDescriptor for the current type of the select
	const TypeDescriptor * curType = sa->ptr.sh->CurrentUnderlyingType();
	const TypeDescriptor * tmp = 0;
	int curTypeIndex = -1;
	int index = 0;

        SCLstring s;
	TypeDescLinkNode *tdln = (TypeDescLinkNode *)std->Elements().GetHead();
	while(tdln)
	{
	    tmp = tdln->TypeDesc();
	    if(tmp == curType)
		curTypeIndex = index;
	    lic->Append(tmp->AttrTypeName(s));
	    tdln = (TypeDescLinkNode *)tdln->NextNode();
	    index++;
	}
	InsertInteractor(lic, 2, see);
//    int SearchForward(char *substr, int startIndex, int ignoreCase = 1);
	if(!(curTypeIndex == -1))
	    lic->Select(curTypeIndex);
	if(lic->Accept())
	{
	    int choice = lic->Choice();
	    if(0 <= choice)
	    {
                istrstream oldAttr((const char *)attrRow->GetEditFieldText());
                strstreambuf oldValbuf;
                ostrstream newType;
                ostrstream newVal;  // new attribute field value

                // get selected type name and descriptor
                newType << (const char *)lic->String(choice) << ends;
                const TypeDescriptor *td = registry->FindType(newType.str(),0);

                // get the proper value from the old attribute

                int c = oldAttr.peek();
                // if attribute is a literal string or an aggregate
                // it might/will contain parentheses, so just read it.
                // literal strings start with single-quote, aggrs with '('.
                if (c == '\'' || c == '(')
                {
                    oldAttr.get(oldValbuf);
                    oldValbuf.sputc('\0');
                }

                // otherwise, if the selected type is a SELECT, the user
                // probably wants the given field to be specified within
                // the select, e.g., Str('foo') --> Sel1(Str('foo'))
                // ---FIX--- : it would be better if it checked that
                // the given field's type is a valid subtype of the SELECT
                // (this is why I made this a seperate case from above,
                //  even though the 'then' code is the same...)
                else if (td && (td->Type() == SELECT_TYPE))
                {
                    oldAttr.get(oldValbuf);
                    oldValbuf.sputc('\0');
                }

                // otherwise, read up until a possible open paren
                // this means we already have a type specified, and
                // need to replace it.
                else
                {
                    oldAttr.get(oldValbuf, '(');  // read until a '('
                    if (((c = oldAttr.peek()) != EOF) && (c != '\0'))
                    {
                        // if we're not at EOF or null, we hit a '(',
                        // and want to keep the value in the parens only.
                        oldAttr.get();           // skip '('
                        oldValbuf.seekpos(0);    // reset buffer;
                        oldAttr.get(oldValbuf);  // get attribute

                        // remove trailing right-paren ')'
                        oldValbuf.seekoff(-1, ios::end, ios::out);
                        oldValbuf.sputc('\0');
                    }
                    else
                        oldValbuf.sputc('\0');
                }

//                cout << "oldValbuf: " << oldValbuf.str() << endl;

                // re-write the attribute entry, with specified type
                // e.g., .FOOBAR. --> myenum(.FOOBAR.)
                ostrstream oldValstr;
                oldValstr << oldValbuf.str() << ends;
                newVal << newType.str() << "(" << oldValstr.str()
                       << ")" << ends;

//                cout << "newVal: " << newVal.str() << endl;

                // put new attribute entry into field... validation
                // will happen on return to StepEntityEditor::ExecuteCommand()
                attrRow->SetEditFieldText(newVal.str());
		rval = 1;
	    }
	    RemoveInteractor(lic);
	    lic->Flush();

	    delete lic;
	}		
	else
	{
	    RemoveInteractor(lic);
	    lic->Flush();

	    delete lic;
	}
    }
    return rval;
}

int Probe::seeEntityEdit(StepEntityEditor *see)
{
    seeAttrRow *attrRow = see->AttrRowList()->CurRow();
    const char *existFileIdStr = attrRow->GetEditFieldText();
    int fileId = 0;
    MgrNode *mn = 0;

    if ((1 == sscanf((char *)existFileIdStr, " #%d", &fileId)) || 
	(1 == sscanf((char *)existFileIdStr, " %d", &fileId)))
    {
	mn = instMgr->FindFileId(fileId);
	if(mn) {
	    int index = mn->ArrayIndex();
	    sedlModify(mn, index);
	    return 1;
	}
	else { 
	    see->NotifyUser("Cannot edit STEPentity... invalid File Id.", 1);
//	    ErrorMsg("Cannot edit STEPentity... invalid File Id.");
//	    fprintf(stderr, "%c" , RingBell);
//	    fflush(stderr);
	    return 0;
	}
    }
    else
    {
	const char *entityTypeName = attrRow->GetAttrTypeName();
	STEPentity *newEntity;

	const TypeDescriptor *td = attrRow->StepAttr()->ReferentType();
	if (td && td->NonRefType() == ENTITY_TYPE)
	{
	    ListItemChooser *lic = 
	      new ListItemChooser(new ButtonState(0),
				  "Choose the ENTITY type to create",
				  8, 24, " Create ");
	    EntityDescriptor *ed = 
				(EntityDescriptor *)td->NonRefTypeDescriptor();
	    BuildSubtypeList(lic, ed);
/*
	    EntityDescriptor *ed = 
				(EntityDescriptor *)td->NonRefTypeDescriptor();
	    int choiceCount = 0;

		// add the entity type referenced by the attribute to choices
		// available to be created if it is not an abstract entity
	    if(((EntityDescriptor *)ed)->AbstractEntity().asInt() == F)
	    {
		lic->Append(entityTypeName);
		choiceCount++;
	    }

	    // add the 1st generation subtypes of the attribute's ref entity
	    const EntityDescriptorList *subtypeList = 
				&( ((EntityDescriptor *)ed)->Subtypes());
	    EntityDescLinkNode *subtypePtr = 
				(EntityDescLinkNode *)subtypeList->GetHead();
	    EntityDescriptor *entDesc = 0;
	    while( subtypePtr != 0)
	    {
		entDesc = subtypePtr->EntityDesc();
		LOGICAL logi = entDesc->AbstractEntity().asInt();
//		if(entDesc->AbstractEntity().asInt() == F)
		if(logi == F)
		{
		    lic->Append(entDesc->Name());
		    choiceCount++;
		}
		subtypePtr = (EntityDescLinkNode *)subtypePtr->NextNode();
	    }
*/

/*
static const unsigned TopLeft = 0;
static const unsigned TopCenter = 1;
static const unsigned TopRight = 2;
static const unsigned CenterLeft = 3;
static const unsigned Center = 4;
static const unsigned CenterRight = 5;
static const unsigned BottomLeft = 6;
static const unsigned BottomCenter = 7;
static const unsigned BottomRight = 8;
static const unsigned Left = 9;
static const unsigned Right = 10;
static const unsigned Top = 11;
static const unsigned Bottom = 12;
static const unsigned HorizCenter = 13;
static const unsigned VertCenter = 14;
	    */
//	    InsertInteractor(lic, TopRight, see);
	    InsertInteractor(lic, 2, see);
	    lic->Select(0);
//	    lic->Browse();
	    if(lic->Accept())
	    {
	    	int choice = lic->Choice();
		if(0 <= choice && choice < lic->Count())
		{
		    entityTypeName = (const char *)lic->String(choice);
		}
		RemoveInteractor(lic);
		lic->Flush();

		delete lic;
	    }		
	    else
	    {
		entityTypeName = 0;

		RemoveInteractor(lic);
		lic->Flush();

		delete lic;
	    }
	}

	if( entityTypeName != 0 &&
	    (newEntity = registry->ObjCreate((char *)entityTypeName) ) && 
	    (newEntity != S_ENTITY_NULL) )
	{
		// BUG remove the next line when it is generated by the core?
		// this may be replaced with an InstMgr function.
	    AssignFileId(newEntity);	// assign fileId to new entity

		// append the new instance to the InstMgr
	    mn = instMgr->Append(newEntity, newSE);

		// assign fileId to the attr. field of the SEE
	    fileId = newEntity->GetFileId(); 
	    char fileIdStr[20];
	    sprintf(fileIdStr, "#%d", fileId);
	    attrRow->SetEditFieldText((const char *)fileIdStr);

		// write the new instance to the instance display list
	    SCLstring instanceInfo;
	    newEntity->STEPwrite(instanceInfo);
	    int index = entityInstanceList->Append( instanceInfo.chars() );
	    mn->ArrayIndex(index);
	    entityInstanceList->WriteCmdChar(NEW_STATE_CHAR, 
					     NEW_STATE_COL, index);
//	    entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
//					     MODIFY_STATE_COL, index);

	    sedlModify(mn, index);
	    return 1;
	} else {
	    see->NotifyUser("Selected type is not a STEPentity.", 1);
//	    ErrorMsg("Selected type is not a STEPentity.");
//	    fprintf(stderr, "%c" , RingBell);
//	    fflush(stderr);
	    return 0;
	}
    }
//    return 0;
}
	//////////////////////////////////

int Probe::seeEnumEdit(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeList()\n";
/*    ErrorMsg("SEE list enumeration.");*/

    char fileIdStr[20];

    seeAttrRow *attrRow = see->AttrRowList()->CurRow();

//    ObjType type             = attrRow->GetType();
//    const char *attrTypeName = attrRow->GetAttrTypeName();
    STEPattribute *currAttr  = attrRow->StepAttr();

    switch(currAttr->BaseType())
    {
      case ENUM_TYPE:
      case LOGICAL_TYPE:
      case BOOLEAN_TYPE:
	{
	    ListItemChooser *lic = new ListItemChooser(new ButtonState(0));
	    int numElements = currAttr->ptr.e->no_elements();
	    int i;
	    int currValue = currAttr->ptr.e->asInt();
	    SCLstring tmp;

	    if(currAttr->BaseType() == LOGICAL_TYPE)
	    {
		lic->Append("F");
		lic->Append("T");
		lic->Append("U");
	    }
	    else
	    {
		for(i = 0; i < numElements; i++)
		{
		    currAttr->ptr.e->put(i);
		    lic->Append(currAttr->ptr.e->asStr(tmp));
		}
	    }
	    InsertInteractor(lic, 2, see);
	    lic->Select(0);
//	    lic->Browse();
	    if(lic->Accept())
	    {
		int choice = lic->Choice();
		if(0 <= choice && choice < numElements)
		{
		    if(currAttr->BaseType() == LOGICAL_TYPE &&
		       choice == 2)
		    {
			currAttr->ptr.e->put(LUnknown);
		    }
		    else
		    {
			currAttr->ptr.e->put(choice);
		    }
		  attrRow->SetEditFieldText((const char *)lic->String(choice));
		}
		else
		    currAttr->ptr.e->put(currValue);
		RemoveInteractor(lic);
		lic->Flush();

		delete lic;
		return 1;
	    }		
	    else
	    {
		currAttr->ptr.e->put(currValue);
		RemoveInteractor(lic);
		lic->Flush();

		delete lic;
		return 0;
	    }
	}
      default:
	    see->NotifyUser("Wrong attribute type.", 1);
//	    fprintf(stderr, "%c" , RingBell);
//	    fflush(stderr);
	    return 0;
    } // end switch(type)
/*
    STEPentity *se;
    int fileId;
    char *entityName;
	const char *attrName = attrRow->GetAttrName();
	cout << "attribute Name: " << attrName << "\n";
	cout << "attribute Type Name: " << attrTypeName << "\n";
	cout << "chosen entity Name: " << entityName << "\n";
	cout << "chosen entity file id: " << fileIdStr << "\n";
*/
}

	//////////////////////////////////

int Probe::seeSelectMark(StepEntityEditor *see)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeSelectMark()\n";
/*    ErrorMsg("SEE select mark.");*/
    int index = entityInstanceList->Selection();
    if(0 <= index) // if there is a selected instance.
    {
	MgrNode *mn;
	STEPentity *se;
	int fileId;
	char fileIdStr[20];
	const char *entityName;

	seeAttrRow *attrRow = see->AttrRowList()->CurRow();

	BASE_TYPE type             = attrRow->GetType();
	const char *attrTypeName = attrRow->GetAttrTypeName();

	switch(type)
	{
	  case ENTITY_TYPE:
		    mn = instMgr -> GetMgrNode (index);
		    se = mn->GetSTEPentity();

		    fileId = se->GetFileId();
		    sprintf(fileIdStr, "#%d", fileId);
		    entityName = se->EntityName();

	            if(strcmp(attrTypeName, entityName))
		    {
			fprintf(stderr, "%c" , RingBell);
			ErrorMsg(
			"Possible type mismatch for selected instance.");
		    }
		    
		    attrRow->SetEditFieldText((const char *)fileIdStr);
		    return 1;
	  case AGGREGATE_TYPE:
		{
		    mn = instMgr -> GetMgrNode (index);
		    se = mn->GetSTEPentity();

		    fileId = se->GetFileId();
		    sprintf(fileIdStr, "#%d", fileId);

		    char attrVal[BUFSIZ];
		    attrVal[0] = 0;
		    strcpy(attrVal, attrRow->GetEditFieldText());
		    int i = 0;
		    for(i = 0; attrVal[i] != '\0'; i++) ;
		    if(i > 0 && !isspace(attrVal[i-1]))
			strcat(attrVal, ", ");
		    strcat(attrVal, fileIdStr);
		    attrRow->SetEditFieldText((const char *)attrVal);
		}
		    return 0;
	default:
		    ErrorMsg("Wrong attribute type.");
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		    return 0;
	} // end switch(type)
/*
	const char *attrName = attrRow->GetAttrName();
	cout << "attribute Name: " << attrName << "\n";
	cout << "attribute Type Name: " << attrTypeName << "\n";
	cout << "chosen entity Name: " << entityName << "\n";
	cout << "chosen entity file id: " << fileIdStr << "\n";
*/
    }
    return 0;
}
	//////////////////////////////////

void Probe::seeToggle(StepEntityEditor *see, int code)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::seeToggle()\n";
    MgrNode *mn = see->GetMgrNode();
    int index = mn->ArrayIndex();
    if(code == SEE_MODIFY)
    {
	mn->ChangeState(mappedWrite);
	entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR,
					 MODIFY_STATE_COL, index);
    }
    else if(code == SEE_VIEW)
    {
	mn->ChangeState(mappedView);
	entityInstanceList->WriteCmdChar(VIEW_STATE_CHAR,
					 VIEW_STATE_COL, index);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
//  Helper type functions
//
///////////////////////////////////////////////////////////////////////////////
void Probe::WriteInstance(int index)
{
    SCLstring instanceInfo;
    MgrNode *mn = instMgr -> GetMgrNode (index);
    
    mn->GetSTEPentity()->STEPwrite(instanceInfo);
    entityInstanceList->Append( instanceInfo.chars() );
    switch(mn->CurrState())
    {
/*
	case completeSE:
		entityInstanceList->WriteCmdChar(SAVE_COMPLETE_STATE_CHAR, 
					SAVE_COMPLETE_STATE_COL, index);
		break;
*/
	case incompleteSE:
		entityInstanceList->WriteCmdChar(SAVE_INCOMPLETE_STATE_CHAR, 
					     SAVE_INCOMPLETE_STATE_COL, index);
		break;
	case deleteSE:
//		entityInstanceList->WriteCmdChar(DELETE_CMD_CHAR, 
//						 DELETE_CMD_COL, index);
		entityInstanceList->WriteCmdChar(DELETE_STATE_CHAR, 
						 DELETE_STATE_COL, index);
		break;
	case newSE:
		entityInstanceList->WriteCmdChar(NEW_STATE_CHAR, 
						 NEW_STATE_COL, index);
		break;
	default:
		break;
    }
    switch (instMgr -> GetMgrNode (index) ->DisplayState())
    {
	case mappedWrite:
		entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
						 MODIFY_STATE_COL, index);
		break;
	case mappedView:
		entityInstanceList->WriteCmdChar(VIEW_STATE_CHAR, 
						 VIEW_STATE_COL, index);
		break;
	default:
		break;
    }
}

void Probe::ListEntityInstances()
{

    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ListEntityInstances()\n";

    if(entityInstanceList->Count() > 0)
	entityInstanceList->Clear();
    if(instMgr->InstanceCount() > 0)
    {
	if(debug_level >= PrintValues) cout << "Entity Instances:\n";

	int count = instMgr->InstanceCount();
	int i = 0;
	while(i < count){
	    WriteInstance(i);
	    i++;
	}
	char tmpStr[BUFSIZ];
	sprintf(tmpStr, "\n\nCreated %d ENTITY instances.\n", count);
	cout << tmpStr << "\n";
    } //else ErrorMsg("Must read in a step file first");
}

///////////////////////////////////////////////////////////////////////////////
//
//  functions from the schema_mgmt_menu.
//
///////////////////////////////////////////////////////////////////////////////

void ShellSort(const char **a, int count)
{
    int i, j, incr;
    const char *tmp;

    incr = (int)(count/2);
    while (incr > 0)
    {
	for(i = incr + 1; i <= count; i++)
	{
	    j = i - incr;
	    while(j > 0)
	    {
		if( strcmp(a[j], a[j+incr]) > 0 )
		{
		    tmp = a[j];
		    a[j] = a[j+incr];
		    a[j+incr] = tmp;
		    j = j - incr;
		}
		else
		{
		    j = 0;
		}
	    } // while
	} // for
	incr = (int)(incr/2);
    }
}

void Probe::ListEntityTypesCmd()
{
    char *entityName = 0;

    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ListEntityTypesCmd()\n";

	// This does not check the Registry to see if it's been
	// initialized for a schema.
    if(entityTypeList->Count() > 0)
	entityTypeList->Clear();

    int count, abstractCount = 0;
      // DAR - actually "abstractCount" (and related vars) also keep track of
      // ents which can't be instantiated without external mapping - 5/21/97
    const EntityDescriptor *enDesc;
    int arrSize = 0;
    registry -> ResetEntities ();

    count = registry -> GetFullEntCnt ();

//    const char *EntityNameArray[count + 1];
    const char **EntityNameArray = new const char *[count + 1];
    char **AbstractEntityArray = new char *[count + 1];

    while (count > 0)
    {
	enDesc = registry -> NextEntity ();
	// I am purposely assigning this array starting at index 1 instead of 0
	if(enDesc)
	{
	    if( (Logical)(
			   ((EntityDescriptor*)enDesc)->AbstractEntity() ) 
		== LTrue )
	    {
		arrSize = strlen(enDesc->Name()) + 2;
		AbstractEntityArray[abstractCount] = new char[arrSize];

		strcpy( AbstractEntityArray[abstractCount], enDesc->Name() );
		AbstractEntityArray[abstractCount][arrSize-2] = '*';
		AbstractEntityArray[abstractCount][arrSize-1] = 0;

		EntityNameArray[count] = AbstractEntityArray[abstractCount];
		abstractCount++;
	    }
	    else if( (Logical)(
			        ((EntityDescriptor*)enDesc)->ExtMapping() ) 
		== LTrue )
	    {
		arrSize = strlen(enDesc->Name()) + 2;
		AbstractEntityArray[abstractCount] = new char[arrSize];

		strcpy( AbstractEntityArray[abstractCount], enDesc->Name() );
		AbstractEntityArray[abstractCount][arrSize-2] = '%';
		AbstractEntityArray[abstractCount][arrSize-1] = 0;

		EntityNameArray[count] = AbstractEntityArray[abstractCount];
		abstractCount++;
	    }
	    else
		EntityNameArray[count] = enDesc->Name();
	}
	else EntityNameArray[count] = "There is a BUG in this program.";
	-- count;
    }
    count = registry -> GetFullEntCnt ();

    // sort names alphabetically
    ShellSort(EntityNameArray, count);

    int i = 1;
    while (i <= count)
    {
	if(debug_level >= PrintValues) 
	  cout << EntityNameArray[i] << "\n";
	if ( i == 1 || strcmp( EntityNameArray[i], EntityNameArray[i-1] ) ) {
	    // If this ent has the same name as the previous, skip it.  This
	    // may happen if an entity has "clones" of an entity, or if it's
	    // stored in the Registry's hash table under >1 name.  This is done
	    // if an entity has additional names used by other schemas which
	    // USE or REFERENCE the entity and rename it in the process.
	    entityTypeList->Append(EntityNameArray[i]);
	}
	i++;
    }

    // delete all the space created to hold the modified name for 
    // abstract entities
    for(i = 0; i < abstractCount; i++)
	delete AbstractEntityArray[i];

    delete [] EntityNameArray;
    delete [] AbstractEntityArray;

    char tmpStr[BUFSIZ];
    sprintf(tmpStr, "Listed %d entity types.", registry->GetEntityCnt());
    cout << tmpStr << "\n";
}

	//////////////////////////////////

void Probe::DescribeEntityCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::DescribeEntityCmd()\n";
    int index = entityTypeList->Selection();
    if(0 <= index) // if there is a selected instance.
    {
	char *entPtr = entityTypeList->String(index);
	// abstract entities are marked with an asterisk at end of entity name
	if(entPtr[strlen(entPtr) - 1] == '*')
	{
	    char ent[512];
	    strncpy(ent, entPtr, 512);
	    ent[strlen(entPtr)-1] = '\0'; // get rid of trailing * indicating 
					  // it is an abstract entity type
	    entPtr = ent;
	}
	// entities requiring external mapping have an '%' at the end
	else if(entPtr[strlen(entPtr) - 1] == '%')
	{
	    char ent[512];
	    strncpy(ent, entPtr, 512);
	    ent[strlen(entPtr)-1] = '\0';
	    entPtr = ent;
	}
	StepEntityDescriptor * sed = 
		new StepEntityDescriptor( registry->FindEntity(entPtr) );
/*
	StepEntityDescriptor * sed = 
	    new StepEntityDescriptor 
		( registry -> FindEntity(entityTypeList->String(index)) ); */
	InsertSED(sed);
    }
}

	//////////////////////////////////

void Probe::SelectSchemaCmd()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::SelectSchemaCmd()\n";
}

///////////////////////////////////////////////////////////////////////////////
//
//  functions from the instance_mgmt_menu.
//
///////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////

void Probe::CreateInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::CreateInstanceCmd()\n";

    StepEntityEditor *see;
    STEPentity *se;

    if(entityTypeList->Selections()){
	char * entityTypeName = 
		entityTypeList->String(entityTypeList->Selection());

//	if( (LOGICAL)( ((EntityDescriptor*)enDesc)->AbstractEntity() ) 
//	    == sdaiTRUE )
	if(entityTypeName[strlen(entityTypeName)-1] == '*')
	{
	    ErrorMsg("Selected type is an ABSTRACT entity type.");
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	}
	else if(entityTypeName[strlen(entityTypeName)-1] == '%')
	{
	    ErrorMsg("Selected type can only be instantiated using external mapping.");
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	}
	else if( (se = registry->ObjCreate(entityTypeName)) &&
	    (se != S_ENTITY_NULL) )
	{
	    SCLstring instanceInfo;

		// BUG remove the next line when it is generated by the core
	    AssignFileId(se);
		// this will probably be replaced with an InstanceMgr function.
		// also this probably doesn't make sense until the se can be
		// marked as incomplete since it doesn't have any values yet
	    MgrNode *mn = instMgr->Append(se, newSE);
	    if(!mn->displayNode())
		mn->displayNode() = new DisplayNode(mn);
	    DisplayNode *dn = mn->displayNode();
	    dn->SEE(GetStepEntityEditor(mn, 0));
	    see = (StepEntityEditor *)dn->SEE();

	    se->STEPwrite(instanceInfo);
	    int index = entityInstanceList->Append( instanceInfo.chars() );
	    mn->ArrayIndex(index);
	    entityInstanceList->WriteCmdChar(NEW_STATE_CHAR, 
					     NEW_STATE_COL, index);
	    entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
					     MODIFY_STATE_COL, index);

	    mn->ChangeState(mappedWrite);
	    InsertSEE(see);
//	    see->Validate();
	    see->Edit();
	} else {
	    ErrorMsg("Could not create selected type.");
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	}
    } else {
	ErrorMsg("Select a type from the Entity Type List window first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}

	//////////////////////////////////

void Probe::ModifyInstanceCmd()
{ 
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ModifyInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);

	sedlModify(mn, index);
	DisplayNode *dn = mn->displayNode();
// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
//	if(dn)
//	    dn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    } else {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::ViewInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::ViewInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);

	sedlView(mn, index);
	DisplayNode *dn = mn->displayNode();
// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
//	if(dn)
//	    dn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    } else {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::CloseInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::CloseInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);

	sedlClose(mn, index);
/*	ErrorMsg("Close Instance");*/
	DisplayNode *dn = mn->displayNode();
// dangerous because it messes up the execution of Probe::ExecuteInstanceCmds()
//	if(dn)
//	    dn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::SaveComplInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::SaveComplInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);
	sedlSaveComplete(mn, index);
/*	ErrorMsg("Save Instance Complete");*/
	mn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::SaveIncomplInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::SaveIncomplInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);
	sedlSaveIncomplete(mn, index);
/*	ErrorMsg("Save Instance Incomplete");*/
	mn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::ReplicateInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ReplicateInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);
	sedlReplicate(mn, index);
/*	ErrorMsg("Replicate Instance");*/
	cmdMgr->RepList()->Remove(mn);
//	mn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::UnmarkInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::UnmarkInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);
	sedlUnmark(mn, index);
/*	ErrorMsg("Unmark Instance Entry on Instance List Display");*/
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

void Probe::DeleteInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::DeleteInstanceCmd()\n";

    int index;
    if(entityInstanceList->Selections() &&
       (index = entityInstanceList->Selection()) >= 0)
    {
	MgrNode *mn = instMgr -> GetMgrNode (index);
	sedlDelete(mn, index);
/*	ErrorMsg("Delete Instance");*/
	mn->Remove(); // remove from list of intended actions for CmdMgr
	entityInstanceList->AdvanceSelection(index);
    }
    else
    {
	ErrorMsg("Select an instance from the Entity Instance List first.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
}
	//////////////////////////////////

/*
void Probe::PrintInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::PrintInstanceCmd()\n";
}
*/
	//////////////////////////////////

void Probe::SelectInstanceCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::SelectInstanceCmd()\n";
}

///////////////////////////////////////////////////////////////////////////////
//
//  functions from the file_mgmt_menu.
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// The next 3 functions are used by the functions that follow them.
///////////////////////////////////////////////////////////////////////////////

void 
Probe::ReadFile()
{
    fileChooser->SetTitle("Read File");
    fileChooser->SetSubtitle("Please select a file");
    InsertInteractor(fileChooser, Center);

    fileChooser->UpdateDir();
    fileChooser->SelectFile();
    if(fileChooser->Accept())
    {
	Cursor* origCursor = GetCursor();
	SetProbeCursor(hourglass);
	fileChooser->SetCursor(hourglass);

	const char *fileName = fileChooser->Choice();
	int fileType = fileChooser->FileType();

	if(fileType == CHOOSER_EXCHANGE) 
	{
	    if(headerDisplay)
	    {
		delete headerDisplay;
		headerDisplay = 0;
	    }
	    Severity sev;
	    if(readUsingTC)
	      sev = fileMgr -> ReadExchangeFile (fileName);
	    else
	      sev = fileMgr -> ReadExchangeFile (fileName,0);
	    if (sev < SEVERITY_NULL) 
	    {
		// read NOT successful 
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg(fileMgr->Error().UserMsg());
		cerr << fileMgr->Error().DetailMsg();
		fileMgr->Error().ClearErrorMsg();
	    }
	    ListEntityInstances();
	}
	else if(fileType == CHOOSER_WORKING) 
	{
	    if(headerDisplay)
	    {
		delete headerDisplay;
		headerDisplay = 0;
	    }
	    Severity sev;
	    if(readUsingTC)
	      sev = fileMgr -> ReadWorkingFile (fileName);
	    else
	      sev = fileMgr -> ReadWorkingFile (fileName,0);
	    if (sev < SEVERITY_NULL) 
	    {
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg(fileMgr->Error().UserMsg());
		cerr << fileMgr->Error().DetailMsg();
		fileMgr->Error().ClearErrorMsg();
	    }
	    ListEntityInstances();
	}
	else
	{
	    cerr << "Internal problem - send mail to dp1-bugs@cme.nist.gov\n";
	}
	SetProbeCursor(origCursor);
	fileChooser->SetCursor(origCursor);
    }
    RemoveInteractor(fileChooser);
}

void 
Probe::AppendFile()
{
    Severity sev ;
    fileChooser->SetTitle("Append File to Instance List");
    fileChooser->SetSubtitle("Please select a file");
    InsertInteractor(fileChooser, Center);

    fileChooser->UpdateDir();
    fileChooser->SelectFile();
    if(fileChooser->Accept())
    {
	Cursor* origCursor = GetCursor();
	SetProbeCursor(hourglass);
	fileChooser->SetCursor(hourglass);

	const char *fileName = fileChooser->Choice();
	int fileType = fileChooser->FileType();

	if(fileType == CHOOSER_EXCHANGE) 
	{
	    if(headerDisplay)
	    {
		delete headerDisplay;
		headerDisplay = 0;
	    }
	    sev = fileMgr -> AppendExchangeFile (fileName);
	    if (sev < SEVERITY_NULL) 
	    {
		// read NOT successful 
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg(fileMgr->Error().UserMsg());
		cerr << fileMgr->Error().DetailMsg();
		fileMgr->Error().ClearErrorMsg();
	    }
	    ListEntityInstances();
	}
	else if(fileType == CHOOSER_WORKING) 
	{
	    if(headerDisplay)
	    {
		delete headerDisplay;
		headerDisplay = 0;
	    }
	    Severity sev = fileMgr -> AppendWorkingFile (fileName);
	    if (sev < SEVERITY_NULL) 
	    {
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg(fileMgr->Error().UserMsg());
		cerr << fileMgr->Error().DetailMsg();
		fileMgr->Error().ClearErrorMsg();
	    }
	    ListEntityInstances();
	}
	else
	{
	    cerr << "Internal problem - send mail to dp1-bugs@cme.nist.gov\n";
	}
	SetProbeCursor(origCursor);
	fileChooser->SetCursor(origCursor);
    }
    RemoveInteractor(fileChooser);
}

void 
Probe::WriteFile()
{
    fileChooser->SetTitle("Write File");
    fileChooser->SetSubtitle("Please select a file");
    InsertInteractor(fileChooser, Center);

    fileChooser->UpdateDir();
    fileChooser->SelectFile();
    if(fileChooser->Accept()){
	Cursor* origCursor = GetCursor();
	SetProbeCursor(hourglass);
	fileChooser->SetCursor(hourglass);

	const char *fileName = fileChooser->Choice();
	int fileType = fileChooser->FileType();

	if(fileType == CHOOSER_EXCHANGE) 
	{
	    //VALIDATE instances on exchange file
	    ErrorDescriptor e;
	    Severity sev = instMgr->VerifyInstances(e);
	    if (sev < SEVERITY_USERMSG) 
	    {
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg(
			 "Contains invalid instances. File not written. Save as a working file.\n");
		cerr << e.UserMsg();
		cerr << e.DetailMsg();
	    }	
	    else
	    {
//		ostream* out  = new ostream(fileName, io_writeonly, a_create);
		ofstream* out  = new ofstream(fileName);
//		if(out->is_open() && out->writable())
		if((!(*out)) == 0)
		{
		    if (fileMgr->WriteExchangeFile(*out,0) < SEVERITY_NULL) 
		    {
			fprintf(stderr, "%c" , RingBell);
			fflush(stderr);
			ErrorMsg(fileMgr->Error().UserMsg());
			cerr << fileMgr->Error().DetailMsg();
			fileMgr->Error().ClearErrorMsg();
		    }
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		    ErrorMsg("Exchange File could not be opened for writing.");
		}
		delete out;
	    }
	}
	else if(fileType == CHOOSER_WORKING) 
	{
//	    ostream* out  = new ostream(fileName, io_writeonly, a_create);
	    ofstream* out  = new ofstream(fileName);
//	    if(out->is_open() && out->writable())
	    if((!(*out)) == 0)
	    {
		if (fileMgr->WriteWorkingFile(*out) < SEVERITY_NULL) 
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		    ErrorMsg(fileMgr->Error().UserMsg());
		    cerr << fileMgr->Error().DetailMsg();
		    fileMgr->Error().ClearErrorMsg();
		}
	    }
	    else
	    {
		fprintf(stderr, "%c" , RingBell);
		fflush(stderr);
		ErrorMsg("Working File could not be opened for writing.");
	    }
	    delete out;
	}
	else
	{
	    cerr << "Internal problem - send mail to dp1-bugs@cme.nist.gov\n";
	}
	SetProbeCursor(origCursor);
	fileChooser->SetCursor(origCursor);
    }
    RemoveInteractor(fileChooser);
}

void Probe::ReadExchangeFileCmd()
{ 
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ReadExchangeFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_EXCHANGE);
    readUsingTC = 1;
    ReadFile();
}
	//////////////////////////////////

void Probe::ReadExchangeFileCmdPreTC()
{ 
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ReadExchangeFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_EXCHANGE);
    readUsingTC = 0;
    ReadFile();
}
	//////////////////////////////////

void Probe::AppendExchangeFileCmd()
{ 
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::AppendExchangeFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_EXCHANGE);
    readUsingTC = 1;
    AppendFile();
}

	//////////////////////////////////

void Probe::WriteExchangeFileCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::WriteExchangeFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_EXCHANGE);
    WriteFile();
}

	//////////////////////////////////

void Probe::ReadWorkingFileCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ReadWorkingFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_WORKING);
    readUsingTC = 1;
    ReadFile();
}
	//////////////////////////////////

void Probe::ReadWorkingFileCmdPreTC()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::ReadWorkingFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_WORKING);
    readUsingTC = 0;
    ReadFile();
}
	//////////////////////////////////

void Probe::AppendWorkingFileCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::AppendWorkingFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_WORKING);
    readUsingTC = 1;
    AppendFile();
}
	//////////////////////////////////

void Probe::WriteWorkingFileCmd()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::WriteWorkingFileCmd()\n";

    fileChooser->SetFileType(CHOOSER_WORKING);
    WriteFile();
}

void Probe::VerifyInstsCmd()
{
    ErrorDescriptor e;
    
    Severity sev = instMgr->VerifyInstances(e);

    if (sev < SEVERITY_NULL) 
      {
	  ErrorMsg(e.UserMsg());
	  cerr << e.DetailMsg();
      }
    else 
      {
	  ErrorMsg("All instances are complete.");
      }
}

void Probe::RemoveDelInstsCmd()
{
    char str[BUFSIZ];
    str[0] = '\0';
    
    sprintf(str, "%s.del", autoSaveFile);
    
    Cursor* origCursor = GetCursor();
    SetProbeCursor(hourglass);

    Severity sev = fileMgr->WriteWorkingFile(str);
    if (sev < SEVERITY_NULL) 
      {
	  fprintf(stderr, "%c" , RingBell);
	  fflush(stderr);
	  ErrorMsg(fileMgr->Error().UserMsg());
	  cerr << fileMgr->Error().DetailMsg();
	  fileMgr->Error().ClearErrorMsg();
      }

    // might want to check (based on sev) whether user wants to continue
    // if (sev < SEVERITY_USER_MSG) return ;
    
    fileMgr->ReadWorkingFile(str);
    ListEntityInstances();

    SetProbeCursor(origCursor);
}

void Probe::ClearInstanceListCmd()
{
    if(headerDisplay)
    {
	delete headerDisplay;
	headerDisplay = 0;
    }
    entityInstanceList->Clear();
    instMgr->ClearInstances();
}

void 
Probe::InsertHeaderEditor ()
{
    if(!headerDisplay)
    {
	InstMgr *headerIM = fileMgr->HeaderInstances();
	headerDisplay = new HeaderDisplay(headerIM, this, 0);
    }

    if(headerDisplay->IsMapped())
    {
	ErrorMsg("Header Editor is already on the screen.");
	fprintf(stderr, "%c" , RingBell);
	fflush(stderr);
    }
    else
    {
	Frame* framedHE = new Frame(headerDisplay, 2);
//	World* worldPtr = GetWorld();
    World *worldPtr = ::world;

//	Coord x, y;

//	char winName[128];
//	winName[0] = '\0';
//	sprintf(winName, "dp - %s", "Header Editor");
//IVBUG	framedHE->SetName(winName);
//IVBUG	framedHE->SetIconName("Header Editor");

	worldPtr->InsertToplevel(framedHE, this, 
			     (int)(worldPtr->Width()/2), (int)(worldPtr->Height()/2),
			      Center);
//	worldPtr->InsertToplevel(framedHE, this);
    }
}

void 
Probe::RemoveHeaderEditor ()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::RemoveHeaderEditor()\n";

//    World* worldPtr = GetWorld();
    World *worldPtr = ::world;

    if(headerDisplay)
    {
	Frame* framedHE = (Frame*) headerDisplay->Parent();

	if(headerDisplay->IsMapped())
	{
	    worldPtr->Remove(framedHE);
	    framedHE->Remove(headerDisplay);
	    delete framedHE;
	}
    }
//    delete headerDisplay;
}

void Probe::EditHeaderInstancesCmd()
{
    InsertHeaderEditor();
}

///////////////////////////////////////////////////////////////////////////////
//
// Command options called from the Main STEPentity display list
//
///////////////////////////////////////////////////////////////////////////////

void Probe::UpdateCmdList(int index, char cmd, int cmdCol)
{
  MgrNode *mn = instMgr -> GetMgrNode (index);

    switch(cmd)
    {
	case SAVE_COMPLETE_CMD_CHAR:
//		if(mn->CurrState() == completeSE)
		cmdMgr->SaveCompleteCmdList(mn);
		entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		break;
	case SAVE_INCOMPLETE_CMD_CHAR:
		cmdMgr->SaveIncompleteCmdList(mn);
		entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		break;
	case DELETE_CMD_CHAR:
		cmdMgr->DeleteCmdList(mn);
		entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		break;
	case MODIFY_CMD_CHAR:
		if(mn->DisplayState() != mappedWrite)
		{
		    cmdMgr->ModifyCmdList(mn);
		    entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		}
		break;
	case VIEW_CMD_CHAR:
		if(mn->DisplayState() != mappedView)
		{
		    cmdMgr->ViewCmdList(mn);
		    entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		}
		break;
	case CLOSE_CMD_CHAR:
		if((mn->DisplayState() == mappedWrite) ||
		   (mn->DisplayState() == mappedView))
		{
		    cmdMgr->CloseCmdList(mn);
		    entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		}
		break;
	case REPLICATE_CMD_CHAR:
		cmdMgr->ReplicateCmdList(mn);
		entityInstanceList->WriteCmdChar(cmd, cmdCol, index);
		break;
	case EXECUTE_CMD_CHAR:
		ExecuteInstanceCmds();
		break;
	case UNMARK_CMD_CHAR:
		sedlUnmark(mn, index);
		break;
//	case CANCEL_CMD_CHAR:
//		cmdMgr->CancelCmdList(mn);
//		break;
//	case NEW_CMD_CHAR:
    }
    
}

	//////////////////////////////////

void Probe::ExecuteInstanceCmds()
{
    ExecuteDelete();
    ExecuteSaveComplete();
    ExecuteSaveIncomplete();
    ExecuteModify();
    ExecuteView();
    ExecuteClose();
    ExecuteReplicate();
}
	//////////////////////////////////

void Probe::ExecuteSaveComplete()
{
    MgrNode *listHead = cmdMgr->GetHead(completeSE);
    MgrNode *mn = (MgrNode *)listHead->Next();
    int index;
    MgrNode *mnNext;
    while(mn && mn != listHead)
    {
	index = mn->ArrayIndex();
	mnNext = (MgrNode *)mn->Next();
	sedlSaveComplete(mn, index);
	mn = mnNext;
    }
    cmdMgr->ClearEntries(completeSE);
}
	//////////////////////////////////

void Probe::ExecuteSaveIncomplete()
{
    MgrNode *listHead = cmdMgr->GetHead(incompleteSE);
    MgrNode *mn = (MgrNode *)listHead->Next();
    int index;
    MgrNode *mnNext;
    while(mn && mn != listHead)
    {
	index = mn->ArrayIndex();
	mnNext = (MgrNode *)mn->Next();
	sedlSaveIncomplete(mn, index);
	mn = mnNext;
    }
    cmdMgr->ClearEntries(incompleteSE);
}
//////////////////////////////////

void Probe::ExecuteDelete()
{
    MgrNode *listHead = cmdMgr->GetHead(deleteSE);
    MgrNode *mn = (MgrNode *)listHead->Next();
    int index;
    MgrNode *mnNext;
    while(mn && mn != listHead)
    {
	index = mn->ArrayIndex();
	mnNext = (MgrNode *)mn->Next();
	mn->ChangeState(deleteSE);
	entityInstanceList->WriteCmdChar(DELETE_STATE_CHAR, 
					 DELETE_STATE_COL, index);
	entityInstanceList->WriteCmdChar(' ', DELETE_CMD_COL, 
					 index);
	mn = mnNext;
    }
    cmdMgr->ClearEntries(deleteSE);
}
	//////////////////////////////////

void Probe::ExecuteReplicate()
{
    ReplicateLinkNode *rn = cmdMgr->GetReplicateHead();
    MgrNode *mn;
    int index;
    while(rn)
    {
	mn = rn->ReplicateNode();
	index = mn->ArrayIndex();
		// this assignment must be before the sedlReplicate() call
	rn = (ReplicateLinkNode *)rn->NextNode();
	sedlReplicate(mn, index);
    }
//    cmdMgr->ClearReplicateEntries();
}
	//////////////////////////////////

void Probe::ExecuteModify()
{
    DisplayNode *listHead = cmdMgr->GetHead(mappedWrite);
    DisplayNode *dn = (DisplayNode *)listHead->Next();
    MgrNode *mn = dn->mgrNode();
    int index;
    while(dn && dn != listHead)
    {
	index = mn->ArrayIndex();
	sedlModify(mn, index);
	dn = (DisplayNode *)dn->Next();
	mn = dn->mgrNode();
    }
    cmdMgr->ClearEntries(mappedWrite);
}
	//////////////////////////////////

void Probe::ExecuteView()
{
    DisplayNode *listHead = cmdMgr->GetHead(mappedView);
    DisplayNode *dn = (DisplayNode *)listHead->Next();
    MgrNode *mn = dn->mgrNode();
    int index;
    while(dn && dn != listHead)
    {
	index = mn->ArrayIndex();
	sedlView(mn, index);
	dn = (DisplayNode *)dn->Next();
	mn = dn->mgrNode();
    }
    cmdMgr->ClearEntries(mappedView);
}
	//////////////////////////////////

void Probe::ExecuteClose()
{
    DisplayNode *listHead = cmdMgr->GetHead(notMapped);
    DisplayNode *dn = (DisplayNode *)listHead->Next();
    MgrNode *mn = dn->mgrNode();
    int index;
    while(dn && dn != listHead)
    {
	index = mn->ArrayIndex();
	sedlClose(mn, index);
	dn = (DisplayNode *)dn->Next();
	mn = dn->mgrNode();
    }
    cmdMgr->ClearEntries(notMapped);
}
	//////////////////////////////////

boolean Probe::sedlSaveComplete(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::sedlSaveComplete()\n";

    if(index >= 0){
//	MgrNode *mn = (*instMgr)[index];
	if(mn->IsDisplayState(mappedWrite) || mn->IsDisplayState(mappedView) ||
	   mn->IsDisplayState(notMapped))
//	if(mn->IsDisplayState(mappedWrite) || mn->IsDisplayState(mappedView) )
	{
	    DisplayNode *dn = mn->displayNode();
	    ((StepEntityEditor *)dn->SEE())->PressButton(SEE_SAVE_COMPLETE);
	    if(mn->CurrState() == completeSE)
	    {
		// BUG not really, decide if seeSave will remove this node from
		// the entity instance list save command list. - it does DAS

		// remove from save to do list
//		mn->Remove();
		// remove indicator of being on list
//		entityInstanceList->WriteCmdChar(' ', SAVE_COMPLETE_CMD_COL, 
//						 index);
		return true;
	    }
	    else
		return false;
	}
	else
	{
	    STEPentity *entity = mn->GetSTEPentity();
	    Severity s = entity->ValidLevel(&(entity->Error()), instMgr, 1);
	    if(s >= SEVERITY_USERMSG)
	    {
		mn->ChangeState(completeSE);
		entityInstanceList->WriteCmdChar(SAVE_COMPLETE_STATE_CHAR, 
					       SAVE_COMPLETE_STATE_COL, index);
		// remove from save to do list
		mn->Remove();
		// remove indicator of being on list
		entityInstanceList->WriteCmdChar(' ', SAVE_COMPLETE_CMD_COL, 
						 index);
		return true;
	    }
	    else
	    {
		mn->ChangeState(incompleteSE);
		entityInstanceList->WriteCmdChar(SAVE_INCOMPLETE_STATE_CHAR, 
					     SAVE_INCOMPLETE_STATE_COL, index);
		// (if it's on) to do list for saving complete, leave on 
//		entityInstanceList->WriteCmdChar(' ', SAVE_INCOMPLETE_CMD_COL, 
//						 index);
		return false; // else else => end result couldn't save complete
	    }
	}
    } else return false;
}
	//////////////////////////////////

boolean Probe::sedlSaveIncomplete(MgrNode *mn, int index) 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "Probe::sedlSaveIncomplete()\n";
    if(index >= 0){
	if(mn->IsDisplayState(mappedWrite) || mn->IsDisplayState(mappedView) ||
	   mn->IsDisplayState(notMapped))
//	if(mn->IsDisplayState(mappedWrite) || mn->IsDisplayState(mappedView))
	{
	    DisplayNode *dn = mn->displayNode();
	    ((StepEntityEditor *)dn->SEE())->PressButton(SEE_SAVE_INCOMPLETE);
	    if(mn->CurrState() == incompleteSE)
	    {
		// BUG not really, decide if seeSave will remove this node from
		// the entity instance list save command list. - it does DAS

		// remove from save to do list
//		mn->Remove();
		// remove indicator of being on list
//		entityInstanceList->WriteCmdChar(' ', SAVE_INCOMPLETE_CMD_COL, 
//						 index);
//		return true;
	    }
//	    else
//		return false;
//		seeSaveIncomplete() doesn't copy the values from the SEE
//	    seeSaveIncomplete(mn->SEE());
	}
	else
	{
	    mn->ChangeState(incompleteSE);
	    entityInstanceList->WriteCmdChar(SAVE_INCOMPLETE_STATE_CHAR, 
					     SAVE_INCOMPLETE_STATE_COL, index);
	    // remove from save to do list
	    mn->Remove();
	    // remove indicator of being on list
	    entityInstanceList->WriteCmdChar(' ', SAVE_INCOMPLETE_CMD_COL, 
					     index);
	}
	return 1;
    } else return false;
}
	//////////////////////////////////

void Probe::sedlUnmark(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlUnmark()\n";

	// remove any change edit state cmd and write over cmd char.
    mn->Remove(); // remove from list of intended actions for CmdMgr
    entityInstanceList->WriteCmdChar(' ', SAVE_COMPLETE_CMD_COL, index);

	// remove any change display state cmd and write over cmd char.
    DisplayNode *dn = mn->displayNode();
    if(dn)
	dn->Remove(); // remove from list of intended actions for CmdMgr
    entityInstanceList->WriteCmdChar(' ', MODIFY_CMD_COL, index);

       // remove from the list of entities to replicate and write over cmd char
    entityInstanceList->WriteCmdChar(' ', REPLICATE_CMD_COL,index);
}
	//////////////////////////////////

void Probe::sedlDelete(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlDelete()\n";
    if(index >= 0){
	if(mn->IsDisplayState(mappedWrite) || mn->IsDisplayState(mappedView) ||
	   mn->IsDisplayState(notMapped))
	{
	    DisplayNode *dn = mn->displayNode();
	    ((StepEntityEditor *)dn->SEE())->PressButton(SEE_DELETE);
		// BUG not really, decide if seeSave will remove this node from
		// the entity instance list save command list. - it does DAS

		// remove from save to do list
//	    mn->Remove();
		// remove indicator of being on list
//	    entityInstanceList->WriteCmdChar(' ', DELETE_CMD_COL, 
//					     index);
	}
	else
	{
	    mn->ChangeState(deleteSE);
		// remove from save to do list
	    mn->Remove();
	    entityInstanceList->WriteCmdChar(DELETE_STATE_CHAR, 
					     DELETE_STATE_COL, index);
		// remove indicator of being on list
	    entityInstanceList->WriteCmdChar(' ', DELETE_CMD_COL, 
					     index);
	}
//	return 1;
    }// else return false;
}
	//////////////////////////////////

void Probe::sedlReplicate(MgrNode *existMN, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlReplicate()\n";

//    entityInstanceList->WriteCmdChar(' ', REPLICATE_CMD_COL, index);

    STEPentity *seExist = existMN->GetSTEPentity();
    STEPentity *seNew;
//    EntityDescriptor *ed = seExist->EntityDescriptor;

//    if(seNew = ed->NewSTEPentity()){
    if(seNew = seExist->Replicate()){

	cmdMgr->RepList()->Remove(existMN);
	entityInstanceList->WriteCmdChar(' ', REPLICATE_CMD_COL, index);

	SCLstring instanceInfo;

		// BUG remove the next line when it is generated by the core
	AssignFileId(seNew);
		// this will probably be replaced with an InstanceMgr function.
		// also this probably doesn't make sense until the se can be
		// marked as incomplete since it doesn't have any values yet
	MgrNode *mn = instMgr->Append(seNew, newSE);
	if(!mn->displayNode())
	    mn->displayNode() = new DisplayNode(mn);
	DisplayNode *dn = mn->displayNode();
	dn->SEE(GetStepEntityEditor(mn, 0));
	StepEntityEditor *see = (StepEntityEditor *)dn->SEE();

	seNew->STEPwrite(instanceInfo);
	int newIndex = entityInstanceList->Append( instanceInfo.chars() );
	mn->ArrayIndex(newIndex);
	entityInstanceList->WriteCmdChar(NEW_STATE_CHAR, 
					 NEW_STATE_COL, newIndex);
	entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
					 MODIFY_STATE_COL, newIndex);

	mn->ChangeState(mappedWrite);
	InsertSEE(see);
//	see->Validate();
//	see->Edit();
    } else {
	ErrorMsg("Could not replicate selected instance.");
    }
}
	//////////////////////////////////

void Probe::sedlModify(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlModify()\n";

    StepEntityEditor *see;
    if(index >= 0){
//	MgrNode *mn = (*instMgr)[index];
	DisplayNode *dn = mn->displayNode();

	switch(mn->DisplayState())
	{
	    case mappedView:
		    ((StepEntityEditor *)dn->SEE())->PressButton(SEE_MODIFY);
		    mn->ChangeState(mappedWrite);
		    entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
						 MODIFY_STATE_COL, index);
		    entityInstanceList->WriteCmdChar(' ', MODIFY_CMD_COL, 
							index);
		    break;
	    case noMapState:
		    if(!dn)
			mn->displayNode() = new DisplayNode(mn);
		    dn = mn->displayNode();
		    dn->SEE(GetStepEntityEditor(mn, 0));
//		    ((StepEntityEditor *)dn->SEE())->Validate();
	    case notMapped:
		    see = (StepEntityEditor *)dn->SEE();
		    InsertSEE(see);
		    see->PressButton(SEE_MODIFY);
		    mn->ChangeState(mappedWrite);
		    entityInstanceList->WriteCmdChar(MODIFY_STATE_CHAR, 
						 MODIFY_STATE_COL, index);
		    entityInstanceList->WriteCmdChar(' ', MODIFY_CMD_COL, 
							index);
		    break;
	    case mappedWrite:
		    if(GetTopLevelWindow())
			GetTopLevelWindow()->raise();
		    break;
	    default:
		    break;
	}
    } else {
	ErrorMsg("Select an instance from the Entity Instance List first.");
    }
}
	//////////////////////////////////

void Probe::sedlView(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlView()\n";

    StepEntityEditor *see;
    if(index >= 0){
//	MgrNode *mn = (*instMgr)[index];
	DisplayNode *dn = mn->displayNode();

	switch(mn->DisplayState())
	{
	    case mappedWrite:
		    ((StepEntityEditor *)dn->SEE())->PressButton(SEE_VIEW);
		    mn->ChangeState(mappedView);
		    entityInstanceList->WriteCmdChar(VIEW_STATE_CHAR, 
						 VIEW_STATE_COL, index);
		    entityInstanceList->WriteCmdChar(' ', VIEW_CMD_COL, 
							index);
		    break;
	    case noMapState:
		    if(!dn)
			mn->displayNode() = new DisplayNode(mn);
		    dn = mn->displayNode();
		    dn->SEE(GetStepEntityEditor(mn, 0));
//		    ((StepEntityEditor *)dn->SEE())->Validate();
//		    dn->SEE(GetStepEntityEditor(mn, 0, 0));
	    case notMapped:
		    see = (StepEntityEditor *)dn->SEE();
		    InsertSEE(see);
		    see->PressButton(SEE_VIEW);
		    mn->ChangeState(mappedView);
		    entityInstanceList->WriteCmdChar(VIEW_STATE_CHAR, 
						 VIEW_STATE_COL, index);
		    entityInstanceList->WriteCmdChar(' ', VIEW_CMD_COL, 
							index);
		    break;
	    case mappedView:
		    canvas->window()->raise();
//		    if(GetTopLevelWindow())
//			GetTopLevelWindow()->raise();
		    break;
	    default:
		    break;
	}
    } else {
	ErrorMsg("Select an instance from the Entity Instance List first.");
    }
}
	//////////////////////////////////

void Probe::sedlClose(MgrNode *mn, int index)
{
    if(debug_level >= PrintFunctionTrace) cout << "Probe::sedlClose()\n";

    StepEntityEditor *see;
    if(index >= 0){
//	MgrNode *mn = (*instMgr)[index];
	DisplayNode *dn = mn->displayNode();

	switch(mn->DisplayState())
	{
	    case mappedWrite:
	    case mappedView:
		    see = (StepEntityEditor *)dn->SEE();
		    mn->ChangeState(notMapped);
		    RemoveSEE(see);
		    // write over the cmd and the state indicator in type list
		    entityInstanceList->WriteCmdChar(' ', VIEW_STATE_COL, 
						     index);
		    entityInstanceList->WriteCmdChar(' ', VIEW_CMD_COL, index);
		    break;
	    case notMapped:
	    default:
		    // write over the cmd and state indicator in instance list
		    entityInstanceList->WriteCmdChar(' ', VIEW_STATE_COL, 
						     index);
		    entityInstanceList->WriteCmdChar(' ', VIEW_CMD_COL, index);
		    break;
	}
    } else {
	ErrorMsg("Select an instance from the Entity Instance List first.");
    }
}
	//////////////////////////////////
