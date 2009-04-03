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
 * MyMessage - display a string of text (fixed size)
 *
 * adapted from Message by David Sauder 
 */

#ifndef MyMessage_h
#define MyMessage_h

#define iv2_6_compatible

#include <IV-2_6/InterViews/interactor.h>

#include <IV-2_6/_enter.h>

class MyMessage : public Interactor {
public:
    MyMessage(
	const char* msg, Alignment al = Center, int Pad = 0,
	int hstretch = 0, int vstretch = 0
    );
    MyMessage(
	const char* name,
	const char* msg, Alignment al = Center, int Pad = 0,
	int hstretch = 0, int vstretch = 0
    );
    virtual ~MyMessage();

    virtual void Highlight(boolean);
    virtual void Realign(Alignment);
    void NewMessage (char* t);
    char* Text ();
protected:
    char* text;
    char* textReturned;
    Alignment alignment : 16;
    unsigned pad : 15;
    boolean highlighted : 1;

    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
    int textLen;
private:
    void Init(const char*, Alignment, int p, int hstretch, int vstretch);
};

#include <IV-2_6/_leave.h>

#endif
