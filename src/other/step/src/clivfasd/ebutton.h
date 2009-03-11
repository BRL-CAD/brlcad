
#include <IV-2_6/InterViews/button.h>

#include <IV-2_6/_enter.h>

class EButton : public PushButton
{
public:
  EButton(const char*, ButtonState*, int, const int, const int);
  EButton(const char*, ButtonState*, int);
protected:
  virtual void Reconfig();
  virtual void Refresh();
  virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:
  int _vert;
  int _horz;
};

#include <IV-2_6/_leave.h>
