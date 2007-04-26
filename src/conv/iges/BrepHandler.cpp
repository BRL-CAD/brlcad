#include "n_iges.hpp"

namespace brlcad {

BrepHandler::BrepHandler() {}

BrepHandler::~BrepHandler() {}

void 
BrepHandler::extract(IGES* iges, const DirectoryEntry* de) {
  ParameterData params;
  iges->getParameter(de->paramData(), params);
  
}
  
// extract calls these methods for custom work!
ShellHandler* 
BrepHandler::handleBoundaryShell() {
  debug("handleBoundaryShell");
  return NULL;
}

ShellHandler* 
BrepHandler::handleVoidShell() {
  debug("handleVoidShell");
  return NULL;
}

}
