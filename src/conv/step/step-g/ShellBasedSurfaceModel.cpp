#include "STEPWrapper.h"
#include "Factory.h"

#include "ShellBasedSurfaceModel.h"
#include "OpenShell.h"

#define CLASSNAME "ShellBasedSurfaceModel"
#define ENTITYNAME "Shell_Based_Surface_Model"
string ShellBasedSurfaceModel::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ShellBasedSurfaceModel::Create);

ShellBasedSurfaceModel::ShellBasedSurfaceModel()
{
    step = NULL;
    id = 0;
    sbsm_boundary.clear();
}

ShellBasedSurfaceModel::ShellBasedSurfaceModel(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    sbsm_boundary.clear();
}

ShellBasedSurfaceModel::~ShellBasedSurfaceModel()
{
    sbsm_boundary.clear();
}

bool
ShellBasedSurfaceModel::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!GeometricRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    if (sbsm_boundary.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "sbsm_boundary");
	if (attr) {
	    SelectAggregate *sa = static_cast<SelectAggregate *>(attr->ptr.a);
	    if (!sa) goto step_error;
	    SelectNode *sn = static_cast<SelectNode *>(sa->GetHead());

	    SDAI_Select *stype;
	    while (sn != NULL) {
		stype = static_cast<SDAI_Select *>(sn->node);
		const TypeDescriptor *underlying_type = stype->CurrentUnderlyingType();
		if (underlying_type == SCHEMA_NAMESPACE::e_open_shell) {
		    SdaiShell *shell = (SdaiShell *)stype;
		    if (shell->IsOpen_shell()) {
			SdaiOpen_shell *oshell = *shell;
			OpenShell *os = dynamic_cast<OpenShell *>(Factory::CreateObject(sw, (SDAI_Application_instance *)oshell));
			if (os) {
			    sbsm_boundary.push_back(os);
			}
		    }
		}
		sn = (SelectNode *)sn->NextNode();
	    }
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
step_error:
    sw->entity_status[id] = STEP_LOAD_ERROR;
    return false;
}

void
ShellBasedSurfaceModel::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level + 1);
    std::cout << "sbsm_boundary:" << std::endl;
    LIST_OF_OPEN_SHELLS::iterator i;
    for (i = sbsm_boundary.begin(); i != sbsm_boundary.end(); ++i) {
	(*i)->Print(level + 1);
    }

    GeometricRepresentationItem::Print(level + 1);
}

STEPEntity *
ShellBasedSurfaceModel::GetInstance(STEPWrapper *sw, int id)
{
    return new ShellBasedSurfaceModel(sw, id);
}

STEPEntity *
ShellBasedSurfaceModel::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
