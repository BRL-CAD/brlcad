
#include <ebutton.h>	// ivfasd/ebutton.h

#include <InterViews/font.h>
#include <InterViews/pattern.h>

#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/shape.h>
#include <IV-2_6/InterViews/subject.h>

#include <IV-2_6/_enter.h>

static const int pad = 5;

EButton::EButton(const char* c,
                 ButtonState* b, 
                 int i, 
                 const int horz, 
                 const int vert) : PushButton(c, b, i)
{
   _vert = vert;
   _horz = horz;
   SetClassName("EButton");
   Listen(noEvents);
}

EButton::EButton(const char* c,
                 ButtonState* b, 
                 int i) : PushButton(c, b, i)
{
   _vert = -1;
   _horz = -1;
   SetClassName("EButton");
   Listen(noEvents);
}

void EButton::Reconfig () {
    TextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	if (_vert<0 || _horz < 0)
	{
	    _vert = (output->GetFont()->Height()+pad*2)/2; 
	    _horz = (output->GetFont()->Width(text)+output->GetFont()->Width("   "))/2;
	}
	shape->width = 2*_horz+1;
	shape->height = 2*_vert+1;
    }
}

void EButton::Redraw (IntCoord x1, IntCoord y1, IntCoord x2, IntCoord y2) {
    output->ClearRect(canvas, x1, y1, x2, y2);
    Refresh();
}

void EButton::Refresh () {
    IntCoord x, y;
    IntCoord tx, ty;

    x = xmax/2;
    y = ymax/2;
    tx = (xmax - output->GetFont()->Width(text))/2;
    ty = (ymax-output->GetFont()->Height())/2;
    if (chosen || hit) {
		/* (Redundant casts to work around a bug in cfront 1.2 .) */
	output->FillEllipse(canvas, x, y, _horz, _vert);
	background->Text(canvas, text, tx, ty);
    } else {
	background->FillRect(canvas, x-_horz, y-_vert, x+_horz, y+_vert);
		/* (Redundant casts to work around a bug in cfront 1.2 .) */
	output->Ellipse(canvas, x, y, _horz, _vert);
	output->Text(canvas, text, tx, ty);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
}

#include <IV-2_6/_leave.h>
