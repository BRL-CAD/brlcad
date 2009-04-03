/*
 * Copyright (c) 1987, 1988, 1989 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * MyFileChooser implementation.
 */

#include <stream.h>

#include <InterViews/event.h>

#include <IV-2_6/InterViews/adjuster.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/scroller.h>
#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/streditor.h>

#include <streditor2.h>
#include <myfilebrowser.h>
#include <myfilechooser.h>

#include <IV-2_6/_enter.h>

#include <string.h>
    
void MyFileChooser::Restrict(int boolvar) 
{
    restrict = boolvar;
    ((MyFileBrowser*)_browser)->Restrict(boolvar);
    _sedit->Message("");
    ((MyFileBrowser*)_browser)->Update();
}	

MyFileChooser::MyFileChooser (
    const char* Title, const char* Subtitle, const char* dir,
    int rows, int cols, const char* acceptLabel, Alignment a, int restrictBool)
 : StringChooser(new ButtonState, a) 
{

    MyFileBrowser* fb = new MyFileBrowser(state, dir, rows, cols, true, 
					  Reversed, SBDone, restrictBool);

	//DAS Initialize the StringChooser with the customized file brower
    if(restrictBool)
	StringChooser::Init(new StringEditor2(state, "", SEDone, 40), fb);
    else
	StringChooser::Init(new StringEditor2(state, fb->GetDirectory()), fb);
    Init(Title, Subtitle);

    Insert(Interior(acceptLabel));
}

MyFileChooser::MyFileChooser (
    const char* name, const char* Title, const char* Subtitle, const char* dir,
    int rows, int cols, const char* acceptLabel, Alignment a, int restrictBool)
 : StringChooser(new ButtonState, a) 
{

    SetInstance(name);

    MyFileBrowser* fb = new MyFileBrowser(state, dir, rows, cols, true, 
					  Reversed, SBDone, restrictBool);
	//DAS Initialize the StringChooser with the customized file brower
    if(restrictBool)
	StringChooser::Init(new StringEditor2(state, "", SEDone, 40), fb);
    else
	StringChooser::Init(new StringEditor2(state, fb->GetDirectory()), fb);
    Init(Title, Subtitle);
    Insert(Interior(acceptLabel));
}

MyFileChooser::MyFileChooser (
    ButtonState* bs, const char* dir, int rows, int cols, Alignment a) : 
							StringChooser(bs, a) 
{
    MyFileBrowser* fb = new MyFileBrowser(state, dir, rows, cols);
    StringChooser::Init(new StringEditor2(state, fb->GetDirectory()), fb);
}    

MyFileChooser::MyFileChooser (ButtonState* bs, Alignment a) : 
							StringChooser(bs, a) 
{ }

void MyFileChooser::Init (const char* t, const char* subt)
{
    if (*t == '\0')
    {
        title = new MarginFrame(new VGlue(0, 0));
    }
    else
    {
//	Interactor *i = new class Message(t, 4);
//        title = new MarginFrame(i);
        title = new MarginFrame(new class Message(t));
    }
    if (*subt == '\0') {
        subtitle = new MarginFrame(new VGlue(0, 0));
    } else {
        subtitle = new MarginFrame(new class Message(subt));
    }
    restrict = 0;
    // set parents _browser also to not be restricted
    ((MyFileBrowser*)_browser)->Restrict(0);
}

void MyFileChooser::SelectFile () {
    const char* path = _sedit->Text();
    int left = strlen(browser()->ValidDirectories(path));
    int right = strlen(path);

    Select(left, right);
}

void 
MyFileChooser::UpdateDir()
{
    ((MyFileBrowser*)_browser)->UpdateDir();
}

void MyFileChooser::UpdateEditor () {
    int index = browser()->Selection();

    if(!restrict)
    {
	if (index >= 0) {
	    _sedit->Message(browser()->Path(index));
	    browser()->UnselectAll();
	} else {
	    _sedit->Message(browser()->Normalize(_sedit->Text()));
	}
    } else {
	if (index >= 0) {
	    _sedit->Message(browser()->String(index));
	    browser()->UnselectAll();
	} else {
	    const char *selection = _sedit->Text();
	    int illegal = 0;
	    char *foundDot = strchr(selection, '.');
	    if((foundDot == selection ||
		(foundDot > selection && *(foundDot-1) == '/') ||
		(foundDot > selection && *(foundDot+1) == '.') ||
		(selection[0] == '/') ||
		(selection[0] == '~')
	       ) && 
	       restrict
	      )
		  illegal = 1;
	    if(!illegal)
		browser()->SetDirectory(selection);
	    else
	    {
		cerr << "\007***ERROR*** Choice is restricted... " 
		     << "Will not accept it.\n";
		_sedit->Message("");
	    }
	}
    }
    SelectFile();
}

void MyFileChooser::UpdateBrowser () {
    const char *selection = Choice();

    int illegal = 0;
    char *foundDot = strchr(selection, '.');
    if((foundDot == selection ||
        (foundDot > selection && *(foundDot-1) == '/') ||
        (foundDot > selection && *(foundDot+1) == '.') ||
	(selection[0] == '/') ||
	(selection[0] == '~')
       ) && 
       restrict
      )
	illegal = 1;
    if(!illegal)
	browser()->SetDirectory(selection);
    else
    {
	_sedit->Message("");
       cerr << "\007***ERROR*** Choice is restricted... Will not accept it.\n";
    }

//    browser()->SetDirectory(Choice());
}

boolean MyFileChooser::Accept () {
    boolean accepted, dirSelected;

    do {
        accepted = StringChooser::Accept();
        dirSelected = browser()->IsADirectory(Choice());
    } while (accepted && dirSelected);

    return accepted;
}

static void ChangeMsg (const char* name, MarginFrame* frame) {
    Interactor* msg;

    if (*name == '\0') {
        msg = new VGlue(0, 0);
    } else {
        msg = new Message(name);
    }
    frame->Insert(msg);
    frame->Change(msg);
}

void MyFileChooser::SetTitle (const char* name) {
    ChangeMsg(name, title);
}

void MyFileChooser::SetSubtitle (const char* name) {
    ChangeMsg(name, subtitle);
}

Interactor* MyFileChooser::AddScroller (Interactor* i) {
    return new HBox(
        new MarginFrame(i, 2),
        new VBorder,
        new VBox(
            new UpMover(i, 1),
            new HBorder,
            new VScroller(i),
            new HBorder,
            new DownMover(i, 1)
        )                
    );
}

Interactor* MyFileChooser::Interior (const char* acptLbl) {
    const int space = round(.5*cm);
    VBox* titleblock = new VBox(
        new HBox(title, new HGlue),
        new HBox(subtitle, new HGlue)
    );

    addButsArea = new MyMonoScene;
//    addButsArea = new Scene;
    VGlue *vg = new VGlue(space, 0);
    addButsArea->Insert(vg);

    return new MarginFrame(
        new VBox(
            titleblock,
            new VGlue(space, 0),
            new Frame(new MarginFrame(_sedit, 2)),
            new VGlue(space, 0),
            new Frame(AddScroller(browser())),
            new VGlue(space, 0),
            new HBox(
		addButsArea,
                new HGlue,
                new PushButton("Cancel", state, '\007'),
                new HGlue(space, 0),
                new PushButton(acptLbl, state, '\r')
            )
        ), space, space/2, 0
    );
}

#include <IV-2_6/_leave.h>
