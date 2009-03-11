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
 * Common menus built on top of controls
 */

#ifndef mymenu_h
#define mymenu_h

#include <IV-2_6/InterViews/box.h>
#include <mycontrol.h>

#include <IV-2_6/_enter.h>

class MyMenuItem : public MyControl {
public:
    MyMenuItem(Interactor*);
    MyMenuItem(const char* name, Interactor*);
    MyMenuItem(const char* str, Alignment = Left);
    MyMenuItem(const char* name, const char* str, Alignment = Left);
    virtual ~MyMenuItem();

protected:
    virtual void Busy();
    virtual void Done();
    
private:
    void Init();
};

class MyMenu : public MyControl {
public:
    MyMenu(Interactor*);
    MyMenu(const char* name, Interactor* i);
    virtual ~MyMenu();

    void SetBody(Interactor*);
    Interactor* GetBody() { return body_; }

    void SetAlign(Alignment);
    Alignment GetAlign() { return align_; }

    void SetDepth(int);
    int GetDepth() { return depth_; }

    void SetBodyState(MyControlState*);
    MyControlState* GetBodyState() { return state_; }

    void SetScene(Scene*);
    Scene* GetScene() { return scene_; }
    virtual void Include(MyControl*);

    virtual void Popup(Event&);

    IntCoord InsertX() { return ins_x_; }
    IntCoord InsertY() { return ins_y_; }
protected:
    virtual void Leave();
    virtual void Open();
    virtual void Close();

    virtual void Reconfig();
    virtual void Setup();
    virtual void InsertBody(IntCoord, IntCoord);
private:
    Interactor* body_;
    Scene* scene_;
    MyControlState* state_;
    Interactor* shadow_;
    unsigned int depth_ : 16;
    unsigned int align_ : 16;
    class World* world_;
    IntCoord ins_x_, ins_y_;
    IntCoord rel_x_, rel_y_;
private:
    void Init();
};

class MyMenuBar : public HBox {
public:
    MyMenuBar();
    MyMenuBar(const char* name);
    virtual ~MyMenuBar();

    virtual void Include(MyControl*);
protected:
    MyControlState* state_;
private:
    void Init();
};

class MyPulldownMenu : public MyMenu {
public:
    MyPulldownMenu(Interactor* i);
    MyPulldownMenu(const char* name, Interactor* i);
    MyPulldownMenu(const char* str);
    MyPulldownMenu(const char* name, const char* str);
    virtual ~MyPulldownMenu();

private:
    void Init();
};

class MyPullrightMenu : public MyMenu {
public:
    MyPullrightMenu(Interactor* i);
    MyPullrightMenu(const char* name, Interactor* i);
    MyPullrightMenu(const char* str);
    MyPullrightMenu(const char* name, const char* str);
    virtual ~MyPullrightMenu();

private:
    void Init();
};

class MyPopupMenu : public MyMenu {
public:
    MyPopupMenu();
    MyPopupMenu(const char*);
    virtual ~MyPopupMenu();
private:
    void Init();
};

#include <IV-2_6/_leave.h>

#endif
