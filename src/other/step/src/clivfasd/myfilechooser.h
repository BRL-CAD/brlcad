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
 * MyFileChooser - a StringChooser for choosing files from a directory.
 */

#ifndef myfilechooser_h
#define myfilechooser_h

#include <IV-2_6/InterViews/strchooser.h>
#include <IV-2_6/InterViews/scene.h>

#include <IV-2_6/_enter.h>

class Button;
class MyFileBrowser;
class MarginFrame;

// I created this class so that I can create a Monoscene since the constructor
// is protected for MonoScene.

class MyMonoScene : public MonoScene
{
  public:
    MyMonoScene() { }
};


class MyFileChooser : public StringChooser {
public:
    MyFileChooser(
        const char* Title = "Please select a file:",
        const char* Subtitle = "",
        const char* dir = "~",
        int rows = 10, int cols = 24,
        const char* acceptLabel = " Open ",
        Alignment a = Center,
	int restrictBool = 0
    );
    MyFileChooser(
		  const char* name, const char* Title, const char* Subtitle,
		  const char* dir, int rows, int cols, const char* acceptLabel,
		  Alignment a, int restrictBool = 0
    );

    void SetTitle(const char*);
    void SetSubtitle(const char*);
    void SelectFile();

    virtual boolean Accept();
    void Restrict(int boolvar);
    int Restrict() {
	return restrict;
    }
    void UpdateDir();
protected:
    MyFileChooser(ButtonState*, Alignment = Center);
    MyFileChooser(ButtonState*, const char* dir, int rows, int cols,Alignment);
    void Init(const char*, const char*);
    virtual Interactor* Interior(const char* acptlbl);
    Interactor* AddScroller(Interactor*);
    MyFileBrowser* browser();

    virtual void UpdateEditor();
    virtual void UpdateBrowser();
protected:
    MarginFrame* title, *subtitle;
    int restrict;
//    Scene *addButsArea;
    MyMonoScene *addButsArea;
};

inline MyFileBrowser* MyFileChooser::browser () { return (MyFileBrowser*) _browser; }

#include <IV-2_6/_leave.h>

#endif
