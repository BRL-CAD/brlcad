/*                     G _ B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_  */
/*@{*/
/** @file g_brep.c
 *
 */

#include "brep.h"
#include "raytrace.h"
#include "rtgeom.h"
#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

int 
rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip);
void 
rt_brep_print(register const struct soltab *stp);
int
rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
void
rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp);
void
rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp);
int
rt_brep_class();
void
rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp);
void
rt_brep_free(register struct soltab *stp);
int
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol);
int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip);
int
rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip);
void
rt_brep_ifree(struct rt_db_internal *ip);
int
rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local);
int
rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr);
int
rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv);

#ifdef __cplusplus
  }
#endif

/**
 *   			R T _ B R E P _ P R E P
 *
 *  Given a pointer of a GED database record, and a transformation matrix,
 *  determine if this is a valid NURB, and if so, prepare the surface
 *  so the intersections will work.
 */
int
rt_brep_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip)
{
    /* This prepares the NURBS specific data structures to be used
       during intersection... i.e. acceleration data structures and
       whatever else is needed.
     
       Abert's paper (Direct and Fast Ray Tracing of NURBS Surfaces)
       suggests using a bounding volume hierarchy (instead of KD-tree)
       and building it down to a satisfactory flatness criterion (which
       they do not give information about).
    */
    struct rt_brep_internal* sip;

    return 0;
}


/**
 *			R T _ B R E P _ P R I N T
 */
void
rt_brep_print(register const struct soltab *stp)
{
}


/**
 *  			R T _ B R E P _ S H O T
 *
 *  Intersect a ray with a nurb.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_brep_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    return 0;
}


/**
 *  			R T _ B R E P _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_brep_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
}


/**
 *			R T _ B R E P _ C U R V E
 *
 *  Return the curvature of the nurb.
 */
void
rt_brep_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
}

/**
 *                      R T _ B R E P _ C L A S S
 *
 *  Don't know what this is supposed to do...
 *
 *  Looking at g_arb.c, seems the actual signature is:
 *    class(const struct soltab* stp, 
 *          const fastf_t* min, 
 *          const fastf_t* max, 
 *          const struct bn_tol* tol)
 *
 *  Hmmm...
 */
int
rt_brep_class() 
{
  return RT_CLASSIFY_UNIMPLEMENTED; 
}


/**
 *  			R T _ B R E P _ U V
 *
 *  For a hit on the surface of an nurb, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_brep_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
}


/**
 *		R T _ B R E P _ F R E E
 */
void
rt_brep_free(register struct soltab *stp)
{
}


/**
 *			R T _ B R E P _ P L O T
 */
int
rt_brep_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    return 0;
}


/**
 *			R T _ B R E P _ T E S S
 */
int
rt_brep_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    return -1;
}


/* XXX In order to facilitate exporting the ON_BRep object without a
 * whole lot of effort, we're going to (for now) extend the
 * ON_BinaryArchive to support an "in-memory" representation of a
 * binary archive. Currently, the openNURBS library only supports
 * file-based archiving operations. This implies the 
 */

class ON_CLASS RT_MemoryArchive : public ON_BinaryArchive
{
public:
  RT_MemoryArchive();
  virtual ~RT_MemoryArchive();
  
  // ON_BinaryArchive overrides
  size_t CurrentPosition() const;
  bool SeekFromCurrentPosition(int);
  bool SeekFromStart(size_t);
  bool AtEnd() const;
  
protected:
  size_t Read(size_t, void*);
  size_t Write(size_t, const void*);
  bool Flush();
  
private:
  std::stringstream m_buffer;
};

RT_MemoryArchive::RT_MemoryArchive()
  : ON_BinaryArchive(ON::write), m_buffer(std::stringstream::out)
{
}

RT_MemoryArchive::~RT_MemoryArchive()
{
}

size_t 
RT_MemoryArchive::CurrentPosition() const
{
  return 0; // XXX FIX me, because tellg() in std::istream is NOT const!!!! stupidity
}

bool
RT_MemoryArchive::SeekFromCurrentPosition(int seek_to)
{
  std::streampos p = seek_to;
  m_buffer.seekg(m_buffer.tellg()+p);
  m_buffer.seekp(m_buffer.tellp()+p);
  return m_buffer.good();
}

bool 
RT_MemoryArchive::SeekFromStart(size_t seek_to)
{
  m_buffer.seekg(seek_to);
  m_buffer.seekp(seek_to);
  return m_buffer.good();
}

bool 
RT_MemoryArchive::AtEnd() const
{
  return m_buffer.eof();
}

size_t 
RT_MemoryArchive::Read(size_t amount, void* buf)
{
  m_buffer.read((char*)buf, amount);
  return m_buffer.gcount();
}

size_t 
RT_MemoryArchive::Write(size_t amount, const void* buf)
{
  m_buffer.write((const char*)buf, amount);
  return (m_buffer.good()) ? amount : 0;
}

bool 
RT_MemoryArchive::Flush()
{
  m_buffer.flush();
  return m_buffer.good();
}
 

/**
 *			R T _ B R E P _ E X P O R T 5
 */
int
rt_brep_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_brep_internal* oni;
    
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BREP) return -1;
    oni = (struct rt_brep_internal*)ip->idb_ptr;
    RT_BREP_CK_MAGIC(oni);

    BU_INIT_EXTERNAL(ep);
    ep->ext_nbytes = 0;
    
    

    return 0;
}




/**
 *			R T _ B R E P _ I M P O R T 5
 */
int
rt_brep_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{

    return 0;
}


/**
 *			R T _ B R E P _ I F R E E
 */
void
rt_brep_ifree(struct rt_db_internal *ip)
{
}


/**
 *			R T _ B R E P _ D E S C R I B E
 */
int
rt_brep_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    return 0;
}

/**
 *                      R T _ B R E P _ T C L G E T
 */
int
rt_brep_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
    return 0;
}


/**
 *                      R T _ B R E P _ T C L A D J U S T
 */
int
rt_brep_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
    return 0;
}

/*@}*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
