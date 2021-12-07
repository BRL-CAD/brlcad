#ifndef _MATERIALX_H
#define _MATERIALX_H

#include "common.h"
#include "rt/defines.h"
#include "rt/nongeom.h"
#include "bu/vls.h"

__BEGIN_DECLS

RT_EXPORT extern void rt_material_mtlx_to_osl(const rt_material_internal material_ip, struct bu_vls* vp);

__END_DECLS
#endif		/* _MATERIALX_H */
