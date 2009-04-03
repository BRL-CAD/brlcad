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
 * StringDisplay - scrollable display for character strings
 * 		   This object started out as InterView's StringEditor
 */

#ifndef strdisplay_h
#define strdisplay_h

#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/streditor.h>

#include <IV-2_6/_enter.h>

class ButtonState;
class TextDisplay;
class TextBuffer;

class StringDisplay : public Interactor {
public:
    StringDisplay(ButtonState*, const char* sample, const char* done = SEDone);
    StringDisplay(
        const char* name, ButtonState*, const char*, const char* = SEDone
    );
    virtual ~StringDisplay();

    void Message(const char* text);
    void Select(int);
    void Select(int left, int right);
    void Edit();
    void Edit(const char* text, int left, int right);

    const char* Text();

    virtual void Handle(Event&);
    virtual void Reconfig();
protected:
    virtual boolean HandleChar(char);
    void InsertText(const char*, int);
    void DoSelect(int left, int right);

    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
    virtual void Resize();

    TextBuffer* text;
    int left, right;
    ButtonState* subject;
    char* done;
    char* sample;
    char* buffer;
    int size;
    TextDisplay* display;
private:
    void Init(ButtonState*, const char*, const char*);
/*
    void *HighlightText(int boolVal);
    int highlighted;
*/
};

#include <IV-2_6/_leave.h>

#endif
