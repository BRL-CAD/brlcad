/*
 *			P H O T O N M A P. C
 *
 *  Implemention of Photon Mapping
 *
 *  Author -
 *	Justin L. Shumaker
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2002-2003 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSphotonmap[] = "";
#endif


#include "photonmap.h"
#include "plastic.h"
#include "light.h"
#include <stdlib.h>

#define	NRoot(x,y) exp(log(x)/y)	/* Not in Use */
int Hit(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs);
void GetEstimate(vect_t irrad, point_t pos, vect_t normal, fastf_t rad, int np, int map, int iter, int centog);
void Polar2Euclidian(vect_t Dir, vect_t Normal, double Theta, double Phi);


int			PM_Activated;

struct	PhotonMap	*PMap[3];		/* Photon Map (KD-TREE) */
struct	Photon		*Emit[3];		/* Emitted Photons */
struct	Photon		CurPh;
vect_t			BBMin;			/* Min Bounding Box */
vect_t			BBMax;			/* Max Bounding Box */
int			Depth;			/* Used to determine how many times the photon has propogated */
int			PType;			/* Used to determine the type of Photon: Direct,Indirect,Specular,Caustic */
int			PInit;
int			EPM;			/* Emitted Photons Master */
int			EPS[3];			/* Emitted Photons Seperate, one for each map */
int			ICSize;
double			ScaleFactor;
struct	IrradCache	*IC;			/* Irradiance Cache for Hypersampling */
char			*Map;			/* Used for Irradiance HyperSampling Cache */
int			GPM_IH;			/* Irradiance Hypersampling Toggle, 0=off, 1=on */
int			GPM_WIDTH;
int			GPM_HEIGHT;
int			GPM_RAYS;		/* Number of Sample Rays for each Direction in Irradiance Hemi */
double			GPM_ATOL;		/* Angular Tolerance for Photon Gathering */
struct	resource	*GPM_RTAB;		/* Resource Table for Multi-threading */



/* Split so that equal numbers are above and below the splitting plane */
int FindMedian(struct Photon *List, int Num, int Axis) {
  int		i;
  fastf_t	Min,Max,Mean;

  Min= Max= List[0].Pos[Axis];
  for (i= 1; i < Num; i++) {
    if (List[i].Pos[Axis] < Min)
      Min= List[i].Pos[Axis];
    if (List[i].Pos[Axis] > Max)
      Max= List[i].Pos[Axis];
  } 
  Mean= (Min+Max)/2.0;
  i= 0;
  while (List[i].Pos[Axis] < Mean && i < Num)
    i++;

  return i;
}


/* Generate a KD-Tree from a Flat Array of Photons */
void BuildTree(struct Photon *EList, int ESize, struct PNode *Root) {
  struct	Photon	*LList,*RList;
  vect_t		Min,Max;
  int			i,Axis,MedianIndex,LInd,RInd;


  /* Allocate memory for left and right lists */
  LList= (struct Photon*)malloc(sizeof(struct Photon)*ESize);
  RList= (struct Photon*)malloc(sizeof(struct Photon)*ESize);

  /* Find the Bounding volume of the Current list of photons */
  Min[0]= Max[0]= EList[0].Pos[0];
  Min[1]= Max[1]= EList[0].Pos[1];
  Min[2]= Max[2]= EList[0].Pos[2];
  for (i= 1; i < ESize; i++) {
    if (EList[i].Pos[0] < Min[0]) Min[0]= EList[i].Pos[0];
    if (EList[i].Pos[1] < Min[1]) Min[1]= EList[i].Pos[1];
    if (EList[i].Pos[2] < Min[2]) Min[2]= EList[i].Pos[2];
    if (EList[i].Pos[0] > Max[0]) Max[0]= EList[i].Pos[0];
    if (EList[i].Pos[1] > Max[1]) Max[1]= EList[i].Pos[1];
    if (EList[i].Pos[2] > Max[2]) Max[2]= EList[i].Pos[2];
  }

  /* Obtain splitting Axis, which is the largest dimension of the bounding volume */
  Max[0]-= Min[0];
  Max[1]-= Min[1];
  Max[2]-= Min[2];
  Axis= 0;
  if (Max[1] > Max[0] && Max[1] > Max[2]) Axis= 1;
  if (Max[2] > Max[0] && Max[2] > Max[1]) Axis= 2;

  /* Find Median Photon to splt by. */
  MedianIndex= FindMedian(EList,ESize,Axis);

  /* Build Left and Right Lists and make sure the Median Photon is not included in either list. */
  LInd= RInd= 0;
  for (i= 0; i < ESize; i++) {
    if (i != MedianIndex) {
      if (EList[i].Pos[Axis] < EList[MedianIndex].Pos[Axis]) {
        LList[LInd++]= EList[i];
      } else {
        RList[RInd++]= EList[i];
      }
/*
      if (EList[i].Pos[Axis] < Median.Pos[Axis]) {
        LList[LInd++]= EList[i];
      } else {
        RList[RInd++]= EList[i];
      }
*/
    }
  }

  /* Store the Median Photon into the KD-Tree. */
/*  bu_log("insertKD: %.3f,%.3f,%.3f\n",EList[MedianIndex].Pos[0],EList[MedianIndex].Pos[1],EList[MedianIndex].Pos[2]);*/
  Root -> P= EList[MedianIndex];
  Root -> P.Axis= Axis;
  Root -> C= 0;

  /* With Left and Right if either contain any photons then repeat this process */
/*  if (LInd) bu_log("Left Branch\n");*/
  if (LInd) {
    Root -> L= (struct PNode*)malloc(sizeof(struct PNode));
    Root -> L -> L= 0;
    Root -> L -> R= 0;
    BuildTree(LList,LInd,Root -> L);
  }
/*  if (RInd) bu_log("Right Branch\n");*/
  if (RInd) {
    Root -> R= (struct PNode*)malloc(sizeof(struct PNode));
    Root -> R -> L= 0;
    Root -> R -> R= 0;
    BuildTree(RList,RInd,Root -> R);
  }

  free(LList);
  free(RList);
}


/* Places photon into flat array that wwill form the final kd-tree. */
void Store(point_t Pos, vect_t Dir, vect_t Normal, int Map) {
  int			i;


  EPM++;
  if (PMap[Map] -> StoredPhotons < PMap[Map] -> MaxPhotons) {

    for (i= 0; i < 3; i++) {
      /* Store Position, Direction, and Power of Photon */
      Emit[Map][PMap[Map] -> StoredPhotons].Pos[i]= Pos[i];
      Emit[Map][PMap[Map] -> StoredPhotons].Dir[i]= Dir[i];
      Emit[Map][PMap[Map] -> StoredPhotons].Normal[i]= Normal[i];
      Emit[Map][PMap[Map] -> StoredPhotons].Power[i]= CurPh.Power[i];
    }
    PMap[Map] -> StoredPhotons++;

/* bu_log("Map: %d, Size: %d\n",Map,PMap[Map] -> StoredPhotons);*/
  }

  if (!EPS[Map] && PMap[Map] -> StoredPhotons == PMap[Map] -> MaxPhotons)
    EPS[Map]= EPM;

/*
  bu_log("[%d][%d][%.3f,%.3f,%.3f]\n",Map,PMap[Map] -> StoredPhotons,CurPh.Power[0],CurPh.Power[1],CurPh.Power[2]);
  if (!(PMap[Map] -> StoredPhotons % 64))
  bu_log("[%d][%d][%.3f,%.3f,%.3f]\n",Map,PMap[Map] -> StoredPhotons,Pos[0],Pos[1],Pos[2]);
*/
}


/* Compute a specular reflection */
void SpecularReflect(vect_t normal, vect_t rdir) {
  vect_t	d;
  fastf_t	dot;

  VSCALE(d,rdir,-1);
  dot= VDOT(d,normal);

  if (dot < 0.0f) {
    rdir[0]= rdir[1]= rdir[2]= 0;
  } else {
    VSCALE(rdir,normal,2*dot);
    VSUB2(rdir,rdir,d);
  }
}


/* Compute a random reflected diffuse direction */
void DiffuseReflect(vect_t normal, vect_t rdir) {
  /* Allow Photons to get a random direction at most 60 degrees to the normal */
  do {
    rdir[0]= 2.0*drand48()-1.0;
    rdir[1]= 2.0*drand48()-1.0;
    rdir[2]= 2.0*drand48()-1.0;
    VUNITIZE(rdir);
  } while (VDOT(rdir,normal) < 0.5);
}


/* Compute refracted ray given Incident Ray, Normal, and 2 refraction indices */
int Refract(vect_t I, vect_t N, fastf_t n1, fastf_t n2) {
  fastf_t	n,c1,c2,radicand;
  vect_t	t,r;

  n= n1/n2;
  c1= -VDOT(I,N);
  radicand= 1.0 - (n*n)*(1.0-c1*c1);
  if (radicand < 0) {
    /* Total Internal Reflection */
    I[0]= I[1]= I[2]= 0;
    return(0);
  }
  c2= sqrt(radicand);

  VSCALE(r,I,n);
  VSCALE(t,N,n*c1-c2);
  VADD2(I,r,t);
  return(1);
}


int CheckMaterial(char *cmp, char *MS) {
  int	i;

  if (MS) {
    for (i= 0; i < strlen(cmp) && i < strlen(MS); i++)
      if (MS[i] != cmp[i])
        return(0);
    return(1);
  } else {
    return(0);
  }
}



/* This function parses the material string to obtain specular and refractive values */
void GetMaterial(char *MS, vect_t spec, fastf_t *refi, fastf_t *transmit) {
  struct	phong_specific	*phong_sp;
  struct	bu_vls		matparm;

  phong_sp= (struct phong_specific*)malloc(sizeof(struct phong_specific));

  /* Initialize spec and refi */
  spec[0]= spec[1]= spec[2]= *refi= *transmit= 0;
  if (CheckMaterial("plastic",MS)) { /* Checks that the first 7 chars match any of the characters found in plastic */
    /* Plastic Shader */
    phong_sp -> magic= PL_MAGIC;
    phong_sp -> shine= 10;
    phong_sp -> wgt_specular= 0.7;
    phong_sp -> wgt_diffuse= 0.3;
    phong_sp -> transmit= 0.0;
    phong_sp -> reflect= 0.0;
    phong_sp -> refrac_index= 1.0;
    phong_sp -> extinction= 0.0;
/*
    BU_GETSTRUCT(phong_sp, phong_specific);
    memcpy(phong_sp, &phong_defaults, sizeof(struct phong_specific) );
*/
    MS+= 7;
    bu_vls_init(&matparm);
    bu_vls_printf(&matparm,"%s",MS);
    bu_struct_parse(&matparm, phong_parse, (char *)phong_sp);
    bu_vls_free(&matparm);

/*    bu_log("string: %s\n",MS);*/
/*    bu_log("info[%d]: sh: %d, spec: %.3f, diff: %.3f, tran: %.3f, refl: %.3f, refr: %.3f\n", PMap -> StoredPhotons, phong_sp -> shine, phong_sp -> wgt_specular, phong_sp -> wgt_diffuse, phong_sp -> transmit, phong_sp -> reflect, phong_sp -> refrac_index);*/
/*    bu_log("spec[%d]: %.3f\n",PMap -> StoredPhotons, phong_sp -> wgt_specular);*/

    spec[0]= spec[1]= spec[2]= phong_sp -> wgt_specular;
    *refi= phong_sp -> refrac_index;
    spec[0]= spec[1]= spec[2]= 0.7;
/*
    *refi= 1.0;
    *transmit= 0.0;
*/
/*
    if (phong_sp -> refrac_index != 1.0)
    bu_log("refi: %.3f\n",phong_sp -> refrac_index);
*/
  } else if (CheckMaterial("glass",MS)) {
    /* Glass Shader */
    phong_sp -> magic= PL_MAGIC;
    phong_sp -> shine= 4;
    phong_sp -> wgt_specular= 0.7;
    phong_sp -> wgt_diffuse= 0.3;
    phong_sp -> transmit= 0.8;
    phong_sp -> reflect= 0.1;
    phong_sp -> refrac_index= 1.65;
    phong_sp -> extinction= 0.0;

/*
    BU_GETSTRUCT(phong_sp, phong_specific);
    memcpy(phong_sp, &phong_defaults, sizeof(struct phong_specific) );
*/
    MS+= 5; /* move pointer past "pm " (3 characters) */
    bu_vls_init(&matparm);
    bu_vls_printf(&matparm,"%s",MS);
    bu_struct_parse(&matparm, phong_parse, (char *)phong_sp);
    bu_vls_free(&matparm);

/*    bu_log("string: %s\n",MS);*/
/*    bu_log("info[%d]: sh: %d, spec: %.3f, diff: %.3f, tran: %.3f, refl: %.3f, refr: %.3f\n", PMap -> StoredPhotons, phong_sp -> shine, phong_sp -> wgt_specular, phong_sp -> wgt_diffuse, phong_sp -> transmit, phong_sp -> reflect, phong_sp -> refrac_index);*/
/*    bu_log("spec[%d]: %.3f\n",PMap -> StoredPhotons, phong_sp -> wgt_specular);*/

    spec[0]= spec[1]= spec[2]= phong_sp -> wgt_specular;
    *refi= phong_sp -> refrac_index;
    *transmit= phong_sp -> transmit;

/*
    if (phong_sp -> refrac_index != 1.0)
    bu_log("refi: %.3f\n",phong_sp -> refrac_index);
*/
  }

  free(phong_sp);
}


fastf_t max(fastf_t a, fastf_t b, fastf_t c) {
  return a > b ? a > c ? a : c : b > c ? b : c;
}


int HitRef(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs) {
  struct	partition	*part;
  vect_t			pt,normal,spec;
  fastf_t			refi,transmit;

  ap -> a_hit= Hit;


  part= PartHeadp -> pt_forw;

  /* Compute Intersection Point, Pt= o + td */
  VJOIN1(pt, ap -> a_ray.r_pt, part -> pt_outhit -> hit_dist, ap -> a_ray.r_dir);

  /* Fetch Intersection Normal */
  RT_HIT_NORMAL(normal, part -> pt_inhit, part -> pt_inseg -> seg_stp, &(ap->a_ray), part -> pt_inflip);
/*  RT_HIT_NORMAL(normal, part -> pt_outhit, part -> pt_inseg -> seg_stp, &(ap->a_ray), part -> pt_inflip);*/
/*  RT_HIT_NORMAL(normal, part -> pt_outhit, part -> pt_outseg -> seg_stp, &(ap->a_ray), part -> pt_outflip);*/

  /* Assign pt */
  ap -> a_ray.r_pt[0]= pt[0];
  ap -> a_ray.r_pt[1]= pt[1];
  ap -> a_ray.r_pt[2]= pt[2];


  /* Fetch Material */
  GetMaterial(part -> pt_regionp -> reg_mater.ma_shader, spec, &refi, &transmit);


  if (Refract(ap -> a_ray.r_dir,normal,refi,1.0)) {
/*
    bu_log("1D: %d, [%.3f,%.3f,%.3f], [%.3f,%.3f,%.3f], [%.3f,%.3f,%.3f]\n",Depth,pt[0],pt[1],pt[2],ap -> a_ray.r_dir[0], ap -> a_ray.r_dir[1], ap -> a_ray.r_dir[2],normal[0],normal[1],normal[2]);
    bu_log("p1: [%.3f,%.3f,%.3f]\n",part -> pt_inhit -> hit_point[0], part -> pt_inhit -> hit_point[1], part -> pt_inhit -> hit_point[2]);
    bu_log("p2: [%.3f,%.3f,%.3f]\n",part -> pt_outhit -> hit_point[0], part -> pt_outhit -> hit_point[1], part -> pt_outhit -> hit_point[2]);
*/
    Depth++;
    rt_shootray(ap);
  } else {
    bu_log("TIF\n");
  }

  ap -> a_onehit= 0;
  return(1);
}


/* Callback for Ray Hit */
/* The 'current' photon is Emit[PMap -> StoredPhotons] */
int Hit(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs) {
  struct	partition	*part;
  vect_t			pt,normal,color,spec,power;
  fastf_t			refi,transmit,prob,prob_diff,prob_spec,prob_ref;
  int				hit;


  /* Move ptr forward to next region and call Hit recursively until reaching a region this is either
     not a light or none at all */
  hit= 0;
  for (BU_LIST_FOR(part, partition, (struct bu_list *)PartHeadp)) {
    if (part != PartHeadp) {
    hit++;
    VJOIN1(pt, ap -> a_ray.r_pt, part -> pt_inhit -> hit_dist, ap -> a_ray.r_dir);
/*    printf("pt[%d][%d]: --- [%.3f,%.3f,%.3f], %s\n",hit,CheckMaterial("light",part -> pt_regionp -> reg_mater.ma_shader),pt[0],pt[1],pt[2],part -> pt_regionp -> reg_mater.ma_shader);*/

      if (!CheckMaterial("light",part -> pt_regionp -> reg_mater.ma_shader)) {
/*        bu_log("  Found object!\n");*/
        break;
      }
    }
  }


  if (part == PartHeadp)
    return 0;


  if (CheckMaterial("light",part -> pt_regionp -> reg_mater.ma_shader))
    return 0;


  /* Compute Intersection Point, Pt= o + td */
  VJOIN1(pt, ap -> a_ray.r_pt, part -> pt_inhit -> hit_dist, ap -> a_ray.r_dir);


  /* Generate Bounding Box for Scaling Phase */
  if (PInit) {
    BBMin[0]= BBMax[0]= pt[0];
    BBMin[1]= BBMax[1]= pt[1];
    BBMin[2]= BBMax[2]= pt[2];
    PInit= 0;
  } else {
    if (pt[0] < BBMin[0])
      BBMin[0]= pt[0];
    if (pt[0] > BBMax[0])
      BBMax[0]= pt[0];
    if (pt[1] < BBMin[1])
      BBMin[1]= pt[1];
    if (pt[1] > BBMax[1])
      BBMax[1]= pt[1];
    if (pt[2] < BBMin[2])
      BBMin[2]= pt[2];
    if (pt[2] > BBMax[2])
      BBMax[2]= pt[2];
  }

  /* Fetch Intersection Normal */
  RT_HIT_NORMAL(normal, part -> pt_inhit, part -> pt_inseg -> seg_stp, &(ap->a_ray), part -> pt_inflip);

  /* Fetch Material */
  GetMaterial(part -> pt_regionp -> reg_mater.ma_shader, spec, &refi, &transmit);
/*  bu_log("Spec: [%.3f,%.3f,%.3f], Refi: %.3f\n",spec[0],spec[1],spec[2],refi);*/


  /* Compute Diffuse, Specular, and Caustics */
  color[0]= part -> pt_regionp -> reg_mater.ma_color[0];
  color[1]= part -> pt_regionp -> reg_mater.ma_color[1];
  color[2]= part -> pt_regionp -> reg_mater.ma_color[2];

  prob_ref= max(color[0]+spec[0],color[1]+spec[1],color[2]+spec[2]);
  prob_diff= ((color[0]+color[1]+color[2])/(color[0]+color[1]+color[2]+spec[0]+spec[1]+spec[2]))*prob_ref;
  prob_spec= prob_ref - prob_diff;
  prob= drand48();

/* bu_log("pr: %.3f, pd: %.3f, [%.3f,%.3f,%.3f] [%.3f,%.3f,%.3f]\n",prob_ref,prob_diff,color[0],color[1],color[2],spec[0],spec[1],spec[2]);*/
/* bu_log("prob: %.3f, prob_diff: %.3f, pd+ps: %.3f\n",prob,prob_diff,prob_diff+prob_spec);*/

  if (prob < 1.0 - transmit) {
    if (prob < prob_diff) {
      /* Store power of incident Photon */
      power[0]= CurPh.Power[0];
      power[1]= CurPh.Power[1];
      power[2]= CurPh.Power[2];


      /* Scale Power of reflected photon */
      CurPh.Power[0]= power[0]*color[0]/prob_diff;
      CurPh.Power[1]= power[1]*color[1]/prob_diff;
      CurPh.Power[2]= power[2]*color[2]/prob_diff;

      /* Store Photon */
/*      if ((Depth++ && PType == PM_GLOBAL) || PType == PM_CAUSTIC)*/
      Store(pt, ap -> a_ray.r_dir, normal,PType);

      /* Assign diffuse reflection direction */
      DiffuseReflect(normal,ap -> a_ray.r_dir);

      /* Assign pt */
      ap -> a_ray.r_pt[0]= pt[0];
      ap -> a_ray.r_pt[1]= pt[1];
      ap -> a_ray.r_pt[2]= pt[2];

      if (PType != PM_CAUSTIC)
      if (PMap[PM_GLOBAL] -> StoredPhotons < PMap[PM_GLOBAL] -> MaxPhotons) {
        Depth++;
        rt_shootray(ap);
      }
    } else if (prob >= prob_diff && prob < prob_diff + prob_spec) {
      /* Store power of incident Photon */
      power[0]= CurPh.Power[0];
      power[1]= CurPh.Power[1];
      power[2]= CurPh.Power[2];

      /* Scale power of reflected photon */
      CurPh.Power[0]= power[0]*spec[0]/prob_spec;
      CurPh.Power[1]= power[1]*spec[1]/prob_spec;
      CurPh.Power[2]= power[2]*spec[2]/prob_spec;

      /* Reflective */
      SpecularReflect(normal,ap -> a_ray.r_dir);

      /* Assign pt */
      ap -> a_ray.r_pt[0]= pt[0];
      ap -> a_ray.r_pt[1]= pt[1];
      ap -> a_ray.r_pt[2]= pt[2];

      PType= PM_CAUSTIC;
      Depth++;
      rt_shootray(ap);
    } else {
      /* Store Photon */
/*      if ((Depth++ && PType == 1) || PType == 3)*/
      Store(pt, ap -> a_ray.r_dir, normal,PType);
    }
  } else {
    if (refi > 1.0 && (PType == PM_CAUSTIC || Depth == 0)) {
      PType= PM_CAUSTIC;

      /* Store power of incident Photon */
      power[0]= CurPh.Power[0];
      power[1]= CurPh.Power[1];
      power[2]= CurPh.Power[2];

      /* Scale power of reflected photon */
      CurPh.Power[0]= power[0]*spec[0]/prob_spec;
      CurPh.Power[1]= power[1]*spec[1]/prob_spec;
      CurPh.Power[2]= power[2]*spec[2]/prob_spec;

      /* Refractive or Reflective */
      if (refi > 1.0 && prob < transmit) {
        CurPh.Power[0]= power[0];
        CurPh.Power[1]= power[1];
        CurPh.Power[2]= power[2];

        if (!Refract(ap -> a_ray.r_dir,normal,1.0,refi))
          printf("TIF0\n");

        ap -> a_hit= HitRef;

/*
        bu_log("dir: [%.3f,%.3f,%.3f]\n",ap -> a_ray.r_dir[0],ap -> a_ray.r_dir[1],ap -> a_ray.r_dir[2]);
        bu_log("ref: [%.3f,%.3f,%.3f]\n",ap -> a_ray.r_dir[0],ap -> a_ray.r_dir[1],ap -> a_ray.r_dir[2]);
        bu_log("nor: [%.3f,%.3f,%.3f]\n",normal[0],normal[1],normal[2]);
        bu_log("p0: [%.3f,%.3f,%.3f],[%.3f,%.3f,%.3f]\n",pt[0],pt[1],pt[2],ap -> a_ray.r_dir[0],ap -> a_ray.r_dir[1],ap -> a_ray.r_dir[2]);
*/
        ap -> a_onehit= 0;
      } else {
        SpecularReflect(normal,ap -> a_ray.r_dir);
      }
 
      /* Assign pt */
      ap -> a_ray.r_pt[0]= pt[0];
      ap -> a_ray.r_pt[1]= pt[1];
      ap -> a_ray.r_pt[2]= pt[2];

/*      bu_log("2D: %d, [%.3f,%.3f,%.3f], [%.3f,%.3f,%.3f], [%.3f,%.3f,%.3f]\n",Depth,pt[0],pt[1],pt[2],ap -> a_ray.r_dir[0], ap -> a_ray.r_dir[1], ap -> a_ray.r_dir[2],normal[0],normal[1],normal[2]);*/
      Depth++;
      rt_shootray(ap);
    }
  }
  return 1;
}


/* Callback for Ray Miss */
int Miss(struct application *ap) {
  EPM++;
  return 0;
}


/* ScalePhotonPower() is used to scale the power of all photons once they
 * have been emitted from the light source.  Scale= 1/(#emitted photons).
 * Call this function after each light source is processed. 
 * This function also handles setting a default power for the photons based
 * on the size of the scene, i.e power of light source */
void ScalePhotonPower(fastf_t Scale, int Map) {
  int		i;


  for (i= 0; i < PMap[Map] -> StoredPhotons; i++) {
    Emit[Map][i].Power[0]*= Scale;
    Emit[Map][i].Power[1]*= Scale;
    Emit[Map][i].Power[2]*= Scale;
  }
}



/* Emit a photons in a random direction based on a point light */
void EmitPhotonsRandom(struct application *ap, struct light_specific *lp) {
  vect_t	ldir;

  ldir[0]= 0;
  ldir[1]= 0;
  ldir[2]= -1;

/*
  for (i= 0; i < 8; i++) 
    bu_log("sample points: [%.3f,%.3f,%.3f]\n",lp -> lt_sample_pts[i].lp_pt[0], lp -> lt_sample_pts[i].lp_pt[1], lp -> lt_sample_pts[i].lp_pt[2]);
*/
  while (1) {
    /* If the Global Photon Map Completes before the Caustics Map, then it probably means there are no caustic objects in the Scene */
    if (PMap[PM_GLOBAL] -> StoredPhotons == PMap[PM_GLOBAL] -> MaxPhotons && (!PMap[PM_CAUSTIC] -> StoredPhotons || PMap[PM_CAUSTIC] -> StoredPhotons == PMap[PM_CAUSTIC] -> MaxPhotons))
      return;

    do {
    do {
      /* Set Ray Direction to application ptr */
/*
      ap -> a_ray.r_dir[0]= 2.0*rand()/RAND_MAX-1.0;
      ap -> a_ray.r_dir[1]= 2.0*rand()/RAND_MAX-1.0;
      ap -> a_ray.r_dir[2]= 2.0*rand()/RAND_MAX-1.0;
*/
      ap -> a_ray.r_dir[0]= 2.0*drand48()-1.0;
      ap -> a_ray.r_dir[1]= 2.0*drand48()-1.0;
      ap -> a_ray.r_dir[2]= 2.0*drand48()-1.0;

    } while (ap -> a_ray.r_dir[0]*ap -> a_ray.r_dir[0] + ap -> a_ray.r_dir[1]*ap -> a_ray.r_dir[1] + ap -> a_ray.r_dir[2]*ap -> a_ray.r_dir[2] > 1);
      /* Normalize Ray Direction */
      VUNITIZE(ap -> a_ray.r_dir);
    } while (drand48() > VDOT(ap -> a_ray.r_dir,ldir)); /* we want this to terminate when a rnd# is less than the angle */

    /* Set Ray Position to application ptr */
    ap -> a_ray.r_pt[0]= lp -> lt_pos[0];
    ap -> a_ray.r_pt[1]= lp -> lt_pos[1];
    ap -> a_ray.r_pt[2]= lp -> lt_pos[2];


    /* Shoot Photon into Scene */
/*bu_log("Shooting Ray: [%.3f,%.3f,%.3f] [%.3f,%.3f,%.3f]\n",lp -> lt_pos[0], lp -> lt_pos[1], lp -> lt_pos[2], x,y,z);*/
    CurPh.Power[0]=
    CurPh.Power[1]=
    CurPh.Power[2]= 1000.0 * 1000.0 * 3.0 * 10.0;

    Depth= 0;
    PType= PM_GLOBAL;
    rt_shootray(ap);
/*    bu_log("1: %d, 2: %d\n",PMap[PM_GLOBAL] -> StoredPhotons, PMap[PM_CAUSTIC] -> StoredPhotons);*/
  }
}


void SanityCheck(struct PNode *Root, int LR) {
  if (!Root)
    return;

  bu_log("Pos[%d]: [%.3f,%.3f,%.3f]\n",LR,Root -> P.Pos[0], Root -> P.Pos[1], Root -> P.Pos[2]);
  SanityCheck(Root -> L,1);
  SanityCheck(Root -> R,2);
}


int ICHit(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs) {
  struct	partition	*part;
  point_t			pt,normal;
  vect_t			C1,C2;

  part= PartHeadp -> pt_forw;

  VJOIN1(pt, ap -> a_ray.r_pt, part -> pt_inhit -> hit_dist, ap -> a_ray.r_dir);

  RT_HIT_NORMAL(normal, part -> pt_inhit, part -> pt_inseg -> seg_stp, &(ap->a_ray), part -> pt_inflip);
  GetEstimate(C1, pt, normal, ScaleFactor/200.0, PMap[PM_GLOBAL] -> StoredPhotons / 100, PM_GLOBAL, 5, 1);
  GetEstimate(C2 ,pt, normal, (int)(ScaleFactor/pow(2,(log(PMap[PM_CAUSTIC] -> MaxPhotons/2)/log(4)))), PMap[PM_CAUSTIC] -> MaxPhotons / 50,PM_CAUSTIC, 1, 0);
/*    GetEstimate(IMColor2, pt, normal, (int)(ScaleFactor/100.0),PMap[PM_CAUSTIC] -> MaxPhotons/50,PM_CAUSTIC,1, 0);*/

  (*(vect_t*)ap -> a_purpose)[0]+= C1[0] + C2[0];
  (*(vect_t*)ap -> a_purpose)[1]+= C1[1] + C2[1];
  (*(vect_t*)ap -> a_purpose)[2]+= C1[2] + C2[2];

  return(1);
}


int ICMiss(struct application *ap) {
  /* Set to Background/Ambient Color later */
  return(0);
}


/* Convert a Polar vector into a euclidian vector:
 * - Requires that an orthogonal basis, so generate one using the photon normal as the first vector.
 * - The Normal passed to this function must be unitized.
 * - This took me almost 3 hours to write, and it's tight.
 */
void Polar2Euclidian(vect_t Dir, vect_t Normal, double Theta, double Phi) {
  int		i;
  vect_t	BasisX,BasisY;


  BasisX[0]= fabs(Normal[0]) < 0.001 ? 1.0 : fabs(Normal[0]) > 0.999 ? 0.0 : -(1.0/Normal[0]);
  BasisX[1]= fabs(Normal[1]) < 0.001 ? 1.0 : fabs(Normal[1]) > 0.999 ? 0.0 :  (1.0/Normal[1]);
  BasisX[2]= 0.0;
  VUNITIZE(BasisX);
  VCROSS(BasisY,Normal,BasisX);
	
  for (i= 0; i < 3; i++)
    Dir[i]= sin(Theta)*cos(Phi)*BasisX[i] + sin(Theta)*sin(Phi)*BasisY[i] + cos(Theta)*Normal[i];
}


/*
 * Irradiance Calculation for a given position
 */
void Irradiance(int pid, struct Photon *P, struct application *ap) {
  struct	application	*lap;		/* local application instance */
  int		i,j,M,N;
  double	theta,phi,Coef;


  lap= (struct application*)malloc(sizeof(struct application));
  RT_APPLICATION_INIT(lap);
  lap -> a_rt_i= ap -> a_rt_i;
  lap -> a_hit= ap -> a_hit;
  lap -> a_miss= ap -> a_miss;
  lap -> a_resource= &GPM_RTAB[pid];

  M= N= GPM_RAYS;
  P -> Irrad[0]= P -> Irrad[1]= P -> Irrad[2]= 0.0;
  for (i= 1; i <= M; i++) {
    for (j= 1; j <= N; j++) {
      theta= asin(sqrt((j-drand48())/M));
      phi= (2*M_PI)*((i-drand48())/N);

      /* Assign pt */
      lap -> a_ray.r_pt[0]= P -> Pos[0];
      lap -> a_ray.r_pt[1]= P -> Pos[1];
      lap -> a_ray.r_pt[2]= P -> Pos[2];

      /* Assign Dir */
      Polar2Euclidian(lap -> a_ray.r_dir,P -> Normal,theta,phi);

      /* Utilize the purpose pointer as a pointer to the Irradiance Color */
      lap -> a_purpose= (void*)P -> Irrad;

/*      bu_log("Vec: [%.3f,%.3f,%.3f]\n",ap -> a_ray.r_dir[0],ap -> a_ray.r_dir[1],ap -> a_ray.r_dir[2]);*/
      rt_shootray(lap);	

/*      bu_log("[%.3f,%.3f,%.3f] [%.3f,%.3f,%.3f] [%.3f,%.3f,%.3f]\n",P.Pos[0],P.Pos[1],P.Pos[2],P.Normal[0],P.Normal[1],P.Normal[2],IMColor[0],IMColor[1],IMColor[2]);*/
    }
  }

  Coef= 1.0/((double)(M*N));
  P -> Irrad[0]*= Coef;
  P -> Irrad[1]*= Coef;
  P -> Irrad[2]*= Coef;
/*  bu_log("val: [%.3f,%.3f,%.3f]\n",P -> Irrad[0],P -> Irrad[1],P -> Irrad[2]);*/

  free(lap);
}


/*
 *  Irradiance Cache for Indirect Illumination
 *  Go through each photon and use it for the position of the hemisphere
 *  and then determine whether that should be included as a Cache Pt.
 */
void BuildIrradianceCache(int pid, struct PNode *Node, struct application *ap) {
  if (!Node)
    return;

  
  /* Determine if this pt will be used by calculating a weight */
  bu_semaphore_acquire(PM_SEM);
  if (!Node -> C) {
    ICSize++;
    Node -> C++;
/*    bu_log("cp:A:%d\n",Node -> C);*/
    if (!(ICSize%(PMap[PM_GLOBAL] -> MaxPhotons/8)))
      bu_log("    Irradiance Cache Progress: %d%%\n",(int)(0.5+100.0*ICSize/PMap[PM_GLOBAL] -> MaxPhotons));
    bu_semaphore_release(PM_SEM);
    Irradiance(pid, &Node -> P, ap);
  } else {
    bu_semaphore_release(PM_SEM);
  }


  BuildIrradianceCache(pid, Node -> L, ap);
  BuildIrradianceCache(pid, Node -> R, ap);
}


void IrradianceThread(int pid, genptr_t arg) {
  BuildIrradianceCache(pid, PMap[PM_GLOBAL] -> Root, (struct application*)arg);
}


/*
 *  Main Photon Mapping Function
 */
void BuildPhotonMap(struct application *ap, int cpus, int width, int height, int Hypersample, int GlobalPhotons, double CausticsPercent, int Rays, double AngularTolerance, int RandomSeed, int IrradianceHypersampling) {
  struct	light_specific	*lp;
  int				i,MapSize[3];


  bu_log("Building Photon Map:\n");

  GPM_IH= IrradianceHypersampling;

  GPM_RAYS= Rays;
  GPM_ATOL= cos(AngularTolerance*bn_degtorad);

  GPM_WIDTH= width;
  GPM_HEIGHT= height;

  PInit= 1;
  srand48(RandomSeed);
/*  bu_log("Photon Structure Size: %d\n",sizeof(struct PNode));*/

/*
  bu_log("Checking application struct\n");
  RT_CK_APPLICATION(ap);
*/

  /* Initialize Emitted Photons for each map to 0 */
  EPM= 0;
  for (i= 0; i < 3; i++)
    EPS[i]= 0;

  CausticsPercent/= 100.0;
  MapSize[PM_GLOBAL]= (int)((1.0-CausticsPercent)*GlobalPhotons);
  MapSize[PM_CAUSTIC]= (int)(CausticsPercent*GlobalPhotons);
  MapSize[2]= 0;

/*  bu_log("Caustic Photons: %d\n",MapSize[PM_CAUSTIC]);*/
  /* Allocate Memory for Photon Maps */
  for (i= 0; i < 3; i++) {
    PMap[i]= (struct PhotonMap*)malloc(sizeof(struct PhotonMap));
    PMap[i] -> MaxPhotons= MapSize[i];
    PMap[i] -> Root= (struct PNode*)malloc(sizeof(struct PNode));
    PMap[i] -> StoredPhotons= 0;
    Emit[i]= (struct Photon*)malloc(sizeof(struct Photon)*MapSize[i]);
  }


  /* Populate Application Structure */
  /* Set Recursion Level, Magic Number, Hit/Miss Callbacks, and Purpose */
  ap -> a_level= 1;
  ap -> a_onehit= 0;
  ap -> a_ray.magic= RT_RAY_MAGIC;
  ap -> a_hit= Hit;
  ap -> a_miss= Miss;
  ap -> a_purpose= "Photon Mapping";


  /* Photons must be shot from all light sources, So just divide
   * the number of photons among all present light sources for now... */
  bu_log("  Emitting Photons...\n");

  for (BU_LIST_FOR(lp, light_specific, &(LightHead.l))) {
    EmitPhotonsRandom(ap, lp);
  }

  /* Generate Scale Factor */
  ScaleFactor= max(BBMax[0]-BBMin[0],BBMax[1]-BBMin[1],BBMax[2]-BBMin[2]);
/*  bu_log("Scale Factor: %.3f\n",ScaleFactor);*/

  /* Scale Photon Power */
  for (i= 0; i < 3; i++)
    if (PMap[i] -> StoredPhotons)
      ScalePhotonPower(1.0/(double)EPS[i],i);


/*
  for (i= 0; i < PMap -> StoredPhotons; i++)
    bu_log("insertLS[%d]: %.3f,%.3f,%.3f\n",i,Emit[i].Pos[0],Emit[i].Pos[1],Emit[i].Pos[2]);
*/


  bu_log("  Building KD-Tree...\n");
  /* Balance KD-Tree */
  for (i= 0; i < 3; i++)
    if (PMap[i] -> StoredPhotons)
      BuildTree(Emit[i],PMap[i] -> StoredPhotons,PMap[i] -> Root);


  bu_semaphore_init(PM_SEM_INIT);
  bu_log("  Building Irradiance Cache...\n");
  ap -> a_level= 1;
  ap -> a_onehit= 0;
  ap -> a_ray.magic= RT_RAY_MAGIC;
  ap -> a_hit= ICHit;
  ap -> a_miss= ICMiss;
  ICSize= 0;

  GPM_RTAB= (struct resource*)malloc(sizeof(struct resource)*cpus);
  bzero(GPM_RTAB,cpus*sizeof(struct resource));
  for (i= 0; i < cpus; i++) {
    GPM_RTAB[i].re_cpu= i;
    GPM_RTAB[i].re_magic= RESOURCE_MAGIC;
    BU_PTBL_SET(&ap -> a_rt_i -> rti_resources, i, &GPM_RTAB[i]);
    rt_init_resource(&GPM_RTAB[i], GPM_RTAB[i].re_cpu, ap -> a_rt_i);
  }
  bu_parallel(IrradianceThread, cpus, ap);


  /* Allocate Memory for Irradiance Cache and Initialize Pixel Map */
/*  bu_log("Image Size: %d,%d\n",width,height);*/
  if (GPM_IH) {
    Map= (char*)malloc(sizeof(char)*width*height);
    for (i= 0; i < width*height; i++)
      Map[i]= 0;
    IC= (struct IrradCache*)malloc(sizeof(struct IrradCache)*width*height);
    for (i= 0; i < width*height; i++) {
      IC[i].List= (struct IrradNode*)malloc(sizeof(struct IrradNode));
      IC[i].Num= 0;
    }
  }


/*
  bu_log("  Sanity Check...\n");
  SanityCheck(PMap[PM_GLOBAL] -> Root,0);
*/


  for (i= 0; i < 3; i++)
    if (PMap[i] -> StoredPhotons)
      bu_log("  Results:  Map: %d, Total Emitted: %d, Local Emitted: %d, Map Size: %d\n",i, EPM, EPS[i], PMap[i] -> MaxPhotons);


  for (i= 0; i < 3; i++)
    free(Emit[i]);
  free(GPM_RTAB);
}


void LocatePhotons(struct NearestPhotons *NP, struct PNode *Root) {
  fastf_t	Dist,MDist,TDist,angle;
  int		i,MaxInd,Axis;

  if (!Root)
    return;

  Axis= Root -> P.Axis;
  Dist= NP -> Pos[Axis] - Root -> P.Pos[Axis];

  if (Dist < 0) {
    /* Left of plane - search left subtree first */
    LocatePhotons(NP,Root -> L);
    if (Dist*Dist < NP -> RadSq)
      LocatePhotons(NP,Root -> R);
  } else {
    /* Right of plane - search right subtree first */
    LocatePhotons(NP,Root -> R);
    if (Dist*Dist < NP -> RadSq)
      LocatePhotons(NP,Root -> L);
  }

  /* Find Distance between Root Photon and NP -> Pos */
  Dist= (Root -> P.Pos[0] - NP -> Pos[0])*(Root -> P.Pos[0] - NP -> Pos[0]) + (Root -> P.Pos[1] - NP -> Pos[1])*(Root -> P.Pos[1] - NP -> Pos[1]) + (Root -> P.Pos[2] - NP -> Pos[2])*(Root -> P.Pos[2] - NP -> Pos[2]);

  angle= VDOT(NP -> Normal, Root -> P.Normal);
  if (Dist < NP -> RadSq && angle > GPM_ATOL) { /* Check that Result is within Radius and Angular Tolerance */
/*  if (Dist < NP -> RadSq) {*/
    if (NP -> Found < NP -> Max) {
      NP -> List[NP -> Found++]= Root -> P;
    } else {
      MDist= (NP -> Pos[0] - NP -> List[0].Pos[0])*(NP -> Pos[0] - NP -> List[0].Pos[0])+(NP -> Pos[1] - NP -> List[0].Pos[1])*(NP -> Pos[1] - NP -> List[0].Pos[1])+(NP -> Pos[2] - NP -> List[0].Pos[2])*(NP -> Pos[2] - NP -> List[0].Pos[2]);
      MaxInd= 0;
      for (i= 1; i < NP -> Found; i++) {
        TDist= (NP -> Pos[0] - NP -> List[i].Pos[0])*(NP -> Pos[0] - NP -> List[i].Pos[0])+(NP -> Pos[1] - NP -> List[i].Pos[1])*(NP -> Pos[1] - NP -> List[i].Pos[1])+(NP -> Pos[2] - NP -> List[i].Pos[2])*(NP -> Pos[2] - NP -> List[i].Pos[2]);
        if (TDist > MDist) {
          MDist= TDist;
          MaxInd= i;
        }
      }

      if (Dist < MDist)
        NP -> List[MaxInd]= Root -> P;
    }
  }
}


fastf_t Dist(point_t a, point_t b) {
  return (a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]);
}


fastf_t GaussFilter(fastf_t dist, fastf_t rad) {
  return( 0.918 * (1.0 - (1.0 - exp(-1.953*dist*dist/(2.0*rad*rad)))/(1.0 - exp(-1.953))) );
}


fastf_t ConeFilter(fastf_t dist, fastf_t rad) {
  return( 1.0 - dist/rad );
}


void IrradianceEstimate(struct application *ap, vect_t irrad, point_t pos, vect_t normal, fastf_t rad, int np) {
  struct	NearestPhotons	NP;
  int				i,index;
  fastf_t			tmp,dist,Filter,TotDist;
  vect_t			t,cirrad;


  if (GPM_IH) {
    index= ap -> a_x + ap -> a_y*GPM_WIDTH;
    /* See if there is a cached irradiance calculation for this point */
    for (i= 0; i < IC[index].Num; i++) {
      dist= (pos[0]-IC[index].List[i].Pos[0])*(pos[0]-IC[index].List[i].Pos[0])+(pos[1]-IC[index].List[i].Pos[1])*(pos[1]-IC[index].List[i].Pos[1])+(pos[2]-IC[index].List[i].Pos[2])*(pos[2]-IC[index].List[i].Pos[2]);
      if (dist < (ScaleFactor/100.0)*(ScaleFactor*100.0)) {
        irrad[0]= IC[index].List[i].RGB[0];
        irrad[1]= IC[index].List[i].RGB[1];
        irrad[2]= IC[index].List[i].RGB[2];
        return;
      }
    }

    /* There is no precomputed irradiance for this point, allocate space
       for a new one if neccessary. */
    if (IC[index].Num)
      IC[index].List= (struct IrradNode*)realloc(IC[index].List,sizeof(struct IrradNode)*(IC[index].Num+1));
  }

  NP.Pos[0]= pos[0];
  NP.Pos[1]= pos[1];
  NP.Pos[2]= pos[2];

  NP.Found= 0;
/*  NP.RadSq= (ScaleFactor/(10.0*pow(2,(log(PMap[PM_GLOBAL] -> MaxPhotons/2)/log(4))))) * (ScaleFactor/(10.0*pow(2,(log(PMap[PM_GLOBAL] -> MaxPhotons/2)/log(4)))));*/
/*  bu_log("SF: %.3f\n",(ScaleFactor/(10.0*pow(2,(log(PMap[PM_GLOBAL] -> MaxPhotons/2)/log(4))))));*/
/*  bu_log("SF: %.3f\n",ScaleFactor/pow(PMap[PM_GLOBAL] -> MaxPhotons,0.5));*/

/*
  NP.RadSq= 0.5*ScaleFactor/pow(PMap[PM_GLOBAL] -> MaxPhotons,0.5) * ScaleFactor/pow(PMap[PM_GLOBAL] -> MaxPhotons,0.5);
  NP.Max= pow(PMap[PM_GLOBAL] -> StoredPhotons, 0.5);
*/

  NP.RadSq= (ScaleFactor/1000.0) * (ScaleFactor/1000.0);
/*  NP.RadSq= (4.0*ScaleFactor/PMap[PM_GLOBAL] -> MaxPhotons) * (4.0*ScaleFactor/PMap[PM_GLOBAL] -> MaxPhotons);*/
/*  NP.Max= 2.0*pow(PMap[PM_GLOBAL] -> StoredPhotons, 0.5);*/
  NP.Max= PMap[PM_GLOBAL] -> StoredPhotons / 50;

  NP.Normal[0]= normal[0];
  NP.Normal[1]= normal[1];
  NP.Normal[2]= normal[2];

  NP.List= (struct Photon*)malloc(sizeof(struct Photon)*NP.Max);
  i= 0;
  while (NP.Found < NP.Max && i < 8) {
    NP.Found= 0;
    LocatePhotons(&NP,PMap[PM_GLOBAL] -> Root);
    NP.RadSq*= 4;
    i++;
  }

  irrad[0]= irrad[1]= irrad[2]= 0;
  TotDist= 0;
  for (i= 0; i < NP.Found; i++) {
    t[0]= NP.List[i].Pos[0] - pos[0];
    t[1]= NP.List[i].Pos[1] - pos[1];
    t[2]= NP.List[i].Pos[2] - pos[2];
    TotDist+= t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
  }


  for (i= 0; i < NP.Found; i++) {
    t[0]= NP.List[i].Pos[0] - pos[0];
    t[1]= NP.List[i].Pos[1] - pos[1];
    t[2]= NP.List[i].Pos[2] - pos[2];

    t[0]= (t[0]*t[0] + t[1]*t[1] + t[2]*t[2])/TotDist;
    irrad[0]+= NP.List[i].Irrad[0] * t[0];
    irrad[1]+= NP.List[i].Irrad[1] * t[0];
    irrad[2]+= NP.List[i].Irrad[2] * t[0];
  }
  free(NP.List);

/*  GetEstimate(cirrad, pos, normal, (int)(ScaleFactor/100.0),PMap[PM_CAUSTIC] -> MaxPhotons/50,PM_CAUSTIC,1, 0);*/
  GetEstimate(cirrad,pos,normal,(int)(ScaleFactor/pow(2,(log(PMap[PM_CAUSTIC] -> MaxPhotons/2)/log(4)))),PMap[PM_CAUSTIC] -> MaxPhotons / 50,PM_CAUSTIC,1,0);

  irrad[0]+= cirrad[0];
  irrad[1]+= cirrad[1];
  irrad[2]+= cirrad[2];

  if (GPM_IH) {
    /* Store Irradiance */
    IC[index].List[IC[index].Num].RGB[0]= irrad[0];
    IC[index].List[IC[index].Num].RGB[1]= irrad[1];
    IC[index].List[IC[index].Num].RGB[2]= irrad[2];

    IC[index].List[IC[index].Num].Pos[0]= pos[0];
    IC[index].List[IC[index].Num].Pos[1]= pos[1];
    IC[index].List[IC[index].Num].Pos[2]= pos[2];

    IC[index].Num++;
  }

/*
  GetEstimate(cirrad,pos,normal,350,250,PM_GLOBAL,1);

  irrad[0]= cirrad[0];
  irrad[1]= cirrad[1];
  irrad[2]= cirrad[2];
*/
/*
  if (irrad[0] < 0.05 && irrad[1] < 0.05 && irrad[2] < 0.05)
    bu_log("Wi: %.3f,%.3f, [%.3f,%.3f,%.3f]\n",Wi,wgttotal,irrad[0],irrad[1],irrad[2]);
*/
}


void GetEstimate(vect_t irrad, point_t pos, vect_t normal, fastf_t rad, int np, int map, int iter, int centog) {
  struct	NearestPhotons	NP;
  int				i;
  fastf_t			tmp,dist,Filter;
  vect_t			t,Centroid;


  irrad[0]= irrad[1]= irrad[2]= 0;
  if (!PMap[map] -> StoredPhotons) return;

  NP.Pos[0]= pos[0];
  NP.Pos[1]= pos[1];
  NP.Pos[2]= pos[2];

  NP.RadSq= rad*rad;
  NP.Max= np < 15 ? 15 : np;
  NP.Found= 0;
  NP.Normal[0]= normal[0];
  NP.Normal[1]= normal[1];
  NP.Normal[2]= normal[2];

  NP.List= (struct Photon*)malloc(sizeof(struct Photon)*NP.Max);
  LocatePhotons(&NP,PMap[map] -> Root);
  i= 0;
  while (NP.Found < 10 && i < iter) {
    NP.Found= 0;
    LocatePhotons(&NP,PMap[map] -> Root);
    NP.RadSq*= 4.0;
    i++;
  }

  if (NP.Found < 5) {
    free(NP.List);
    return;
  }


  /* Calculate Max Distance */
  NP.RadSq= 1;
  Centroid[0]= Centroid[1]= Centroid[2]= 0;

  for (i= 0; i < NP.Found; i++) {
    t[0]= NP.List[i].Pos[0] - pos[0];
    t[1]= NP.List[i].Pos[1] - pos[1];
    t[2]= NP.List[i].Pos[2] - pos[2];

    Centroid[0]+= NP.List[i].Pos[0];
    Centroid[1]+= NP.List[i].Pos[1];
    Centroid[2]+= NP.List[i].Pos[2];

    dist= t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
    if (dist > NP.RadSq)
      NP.RadSq= dist;
  }

  if (NP.Found) {
    Centroid[0]/= (double)NP.Found;
    Centroid[1]/= (double)NP.Found;
    Centroid[2]/= (double)NP.Found;
  }


  for (i= 0; i < NP.Found; i++) {
      t[0]= NP.List[i].Pos[0] - pos[0];
      t[1]= NP.List[i].Pos[1] - pos[1];
      t[2]= NP.List[i].Pos[2] - pos[2];

      dist= t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
      Filter= 0.75;
/*      Filter= ConeFilter(dist,NP.RadSq);*/
/*      Filter= GaussFilter(dist,NP.RadSq);*/

      irrad[0]+= NP.List[i].Power[0]*Filter;
      irrad[1]+= NP.List[i].Power[1]*Filter;
      irrad[2]+= NP.List[i].Power[2]*Filter;
  }

  if (!centog) {
    Centroid[0]= pos[0];
    Centroid[1]= pos[1];
    Centroid[2]= pos[2];
  }

  tmp= M_PI*NP.RadSq;
  t[0]= sqrt((Centroid[0] - pos[0])*(Centroid[0] - pos[0])+(Centroid[1] - pos[1])*(Centroid[1] - pos[1])+(Centroid[2] - pos[2])*(Centroid[2] - pos[2]));
  tmp= M_PI*(sqrt(NP.RadSq)-t[0])*(sqrt(NP.RadSq)-t[0]);


  irrad[0]/= tmp;
  irrad[1]/= tmp;
  irrad[2]/= tmp;


  if (map == PM_CAUSTIC) {
    tmp= (double)NP.Found/(double)NP.Max;
    irrad[0]*= tmp;
    irrad[1]*= tmp;
    irrad[2]*= tmp;
  }

/*
  irrad[0]*= (1.0/M_PI)/NP.RadSq;
  irrad[1]*= (1.0/M_PI)/NP.RadSq;
  irrad[2]*= (1.0/M_PI)/NP.RadSq;
*/
  free(NP.List);
/*  bu_log("Radius: %.3f, Max Phot: %d, Found: %d, Power: [%.4f,%.4f,%.4f], Pos: [%.3f,%.3f,%.3f]\n",sqrt(NP.RadSq), NP.Max,NP.Found,irrad[0],irrad[1],irrad[2],pos[0],pos[1],pos[2]);*/
}
