
#include <IV-2_6/InterViews/button.h>

#include <IV-2_6/_enter.h>

class SButton : public PushButton
{
 public:
  SButton(const char*, ButtonState*, int);
  virtual void Refresh();
 protected:
  virtual void Reconfig();
  virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
};

#include <IV-2_6/_leave.h>
