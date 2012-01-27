#include <windows.h>

static bool
bootstrap()
{
  return SetDllDirectory(BRLCAD_ROOT "\lib");
}


static bool initialized = bootstrap();

