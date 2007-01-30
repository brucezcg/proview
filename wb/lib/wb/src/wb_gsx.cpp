/* 
 * Proview   $Id: wb_gsx.cpp,v 1.1 2007-01-04 07:29:03 claes Exp $
 * Copyright (C) 2005 SSAB Oxel�sund AB.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with the program, if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/

/* wb_gsx.c -- syntax control
   Gre syntax control.
   This module control the syntax for conections and nets.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pwr.h"
#include "pwr_baseclasses.h"
#include "pwr_nmpsclasses.h"
#include "flow_ctx.h"
#include "flow_api.h"
#include "wb_ldh.h"
#include "wb_foe_msg.h"
#include "wb_vldh_msg.h"
#include "wb_vldh.h"
#include "wb_goen.h"
#include "wb_goec.h"
#include "wb_gre.h"
#include "wb_foe.h"
#include "wb_gsx.h"
#include "wb_gcg.h"

/*_define _______________________________________________________*/

#define GRAFCET_CONN  pwr_cClass_ConGrafcet
#define FLOAT_CONN  pwr_cClass_ConAnalog
#define BOOLEAN_CONN pwr_cClass_ConDigital
#define INT32_CONN  pwr_cClass_ConAnalog
#define DATA_CONN  pwr_cClass_ConData
#define DEFAULT_CONN pwr_cClass_ConAnalog
#define STEPDIV_CONN pwr_cClass_StepDiv
#define STEPCONV_CONN pwr_cClass_StepConv
#define TRANSDIV_CONN pwr_cClass_TransDiv
#define TRANSCONV_CONN pwr_cClass_TransConv

/*_Local procedues_______________________________________________________*/

/*_Backcalls for the controlled gredit module______________________________*/


/*_Methods defined for this module_______________________________________*/

/*************************************************************************
*
* Name:		gsx_check_connecton()
*
* Type		int
*
* Type		Parameter	IOGF	Description
* vldh_t_node	sourceobject	I	vldh source node.
* unsigned long	sourcepoint	I	connectionpoint on source node.
* vldh_t_node	destobject	I	vldh destination node.
* unsigned long	destpoint	I	connectionspoint on destination node.
* unsigned long	*conclass	O	class of chosen connection.
*
* Description:
*	Check that a connection is allowed and returns connectiontype.
*	So far this routine does only return the connection class
*	and makes no syntax control.
*	There are three types of classes returned: float, logic and grafcet.
*	
* Modif : SG 08.03.91 determine the connection_type .
**************************************************************************/

int	gsx_check_connection( 
	WFoe		*foe,
	vldh_t_node	sourceobject,
	unsigned long	sourcepoint,
	vldh_t_node	destobject,
	unsigned long	destpoint,
	pwr_tClassId	*conclass,
	pwr_tClassId	user_conclass
)
{
	vldh_t_node		dummyobject;
	goen_conpoint_type	graph_pointer;
	unsigned long		par_type;
	unsigned long		par_inverted;
	unsigned long		source_par_index;
	unsigned long		dest_par_index;
	unsigned long		dummy_par_index;
	ldh_sParDef 		*bodydef;
	int			rows, sts, size;
        pwr_eType		source_type;
        pwr_eType		dest_type;
	pwr_tUInt32		source_pointer_flag;
	pwr_tUInt32		dest_pointer_flag;
	ldh_tSesContext		ldhses;
	pwr_tClassId		dest_class;
	pwr_tClassId		source_class;
	pwr_tClassId		dummyclass;
	pwr_tClassId		bodyclass;
	pwr_sGraphPlcConnection *graphbody;

	ldhses = (sourceobject->hn.wind)->hw.ldhses; 

	if ( user_conclass != 0)
	{
	  /* Get graphbody for the class */
	  sts = ldh_GetClassBody(ldhses, user_conclass, "GraphPlcCon", 
		&bodyclass, (char **)&graphbody, &size);
	  if ( EVEN(sts) ) return sts;

	  if ( !(graphbody->attributes & GOEN_CON_SIGNAL))
	  {
	    /* This is not a signal transfering connection, no syntax... */
	    return GSX__SUCCESS;
	  }
	}

	/* Check that the points datatype correspond */
 
	/* Get parameter info */
	sts = goen_get_parinfo( foe->gre, sourceobject->ln.cid, 
			ldhses,
			sourceobject->ln.mask, 
			strlen( sourceobject->hn.name),
			sourcepoint, 
			&graph_pointer,	&par_inverted, &par_type, 
			&source_par_index, sourceobject);
	if ( EVEN(sts) ) return( sts);
	sts = goen_get_parinfo( foe->gre, destobject->ln.cid, 
			(destobject->hn.wind)->hw.ldhses, 
			destobject->ln.mask, 
			strlen( destobject->hn.name),
			destpoint, 
			&graph_pointer,	&par_inverted, &par_type, 
			&dest_par_index, destobject);
	if ( EVEN(sts) ) return( sts);

	source_class = sourceobject->ln.cid;
	dest_class = destobject->ln.cid;

        /* 
	SG 08.03.91 
	Determine the type of connection to use in function of 
        the type of parameter
        */

	/* If one class is a point let the other determine the contype */
	if ( source_class == pwr_cClass_Point ||
	     source_class == pwr_cClass_Backup)
	{
	  dummyclass = dest_class;
	  dest_class = source_class;	
	  source_class = dummyclass;	
	  dummyobject = destobject;
	  destobject = sourceobject;
	  sourceobject = dummyobject;
	  dummy_par_index = dest_par_index;
	  dest_par_index = source_par_index;
	  source_par_index = dummy_par_index;
	}

	/* Grafcet, if both objects is of grafcet type, connections should
	  be GRAFCET_CONN */

	if ( (source_class == pwr_cClass_order && dest_class == pwr_cClass_trans) ||	
	     (dest_class == pwr_cClass_order && source_class == pwr_cClass_trans) )
	{
	  /* Trans and Order -> Logic connection */
	  *conclass = BOOLEAN_CONN;
	  return GSX__SUCCESS;
	}
	else if ( (
	  (source_class == pwr_cClass_order) ||	
	  (source_class == pwr_cClass_trans) ||	
	  (source_class == pwr_cClass_step) ||	
	  (source_class == pwr_cClass_initstep) ||	
	  (source_class == pwr_cClass_ssbegin) ||	
	  (source_class == pwr_cClass_ssend) ||	
	  (source_class == pwr_cClass_substep) 	
	  	) && (
	  (dest_class == pwr_cClass_order) ||	
	  (dest_class == pwr_cClass_trans) ||	
	  (dest_class == pwr_cClass_step) ||	
	  (dest_class == pwr_cClass_initstep) ||	
	  (dest_class == pwr_cClass_ssbegin) ||	
	  (dest_class == pwr_cClass_ssend) ||	
	  (dest_class == pwr_cClass_substep)  )) 
	{
	  vldh_t_conpoint	*pointlist;
	  unsigned long		point_count;
	  vldh_t_node		next_node;
	  unsigned long		next_point;
	  vldh_t_node		trans_object, other_object;
	  unsigned long		trans_point, other_point;
	  pwr_tClassId		other_class;
	  int			transcount, stepcount;
	  int			k;
	  pwr_tClassId		cid;


	  if ((dest_class == pwr_cClass_trans) ||
	      (source_class == pwr_cClass_trans))
	  {
	    if (dest_class == pwr_cClass_trans)
	    {
	      trans_object = destobject;
	      trans_point = destpoint;	     
	      other_object = sourceobject;
	      other_point = sourcepoint;
	      other_class = source_class;
	    }
	    else
	    {
	      trans_object = sourceobject;
	      trans_point = sourcepoint;	     
	      other_object = destobject;
	      other_point = destpoint;
	      other_class = dest_class;
	    }

	    if ( trans_point == 0)
	    {
	      if ((other_class == pwr_cClass_step) ||	
	          (other_class == pwr_cClass_initstep) ||	
	          (other_class == pwr_cClass_ssbegin) ||	
	          (other_class == pwr_cClass_ssend) ||	
	          (other_class == pwr_cClass_substep))
	      {
	        /* Check if there is more steps connected to the step */
	        stepcount = 0;
	        transcount = 0;
	        gcg_get_conpoint_nodes( other_object, other_point, 
			&point_count, &pointlist,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	        if ( point_count <= 1 )
	        {
	          /* Check if there is more steps connected to the trans */
	          gcg_get_conpoint_nodes( trans_object, trans_point, 
			&point_count, &pointlist,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	        }
	        if ( point_count > 1 )
	        {
	          for ( k = 1; k < (int)point_count; k++)
	          {
	            next_node = (pointlist + k)->node;
	            next_point = (pointlist + k)->conpoint;
	            /* Check class of connected nodes */
	            sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	            if (EVEN(sts)) return sts;

	            if ( 
		      ((cid == pwr_cClass_step) &&
			( next_point == 2 )) ||
		      ((cid == pwr_cClass_initstep) && 
			( next_point == 2 )) ||
		      ((cid == pwr_cClass_substep) && 
			( next_point == 2 )) ||
		      ((cid == pwr_cClass_ssbegin) && 
			( next_point == 1 )) )
	            {
	              stepcount++;
	            }
	            else if (cid == pwr_cClass_trans )
	            {
	              transcount++;
	            }
	          }
	        }
	      }
	      if ( stepcount > 0)
	        *conclass = TRANSCONV_CONN;
	      else if ( transcount > 0)
	        *conclass = STEPDIV_CONN;
	      else
	        *conclass = GRAFCET_CONN;
	    }
	    else if ( trans_point == 2)
	    {
	      if ((other_class == pwr_cClass_step) ||	
	          (other_class == pwr_cClass_initstep) ||	
	          (other_class == pwr_cClass_ssbegin) ||	
	          (other_class == pwr_cClass_ssend) ||	
	          (other_class == pwr_cClass_substep))
	      {
	        /* Check if there is more steps connected to the trans */
	        stepcount = 0;
	        transcount = 0;
	        gcg_get_conpoint_nodes( trans_object, trans_point, 
			&point_count, &pointlist,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	        if ( point_count <= 1 )
	        {
	          if ( point_count > 0)
 	            free((char *) pointlist);
	          /* Check if there is more steps connected to the step */
	          gcg_get_conpoint_nodes( other_object, other_point, 
			&point_count, &pointlist,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	        }
	        if ( point_count > 1 )
	        {
	          for ( k = 1; k < (int)point_count; k++)
	          {
	            next_node = (pointlist + k)->node;
	            next_point = (pointlist + k)->conpoint;
	            /* Check class of connected nodes */
	            sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	            if (EVEN(sts)) return sts;

	            if ( 
		      ((cid == pwr_cClass_step) &&
			( next_point == 0 )) ||
		      ((cid == pwr_cClass_initstep) && 
			( next_point == 0 )) ||
		      ((cid == pwr_cClass_substep) && 
			( next_point == 0 )) ||
		      ((cid == pwr_cClass_ssbegin) && 
			( next_point == 0 )) )
	            {
	              stepcount++;
	            }
	            else if (cid == pwr_cClass_trans)
	            {
	              transcount++;
	            }
	          }
	        }
	        if ( point_count > 0)
 	          free((char *) pointlist);
	      }
	      if ( stepcount > 0)
	        *conclass = TRANSDIV_CONN;
	      else if ( transcount > 0)
	        *conclass = STEPCONV_CONN;
	      else
	        *conclass = GRAFCET_CONN;
	    }
	    else 
	      *conclass = GRAFCET_CONN;

	    /* Check that all connections are of the same class */
            if ( *conclass != GRAFCET_CONN)
	    {
	      vldh_t_con 	*conlist;
	      vldh_t_con 	con;
	      unsigned long	con_count;
	      int		i;
	      vldh_t_node	src, dest;
	      unsigned long	dpoint, spoint;
	
	      sts = vldh_get_conpoint_cons( destobject, destpoint, &con_count,
			&conlist);
	      if ( EVEN(sts)) return sts;
	      if ( con_count)
	      {
	        con = *conlist;
	        for ( i = 0; i < (int)con_count; i++)
	        {
	          con = *(conlist + i);
	          if ( con->lc.cid != *conclass)
	          {
	            /* Exchange this connection */
	            src = con->hc.source_node;
	            dest = con->hc.dest_node;
	            spoint = con->lc.source_point;
	            dpoint = con->lc.dest_point;
	            goec_con_delete( foe->gre, con);
	            vldh_con_delete(con);
	            sts = foe->gre->create_con( *conclass, 
			src, spoint, dest, dpoint, foe->con_drawtype);
	          }
	        }
	        free( (char *)conlist);
	      }

	      sts = vldh_get_conpoint_cons( sourceobject, sourcepoint, 
			&con_count, &conlist);
	      if ( EVEN(sts)) return sts;
	      if ( con_count)
	      {
	        con = *conlist;
	        for ( i = 0; i < (int)con_count; i++)
	        {
	          con = *(conlist + i);
	          if ( con->lc.cid != *conclass)
	          {
	            /* Exchange this connection */
	            src = con->hc.source_node;
	            dest = con->hc.dest_node;
	            spoint = con->lc.source_point;
	            dpoint = con->lc.dest_point;
	            goec_con_delete( foe->gre, con);
	            vldh_con_delete(con);
	            sts = foe->gre->create_con( *conclass, 
			src, spoint, dest, dpoint, foe->con_drawtype);
	          }
	        }
	        free( (char *)conlist);
	      }
	    }
	  }
	  else
	    *conclass = GRAFCET_CONN;
	  return GSX__SUCCESS;
	}
	
        /* Get the type of the source attribute */
	sts = ldh_GetObjectBodyDef (
			ldhses,
			sourceobject->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  /* This is a development object */
	  sts = ldh_GetObjectBodyDef (
			ldhses,
			sourceobject->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) return sts;
	}

        /* Determine the type of connection */        
        
	switch (bodydef[source_par_index].ParClass )  {
        case pwr_eClass_Input:
          source_type = bodydef[source_par_index].Par->Input.Info.Type ;
          source_pointer_flag = PWR_MASK_POINTER & 
		bodydef[source_par_index].Par->Input.Info.Flags;
	  break;
        case pwr_eClass_Output:
          source_type = bodydef[source_par_index].Par->Output.Info.Type ;
          source_pointer_flag = PWR_MASK_POINTER & 
		bodydef[source_par_index].Par->Output.Info.Flags;
	  break;
        default: 
          printf ("gsx_check_connection ... error ParClass not tested ");
          return 0;
        };

        /* Get the type of the destination attribute */
	sts = ldh_GetObjectBodyDef (
			ldhses,
			destobject->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  /* This is a development object */
	  sts = ldh_GetObjectBodyDef (
			ldhses,
			destobject->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) return sts;
	}

        /* Determine the type of connection */        
        
	switch (bodydef[dest_par_index].ParClass )  {
        case pwr_eClass_Input:
          dest_type = bodydef[dest_par_index].Par->Input.Info.Type ;
          dest_pointer_flag = PWR_MASK_POINTER & 
		bodydef[dest_par_index].Par->Input.Info.Flags;
	  break;
        case pwr_eClass_Output:
          dest_type = bodydef[dest_par_index].Par->Output.Info.Type ;
          dest_pointer_flag = PWR_MASK_POINTER & 
		bodydef[dest_par_index].Par->Output.Info.Flags;
	  break;
        default: 
          printf ("gsx_check_connection ... error ParClass not tested ");
          return 0;
        };
        free ((char *) bodydef );

	if ( !( dest_class == pwr_cClass_Point ||	
	        dest_class == pwr_cClass_Backup))
	{
	  /* source and destination has to be of the same type */
	  if ( source_pointer_flag != dest_pointer_flag)
	    return GSX__CONTYPE;
	  else if ( source_type != dest_type)
	    return GSX__CONTYPE;
	}
	if ( source_pointer_flag)
	  *conclass = DATA_CONN;
	else
	{
	  switch ( source_type ) 
	  {
            case pwr_eType_Float32: 
	      *conclass = FLOAT_CONN;
              break;
            case pwr_eType_Boolean:
	      *conclass = BOOLEAN_CONN;
              break;
            case pwr_eType_Int32:
	      *conclass = INT32_CONN;
              break;
            default: 
	      *conclass = DEFAULT_CONN;
          }
	}
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gsx_check_subwindow()
*
* Type		int
*
* Type		Parameter	IOGF	Description
* vldh_t_node	object		I	vldh node.
* unsigned long	*subwindow_nr	IO	windowindex.
*
* Description:
*	Checks that the syntax of a subwindow is correct.
*	Special cases for this objekts:
*	-  Order: 
*	Checks that the order has a C attribute if the windowindex
*	is 2 (condition window). 
*	If it hasn't, the windowindex is set to 1 (activity window)
*	
*	
*
**************************************************************************/

int gsx_check_subwindow( 
	vldh_t_node	object,
	unsigned long	*subwindow_nr,
	foe_eFuncAccess	*function_access
)
{
	ldh_tSesContext		ldhses;
	pwr_tObjid		next_objdid;
	pwr_tClassId		cid;
	int			found, sts;

	ldhses = (object->hn.wind)->hw.ldhses; 

	/* Order */
	if ( object->ln.cid == pwr_cClass_order)
	{
	  if ( *subwindow_nr == 2 )
	  {
	    /* Check if this order has a COrder as a child */	
	    found = 0;
	    sts = ldh_GetChild( ldhses,	object->ln.oid, &next_objdid);
	    while ( ODD(sts) )
	    {
	      /* Find out if this is a COrder */
	      sts = ldh_GetObjectClass( ldhses,	next_objdid, &cid);

	      if ( cid == pwr_cClass_corder)
	      {
	        found = 1;
	      }
	      sts = ldh_GetNextSibling( ldhses,	next_objdid, &next_objdid);
	    }
	    if ( !found )
	    {
	      /* Subwindow should be a Activity window */
	      *subwindow_nr = 1; 
	    }
	  }
	  *function_access = foe_eFuncAccess_Edit;
	}
	else if ( object->ln.cid == pwr_cClass_Func)
	  *function_access = foe_eFuncAccess_View;
	else
	  *function_access = foe_eFuncAccess_Edit;

	return GSX__SUCCESS;
}

int gsx_auto_create( 
	WFoe		*foe,
	double 		x,
	double		y,
	vldh_t_node	source,
	unsigned long	sourcepoint,
	vldh_t_node	*dest,
	unsigned long	*destpoint
)
{
  int sts;
  ldh_tSesContext		ldhses;

  ldhses = (source->hn.wind)->hw.ldhses; 

  switch( source->ln.cid)
  {
    case pwr_cClass_step:
    case pwr_cClass_initstep:
    case pwr_cClass_substep:
    case pwr_cClass_ssend:
    {
      if ( sourcepoint == 0)
      {
        /* Create a trans object */
        sts = foe->gre->create_node( pwr_cClass_trans, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 2;
      }
      else if ( sourcepoint == 1)
      {
        /* Create an order object */
        sts = foe->gre->create_node( pwr_cClass_order, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 0;
      }
      else if ( sourcepoint == 2)
      {
        /* Create a trans object */
        sts = foe->gre->create_node( pwr_cClass_trans, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 0;
      }
      break;
    }
    case pwr_cClass_ssbegin:
    {
      if ( sourcepoint == 0)
      {
        /* Create an order object */
        sts = foe->gre->create_node( pwr_cClass_order, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 0;
      }
      else if ( sourcepoint == 1)
      {
        /* Create a trans object */
        sts = foe->gre->create_node( pwr_cClass_trans, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 0;
      }
      break;
    }
    case pwr_cClass_trans:
    {
      if ( sourcepoint == 0)
      {
        /* Create a step object */
        sts = foe->gre->create_node( pwr_cClass_step, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 2;
      }
      else if ( sourcepoint == 1)
      {
        sts = foe->gre->create_node( pwr_cClass_GetDgeneric, 
			x, y, dest);
        if ( EVEN(sts)) return sts;
	*destpoint = 0;
      }
      else if ( sourcepoint == 2)
      {
        /* Create a trans object */
        sts = foe->gre->create_node( pwr_cClass_step, x, y, dest);
        if ( EVEN(sts)) return sts;

	*destpoint = 0;
      }
      break;
    }
    default:
    {
      ldh_sParDef 		*bodydef;
      int			rows;
      goen_conpoint_type	graph_pointer;
      unsigned long		par_type;
      unsigned long		par_inverted;
      unsigned long		source_par_index;
      pwr_tUInt32		source_pointer_flag;

      /* Create a generic sto or get */
      sts = goen_get_parinfo( foe->gre, source->ln.cid,
			ldhses,
			source->ln.mask, 
			strlen( source->hn.name),
			sourcepoint, 
			&graph_pointer,	&par_inverted, &par_type,
			&source_par_index, source);
      if ( EVEN(sts) ) return( sts);

      /* Get the type of the source attribute */
      sts = ldh_GetObjectBodyDef( ldhses, source->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
      if ( EVEN(sts) )
      {
         /* This is a development object */
         sts = ldh_GetObjectBodyDef( ldhses, source->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
          if ( EVEN(sts) ) return sts;
      }

      switch (bodydef[source_par_index].ParClass)  
      {
        case pwr_eClass_Input:
          par_type = bodydef[source_par_index].Par->Input.Info.Type;
          source_pointer_flag = PWR_MASK_POINTER & 
		bodydef[source_par_index].Par->Output.Info.Flags;
          switch ( par_type ) 
          {
            case pwr_eType_Float32: 
	      if ( !source_pointer_flag)
                sts = foe->gre->create_node( pwr_cClass_GetAgeneric, 
			x, y, dest);
	      else
                sts = foe->gre->create_node( pwr_cClass_GetData, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Boolean:
              sts = foe->gre->create_node( pwr_cClass_GetDgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Int32:
              sts = foe->gre->create_node( pwr_cClass_GetIgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_String:
              sts = foe->gre->create_node( pwr_cClass_GetSgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Time:
              sts = foe->gre->create_node( pwr_cClass_GetATgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_DeltaTime:
              sts = foe->gre->create_node( pwr_cClass_GetDTgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            default: 
              free((char *) bodydef);
	      return 0;
          }
	  break;
        case pwr_eClass_Output:
          par_type = bodydef[source_par_index].Par->Output.Info.Type;
          switch ( par_type ) 
          {
            case pwr_eType_Float32: 
              sts = foe->gre->create_node( pwr_cClass_StoAgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Boolean:
              sts = foe->gre->create_node( pwr_cClass_StoDgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Int32:
              sts = foe->gre->create_node( pwr_cClass_StoIgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_String:
              sts = foe->gre->create_node( pwr_cClass_StoSgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_Time:
              sts = foe->gre->create_node( pwr_cClass_StoATgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            case pwr_eType_DeltaTime:
              sts = foe->gre->create_node( pwr_cClass_StoDTgeneric, 
			x, y, dest);
              if ( EVEN(sts)) return sts;
	      *destpoint = 0;
              break;
            default: 
              free((char *) bodydef);
	      return 0;
          }
	  break;
        default: 
          free((char *) bodydef);
          return 0;
      }

      free((char *) bodydef);
    }
  }
  return GSX__SUCCESS;
}



