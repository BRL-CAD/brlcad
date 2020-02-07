#ifndef CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H

#include "common.h"

/* system interface headers */
#include <list>
#include <string>

/* must come before step headers */
#include "opennurbs.h"

/* interface headers */
#include "ShapeRepresentation.h"

// forward declaration of class
class Axis2Placement3D;

class ManifoldSurfaceShapeRepresentation : public ShapeRepresentation
{
private:
    static std::string entityname;
    static EntityInstanceFunc GetInstance;

protected:

public:
    ManifoldSurfaceShapeRepresentation();
    ManifoldSurfaceShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~ManifoldSurfaceShapeRepresentation();

    Axis2Placement3D *GetAxis2Placement3d();

    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    std::string Name() {
	return name;
    };
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};


#endif /* CONV_STEP_STEP_G_MANIFOLDSURFACESHAPEREPRESENTATION_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
