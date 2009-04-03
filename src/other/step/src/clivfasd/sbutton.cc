
#include <sbutton.h>	// ivfasd/sbutton.h

#include <InterViews/font.h>
#include <InterViews/pattern.h>

#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/shape.h>
#include <IV-2_6/InterViews/subject.h>

#include <IV-2_6/_enter.h>

static const int pad = 3;

SButton::SButton(const char* c, ButtonState* b, int i) : PushButton(c, b, i)
{
  SetClassName("SButton");
  Listen(noEvents);
}

void SButton::Reconfig () {
    TextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	shape->width += output->GetFont()->Width("  ");
	shape->height += 2*pad;
    }
}

void SButton::Redraw (IntCoord x1, IntCoord y1, IntCoord x2, IntCoord y2) {
    output->ClearRect(canvas, x1, y1, x2, y2);
    Refresh();
}

void SButton::Refresh () {
    IntCoord x[4], y[4];
    IntCoord tx, ty;

    x[0] = 0; y[0] = 0;
    x[1] = 0; y[1] = ymax;
    x[2] = xmax; y[2] = ymax;
    x[3] = xmax; y[3] = 0;
    tx = (xmax - output->GetFont()->Width(text))/2;
    ty = pad;
    if (chosen || hit) {
		/* (Redundant casts to work around a bug in cfront 1.2 .) */
	output->FillPolygon(canvas, (Coord *)x, (Coord *)y, 4);
	background->Text(canvas, text, tx, ty);
    } else {
	background->FillRect(canvas, 0, 0, xmax, ymax);
		/* (Redundant casts to work around a bug in cfront 1.2 .) */
	output->Polygon(canvas, (Coord *)x, (Coord *)y, 4);
	output->Text(canvas, text, tx, ty);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
}

#include <IV-2_6/_leave.h>
