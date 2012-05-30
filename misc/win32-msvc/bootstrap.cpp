#include "common.h"
#include "bio.h"

/*
 * This bootstrap's application static initialization on Windows to
 * add BRL-CAD's library installation path to the list of paths
 * searched for DLLs.  This is useful in leu of requiring PATH being
 * set apriori, avoids registry entries (and associated permissions
 * required), and avoids putting libraries in the same dir as binaries
 * (which would make our installation hierarchy inconsistent).
 */
static bool
bootstrap()
{
  return SetDllDirectory(BRLCAD_ROOT "\\lib");
}


BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
  return bootstrap();
}


static bool initialized = bootstrap();

