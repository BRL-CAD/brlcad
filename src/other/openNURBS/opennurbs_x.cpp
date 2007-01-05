/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2005 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

ON_X_EVENT::ON_X_EVENT()
{
  memset(this,0,sizeof(*this));
}

int ON_X_EVENT::Compare( const ON_X_EVENT* a, const ON_X_EVENT* b) 
{
  if ( !a )
  {
    return b ? 1 : 0;
  }

  if  (!b)
    return -1;

	if( a->m_a[0] < b->m_a[0])
		return -1;
	
  if( a->m_a[0] > b->m_a[0])
		return 1;

	if( a->m_a[1] < b->m_a[1])
		return -1;
	
  if( a->m_a[1] > b->m_a[1])
		return 1;

	return 0;
}

static void DumpDistanceABHelper( ON_TextLog& text_log, ON_3dPoint A, ON_3dPoint B )
{
  const double tinyd = 1.0e-14;
  double d = A.DistanceTo(B);
  text_log.Print("distance A to B");
  if ( !ON_IsValid(d) || d >= 1.0e-14 || d <= 0.0 )
  {
    text_log.Print(" = ");
  }
  else
  {
    // This prevents diff from compaining about tiny changes.
    text_log.Print(" < ");
    d = tinyd;
  }
  text_log.Print(d);
  text_log.Print("\n");
}

static void DumpDistanceABHelper( ON_TextLog& text_log, ON_3dPoint A0, ON_3dPoint B0, ON_3dPoint A1, ON_3dPoint B1 )
{
  if ( A0.DistanceTo(B0) <= A1.DistanceTo(B0) )
    DumpDistanceABHelper( text_log, A0, B0 );
  else
    DumpDistanceABHelper( text_log, A1, B1 );
}

void ON_X_EVENT::Dump(ON_TextLog& text_log) const
{
  int j;

  text_log.Print("m_type: ");
  switch( m_type )
  {
  case ON_X_EVENT::no_x_event:
    text_log.Print("no_x_event");
    break;
  case ON_X_EVENT::ccx_point:
    text_log.Print("ccx_point");
    break;
  case ON_X_EVENT::ccx_overlap:
    text_log.Print("ccx_overlap");
    break;
  case ON_X_EVENT::csx_point:
    text_log.Print("csx_point");
    break;
  case ON_X_EVENT::csx_overlap:
    text_log.Print("csx_overlap");
    break;
  default:
    text_log.Print("illegal value");
    break;
  }
  text_log.Print("\n");
  text_log.PushIndent();

  switch( m_type )
  {
  case ON_X_EVENT::ccx_point:
  case ON_X_EVENT::csx_point:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("curveA(");
    text_log.Print(m_a[0]);
    text_log.Print(") = \n");
    text_log.PushIndent();
    text_log.Print(m_A[0]);
    text_log.Print("\n");
    text_log.PopIndent();
    break;

  case ON_X_EVENT::ccx_overlap:
  case ON_X_EVENT::csx_overlap:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("curveA(");
    text_log.Print(m_a[0]);
    text_log.Print(" to ");
    text_log.Print(m_a[1]);
    text_log.Print(") = \n");
    text_log.PushIndent();
    text_log.Print(m_A[0]);
    text_log.Print(" to ");
    text_log.Print(m_A[1]);
    text_log.Print("\n");
    text_log.PopIndent();
    break;

  case ON_X_EVENT::no_x_event:
    break;
  }

  switch( m_type )
  {
  case ON_X_EVENT::ccx_point:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("curveB(");
    text_log.Print(m_b[0]);
    text_log.Print(") = \n");
    text_log.PushIndent();
    text_log.Print(m_B[0]);
    text_log.Print("\n");
    text_log.PopIndent();
    DumpDistanceABHelper(text_log,m_A[0],m_B[0]);
    break;

  case ON_X_EVENT::csx_point:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("surfaceB");
    text_log.Print(ON_2dPoint(m_b[0],m_b[1]));
    text_log.Print(" = \n");
    text_log.PushIndent();
    text_log.Print(m_B[0]);
    text_log.Print("\n");
    text_log.PopIndent();
    DumpDistanceABHelper(text_log,m_A[0],m_B[0]);
    break;

  case ON_X_EVENT::ccx_overlap:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("curveB(");
    text_log.Print(m_b[0]);
    text_log.Print(" to ");
    text_log.Print(m_b[1]);
    text_log.Print(") = \n");
    text_log.PushIndent();
    text_log.Print(m_B[0]);
    text_log.Print(" to ");
    text_log.Print(m_B[1]);
    text_log.Print("\n");
    text_log.PopIndent();
    DumpDistanceABHelper(text_log,m_A[0],m_B[0],m_A[1],m_B[1]);
    break;

  case ON_X_EVENT::csx_overlap:
    // don't use %g so the text_log double format can control display precision
    text_log.Print("surface(");
    text_log.Print(ON_2dPoint(m_b[0],m_b[1]));
    text_log.Print(" to ");
    text_log.Print(ON_2dPoint(m_b[2],m_b[3]));
    text_log.Print(") =  \n");
    text_log.PushIndent();
    text_log.Print(m_B[0]);
    text_log.Print(" to ");
    text_log.Print(m_B[1]);
    text_log.Print("\n");
    text_log.PopIndent();
    DumpDistanceABHelper(text_log,m_A[0],m_B[0],m_A[1],m_B[1]);
    break;

  case ON_X_EVENT::no_x_event:
    break;
  }

  text_log.Print("m_dirA[] = (");
  for ( j = 0; j < 2; j++ )
  {
    if (j)
    {
      text_log.Print(", ");
    }
    switch(m_dirA[j])
    {
    case ON_X_EVENT::no_x_dir:
      text_log.Print("no_x_dir");
      break;
    case ON_X_EVENT::at_end_dir:
      text_log.Print("at_end_dir");
      break;
    case ON_X_EVENT::from_above_dir:
      text_log.Print("from_above_dir");
      break;
    case ON_X_EVENT::from_below_dir:
      text_log.Print("from_below_dir");
      break;
    case ON_X_EVENT::from_on_dir:
      text_log.Print("from_on_dir");
      break;
    case ON_X_EVENT::to_above_dir:
      text_log.Print("to_above_dir");
      break;
    case ON_X_EVENT::to_below_dir:
      text_log.Print("to_below_dir");
      break;
    case ON_X_EVENT::to_on_dir:
      text_log.Print("to_on_dir");
      break;
    default:
      text_log.Print("illegal value");
      break;
    }
  }
  text_log.Print(")\n");

#if defined(OPENNURBS_PLUS_INC_)
  switch( m_type )
  {
  case ON_X_EVENT::ccx_point:
  case ON_X_EVENT::ccx_overlap:
    text_log.Print("cnodeA sn = %d,%d  ",m_cnodeA[0]?m_cnodeA[0]->m_nodesn:-1,m_cnodeA[1]?m_cnodeA[1]->m_nodesn:-1);
    text_log.Print("cnodeB sn = %d,%d\n",m_cnodeB[0]?m_cnodeB[0]->m_nodesn:-1,m_cnodeB[1]?m_cnodeB[1]->m_nodesn:-1);
    break;

  case ON_X_EVENT::csx_point:
  case ON_X_EVENT::csx_overlap:
    text_log.Print("cnodeA sn = %d,%d  ",m_cnodeA[0]?m_cnodeA[0]->m_nodesn:-1,m_cnodeA[1]?m_cnodeA[1]->m_nodesn:-1);
    text_log.Print("snodeB sn = %d,%d\n",m_snodeB[0]?m_snodeB[0]->m_nodesn:-1,m_snodeB[1]?m_snodeB[1]->m_nodesn:-1);
    break;
  }
#endif

  text_log.PopIndent();
}


double ON_X_EVENT::IntersectionTolerance( double intersection_tolerance )
{
  if ( intersection_tolerance <= 0.0 || !ON_IsValid(intersection_tolerance) )
  {
    intersection_tolerance = 0.001;
  }
  else if ( intersection_tolerance < 1.0e-6 )
  {
    intersection_tolerance = 1.0e-6;
  }
  return intersection_tolerance;
}

double ON_X_EVENT::OverlapTolerance( double intersection_tolerance, double overlap_tolerance )
{
  if ( overlap_tolerance <= 0.0 || !ON_IsValid(overlap_tolerance) )
  {
    overlap_tolerance = 2.0*IntersectionTolerance(intersection_tolerance);
  }
  else if ( overlap_tolerance < 1.0e-6 )
  {
    overlap_tolerance = 1.0e-6;
  }
  return overlap_tolerance;
}

bool ON_X_EVENT::IsPointEvent() const
{
  return (ccx_point == m_type || csx_point == m_type);
}

bool ON_X_EVENT::IsOverlapEvent() const
{
  return (ccx_overlap == m_type || csx_overlap == m_type);
}

bool ON_X_EVENT::IsCCXEvent() const
{
  return (ccx_point == m_type || ccx_overlap == m_type);
}

bool ON_X_EVENT::IsCSXEvent() const
{
  return (csx_point == m_type || csx_overlap == m_type);
}
