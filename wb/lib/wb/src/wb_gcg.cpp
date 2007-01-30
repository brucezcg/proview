/* 
 * Proview   $Id: wb_gcg.cpp,v 1.1 2007-01-04 07:29:03 claes Exp $
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

/* wb_gcg.c -- code generation
   Gre code generation.
   This module generates code for the runtime system.  */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

#include "pwr_class.h"
#include "pwr_systemclasses.h"
#include "pwr_baseclasses.h"
#include "pwr_nmpsclasses.h"
#include "flow.h"
#include "flow_ctx.h"
#include "co_time.h"
#include "co_cdh.h"
#include "co_dcli.h"
#include "co_utl_batch.h"
#include "co_msg.h"
#include "co_api.h"
#include "wb_foe_msg.h"
#include "wb_vldh_msg.h"
#include "wb_ldh_msg.h"
#include "wb_vldh.h"
#include "wb_ldh.h"
#include "rt_load.h"
#include "wb_goen.h"
#include "wb_trv.h"
#include "wb_exo.h"
#include "wb_utl_api.h"
#include "wb_gcg.h"
#include "wb_dir.h"

extern "C" {
#include "wb_foe_dataarithm.h"
}

/*_Local procedues_______________________________________________________*/

/* Libraries for the plcmodules */
#define PLCLIB_VAX_ELN "pwrp_root:[vax_eln.lib]plc"
#define PLCLIB_VAX_VMS "pwrp_root:[vax_vms.lib]plc"
#define PLCLIB_AXP_VMS "pwrp_root:[axp_vms.lib]plc"
#define PLCLIB_UNIX    "libplc"



/* Frozen libraries for the plcmodules */
#define PLCLIB_FROZEN_VAX_ELN "pwrp_root:[vax_eln.lib]plcf"
#define PLCLIB_FROZEN_VAX_VMS "pwrp_root:[vax_vms.lib]plcf"
#define PLCLIB_FROZEN_AXP_VMS "pwrp_root:[axp_vms.lib]plcf"
#define PLCLIB_FROZEN_UNIX    "libplcf"

#define PLCLIB_FROZEN_LINK_UNIX "-lplcf"

/* Filename of cc-compile and link command file */
#define CC_COMMAND 	"pwr_exe:wb_gcg.com"
#define CC_COMMAND_UNIX "$pwr_exe/wb_gcg.sh"

/* Filename of struct includefile */
#define STRUCTINC "pwr_baseclasses.h"

/* Filename of macro includefile */
#define MACROINC "rt_plc_macro.h"

/* Filename of plc includefile */
#define PLCINC "rt_plc.h"

/* Filename of process includefile */
#define PROCINC "rt_plc_proc.h"

#define GCDIR 		"pwrp_root:[common.tmp]"
#define GCEXT ".gc"

#define DATDIR 		"pwrp_root:[common.load]"
#define DATEXT ".dat"

#define IS_LYNX(os) ((os & pwr_mOpSys_PPC_LYNX) \
		     || (os & pwr_mOpSys_X86_LYNX))

#define IS_LINUX(os) ((os & pwr_mOpSys_PPC_LINUX) \
		     || (os & pwr_mOpSys_X86_LINUX) \
		     || (os & pwr_mOpSys_AXP_LINUX))

#define IS_UNIX(os) (IS_LINUX(os) || IS_LYNX(os))

#define IS_NOT_UNIX(os) (!IS_UNIX(os))

#define IS_VMS_OR_ELN(os) ((os & pwr_mOpSys_VAX_ELN) \
			    || (os & pwr_mOpSys_VAX_VMS) \
			    || (os & pwr_mOpSys_AXP_VMS))

#define IS_NOT_VMS_OR_ELN(os) (!(IS_VMS_OR_ELN(os))


#define IS_VALID_OS(os) (IS_VMS_OR_ELN(os) || IS_UNIX(os))
	       	      
#define IS_NOT_VALID_OS(os) (!IS_VALID_OS(os))

typedef struct {
  pwr_tObjid	thread_objid;
  float 	scantime;
  int		prio;
  pwr_tObjid	*plclist;
  int		plc_count;
} gcg_t_timebase;

typedef struct {
   short buflen;
   short itemcode;
   void  *buf;
   short *retlen;
} itemdsc;   


static int	gcg_debug;
static char	gcgmv_filenames[2][30] = {
			"plc_v"};

static char	gcgmn_filenames[2][30] = {
			"plc_"};

static char	gcgm0_filenames[ GCGM0_MAXFILES ][30] = {
			"plc_m"};

static char	gcgm1_filenames[ GCGM1_MAXFILES ][30] = {
			"plc_dec", 
			"plc_r1r", 
			"plc_r2r", 
			"plc_ref", 
			"plc_cod",
			"plc_m"};

typedef int (* gcg_tMethod)(gcg_ctx, vldh_t_node);

int     gcg_comp_m0( vldh_t_plc	plc,
		     unsigned long	codetype,
		     unsigned long	*errorcount,
		     unsigned long	*warningcount,
		     unsigned long	spawn);
int	gcg_comp_m1( vldh_t_wind wind,
		     unsigned long codetype,
		     unsigned long *errorcount,
		     unsigned long *warningcount,
		     unsigned long spawn);
int	gcg_comp_m2( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m4( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m5( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m6( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m7( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m8( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m9( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m10( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m11( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m12( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m13( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m15( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m16( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m17( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m18( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m19( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m20( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m21( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m22( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m23( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m24( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m25( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m26( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m28( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m29( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m30( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m31( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m32( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m33( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m34( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m35( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m36( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m37( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m38( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m39( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m40( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m41( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m42( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m43( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m44( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m45( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m46( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m47( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m48( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m49( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m50( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m51( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m52( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m53( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m54( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m55( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m56( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m57( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m58( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m59( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m60( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m61( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m62( gcg_ctx gcgctx, vldh_t_node node);
int	gcg_comp_m63( gcg_ctx gcgctx, vldh_t_node node);

gcg_tMethod gcg_comp_m[70] = {
	(gcg_tMethod)gcg_comp_m0,
	(gcg_tMethod)gcg_comp_m1,
	gcg_comp_m2,
	gcg_comp_m2,
	gcg_comp_m4,
	gcg_comp_m5,
	gcg_comp_m6,
	gcg_comp_m7,
	gcg_comp_m8,
	gcg_comp_m9,
        gcg_comp_m10,
	gcg_comp_m11,
	gcg_comp_m12,
	gcg_comp_m13,
	gcg_comp_m13,
	gcg_comp_m15,
	gcg_comp_m16,
	gcg_comp_m17,
	gcg_comp_m18,
        gcg_comp_m19,
	gcg_comp_m20,
	gcg_comp_m21,
	gcg_comp_m22,
	gcg_comp_m23,
	gcg_comp_m24,
	gcg_comp_m25,
	gcg_comp_m26,
	gcg_comp_m26,
        gcg_comp_m28,
	gcg_comp_m29,
	gcg_comp_m30,
	gcg_comp_m31,
	gcg_comp_m32,
	gcg_comp_m33,
	gcg_comp_m34,
	gcg_comp_m35,
	gcg_comp_m36,
	gcg_comp_m37,
	gcg_comp_m38,
	gcg_comp_m39,
	gcg_comp_m40,
	gcg_comp_m41,
	gcg_comp_m42,
	gcg_comp_m43,
	gcg_comp_m44,
	gcg_comp_m45,
	gcg_comp_m46,
	gcg_comp_m47,
	gcg_comp_m48,
	gcg_comp_m49,
	gcg_comp_m50,
	gcg_comp_m51,
	gcg_comp_m52,
	gcg_comp_m53,
	gcg_comp_m54,
	gcg_comp_m55,
	gcg_comp_m56,
	gcg_comp_m57,
	gcg_comp_m58,
	gcg_comp_m59,
	gcg_comp_m60,
	gcg_comp_m61,
	gcg_comp_m62,
	gcg_comp_m63
	};


#if 0
static int gcg_get_converted_hiername( 
    ldh_tSesContext ldhses,
    pwr_tObjid	objdid,
    char	*converted_name
);
#endif

static pwr_tStatus gcg_get_build_host(
    pwr_mOpSys os,
    char *buf,
    int bufsize
);


static int gcg_cc(
    unsigned long	    filetype,
    char		    *p1,
    char		    *p2,
    char		    *p3,
    pwr_mOpSys		    os,
    unsigned long	    spawn
);

static int	gcg_get_structname( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		**structname
);

static int	gcg_get_connected_par_close( 
    vldh_t_node	node,
    unsigned long	point,
    vldh_t_node		*conn_node,
    char		*conn_obj,
    char		*conn_par
);

static int	gcg_pgmname_to_parname( 
    ldh_tSesContext 	ldhses,
    pwr_tClassId	cid,
    char		*pgmname,
    char		*parname
);

static int	gcg_get_par_close( 
    vldh_t_node	node,
    unsigned long	point,
    unsigned long	*output_count,
    vldh_t_node	*output_node,
    unsigned long	*output_point,
    ldh_sParDef 	*output_bodydef,
    unsigned long	conmask,
    unsigned long	*par_type
);

static int	gcg_get_input( 
    vldh_t_node		node,
    unsigned long	point,
    unsigned long	*output_count,
    vldh_t_node		*output_node,
    unsigned long	*output_point,
    ldh_sParDef 	*output_bodydef,
    unsigned long	conmask
);
static void	gcg_ctx_new(
    gcg_ctx	    *gcgctx,
    vldh_t_wind	    wind
);

static void	gcg_ctx_delete(
    gcg_ctx		gcgctx
);

static int	gcg_ioread_insert(
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
);

static int	gcg_iowrite_insert( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
);

static int	gcg_ref_insert( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		prefix
);

static int	gcg_aref_insert( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
);

static int	gcg_ref_print( 
    gcg_ctx		gcgctx
);

static int	gcg_aref_print( 
    gcg_ctx		gcgctx
);

static int	gcg_ioread_print(
    gcg_ctx		gcgctx
);

static int	gcg_iowrite_print(
    gcg_ctx		gcgctx
);

static int	gcg_get_outputstring_spec( 
    gcg_ctx		gcgctx,
    vldh_t_node		output_node,
    ldh_sParDef 	*output_bodydef,
    pwr_sAttrRef       	*parattrref,
    int			*partype,
    char		*parprefix,
    char		*parstring
);

static int	gcg_get_inputstring( 
    gcg_ctx		gcgctx,
    vldh_t_node		output_node,
    ldh_sParDef 	*output_bodydef,
    pwr_sAttrRef       	*parattrref,
    int			*partype,
    char		*parprefix,
    char		*parstring
);

static void gcg_print_decl( 
    gcg_ctx		gcgctx,
    char		*objname,
    pwr_tObjid		objdid,
    char		prefix,
    unsigned long	reftype
);

static void gcg_print_adecl( 
    gcg_ctx		gcgctx,
    char		*objname,
    pwr_sAttrRef       	attrref,
    char		prefix,
    unsigned long	reftype
);

static void gcg_print_rtdbref( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		prefix,
    pwr_tClassId	cid,
    unsigned long	reftype
);

static void gcg_print_artdbref( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix,
    pwr_tClassId	cid,
    unsigned long	reftype
);

static int gcg_error_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    vldh_t_node		node
);

static int gcg_plc_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    pwr_tObjid		plcobjdid
);

static int gcg_node_comp( 
    gcg_ctx	gcgctx,
    vldh_t_node	node
);

static int	gcg_get_child_windows( 
    ldh_tSesContext	ldhses,
    pwr_tObjid		parent_objdid,
    unsigned long	*wind_count,
    pwr_tObjid		**windlist
);

static int	gcg_get_plc_windows( 
    ldh_tSesContext	ldhses,
    pwr_tObjid		plcobjdid,
    unsigned long	*wind_count,
    pwr_tObjid		**windlist
);

static int	gcg_get_child_plc( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*plc_count,
    gcg_t_plclist	**plclist
);

static int	gcg_get_rtnode_plc( 
    gcg_ctx		gcgctx,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*plc_count,
    gcg_t_plclist	**plclist
);

static int  gcg_sort_plclist( 	
    gcg_ctx		gcgctx,
    gcg_t_plclist	*plclist,
    unsigned long	size
);

static int  gcg_sort_threadlist( 	
    gcg_ctx		gcgctx,
    gcg_t_threadlist	*threadlist,
    unsigned long	size
);

static int	gcg_file_check(
    char	*filename
);

static pwr_tStatus gcg_read_volume_plclist( 
  gcg_ctx	gcgctx,
  pwr_tVolumeId	volid,
  unsigned long	*plc_count, 
  gcg_t_plclist **plclist,
  unsigned long	*thread_count, 
  gcg_t_threadlist **threadlist
);


static int	gcg_parname_to_pgmname( 
    ldh_tSesContext ldhses,
    pwr_tClassId cid,
    char	*parname,
    char	*pgmname
);

static pwr_tStatus gcg_replace_ref( 
    gcg_ctx gcgctx, 
    pwr_sAttrRef *attrref, 
    vldh_t_node output_node
);

static int gcg_is_window( 
    gcg_ctx gcgctx, 
    pwr_tOid oid
);


static int gcg_set_cmanager( 
    vldh_t_wind wind);

static int gcg_cmanager_find_nodes( 
    vldh_t_wind wind, 
    vldh_t_node mgr, 
    vldh_t_node *nodelist,
    int node_count);

static int gcg_cmanager_comp( 
    gcg_ctx	gcgctx,
    vldh_t_node	node);

static int gcg_reset_cmanager( 
    gcg_ctx	gcgctx);



/*_Methods defined for this module_______________________________________*/

/*************************************************************************
*
* Name:		gcg_executorder_nodes()
*
* Type		int
*
* Type		Parameter	IOGF	Description
* vldh_t_node	*nodelist	I	list of vldh nodes
* unsigned long	node_count	I	number of nodes in nodelist
*					on the nodes.
*
* Description:	Order the nodes in the nodelist in order of increasing
*	executeorder. The executer order has to be calculated by the 
*	exo module first.
*
**************************************************************************/

int gcg_executeorder_nodes ( 
  unsigned long		node_count,
  vldh_t_node		*nodelist
)
{
	vldh_t_node		*node_ptr1;
	vldh_t_node		*node_ptr2;
	vldh_t_node		dum;
	int			i,j;

	for ( i = node_count - 1; i > 0; i--)
	{
	  node_ptr1 = nodelist;
	  node_ptr2 = node_ptr1 + 1;
	  for ( j = 0; j < i; j++)
	  {
	    if ( (*node_ptr1)->hn.executeorder > (*node_ptr2)->hn.executeorder)
	    {
	      /* Change order */
	      dum = *node_ptr2;
	      *node_ptr2 = *node_ptr1;
	      *node_ptr1 = dum;
	    }
	    node_ptr1++;
	    node_ptr2++;
	  }
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_conpoint_nodes()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node
* unsigned long	point		I	connection point on node
* unsigned long	*point_count	O	number of conpoints in pointlist
* vldh_t_conpoint **pointlist	O	connected nodes and connectionpoints
*					on the nodes.
*
* Description:
*	This routine calls vldh_get_conpoint_nodes which
*	returns all nodeobjects connected to a connectionpoint and the
*	connectionpoints on the found nodes. The input node and
*	connectionpoint is placed first in the list.
*	All nodes of class Point is taken away from the list in this
*	routine.
*
**************************************************************************/

int gcg_get_conpoint_nodes (
  vldh_t_node	  node,
  unsigned long	  point,
  unsigned long	  *point_count,
  vldh_t_conpoint **pointlist,
  unsigned long	  conmask
)
{
	unsigned long	p_count;
	vldh_t_node	next_node;
	int		nopoint;
	int		i, sts;

	sts = vldh_get_conpoint_nodes( node, point, 
			&p_count, pointlist, conmask);
	if ( EVEN(sts)) return sts;

	nopoint = 0;
	for ( i = 0; i < (int)p_count; i++) {
	  next_node = (*pointlist + i)->node;
	  if ( next_node->ln.cid != pwr_cClass_Point) {
	    (*pointlist + nopoint)->node = (*pointlist + i)->node;
	    (*pointlist + nopoint)->conpoint = (*pointlist + i)->conpoint;
	    nopoint++;
	  }
	}
	*point_count = nopoint;
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_scantime_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_tObjid	objdid		I	objdid of the object.
*
* Description:
*	This routine is called when generating code for objects that
*	might use the scantime of the plcprogram. 
*	Checks first if there is a parameter called Scantime in rtbody
*	and if this is found prints the line
*	'objectpointer'->Scantime = Time;
*	in initiation file (GCGM1_REF_FILE).
*
**************************************************************************/

int gcg_scantime_print( 
  gcg_ctx	gcgctx,
  pwr_tObjid	objdid
)
{
	float	*scantime;
	int 	sts, size;

	/* If there is a parameter named scantime insert it 
	  in the reffile */
	sts = ldh_GetObjectPar( gcgctx->ldhses,
			objdid, 
			"RtBody",
			"ScanTime",
			(char **)&scantime, &size); 
	if ( ODD(sts))
	{
	  free((char *) scantime);
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"%c%s->ScanTime = &tp->ActualScanTime;\n", 
		GCG_PREFIX_REF,
		vldh_IdToStr(0, objdid));
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_timer_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_tObjid	objdid		I	objdid of the object.
*
* Description:
*	This routine is called when generating code for objects that
*	might include a timer.
*	Checks first if there is a parameter called TimerDo in rtbody
*	and if this is found prints the line
*	'objectpointer'->TimerDO = &'objectpointer'->TimerDODum;
*	in initiation file (GCGM1_REF_FILE).
*
**************************************************************************/

static int gcg_timer_print( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid
)
{
	int 	sts, size;
	char	*dummy;

	/* If there is a parameter named TimerDo insert it 
	  in the reffile */
	sts = ldh_GetObjectPar( gcgctx->ldhses,
			objdid, 
			"RtBody",
			"TimerDO",
			&dummy, &size); 
	if ( ODD(sts))
	{
	  free( dummy);
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"%c%s->TimerDO = &%c%s->TimerDODum;\n", 
		GCG_PREFIX_REF,
		vldh_IdToStr(0, objdid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, objdid));
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_converted_hiername()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tObjid	objdid		I	objdid of object.
* char		*converted_name	O	the converted hierachy name of the 
*					object.
*
* Description:
*	Returns the hierachy name of the object with all "-" substituted
*	with "_" so the name can be used in c-code.
*
**************************************************************************/
#if 0
static int gcg_get_converted_hiername( 
    ldh_tSesContext ldhses,
    pwr_tObjid	objdid,
    char	*converted_name
)
{
	pwr_tOName	name;
	int		sts, size;
	char		*b,*s;
	int		i;	

	/* Get the hierarchy name */
	sts = ldh_ObjidToName( ldhses, objdid, ldh_eName_Hierarchy,
		name, sizeof( name), &size);
	if ( EVEN(sts)) return sts;

	/* Convert */
	b = name;
	s = converted_name;
	for ( i = 0; *b != '\0'; b++ )
	{
	  *s = *b;
	  if ( *b == '-' ) 
	    *s = '_';
	  s++;
	}	
	*s = '\0';

	return GSX__SUCCESS;
}
#endif

/*************************************************************************
*
* Name:		gcg_get_build_host
*
* Description: Gets the name of UNIX build host
*
**************************************************************************/
static pwr_tStatus gcg_get_build_host(
    pwr_mOpSys os,
    char *buf,
    int bufsize
)
{
	int sts;
	char logname[32];

	if (os & pwr_mOpSys_PPC_LYNX)
	  strcpy(logname, "pwr_build_host_ppc_lynx");
	else if (os & pwr_mOpSys_X86_LYNX)
	  strcpy(logname, "pwr_build_host_x86_lynx");
	else if (os & pwr_mOpSys_PPC_LINUX)
	  strcpy(logname, "pwr_build_host_ppc_linux");
	else if (os & pwr_mOpSys_X86_LINUX)
	  strcpy(logname, "pwr_build_host_x86_linux");
	else if (os & pwr_mOpSys_AXP_LINUX)
	  strcpy(logname, "pwr_build_host_axp_linux");
	else
	  return GSX__NOBUILDHOST;

#if defined OS_VMS
	{
	  unsigned int attr = LNM$M_CASE_BLIND;
	  struct dsc$descriptor tabdsc;
	  char *tabname = "LNM$DCL_LOGICAL";
	  struct dsc$descriptor logdsc;
	  short namelen;
	  itemdsc itemlist[2];

	  itemlist[0].buflen = bufsize - 1;
	  itemlist[0].itemcode = LNM$_STRING;
	  itemlist[0].buf = buf; 
	  itemlist[0].retlen = &namelen;

	  itemlist[1].buflen = 0;
	  itemlist[1].itemcode = 0;
	  itemlist[1].buf = NULL; 
	  itemlist[1].retlen = NULL;

	  tabdsc.dsc$w_length   = strlen(tabname);
	  tabdsc.dsc$b_dtype = DSC$K_DTYPE_T;
	  tabdsc.dsc$b_class = DSC$K_CLASS_S;
	  tabdsc.dsc$a_pointer = tabname;

	  logdsc.dsc$w_length   = strlen(logname);
	  logdsc.dsc$b_dtype = DSC$K_DTYPE_T;
	  logdsc.dsc$b_class = DSC$K_CLASS_S;
	  logdsc.dsc$a_pointer = logname;

	  sts = sys$trnlnm(&attr, &tabdsc, &logdsc, NULL, itemlist);
	  if (ODD(sts)) 
	    buf[namelen] = '\0';  
	}
#else
	{
	  char *value;

          if ( (value = getenv( logname)) == NULL)
	    return GSX__NOBUILDHOST;

	  strncpy( value, buf, bufsize);
	}
#endif

	return sts;
}

/*************************************************************************
*
* Name:		gcg_cc()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	filetype	I	parameter to ds_foe_gcg.com
*					type of operation
*					0: create plc process objectmodule
*					1: create plcpgm objectmodule
*					2: create window objectmodule
*					3: link
* char		*p1		I	parameter to ds_foe_gcg.com
* char		*p2		I	parameter to ds_foe_gcg.com
* char		*p3		I	parameter to ds_foe_gcg.com
* pwr_mOpSys os       I       Operating system
* unsigned long	spawn		I	if eq GCG_SPAWN the operation
*					is spawned, else a system call is done.
*
* Description:
*	Create an objectfile from the generated c-code.
*	The command file ds_foe_gcg.com is started and one of the 
*	following operations is made:
*	- create plc process objectmodule
*	- create plcpgm objectmodule
*	- create window objectmodule
*	- linking of the plc job.
*	
*
**************************************************************************/

static int gcg_cc(
    unsigned long	    filetype,
    char		    *p1,
    char		    *p2,
    char		    *p3,
    pwr_mOpSys		    os,
    unsigned long	    spawn
)
{ 
	char		build_host[32];
#if defined OS_VMS
	int		sts;
	static char	cmd[256];
	char		queue[32];
	$DESCRIPTOR(cmd_desc,cmd);
        unsigned long 	cli_flag = CLI$M_NOWAIT;
	pwr_tBoolean  	batch;
	char		systemname[80];
	char		systemgroup[80];

	utl_get_systemname( systemname, systemgroup);

        if (IS_UNIX(os)) {
	  sts = gcg_get_build_host(os, build_host, sizeof(build_host));
	  if (EVEN(sts)) {
	    printf("Couldn't get build host for pwr_mOpSys = %d\n", os);
	    return sts;
	  }
	}

	if ( os & pwr_mOpSys_VAX_VMS)
	{
#if defined(VAX) || defined(__VAX)
	  batch = 0;
#else
	  batch = 1;
	  strcpy(queue, "PWR_VAX_VMS_BATCH");
#endif
	}
	else if ( os & pwr_mOpSys_VAX_ELN)
	{
#if defined(VAX) || defined(__VAX)
	  batch = 0;
#else
	  batch = 1;
	  strcpy(queue, "PWR_VAX_ELN_BATCH");
#endif
	}
	else if ( os & pwr_mOpSys_AXP_VMS)
	{
#if defined(__ALPHA) && defined(__VMS) 
	  batch = 0;
#else
	  batch = 1;
	  strcpy(queue, "PWR_AXP_VMS_BATCH");
#endif
	}
	else if (IS_UNIX(os))
	  batch = 0;
	else
	  return GSX__UNKNOPSYS;

	if (batch) {
	  sts = utl_BatchFindQueue(queue);
	  if (EVEN(sts)) {
	    printf("- ,Could not find batch queue: %s\n", queue);
	    return sts;
	  }
	}

	if (p2 == NULL) {
	  p2 = "Dummy";
	}
	if (p3 == NULL) {
	  p3 = "Dummy";
	}

	if (batch) {
	  sprintf(cmd, "%s %s %s %d %d %s %d %s \"%s\"", "mc pwr_exe:pwr_batch",
	    queue, CC_COMMAND, gcg_debug, filetype, p1, os, p2, p3);
#if 0
	} else if (IS_UNIX(os)) {
 	  char *p = cmd;

	  sprintf( cmd, "rsh %s %s %d %d %s %d %s %s", 
	    build_host, CC_COMMAND_UNIX, gcg_debug, filetype, p1, os, p2, p3);
	  while (*p++)
	    *p = tolower(*p);
#endif
	} else {
 	  sprintf( cmd, "@%s %d %d %s %d %s \"%s\" \"%s\"", 
	    CC_COMMAND, gcg_debug, filetype, p1, os, p2, p3, systemname);
	}

	if ( spawn == GCG_SPAWN && IS_NOT_UNIX(os)) {
	  sts = lib$spawn (&cmd_desc , 0 , 0 , &cli_flag );
	  if (EVEN(sts)) {
	    printf("** Error in lib$spawn, when creating subprocess.\n");
	    return sts;
	  }
	} else {
	  sts = system( cmd);
	  if ( EVEN(sts)) return sts;
	}

#else
	char		systemname[80];
	int		sts;
	static pwr_tCmd	cmd;
	pwr_tFileName  	fname;

	// TODO 
	if ( 1 /* os & pwr_mOpSys_X86_LINUX */ )
	{
	  if (p2 == NULL)
	    p2 = "Dummy";
	  if (p3 == NULL)
	    p3 = "Dummy";

	  utl_get_projectname( systemname);

	  dcli_translate_filename( fname, CC_COMMAND_UNIX);
 	  sprintf( cmd, "%s %d %ld %s %d %s %s %s", 
	    fname, gcg_debug, filetype, p1, os, p2, p3, systemname);
	  sts = system( cmd);
          if ( sts != 0) return GSX__CCERROR;
	}
	else
	{
	  printf("NYI...\n");
	  gcg_get_build_host(os, build_host, sizeof(build_host));
	}
#endif
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_structname()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context
* pwr_tObjid	objdid		I	objdid of object
* char		**structname	O	name of c-struct describing the 
*					object in rtdb.
*
* Description:
*	Get the name of the c-struct describing the object in rtdb.
*	The name is fetched from StructName in rtbody.
*
*
**************************************************************************/

static int	gcg_get_structname_from_cid( 
    gcg_ctx		gcgctx,
    pwr_tCid		cid,
    char		**structname
)
{ 
	ldh_tSesContext ldhses;
	pwr_tClassId			bodyclass;
	pwr_sObjBodyDef			*bodydef;	
	int				sts, size;

	ldhses = gcgctx->wind->hw.ldhses;

	sts = ldh_GetClassBody(ldhses, cid, "RtBody", &bodyclass,
	  (char **)&bodydef, &size);
   	if ( EVEN(sts))
   	{
   	  /* Try sysbody */
   	  sts = ldh_GetClassBody(ldhses, cid, "SysBody",
   		&bodyclass, (char **)&bodydef, &size);
   	  if( EVEN(sts) ) return sts;
   	}

	*structname = bodydef->StructName;

	return GSX__SUCCESS;
}

static int	gcg_get_structname( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		**structname
)
{ 
	pwr_tClassId		cid;
	ldh_tSesContext 	ldhses;
	pwr_tStatus 		sts;

	ldhses = gcgctx->wind->hw.ldhses;

 	sts = ldh_GetObjectClass(ldhses, objdid, &cid);
	if ( EVEN(sts)) return sts;

	return gcg_get_structname_from_cid( gcgctx, cid, structname);
}

/*************************************************************************
*
* Name:		gcg_plc_compile()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_plc	plc		I	vldh plc.
* unsigned long	codetype	I	1: code is generated
*					0: only syntax control
* unsigned long	*errorcount	O	number of errors
* unsigned long	*warningcount	O	number of warnings
* unsigned long	spawn		I	if the c compilation should be spawned
*					(0) or not (1).
*
* Description:
*	Calls the gcg method for a plcpgm.
*
**************************************************************************/

int gcg_plc_compile ( 
  vldh_t_plc	plc,
  unsigned long	codetype,
  unsigned long	*errorcount,
  unsigned long	*warningcount,
  unsigned long	spawn,
  int		debug
)
{
	int sts;

	gcg_debug = debug;

	/* The compilation method for plc programs is 0 */
	sts = gcg_comp_m0(plc, codetype, errorcount, warningcount, spawn); 
	if ( EVEN(sts)) return sts;
/***********
	if ( codetype == 1)
	{
	  sts = ldhld_PlcPgmModule(plc->hp.ldhsesctx, plc->lp.oid);
	  if ( EVEN(sts)) return sts;
	}
*/
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_plcwindow_compile()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_wind	wind		I	vldh window.
* unsigned long	codetype	I	1: code is generated
*					0: only syntax control
* unsigned long	*errorcount	O	number of errors
* unsigned long	*warningcount	O	number of warnings
* unsigned long	spawn		I	if the c compilation should be spawned
*					(0) or not (1).
*
* Description:
*	Calls the gcg method for a window.
*
**************************************************************************/
int gcg_plcwindow_compile ( 
  vldh_t_wind	wind,
  unsigned long	codetype,
  unsigned long	*errorcount,
  unsigned long	*warningcount,
  unsigned long	spawn,
  int		debug
)
{
	int sts;

	gcg_debug = debug;

	/* Find any compilation managers */
	sts = gcg_set_cmanager( wind);

	/* Calculate execute order */
	sts = exo_wind_exec( wind);
	if ( EVEN(sts)) return sts;

	/* Call the compilation method for a plc window */
	sts = gcg_comp_m1( wind, codetype, errorcount, 
			   warningcount, spawn); 
	if ( EVEN(sts)) return sts;
/************
	if ( codetype == 1)
	{
	  sts = ldhld_PlcWinModule(wind->hw.ldhses, wind->lw.oid);
	  if ( EVEN(sts)) return sts;
	}
*/
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_print_inputs()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
* char		*delimstr	I	delimiter string.
* unsigned long	printtype	I	if all input should be printed 
*					(GCG_PRINT_ALLPAR) or only the 
*					connected (GCG_PRINT_CONPAR).
* unsigned long	*nocondef	I	pointer to a value that should be
*					printed as defaultvalue if the input
*					is not connected.
* unsigned long	*nocontype	I	type of the inputvalue.
*
* Description:
*	Gets and prints all inputs.
*	This routine is used by some gcgmethods to get and print
*	all the connected objects and parameters in the printed function call.
*	Connected outputs for all the inputs are looked for and if 
*	they are found they are printed separated by the delimstr
*	If one input is not found printtype tells wether to jump to
*	the next on without printing anything, or to print 
*	- 0 if nocondef is NULL or
*	- the value specified by nocondef.
*	nocondef makes it possible to define an array that contains a default
*	value for each input parameter. nocondef is a pointer to this array and
*	nocontype is a pointer to an array with the types of the values in
*	nocondef.
*	Ex declaration of nocondef and nocontype:
* 	unsigned long		nocondef[2] = { 0, TRUE};
*	unsigned long		nocontype[2] = { GCG_BOOLEAN, GCG_BOOLEAN };
*
**************************************************************************/

int	gcg_print_inputs( 
    gcg_ctx		gcgctx,
    vldh_t_node		node,
    char		*delimstr,
    unsigned long	printtype,
    gcg_t_nocondef	*nocondef,
    unsigned long	*nocontype
)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	ldh_tSesContext 	ldhses;
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	int			input_count, inputscon_count;

	ldhses = (node->hn.wind)->hw.ldhses;

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	input_count = 0;
	inputscon_count = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    input_count++;
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0)
	    {	
	      output_found = 1;
	      inputscon_count++;
	      if ( output_count > 1) 
	   	   gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( (inputscon_count - 4 * (inputscon_count / 4)) == 0 &&
		   inputscon_count)
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "\n");
	      if ( !first_par)
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%s",
			delimstr);
	      if ( par_inverted ) 
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"!");
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%c%s->%s", 
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(0, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	  }
	  if ( !output_found )
	  {
	    if ( printtype == GCG_PRINT_ALLPAR )
	    {
	      /* The point is not connected and not visible */
	      if ( !first_par)
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%s",
			delimstr);
	      }
	      if ( nocondef == NULL)
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "0");
	      }
	      else
	      { 
	        switch ( *(nocontype + i))
	        {
	          case GCG_BOOLEAN :
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"%d",
			(nocondef + i)->bo);
	            break;
	          case GCG_INT32 :
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"%d",
			(nocondef + i)->bo);
	            break;
	          case GCG_FLOAT:
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"%f",
			(nocondef + i)->fl);
	            break;
	          case GCG_STRING:
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"\"%s\"",
			(nocondef + i)->str);
	            break;
	          case GCG_ATIME:
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"{%ld,%ld}",
			(long int)(nocondef + i)->atime.tv_sec, (nocondef + i)->atime.tv_nsec);
	            break;
	          case GCG_DTIME:
	            IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 	"{%d,%d}",
			(nocondef + i)->dtime.tv_sec, (nocondef + i)->dtime.tv_nsec);
	            break;
	        }
	      }
	      first_par = 0;
	    }
	  }
	  if ( input_count > 0 )
	    first_par = 0;

	  i++;
	}
	if ( (input_count == 0) || (inputscon_count == 0))
	{
	  /* No inputs in the graphics or no inputs connected */
	  if ( inputscon_count > inputscon_count)
	    gcg_error_msg( gcgctx, GSX__NOINPUTS, node);

	  if ( printtype == GCG_PRINT_CONPAR )
	  {
	    if ( nocondef == NULL)
	    {
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "0");
  	    }
	    else
	    {
	      if ( *(nocontype + i) == GCG_BOOLEAN )
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		 	"%d",
			(nocondef + i)->bo);
	      }
	      else
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		 	"%f",
			(nocondef + i)->fl);
	      }
	    }
	  }	  
	}

	free((char *) bodydef);	
	return GSX__SUCCESS;
}




#if 0
/*************************************************************************
*
* Name:		gcg_print_exec()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
* pwr_tObjid	objdid		I  	objdid.
* char		prefix		I	rtdb pointer prefix.
*
* Description:
*	Prints the beginning of an exec command for a function.
*		'structname'_exec( tp, 'pointername',
*
**************************************************************************/

static int	gcg_print_exec(
    gcg_ctx		gcgctx,
    vldh_t_node		node,
    pwr_tObjid		objdid,
    char		prefix
)
{
	int			sts;
	char			*name;

	/* Get name for the exec command for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;	

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s, ",
		name,
		prefix,
		vldh_IdToStr(0, objdid));
	return GSX__SUCCESS;

}
#endif

/*************************************************************************
*
* Name:		gcg_print_exec_macro()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
* pwr_tObjid	objdid		I  	objdid.
* char		prefix		I	rtdb pointer prefix.
*
* Description:
*	Prints the beginning of an exec command for a macro.
*		'structname'_exec( 'pointername',
*
**************************************************************************/

static int	gcg_print_exec_macro(
    gcg_ctx		gcgctx,
    vldh_t_node		node,
    pwr_tObjid		objdid,
    char		prefix
)
{
	int			sts;
	char			*name;

	/* Get name for the exec command for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;	

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
		prefix,
		vldh_IdToStr(0, objdid));
	return GSX__SUCCESS;

}



/*************************************************************************
*
* Name:		gcg_get_connected_parameter()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* unsigned long	point		I	connection point.
* vldh_t_node	*conn_node	O	found connected node.
* char		*conn_obj	O	object string of connected node.
* char		*conn_par	O	parameter name of connected node.
*
* Description:
*	Looks for an output or input connected to a connectionpoint. 
*	Looks first for an output, if no output is found it looks for an input.
*	Returns a string with objectname and parameter and index 0
*	if output is an array.
*
**************************************************************************/

int gcg_get_connected_parameter (
  vldh_t_node	node,
  unsigned long	point,
  vldh_t_node	*conn_node,
  char		*conn_obj,
  char		*conn_par
)
{
	gcg_t_ctx		gcgctx;
	char			*name;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	int			output_found, sts, size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	pwr_tClassId		cid;
	unsigned long		par_type;
	unsigned long		par_inverted;
	unsigned long		par_index;
	ldh_sParDef 		*bodydef;
	int 			rows;

	gcgctx.ldhses = (node->hn.wind)->hw.ldhses;
	gcgctx.errorcount = 0;
	gcgctx.warningcount = 0;

	/* Check if the point is an output */
	sts = goen_get_parameter( node->ln.cid, 
		node->hn.wind->hw.ldhses, 
		node->ln.mask, point, 
		&par_type, &par_inverted, &par_index);
	if ( EVEN(sts)) return sts;

	if ( par_type == PAR_OUTPUT )
	{
	  output_count = 1;
	  output_node = node;
	  output_point = point;
	  sts = ldh_GetObjectBodyDef(
			node->hn.wind->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) )
	  {
	    /* This is a development node, get the development body instead */
	    sts = ldh_GetObjectBodyDef(
			node->hn.wind->hw.ldhses, 
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	    if ( EVEN(sts) ) return sts;
	  }
	  memcpy( &output_bodydef,  &(bodydef[par_index]), 
			sizeof(output_bodydef)); 
	  free((char *) bodydef);
	}
	else
        {
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;
	}

	if ( output_count > 0 )
	{
	  sts = gcg_get_outputstring( &gcgctx, output_node, &output_bodydef, 
		        &output_attrref, &output_type, &output_prefix, output_par);
	  if ( sts == GSX__NEXTPAR ) return GSX__SWINDERR;
	  if ( EVEN(sts)) return sts;
	  
	  /* Get the name of the node */
	  sts = ldh_AttrRefToName( 
		gcgctx.ldhses,
		&output_attrref, cdh_mNName,
		&name, &size);
	  if( EVEN(sts)) return sts;

	  strcpy( conn_obj, name);

	  /* Get class of the output object */
	  sts = ldh_GetObjectClass(
			gcgctx.ldhses,
			output_attrref.Objid,
			&cid);
	  if( EVEN(sts)) return sts;

	  /* Convert the pgmname given in gcg_get_outputstring to parname */
	  sts = gcg_pgmname_to_parname( gcgctx.ldhses, cid, 
		output_par, conn_par );
	  if( EVEN(sts)) return sts;
	
	  *conn_node = output_node;
	}
	else
	{
	  /* Look for an input connected to this point */
	  sts = gcg_get_input( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  if ( output_count > 0 )
	  {
	    output_found = 1;

	    sts = gcg_get_inputstring( &gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return GSX__SWINDERR;
	    if ( EVEN(sts)) return sts;
	  
	    /* Get the name of the node */
	    sts = ldh_AttrRefToName( 
		gcgctx.ldhses,
		&output_attrref, ldh_eName_Hierarchy,
		&name, &size);
	    if( EVEN(sts)) return sts;

	    strcpy( conn_obj, name);

	    /* Get class of the output object */
	    sts = ldh_GetAttrRefOrigTid(
			gcgctx.ldhses,
			&output_attrref,
			&cid);
	    if( EVEN(sts)) return sts;

	    /* Convert the pgmname given in gcg_get_outputstring to parname */
	    sts = gcg_pgmname_to_parname( gcgctx.ldhses, cid, 
			output_par, conn_par );
	    if( EVEN(sts)) return sts;
	
	    *conn_node = output_node;
	  }
	  else
	  {
	    /* No output and no input found */
	    return GSX__NOTCON;
	  }
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_connected_parameter()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* unsigned long	point		I	connection point.
* vldh_t_node	*conn_node	O	found connected node.
* char		*conn_obj	O	object string of connected node.
* char		*conn_par	O	parameter name of connected node.
*
* Description:
*	Looks for an output or input connected to a connectionpoint. 
*	Looks first for an output, if no output is found it looks for an input.
*	Returns a string with objectname and parameter and index 0
*	if output is an array.
*
**************************************************************************/

static int	gcg_get_connected_par_close( 
    vldh_t_node	node,
    unsigned long	point,
    vldh_t_node	*conn_node,
    char		*conn_obj,
    char		*conn_par
)
{
	gcg_t_ctx		gcgctx;
	char			*name;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	int			sts, size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	pwr_tClassId		cid;
	unsigned long		par_type;

	gcgctx.ldhses = (node->hn.wind)->hw.ldhses;
	gcgctx.errorcount = 0;
	gcgctx.warningcount = 0;

	/* Look for a parameter connected to this point */
	sts = gcg_get_par_close( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL, &par_type);
	if ( EVEN(sts)) return sts;

	if (output_count > 0) {
	  if (par_type == PAR_OUTPUT) {
	    sts = gcg_get_outputstring( &gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return GSX__SWINDERR;
	    if ( EVEN(sts)) return sts;
	  }	  
	  else if (par_type == PAR_INPUT) {
	    sts = gcg_get_inputstring( &gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return GSX__SWINDERR;
	    if ( EVEN(sts)) return sts;
	  }
	  else
	    return GSX__SWINDERR;

	  /* Get the name of the node */
	  sts = ldh_AttrRefToName( 
		gcgctx.ldhses,
		&output_attrref, ldh_eName_Hierarchy,
		&name, &size);
	  if( EVEN(sts)) return sts;

	  strcpy( conn_obj, name);

	  /* Get class of the output object */
	  sts = ldh_GetAttrRefOrigTid(
			gcgctx.ldhses,
			&output_attrref,
			&cid);
	  if( EVEN(sts)) return sts;

	  /* Convert the pgmname given in gcg_get_outputstring to parname */
	  sts = gcg_pgmname_to_parname( gcgctx.ldhses, cid, 
		output_par, conn_par );
	  if( EVEN(sts)) return sts;
	
	  *conn_node = output_node;
	}
	else {
	  /* No output and no input found */
	  return GSX__NOTCON;
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_debug()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* char		*debug_parname	I	pgmname of debug parameter.
* char		*conn_obj	I	name of object associated with debugpar.
* char		*conn_par	I	name of the parameter.
*
* Description:
*	Return the object and parameter for the debugparameter in
*	graphbody.
*	First parameter refered to by the debugpar is identified.
*	Then the object and parameter is fetched with the routine 
*	gcg_get_outputstring and the pgmname return by this routine
*	is converted to parametername.
*
*
**************************************************************************/

int gcg_get_debug (
  vldh_t_node	node,
  char		*debug_parname,
  char		*conn_obj,
  char		*conn_par,
  pwr_eType	*par_type
)
{
	gcg_t_ctx		gcgctx;
	char			*name;
	pwr_tClassId		cid;
	ldh_sParDef 		debug_bodydef;
	int			i, sts, size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			debug_par[32];
	size_t			pos;
	ldh_sParDef 		*bodydef;
	int 			rows;
	int			par_index, found;

	gcgctx.ldhses = (node->hn.wind)->hw.ldhses;
	gcgctx.errorcount = 0;
	gcgctx.warningcount = 0;

	/* remove index from debugparameter if it is an array */
	strcpy( debug_par, debug_parname); 
	pos = strcspn ( debug_par , "[") ;
	debug_par[pos] = '\0';

	/* Get the parameter index */
	sts = ldh_GetObjectBodyDef(
			gcgctx.ldhses,
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  /* This is a development node, get the development body instead */
	  sts = ldh_GetObjectBodyDef(
			gcgctx.ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) return sts;
	}	
	found = 0;
	for ( i = 0; i < rows; i++ )
	{
	  if ( strcmp( debug_par, bodydef[i].ParName) == 0)
	  {
	    found = 1;
	    par_index = i;
	    break;
	  }
	}	
	if ( !found)
	  return GSX__REFPAR;  

	*par_type = bodydef[par_index].Par->Param.Info.Type;
	memcpy( &debug_bodydef,  &(bodydef[par_index]), 
			sizeof(debug_bodydef)); 
	free((char *) bodydef);

	sts = gcg_get_outputstring( &gcgctx, node, &debug_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	if ( sts == GSX__NEXTPAR ) return GSX__SWINDERR;
	if ( EVEN(sts)) return sts;

	pos = strcspn ( output_par , "[") ;
	output_par[pos] = '\0';
	  
	/* Get the name of the node */
	sts = ldh_AttrRefToName( 
		gcgctx.ldhses,
		&output_attrref, cdh_mNName,
		&name, &size);
	if( EVEN(sts)) return sts;

	strcpy( conn_obj, name);

	/* Get class of the output object */
	sts = ldh_GetAttrRefOrigTid(
			gcgctx.ldhses,
			&output_attrref,
			&cid);
	if( EVEN(sts)) return sts;

	/* Convert the pgmname given in gcg_get_outputstring to parname */
	sts = gcg_pgmname_to_parname( gcgctx.ldhses, cid, 
		output_par, conn_par );
	if( EVEN(sts)) return sts;
	

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_debug_virtual()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* char		*debug_parname	I	pgmname of debug parameter.
* char		*conn_obj	I	name of object associated with debugpar.
* char		*conn_par	I	name of the parameter.
*
* Description:
*	Return the object and parameter for the debugparameter in
*	graphbody for a debugparameter which is a mask RTVIRTUAL
*	$Input.
*	For this type of object the object and parameter to
*	debug is the one the debugparameter is connected to by a 
*	vldh connection.
*	First the parameter and the connection point of this parameter is 
*	identified. Then a output connected to this point is returned.
*
**************************************************************************/

int gcg_get_debug_virtual (
  vldh_t_node	node,
  char		*debug_parname,
  char		*conn_obj,
  char		*conn_par,
  pwr_eType	*par_type,
  int		*par_inverted
)
{
	gcg_t_ctx		gcgctx;
	int			i, sts;
	char			debug_par[32];
	size_t			pos;
	ldh_sParDef 		*bodydef;
	int 			rows;
	int			par_index, found;
	unsigned long		point;
	unsigned long		inverted;
	vldh_t_node		conn_node;

	gcgctx.ldhses = (node->hn.wind)->hw.ldhses;
	gcgctx.errorcount = 0;
	gcgctx.warningcount = 0;

	/* remove index from debugparameter if it is an array */
	strcpy( debug_par, debug_parname); 
	pos = strcspn ( debug_par , "[") ;
	debug_par[pos] = '\0';

	/* Get the parameter index */
	sts = ldh_GetObjectBodyDef(
			gcgctx.ldhses,
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	found = 0;
	for ( i = 0; i < rows; i++ )
	{
	  if ( strcmp( debug_par, bodydef[i].ParName) == 0)
	  {
	    found = 1;
	    par_index = i;
	    break;
	  }
	}
	if ( !found)
	  return GSX__REFPAR;

	*par_type = bodydef[par_index].Par->Param.Info.Type;

	/* Get the point for this parameter */
	sts = gcg_get_point( node, par_index, &point, &inverted);
	if ( EVEN(sts)) return sts;	

	*par_inverted = inverted;

	/* Get the connected output */
	sts = gcg_get_connected_parameter( node, point, &conn_node, conn_obj,
		conn_par);
	if ( EVEN(sts)) return sts;

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_parname_to_pgmname()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tObjid	cid		I	class.
* char		*parname	I	parameter name.
* char		*pgmname	O	pgm name.
*
* Description:
*	Converts a parametername to a pgmname.
*
**************************************************************************/

static int	gcg_parname_to_pgmname( 
    ldh_tSesContext ldhses,
    pwr_tClassId	cid,
    char	*parname,
    char	*pgmname
)
{
	ldh_sParDef 		*bodydef;
	int 			rows;
	int			sts, i, par_index, found;
	char			*s;
	char			indexstr[10];
	char			superstr[80];

	/* Get the parameter index */
	sts = ldh_GetObjectBodyDef(
			ldhses,
			cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) {
	  /* This is a development node, get the development body instead */
	  sts = ldh_GetObjectBodyDef(
			ldhses,
			cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) {
	    /* This is a system node, get the system body instead */
	    sts = ldh_GetObjectBodyDef(
			ldhses,
			cid, "SysBody", 1, 
			&bodydef, &rows);
	    if ( EVEN(sts) ) return sts;
	  }	
	}	
	/* If the pgmname contains an index, store the index */
	s = strchr( parname, '[');
	if ( s != 0) {
	  strcpy( indexstr, s);
	  *s = 0;
	}	
	else
	  strcpy( indexstr, "");

	found = 0;
	for ( i = 0; i < rows; i++ ) {
	  s = bodydef[i].ParName;
	  strcpy( superstr, "");
	  while ( strncmp( s, "Super.", 6) == 0) {
	    strcat( superstr, "Super.");
	    s += 6;
	  }	 
	  
	  if ( strcmp( parname, s) == 0) {
	    found = 1;
	    par_index = i;
	    break;
	  }
	}	
	if ( !found)
	  return GSX__REFPAR;  


	strcpy( pgmname, superstr);
	strcat( pgmname, bodydef[par_index].Par->Param.Info.PgmName);

	free((char *) bodydef);

	/* Copy the index string ????*/
	strcat( pgmname, indexstr);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_pgmname_to_parname()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tClassId	cid		I	class.
* char		*pgmname	I	pgmname.
* char		*parname	O	parameter name.
*
* Description:
*	Converts a pgmname to a parametername.
*
**************************************************************************/

static int	gcg_pgmname_to_parname( 
    ldh_tSesContext ldhses,
    pwr_tClassId	cid,
    char	*pgmname,
    char	*parname
)
{
	ldh_sParDef 		*bodydef;
	int 			rows;
	int			sts, i, par_index, found;
	char			*s;
	char			indexstr[10];
	char			pname[80];

	/* Suppress any leading Super */
	cdh_SuppressSuper( pname, pgmname);

	/* Get the parameter index */
	sts = ldh_GetObjectBodyDef(
			ldhses,
			cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  /* This is a development node, get the development body instead */
	  sts = ldh_GetObjectBodyDef(
			ldhses,
			cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) return sts;
	}	
	/* If the pgmname contains an index, store the index */
	s = strchr( pname, '[');
	if ( s != 0)
	{
	  strcpy( indexstr, s);
	  *s = 0;
	}	
	else
	  strcpy( indexstr, "");

	found = 0;
	for ( i = 0; i < rows; i++ )
	{
	  if ( strcmp( pname, bodydef[i].Par->Param.Info.PgmName) == 0)
	  {
	    found = 1;
	    par_index = i;
	    break;
	  }
	}	
	if ( !found)
	  return GSX__REFPAR;  


	/* Suppress any leading Super */
	cdh_SuppressSuper( parname, bodydef[par_index].ParName);
	s = bodydef[par_index].ParName;

	free((char *) bodydef);

	/* Copy the index string ????? */
/*	strcpy( parname, indexstr);	*/

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_wind_comp_all()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	ldhwb		I	ldh workbench.
* ldh_tSesContext ldhses		I	ldh session.
* unsigned long	window		I	vldh window.
* unsigned long	codetype	I	generate code or syntax control only.
* unsigned long	modified	I	only modified windows will be compiled.
*
* Description:
*	Compile the window and all subwindows.
*	This routine fetches all subwindows to the window.
*	Every window is loaded to vldh and compiled.
*	The plc is also loaded and compiled.
*
**************************************************************************/

static pwr_tBoolean IsWindowLoaded (
  vldh_t_wind     loaded_windlist[],
  int		  loaded_windcount,	  
  pwr_tObjid	  object,
  vldh_t_wind     *wind
) {
  int i;
  
  for (i = 0; i < loaded_windcount; i++) {
    if ( cdh_ObjidIsEqual( loaded_windlist[i]->lw.oid, object)) {
      *wind = loaded_windlist[i];
      return 1;
    }
  }
  return 0;
}

int	gcg_wind_comp_all( 
  ldh_tWBContext  ldhwb,
  ldh_tSesContext ldhses,
  pwr_tObjid	window,
  unsigned long	codetype,
  int		modified,
  int		debug
)
  {
	pwr_tObjid	*windlist;
	unsigned long	wind_count;
	pwr_tObjid	*windlist_ptr;
	int		sts, j, i;
	vldh_t_wind	wind;
	vldh_t_wind	parentwind;
	vldh_t_plc	plc;
	vldh_t_node	node;
	pwr_tObjid	*parentlist;
	unsigned long	parent_count = 0;
	unsigned long	errorcount;
	unsigned long	warningcount;
	int		wind_compiled;
	vldh_t_wind	*loaded_windlist;
	int		*loaded_list;
	int		loaded_windcount;		
	pwr_tObjid	parent;
	pwr_tStatus	sumsts = GSX__SUCCESS;

	sts = trv_get_window_windows( ldhses, window, &wind_count, 
			&windlist);
	if ( EVEN(sts)) return sts;

	/* Store the windows in the order they are loaded, and unload them 
           later in the opposite order (make space for possible parent windows) */
	loaded_windlist = (vldh_t_wind *)calloc( 20 * wind_count, sizeof( vldh_t_wind));
	loaded_list = (int *)calloc( 20 * wind_count, sizeof(int));
	loaded_windcount = 0;

	wind_compiled = 0;
	windlist_ptr = windlist;
	for ( j = 0; j < (int)wind_count; j++) {

	  /* Check that the window still exist, if Func subwindow it has
	     been replaced and has already been compiled during parent window
	     compilation */
	  sts = ldh_GetParent( ldhses, *windlist_ptr, &parent);
	  if ( sts == LDH__NOSUCHOBJ) {
	    windlist_ptr++;
	    continue;
	  }
	  else if ( EVEN(sts)) return sts;

	  /* Check if this window is modified */
	  if ( modified) {
	    sts = gcg_wind_check_modification( ldhses, *windlist_ptr);
	    if (ODD(sts)) {
	      /* This object is ok, take the next one */
	      windlist_ptr++;
	      continue;
	    }
	  }
	  /* Get the parentlist for this window */
	  sts = trv_get_parentlist( ldhses, *windlist_ptr, &parent_count, 
		&parentlist);
	  if ( EVEN(sts)) return sts;

	  /* Check if the plcpgm is loaded in vldh */
	  sts = vldh_get_plc_objdid( *(parentlist + parent_count -1), &plc);
	  if ( sts == VLDH__OBJNOTFOUND ) {
	    /* Load the plcpgm */
	    sts = vldh_plc_load( *(parentlist + parent_count -1), 
		ldhwb, ldhses, &plc);
	  }
	  else
	    if ( EVEN(sts)) return sts;
	    
	  for ( i = parent_count - 2; i >= 0; i -= 2) {

	    /* Check if this window is loaded */
	    if (!IsWindowLoaded(loaded_windlist, loaded_windcount, *(parentlist + i),  &wind)) {
	      sts = vldh_get_wind_objdid( *(parentlist + i), &wind); 
	      if ( sts == VLDH__OBJNOTFOUND ) {
		/* Load the window */
		if ( i == (int)( parent_count - 2)) {
		  /* This is the child to the plc */
		  sts = vldh_wind_load( plc, 0, *(parentlist + i), 0, &wind,
					ldh_eAccess_SharedReadWrite);
		  if ( EVEN(sts)) return sts;
		  plc->hp.wind = wind;
		}
		else {
		  /* Get the parent vldhnode */
		  sts = vldh_get_node_objdid( *(parentlist + i + 1), parentwind, 
					      &node);
		  if ( EVEN(sts)) return sts;
		  
		  sts = vldh_wind_load( plc, node, *(parentlist + i), 0,
					&wind, ldh_eAccess_SharedReadWrite);
		  if ( EVEN(sts)) return sts;
		}
		sts = vldh_wind_load_all( wind);
		if ( EVEN(sts)) return sts;

		*(loaded_windlist + loaded_windcount) = wind;
		*(loaded_list + loaded_windcount) = 1;
		loaded_windcount++;
	      }
	    }
	    else
	      if ( EVEN(sts)) return sts;

	    parentwind = wind;
	  }
  	  /* Load the last window */
	  /* Check if this window is loaded */
#if 0
	  sts = vldh_get_wind_objdid( *windlist_ptr, &wind); 
	  if ( sts == VLDH__OBJNOTFOUND )
#else
	  if (!IsWindowLoaded(loaded_windlist, loaded_windcount, *windlist_ptr,  &wind))
#endif
	  {
	    if ( parent_count == 1) {
	      /* This is the child to the plc */
	      sts = vldh_get_wind_objdid( *windlist_ptr, &wind); 
	      if ( sts == VLDH__OBJNOTFOUND ) {
		sts = vldh_wind_load( plc, 0, *windlist_ptr, 0, &wind, 
				      ldh_eAccess_SharedReadWrite);
		if ( EVEN(sts)) return sts;
		plc->hp.wind = wind;

		sts = vldh_wind_load_all( wind);
		*(loaded_list + loaded_windcount) = 1;
	      }
	      else {
		ldh_sSessInfo		info;

		sts = ldh_GetSessionInfo( wind->hw.ldhses, &info);
		if ( EVEN(sts)) return sts;

		if ( info.Access != ldh_eAccess_ReadOnly)
		  return LDH__OTHERSESS;

		sts = ldh_SetSession( wind->hw.ldhses, ldh_eAccess_SharedReadWrite);
		if ( EVEN(sts)) return sts;
	      }
	    }
	    else {
	      /* Get the parent vldhnode */
	      sts = vldh_get_node_objdid( *parentlist, parentwind,  
			&node);
	      if ( EVEN(sts)) return sts;
	       
	      sts = vldh_get_wind_objdid( *windlist_ptr, &wind); 
	      if ( sts == VLDH__OBJNOTFOUND ) {
		sts = vldh_wind_load( plc, node, *windlist_ptr, 0, &wind,
				      ldh_eAccess_SharedReadWrite);
		if ( EVEN(sts)) return sts;
		*(loaded_list + loaded_windcount) = 1;

		sts = vldh_wind_load_all( wind);
	      }
	      else {
		ldh_sSessInfo		info;

		sts = ldh_GetSessionInfo( wind->hw.ldhses, &info);
		if ( EVEN(sts)) return sts;

		if ( info.Access != ldh_eAccess_ReadOnly)
		  return LDH__OTHERSESS;

		sts = ldh_SetSession( wind->hw.ldhses, ldh_eAccess_SharedReadWrite);
		if ( EVEN(sts)) return sts;
	      }
	    }
	    if ( EVEN(sts)) return sts;

	    *(loaded_windlist + loaded_windcount) = wind;
	    loaded_windcount++;
	  }

	  /* Compile the window */
	  sts = gcg_plcwindow_compile( wind, codetype, &errorcount, 
			&warningcount, 1, debug);
	  if ( sts == GSX__PLCWIND_ERRORS || sts == GSX__AMBIGOUS_EXECUTEORDER) {
	    /* continue */
	    sumsts = sts;
	  }
	  else if ( EVEN(sts)) return sts;
	  else
	    wind_compiled = 1;

	  windlist_ptr++;
	}

	/* Compile the plc */


	/* FIX, the plc is unloaded by the Func objects, load it again... */
	if ( parent_count) {
	  sts = vldh_get_plc_objdid( *(parentlist + parent_count -1), &plc);
	  if ( sts == VLDH__OBJNOTFOUND ) {
	    /* Load the plcpgm */
	    sts = vldh_plc_load( *(parentlist + parent_count -1), 
				 ldhwb, ldhses, &plc);
	  }
	  else
	    if ( EVEN(sts)) return sts;
	}
        /* End of FIX */


	if ( wind_compiled) {
	  sts = gcg_plc_compile( plc, codetype, &errorcount, &warningcount, 1,
			debug);
	  if ( sts == GSX__PLCPGM_ERRORS) {
	    /* continue */
	    sumsts = sts;
	  }
	  else if ( EVEN(sts)) return sts;
	}

	/* Unload the loaded windows */
	for ( i = 0; i < loaded_windcount; i++) {
	  if ( *(loaded_list + loaded_windcount - i - 1)) {
	    sts = vldh_wind_quit_all( 
		   *(loaded_windlist + loaded_windcount - i - 1));
	    if ( EVEN(sts)) return sts;
	  }
	  else {
	    ldh_sSessInfo		info;

	    sts = ldh_GetSessionInfo( wind->hw.ldhses, &info);
	    if ( EVEN(sts)) return sts;

	    if ( !info.Empty) {
	      sts = ldh_RevertSession( wind->hw.ldhses);
	      if ( EVEN(sts)) return sts;
	    }

	    sts = ldh_SetSession( wind->hw.ldhses, ldh_eAccess_ReadOnly);
	    if ( EVEN(sts)) return sts;
	  }
	}
	free((char *) loaded_windlist);
	free((char *) loaded_list);

	if ( !wind_compiled)
	  return GSX__NOMODIF;
	return sumsts;
}


/*************************************************************************
*
* Name:		gcg_get_output()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* unsigned long	point		I	connectionpoint on node.
* unsigned long	*output_count	O	number of found connected outputs
* vldh_t_node	*output_node	O	found node connected with an output
*					(last node).
* unsigned long	*output_point	O	output connectionpoint on output_node
* ldh_sParDef 	*output_bodydef	O	bodydef of the corresponding parameter
*					of output_point.
*
* Description:
*	The routine searches though all connected nodes connected
*	to a connectionpoint on a node, looking for an output.
*	The output is returned (node, connectionpoint and corresponding
*	parameter) and the number of found outputs.
*	Normally if this routine is called only one output is expected
*	and more than one is an error.
*	If more than one outputs is found the last one is returned.
*
**************************************************************************/

int	gcg_get_output (
  vldh_t_node	node,
  unsigned long	point,
  unsigned long	*output_count,
  vldh_t_node	*output_node,
  unsigned long	*output_point,
  ldh_sParDef 	*output_bodydef,
  unsigned long	conmask
)
{
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	unsigned long		next_par_type;
	unsigned long		next_par_inverted;
	unsigned long		next_par_index;
	int			k, sts;
	ldh_sParDef 		*bodydef;
	int 			rows;

	gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, conmask);

	*output_count = 0;
	for ( k = 1; k < (int)point_count; k++)
	{
	  next_node = (pointlist + k)->node;
	  next_point = (pointlist + k)->conpoint;
	  sts = goen_get_parameter( next_node->ln.cid, 
		(next_node->hn.wind)->hw.ldhses, 
		next_node->ln.mask, next_point, 
		&next_par_type, &next_par_inverted, &next_par_index);
	  if ( EVEN(sts)) return sts;

	  if ( next_par_type == PAR_OUTPUT )
	  {
	    (*output_count)++;
	    *output_node = (pointlist + k)->node;
	    *output_point = (pointlist + k)->conpoint;
	    sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	    if ( EVEN(sts) )
	    {
	      /* This is a development node, get the development body instead */
	      sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	      if ( EVEN(sts) ) return sts;
	    }	
	    memcpy( output_bodydef,  &(bodydef[next_par_index]), 
			sizeof(*output_bodydef)); 
	    free((char *) bodydef);
	  }
	}
	if (  point_count > 0) free((char *) pointlist);
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_par_close()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* unsigned long	point		I	connectionpoint on node.
* unsigned long	*output_count	O	number of found connected outputs
* vldh_t_node	*output_node	O	found node connected with an output
*					(last node).
* unsigned long	*output_point	O	output connectionpoint on output_node
* ldh_sParDef 	*output_bodydef	O	bodydef of the corresponding parameter
*					of output_point.
*
* Description:
*	The routine searches though all connected nodes connected
*	to a connectionpoint on a node, looking for an output.
*	The output is returned (node, connectionpoint and corresponding
*	parameter) and the number of found outputs.
*	Normally if this routine is called only one output is expected
*	and more than one is an error.
*	If more than one outputs is found the last one is returned.
*
**************************************************************************/

static int	gcg_get_par_close( 
    vldh_t_node	node,
    unsigned long	point,
    unsigned long	*output_count,
    vldh_t_node	*output_node,
    unsigned long	*output_point,
    ldh_sParDef 	*output_bodydef,
    unsigned long	conmask,
    unsigned long	*par_type
)
{
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	unsigned long		next_par_type;
	unsigned long		next_par_inverted;
	unsigned long		next_par_index;
	int			k, sts;
	ldh_sParDef 		*bodydef;
	int 			rows;

	sts = vldh_get_conpoint_nodes_close( node, point, 
			&point_count, &pointlist, conmask);
	if ( EVEN(sts)) return sts;

	*output_count = point_count - 1;
	if ( *output_count == 0)
	{
	  /* Nothing connected */
	  if ( point_count > 0)
	    free((char *) pointlist);
	  return GSX__SUCCESS;
	}

	k = 1;

	next_node = (pointlist + k)->node;
	next_point = (pointlist + k)->conpoint;
	sts = goen_get_parameter( next_node->ln.cid, 
		(next_node->hn.wind)->hw.ldhses, 
		next_node->ln.mask, next_point, 
		&next_par_type, &next_par_inverted, &next_par_index);
	if ( EVEN(sts)) return sts;

	*par_type = next_par_type;
	*output_node = (pointlist + k)->node;
	*output_point = (pointlist + k)->conpoint;
	sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  /* This is a development node, get the development body instead */
	  sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	  if ( EVEN(sts) ) return sts;
	}	
	memcpy( output_bodydef,  &(bodydef[next_par_index]), 
			sizeof(*output_bodydef)); 
	free((char *) bodydef);
	if ( point_count > 0)
 	  free((char *) pointlist);
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_output()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_node	node		I	vldh node.
* unsigned long	point		I	connectionpoint on node.
* unsigned long	*output_count	O	number of found connected inputs
* vldh_t_node	*output_node	O	found node connected with an input
*					(last node).
* unsigned long	*output_point	O	input connectionpoint on output_node
* ldh_sParDef 	*output_bodydef	O	bodydef of the corresponding parameter
*					of output_point.
*
* Description:
*	The routine searches though all connected nodes connected
*	to a connectionpoint on a node, looking for an input.
*	The input is returned (node, connectionpoint and corresponding
*	parameter) and the number of found inputs.
*	Normally if this routine is called only one input is expected
*	and more than one is an error.
*	If more than one inputs is found the last one is returned.
*
**************************************************************************/

static int	gcg_get_input( 
    vldh_t_node		node,
    unsigned long	point,
    unsigned long	*output_count,
    vldh_t_node		*output_node,
    unsigned long	*output_point,
    ldh_sParDef 	*output_bodydef,
    unsigned long	conmask
)
{
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	unsigned long		next_par_type;
	unsigned long		next_par_inverted;
	unsigned long		next_par_index;
	int			k, sts;
	ldh_sParDef 		*bodydef;
	int 			rows;

	gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, conmask);

	*output_count = 0;
	for ( k = 1; k < (int)point_count; k++)
	{
	  next_node = (pointlist + k)->node;
	  next_point = (pointlist + k)->conpoint;
	  sts = goen_get_parameter( next_node->ln.cid, 
		(next_node->hn.wind)->hw.ldhses, 
		next_node->ln.mask, next_point, 
		&next_par_type, &next_par_inverted, &next_par_index);
	  if ( EVEN(sts)) return sts;

	  if ( next_par_type == PAR_INPUT )
	  {
	    (*output_count)++;
	    *output_node = (pointlist + k)->node;
	    *output_point = (pointlist + k)->conpoint;
	    sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	    if ( EVEN(sts) )
	    {
	      /* This is a development node, get the development body instead */
	      sts = ldh_GetObjectBodyDef(
			(next_node->hn.wind)->hw.ldhses, 
			next_node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	      if ( EVEN(sts) ) return sts;
	    }	
	    memcpy( output_bodydef,  &(bodydef[next_par_index]), 
			sizeof(*output_bodydef)); 
	    free((char *) bodydef);
	  }
	}
	if ( point_count > 0) free((char *) pointlist);
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_ctx_new()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	*gcgctx		O	created gcg context
* vldh_t_wind	wind		I	vldh window.
*
* Description:
*	Create a gcg context for a window.
*
**************************************************************************/

static void	gcg_ctx_new(
    gcg_ctx	    *gcgctx,
    vldh_t_wind	    wind
)
{
	/* Create the context */
	*gcgctx = (gcg_ctx)calloc( 1, sizeof( **gcgctx ) );

	(*gcgctx)->wind = wind;

}
/*************************************************************************
*
* Name:		gcg_ctx_delete()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
*
* Description:
*	Delete a context for a window.
*	Free's all allocated memory in the gcg context.
*
**************************************************************************/

static void	gcg_ctx_delete(
    gcg_ctx		gcgctx
)
{
	/* Free the memory for the ioreadlist, iowritelist, reflist and
	  the context */
	if (gcgctx->ioreadcount > 0)
	  free((char *) gcgctx->ioread);
	if (gcgctx->iowritecount > 0)
	  free((char *) gcgctx->iowrite);
	if (gcgctx->refcount > 0)
	  free((char *) gcgctx->ref);
	free((char *) gcgctx);
}

/*************************************************************************
*
* Name:		gcg_ioread_insert()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_sAttrRef	attrref		I 	attrref of object.
* char		prefix		I	prefix in pointer name.
*
* Description:
*	Insert objdid for a io-node in the ioread list in gcg context.
*	Check first that the objdid is not already inserted.
*
**************************************************************************/

static int	gcg_ioread_insert(
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
)
{
	int	i, found, sts;
	
	/* Check if the objdid already is inserted */
	found = 0;
	for ( i = 0; i < (int)gcgctx->ioreadcount; i++) {
	  if ( cdh_ObjidIsEqual( (gcgctx->ioread + i)->attrref.Objid, attrref.Objid) &&
	       (gcgctx->ioread + i)->attrref.Offset == attrref.Offset &&
	       (gcgctx->ioread + i)->attrref.Size == attrref.Size &&
	       (gcgctx->ioread + i)->prefix == prefix) {
	    found = 1;
	    break;
	  }
	}
	if ( !found ) {
	  /* The attrref was not found, insert it */
	  /* Increase size of the iolist */
	  sts = utl_realloc((char **) &gcgctx->ioread, 
		gcgctx->ioreadcount * sizeof(gcg_t_areflist), 
		(gcgctx->ioreadcount + 1) * sizeof(gcg_t_areflist));
	  if (EVEN(sts)) return sts;

	  (gcgctx->ioread + gcgctx->ioreadcount)->attrref = attrref;
	  (gcgctx->ioread + gcgctx->ioreadcount)->prefix = prefix;
	  gcgctx->ioreadcount++;
	}
	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_iowrite_insert()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_tObjid	objdid		I 	objdid of object.
* char		prefix		I	prefix in pointer name.
*
* Description:
*	Insert attrref for a io-node in the iowrite list in gcg context.
*	Check first that the attrref is not already inserted.
*
*
**************************************************************************/

static int	gcg_iowrite_insert( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
)
{
	int	i, found, sts;
	
	/* Check if the objdid already is inserted */
	found = 0;
	for ( i = 0; i < (int)gcgctx->iowritecount; i++) {
	  if ( cdh_ObjidIsEqual(( gcgctx->iowrite + i)->attrref.Objid, attrref.Objid) &&
	       (gcgctx->iowrite + i)->attrref.Offset == attrref.Offset &&
	       (gcgctx->iowrite + i)->attrref.Size == attrref.Size &&
	       (gcgctx->iowrite + i )->prefix == prefix) {
	    found = 1;
	    break;
	  }
	}
	if ( !found ) {
	  /* The objdid was not found, insert it */
	  /* Increase size of the iolist */
	  sts = utl_realloc((char **) &gcgctx->iowrite, 
		gcgctx->iowritecount * sizeof(gcg_t_areflist), 
		(gcgctx->iowritecount + 1) * sizeof(gcg_t_areflist));
	  if (EVEN(sts)) return sts;
	  (gcgctx->iowrite + gcgctx->iowritecount)->attrref = attrref;
	  (gcgctx->iowrite + gcgctx->iowritecount)->prefix = prefix;
	  gcgctx->iowritecount++;
	}
	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_ref_insert()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_tObjid	objdid		I 	objdid of object.
* char		prefix		I	prefix in pointer name.
*
* Description:
*	Insert objdid for a node in the ref list in gcg context.
*	Check first that the objdid is not already inserted.
*
**************************************************************************/

static int	gcg_ref_insert( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		prefix
)
{
	int	i, found, sts;

	/* Check if the objdid already is inserted */
	found = 0;
	for ( i = 0; i < (int)gcgctx->refcount; i++)
	{
	  if ( cdh_ObjidIsEqual( (gcgctx->ref + i )->objdid, objdid))
	  {
	    found = 1;
	    break;
	  }
	}
	if ( !found )
	{
	  /* The objdid was not found, 
	    increase size of the reflist and insert it */
	  sts = utl_realloc((char **) &gcgctx->ref, 
		gcgctx->refcount * sizeof(gcg_t_reflist), 
		(gcgctx->refcount + 1) * sizeof(gcg_t_reflist));
	  if ( EVEN(sts)) return sts;
	  (gcgctx->ref + gcgctx->refcount)->objdid = objdid;
	  (gcgctx->ref + gcgctx->refcount)->prefix = prefix;
	  gcgctx->refcount++;
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_aref_insert()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_sAttrRef	attrref		I 	attrref of object.
* char		prefix		I	prefix in pointer name.
*
* Description:
*	Insert objdid for a node in the ref list in gcg context.
*	Check first that the objdid is not already inserted.
*
**************************************************************************/

static int	gcg_aref_insert( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix
)
{
	int	i, found, sts;

	/* Check if the objdid already is inserted */
	found = 0;
	for ( i = 0; i < (int)gcgctx->arefcount; i++) {
	  if ( cdh_ObjidIsEqual( (gcgctx->aref + i)->attrref.Objid, attrref.Objid) &&
	       (gcgctx->aref + i)->attrref.Offset == attrref.Offset &&
	       (gcgctx->aref + i)->attrref.Size == attrref.Size) {
	    found = 1;
	    break;
	  }
	}
	if ( !found ) {
	  /* The attrref was not found, 
	    increase size of the reflist and insert it */
	  sts = utl_realloc((char **) &gcgctx->aref, 
		gcgctx->arefcount * sizeof(gcg_t_areflist), 
		(gcgctx->arefcount + 1) * sizeof(gcg_t_areflist));
	  if ( EVEN(sts)) return sts;
	  (gcgctx->aref + gcgctx->arefcount)->attrref = attrref;
	  (gcgctx->aref + gcgctx->arefcount)->prefix = prefix;
	  gcgctx->arefcount++;
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_ref_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
*
* Description:
*	Print the ref list.
*	Prints pointer declarations and rtdb directlink code
*	for the objects in ref list of gcg context.
*
**************************************************************************/

static int	gcg_ref_print( 
    gcg_ctx		gcgctx
)
{
	int		i, sts;
	pwr_tObjid	objdid;
	char		prefix;
	pwr_tClassId	cid;
	char		*name;

	/* Check if the objdid already is inserted */
	for ( i = 0; i < (int)gcgctx->refcount; i++)
	{
	  objdid = (gcgctx->ref + i)->objdid;
	  prefix = (gcgctx->ref + i)->prefix;

	  sts = ldh_GetObjectClass(gcgctx->wind->hw.ldhses, objdid, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Get name for this class */
	  sts = gcg_get_structname( gcgctx, objdid, &name);
	  if( EVEN(sts)) return sts;

	  gcg_print_decl( gcgctx, name, objdid, prefix, GCG_REFTYPE_REF);
	  gcg_print_rtdbref( gcgctx, objdid, prefix, cid, GCG_REFTYPE_REF);
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_aref_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
*
* Description:
*	Print the aref list.
*	Prints pointer declarations and rtdb directlink code
*	for the objects in ref list of gcg context.
*
**************************************************************************/

static int	gcg_aref_print( 
    gcg_ctx		gcgctx
)
{
	int		i, sts;
	pwr_sAttrRef	attrref;
	char		prefix;
	pwr_tClassId	cid;
	char		*name;

	/* Check if the objdid already is inserted */
	for ( i = 0; i < (int)gcgctx->arefcount; i++) {
	  attrref = (gcgctx->aref + i)->attrref;
	  prefix = (gcgctx->aref + i)->prefix;

	  sts = ldh_GetAttrRefOrigTid(gcgctx->wind->hw.ldhses, &attrref, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Get name for this class */
	  sts = gcg_get_structname_from_cid( gcgctx, cid, &name);
	  if( EVEN(sts)) return sts;

	  gcg_print_adecl( gcgctx, name, attrref, prefix, GCG_REFTYPE_REF);
	  gcg_print_artdbref( gcgctx, attrref, prefix, cid, GCG_REFTYPE_REF);
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_ioread_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
*
* Description:
*	Print the ioread list.
*	Prints pointer declarations and rtdb directlink code
*	for the objects in ioread list of gcg context.
*
**************************************************************************/

static int	gcg_ioread_print(
    gcg_ctx		gcgctx
)
{
	int		i, sts;
	pwr_sAttrRef	attrref;
	char		prefix;
	pwr_tClassId	cid;
	char		*name;

	for ( i = 0; i < (int)gcgctx->ioreadcount; i++)
	{
	  attrref = (gcgctx->ioread + i)->attrref;
	  prefix = (gcgctx->ioread + i)->prefix;

	  sts = ldh_GetAttrRefOrigTid(gcgctx->wind->hw.ldhses, &attrref, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Get name for this class */
	  sts = gcg_get_structname_from_cid( gcgctx, cid, &name);
	  if( EVEN(sts)) return sts;

	  gcg_print_adecl( gcgctx, name, attrref, prefix, GCG_REFTYPE_IOR);
	  if ( prefix == GCG_PREFIX_IOC )
	    gcg_print_artdbref( gcgctx, attrref, prefix, cid, GCG_REFTYPE_IOC);
	  else
	    gcg_print_artdbref( gcgctx, attrref, prefix, cid, GCG_REFTYPE_IOR);
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_iowrite_print()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
*
* Description:
*	Print the iowrite list.
*	Prints pointer declarations and rtdb directlink code
*	for the objects in iowrite list of gcg context.
*
**************************************************************************/

static int	gcg_iowrite_print(
    gcg_ctx		gcgctx
)
{
	int		i, sts;
	pwr_sAttrRef	attrref;
	char		prefix;
	pwr_tClassId	cid;
	char		*name;

	for ( i = 0; i < (int)gcgctx->iowritecount; i++)
	{
	  attrref = (gcgctx->iowrite + i)->attrref;
	  prefix = (gcgctx->iowrite + i)->prefix;

	  sts = ldh_GetAttrRefOrigTid(gcgctx->wind->hw.ldhses, &attrref, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Get name for this class */
	  sts = gcg_get_structname_from_cid( gcgctx, cid, &name);
	  if( EVEN(sts)) return sts;

	  gcg_print_adecl( gcgctx, name, attrref, prefix, GCG_REFTYPE_IOW);
	  if ( prefix == GCG_PREFIX_IOCW )
	    gcg_print_artdbref( gcgctx, attrref, prefix, cid, GCG_REFTYPE_IOCW);
	  else
	    gcg_print_artdbref( gcgctx, attrref, prefix, cid, GCG_REFTYPE_IOW);

	}

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_outputstring()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	output_node	I	vldh node
* ldh_sParDef 	*output_bodydef	I	bodydef of output node
* pwr_tObjid	objdid	O	objdid for referenced object.
* char		*parprefix	O	prefix for rtdb pointer.
* char		*parstring	O	pgamname of referenced parameter
*
* Description:
*	This routine is called when an output connectionpoint is found
*	connected to some node and you want to get information to
*	a reference to this parameter in a function call (code producing)
*	or make a direct link to the referenced object (trace).
*	The returned parameter and objdid is calculated:
*	- if the object is treated in gcg_get_outputstring_spec this
*	  routin returns the objdid and parstring.
*	- if the parameter has flag PWR_MASK_DEVBODYREF the content first found
*	found parameter of type objdid in devbody is returned as objdid, the
*	pgmname of the connected parameter is returned as parstring.
*	Ex for a getdi you want to get the objdid of the Di object and
*	the pgmname ActualValue. The ouputparameter in rtbody that is connected
*	has flag PWR_MASK_DEVBODYREF and pgmname ActualValue. In the devbody
*	parameter DiObject the objdid of the Di object is stored and the 
*	type of this parameter is pwr_eType_ObjDId.
*	- default the objdid of the connected object and the pgmname of
*	the connected point is return as parstring.
*
**************************************************************************/

int gcg_get_outputstring ( 
  gcg_ctx	gcgctx,
  vldh_t_node	output_node,
  ldh_sParDef 	*output_bodydef,
  pwr_sAttrRef	*parattrref,
  int		*partype,
  char		*parprefix,
  char		*parstring
)
{
	int		sts, size;
	ldh_sParDef 	*bodydef;
	int 		rows;
	int		i, found;
	pwr_tObjid	*objdid;
	pwr_sAttrRef	*attrref;
	pwr_tClassId	cid;

	    /* Look if this is a specific class with special treatment */
	    sts = gcg_get_outputstring_spec( gcgctx, output_node, 
		output_bodydef, parattrref, partype, parprefix, parstring);
	    if ( EVEN(sts)) return sts;
	    if ( sts == GSX__SPECFOUND ) return GSX__SUCCESS;
	    if ( sts == GSX__NEXTPAR ) return sts;
	
	    /* Special output not found, continue to get the outputstring
	      the default way */

	    if ( output_bodydef->Par->Output.Info.Flags & 
			PWR_MASK_DEVBODYREF )
	    {
	      /* This is a reference to an objdid in the devbody,
		and the parameter in the referenced object has the 
		same name as the name of the parameter in rtbody
		in the referenceobject */
	      sts = ldh_GetObjectBodyDef(
			(output_node->hn.wind)->hw.ldhses, 
			output_node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	      if ( EVEN(sts) ) return sts;
	      found = 0;
	      for ( i = 0; i < rows; i++ ) {
	        if ( bodydef[i].Par->Output.Info.Type == pwr_eType_Objid) {
		  found = 1;
		  /* Get the objdid stored in the parameter */
	          sts = ldh_GetObjectPar( 
			(output_node->hn.wind)->hw.ldhses,  
			output_node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&objdid, &size); 
		  if ( EVEN(sts)) return sts;

		  /* Check that this is objdid of an existing object */
		  sts = ldh_GetObjectClass(
			(output_node->hn.wind)->hw.ldhses, 
			*objdid,
			&cid);
		  if ( EVEN(sts)) {
		    gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
	  	    free((char *) objdid);
		    return GSX__NEXTPAR;
		  }
	  	  strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	          if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	            strncat( parstring, "[0]", 80);
		  *parattrref = cdh_ObjidToAref( *objdid);
		  *partype = GCG_OTYPE_OID;
		  free((char *) objdid);
	          break;
		}
	        else if ( bodydef[i].Par->Output.Info.Type == pwr_eType_AttrRef) {
		  found = 1;
		  /* Get the objdid stored in the parameter */
	          sts = ldh_GetObjectPar( 
			(output_node->hn.wind)->hw.ldhses,  
			output_node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&attrref, &size); 
		  if ( EVEN(sts)) return sts;

		  sts = gcg_replace_ref( gcgctx, attrref, output_node);
		  if ( EVEN(sts)) return sts;

		  /* Check that this is objdid of an existing object */
		  sts = ldh_GetAttrRefOrigTid(
			(output_node->hn.wind)->hw.ldhses, 
			attrref,
			&cid);
		  if ( EVEN(sts)) {
		    gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
	  	    free((char *) attrref);
		    return GSX__NEXTPAR;
		  }
	  	  strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	          if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	            strncat( parstring, "[0]", 80);
		  *parattrref = *attrref;
		  *partype = GCG_OTYPE_AREF;
		  free((char *) attrref);
	          break;
		}
	      }
	      free((char *) bodydef);
	      if ( !found )
	        return GSX__CLASSREF;
	    }
	    else
	    {
	      /* Return the objdid as a string and the name of
		the parameter */
	      strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	      if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	        strncat( parstring, "[0]", 80);

	      /* Convert objdid to ascii */
	      *parattrref = cdh_ObjidToAref( output_node->ln.oid);
	      *partype = GCG_OTYPE_OID;
	    }		
	    *parprefix = GCG_PREFIX_REF;
	    return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_get_outputstring_spec()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	output_node	I	vldh node
* ldh_sParDef 	*output_bodydef	I	bodydef of output node
* pwr_tObjid	*parobjdid	O	objdid for referenced object.
* char		*parprefix	O	prefix for rtdb pointer.
* char		*parstring	O	pgamname of referenced parameter
*
* Description:
*	This routine is called when an output connectionpoint is found
*	connected to some node and you want to get information to
*	a reference to this parameter in a function call (code producing)
*	or make a direct link to the referenced object (trace).
*	If the object is treated in this routine the status GSX__SPECFOUND
*	is returned.
*	The following objects is treated:
*	- getdp, objdid from parameter DpObject and pgmname in parameter
*	  Parameter.
*	- getap, objdid from parameter ApObject and pgmname in parameter
*	  Parameter.
*	- getpi, objdid from parameter CoObject and pgmname from the 
*	  connected parameter. If the connected parameter is "PulsIn"
*	  GCG_PREFIX_IOC is returned as parprefix else GCG_PREFIX_REF.
*
**************************************************************************/

static int	gcg_get_outputstring_spec( 
    gcg_ctx		gcgctx,
    vldh_t_node		output_node,
    ldh_sParDef 	*output_bodydef,
    pwr_sAttrRef       	*parattrref,
    int			*partype,
    char		*parprefix,
    char		*parstring
)
{
  int		sts, size;
  pwr_sAttrRef	*attrref;
  pwr_tClassId	cid;
  ldh_tSesContext ldhses;
  char 		*name_p;
  pwr_tAName     	aname;
  char 		*s;
	
  ldhses = (output_node->hn.wind)->hw.ldhses;

  switch ( output_node->ln.cid) {

  case pwr_cClass_GetDp: {
      
    /**********************************************************
     *  GETDP
     ***********************************************************/	
    
    /* Get the objdid stored in the parameter */
    sts = ldh_GetObjectPar( 
			ldhses,  
			output_node->ln.oid, 
			"DevBody",
			"DpObject",
			(char **)&attrref, &size); 
    if ( EVEN(sts)) return sts;

    sts = gcg_replace_ref( gcgctx, attrref, output_node);
    if ( EVEN(sts)) return sts;

    /* Check that this is objdid of an existing object */
    sts = ldh_GetAttrRefOrigTid( ldhses, attrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }
    /* Get the attribute name of last segment */
    sts = ldh_AttrRefToName( ldhses, attrref, ldh_eName_ArefVol, 
			     &name_p, &size);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }
    free((char *) attrref);

    strcpy( aname, name_p);
    if ( (s = strrchr( aname, '.')) == 0) { 
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    
    *s = 0;
    sts = ldh_NameToAttrRef( ldhses, aname, parattrref);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = ldh_GetAttrRefOrigTid( ldhses, parattrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = gcg_parname_to_pgmname( ldhses, cid, s+1, parstring);
    if ( EVEN(sts)) return sts;
      
    *parprefix = GCG_PREFIX_REF;
    *partype = GCG_OTYPE_AREF;
    return GSX__SPECFOUND;
  }

  case pwr_cClass_GetAp: {
    /**********************************************************
     *  GETAP
     ***********************************************************/	

    /* Get the objdid stored in the parameter */
    sts = ldh_GetObjectPar( ldhses,  
			output_node->ln.oid, 
			"DevBody",
			"ApObject",
			(char **)&attrref, &size); 
    if ( EVEN(sts)) return sts;

    sts = gcg_replace_ref( gcgctx, attrref, output_node);
    if ( EVEN(sts)) return sts;

    /* Check that this is objdid of an existing object */
    sts = ldh_GetAttrRefOrigTid( ldhses, attrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }

    /* Get the attribute name of last segment */
    sts = ldh_AttrRefToName( ldhses, attrref, ldh_eName_ArefVol, 
			     &name_p, &size);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }
    free((char *) attrref);

    strcpy( aname, name_p);
    if ( (s = strrchr( aname, '.')) == 0) { 
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }

    *s = 0;
    sts = ldh_NameToAttrRef( ldhses, aname, parattrref);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }

    sts = ldh_GetAttrRefOrigTid( ldhses, parattrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }

    sts = gcg_parname_to_pgmname( ldhses, cid, s+1, parstring);
    if ( EVEN(sts)) return sts;

    *parprefix = GCG_PREFIX_REF;
    *partype = GCG_OTYPE_AREF;
    return GSX__SPECFOUND;
  }

  case pwr_cClass_GetPi: {
    /**********************************************************
     *  GETPI
     ***********************************************************/	
    /* Get the objdid stored in the parameter */
    sts = ldh_GetObjectPar( ldhses, output_node->ln.oid, 
			    "DevBody", "CoObject",
			    (char **)&attrref, &size); 
    if ( EVEN(sts)) return sts;
	  
    sts = gcg_replace_ref( gcgctx, attrref, output_node);
    if ( EVEN(sts)) return sts;

    /* Check that this is objdid of an existing object */
    sts = ldh_GetAttrRefOrigTid( ldhses, attrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }

    strcpy( parstring, 
	    (output_bodydef->Par)->Param.Info.PgmName);
    *parattrref = *attrref;
    if ( strcmp( "PulsIn", output_bodydef->ParName) == 0)
      *parprefix = GCG_PREFIX_IOC;
    else
      *parprefix = GCG_PREFIX_REF;

    *partype = GCG_OTYPE_AREF;
    free((char *) attrref);
    return GSX__SPECFOUND;
  }
  case pwr_cClass_GetSp:
  case pwr_cClass_GetATp:
  case pwr_cClass_GetDTp: {
    /**********************************************************
     *  GetSp, GetATP, GetDTp
     ***********************************************************/	

    pwr_tAName aname;

    switch ( output_node->ln.cid) {
    case pwr_cClass_GetSp: strcpy( aname, "SpObject"); break;
    case pwr_cClass_GetATp: strcpy( aname, "ATpObject"); break;
    case pwr_cClass_GetDTp: strcpy( aname, "DTpObject"); break;
    default: ;
    }

    /* Get the objdid stored in the parameter */
    sts = ldh_GetObjectPar( ldhses, output_node->ln.oid, 
			    "DevBody", aname,
			    (char **)&attrref, &size); 
    if ( EVEN(sts)) return sts;
	  
    sts = gcg_replace_ref( gcgctx, attrref, output_node);
    if ( EVEN(sts)) return sts;

    /* Check that this is objdid of an existing object */
    sts = ldh_GetAttrRefOrigTid( ldhses, attrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }

    /* Get the attribute name of last segment */
    sts = ldh_AttrRefToName( ldhses, attrref, ldh_eName_ArefVol, 
			     &name_p, &size);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }
    free((char *) attrref);
    
    strcpy( aname, name_p);
    if ( (s = strrchr( aname, '.')) == 0) { 
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }

    *s = 0;
    sts = ldh_NameToAttrRef( ldhses, aname, parattrref);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = ldh_GetAttrRefOrigTid( ldhses, parattrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = gcg_parname_to_pgmname(ldhses, cid, s+1, parstring);
    if ( EVEN(sts)) return sts;

    *parprefix = GCG_PREFIX_REF;
    *partype = GCG_OTYPE_AREF;
    return GSX__SPECFOUND;
  }
	
  case pwr_cClass_GetDattr:
  case pwr_cClass_GetAattr:
  case pwr_cClass_GetIattr:
  case pwr_cClass_GetSattr: {
    /**********************************************************
     *  GetDattr
     ***********************************************************/	
    pwr_tObjid host_objid;
    char *parameter;

    /* Get host object */
    host_objid = (output_node->hn.wind)->lw.poid;

    sts = ldh_GetObjectClass( ldhses, host_objid, &cid);

    if ( cdh_ObjidIsNull(host_objid)) {	
      /* Parent is a plcprogram */
      gcg_error_msg( gcgctx, GSX__BADWIND, output_node);  
      return GSX__NEXTPAR;
    }

    /* Get the referenced attribute stored in Attribute */
    sts = ldh_GetObjectPar( ldhses, output_node->ln.oid, 
			    "DevBody", "Attribute",
			    (char **)&parameter, &size); 
    if ( EVEN(sts)) return sts;
    
    sts = gcg_parname_to_pgmname(ldhses, cid, parameter, parstring);
    if ( EVEN(sts)) return sts;

    *parattrref = cdh_ObjidToAref( host_objid);
    *parprefix = GCG_PREFIX_REF;
    *partype = GCG_OTYPE_OID;
    free((char *) parameter);
    return GSX__SPECFOUND;
  }

  case pwr_cClass_Disabled: {
    /**********************************************************
     *  Disabled
     ***********************************************************/	

    ldh_sAttrRefInfo info;
    pwr_sAttrRef disaref;

    /* Get the objdid stored in the parameter */
    sts = ldh_GetObjectPar( ldhses, output_node->ln.oid, 
			    "DevBody", "Object",
			    (char **)&attrref, &size); 
    if ( EVEN(sts)) return sts;
	  
    sts = gcg_replace_ref( gcgctx, attrref, output_node);
    if ( EVEN(sts)) return sts;

    /* Check that this is objdid of an existing object */
    sts = ldh_GetAttrRefOrigTid( ldhses, attrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }

    /* Check if DisableAttr is present */
    sts = ldh_GetAttrRefInfo( ldhses, attrref, &info);
    if ( EVEN(sts)) return sts;

    if ( !(info.flags & PWR_MASK_DISABLEATTR)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }

    /* Get DisableAttr attribute */
    disaref = cdh_ArefToDisableAref( attrref);

    /* Get the attribute name of last segment */
    sts = ldh_AttrRefToName( ldhses, &disaref, ldh_eName_ArefVol, 
			     &name_p, &size);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      free((char *) attrref);
      return GSX__NEXTPAR;
    }
    free((char *) attrref);
    
    strcpy( aname, name_p);
    if ( (s = strrchr( aname, '.')) == 0) { 
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }

    *s = 0;
    sts = ldh_NameToAttrRef( ldhses, aname, parattrref);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = ldh_GetAttrRefOrigTid( ldhses, parattrref, &cid);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
      return GSX__NEXTPAR;
    }
    sts = gcg_parname_to_pgmname(ldhses, cid, s+1, parstring);
    if ( EVEN(sts)) return sts;

    *parprefix = GCG_PREFIX_REF;
    *partype = GCG_OTYPE_AREF;
    return GSX__SPECFOUND;
  }
  }

  return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_inputstring()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	output_node	I	vldh node
* ldh_sParDef 	*output_bodydef	I	bodydef of output node
* pwr_tObjid	*parobjdid	O	objdid for referenced object.
* char		*parprefix	O	prefix for rtdb pointer.
* char		*parstring	O	pgamname of referenced parameter
*
* Description:
*	The funktions is mainly the same as gcg_get_outputstring, but this
*	routine is working with inputs instead of outputs.
*	This routine is called when an input connectionpoint is found
*	connected to some node and you want to get information
*	make a direct link to the referenced object (trace).
*	The returned parameter and objdid is calculated:
*	- if the parameter has flag PWR_MASK_DEVBODYREF the content first found
*	found parameter of type objdid in devbody is returned as objdid, the
*	pgmname of the connected parameter is returned as parstring.
*	Ex for a stodo you want to get the objdid of the Do object and
*	the pgmname ActualValue. The inputparameter in rtbody that is connected
*	has flag PWR_MASK_DEVBODYREF and pgmname ActualValue. In the devbody
*	parameter DoObject the objdid of the Do object is stored and the 
*	type of this parameter is pwr_eType_ObjDId.
*	- default the objdid of the connected object and the pgmname of
*	the connected point is return as parstring.
*
*
**************************************************************************/

static int	gcg_get_inputstring( 
    gcg_ctx		gcgctx,
    vldh_t_node		output_node,
    ldh_sParDef 	*output_bodydef,
    pwr_sAttrRef       	*parattrref,
    int			*partype,
    char		*parprefix,
    char		*parstring
)
{
	int		sts, size;
	ldh_sParDef 	*bodydef;
	int 		rows;
	int		i, found;
	pwr_tObjid	*objdid;
	pwr_sAttrRef	*attrref;
	pwr_tClassId	cid;

	    if ( output_bodydef->Par->Output.Info.Flags & 
			PWR_MASK_DEVBODYREF )
	    {
	      /* This is a reference to an objdid in the devbody,
		and the parameter in the referenced object has the 
		same name as the name of the parameter in rtbody
		in the referenceobject */
	      sts = ldh_GetObjectBodyDef(
			(output_node->hn.wind)->hw.ldhses, 
			output_node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	      if ( EVEN(sts) ) return sts;
	      found = 0;
	      for ( i = 0; i < rows; i++ ) {
	        if ( bodydef[i].Par->Output.Info.Type == pwr_eType_Objid) {
		  found = 1;
		  /* Get the objdid stored in the parameter */
	          sts = ldh_GetObjectPar( 
			(output_node->hn.wind)->hw.ldhses,  
			output_node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&objdid, &size); 
		  if ( EVEN(sts)) return sts;

		  /* Check that this is objdid of an existing object */
		  sts = ldh_GetObjectClass(
			(output_node->hn.wind)->hw.ldhses, 
			*objdid,
			&cid);
		  if ( EVEN(sts)) 
		  {
		    gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
	  	    free((char *) objdid);
		    return GSX__NEXTPAR;
		  }
	  	  strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	          if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	            strncat( parstring, "[0]", 80);
		  *parattrref = cdh_ObjidToAref( *objdid);
		  *partype = GCG_OTYPE_OID;
		  free((char *) objdid);
	          break;
		}
	        else if ( bodydef[i].Par->Output.Info.Type == pwr_eType_AttrRef) {
		  found = 1;
		  /* Get the objdid stored in the parameter */
	          sts = ldh_GetObjectPar( 
			(output_node->hn.wind)->hw.ldhses,  
			output_node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&attrref, &size); 
		  if ( EVEN(sts)) return sts;

		  sts = gcg_replace_ref( gcgctx, attrref, output_node);
		  if ( EVEN(sts)) return sts;

		  /* Check that this is objdid of an existing object */
		  sts = ldh_GetAttrRefOrigTid(
			(output_node->hn.wind)->hw.ldhses, 
			attrref,
			&cid);
		  if ( EVEN(sts)) {
		    gcg_error_msg( gcgctx, GSX__REFOBJ, output_node);  
	  	    free((char *) attrref);
		    return GSX__NEXTPAR;
		  }
	  	  strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	          if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	            strncat( parstring, "[0]", 80);
		  *parattrref = *attrref;
		  *partype = GCG_OTYPE_AREF;
		  free((char *) attrref);
	          break;
		}
	      }
	      free((char *) bodydef);
	      if ( !found )
	        return GSX__CLASSREF;
	    }
	    else if ( output_bodydef->Par->Output.Info.Flags & 
			PWR_MASK_RTVIRTUAL )
	    {
	      /* The input doesn't exist in runtime */
	      return GSX__NEXTPAR;
	    }
	    else
	    {
	      /* Return the objdid as a string and the name of
		the parameter */
	      strcpy( parstring, 
			(output_bodydef->Par)->Param.Info.PgmName);
	      if ( output_bodydef->Par->Output.Info.Flags & PWR_MASK_ARRAY)
	        strncat( parstring, "[0]", 80);

	      /* Convert objdid to ascii */
	      *parattrref = cdh_ObjidToAref( output_node->ln.oid);
	      *partype = GCG_OTYPE_OID;
	    }		
	    *parprefix = GCG_PREFIX_REF;
	    return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_print_decl()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gre context
* char		*objname	I	name of c-struct
* pwr_tObjid	objdid		I	objdid of object
* char		prefix		I	prefix of rtdbpointer name.
* unsigned long	reftype		I	iocopied object or not.
*
* Description:
*	Print c declaration of a rtdb pointer in the declaration file
*	(GCGM1_DECL_FILE).
*	For an object that is not iocopied (reftype == GCG_REFTYPE_REF)
*
*	static pwr_sClass_'objname' 'pointername';
*
*	For iocopied objects the c struct is typedefed in 
*	rplc_src:rt_plc_io.h
*
*	static plc_sClass_'objname' 'pointername';
*
**************************************************************************/

static void gcg_print_decl( 
    gcg_ctx	gcgctx,
    char		*objname,
    pwr_tObjid		objdid,
    char		prefix,
    unsigned long	reftype
)
{
	if ( reftype == GCG_REFTYPE_REF )
	{
	  /* Print declaration of the rtdb reference pointer */
	  IF_PR fprintf( gcgctx->files[GCGM1_DECL_FILE], 
		"static pwr_sClass_%s *%c%s;\n",
		objname,
		prefix,
		vldh_IdToStr(0, objdid));
	}
	else
	{
	  /* Print declaration of the valuebase reference pointer */
	  IF_PR fprintf( gcgctx->files[GCGM1_DECL_FILE], 
		"static plc_sClass_%s *%c%s;\n",
		objname,
		prefix,
		vldh_IdToStr(0, objdid));
	}
}

static void gcg_print_adecl( 
    gcg_ctx	gcgctx,
    char		*objname,
    pwr_sAttrRef       	attrref,
    char		prefix,
    unsigned long	reftype
)
{
	if ( reftype == GCG_REFTYPE_REF ) {
	  /* Print declaration of the rtdb reference pointer */
	  IF_PR fprintf( gcgctx->files[GCGM1_DECL_FILE], 
		"static pwr_sClass_%s *%c%s;\n",
		objname,
		prefix,
		vldh_AttrRefToStr(0, attrref));
	}
	else {
	  /* Print declaration of the valuebase reference pointer */
	  IF_PR fprintf( gcgctx->files[GCGM1_DECL_FILE], 
		"static plc_sClass_%s *%c%s;\n",
		objname,
		prefix,
		vldh_AttrRefToStr(0, attrref));
	}
}
/*************************************************************************
*
* Name:		gcg_print_rtdbref()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* pwr_tObjid	objdid		I	objdid of object.
* char		prefix		I 	prefix of rtdb pointer name.
* pwr_tClassId	cid		I	object class.
* unsigned long	reftype		I	iocopied object or not.
*
* Description:
*	Prints code for direct och rtdb pointer in the rtdbreference file
*	(GCGM1_RTDBREF_FILE). The code is initialization of an array 
*	that in input to the routine plc_rtdbref.
*	This line is written:
*		{ &'pointername', 'objdid', 'class', 'reftype' },
*
*	Reftype tells which kind of reference that should be made:
*	GCG_REFTYPE_REF 0	direct link to object
*	GCG_REFTYPE_IOR 1	read pointer of iocopied object (to valuebase
*				object for current frequency of plcprocess).
*	GCG_REFTYPE_IOW 2	write pointer of iocopied object (to valuebase
*				object for base frequency of the plc job.
*	GCG_REFTYPE_IOC 3	special for co object.
*
**************************************************************************/

static void gcg_print_rtdbref( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    char		prefix,
    pwr_tClassId	cid,
    unsigned long	reftype
)
{
	/* Print direct link command */
	if ( reftype == 0) {
	  IF_PR fprintf( gcgctx->files[GCGM1_RTDBREF_FILE], 
		"{ (void **)&%c%s, {{%u,%u},0,0,0,0}, %u,  sizeof(*%c%s), %ld},\n",
		prefix,
		vldh_IdToStr(0, objdid),
		objdid.oix, objdid.vid,
		cid,
		prefix,
		vldh_IdToStr(1, objdid),
		reftype);
	}
	else {
	  IF_PR fprintf( gcgctx->files[GCGM1_RTDBREF2_FILE], 
		"{ (void **)&%c%s, {{%u,%u},0,0,0,0}, %u, sizeof(*%c%s), %ld },\n",
		prefix,
		vldh_IdToStr(0, objdid),
		objdid.oix, objdid.vid,
		cid,
		prefix,
		vldh_IdToStr(1, objdid),
		reftype);

	}
}

static void gcg_print_artdbref( 
    gcg_ctx		gcgctx,
    pwr_sAttrRef       	attrref,
    char		prefix,
    pwr_tClassId	cid,
    unsigned long	reftype
)
{
	/* Print direct link command */
	if ( reftype == 0) {
	  IF_PR fprintf( gcgctx->files[GCGM1_RTDBREF_FILE], 
		"{ (void **)&%c%s, {{%u,%u},%u,%u,%u,%u}, %u,  sizeof(*%c%s), %ld},\n",
		prefix,
		vldh_AttrRefToStr(0, attrref),
	        attrref.Objid.oix, attrref.Objid.vid, attrref.Body,
		attrref.Offset, attrref.Size, attrref.Flags.m,
		cid,
		prefix,
		vldh_AttrRefToStr(1, attrref),
		reftype);
	}
	else {
	  IF_PR fprintf( gcgctx->files[GCGM1_RTDBREF2_FILE], 
		"{ (void **)&%c%s, {{%u,%u},%u,%u,%u,%u}, %u, sizeof(*%c%s), %ld },\n",
		prefix,
		vldh_AttrRefToStr(0, attrref),
	        attrref.Objid.oix, attrref.Objid.vid, attrref.Body,
		attrref.Offset, attrref.Size, attrref.Flags.m,
		cid,
		prefix,
		vldh_AttrRefToStr(1, attrref),
		reftype);

	}
}

/*************************************************************************
*
* Name:		gcg_error_msg()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* unsigned long sts		I	message identifier
* vldh_t_node	node		I	vldh node
*
* Description:
*	Prints a error or warning message for a node and increments the 
*	errorcount or warningcount in the gcg context.
*
**************************************************************************/

static int gcg_error_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    vldh_t_node	node
)
{
	static char 	msg[256];
	int		status, size;
	pwr_tOName	hier_name;
	FILE 		*logfile;

	logfile = NULL;

	if (EVEN(sts))
	{
	  msg_GetMsg( sts, msg, sizeof(msg));

	  if (logfile != NULL)
	    fprintf(logfile, "%s\n", msg);
	  else
	    printf("%s\n", msg);
	  if ( node == 0)
	    msgw_message_sts( sts, 0, 0);
	  else
	  {
	    /* Get the full hierarchy name for the node */
	    status = ldh_ObjidToName( 
		(node->hn.wind)->hw.ldhses, 
		node->ln.oid, ldh_eName_Hierarchy,
		hier_name, sizeof( hier_name), &size);
	    if( EVEN(status)) return status;
	    if (logfile != NULL)
	      fprintf(logfile, "        in object  %s\n", hier_name);
	    else
	      printf("        in object  %s\n", hier_name);
	    msgw_message_plcobject( sts, "   in object", hier_name, node->ln.oid);
	  }
	  if ( gcgctx) {
	    if ( (sts & 2) && !(sts & 1))
	      gcgctx->errorcount++;
	    else if ( !(sts & 2) && !(sts & 1))
	      gcgctx->warningcount++;
	  }
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_wind_msg()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* unsigned long sts		I	message identifier
* vldh_t_node	wind		I	vldh wind
*
* Description:
*	Prints a message for a window. The number of errors and warning
*	found in errorcount and warningcount in gcg context is printed if
*	they are greater than zero.
*
**************************************************************************/

int gcg_wind_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    vldh_t_wind	wind
)
{
	static char 	msg[256];
	int		status, size;
	pwr_tOName	hier_name;
	FILE 		*logfile;

	logfile = NULL;

	msg_GetMsg( sts, msg, sizeof(msg));

	if (logfile != NULL)
	  fprintf(logfile, "%s\n", msg);
	else
	  printf("%s\n", msg);
	if ( wind == 0)
	  msgw_message_sts( sts, 0, 0);
	else {
	  char str[80] = "";

	  /* Get the full hierarchy name for the wind */
	  status = ldh_ObjidToName( 
		wind->hw.ldhses, 
		wind->lw.oid, ldh_eName_Hierarchy,
		hier_name, sizeof( hier_name), &size);
	  if( EVEN(status)) return status;
	  if (logfile != NULL)
	    fprintf(logfile, "        ");
	  else
	    printf("        ");
	  if ( gcgctx->errorcount > 0 )
	  {
	    if (logfile != NULL)
	      fprintf(logfile, "%ld errors ", gcgctx->errorcount);
	    else
	      printf("%ld errors ", gcgctx->errorcount);
	    sprintf( &str[strlen(str)], "%ld errors ", gcgctx->errorcount);
	  }
	  if ( gcgctx->warningcount > 0 )
	  {
	    if (logfile != NULL)
	      fprintf(logfile, "%ld warnings ", gcgctx->warningcount);
	    else
	      printf("%ld warnings ", gcgctx->warningcount);
	    sprintf( &str[strlen(str)], "%ld warnings ", gcgctx->warningcount);
	  }
	  if (logfile != NULL)
	    fprintf(logfile, "in window  %s\n", hier_name);
	  else
	    printf("in window  %s\n", hier_name);
	  sprintf( &str[strlen(str)], " in window");

	  msgw_message_plcobject( sts, str, hier_name, wind->lw.oid);
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_plc_msg()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* unsigned long sts		I	message identifier
* pwr_tObjid	plcobjdid	I	objdid of the plcpgm object.
*
* Description:
*	Prints a message for a plcpgm. If it is a error or warning 
*	message the errorcount and warningcount in gcg context is 
*	incremented.
*
**************************************************************************/

static int gcg_plc_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    pwr_tObjid		plcobjdid
)
{
	static char 	msg[256];
	int		status, size;
	pwr_tOName	hier_name;
	FILE 		*logfile;

	logfile = NULL;

	msg_GetMsg( sts, msg, sizeof(msg));

	if (logfile != NULL)
	  fprintf(logfile, "%s\n", msg);
	else
	  printf("%s\n", msg);
	if ( cdh_ObjidIsNull( plcobjdid))
	  msgw_message_sts( sts, 0, 0);
	else {
	  /* Get the full hierarchy name for the plc */
	  status = ldh_ObjidToName( 
		gcgctx->ldhses,
		plcobjdid, ldh_eName_Hierarchy,
		hier_name, sizeof( hier_name), &size);
	  if( EVEN(status)) return status;
	  if (logfile != NULL)
	    fprintf(logfile, "        in plcprogram  %s\n", hier_name);
	  else
	    printf("        in plcprogram  %s\n", hier_name);
	  msgw_message_object( sts, "   in plcprogram", hier_name, plcobjdid);
	}
	if ( (sts & 2) && !(sts & 1))
	  gcgctx->errorcount++;
	else if ( !(sts & 2) && !(sts & 1))
	  gcgctx->warningcount++;

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_text_msg()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* unsigned long sts		I	message identifier
* char		*text		I	some text.
*
* Description:
*	Prints a message. If it is a error or warning 
*	message the errorcount and warningcount in gcg context is 
*	incremented.
*
**************************************************************************/

static int gcg_text_msg( 
    gcg_ctx		gcgctx,
    unsigned long 	sts,
    char		*text
)
{
	static char msg[256];
	FILE 		*logfile;

	logfile = NULL;

	msg_GetMsg( sts, msg, sizeof(msg));

	if (logfile != NULL)
	  fprintf(logfile, "%s\n", msg);
	else
	{
	  printf("%s\n", msg);
	  if (logfile != NULL)
	    fprintf(logfile, "        %s\n", text);
	  else
	    printf("        %s\n", text);
	}
	if ( (sts & 2) && !(sts & 1))
	  gcgctx->errorcount++;
	else if ( !(sts & 2) && !(sts & 1))
	  gcgctx->warningcount++;

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_node_comp()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Generates code for an object.
*	Calls the compile method for the object.
*	Compilemethod is stored in the parameter compmethod in graphbody
*	of the class object.
*
**************************************************************************/

static int gcg_node_comp( 
    gcg_ctx	gcgctx,
    vldh_t_node	node
)
{
	pwr_tClassId		bodyclass;
	pwr_sGraphPlcNode 	*graphbody;
	int 			sts, size;
	int			compmethod;
	vldh_t_wind		wind;

	wind = node->hn.wind;

	if ( !node->hn.comp_manager) {
	  if ( gcgctx->current_cmanager)
	    gcg_reset_cmanager( gcgctx);

	  /* Get comp method for this node */
	  sts = ldh_GetClassBody(wind->hw.ldhses, node->ln.cid, 
				 "GraphPlcNode", &bodyclass, 
				 (char **)&graphbody, &size);
	  if ( EVEN(sts)) return sts;
	  
	  compmethod = graphbody->compmethod;
	  // printf( "Compiling %s\n", node->hn.name);
	  sts = (gcg_comp_m[ compmethod ]) (gcgctx, node); 
	}
	else {
	  sts = gcg_cmanager_comp( gcgctx, node);
	}
	return sts;
}	

/*************************************************************************
*
* Name:		gcg_get_inputpoint()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
* unsigned long	index		I	index for parameter in objbodydef.
* unsigned long	*pointptr	I	connection point.
* unsigned long	*inverted	I	connection point inverted or not.
*
* Description:
*	This routine returns the connection point for a parameter in an 
*	object if it is an output. 
*	The id of the parameter is index in the object bodydef.
*
**************************************************************************/

int gcg_get_inputpoint (
  vldh_t_node	node,
  unsigned long	index,
  unsigned long	*pointptr,
  unsigned long	*inverted
)
{
	unsigned long	par_inverted;
	unsigned long	par_index;
	unsigned long	par_type;
	unsigned long	point;
	int		par_found;

	  /* Get the point for this parameter if there is one */
	  point = 0;
	  par_found = 0;
	  while ( ODD( goen_get_parameter( node->ln.cid, 
			(node->hn.wind)->hw.ldhses, 
			node->ln.mask, point, 
			&par_type, &par_inverted, &par_index)) 
		&& (par_type == PAR_INPUT))
	  {
	    if ( index == par_index )
	    {
	      par_found = 1;
	      break;
	    }
	    point++;
	  }
	  if ( !par_found )
	  {
	    return GSX__NOPOINT;
	  }
	  else
	  {
	    *pointptr = point;
	    *inverted = par_inverted;
	    return GSX__SUCCESS;
	  }
}

/*************************************************************************
*
* Name:		gcg_get_inputpoint()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
* unsigned long	index		I	index for parameter in objbodydef.
* unsigned long	*pointptr	I	connection point.
* unsigned long	*inverted	I	connection point inverted or not.
*
* Description:
*	This routine returns the connection point for a parameter in an 
*	object (input or output).
*	The id of the parameter is index in the object bodydef.
*
**************************************************************************/

int gcg_get_point ( 
  vldh_t_node	node,
  unsigned long	index,
  unsigned long	*pointptr,
  unsigned long	*inverted
)
{
	unsigned long	par_inverted;
	unsigned long	par_index;
	unsigned long	par_type;
	unsigned long	point;
	int		par_found;

	  /* Get the point for this parameter if there is one */
	  point = 0;
	  par_found = 0;
	  while ( ODD(goen_get_parameter( node->ln.cid, 
			(node->hn.wind)->hw.ldhses, 
			node->ln.mask, point, 
			&par_type, &par_inverted, &par_index)) 
		&& ((par_type == PAR_INPUT) || (par_type == PAR_OUTPUT)))
	  {
	    if ( index == par_index )
	    {
	      par_found = 1;
	      break;
	    }
	    point++;
	  }
	  if ( !par_found )
	  {
	    return GSX__NOPOINT;
	  }
	  else
	  {
	    *pointptr = point;
	    *inverted = par_inverted;
	    return GSX__SUCCESS;
	  }
}


/*************************************************************************
*
* Name:		gcg_get_child_windows()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tObjid	parent_objdid	I	objdid of object.
* unsigned long	*wind_count	IO	number of windows in windlist.
* void		**windlist	IO	found windows.
*
* Description:
*	Routine used by gcg_get_plc_windows to find all the windows in
*	a plcpgm. If a window is found it is added to the windlist.
*	Rekursive funktion which calls itself for all children to the object.
*
**************************************************************************/

static int	gcg_get_child_windows( 
    ldh_tSesContext	ldhses,
    pwr_tObjid		parent_objdid,
    unsigned long	*wind_count,
    pwr_tObjid		**windlist
)
{
	pwr_tClassId		cid;
	pwr_tObjid		objdid;
	int			sts;
	pwr_tObjid		*windlist_pointer;	


	/* Get all the children of this  node */
	sts = ldh_GetChild( ldhses, parent_objdid, &objdid);
	while ( ODD(sts) )
	{
	  /* Check if plc */
	  sts = ldh_GetObjectClass( ldhses, objdid, &cid);
	  if ( cid == pwr_cClass_windowplc ||
	       cid == pwr_cClass_windowcond ||
	       cid == pwr_cClass_windowsubstep ||
	       cid == pwr_cClass_windoworderact) {
	    sts = utl_realloc((char **) windlist, 
		*wind_count * sizeof( *windlist_pointer), 
		(*wind_count + 1) * sizeof( *windlist_pointer));
	    windlist_pointer = *windlist;
	    *(windlist_pointer + *wind_count) = objdid;
	    (*wind_count)++;
	  }

	  /* Check if the children has a window */
	  sts = gcg_get_child_windows( ldhses, objdid, wind_count, 
			windlist); 
	  if ( EVEN(sts)) return sts;
	  sts = ldh_GetNextSibling( ldhses, objdid, &objdid);
	}
	return GSX__SUCCESS;
}



/*************************************************************************
*
* Name:		gcg_get_plc_windows()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tObjid	plcobjdid	I	objdid of an plc object.
* unsigned long	*wind_count	O	number of windows in windlist.
* void		**windlist	O	found windows.
*
* Description:
* 	Returns a list of all windows in a plcpgm.
*	The list should be freed by a free call after use.
*
**************************************************************************/

static int	gcg_get_plc_windows( 
    ldh_tSesContext	ldhses,
    pwr_tObjid		plcobjdid,
    unsigned long	*wind_count,
    pwr_tObjid		**windlist
)
{
	int			sts;

	/* Get all the window objects that has this plcobject as a
	  father */

	*wind_count = 0;
	sts = gcg_get_child_windows( ldhses, plcobjdid, wind_count, 
			windlist); 
	if ( EVEN(sts)) return sts;

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_get_child_plcthread()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	gcgctx		I	gcg context.
* pwr_tObjid	plcobjdid	I	objdid of an plc object.
* pwr_tObjid	rtnode		I	object of class $Node.
* unsigned long	*plc_count	IO	number of plcpgm's in plclist.
* void		**plclist	IO	found plcpgm's.
*
* Description:
*	Routine used by gcg_get_rtnode_plc to get all the plcthreads's that
*	belongs to a rtnode. If a plcthread is found it is added to the threadlist
*	and the parameter scantime is checked and stored in the list.
*	Recursive funktion which calls itself for all children to the object.
*	LibHier hierarchies are not searched.
*
**************************************************************************/

static int	gcg_get_child_plcthread( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*thread_count,
    gcg_t_threadlist	**threadlist
)
{
	pwr_tClassId		cid;
	int			sts, size;
	gcg_t_threadlist	*threadlist_pointer;	
	float			*scantime_ptr;
	pwr_tInt32		*prio_ptr;
	int			timebase;

	/* Get all the children of this  node */
	sts = ldh_GetChild( gcgctx->ldhses, objdid, &objdid);
	while ( ODD(sts) ) {
	  /* Check if plc */
	  sts = ldh_GetObjectClass( gcgctx->ldhses, objdid, &cid);
	  if ( cid == pwr_cClass_PlcThread) {
	    /* Get the scantime */
	    sts = ldh_GetObjectPar( gcgctx->ldhses, objdid, "RtBody", 
			"ScanTime", (char **)&scantime_ptr, &size);
	    if ( EVEN(sts)) return sts;
	
            /* Check the scantime */
	    timebase = (int)((*scantime_ptr) * 1000 + 0.5);
	    if ( (timebase <= 0)
	        || (IS_LYNX(os) && (((timebase / 10) * 10) != timebase))
		 || (IS_LINUX(os) && 0 /* (((timebase / 10) * 10) != timebase) */)
	    	|| (IS_VMS_OR_ELN(os) && (((timebase / 10) * 10) != timebase))
	    )
	    {
	      gcg_plc_msg( gcgctx, GSX__BADSCANTIME, objdid);
	    }
	    else
	    { 
	        
	      /* Get the priority */
	      sts = ldh_GetObjectPar( gcgctx->ldhses, objdid, "RtBody", 
			"Prio",
			(char **)&prio_ptr, &size);
	      if ( EVEN(sts)) return sts;

	      sts = utl_realloc((char **) threadlist, 
		  *thread_count * sizeof(gcg_t_threadlist), 
		  (*thread_count + 1) * sizeof(gcg_t_threadlist));
	      if ( EVEN(sts)) return sts;
	      threadlist_pointer = *threadlist;
	      (threadlist_pointer + *thread_count)->objdid = objdid;
	      (threadlist_pointer + *thread_count)->scantime = *scantime_ptr;
	      (threadlist_pointer + *thread_count)->prio = *prio_ptr;
	      sts = ldh_ObjidToName( gcgctx->ldhses, objdid,
			ldh_eName_VolPath, 
			(threadlist_pointer + *thread_count)->name, 
			sizeof( (threadlist_pointer + *thread_count)->name), 
			&size);
	      if ( EVEN(sts)) return sts;
	      (*thread_count)++;
	      free((char *) prio_ptr);
	    }
	    free((char *) scantime_ptr);
	  }
	  else if (cid == pwr_eClass_LibHier || 
		   cid == pwr_eClass_MountObject)
	  {
	    ; /* Do nothing if it is a LibHier */
	  }
	  else
	  {
	    /* Check if the children is a thread */
	    sts = gcg_get_child_plcthread( gcgctx, objdid, rtnode, os, 
	    		thread_count, threadlist); 
	    if ( EVEN(sts)) return sts;
	  }
	  sts = ldh_GetNextSibling( gcgctx->ldhses, objdid, &objdid);
	}
	return GSX__SUCCESS;
}



/*************************************************************************
*
* Name:		gcg_get_rtnode_plcthread()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	gcgctx		I	gcg context.
* pwr_tObjid	rtnode		I	object of class $Node.
* unsigned long	*thread_count	O	number of thread's in threadlist.
* gcg_t_threadlist **threadlist	O	found thread's.
*
* Description:
*	This routine returns a list of all thread's that is assigned to
*	a rtnode. In the returned list the objdid and the scantime of 
*	the thred is stored. The list should be freed by the caller with
*	an free call.
*
**************************************************************************/

static int	gcg_get_rtnode_plcthread( 
    gcg_ctx		gcgctx,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*thread_count,
    gcg_t_threadlist	**threadlist
)
{
	int			sts;
	pwr_tObjid		objdid;
	pwr_tClassId		cid;

	/* Get all the thread in this volume */

	*thread_count = 0;

	sts = ldh_GetRootList( gcgctx->ldhses, &objdid);
	while ( ODD(sts) ) {
	  sts = ldh_GetObjectClass( gcgctx->ldhses, objdid, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Check that the class of the node object is correct */
	  if ( cid == pwr_cClass_NodeHier) {
	    /* Check if the children is a thread */
	    sts = gcg_get_child_plcthread( gcgctx, objdid, rtnode, os,
	    		thread_count, threadlist);
	    if ( EVEN(sts)) return sts;
	  }
	  sts = ldh_GetNextSibling( gcgctx->ldhses, objdid, &objdid);
	}
	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_get_child_plc()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	gcgctx		I	gcg context.
* pwr_tObjid	plcobjdid	I	objdid of an plc object.
* pwr_tObjid	rtnode		I	object of class $Node.
* unsigned long	*plc_count	IO	number of plcpgm's in plclist.
* void		**plclist	IO	found plcpgm's.
*
* Description:
*	Routine used by gcg_get_rtnode_plc to get all the plcpgm's that
*	belongs to a rtnode. If a plcpgm is found it is added to the plclist
*	and the parameter scantime is checked and stored in the list.
*	Recursive funktion which calls itself for all children to the object.
*	LibHier hierarchies are not searched.
*
**************************************************************************/

static int	gcg_get_child_plc( 
    gcg_ctx		gcgctx,
    pwr_tObjid		objdid,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*plc_count,
    gcg_t_plclist	**plclist
)
{
	pwr_tClassId		cid;
	int			sts, size;
	gcg_t_plclist		*plclist_pointer;	
	pwr_tObjid		*threadobject_ptr;
	pwr_tInt32		*executeorder_ptr;

	/* Get all the children of this  node */
	sts = ldh_GetChild( gcgctx->ldhses, objdid, &objdid);
	while ( ODD(sts) )
	{
	  /* Check if plc */
	  sts = ldh_GetObjectClass( gcgctx->ldhses, objdid, &cid);
	  if ( cid == pwr_cClass_plc) {
	    /* Get the thread */
	    sts = ldh_GetObjectPar( gcgctx->ldhses, objdid, "RtBody",
			"ThreadObject",
			(char **)&threadobject_ptr, &size);
	    if ( EVEN(sts)) return sts;

	    if ( cdh_ObjidIsNull( *threadobject_ptr)) {
	      gcg_plc_msg( gcgctx, GSX__NOTHREAD, objdid);
	    }
	    else {
	      /* Get the execute order */
	      sts = ldh_GetObjectPar( gcgctx->ldhses, objdid, "DevBody", 
			"ExecuteOrder",
			(char **)&executeorder_ptr, &size);
	      if ( EVEN(sts)) return sts;

	      sts = utl_realloc((char **) plclist, 
		  *plc_count * sizeof(gcg_t_plclist), 
		  (*plc_count + 1) * sizeof(gcg_t_plclist));
	      if ( EVEN(sts)) return sts;
	      plclist_pointer = *plclist;
	      (plclist_pointer + *plc_count)->objdid = objdid;
	      (plclist_pointer + *plc_count)->thread = *threadobject_ptr;
	      (plclist_pointer + *plc_count)->executeorder = *executeorder_ptr;
	      sts = ldh_ObjidToName( gcgctx->ldhses, objdid,
			ldh_eName_VolPath, 
			(plclist_pointer + *plc_count)->name, 
			sizeof( (plclist_pointer + *plc_count)->name), 
			&size);
	      if ( EVEN(sts)) return sts;
	      (*plc_count)++;
	      free((char *) executeorder_ptr);
	    }
	    free((char *) threadobject_ptr);
	  }
	  else if (cid == pwr_eClass_LibHier || 
		   cid == pwr_eClass_MountObject)
	  {
	    ; /* Do nothing if it is a LibHier */
	  }
	  else
	  {
	    /* Check if the children is a plc */
	    sts = gcg_get_child_plc( gcgctx, objdid, rtnode, os, 
	    		plc_count, plclist); 
	    if ( EVEN(sts)) return sts;
	  }
	  sts = ldh_GetNextSibling( gcgctx->ldhses, objdid, &objdid);
	}
	return GSX__SUCCESS;
}



/*************************************************************************
*
* Name:		gcg_get_child_plc()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* unsigned long	gcgctx		I	gcg context.
* pwr_tObjid	rtnode		I	object of class $Node.
* unsigned long	*plc_count	O	number of plcpgm's in plclist.
* gcg_t_plclist	**plclist	O	found plcpgm's.
*
* Description:
*	This routine returns a list of all plcpgm's that is assigned to
*	a rtnode. In the returned list the objdid and the scantime of 
*	the plcpgm is stored. The list should be freed by the caller with
*	an free call.
*
**************************************************************************/

static int	gcg_get_rtnode_plc( 
    gcg_ctx		gcgctx,
    pwr_tObjid		rtnode,
    pwr_mOpSys 		os,
    unsigned long	*plc_count,
    gcg_t_plclist	**plclist
)
{
	int			sts;
	pwr_tObjid		objdid;
	pwr_tClassId		cid;

	/* Get all the plcprogram objects that has this rtnode in
	  the node parameter */

	*plc_count = 0;

	sts = ldh_GetRootList( gcgctx->ldhses, &objdid);
	while ( ODD(sts) ) {
	  sts = ldh_GetObjectClass( gcgctx->ldhses, objdid, &cid);
	  if ( EVEN(sts)) return sts;

	  /* Check that the class of the node object is correct */
	  if ( cid == pwr_cClass_PlantHier) {
	    /* Check if the children is a plc */
	    sts = gcg_get_child_plc( gcgctx, objdid, rtnode, os,
	    		plc_count, plclist);
	    if ( EVEN(sts)) return sts;
	  }
	  sts = ldh_GetNextSibling( gcgctx->ldhses, objdid, &objdid);
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_sort_threadlist()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*	This function sorts a threadlist in priority order.
*
**************************************************************************/

static int  gcg_sort_threadlist( 	
    gcg_ctx		gcgctx,
    gcg_t_threadlist	*threadlist,
    unsigned long	size
)
{
	int	i, j;
	gcg_t_threadlist dum;
	gcg_t_threadlist *list_ptr;

	if ( threadlist == 0)
	  return GSX__SUCCESS;

	for ( i = size - 1; i > 0; i--)
	{
	  list_ptr = threadlist;
	  for ( j = 0; j < i; j++)
	  {
	    if ( list_ptr->prio < (list_ptr + 1)->prio)
	    {
	      /* Change order */
	      memcpy( &dum, list_ptr + 1, sizeof( gcg_t_threadlist));
	      memcpy( list_ptr + 1, list_ptr, sizeof( gcg_t_threadlist));
	      memcpy( list_ptr, &dum, sizeof( gcg_t_threadlist));
	    }
	    list_ptr++;
	  }
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_sort_plclist()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*	This function sorts a plclist in execute order order.
*
**************************************************************************/

static int  gcg_sort_plclist( 	
    gcg_ctx		gcgctx,
    gcg_t_plclist	*plclist,
    unsigned long	size
)
{
	int	i, j;
	gcg_t_plclist	dum;
	gcg_t_plclist	*list_ptr;

	if ( plclist == 0)
	  return GSX__SUCCESS;

	for ( i = size - 1; i > 0; i--)
	{
	  list_ptr = plclist;
	  for ( j = 0; j < i; j++)
	  {
	    if ( list_ptr->executeorder > (list_ptr + 1)->executeorder)
	    {
	      /* Change order */
	      memcpy( &dum, list_ptr + 1, sizeof( gcg_t_plclist));
	      memcpy( list_ptr + 1, list_ptr, sizeof( gcg_t_plclist));
	      memcpy( list_ptr, &dum, sizeof( gcg_t_plclist));
	    }
	    list_ptr++;
	  }
	}
	return GSX__SUCCESS;
}


static pwr_tStatus gcg_read_volume_plclist( 
  gcg_ctx	gcgctx,
  pwr_tVolumeId	volid,
  unsigned long	*plc_count, 
  gcg_t_plclist **plclist,
  unsigned long	*thread_count, 
  gcg_t_threadlist **threadlist)
{
	FILE		*file;
	pwr_tFileName	filenames;
	pwr_tFileName	fullfilename;
	char		type[20];
	int		line_count = 0;
	char		line[160];
	char		objid_str[20];
	char		thread_str[20];
	float		scantime;
	int		executeorder;
	int		prio;
	int		sts;
	gcg_t_plclist 	*plclist_pointer;
	gcg_t_threadlist *threadlist_pointer;
	char		name[120];

	sprintf( filenames, "%s%s", gcgmv_filenames[0], 
		vldh_VolumeIdToStr( volid));
	sprintf( fullfilename,"%s%s%s", DATDIR, filenames, DATEXT);
	dcli_translate_filename( fullfilename, fullfilename);
	file = fopen( fullfilename,"r");
	if ( !file )
	  return GSX__NOLOADFILE;

	while( 
	   ODD( sts = utl_read_line( line, sizeof(line), file, &line_count)))
	{
	  if ( strncmp( line, "PlcThread", 9) == 0)
	  {
	    sscanf( line, "%s %s %f %d %s", type, objid_str, &scantime, &prio,
			name);
	    sts = utl_realloc((char **) threadlist, 
		  *thread_count * sizeof(gcg_t_threadlist), 
		  (*thread_count + 1) * sizeof(gcg_t_threadlist));
	    if ( EVEN(sts)) return sts;
	    threadlist_pointer = *threadlist;
	    sts = cdh_StringToObjid( objid_str, 
	  		&(threadlist_pointer + *thread_count)->objdid);
	    if ( EVEN(sts)) return sts;
	    (threadlist_pointer + *thread_count)->scantime = scantime;
	    (threadlist_pointer + *thread_count)->prio = prio;
	    (*thread_count)++;
	  }
	  else
	  {
	    sscanf( line, "%s %s %s %d %s", type, objid_str, thread_str, 
		&executeorder, name);
	    sts = utl_realloc((char **) plclist, 
		  *plc_count * sizeof(gcg_t_plclist), 
		  (*plc_count + 1) * sizeof(gcg_t_plclist));
	    if ( EVEN(sts)) return sts;
	    plclist_pointer = *plclist;
	    sts = cdh_StringToObjid( objid_str, 
	  		&(plclist_pointer + *plc_count)->objdid);
	    if ( EVEN(sts)) return sts;
	    sts = cdh_StringToObjid( thread_str, 
	  		&(plclist_pointer + *plc_count)->thread);
	    (plclist_pointer + *plc_count)->executeorder = executeorder;
	    strcpy( (plclist_pointer + *plc_count)->name, name);
	    (*plc_count)++;
	  }
	}
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_rtnode()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
* pwr_tObjid	rtnode		I	objdid of rtnode.
* vldh_t_plc	plc		I	vldh plc.
* unsigned long	codetype	I	generating code or syntaxcontrol only.
* unsigned long	*errorcount	O	error counter
* unsigned long	*warningcount	O	warning counter
*
* Description:
*	Compile method for a node.
*	This routine generates code for the plcjob of a node.
*	All plcpgm objects in this node with the same scantime
*	is put together in processes. Code is generated for
*	every process and the process module is compiled to objectmodules.
*	An .opt_eln file is also produced for the linking of the plcjob
*	and the linking is done.
*
**************************************************************************/

int	gcg_comp_rtnode( 
    char	    *nodename,
    pwr_mOpSys 	    os,
    pwr_tUInt32	    bus,
    unsigned long   codetype,
    unsigned long   *errorcount,
    unsigned long   *warningcount,
    int		    debug,
    pwr_tVolumeId   *volumelist,
    int		    volume_count,
    unsigned long   plc_version,
    pwr_tFloat32    single_scantime
)
{
	int			i, j, k, l, sts;
	pwr_tFileName  		fullfilename;
	FILE			*files[2];
	char			module_name[80];
	pwr_tFileName	       	plcfilename;
	unsigned long		plc_count;
	gcg_t_plclist		*plclist;
	gcg_t_timebase		*timebase;
	int			timebase_count;
	int			found;
	pwr_tObjid		*timebase_ptr;
	int			timebase_ms;		
	gcg_ctx			gcgctx;
	char			gcdir[80];
	char			objdir[80];
	char			plclib_frozen[80];
	pwr_tVolumeId		*volumelist_ptr;
	char			os_str[20];
	int			max_no_timebase;
	gcg_t_threadlist	*threadlist;
	unsigned long		thread_count;
	char			text[80];

	gcg_debug = debug;

	gcg_ctx_new( &gcgctx, 0);
	strcpy(gcdir, GCDIR);

	switch ( os) 
	{
	  case pwr_mOpSys_VAX_ELN:
       	    strcpy( objdir, "pwrp_root:[vax_eln.obj]");
	    strcpy( os_str, "VAX_ELN");
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_ELN;
	    break;
	  case pwr_mOpSys_VAX_VMS:
	    strcpy( objdir, "pwrp_root:[vax_vms.obj]");
	    strcpy( os_str, "VAX_VMS");
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_VMS;
	    break;
	  case pwr_mOpSys_AXP_VMS:
	    strcpy( objdir, "pwrp_root:[axp_vms.obj]");
	    strcpy( os_str, "AXP_VMS");
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_VMS;
	    break;
	  case pwr_mOpSys_PPC_LYNX:
	    strcpy( objdir, "xxx"); /* Not used */
	    strcpy( os_str, "PPC_LYNX");
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_LYNX;
	    break;
	  case pwr_mOpSys_X86_LYNX:
	    strcpy( objdir, "xxx");
	    strcpy( os_str, "X86_LYNX"); /* Not used */
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_LYNX;
	    break;
	  case pwr_mOpSys_PPC_LINUX:
	    strcpy( objdir, "xxx"); /* Not used */
	    strcpy( os_str, "PPC_LINUX");
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_LINUX;
	    break;
	  case pwr_mOpSys_X86_LINUX:
	    strcpy( objdir, "xxx");
	    strcpy( os_str, "X86_LINUX"); /* Not used */
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_LINUX;
	    break;
	  case pwr_mOpSys_AXP_LINUX:
	    strcpy( objdir, "xxx");
	    strcpy( os_str, "AXP_LINUX"); /* Not used */
	    max_no_timebase = GCG_MAX_NO_TIMEBASE_LINUX;
	    break;
	  default:
	    return GSX__UNKNOPSYS;
	}


	/* Get the plclist for this node */
	plc_count = 0;
	thread_count = 0;
	volumelist_ptr = volumelist;
	for( i = 0; i < volume_count; i++)
	{
	  sts = gcg_read_volume_plclist( gcgctx, *volumelist_ptr, 
			&plc_count, &plclist, &thread_count, &threadlist);
	  if ( EVEN(sts)) 
	  {
	    /* No plcpgm's in this volume */
	    *volumelist_ptr = 0;
	  }
	  volumelist_ptr++;
	}

	/* Sort the plclist in executeorder order */
	sts = gcg_sort_plclist( gcgctx, plclist, plc_count);
	if ( EVEN(sts)) return sts;

	/* Sort the threadlist in priority order */
	sts = gcg_sort_threadlist( gcgctx, threadlist, thread_count);
	if ( EVEN(sts)) return sts;

	if ( plc_count == 0)
	{
	  /* No plcpgms on this node */
	  printf( "-- No plcpgms found on node %s\n", nodename);
	  return GSX__NOPLC;
	}

	/* Check that every plcpgm has a valid thread */
	for ( i = 0; i < (int)plc_count; i++)
	{
	  found = 0;
	  for ( j = 0; j < (int)thread_count; j++)
	  {
	    if ( cdh_ObjidIsEqual( (plclist + i)->thread, 
			(threadlist+ j)->objdid))
	    {
	      found = 1;
	      break;
	    }
	  }
	  if ( !found)
	  {
	    sprintf( text, "in plcpgm %s", (plclist+i)->name);
	    gcg_text_msg( gcgctx, GSX__NOTHREAD, text);
	  }
	}

	if ( thread_count == 0)
	{
	  /* No threads on this node */
	  printf( "-- No PlcThreads found on node %s\n", nodename);
	  return GSX__NOPLC;
	}

	/* Insert the plc objects in the timebaselists */
	timebase = (gcg_t_timebase *) calloc( thread_count, 
		sizeof( gcg_t_timebase));
	for ( i = 0; i < (int)thread_count; i++ )
	{
	  (timebase+i)->thread_objid = (threadlist+i)->objdid;
	  (timebase+i)->scantime = (threadlist+i)->scantime;
	  (timebase+i)->prio = (threadlist+i)->prio;
	  (timebase+i)->plc_count = 0;
	  (timebase+i)->plclist = (pwr_tObjid *)calloc( plc_count, 
		sizeof( pwr_tObjid));
	  for ( k = 0; k < (int)plc_count; k++ )
	  {
	    if ( cdh_ObjidIsEqual((plclist+k)->thread, (threadlist+i)->objdid))
	    {
	      (timebase+i)->plclist[(timebase+i)->plc_count] = 
			(plclist+k)->objdid;
	      (timebase+i)->plc_count++;
	    }
	  }
	}
	timebase_count = thread_count;

	if ( single_scantime != 0)
	{
	  /* Insert all plcpgm's into one timebase */
/*	  timebase->scantime = single_scantime; */
	  timebase->prio = 1;
	  timebase_count = 1;
	  for ( i = 1; i < (int)thread_count; i++ )
	  {
	    for ( k = 0; k < (timebase+i)->plc_count; k++)
	    {
	      timebase->plclist[timebase->plc_count] =
		(timebase+i)->plclist[k];
	      timebase->plc_count++;
	    }
	    free( (char *)(timebase+i)->plclist);
	  }
	  printf ("-- SimulateSingleProcess is configured for this node\n");
	}

	if ( timebase_count > max_no_timebase)
	{
          printf( "** Error, %d frequencies is supported on %s, %d is found\n",
			 max_no_timebase, os_str, timebase_count);
	  return GSX__NONODE;
        }

	/* Generate one c module for every timebase, and one optfile */
	if ( codetype)
	{

	  sprintf( fullfilename,"%s%s%s_%4.4d%s", gcdir, gcgmn_filenames[0], 
		nodename, bus, GCEXT);
	  dcli_translate_filename( fullfilename, fullfilename);
	  if ((files[0] = fopen( fullfilename,"w")) == NULL) 
	  {
	    printf("Cannot open file: %s\n", fullfilename);
	    return GSX__OPENFILE;
	  }
	  fprintf( files[0],"#include \"%s\"\n\n", PLCINC);
	  fprintf( files[0],"#include \"%s\"\n\n", PROCINC);

	  if (IS_VMS_OR_ELN(os)) 
	  {
	    sprintf( fullfilename,"%splc_%s_%4.4d.opt", "pwrp_tmp:", nodename, bus);
	    dcli_translate_filename( fullfilename, fullfilename);
	    if ((files[1] = fopen( fullfilename,"w")) == NULL) 
	    {
	      printf("Cannot open file: %s\n", fullfilename);
	      fclose(files[0]);
	      return GSX__OPENFILE;
	    }
	  }

	  for ( i = 0; i < timebase_count; i++ )
	  {
	    timebase_ms = (int)((timebase+i)->scantime * 1000 + 0.5);
	    timebase_ptr = (timebase+i)->plclist;
	    printf (
"-- Plc thread generated priority %d, scantime %7.3f s, %d plcpgm's \n",
			(timebase+i)->prio, (timebase+i)->scantime, 
			(timebase+i)->plc_count);

	    /* Init */
	    fprintf( files[0],
		"void plc_p%d_init( int DirectLink, plc_sThread *tp){\n",
		i + 1);
	    for ( k = 0; k < (timebase+i)->plc_count; k++ )
	    {
	      fprintf( files[0],
	  	"  %c%s_init( DirectLink, tp);\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, (timebase+i)->plclist[k]));
	    }
	    fprintf(files[0], "}\n");

	    /* Exec */
	    fprintf( files[0],
		"void plc_p%d_exec( int DirectLink, plc_sThread *tp){\n",
		i + 1);
	    for ( k = 0; k < (timebase+i)->plc_count; k++ )
	    {
	      fprintf( files[0],
	  	"  %c%s_exec( tp);\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, (timebase+i)->plclist[k]));
	    }
	    fprintf(files[0], "}\n");

	    /* proctbl */
	    fprintf( files[0],"struct plc_proctbl plc_proc%d = \n", i + 1);
	    fprintf( files[0],
		"{ {%d, %d}, plc_p%d_init, plc_p%d_exec };\n",
		(timebase+i)->thread_objid.oix, (timebase+i)->thread_objid.vid,
		i + 1, 
		i + 1);
	    fprintf( files[0],"\n\n");

	  }

	  fprintf( files[0],"struct plc_proctbl *plc_proctbllist[] = {\n");
	  for ( i = 0; i < timebase_count; i++ )
	    fprintf( files[0],"  &plc_proc%d,\n", i + 1);
	  fprintf( files[0],"  (void *)0\n};\n");
	  fclose(files[0]);
	  files[0] = NULL;	   

	  /* Create an object file */
	  sprintf( module_name, "%s%s_%4.4d", gcgmn_filenames[0], nodename, bus);
	  gcg_cc( GCG_PROC, module_name, NULL, NULL, os, GCG_NOSPAWN);

	    /* print module in option file */
	  if (IS_VMS_OR_ELN(os))
	    fprintf( files[1],"%s%s%s_%4.4d\n", objdir, gcgmn_filenames[0], 
		nodename, bus);
	
	  /* Print plc libraries in option file */
	  volumelist_ptr = volumelist;
	  *plclib_frozen = '\0'; 
	  for (i = l = 0; i < volume_count; i++, volumelist_ptr++)
	  {
	    if ( *volumelist_ptr )
	    {
	      switch ( os) 
	      {
	        case pwr_mOpSys_VAX_ELN:
	          sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_VAX_ELN,
			vldh_VolumeIdToStr( *volumelist_ptr));
	          break;
	        case pwr_mOpSys_VAX_VMS:
	          sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_VAX_VMS,
			vldh_VolumeIdToStr( *volumelist_ptr));
	          break;
	        case pwr_mOpSys_AXP_VMS:
	          sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_AXP_VMS,
			vldh_VolumeIdToStr( *volumelist_ptr));
	          break;
	        case pwr_mOpSys_PPC_LINUX:
	        case pwr_mOpSys_X86_LINUX:
	        case pwr_mOpSys_AXP_LINUX:
	        case pwr_mOpSys_X86_LYNX:
	        case pwr_mOpSys_PPC_LYNX:
	          l += sprintf( &plclib_frozen[l], "%s%s ", PLCLIB_FROZEN_LINK_UNIX,
			vldh_VolumeIdToStr( *volumelist_ptr));
	          break;
	        default:
	          return GSX__UNKNOPSYS;
	      }
	      if (IS_VMS_OR_ELN(os)) 
	        fprintf( files[1],"%s/lib\n", plclib_frozen);
	    }
	  }

	  if (IS_VMS_OR_ELN(os)) 
	  { 
	    fclose(files[1]);
	    files[1] = NULL;	   
	  }

	  /* Link */
	  if (IS_VMS_OR_ELN(os)) 
	  {
	    sprintf( plcfilename, "pwrp_exe:plc_%s_%4.4d_%5.5ld.exe",
		nodename, bus, plc_version);
	    sprintf( fullfilename,"%s%s_%4.4d", gcgmn_filenames[0], nodename, bus);
	    gcg_cc( GCG_RTNODE, fullfilename, plcfilename, NULL, os,
	          GCG_NOSPAWN);
	  } 
	  else
	  {
	    sprintf( plcfilename, "plc_%s_%4.4d_%5.5ld",
		nodename, bus, plc_version);
	    sprintf( fullfilename,"%s%s_%4.4d", gcgmn_filenames[0], nodename, bus);
	    gcg_cc( GCG_RTNODE, fullfilename, plcfilename, 
	        plclib_frozen, os, GCG_NOSPAWN);
	  }

	}

	for ( i = 0; i < timebase_count; i++ )
	{
	  if ( (timebase+i)->plc_count > 0)
	    free((char *) (timebase+i)->plclist);
	}
	free((char *) timebase);

	*errorcount = gcgctx->errorcount;
	*warningcount = gcgctx->warningcount;

	if ( plc_count > 0)
	  free((char *) plclist);

	gcg_ctx_delete( gcgctx);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_volume()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* ldh_tSesContext ldhses		I	ldh session.
*
* Description:
*	Compile method for a volume.
*	All plcpgm are written in a file that will be read later when
*	linking of the plcprogram is done.
*	The plc library for the volume is frozen.
*
**************************************************************************/

int	gcg_comp_volume( 
    ldh_tSesContext ldhses
)
{
	int			i, sts, size;
	pwr_tFileName  		filenames;
	pwr_tFileName  		fullfilename;
	FILE			*file;
	unsigned long		plc_count;
	gcg_t_plclist		*plclist;
	gcg_ctx			gcgctx;
	char			volumename[80];
	pwr_tVolumeId 		volid;
	ldh_sVolumeInfo 	info;
	pwr_tObjid		volobjid;
	char			plclibrary[80];
	char			plclib_frozen[80];
	pwr_mOpSys    		*os;
	pwr_mOpSys    		operating_system;
	int			opsys;
	gcg_t_threadlist	*threadlist;
	unsigned long		thread_count;
	char			thread_objid_str[40];

	/* Get the volumeid from the ldh session */
	sts = ldh_GetVolumeInfo( ldh_SessionToVol( ldhses), &info);
	if ( EVEN(sts)) return sts;

	volid = info.Volume;
	volobjid.vid = volid;
	volobjid.oix = 0;

	gcg_ctx_new( &gcgctx, 0);
	gcgctx->ldhses = ldhses;

        /* Get the operating system for this volume */
        sts = ldh_GetObjectPar(ldhses, volobjid, "SysBody", "OperatingSystem",
	  (char **)&os, &size);
        if ( EVEN(sts)) return sts;
   
        operating_system = *os;
	free((char *)os);

	/* Get all plcthread objects in this volume */
	sts = gcg_get_rtnode_plcthread( gcgctx, volobjid, operating_system, 
			&thread_count, &threadlist);
	if ( EVEN(sts)) return sts;

	/* Get the plclist for all plcpgm's in this volume */
	sts = gcg_get_rtnode_plc( gcgctx, volobjid, operating_system, 
			&plc_count, &plclist);
	if ( EVEN(sts)) return sts;

	if ( plc_count == 0)
	{
	  sts = ldh_VolumeIdToName( ldh_SessionToWB(ldhses), volid,
		volumename, sizeof(volumename), &size);
	  if ( EVEN(sts)) return sts;

	  /* No plcpgms on this node */
	  printf( "-- No plcpgms found in volume %s\n", volumename);
	}
	else
	{
	  /* Check operating system */
	  if (IS_NOT_VALID_OS(operating_system))
	    return GSX__NOOPSYS;
	}

	sprintf( filenames, "%s%s", gcgmv_filenames[0],
		vldh_VolumeIdToStr( volid));
	sprintf( fullfilename,"%s%s%s", DATDIR, filenames, DATEXT);
	dcli_translate_filename( fullfilename, fullfilename);
	if ((file = fopen( fullfilename,"w"))  == NULL)
	{
	  printf("Cannot open file: %s\n", fullfilename);
	  return GSX__OPENFILE;
	}

	for ( i = 0; i < (int)thread_count; i++ )
	{
	  fprintf( file, "%s %s %f %ld %s\n", 
		"PlcThread", 
		cdh_ObjidToString( NULL, (threadlist+i)->objdid, 0),
		(threadlist+i)->scantime,
		(threadlist+i)->prio,
		(threadlist+i)->name);
	}

	for ( i = 0; i < (int)plc_count; i++ )
	{
	  strcpy( thread_objid_str, "");
	  cdh_ObjidToString( thread_objid_str, (plclist + i)->thread, 0);
	  fprintf( file, "%s %s %s %ld %s\n", 
		"PlcPgm",
		cdh_ObjidToString( NULL, (plclist + i)->objdid, 0),
		thread_objid_str,
		(plclist + i)->executeorder,
		(plclist + i)->name);
	}
	fclose( file);
	
	if ( plc_count > 0)
	{
	  /* Freeze the plc library */

	  for ( opsys = 1; opsys != pwr_mOpSys_; opsys <<= 1)
	  {
	    if ( operating_system & opsys)
	    {
	      if ( opsys & pwr_mOpSys_VAX_ELN)
	      {
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_ELN,
			vldh_VolumeIdToStr( volid));
	        sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_VAX_ELN,
			vldh_VolumeIdToStr( volid));
	      }
	      else if ( opsys & pwr_mOpSys_VAX_VMS)
	      {
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_VMS,
			vldh_VolumeIdToStr( volid));
	        sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_VAX_VMS,
			vldh_VolumeIdToStr( volid));
	      }
	      else if ( opsys & pwr_mOpSys_AXP_VMS)
	      {
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_AXP_VMS,
			vldh_VolumeIdToStr( volid));
	        sprintf( plclib_frozen, "%s%s.olb", PLCLIB_FROZEN_AXP_VMS,
			vldh_VolumeIdToStr( volid));
	      }
	      else if ( IS_UNIX(opsys))
	      {
	        sprintf( plclibrary, "NotUsed");
	        sprintf( plclib_frozen, "%s%s.a", PLCLIB_FROZEN_UNIX,
			vldh_VolumeIdToStr( volid));
	      }

#if 0
	      if (IS_VMS_OR_ELN(opsys))
	      {
	        sts = dir_CopyFile( plclibrary, plclib_frozen);
	        if (EVEN(sts)) return sts;
	        sts = dir_PurgeFile( plclib_frozen, 1);
	        if (EVEN(sts)) return sts;
	      }
	      else if (IS_UNIX(opsys))
#endif
	      {
		/* Insert all objects into the archive */
	        sts = gcg_cc( GCG_LIBRARY, vldh_VolumeIdToStr( volid), plclib_frozen, 
			      plclibrary, (pwr_mOpSys)opsys, GCG_NOSPAWN);
	        if (EVEN(sts)) return sts;
	      }
	    }
          }
	}

	if ( plc_count > 0)
	  free((char *) plclist);

	gcg_ctx_delete( gcgctx);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m0()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_plc	plc		I	vldh plc.
* unsigned long	codetype	I	generate code or syntax only.
* unsigned long	*errorcount	O	error counter.
* unsigned long	*warningcount	O	error counter.
* unsigned long	spawn		I	c-compilation spawned or not.
*
* Description:
*	Compile method for a plcprogram.
*	The routine generates a c-module whith one init function and
*	one exec function. The init function calls the initfunctions of
*	all the windows in the plc, the exec function calls the exec function
*	of the main window of the plc.
*	A c compilation of the c module is done and the object module
*	is placed in the library pwrp_root:['platform'.lib]ra_plc.olb
*	( this is done by the command file ds_foe_gcg.com)
*
**************************************************************************/

int	gcg_comp_m0( vldh_t_plc	plc,
		     unsigned long	codetype,
		     unsigned long	*errorcount,
		     unsigned long	*warningcount,
		     unsigned long	spawn)
{
	unsigned long		wind_count;
	vldh_t_wind		*windlist;
	unsigned long		ldhwind_count;
	pwr_tObjid		*ldhwindlist;
	int			i, sts, size;
	pwr_tFileName	       	filenames[ GCG_MAXFILES ];
	pwr_tFileName	       	fullfilename;
	gcg_t_files		files;
	char			module_name[80];
        pwr_mOpSys		operating_system;
	char			gcdir[80];
	unsigned long		opsys;
	char			plclibrary[80];
	pwr_tOName	       	plc_name;

	/* Reset error and warning counts */
	*errorcount = 0;
	*warningcount = 0;


        /* Get operating system */
        sts = gcg_plcpgm_to_operating_system(plc->hp.ldhsesctx, plc->lp.oid,
	      &operating_system);
	if ( EVEN(sts)) return sts;

	strcpy( gcdir, GCDIR);

	/* Check operating system */
	if ( IS_NOT_VALID_OS(operating_system))
	  return GSX__UNKNOPSYS;

	/* Get all the windows in this plc program in vldh */
	sts = vldh_get_plc_windows( plc, &wind_count, &windlist);
	if ( EVEN(sts)) return sts;

	/* Get all the windows in this plc program in ldh */
	sts = gcg_get_plc_windows( plc->hp.ldhsesctx, plc->lp.oid,
		&ldhwind_count, &ldhwindlist);
	if ( EVEN(sts)) return sts;

	if ( codetype)
	{
	  /* The plc is a c module, open the files needed for the
	     module */
	  for( i = 0; i < GCGM0_MAXFILES; i++)
	  {
	    sprintf( filenames[i], "%s%s", gcgm0_filenames[i], 
		vldh_IdToStr(0, plc->lp.oid));
	    sprintf( fullfilename,"%s%s%s", gcdir, filenames[i], GCEXT);
	    dcli_translate_filename( fullfilename, fullfilename);
	    if ((files[i] = fopen( fullfilename,"w")) == NULL)
	    {
              int j;
	      printf("Cannot open file: %s\n", fullfilename);
              for (j = 0; j < i; j++)
	        fclose(files[j]); 
	      return GSX__OPENFILE;
	    }
	  }

/*
	  sts = gcg_get_converted_hiername( plc->hp.ldhsesctx, 
		plc->lp.oid, module_name);
	  if ( EVEN(sts)) return sts;
	  fprintf( files[ GCGM0_MODULE_FILE ],"#module %s\n\n",
		module_name);
*/
	  fprintf( files[ GCGM0_MODULE_FILE ],
		"#include \"%s\"\n", PLCINC);

	  fprintf( files[ GCGM0_MODULE_FILE ],
	    "void %c%s_init( int DirectLink, plc_sThread *tp){\n",
	     GCG_PREFIX_MOD, vldh_IdToStr(0, plc->lp.oid));

	  /* Print init calls for the windows */
	  for ( i = 0; i < (int)ldhwind_count; i++)
	  {
	    fprintf( files[ GCGM0_MODULE_FILE ],
	  	"%c%s_init( DirectLink, tp);\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, *(ldhwindlist + i)));
	  }
	  fprintf( files[ GCGM0_MODULE_FILE ],
		"}\nvoid %c%s_exec( plc_sThread *tp){\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, plc->lp.oid));

	  /* Print exec calls for the plc window */
	  fprintf( files[ GCGM0_MODULE_FILE ],
	  	"%c%s_exec( tp);\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, *ldhwindlist));

	  fprintf( files[ GCGM0_MODULE_FILE ],
		"}\n");

	  /* Close the files */
	  for( i = 0; i < GCGM0_MAXFILES; i++ )
	    fclose( files[i]);

	  /* Compile the c code */
	  if ( *errorcount == 0)
	  {    
	    for ( opsys = 1; opsys != pwr_mOpSys_; opsys <<= 1)
	    {
	      if ( operating_system & opsys)
	      {
	        if ( opsys & pwr_mOpSys_VAX_ELN)
	          sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_ELN,
			vldh_VolumeIdToStr( plc->lp.oid.vid));
	        else if ( opsys & pwr_mOpSys_VAX_VMS)
	          sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_VMS,
			vldh_VolumeIdToStr( plc->lp.oid.vid));
	        else if ( opsys & pwr_mOpSys_AXP_VMS)
	          sprintf( plclibrary, "%s%s.olb", PLCLIB_AXP_VMS,
			vldh_VolumeIdToStr( plc->lp.oid.vid));
	        else if ( IS_UNIX(opsys))
	          sprintf( plclibrary, "%s%s.a", PLCLIB_UNIX,
			vldh_VolumeIdToStr( plc->lp.oid.vid));

	        sprintf( module_name, "%s", vldh_IdToStr(0, plc->lp.oid));
	        sts = ldh_ObjidToName( plc->hp.ldhsesctx, plc->lp.oid,
			ldh_eName_Hierarchy, plc_name, sizeof( plc_name), 
			&size);
	        if ( EVEN(sts)) return sts;

	        sts = gcg_cc( GCG_PLC, module_name, plclibrary, 
			      plc_name, (pwr_mOpSys)opsys, spawn);
                if ( EVEN(sts)) {
		  gcg_error_msg( 0, sts, 0);
		  (*errorcount)++;
		}
	      }
	    }          
	  }
	}

	if ( ldhwind_count > 0)
	  free((char *) ldhwindlist);
	if ( wind_count > 0)
	  free((char *) windlist);
	
	if ( *errorcount > 0 )
	  return GSX__PLCPGM_ERRORS;
	else
	  return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m1()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* vldh_t_wind	wind		I	vldh window.
* unsigned long	codetype	I	generate code or syntax control only.
* unsigned long	*errorcount	O	error counter.
* unsigned long	*warningcount	O	warning counter.
* unsigned long	spawn		I	if the c-compliation should be spawned
*					or not.
*
* Description:
*	Compile method for a plcwindow.
*	Generates a c module for the window and creates a number of
*	includefiles which is inlcude in the module.
*	A gcg context is created where data of the compilation of this window
*	is stored.
*	The routine calls the compile method for each object in the window
*	in this order (the order will be the execution order):
*	1. plc objects from left to right (x-koordinate of lower left corner).
*	2. grafcet objects up -> down ( y-koordinate of lower left corner)
*	After the compilation a message of the result of the syntax control
*	is written with the number of errors and warnings.
*
*	Include files:
* 	GCGM1_DECL_FILE 	plc_dec'objdid'.gc
*				Declarations of rtdb pointer.
* 	GCGM1_RTDBREF_FILE 	plc_r1robjdid'.gc
*				Code to get the rtdb pointer.
* 	GCGM1_RTDBREF2_FILE 	_plc_r2r'objdid'.gc
*				Code to get the rtdb pointer for io objects.
* 	GCGM1_REF_FILE 		_plc_ref'objdid'.gc
*				Initializations of rtdb objects.
* 	GCGM1_CODE_FILE 	plc_cod'objdid'.gc
*				Code to be executed every scan.
*	Module file:
* 	GCGM1_MODULE_FILE 	plc_m'objdid'.gc
*				c module with declarations of rtdbpointers,
*				an init functions to get the rtdbpointers
*				and do the initializations of the objects,
*				an exec function to execute code every scan.
*
**************************************************************************/

int	gcg_comp_m1( vldh_t_wind wind,
		     unsigned long codetype,
		     unsigned long *errorcount,
		     unsigned long *warningcount,
		     unsigned long spawn)
{
	ldh_tSesContext ldhses;
	int			sts, size;
	unsigned long		node_count;
	vldh_t_node		*nodelist;
	vldh_t_node		node;
	int			i,j;
	gcg_ctx			gcgctx;
	pwr_tFileName  		fullfilename;
	char			module_name[80];
	char			*name;
	pwr_mOpSys		operating_system;
	ldh_sSessInfo		info;
	pwr_tOName	       	wind_name;
	ldh_eAccess		session_access;
	char			gcdir[80];
	char			plclibrary[80];
	unsigned long		opsys;

	ldhses = wind->hw.ldhses;

	sts = ldh_ObjidToName( wind->hw.ldhses, 
		wind->lw.oid, ldh_eName_Hierarchy,
		wind_name, sizeof( wind_name), &size);
	if( EVEN(sts)) return sts;


	if ( codetype)
	{
	  /* Check that the session is empty */
	  sts = ldh_GetSessionInfo( wind->hw.ldhses, &info);
	  if ( EVEN(sts)) return sts;
	  if ( !info.Empty)
	    return GSX__NOTSAVED;

	  /* Check that session is readwrite */
	  session_access = info.Access;
	  if ( info.Access != ldh_eAccess_ReadWrite)
	  {	  
	    /* Set access read write to be able to compile */
	    sts = ldh_SetSession( ldhses, ldh_eAccess_SharedReadWrite);
	    if ( EVEN(sts)) return sts;
	  }
	}

	msgw_set_nodraw();

	/* Create a context for the window */
	gcg_ctx_new( &gcgctx, wind);
	gcgctx->ldhses = wind->hw.ldhses;
	gcgctx->print = codetype;

        /* Get operating system */
        sts = gcg_wind_to_operating_system(
            wind->hw.ldhses,
            wind->lw.oid,
            &operating_system
            );
	if ( EVEN(sts)) 
	  gcg_error_msg( gcgctx, sts, 0);

	strcpy( gcdir, GCDIR);

	/* Check operating system */
	if (IS_NOT_VALID_OS(operating_system))
	{
	  gcg_error_msg( gcgctx, GSX__NOOPSYS, 0);
	  return GSX__PLCWIND_ERRORS;
	}

	if ( codetype)
	{
	  /* Check that project specific includefiles and
	     libraries exist */
	  
	  /* pwrp_inc:ra_plc_user.h */
	  dcli_translate_filename( fullfilename, "pwrp_inc:ra_plc_user.h");
	  sts = gcg_file_check( fullfilename);
	  if ( EVEN(sts))
	    gcg_error_msg( gcgctx, sts, 0);
	  else if ( sts == GSX__FILECREATED)
	    printf( "%%GSX-I-FILECREA, file pwrp_inc:ra_plc_user.h is created\n");
	}


	/* The window is a c module, open the files needed for the
	   module */
	for( i = 0; i < GCGM1_MAXFILES; i++)
	{
	  sprintf( gcgctx->filenames[i], "%s%s", gcgm1_filenames[i], 
		vldh_IdToStr(0, wind->lw.oid));
	  sprintf( fullfilename,"%s%s%s", gcdir, gcgctx->filenames[i], GCEXT);
	  IF_PR 
	  {
	    dcli_translate_filename( fullfilename, fullfilename);
	    gcgctx->files[i] = fopen( fullfilename,"w");
	    if (gcgctx->files[i] == NULL)
	    {
	      printf("Cannot open file: %s\n", fullfilename);
              for (j = 0; j < i; j++)
	        fclose(gcgctx->files[j]); 
	      return GSX__OPENFILE;
	    }
	  }

	}

/*
	sts = gcg_get_converted_hiername( wind->hw.ldhses, 
		wind->lw.oid, module_name);
	if ( EVEN(sts)) return sts;
*/

	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"#include \"%s\"\n", MACROINC);
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"#include \"%s%s\"\n",
		gcgctx->filenames[ GCGM1_DECL_FILE ], GCEXT);
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"static plc_t_rtdbref rtdbref[] = {\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"#include \"%s%s\"\n",
		gcgctx->filenames[ GCGM1_RTDBREF_FILE ], GCEXT);
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],"{ 0}};\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"static plc_t_rtdbref rtdbref2[] = {\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"#include \"%s%s\"\n",
		gcgctx->filenames[ GCGM1_RTDBREF2_FILE ], GCEXT);
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],"{ 0}};\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"void %c%s_init( int DirectLink, plc_sThread *tp){\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, wind->lw.oid));
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"if (DirectLink == 1)\n  plc_rtdbref( &rtdbref, tp);\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"else if (DirectLink == 2)\n  plc_rtdbref( &rtdbref2, tp);\n");
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"else\n  {\n#include \"%s%s\"\n  }\n}\n\n",
		gcgctx->filenames[ GCGM1_REF_FILE ], GCEXT);
	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"void %c%s_exec( plc_sThread *tp)\n{\n",
		GCG_PREFIX_MOD, vldh_IdToStr(0, wind->lw.oid));

	/* Print exec call for the window */
	sts = gcg_ref_insert( gcgctx, wind->lw.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, wind->lw.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, wind->lw.oid));

	IF_PR fprintf( gcgctx->files[ GCGM1_MODULE_FILE ],
		"#include \"%s%s\"\n}\n",
		gcgctx->filenames[ GCGM1_CODE_FILE ], GCEXT);


	/* Get all nodes this window */
	sts = vldh_get_nodes( wind, &node_count, &nodelist);
	if ( EVEN(sts)) return sts;
	gcg_executeorder_nodes( node_count, nodelist);

        for ( j = 0; j < (int)node_count; j++)
	{
	  node = *(nodelist + j);
	  sts = gcg_node_comp( gcgctx, node);
	  if ( EVEN(sts)) goto classerror; 
	}	  
	if ( node_count > 0)
	  free((char *) nodelist);

	if ( gcgctx->current_cmanager)
	  gcg_reset_cmanager( gcgctx);

	sts = gcg_ref_print( gcgctx);
	if ( EVEN(sts)) goto classerror; 
	sts = gcg_aref_print( gcgctx);
	if ( EVEN(sts)) goto classerror; 
	sts = gcg_ioread_print( gcgctx);
	if ( EVEN(sts)) goto classerror; 
	sts = gcg_iowrite_print( gcgctx);
	if ( EVEN(sts)) goto classerror; 

	*errorcount = gcgctx->errorcount;
	*warningcount = gcgctx->warningcount;

	/* Message of result */
	if ( gcgctx->print )
	{
	  if ( gcgctx->errorcount > 0)
	    gcg_wind_msg( gcgctx, GSX__CWINDERR, wind);
	  else if ( gcgctx->warningcount > 0 )
	    gcg_wind_msg( gcgctx, GSX__CWINDWARN, wind);
	  else
	    printf( "-- Plc window generated            %s\n", wind_name);
	}
	else
	{
	  if ( gcgctx->errorcount > 0)
	    gcg_wind_msg( gcgctx, GSX__SWINDERR, wind);
	  else if ( gcgctx->warningcount > 0 )
	    gcg_wind_msg( gcgctx, GSX__SWINDWARN, wind);
	  else
	    gcg_wind_msg( gcgctx, GSX__SWINDSUCC, wind);
	}

	msgw_reset_nodraw();

	/* Close the files */
	for( i = 0; i < GCGM1_MAXFILES; i++ )
	  IF_PR fclose( gcgctx->files[i]);

	/* Create an object file */
	if ( gcgctx->errorcount == 0 )
	{
	  for ( opsys = 1; opsys != pwr_mOpSys_; opsys <<= 1)
	  {
	    if ( operating_system & opsys)
	    {
	      if ( opsys & pwr_mOpSys_VAX_ELN)
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_ELN,
			vldh_VolumeIdToStr( wind->lw.oid.vid));
	      else if ( opsys & pwr_mOpSys_VAX_VMS)
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_VAX_VMS,
			vldh_VolumeIdToStr( wind->lw.oid.vid));
	      else if ( opsys & pwr_mOpSys_AXP_VMS)
	        sprintf( plclibrary, "%s%s.olb", PLCLIB_AXP_VMS,
			vldh_VolumeIdToStr( wind->lw.oid.vid));
	      else if ( IS_UNIX(opsys))
	        sprintf( plclibrary, "%s%s.a", PLCLIB_UNIX,
			vldh_VolumeIdToStr( wind->lw.oid.vid));

	      IF_PR sprintf( module_name, "%s", vldh_IdToStr(0, wind->lw.oid));
	      IF_PR {
                sts = gcg_cc( GCG_WIND, module_name, plclibrary, wind_name, 
			(pwr_mOpSys)opsys, spawn);
                if ( EVEN(sts)) {
		  gcg_error_msg( 0, sts, 0);
		  (*errorcount)++;
		}
	      }
	    }
          }
	}

	/* Delete the context for the window */
	gcg_ctx_delete( gcgctx);

	/* Generate code for the plc module */
/*	sts = gcg_comp_m[0]( wind->hw.plc, codetype, 0, 0, 0);
	if ( EVEN(sts)) return sts;
*/
	/* Generate code for the rtnode */
/*	sts = gcg_comp_rtnode( wind->hw.ldhses, 0, 
			wind->hw.plc, codetype, &node_errorcount,
			&node_warningcount);
	if ( EVEN(sts)) return sts;
	(*errorcount) += node_errorcount;
	(*warningcount) += node_warningcount;
*/

	if ( *errorcount > 0 )
	{
	  if ( codetype)
	  {
  	    /* Revert the session */
	    sts = ldh_RevertSession( wind->hw.ldhses);
	    if ( EVEN(sts)) return sts;


	    /* Return to previous session access */
	    if ( session_access != ldh_eAccess_ReadWrite)
	    {	  
	      /* Set access read write to be able to compile */
	      sts = ldh_SetSession( ldhses, session_access);
	      if ( EVEN(sts)) return sts;
	    }
	  }
	  return GSX__PLCWIND_ERRORS;
	}
	else
	{
	  if ( codetype)
	  {
            pwr_tTime time;

            /* Store compile time for the window */
	    clock_gettime(CLOCK_REALTIME, &time);
	    sts = ldh_SetObjectPar( wind->hw.ldhses, wind->lw.oid,
		"DevBody", "Compiled", (char *)&time, sizeof( time)); 

	    /* Save the session */
	    sts = ldh_SaveSession( wind->hw.ldhses);
	    if ( EVEN(sts)) return sts;

	    /* Return to previous session access */
	    if ( session_access != ldh_eAccess_ReadWrite)
	    {	  
	      sts = ldh_SetSession( ldhses, session_access);
	      if ( EVEN(sts)) return sts;
	    }
	  }
	  return GSX__SUCCESS;
	}

classerror:
	/* If error status from ldh */
	gcg_wind_msg( gcgctx, GSX__CLASSERR, 0);
	msgw_reset_nodraw();

  	/* Revert the session */
	ldh_RevertSession( wind->hw.ldhses);

	/* Return to previous session access */
	if ( session_access != ldh_eAccess_ReadWrite)
	   ldh_SetSession( ldhses, session_access);

	/* Close the files */
	for( i = 0; i < GCGM1_MAXFILES; i++ )
	  IF_PR fclose( gcgctx->files[i]);

	/* Delete the context for the window */
	gcg_ctx_delete( gcgctx);

	return sts;
}

/*************************************************************************
*
* Name:		gcg_comp_m2()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an object that doesn't need any code in 
*	the plc job.
*
**************************************************************************/

int	gcg_comp_m2( gcg_ctx gcgctx, vldh_t_node node)
{
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m6()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for AND object.
*	Prints code for declaration and direkt link of a rtdbpointer
*	for the and-object.
*	Prints an exec call. Ex :
*	and_exec( Z8000086f, Z8000086d->Status && Z800005d5->Status[0]);
*	
*	If the inputs are not connected or not visible they are not
*	printed in the exec call.
*
**************************************************************************/

int	gcg_comp_m6( gcg_ctx gcgctx, vldh_t_node node)
{
	int	sts;
	ldh_tSesContext ldhses;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	sts = gcg_print_inputs( gcgctx, node, " && ", GCG_PRINT_CONPAR, NULL,
			NULL);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m7()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for OR object.
*	Prints code for declaration and direkt link of rtdbpointer
*	for the or-object.
*	Prints an exec call :
*	or_exec( Z8000086f, Z8000086d->Status || Z800005d5->Status[0]);
*
*	If the inputs are not connected or not visible they are not
*	printed in the exec call.
*
**************************************************************************/

int	gcg_comp_m7( gcg_ctx gcgctx, vldh_t_node node)
{
	int	sts;
	ldh_tSesContext ldhses;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	sts = gcg_print_inputs( gcgctx, node, " || ", GCG_PRINT_CONPAR, NULL,
		NULL);
	if (EVEN(sts)) return sts; 

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m5()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for standard macro node.
*	Prints code for declaration and direkt link of rtdbpointer for 
*	the actual object.
*	Prints an exec call :
*
*	'structname'_exec( 'objectpointer', 'in1', 'in2', 'in3'...);
*	ex: wait_exec( Z800008f1, Z800005d4->Status[0]);
*	
*	If the any inputs are not connected or not visible zero is printed
*	in its place in the exec call.
*
**************************************************************************/

int	gcg_comp_m5( gcg_ctx gcgctx, vldh_t_node node)
{
	int 	sts;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, NULL,
		NULL);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	gcg_scantime_print( gcgctx, node->ln.oid);
	gcg_timer_print( gcgctx, node->ln.oid);
	
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m4()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for standard function call node.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints code for initialization of pointers in the object:
*	'objpointer'->'pgmname'P = &'in';
*	Z80000811->InP = &Z800005f5->ActualValue;
*
*	If a parameter is not connected or not visible the pointer
*	will point to the own object:
*	
*	'objpointer'->'pgmname'P = &'objpointer'->'pgmname';
*	Z80000811->LimP = &Z80000811->Lim;
*
*	Prints an exec call :
*	'structname'_exec( tp, 'objpointer');
*	ex: compl_exec( tp, Z80000811);
*
*	Prints iniatilization for timer and scantime if the object 
*	contain these functions.
*	
**************************************************************************/

int	gcg_comp_m4( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	gcg_timer_print( gcgctx, node->ln.oid);
	gcg_scantime_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m8()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GETDI, GETDO, GETDV, GETAI, GETAO, GETAV,
*       GETII, GETIO, GETIV, GetSv, GetATv and GetDTv node.
*	Checks that the class of the referenced object is correct.
*	Prints declaration and directlink for a read rtdb pointer
*	for the refereced io-object. The pointer will point at
*	the valuebase object for the frequency of the plcpgm.
*
**************************************************************************/

int	gcg_comp_m8( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		attrref;
	pwr_sAttrRef		*attrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&attrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	free((char *) bodydef);	
	attrref = *attrref_ptr;
	free((char *) attrref_ptr);

	sts = gcg_replace_ref( gcgctx, &attrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &attrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that the class of the referenced object is correct */
	if ( node->ln.cid == pwr_cClass_GetDi) {
	  if ( cid != pwr_cClass_Di) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetDo) {
	  if ( ! ( cid == pwr_cClass_Do ||
	           cid == pwr_cClass_Po)) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetDv) {
	  if ( cid != pwr_cClass_Dv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetAi) {
	  if ( cid != pwr_cClass_Ai) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetAo) {
	  if ( cid != pwr_cClass_Ao) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if ( node->ln.cid == pwr_cClass_GetAv) {
	  if ( cid != pwr_cClass_Av) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if ( node->ln.cid == pwr_cClass_GetIi) {
	  if ( cid != pwr_cClass_Ii) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetIo) {
	  if ( cid != pwr_cClass_Io) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if ( node->ln.cid == pwr_cClass_GetIv) {
	  if ( cid != pwr_cClass_Iv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if ( node->ln.cid == pwr_cClass_GetSv) {
	  if ( cid != pwr_cClass_Sv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }

	  /* Insert io object in ioread list */
	  gcg_aref_insert( gcgctx, attrref, GCG_PREFIX_REF);
	  return GSX__SUCCESS;
	}
	else if ( node->ln.cid == pwr_cClass_GetATv) {
	  if ( cid != pwr_cClass_ATv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }

	  /* Insert io object in ioread list */
	  gcg_aref_insert( gcgctx, attrref, GCG_PREFIX_REF);
	  return GSX__SUCCESS;
	}
	else if ( node->ln.cid == pwr_cClass_GetDTv) {
	  if ( cid != pwr_cClass_DTv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }

	  /* Insert io object in ioread list */
	  gcg_aref_insert( gcgctx, attrref, GCG_PREFIX_REF);
	  return GSX__SUCCESS;
	}

	/* Insert io object in ioread list */
	gcg_ioread_insert( gcgctx, attrref, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m9()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GETPI.
*	Checks the class of the referenced object (should be a Co object).
*	Prints declaration and directlink for two read pointers to the 
*	referenced Co objects. The pointers will point to the two
*	valuebase objects that is created for Co for each plc frequency.
*
**************************************************************************/

int	gcg_comp_m9( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		attrref;
	pwr_sAttrRef		*attrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&attrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	free((char *) bodydef);	
	attrref = *attrref_ptr;
	free((char *) attrref_ptr);

	sts = gcg_replace_ref( gcgctx, &attrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &attrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( cid != pwr_cClass_Co) {
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert io object in ioread list */
	gcg_ioread_insert( gcgctx, attrref, GCG_PREFIX_REF);
	gcg_ioread_insert( gcgctx, attrref, GCG_PREFIX_IOC);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m10()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GetDp, GetAp, GetSp, GetATp, GetDTp
*	Checks that the referenced object exists and that the referenced
*	parameter exists in that object, and that the type of the parameter
*	is correct.
*	Prints declaration and direct link of pointer to referenced object.
*
**************************************************************************/

int	gcg_comp_m10( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_sAttrRefInfo	info;
        pwr_tAName	       	aname;
	char			*name_p;
	ldh_tSesContext 	ldhses;
	char			*s;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	sts = ldh_GetAttrRefInfo( ldhses, &refattrref, &info);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Get rid of last attribute segment of the referenced object */
	sts = ldh_AttrRefToName( ldhses, &refattrref, ldh_eName_Aref, 
				 &name_p, &size);
	if ( EVEN(sts)) return sts;

	strcpy( aname, name_p);
	if ( (s = strrchr( aname, '.')) == 0) { 
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTPAR;
	}

	*s = 0;
	sts = ldh_NameToAttrRef( ldhses, aname, &refattrref);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	if ( info.flags & PWR_MASK_RTVIRTUAL) {
	  /* Attribute is not defined in runtime */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	} 

	if ( info.flags & PWR_MASK_ARRAY) {
	  if ( info.nElement == -1) {
	    /* No index in attribute */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  // if ( index > info.nElement - 1) {
	  //  /* Element index to large */
	  //  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  //  return GSX__NEXTNODE;
	  //}
	}

	switch ( info.type ) {
        case pwr_eType_Float32: 
	  if ( node->ln.cid != pwr_cClass_GetAp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Boolean:
	  if ( node->ln.cid != pwr_cClass_GetDp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Int32: 
        case pwr_eType_UInt32: 
        case pwr_eType_Int16: 
        case pwr_eType_UInt16: 
        case pwr_eType_Int8: 
        case pwr_eType_UInt8: 
        case pwr_eType_Enum: 
        case pwr_eType_Mask: 
        case pwr_eType_Status: 
        case pwr_eType_NetStatus: 
	  if ( node->ln.cid != pwr_cClass_GetIp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_String :
	  if ( node->ln.cid != pwr_cClass_GetSp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Time :
	  if ( node->ln.cid != pwr_cClass_GetATp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_DeltaTime :
	  if ( node->ln.cid != pwr_cClass_GetDTp) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
	default:
	  /* Not allowed type */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m13()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for step or initstep object.
*	Gets all order connected to the order pin and calls the 
*	compilemethod for these. 
*	Prints declaration and direct link code for the step object.
*	Prints exec call with resetobject and the orders exec call 
*	included in the call.
*
*	step_exec( 'objectpointer', 'resetobject', 'orderlist' );
*	Ex:
*	step_exec( Z800005da, Z800005f0->ActualValue,
*	sorder_exec( Z8000084a, Z800005da, Z800005f0->ActualValue);
*	order_exec( Z800005d5, Z8000084a,
*	M8000084e_exec();
*	);
*	);
*
**************************************************************************/

int	gcg_comp_m13( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	int 			sts;
	int			size;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	int			k;
	vldh_t_plc		plc;
	pwr_sAttrRef		resattrref;
	pwr_sAttrRef		*resattrref_ptr;

	ldhses = (node->hn.wind)->hw.ldhses;  

	if ( gcgctx->reset_checked == FALSE )
	{
	  /* Check the resetobject in the plcobject */
	  plc = (node->hn.wind)->hw.plc;
	  sts = ldh_GetObjectPar( ldhses,
			plc->lp.oid, 
			"DevBody",
			"ResetObject",
			(char **)&resattrref_ptr, &size); 
	  if ( EVEN(sts)) return sts;

	  resattrref = *resattrref_ptr;
	  free((char *) resattrref_ptr);

	  /* Indicate that the reset object is checked */
	  gcgctx->reset_checked = TRUE;

	  sts = gcg_replace_ref( gcgctx, &resattrref, node);
	  if ( EVEN(sts)) return sts;

	  /* The reset object has to be a di, do or dv */
	  sts = ldh_GetAttrRefOrigTid( ldhses, &resattrref, &cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NORESET, node);
	    return GSX__NEXTNODE;
	  }

	  /* Check that the class of the reset object is correct */
	  if ( !(cid == pwr_cClass_Di ||
		 cid == pwr_cClass_Dv ||
		 cid == pwr_cClass_Po ||
		 cid == pwr_cClass_Do)) {
	    gcg_error_msg( gcgctx, GSX__CLASSRESET, node);
	    return GSX__NEXTNODE;
	  }
	  gcgctx->reset_object = resattrref;

	  /* Insert reset object in ioread list */
	  gcg_ioread_insert( gcgctx, resattrref, GCG_PREFIX_REF);

	}

	/* Insert step object in ref list */
	gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the execute command */
	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts;

	/* Print the reset object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s->ActualValue,\n",
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, gcgctx->reset_object));

	/* Check if there is any order connected to the outputpin */
	sts = gcg_get_point( node, 2, &point, &par_inverted);
	if ( ODD( sts)) {
	  gcgctx->step_comp = TRUE;
	  gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);

	  for ( k = 1; k < (int)point_count; k++) {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ((cid == pwr_cClass_order) && 
		 ( next_point == 0 )) {
	      /* compile this nodes here */
	      sts = gcg_node_comp( gcgctx, next_node);
	      if ( EVEN(sts)) return sts;
	    }
	    else {
	      /* Bad class */
/*
	      gcg_error_msg( gcgctx, GSX__NOORDER, node);
*/
	    }
	  }
	  if ( point_count > 0) free((char *) pointlist);
	  gcgctx->step_comp = FALSE;
	}

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m15()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for trans.
*
*	Syntax control:
*
*	Checks that there is only step objects connected to the 
*	step input and step output pin.
*	Checks that there is not both a condition subwindow and something
*	connected to the condition pin.
*
*	Code generation:
*
*	Prints declaration an directlink of rtdbpointer for the trans object.
*	Prints exec call with one list of input steps, one list
*	of output steps, and a funktion call for a subwindow or
*	a setcond_exec call.
*	If no condition window exists and the condition pin is not
*	connected no condition code is printed and the condition
*	can be set from the attribute editor.
*
*	trans_exec( 'objpointer', 
*	'insteplist', 'outsteplist', 
*	'condition code'
*	);
*	Ex:
*	trans_exec( Z800005c5, {&Z800005db->Status[0] _z_ 0}, 
*		{&Z800005da->Status[0] _z_ 0},
*	setcond_exec( Z800005c5, Z800005f8->ActualValue);
*	);
*
**************************************************************************/

int	gcg_comp_m15( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	int 			sts;
	int			size;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	int			k;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	int			pincond_found, wind_found;
	char			*delimstr = " _z_ ";
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;
	int			stepcount;


	ldhses = (node->hn.wind)->hw.ldhses;  


	/* Insert trans object in ref list */
	gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the execute command */
	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts;

	/* Print the trace condition, the trace parameter in the wind object
	 * If execution only when trace is active 	
	 * IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
	 *	"%c%lx->Trace,",
	 *	GCG_PREFIX_REF,
	 *	(node->hn.wind)->lw.oid);
	 */

	/* Print the step input list */	
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"{");

	/* Check if there is any step connected to the inputpin */
	sts = gcg_get_point( node, 0, &point, &par_inverted);
	if ( EVEN( sts)) return sts;

	gcg_get_conpoint_nodes( node, point, &point_count, &pointlist,
		GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	if ( point_count > 1 ) {
	  stepcount = 0;
	  for ( k = 1; k < (int)point_count; k++) {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( (cid == pwr_cClass_step && next_point == 2) ||
		 (cid == pwr_cClass_initstep && next_point == 2) ||
		 (cid == pwr_cClass_substep && next_point == 2) ||
		 (cid == pwr_cClass_ssbegin && next_point == 1) ) {
	      /* Print in the input list */
	      stepcount++;
	      if ( stepcount > 4) {
	        stepcount = 0;
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "\n");
	      }
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"&%c%s->Status[0]%s", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, next_node->ln.oid),
			delimstr);
	    }
	    else if (cid == pwr_cClass_trans ) {
	      /* If not parallell (same point nr) */
	      if ( next_point != point)
	        gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	    else {
	      /* Bad class */
	      gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	  }
	}
	else
	{
	  /* Not connected */
	  gcg_error_msg( gcgctx, GSX__GNOTCON, node);
	}
	if ( point_count > 0) free((char *) pointlist);

	/* Print the step output list */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"0}, {");

	/* Check if there is any step connected to the outputpin */
	sts = gcg_get_point( node, 1, &point, &par_inverted);
	if ( EVEN( sts)) return sts;

	gcg_get_conpoint_nodes( node, point, &point_count, &pointlist, 
		GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	if ( point_count > 1 ) {
	  for ( k = 1; k < (int)point_count; k++) {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( (cid == pwr_cClass_step && next_point == 0) ||
		 (cid == pwr_cClass_initstep && next_point == 0) ||
		 (cid == pwr_cClass_substep && next_point == 0) ||
		 (cid == pwr_cClass_ssend && next_point == 0)) {
	      /* Print in the input list */
	      stepcount++;
	      if ( stepcount > 4) {
	        stepcount = 0;
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "\n");
	      }
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"&%c%s->Status[0]%s", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, next_node->ln.oid),
			delimstr);
	    }
	    else if (cid == pwr_cClass_trans) {
	      /* If not parallell (same point nr) */
	      if ( next_point != point)
	        gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	    else {
	      /* Bad class */
	      gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	  }
	}
	else {
	  /* Not connected */
	  gcg_error_msg( gcgctx, GSX__GNOTCON, node);
	}
	if ( point_count > 0) free((char *) pointlist);
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"0},\n");


	/* Check if the condition pin is visible */
	sts = gcg_get_inputpoint( node, 2, &point, &par_inverted);
	if ( ODD( sts))
	{
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  pincond_found = 0;
	  if ( output_count > 0 )
	  {
	    pincond_found = 1;
	    if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);

	    sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return sts;
	    if ( EVEN(sts)) return sts;
	    
	    /* Print the execute for a setcond command */
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"setcond_exec( %c%s, ",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));
/*	    if ( par_inverted)
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"!");
*/
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			   "%c%s->%s);\n",
			   output_prefix,
			   output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(0, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			   output_par);
	  }
	}

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	wind_found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *)&windclass, &windbuffer, &size);
	  if ( ODD(sts)) 
	  {
	    free((char *) windbuffer);
	    wind_found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( wind_found )
	{
	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s_exec( tp);\n",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	}
	if ( wind_found && pincond_found )
	{
	  /* The pin is connected and there is a sub window */
	  gcg_error_msg( gcgctx, GSX__PINWIND, node);
	}
	if ( !(wind_found || pincond_found ))
	{
	  /* No window and no pinconnections, set condition true */
/*
*	  gcg_error_msg( gcgctx, GSX__NOPINORWIND, node);
*	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
*		"setcond_exec( %c%lx, 1);\n",
*		GCG_PREFIX_REF,
*		node->ln.oid);
*/
	}


	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m16()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for ORDER object.
*	The order code in included in the code of a step.
*	If the method is not called from the step method it returns 
*	immediately.
*
*	Syntax control:
*	Checks that the input pin is connected to a step.
*	Checks that if the condition pin is connected order has 
*	a C attribute.
*	Check that there is no mismatch between suborder objects and
*	the attributes.
*	Checks that there is not both a condition subwindow and some object
*	connected to the condition pin.
*
*	Code generation:
*	An object is created for every attribute of the order and
*	one object for the order itself. The orders are serially connected
*	with the first attribute first and the order object last.
*	Ex DSC order
*	stepobject - Dorder - Sorder - Corder - order
*
*	Ex Sorder med activity window
*	step_exec( Z800005da, Z800005f0->ActualValue,
*	sorder_exec( Z8000084a, Z800005da, Z800005f0->ActualValue);
*	order_exec( Z800005d5, Z8000084a,
*	M8000084e_exec( tp);
*	);
*	);
*
*	Declares and links pointers to the order object and all suborder
*	objects.
*	Prints an exec call for every suborder.
*	For a C suborder prints a setcond_exec call or a call for
*	a subwidow condition.
*	Prints an exec call for the order including
*	a activity subwindow exec call if such a window exists or
*	a call for the following objects if these ar connected to the
*	output pin
*	STODO, SETDO, RESDO, STODV, SETDV, RESDV, RESDP, SETDP, RESET_SO,
*	STODP
*	This objects will only be executed when the step is activ
*	when connected to an order. Other objects connected to an order
*	will follow the general rules. Also the code in the activity window
*	will be executed only when the step is active.
*
**************************************************************************/

int	gcg_comp_m16( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found;
	int			size;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	int			k;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	pwr_tObjid		step_objdid;
	pwr_tObjid		next_objdid;
	int			found, pincond_found, pinact_found, wind_found;
	char			*parvalue;
	pwr_tClassId		suborderclass[6];
	unsigned long		subordercount;
	unsigned long		ldhsubordercount;
	pwr_tObjid		windobjdid;
	unsigned long		windowindex;
	char			*name;

	ldhses = (node->hn.wind)->hw.ldhses;  

	if ( !gcgctx->step_comp)
	{
	  /* The order will be compile by the step when gcgctx->step_comp
	    is true */
	  return GSX__SUCCESS;
	}

	/* Get the step connected to conpoint 0 */
	sts = gcg_get_output( node, 0, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	if ( EVEN(sts)) return sts;

	if ( output_count > 0 ) {
	  output_found = 1;
	  if ( output_count > 1) 
	    gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);

	  /* Check that is it a step */
	  sts = ldh_GetObjectClass( ldhses, output_node->ln.oid, &cid);
	  if (EVEN(sts)) return sts;

	  if ( !(cid == pwr_cClass_step ||
	         cid == pwr_cClass_substep ||
	         cid == pwr_cClass_ssbegin ||
	         cid == pwr_cClass_ssend ||
	         cid == pwr_cClass_initstep)) {
	    gcg_error_msg( gcgctx, GSX__NOSTEP, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else {
	  /* Not connected */
	  gcg_error_msg( gcgctx, GSX__GNOTCON, node);  
	  return GSX__NEXTNODE;
	}
	
	step_objdid = output_node->ln.oid;

	/* Check if the condition pin is visible */
	sts = gcg_get_inputpoint( node, 1, &point, &par_inverted);
	pincond_found = 0;
	if ( ODD( sts))
	{
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  if ( output_count > 0 )
	  {
	    pincond_found = 1;
	    if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);

	    sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return sts;
	    if ( EVEN(sts)) return sts;
	    
	    /* Get the objdid for the Corder */
	    found = 0;
	    sts = ldh_GetChild( ldhses, node->ln.oid, &next_objdid);
	    while ( ODD(sts) ) {
	      /* Find out if this is a COrder */
	      sts = ldh_GetObjectClass( ldhses, next_objdid, &cid);
	      if (EVEN(sts)) return sts;

	      if ( cid == pwr_cClass_corder) {
	        found = 1;
	        break;
	      }
	      sts = ldh_GetNextSibling( ldhses, next_objdid, &next_objdid);
  	    }
	    if ( !found ) {
	      gcg_error_msg( gcgctx, GSX__NOCORDER, node);  
	      return GSX__NEXTNODE;
	    }
	    /* Print the execute for a setcond command */
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"setcond_exec( %c%s, ",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, next_objdid));
	    if ( par_inverted)
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"!");
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			   "%c%s->%s);\n",
			   output_prefix,
			   output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(0, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			   output_par);
	  }
	  else
	  {
	    /* Point visible but not connected, errormessage */
	    gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	  }
	}

	/* Check the attributes in the parent order node */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);

	if ( EVEN(sts) ) return sts;

/****************************************************************************/
       /* Rows doesn't represent the number of Order attributes.
        * When the attribute ShowAttrTime changed size the code didn't work anymore 
	* Temporary fix: substract rows with 2
	* Attributes ShowAttrTime and PlcNode -> rows - 2
        *
	* ML 950113
        */ 

	subordercount = 0;
	for ( i = 0; i < rows - 2; i += 2) {
	  /* Get the parameter value */
	  sts = ldh_GetObjectPar( (node->hn.wind)->hw.ldhses,  
			node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&parvalue, &size); 
	  if ( EVEN(sts)) return sts;

	  if ( *parvalue == 'S' || *parvalue == 's' ) {
	    suborderclass[subordercount] = pwr_cClass_sorder;
	    subordercount++;
	  }
	  else if ( *parvalue == 'L' || *parvalue == 'l') {
	    suborderclass[subordercount] = pwr_cClass_lorder;
	    subordercount++;
	  }
	  else if ( *parvalue == 'C' || *parvalue == 'c') {
	    suborderclass[subordercount] = pwr_cClass_corder;
	    subordercount++;
	  }
	  else if ( *parvalue == 'D' || *parvalue == 'd') {
	    suborderclass[subordercount] = pwr_cClass_dorder;
	    subordercount++;
	  }
	  else if ( *parvalue == 'P' || *parvalue == 'p') {
	    suborderclass[subordercount] = pwr_cClass_porder;
	    subordercount++;
	  }
	  else if ( *parvalue != 0) {
	    /* This orderattribute is not allowed */
	    gcg_error_msg( gcgctx, GSX__ORDERATTR, node);
	  }
	  free((char *) parvalue);
	}
	free((char *) bodydef);
	
	/* Get the objdid for order children of this node */
	found = 0;
	ldhsubordercount = 0;
	sts = ldh_GetChild( ldhses, node->ln.oid, &next_objdid);
	while ( ODD(sts) ) {
	  /* Find out if this is a suborder */
	  sts = ldh_GetObjectClass( ldhses, next_objdid, &cid);
	  if (EVEN(sts)) return sts;

	  if ( cid == pwr_cClass_sorder ||
	       cid == pwr_cClass_dorder ||
	       cid == pwr_cClass_lorder ||
	       cid == pwr_cClass_corder ||
	       cid == pwr_cClass_porder ) {
	    if ( cid != suborderclass[ldhsubordercount] ) {
	      return GSX__ORDERMISM;
	    }   
	    if ( cid == pwr_cClass_sorder) {
	      sts = gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	      /* Get name for execute call */
	      sts = gcg_get_structname( gcgctx, next_objdid, &name);
	      if( EVEN(sts)) return sts;

	      /* Print the execute command */
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, %c%s, %c%s->ActualValue);\n",
	        name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, next_objdid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, step_objdid),
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(2, gcgctx->reset_object));
	      step_objdid = next_objdid;
	    }
	    else if ( cid == pwr_cClass_lorder) {
	      /* Get the time */

	      sts = gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	      /* Print timer code */
	      gcg_timer_print( gcgctx, next_objdid);

	      /* Get name for execute call */
	      sts = gcg_get_structname( gcgctx, next_objdid, &name);
	      if( EVEN(sts)) return sts;

	      /* Print the execute command */
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, %c%s);\n",
	        name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, next_objdid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, step_objdid));
	      step_objdid = next_objdid;
	    }
	    else if ( cid == pwr_cClass_dorder) {
	      /* Get the time */

	      sts = gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	      /* Print timer code */
	      gcg_timer_print( gcgctx, next_objdid);
 
	      /* Get name for execute call */
	      sts = gcg_get_structname( gcgctx, next_objdid, &name);
	      if( EVEN(sts)) return sts;

	      /* Print the execute command */
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%s_exec( %c%s, %c%s);\n",
			name,
			GCG_PREFIX_REF,
			vldh_IdToStr(0, next_objdid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, step_objdid));
	      step_objdid = next_objdid;
	    }
	    else if ( cid == pwr_cClass_porder) {
	      sts = gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	      /* Get name for execute call */
	      sts = gcg_get_structname( gcgctx, next_objdid, &name);
	      if( EVEN(sts)) return sts;

	      /* Print the execute command */
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%s_exec( %c%s, %c%s);\n",
			name,
			GCG_PREFIX_REF,
			vldh_IdToStr(0, next_objdid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, step_objdid));
	      step_objdid = next_objdid;
	    }
	    else if ( cid == pwr_cClass_corder) {
	      sts = gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	      /* Get name for execute call */
	      sts = gcg_get_structname( gcgctx, next_objdid, &name);
	      if( EVEN(sts)) return sts;

	      /* Print the execute command */
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%s_exec( %c%s, %c%s, ",
			name,
			GCG_PREFIX_REF,
			vldh_IdToStr(0, next_objdid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, step_objdid));

	      /* Look for a condition subwindow */
	      wind_found = 0;
	      windowindex = 1;
	      if ( node->ln.subwindow & (windowindex + 1) )
	      {
	        wind_found = 1;
		windobjdid = node->ln.subwind_oid[windowindex];
	      }
	      if ( wind_found && pincond_found )
	      {
	        /* The pin is connected and there is a sub window */
	        gcg_error_msg( gcgctx, GSX__PINWIND, node);
	      }
	      else if ( !(wind_found || pincond_found ))
	      {
	        /* No window and no pinconnection */
	        gcg_error_msg( gcgctx, GSX__NOPINORWIND, node);
	      }
	      else if ( wind_found )
	      {
	        /* Print execute command for the subwindow */
	        IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			"%c%s_exec( tp);",
			GCG_PREFIX_MOD,
			vldh_IdToStr(0, windobjdid));
	      }
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			");\n");
	      step_objdid = next_objdid;
	    }
	    ldhsubordercount++;
	  }
	  sts = ldh_GetNextSibling( ldhses, next_objdid, &next_objdid);
	}

	if ( ldhsubordercount != subordercount )
	  return GSX__ORDERMISM;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for execute call */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	 /* Print the execute command */
	 IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, %c%s,\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, step_objdid));

	/* Check if the activity output pin is visible */
	sts = gcg_get_point( node, 2, &point, &par_inverted);
	pinact_found = 0;
	if ( ODD( sts))
	{
	  gcgctx->order_comp = TRUE;
	  /* Look for nodes connected to this point */
	  gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);

	  for ( k = 1; k < (int)point_count; k++) {
	    pinact_found = 1;
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( cid == pwr_cClass_stodo ||
	         cid == pwr_cClass_setdo ||
	         cid == pwr_cClass_resdo ||
	         cid == pwr_cClass_stodv ||
	         cid == pwr_cClass_setdv ||
	         cid == pwr_cClass_resdv ||
	         cid == pwr_cClass_setdp ||
	         cid == pwr_cClass_resdp ||
	         cid == pwr_cClass_reset_so ||
	         cid == pwr_cClass_stodp ||
                 cid == pwr_cClass_stodi ||
                 cid == pwr_cClass_setdi ||
                 cid == pwr_cClass_toggledi ||
                 cid == pwr_cClass_resdi) {
	      /* compile this nodes here */
	      sts = gcg_node_comp( gcgctx, next_node);
	      if ( EVEN(sts)) return sts;
	    }
	  }
	  if ( point_count > 0) free((char *) pointlist);
	  gcgctx->order_comp = FALSE;
	}

	/* Look for a activity subwindow */
	wind_found = 0;
	windowindex = 0;
	if ( node->ln.subwindow & (windowindex + 1) )
	{
	  wind_found = 1;
	  windobjdid = node->ln.subwind_oid[windowindex];
	}
	if ( wind_found && pinact_found )
	{
	  /* The pin is connected and there is a sub window */
	  gcg_error_msg( gcgctx, GSX__APINWIND, node);
	}
	else if ( !(wind_found || pinact_found ))
	{
	  /* No window and no pinconnections */
	  gcg_error_msg( gcgctx, GSX__ANOPINORWIND, node);
	}
	else if ( wind_found )
	{
	  /* There should be an activity window */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%c%s_exec( tp);\n",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windobjdid));
	}

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m11()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for STODO, STODV, STOAO, STOAV, SETDO, SETDV, 
*	RESDO, RESDV
*	If the object is connected to an order it will be called
*	from the order method.
*	
*	Syntax control:
*	Checks that the class of the referenced object is correct.
*
*	Code generation:
*	Declares and links a write pointer to the referenced object.
*	This will point to the valuebase object of the base frequency.
*	Prints an exec call.
*	stoav_exec( W800005f6, Z800005f5->ActualValue);
*
*	If the input is not connected the value of the inputparameter
*	will be printed instead of a input object for  sto obect. For set 
*	and reset object TRUE will printed.
*	stoav_exec( W800005f6, 3.2500);
*
**************************************************************************/


int	gcg_comp_m11( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	char			*nocondef_ptr;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2];
	char			*name;

	nocondef[1].bo = 1;
	nocontype[1] = GCG_BOOLEAN;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Check that this is objdid of an existing object */
	sts = ldh_GetObjectClass( ldhses, node->ln.oid, &cid);
	if ( EVEN(sts)) return sts;

	if ( !gcgctx->order_comp) {
	  if ( cid == pwr_cClass_stodo ||
	       cid == pwr_cClass_setdo ||
	       cid == pwr_cClass_resdo ||
	       cid == pwr_cClass_stodv ||
	       cid == pwr_cClass_setdv ||
	       cid == pwr_cClass_resdv) {
	    /* Check first if the object is connected to an order object,
	      if it is, it will be compiled when by the ordermethod, when 
	      gcgctx->order_comp is true */
	    sts = gcg_get_output( node, 0, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;
	    if ( output_count == 1)
	      if ( output_node->ln.cid == pwr_cClass_order)
	        return GSX__SUCCESS;
	  }
	}

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* If the object is not connected the value in the
	   parameter should be written in the macro call */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"RtBody",
			"In",
			&nocondef_ptr, &size); 
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that the class of the referenced object is correct */
	if ( ( node->ln.cid == pwr_cClass_setdo) ||	
	     ( node->ln.cid == pwr_cClass_resdo) ) {
	  if ( !(cid == pwr_cClass_Do ||
	         cid == pwr_cClass_Po)) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = TRUE;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_stodo) {
	  if ( !(cid == pwr_cClass_Do ||
	         cid == pwr_cClass_Po)) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = (int) *nocondef_ptr;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if (( node->ln.cid == pwr_cClass_setdv) ||	
		 ( node->ln.cid == pwr_cClass_resdv)) {
	  if ( cid != pwr_cClass_Dv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = TRUE;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_stodv) {
	  if ( cid != pwr_cClass_Dv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = (int) *nocondef_ptr;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( (node->ln.cid == pwr_cClass_stoao) ||
		  (node->ln.cid == pwr_cClass_cstoao)) {
	  if ( cid != pwr_cClass_Ao) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].fl = *(float *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( ( node->ln.cid == pwr_cClass_stoav) ||
		  (node->ln.cid == pwr_cClass_cstoav)) {
	  if ( cid != pwr_cClass_Av) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].fl = *(float *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( (node->ln.cid == pwr_cClass_stoio) ||
		  (node->ln.cid == pwr_cClass_cstoio))  {
	  if ( cid != pwr_cClass_Io) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = *(int *) nocondef_ptr;
	  nocontype[0] = GCG_INT32;
	}    
	else if ( ( node->ln.cid == pwr_cClass_stoiv) ||
		  (node->ln.cid == pwr_cClass_cstoiv)) {
	  if ( cid != pwr_cClass_Iv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = *(int *) nocondef_ptr;
	  nocontype[0] = GCG_INT32;
	}    
	else if ( node->ln.cid == pwr_cClass_stosv || 
	          node->ln.cid == pwr_cClass_cstosv) {
	  if ( cid != pwr_cClass_Sv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  strcpy( nocondef[0].str, (char *) nocondef_ptr);
	  nocontype[0] = GCG_STRING;
	}    
	else if ( node->ln.cid == pwr_cClass_StoATv || 
	          node->ln.cid == pwr_cClass_CStoATv) {
	  if ( cid != pwr_cClass_ATv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].atime = *(pwr_tTime *) nocondef_ptr;
	  nocontype[0] = GCG_ATIME;
	}    
	else if ( node->ln.cid == pwr_cClass_StoDTv || 
	          node->ln.cid == pwr_cClass_CStoDTv) {
	  if ( cid != pwr_cClass_DTv) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].dtime = *(pwr_tDeltaTime *) nocondef_ptr;
	  nocontype[0] = GCG_DTIME;
	}    
	free(nocondef_ptr);

        if ( cid == pwr_cClass_Sv ||
	     cid == pwr_cClass_ATv ||
	     cid == pwr_cClass_DTv) {
	  /* Insert io object in ref list */
	  gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	  /* Print the execute command */
	  sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	  if ( EVEN(sts)) return sts;	

	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
	        GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, refattrref));
        }
        else {
	  /* Insert io object in iowrite list */
	  gcg_iowrite_insert( gcgctx, refattrref, GCG_PREFIX_IOW);

	  /* Print the execute command */
	  sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	  if ( EVEN(sts)) return sts;	

	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
	        GCG_PREFIX_IOW,
		vldh_AttrRefToStr(0, refattrref));
	  if (EVEN(sts)) return sts; 
        }
	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts) ) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m12()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for STODP, SETDP, RESDP och STOAP.
*	If the object is connected to an order the routine is called
*	from the ordermethod.
*
*	Syntax control:
*	Check that the referenced object exists and that the referenced
*	parameter exists in this object, and that the object is of
*	the correct type.
*
*	Generating code:
*	Declares and links a rtdb pointer to the referenced object.
*	Prints an exec call.
*	If the input is not connected the value of the input parameter
*	is printed instead of the input for a sto. For a set or res
*	TRUE is printed.
*
**************************************************************************/

int	gcg_comp_m12( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	pwr_tAName     		aname;
	char			*name_p;
	ldh_sAttrRefInfo	info;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	char			*name;
	char			parameter[80];
	char			*nocondef_ptr;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2];
	char			*s;

	nocondef[1].bo = 1;
	nocontype[1] = GCG_BOOLEAN;

	ldhses = (node->hn.wind)->hw.ldhses;
	
	if ( !( node->ln.cid == pwr_cClass_StoAtoIp ||
		node->ln.cid == pwr_cClass_CStoAtoIp ||
		node->ln.cid == pwr_cClass_StoIp ||
		node->ln.cid == pwr_cClass_CStoIp ||
		node->ln.cid == pwr_cClass_stoap ||
		node->ln.cid == pwr_cClass_cstoap)) {
	  if ( !gcgctx->order_comp ) {
	    /* Check first if the object is connected to an order object,
	      if it is, it will be compiled when by the ordermethod, when 
	      gcgctx->order_comp is true */
	    sts = gcg_get_output( node, 0, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;
	    if ( output_count == 1)
	      if ( output_node->ln.cid == pwr_cClass_order)
	        return GSX__SUCCESS;
	  }
	}
	/* Get the objdid of the referenced object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	sts = ldh_GetAttrRefInfo( ldhses, &refattrref, &info);
	if ( EVEN(sts)) return sts;

	/* Get rid of last attribute segment of the referenced object */
	sts = ldh_AttrRefToName( ldhses, &refattrref, ldh_eName_Aref, 
				 &name_p, &size);
	if ( EVEN(sts)) return sts;

	strcpy( aname, name_p);
	if ( (s = strrchr( aname, '.')) == 0) { 
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTPAR;
	}

	*s = 0;
	sts = ldh_NameToAttrRef( ldhses, aname, &refattrref);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) return sts;
	
	sts = gcg_parname_to_pgmname(ldhses, cid, s+1, parameter);
	if ( EVEN(sts)) return sts;

	if ( info.flags & PWR_MASK_RTVIRTUAL) {
	  /* Attribute is not defined in runtime */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	} 

	if ( info.flags & PWR_MASK_ARRAY) {
	  if ( info.nElement == -1) {
	    /* No index in attribute */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  // if ( info.index > info.nElement - 1) {
	  //  /* Element index to large */
	  //  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  //  return GSX__NEXTNODE;
	  //}
	}

	switch ( info.type ) {
        case pwr_eType_Float32  : 
	  if ( !( node->ln.cid == pwr_cClass_stoap ||
	          node->ln.cid == pwr_cClass_cstoap)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Int32  : 
        case pwr_eType_UInt32  : 
        case pwr_eType_Int16  : 
        case pwr_eType_UInt16  : 
        case pwr_eType_Int8  : 
        case pwr_eType_UInt8  : 
        case pwr_eType_Enum  : 
        case pwr_eType_Mask  : 
        case pwr_eType_Status  : 
        case pwr_eType_NetStatus  : 
	  if ( !(node->ln.cid == pwr_cClass_StoAtoIp ||
		 node->ln.cid == pwr_cClass_CStoAtoIp ||
		 node->ln.cid == pwr_cClass_StoIp ||
		 node->ln.cid == pwr_cClass_CStoIp)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Boolean :
	  if ( !( node->ln.cid == pwr_cClass_stodp ||
		  node->ln.cid == pwr_cClass_setdp ||
		  node->ln.cid == pwr_cClass_resdp)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_String :
	  if ( !( node->ln.cid == pwr_cClass_stosp ||
	          node->ln.cid == pwr_cClass_cstosp ||
	          node->ln.cid == pwr_cClass_stonumsp ||
	          node->ln.cid == pwr_cClass_cstonumsp )) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_Time :
	  if ( !( node->ln.cid == pwr_cClass_StoATp ||
	          node->ln.cid == pwr_cClass_CStoATp )) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
        case pwr_eType_DeltaTime :
	  if ( !( node->ln.cid == pwr_cClass_StoDTp ||
	          node->ln.cid == pwr_cClass_CStoDTp )) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
	default:
	  /* Not allowed type */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	}

	/* If the object is not connected the value in the
	   parameter should be written in the macro call */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"RtBody",
			"In",
			&nocondef_ptr, &size); 
	if ( EVEN(sts)) return sts;

	/* Check that the class of the referenced object is correct */
	if ( node->ln.cid == pwr_cClass_setdp ||	
	     node->ln.cid == pwr_cClass_resdp) {
	  nocondef[0].bo = TRUE;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_stodp) {
	  nocondef[0].bo = (int)*nocondef_ptr;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_stoap ||
		  node->ln.cid == pwr_cClass_cstoap) {
	  nocondef[0].fl = *(float *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( node->ln.cid == pwr_cClass_StoAtoIp ||
		  node->ln.cid == pwr_cClass_CStoAtoIp ||
		  node->ln.cid == pwr_cClass_StoIp ||
		  node->ln.cid == pwr_cClass_CStoIp) {
	  nocondef[0].bo = *(int *) nocondef_ptr;
	  nocontype[0] = GCG_INT32;
	}    
	else if ( node->ln.cid == pwr_cClass_stosp ||
	          node->ln.cid == pwr_cClass_cstosp ||
	          node->ln.cid == pwr_cClass_stonumsp || 
	          node->ln.cid == pwr_cClass_cstonumsp)  {
	  strcpy( nocondef[0].str, (char *) nocondef_ptr);
	  nocontype[0] = GCG_STRING;
	}    
	else if ( node->ln.cid == pwr_cClass_StoATp ||
		  node->ln.cid == pwr_cClass_CStoATp) {
	  nocondef[0].atime = *(pwr_tTime *) nocondef_ptr;
	  nocontype[0] = GCG_ATIME;
	}    
	else if ( node->ln.cid == pwr_cClass_StoDTp ||
		  node->ln.cid == pwr_cClass_CStoDTp) {
	  nocondef[0].dtime = *(pwr_tDeltaTime *) nocondef_ptr;
	  nocontype[0] = GCG_DTIME;
	}    
	free(nocondef_ptr);

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s->%s, ",
		name,
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, refattrref),
		parameter);

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts) ) return sts;

	if ( node->ln.cid == pwr_cClass_stosp ||
	     node->ln.cid == pwr_cClass_cstosp ) {
          // Add size of connected attribute
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		",%d", info.size);
        }
	else if ( node->ln.cid == pwr_cClass_stonumsp ||
		  node->ln.cid == pwr_cClass_cstonumsp ) {
          // Add size of connected attribute and number of characters
	  pwr_tInt32 *numberofchar_ptr;

	  sts = ldh_GetObjectPar( ldhses, node->ln.oid, "DevBody",
			"NumberOfChar", (char **)&numberofchar_ptr, &size); 
	  if ( EVEN(sts)) return sts;

	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		",%d,%d", info.size, *numberofchar_ptr);
	  free( (char *)numberofchar_ptr);
        }

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");

	return GSX__SUCCESS;
}



/*************************************************************************
*
* Name:		gcg_comp_m17()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for csub.
*	Syntax control:
*	Check that a subwindow exists.
*
*	Generating code:
*	Declares and links a rtdb pointer to the csup object.
*	Prints an exec call including a exec call for the subwindow.
*	If the input is not connected the input is set to TRUE.
*
**************************************************************************/

int	gcg_comp_m17( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts, found, size;
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;
	ldh_tSesContext ldhses;
	char			*name;
	gcg_t_nocondef		nocondef;
	unsigned long		nocontype = GCG_BOOLEAN;

	nocondef.bo = TRUE;
	ldhses = (node->hn.wind)->hw.ldhses; 

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( ",
		name);

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		&nocondef, &nocontype);
	if ( EVEN(sts)) return sts;

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	/* Get the first child to the plc */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, &windbuffer, &size);
	  free((char *) windbuffer);
	  if ( ODD(sts)) 
	  {
	    found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( !found )
	{
	  /* The window is not created */
	  gcg_error_msg( gcgctx, GSX__NOSUBWINDOW, node);
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		",\n");
	}
	else
	{
	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		" ,%c%s_exec( tp);",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	}

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m18()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for RESET_SO
*	If the object is connected to an order it is called from
*	the order method.
*
*	Syntax control:
*	Checks that the referenced order exists, and that it is an
*	s-order.
*
*	Generating code.
*	Declares and links a rtdb pointer to the Sorder object.
*	Prints an exec call.
*	reset_so_exec( Z8000084a, Z800005d3->Status[0]);
*
**************************************************************************/

int	gcg_comp_m18( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	int			size;
	pwr_tObjid		next_objdid;
	pwr_tObjid		refobjdid;
	pwr_tObjid		*refobjdid_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	int			found;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;

	ldhses = (node->hn.wind)->hw.ldhses; 
	
	if ( !gcgctx->order_comp)
	{
	  /* Check first if the object is connected to an order object,
	    if it is, it will be compiled when by the ordermethod, when 
	    gcgctx->order_comp is true */
	  sts = gcg_get_output( node, 0, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;
	  if ( output_count == 1)
	    if ( output_node->ln.cid == pwr_cClass_order)
	      return GSX__SUCCESS;
	}

	/* Get the objdid of the referenced io object stored in the
	  first parameter devbody */

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"OrderObject",
			(char **)&refobjdid_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refobjdid = *refobjdid_ptr;
	free((char *) refobjdid_ptr);

	/* Check that this is objdid of an existing object */
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Get the objdid for the Sorder witch is a child of the order */
	 /* Check if this order has a COrder as a child */	
	found = 0;
	sts = ldh_GetChild( ldhses, refobjdid, &next_objdid);
	while ( ODD(sts) )
	{
	  /* Find out if this is a Sorder */
	  sts = ldh_GetObjectClass( ldhses, next_objdid, &cid);
	  if (EVEN(sts)) return sts;

	  if ( cid == pwr_cClass_sorder) {
	    found = 1;
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, next_objdid, &next_objdid);
	}
	if ( !found )
	{
	  gcg_error_msg( gcgctx, GSX__NOSORDER, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert sorder and reset_so object in ref list */

	gcg_ref_insert( gcgctx, next_objdid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, next_objdid, GCG_PREFIX_REF);
	if ( EVEN(sts))	return sts;

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, NULL,
		NULL);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 	");\n");
	
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m19()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for ORDERACT
*	Syntax control:
*	Checks that the parent is an order object and that it is 
*	equivalent to the referenced orderobject.
*
*	Generating code:
*	Declares and links a pointer to the referenced order object.
*
**************************************************************************/

int	gcg_comp_m19( gcg_ctx gcgctx, vldh_t_node node)
{
	pwr_tObjid		refobjdid;
	pwr_tObjid		*refobjdid_ptr;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	int 			sts;
	int			size;

	ldhses = (node->hn.wind)->hw.ldhses; 
	
	/* Get the objdid of the referenced order object stored in the
	  first parameter devbody */

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"OrderObject",
			(char **)&refobjdid_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refobjdid = *refobjdid_ptr;
	free((char *) refobjdid_ptr);

	if ( cdh_ObjidIsNull( refobjdid))
	{	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that parent is an order object */
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( cid != pwr_cClass_order) {
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}



	/* Check that this is objdid is the same as the parent node */
	if ( cdh_ObjidIsNotEqual( (node->hn.wind)->lw.poid,
		refobjdid)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}	  

	/* Insert order ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m20()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for SETCOND
*	Checks that the parent object of the window is a trans or
*	an order with a C attribute.
*	Checks that the referenced order is equivalent to the parent object.
*
*	Generating code:
*	Declares and links a rtdb pointer to the parent object.
*	Prints an exec call.
* 	setcond_exec( Z800005c6, Z800005cc->Status);
*
**************************************************************************/

int	gcg_comp_m20( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	pwr_tObjid		next_objdid;
	pwr_tObjid		refobjdid;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	int			found;
	char			*name;

	ldhses = (node->hn.wind)->hw.ldhses; 
	
	/* Get the parent node to this window, it should be either
	  a trans or a order object */
	refobjdid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull( refobjdid))
	{	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( !(cid == pwr_cClass_trans ||
	       cid == pwr_cClass_order)) {
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	if ( cid == pwr_cClass_order) {
	  /* Get the objdid for the Corder witch is a child of the order */
	   /* Check if this order has a COrder as a child */	
	  found = 0;
	  sts = ldh_GetChild( ldhses, refobjdid, &next_objdid);
	  while ( ODD(sts)) {
	    /* Find out if this is a COrder */
	    sts = ldh_GetObjectClass( ldhses, next_objdid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( cid == pwr_cClass_corder) {
	      found = 1;
	      break;
	    }
	    sts = ldh_GetNextSibling( ldhses, next_objdid, &next_objdid);
  	  }
	  if ( !found ) {
	    gcg_error_msg( gcgctx, GSX__NOCORDER, node);  
	    return GSX__NEXTNODE;
	  }
	  /* The referenced order is the Corder */
	  refobjdid = next_objdid;
	}
	/* Insert referenced object in ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, refobjdid));

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, NULL,
		NULL);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");
	
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m21()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an object where a rtdb reference should be made
*	but no exec command.
*	Generated code:
*	Declares and links a rtdb pointer to the object.
*
**************************************************************************/

int	gcg_comp_m21( gcg_ctx gcgctx, vldh_t_node node)
{
	int 	sts;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m25()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for ASUP, DSUP.
*	Generated code:
*	Declares and links a rtdb pointer to the sup object.
*	Prints an exec call.
*	If the enable input is not connected it is set to true.
*	Inits the object by putting the supervised object and parameter 
*	into the object and setting AckFlg to true.
*
**************************************************************************/

int	gcg_comp_m25( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	pwr_sAttrRef 		*a_ptr;
	pwr_sAttrRef 		aref;
	pwr_tOid 		parent;
	int 			keep = 0;
	int			size;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2] = { GCG_BOOLEAN, GCG_BOOLEAN };

	nocondef[0].bo = FALSE;
	nocondef[1].bo = TRUE;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	/* Print some init commands */

	/* ++ LW 12-MAY-1992 20:44:29.45 */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"Sup_init(%c%s);\n", 
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));
	/* -- LW 12-MAY-1992 20:44:29.45 */

	/* Print the supervised object and parameter in the object */
	sts = gcg_get_inputpoint( node, 0, &point, &par_inverted);
	if ( ODD( sts))
	{
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  if ( output_count > 0 )
	  {
	    sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return sts;
	    if ( EVEN(sts)) return sts;
	    
	    /* Put the attribut reference in the sup object */
	    IF_PR {
	      /* If a not plc object is assigned, keep this object */
	      sts = ldh_GetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody",
				      "Attribute", (char **)&a_ptr, &size); 
	      if ( EVEN(sts)) return sts;

	      aref = *a_ptr;
	      free((char *) a_ptr);
	      if ( !cdh_ObjidIsNull( aref.Objid)) {
		sts = gcg_replace_ref( gcgctx, &aref, node);
		if ( sts == GSX__REPLACED) {
		  keep = 1;
		  /* Store replaced aref */
		  sts = ldh_SetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
					  "Attribute", (char *)&aref, sizeof( aref)); 
		  if ( EVEN(sts)) return sts;
		}
		else {
		  sts = ldh_GetParent( gcgctx->ldhses, aref.Objid, &parent);
		  if ( ODD(sts)) {
		    if ( !gcg_is_window( gcgctx, parent)) {
		      /* Keep this attribute */
		      keep = 1;
		    }
		  }
		}
	      }
	      if ( !keep) {
		sts = ldh_SetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
				      "Attribute", (char *)&output_attrref, 
				      sizeof( output_attrref)); 
		if ( EVEN(sts)) return sts;
	      }
	    }
	  }
	  else {
	    /* Point not connected, errormessage */
	    gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	  }
	}

	gcg_timer_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m22()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for AARITHM or LARITHM.
*	This object contains a macro call but the connections
*	is put into the rtdb object as pointers.
*
*	Syntax control:
*	Checks that there is an expession stored in the 
*	expression parameter.
*	Generated code:
*	Declares an links a rtdb pointer to the arithm object.
*	Prints a #define command for the expression.
*	Prints an exec call.
*	If an input is not connected it will point to its
*	own object.
*
**************************************************************************/

int	gcg_comp_m22( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	ldh_tSesContext ldhses;
	char			*expression;

	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Get c-expression stored in devbody */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"Expression",
			(char **)&expression, &size); 
	if ( EVEN(sts)) return sts;

	if ( *expression == '\0')
	  /* There is no expression */
	  gcg_error_msg( gcgctx, GSX__NOEXPR, node);

	/* Print the expression */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "#define EXPR%s(A1,A2,A3,A4,A5,A6,A7,A8,d1,d2,d3,d4,d5,d6,d7,d8) ",
	  vldh_IdToStr(0, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "%s\n\n",
	  expression);
	free((char *) expression);

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "%s_exec( %c%s,EXPR%s(*%c%s->AIn1P,*%c%s->AIn2P,\n*%c%s->AIn3P,*%c%s->AIn4P,\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(4, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(5, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"*%c%s->AIn5P,*%c%s->AIn6P,\n*%c%s->AIn7P,*%c%s->AIn8P,\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"*%c%s->DIn1P,*%c%s->DIn2P,\n*%c%s->DIn3P,*%c%s->DIn4P,\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"*%c%s->DIn5P,*%c%s->DIn6P,\n*%c%s->DIn7P,*%c%s->DIn8P));\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses,
			node->ln.cid, "RtBody", 1,
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1)
	        gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR )
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted )
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE],
			"%c%s->%sP = &%c%s->%s;\n",
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m23()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for POS3P, INC3P.
*	Syntax control:
*	Checks that the referenced DoOpen and DoClose objects exists
*	and is of class Do.
*
*	Generated code:
*	Mainly as in method 4.
*	Declare and link rtdb pointer to the xxx3p object and 
*	to the doclose and doopen object (read pointers).
*	Puts the doopen and doclose objects into the rtdb object.
*
*	
*	
*
**************************************************************************/

int	gcg_comp_m23( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	int			size;
	pwr_sAttrRef		doopen_attrref, doclose_attrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses;  
	

	/* Compile with method 4 */
	sts = (gcg_comp_m[4]) (gcgctx, node); 
	if ( EVEN(sts)) return sts;

	/* Get objdid of the open do */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"DoOpen",
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	doopen_attrref = *refattrref_ptr;
	free((char *) refattrref_ptr);

	sts = gcg_replace_ref( gcgctx, &doopen_attrref, node);
	if ( EVEN(sts)) return sts;

	/* Get objdid of the close do */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"DoClose",
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	doclose_attrref = *refattrref_ptr;
	free((char *) refattrref_ptr);

	sts = gcg_replace_ref( gcgctx, &doclose_attrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that class of the objdids is ok */
	sts = ldh_GetAttrRefOrigTid( ldhses, &doopen_attrref, &cid);
	if ( EVEN(sts)) {
	  /* No doopen object */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->OpenP = &%c%s->TimerDODum;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	}
	else {
	  /* Check that the class of the referenced object is correct */
	  if ( !(cid == pwr_cClass_Do ||
	         cid == pwr_cClass_Po)) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  /* Insert open do in io write list */
	  gcg_iowrite_insert( gcgctx, doopen_attrref, GCG_PREFIX_IOW);
	  /* Put the rtdbreferences to the do's in the rtdbobject */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->OpenP = &%c%s->ActualValue;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_IOW,
			vldh_AttrRefToStr(1, doopen_attrref));
	}

	/* Check the close do */
	sts = ldh_GetAttrRefOrigTid( ldhses, &doclose_attrref, &cid);
	if ( EVEN(sts)) {
	  /* No doclose object */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->CloseP = &%c%s->TimerDODum;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	}
	else {
	  /* Check that the class of the referenced object is correct */
	  if ( !(cid == pwr_cClass_Do ||
	         cid == pwr_cClass_Po)) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }

	  gcg_iowrite_insert( gcgctx, doclose_attrref, GCG_PREFIX_IOW);

	  /* Put the rtdbreferences to the do's in the rtdbobject */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->CloseP = &%c%s->ActualValue;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_IOW,
			vldh_AttrRefToStr(1, doclose_attrref));
	}

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m24()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for substep. The same as for an ordinary step
*	with some exeptions.
*	Syntax control:
*	Checks that there is a subwindow to the object.
*	
*	Generated code:
*	Prints an exec call to the subwindow in the substep_exec call.
*
**************************************************************************/

int	gcg_comp_m24( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	int 			sts;
	int			size;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	int			k;
	int			found;
	vldh_t_plc		plc;
	pwr_sAttrRef 		resattrref;
	pwr_sAttrRef		*resattrref_ptr;
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;

	ldhses = (node->hn.wind)->hw.ldhses;  

	if ( gcgctx->reset_checked == FALSE )
	{
	  /* Check the resetobject in the plcobject */
	  plc = (node->hn.wind)->hw.plc;
	  sts = ldh_GetObjectPar( ldhses,
			plc->lp.oid, 
			"DevBody",
			"ResetObject",
			(char **)&resattrref_ptr, &size); 
	  if ( EVEN(sts)) return sts;

	  resattrref = *resattrref_ptr;
	  free((char *) resattrref_ptr);

	  /* Indicate that the reset object is checked */
	  gcgctx->reset_checked = TRUE;

	  sts = gcg_replace_ref( gcgctx, &resattrref, node);
	  if ( EVEN(sts)) return sts;

	  /* The reset object has to be a di, do or dv */
	  sts = ldh_GetAttrRefOrigTid( ldhses, &resattrref, &cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NORESET, node);
	    return GSX__NEXTNODE;
	  }

	  /* Check that the class of the reset object is correct */
	  if ( !(cid == pwr_cClass_Di ||
		 cid == pwr_cClass_Dv ||
		 cid == pwr_cClass_Po ||
		 cid == pwr_cClass_Do)) {
	    gcg_error_msg( gcgctx, GSX__CLASSRESET, node);
	    return GSX__NEXTNODE;
	  }
	  gcgctx->reset_object = resattrref;

	  /* Insert reset object in ioread list */
	  gcg_ioread_insert( gcgctx, resattrref, GCG_PREFIX_REF);

	}

	/* Insert step object in ref list */
	gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the execute command */
	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts;

	/* Print the reset object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s->ActualValue,",
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, gcgctx->reset_object));

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	/* Get the first child to the plc */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, &windbuffer, &size);
	  free((char *) windbuffer);
	  if ( ODD(sts)) 
	  {
	    found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( !found )
	{
	  /* The window is not created */
	  gcg_error_msg( gcgctx, GSX__NOSUBWINDOW, node);
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		",\n");
	}
	else
	{
	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s_exec( tp);,\n",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	}

	/* Check if there is any order connected to the outputpin */
	sts = gcg_get_point( node, 2, &point, &par_inverted);
	if ( ODD( sts))
	{
	  gcgctx->step_comp = TRUE;
	  gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);

	  for ( k = 1; k < (int)point_count; k++) {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if (cid == pwr_cClass_order && 
		next_point == 0) {
	      /* compile this nodes here */
	      sts = gcg_node_comp( gcgctx, next_node);
	      if ( EVEN(sts)) return sts;
	    }
	    else {
	      /* Bad class */
/*
	      gcg_error_msg( gcgctx, GSX__NOORDER, next_node);
*/
	    }
	  }
	  if ( point_count > 0) free((char *) pointlist);
	  gcgctx->step_comp = FALSE;
	}

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m26()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for ssbegin and ssend.
*	Mainly the same as for an ordinary step.
*	Syntax control:
*	Checks that the parent object of the window is a substep.
*	Generated code:
*	Declares and links a rtdb pointer to the parent object.
*
**************************************************************************/

int	gcg_comp_m26( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	int 			sts;
	int			size;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	int			k;
	vldh_t_plc		plc;
	pwr_sAttrRef		resattrref;
	pwr_tObjid		refobjdid;
	pwr_sAttrRef		*resattrref_ptr;
	unsigned long		orderpar_index = 1;

	ldhses = (node->hn.wind)->hw.ldhses;  

	/* Get the parent node to this window, it should be either
	  a trans or a order object */
	refobjdid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull( refobjdid))
	{	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass( ldhses, refobjdid, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( !(cid == pwr_cClass_substep)) {
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert parent object in ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	if ( gcgctx->reset_checked == FALSE )
	{
	  /* Check the resetobject in the plcobject */
	  plc = (node->hn.wind)->hw.plc;
	  sts = ldh_GetObjectPar( ldhses, plc->lp.oid, 
				  "DevBody", "ResetObject",
				  (char **)&resattrref_ptr, &size); 
	  if ( EVEN(sts)) return sts;

	  resattrref = *resattrref_ptr;
	  free((char *) resattrref_ptr);

	  /* Indicate that the reset object is checked */
	  gcgctx->reset_checked = TRUE;

	  sts = gcg_replace_ref( gcgctx, &resattrref, node);
	  if ( EVEN(sts)) return sts;
		  
	  /* The reset object has to be a di, do or dv */
	  sts = ldh_GetAttrRefOrigTid( ldhses, &resattrref, &cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NORESET, node);
	    return GSX__NEXTNODE;
	  }

	  /* Check that the class of the reset object is correct */
	  if ( !(cid == pwr_cClass_Di ||
		 cid == pwr_cClass_Dv ||
		 cid == pwr_cClass_Po ||
		 cid == pwr_cClass_Do)) {
	    gcg_error_msg( gcgctx, GSX__CLASSRESET, node);
	    return GSX__NEXTNODE;
	  }
	  gcgctx->reset_object = resattrref;

	  /* Insert reset object in ioread list */
	  gcg_ioread_insert( gcgctx, resattrref, GCG_PREFIX_REF);

	}

	/* Insert step object in ref list */
	gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the execute command */
	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts;

	/* Print the parent object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s, ",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, refobjdid));

	/* Print the reset object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s->ActualValue,\n",
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, gcgctx->reset_object));

	/* Check if there is any order connected to the outputpin */
	sts = gcg_get_point( node, orderpar_index, &point, 
		&par_inverted);
	if ( ODD( sts))
	{
	  gcgctx->step_comp = TRUE;
	  gcg_get_conpoint_nodes( node, point, 
			&point_count, &pointlist, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);

	  for ( k = 1; k < (int)point_count; k++) {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( cid == pwr_cClass_order && 
		 next_point == 0) {
	      /* compile this nodes here */
	      sts = gcg_node_comp( gcgctx, next_node);
	      if ( EVEN(sts)) return sts;
	    }
	    else {
	      /* Bad class */
/*
	      gcg_error_msg( gcgctx, GSX__NOORDER, node);
*/
	    }
	  }
	  if ( point_count > 0) free((char *) pointlist);
	  gcgctx->step_comp = FALSE;
	}

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m28()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for sum.
*	Method for with the exeption that an input parameter is
*	not visible the pointerparameter for input is set to NULL.
*
**************************************************************************/

int	gcg_comp_m28( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will = NULL */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = NULL;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m29()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method maxmin.
*	The same as  method 4 with the exeption that only the first
*	or the second input will point to its own object if not connected,
*	the other will be set to NULL.
*
**************************************************************************/

int	gcg_comp_m29( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR )
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found && ( i < 2 ))
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  else if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = NULL;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m30()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method drive.
*	The same as method 4 with the exeption that the pointers of
*	the parameters Speed and ConOn will point to the parameter Order,
*	and AutoNoStop will point to AutoStart, if they are not connected.
*
**************************************************************************/

int	gcg_comp_m30( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found && ((strcmp(bodydef[i].Par->Param.Info.PgmName,"Speed") == 0)
		     || (strcmp(bodydef[i].Par->Param.Info.PgmName,"ConOn") == 0)))
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->Order;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	  }
	  else if ( !output_found && 
	    strcmp(bodydef[i].Par->Param.Info.PgmName,"AutoNoStop") == 0)
	  {
	    /* The point AutoNoStart is not connected and will point to 
	       AutoStart in its own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->AutoStart;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	  }
	  else if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	gcg_timer_print( gcgctx, node->ln.oid);
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m31()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method MValve.
*	The same as method 4 with the exeption that the pointer of
*	the parameter ConOpen will point to the parameter OrderOpen
*	if ConOpen is not connected and ConClose will point to OrderClose.
*
**************************************************************************/

int	gcg_comp_m31( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found && 
		(strcmp(bodydef[i].Par->Param.Info.PgmName,"ConOpen") == 0))
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->OrderOpen;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	  }
	  else if ( !output_found && 
		(strcmp(bodydef[i].Par->Param.Info.PgmName,"ConClose") == 0))
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->OrderClose;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	  }
	  else if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	/* Print timer code */
	gcg_timer_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m32()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method filter and ramp.
*	The same as method 4 with the exeption that the pointer of
*	the parameter FeedB will point to the parameter ActVal
*	if FeedB is not connected.
*
*
**************************************************************************/

int	gcg_comp_m32( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found && 
	     (strcmp(bodydef[i].Par->Param.Info.PgmName,"FeedB") == 0))
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->ActVal;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));
	  }
	  else if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */
	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	gcg_scantime_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m33()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for a mpstrp object.
*
*	Syntax control:
*
*	Checks that there is only one object connected to the 
*	cell input and cell output pin.
*	Checks that there is not both a trigg subwindow and some object
*	connected to the trigg pin.
*
*	Code generation:
*
*	Prints declaration an directlink of rtdbpointer for the trp object.
*	Prints exec call with a funktion call for a subwindow or
*	a triggtrp_exec call.
*	The name of connected cell objects are put into the trans object.
*
**************************************************************************/

int	gcg_comp_m33( gcg_ctx gcgctx, vldh_t_node node)
{
#if 0
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	int 			sts;
	int			size;
	int			k;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	int			pincond_found, wind_found;
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;
	char			*name;
	unsigned long		point_count;
	vldh_t_conpoint		*pointlist;
	vldh_t_node		next_node;
	unsigned long		next_point;
	unsigned long		cellcount;
	pwr_tOName     		hier_name;

	ldhses = (node->hn.wind)->hw.ldhses; 

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for the exec command for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;	

	/* Check if there is any step connected to the inputpin */
	sts = gcg_get_point( node, 0, &point, &par_inverted);
	if ( EVEN( sts)) return sts;

	cellcount = 0;
	gcg_get_conpoint_nodes( node, point, &point_count, &pointlist, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	if ( point_count > 1 )
	{
	  for ( k = 1; k < point_count; k++)
	  {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( 
		(cid == vldh_uclass( ldhses, "MpsIn")) || 
		(cid == vldh_uclass( ldhses, "MpsOut")) || 
		(cid == vldh_uclass( ldhses, "MpsTube")) || 
		(cid == vldh_uclass( ldhses, "MpsStack")) || 
		(cid == vldh_uclass( ldhses, "MpsWare")) || 
		(cid == vldh_uclass( ldhses, "MpsQueue")) ) 
	    {
	      cellcount++;
	      /* Print in the input list */
	      /* Get the name of the connected object */
	      sts = ldh_ObjidToName( 
			gcgctx->ldhses,
			next_node->ln.oid, ldh_eName_Hierarchy,
			hier_name, sizeof( hier_name), &size);
	      if( EVEN(sts)) return sts;

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"strcpy( &%c%s->NameOfSendCell, \"%s\");\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			hier_name);

	    }
	    else if (cid != vldh_uclass( ldhses, "MpsTrp") )
	    {
	      /* Bad class */
	      gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	  }
	}
	else
	{
	  /* Not connected */
	  gcg_error_msg( gcgctx, GSX__NOTCON, node);
	}
	if ( point_count > 0) free((char *) pointlist);
	if ( cellcount > 1 )
	{
	  /* More than one cell connected */
	  gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);
	}

	/* Check if there is any step connected to the outputpin */
	sts = gcg_get_point( node, 2, &point, &par_inverted);
	if ( EVEN( sts)) return sts;

	cellcount = 0;
	gcg_get_conpoint_nodes( node, point, &point_count, &pointlist, 
		GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	if ( point_count > 1 )
	{
	  for ( k = 1; k < point_count; k++)
	  {
	    next_node = (pointlist + k)->node;
	    next_point = (pointlist + k)->conpoint;
	    /* Check class of connected nodes */	
	    sts = ldh_GetObjectClass( ldhses, next_node->ln.oid, &cid);
	    if (EVEN(sts)) return sts;

	    if ( 
		(cid == vldh_uclass( ldhses, "MpsIn")) || 
		(cid == vldh_uclass( ldhses, "MpsOut")) || 
		(cid == vldh_uclass( ldhses, "MpsTube")) || 
		(cid == vldh_uclass( ldhses, "MpsStack")) || 
		(cid == vldh_uclass( ldhses, "MpsWare")) || 
		(cid == vldh_uclass( ldhses, "MpsQueue")) ) 
	    {
	      cellcount++;
	      /* Print in the input list */
	      /* Get the name of the connected object */
	      sts = ldh_ObjidToName( 
			gcgctx->ldhses,
			next_node->ln.oid, ldh_eName_Hierarchy,
			hier_name, sizeof( hier_name), &size);
	      if( EVEN(sts)) return sts;

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"strcpy( &%c%s->NameOfReceivCell, \"%s\");\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			hier_name);

	    }
	    else if (cid != vldh_uclass( ldhses, "MpsTrp") )
	    {
	      /* Bad class */
	      gcg_error_msg( gcgctx, GSX__NOSTEP, node);
	    }
	  }
	}
	else
	{
	  /* Not connected */
	  gcg_error_msg( gcgctx, GSX__NOTCON, node);
	}
	if ( point_count > 0) free((char *) pointlist);
	if ( cellcount > 1 )
	{
	  /* More than one cell connected */
	  gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);
	}

	/* Check if the condition pin is visible */
	sts = gcg_get_inputpoint( node, 1, &point, &par_inverted);
	if ( ODD( sts))
	{
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  pincond_found = 0;
	  if ( output_count > 0 )
	  {
	    pincond_found = 1;
	    if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, node);

	    sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return sts;
	    if ( EVEN(sts)) return sts;
	    
	    /* Print the execute for a triggtrp command */
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"mpsTriggtrp_exec( %c%s, ",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));
/*	    if ( par_inverted)
	      IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"!");
*/
	    IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
			   "%c%s->%s);\n",
			   output_prefix,
			   output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(0, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			   output_par);
	  }
	}

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	wind_found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, &windbuffer, &size);
	  if ( ODD(sts)) 
	  {
	    free((char *) windbuffer);
	    wind_found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( wind_found )
	{
	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s_exec( tp);\n",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	}
	if ( wind_found && pincond_found )
	{
	  /* The pin is connected and there is a sub window */
	  gcg_error_msg( gcgctx, GSX__PINWIND, node);
	}

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));
#endif
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m34()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for MPSTRIGGTRP
*	Syntax control:
*	It should be tested that the parent object of the window is a
*	trp object, but this is not implemented.
*	Generated code:
*	Declares and links a pointer to the parentobject to the window.
*	Prints an exec call.
*
**************************************************************************/

int	gcg_comp_m34( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	pwr_tObjid		refobjdid;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	char			*name;

	ldhses = (node->hn.wind)->hw.ldhses; 
	
	/* Get the parent node to this window, it should be either
	  a trans or a order object */
	refobjdid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull( refobjdid))
	{	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

/*	if ( !(cid == vldh_class( ldhses, VLDH_CLASS_TRP)))
	{
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}
*/
	/* Insert referenced object in ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, refobjdid));

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, NULL,
		NULL);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");
	
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m35()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for standard function call node with 
*	initialization function.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints code for initialization of pointers in the object:
*	'objpointer'->'pgmname'P = &'in';
*	Z80000811->InP = &Z800005f5->ActualValue;
*
*	If a parameter is not connected or not visible the pointer
*	will point to the own object:
*	
*	'objpointer'->'pgmname'P = &'objpointer'->'pgmname';
*	Z80000811->LimP = &Z80000811->Lim;
*
*	Prints an init call :
*	'structname'_init( 'objpointer');
*	ex: pispeed_init( Z80000811);
*
*	Prints an exec call :
*	'structname'_exec( 'objpointer');
*	ex: pispeed_exec( tp, Z80000811);
*
*	Prints iniatilization for timer and scantime if the object 
*	contain these functions.
*	
**************************************************************************/

int	gcg_comp_m35( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	pwr_sAttrRef		*connect_aref;
	int			size;
	pwr_tCid		cid;

	/* Check if there is a PlcConnected */
	sts = ldh_GetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
				"PlcConnect", (char **)&connect_aref, &size);
	if ( ODD(sts)) {
	  pwr_sAttrRef aref = *connect_aref;
	  free( (char *)connect_aref);
	  sts = gcg_replace_ref( gcgctx, &aref, node);
	  if ( ODD(sts)) {
	    /* Store the converted aref */
	    sts = ldh_SetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
				"PlcConnect", (char *)&aref, sizeof(aref));
	  }

	  if ( cdh_ObjidIsNull( aref.Objid)) {
	    gcg_error_msg( gcgctx, GSX__NOCONNECT, node);
	    return GSX__NEXTNODE;
	  }

	  // Check that object exist
	  sts = ldh_GetAttrRefOrigTid( gcgctx->ldhses, &aref, &cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NOCONNECT, node);
	    return GSX__NEXTNODE;
	  }
	}

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	gcg_timer_print( gcgctx, node->ln.oid);
	gcg_scantime_print( gcgctx, node->ln.oid);

	/* Print the init command */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"%s_init( %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m36()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GetIpToA, GetIp and GetDatap.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints an exec call :
*	'structname'_exec( 'objpointer');
*	ex: GetIpToA_exec( Z80000811);
*
*	Checks that the referenced object exists and that the referenced
*	parameter exists in that object, and that the type of the parameter
*	is correct.
*	Prints declaration and direct link of pointer to referenced object.
*
**************************************************************************/

int	gcg_comp_m36( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	pwr_tAName     		aname;
	char			*name_p;
	char			*s;
	ldh_sAttrRefInfo	info;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	char			parameter[80];

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	sts = ldh_GetAttrRefInfo( ldhses, &refattrref, &info);
	if ( EVEN(sts)) return sts;

	/* Get rid of last attribute segment of the referenced object */
	sts = ldh_AttrRefToName( ldhses, &refattrref, ldh_eName_Aref, 
				 &name_p, &size);
	if ( EVEN(sts)) return sts;

	strcpy( aname, name_p);
	if ( (s = strrchr( aname, '.')) == 0) { 
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTPAR;
	}

	*s = 0;
	sts = ldh_NameToAttrRef( ldhses, aname, &refattrref);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) return sts;
	
	sts = gcg_parname_to_pgmname(ldhses, cid, s+1, parameter);
	if ( EVEN(sts)) return sts;

	if ( info.flags & PWR_MASK_RTVIRTUAL) {
	  /* Attribute is not defined in runtime */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	} 

	if ( info.flags & PWR_MASK_ARRAY) {
	  if ( info.nElement == -1) {
	    /* No index in attribute */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  // if ( info.index > info.nElement - 1) {
	  //  /* Element index to large */
	  //  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  //  return GSX__NEXTNODE;
	  //}
	}

	if ( info.flags & PWR_MASK_POINTER) {
	  if ( node->ln.cid != pwr_cClass_GetDatap) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else {
	  switch ( info.type ) {
	  case pwr_eType_Int32: 
	  case pwr_eType_UInt32: 
	  case pwr_eType_Int16: 
	  case pwr_eType_UInt16: 
	  case pwr_eType_Int8: 
	  case pwr_eType_UInt8: 
	  case pwr_eType_Enum: 
	  case pwr_eType_Mask: 
	  case pwr_eType_Status: 
	  case pwr_eType_NetStatus: 
	    if ( !(node->ln.cid != pwr_cClass_GetIpToA ||
		   node->ln.cid != pwr_cClass_GetIp)) {
	      gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	      return GSX__NEXTNODE;
	    }
	    break;
	  default:
	    /* Not allowed type */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	/* Print the parent object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s->%s);\n",
		GCG_PREFIX_REF,
		vldh_AttrRefToStr(0, refattrref),
		parameter);

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m37()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for BACKUP
*	Puts the connected parameter in the parameter 'DataName'
*	in the backup object.
*
*	Prints code for declaration and direkt link of a rtdbpointer.
*
*	Prints an init call :
*	'structname'_init( 'objpointer', "'backup parameter'");
*	ex: Backup_init( Z80000811, "Temperature.ActualValue");
*
**************************************************************************/

int	gcg_comp_m37( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	int			size;
	ldh_tSesContext 	ldhses;
	pwr_tAName	       	parname;	
	pwr_tOName     		conn_obj;
	char			conn_par[80];	
	vldh_t_node		conn_node;
	unsigned long		point = 0;
	char			*wholeobject;
	pwr_sAttrRef		pararef;

	ldhses = (node->hn.wind)->hw.ldhses;  

	/* Look if the whole object should be backed up */
	sts = ldh_GetObjectPar( ldhses, node->ln.oid, 
			"RtBody", "ObjectBackup", (char **)&wholeobject, &size); 
	if ( EVEN(sts)) return sts;

	/* Get the connected object and parameter */
	sts = gcg_get_connected_par_close( node, point, &conn_node, conn_obj,
		conn_par);
	if ( sts == GSX__NOTCON)
	{
	  /* Point visible but not connected, errormessage */
	  gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	  strcpy( parname, "");
	}
	else if ((sts == GSX__NEXTPAR) || ( sts == GSX__SWINDERR)
		|| (sts == GSX__REFPAR))
	{
	  /* No backup on this parameter */
	  gcg_error_msg( gcgctx, GSX__NOBACKUP, node);  
	  strcpy( parname, "");
	}
	else if ( EVEN(sts)) {
	  /* Parameter can't be found */
	  gcg_error_msg( gcgctx, GSX__CONBACKUP, node);  
	  strcpy( parname, "");
	}
	else
	{
	  strcpy( parname, conn_obj);
	  if ( !(*wholeobject))
	  {
	    strcat( parname, ".");
	    strcat( parname, conn_par);
	  }
	}	
	free((char *) wholeobject);

	sts = ldh_NameToAttrRef( ldhses, parname, &pararef);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	/* Put the object and parameter in the backup object */
        IF_PR
	{
	  sts = ldh_SetObjectPar( ldhses, node->ln.oid,
		"RtBody", "DataName", (char *)&pararef, sizeof( pararef)); 
	  if ( EVEN(sts)) return sts;
	}
#if 0
	char			*name;
	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the init command */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"%s_init( %c%s,\"%s\");\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		parname);

#endif
	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m38()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for STODI, SETDI, RESDI, TOGGLEDI, STOAI, CSTOAI
*	If the object is connected to an order it will be called
*	from the order method.
*	
*	Syntax control:
*	Checks that the class of the referenced object is correct.
*
*	Code generation:
*	Declares and links a write pointer to the referenced object.
*	This will point to the valuebase object of the base frequency.
*	Prints an exec call.
*	stoai_exec( W800005f6, Z800005f5->ActualValue);
*
*	If the input is not connected the value of the inputparameter
*	will be printed instead of a input object for  sto obect. For set 
*	and reset object TRUE will printed.
*	stoai_exec( W800005f6, 3.2500);
*
**************************************************************************/


int	gcg_comp_m38( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	char			*nocondef_ptr;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2];
	char			*name;

	nocondef[1].bo = 1;
	nocontype[1] = GCG_BOOLEAN;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Check that this is objdid of an existing object */
	sts = ldh_GetObjectClass( ldhses, node->ln.oid, &cid);
	if ( EVEN(sts)) return sts;

        if ( !gcgctx->order_comp) {
          if ( cid == pwr_cClass_stodi ||
	       cid == pwr_cClass_setdi ||
	       cid == pwr_cClass_toggledi ||
	       cid == pwr_cClass_resdi) {
            /* Check first if the object is connected to an order object,
              if it is, it will be compiled when by the ordermethod, when
              gcgctx->order_comp is true */
            sts = gcg_get_output( node, 0, &output_count, &output_node,
                        &output_point, &output_bodydef,
                        GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
            if ( EVEN(sts)) return sts;
            if ( output_count == 1)
              if ( output_node->ln.cid == pwr_cClass_order)
                return GSX__SUCCESS;
          }
        }

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* If the object is not connected the value in the
	   parameter should be written in the macro call */
	sts = ldh_GetObjectPar( ldhses, node->ln.oid, 
				"RtBody", "In", &nocondef_ptr, &size); 
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that the class of the referenced object is correct */
	if ( node->ln.cid == pwr_cClass_setdi ||	
	     node->ln.cid == pwr_cClass_toggledi ||	
	     node->ln.cid == pwr_cClass_resdi) {
	  if ( cid != pwr_cClass_Di) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = TRUE;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_stodi) {
	  if ( cid != pwr_cClass_Di) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = (int)*nocondef_ptr;
	  nocontype[0] = GCG_BOOLEAN;
	}
	else if ( (node->ln.cid == pwr_cClass_stoai) ||
		  (node->ln.cid == pwr_cClass_cstoai)) {
	  if ( cid != pwr_cClass_Ai) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].fl = *(float *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( (node->ln.cid == pwr_cClass_stoii) ||
		  (node->ln.cid == pwr_cClass_cstoii)) {
	  if ( cid != pwr_cClass_Ii) {
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = *(int *) nocondef_ptr;
	  nocontype[0] = GCG_INT32;
	}    
	free(nocondef_ptr);

	/* Insert io object in iowrite list */
	gcg_iowrite_insert( gcgctx, refattrref, GCG_PREFIX_IOW);

	/* Print the execute command */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
		GCG_PREFIX_IOW,
		vldh_AttrRefToStr(0, refattrref));

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts) ) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m39()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for STOPI
*	
*	Syntax control:
*	Checks that the class of the referenced object is correct.
*
*	Code generation:
*	Declares and links a write pointer to the referenced object.
*	This will point to the valuebase object of the base frequency.
*	Prints an exec call.
*	stopi_exec( W800005f6, X800005f6, Z800005f5->ActualValue);
*
*	If the input is not connected the value of the inputparameter
*	will be printed instead of a input object for  sto obect
*	stopi_exec( W800005f6, X800005f6, 32500);
*
**************************************************************************/


int	gcg_comp_m39( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	char			*nocondef_ptr;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2];
	char			*name;

	nocondef[1].bo = 1;
	nocontype[1] = GCG_BOOLEAN;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Check that this is objdid of an existing object */
	sts = ldh_GetObjectClass( ldhses, node->ln.oid, &cid);
	if ( EVEN(sts)) return sts;

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	/* If the object is not connected the value in the
	   parameter should be written in the macro call */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"RtBody",
			"In",
			&nocondef_ptr, &size); 
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( cid != pwr_cClass_Co) {
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}
	nocondef[0].bo = *(int *) nocondef_ptr;
	nocontype[0] = GCG_INT32;

	free(nocondef_ptr);

	/* Insert io object in iowrite list */
	gcg_iowrite_insert( gcgctx, refattrref, GCG_PREFIX_IOW);
	gcg_iowrite_insert( gcgctx, refattrref, GCG_PREFIX_IOCW);

	/* Print the execute command */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;	

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, ",
		name,
		GCG_PREFIX_IOW,
		vldh_AttrRefToStr(0, refattrref));

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s, ",
		GCG_PREFIX_IOCW,
		vldh_AttrRefToStr(0, refattrref));

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts) ) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_40()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for CARITHM.
*	This object contains a macro call but the connections
*	is put into the rtdb object as pointers.
*
*	Syntax control:
*	Checks that there is an expession stored in the 
*	expression parameter.
*	Generated code:
*	Declares an links a rtdb pointer to the arithm object.
*	Prints a #define command for the expression.
*	Prints an exec call.
*	If an input is not connected it will point to its
*	own object.
*
**************************************************************************/

int	gcg_comp_m40( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	ldh_tSesContext ldhses;
	char			*expression;
	char			*s;

	/* Same method as dataarithm */
	sts = gcg_comp_m42( gcgctx, node);
	return sts;

	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Get c-expression stored in devbody */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"Code",
			(char **)&expression, &size); 
	if ( EVEN(sts)) return sts;

	if ( *expression == '\0')
	  /* There is no expression */
	  gcg_error_msg( gcgctx, GSX__NOEXPR, node);

	/* Print the expression */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "#define EXPR%s(A1,A2,A3,A4,A5,A6,A7,A8,d1,d2,d3,d4,d5,d6,d7,d8,\
OA1,OA2,OA3,OA4,OA5,OA6,OA7,OA8,od1,od2,od3,od4,od5,od6,od7,od8) ", 
	  vldh_IdToStr(0, node->ln.oid));
	IF_PR
	{
	  s = expression;
	  while ( *s != 0)
	  {
	    if ( *s == 10)
	      fputc( '\\', gcgctx->files[GCGM1_CODE_FILE]);
	    fputc( *s, gcgctx->files[GCGM1_CODE_FILE]);
	    s++;
	  }
	  fprintf( gcgctx->files[GCGM1_CODE_FILE], "\n\n");
	}

	free((char *) expression);

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "%s_exec( %c%s,\n\
EXPR%s((*%c%s->AIn1P),(*%c%s->AIn2P),(*%c%s->AIn3P),(*%c%s->AIn4P),\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(4, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(5, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(*%c%s->AIn5P),(*%c%s->AIn6P),(*%c%s->AIn7P),(*%c%s->AIn8P),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(*%c%s->DIn1P),(*%c%s->DIn2P),(*%c%s->DIn3P),(*%c%s->DIn4P),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(*%c%s->DIn5P),(*%c%s->DIn6P),(*%c%s->DIn7P),(*%c%s->DIn8P),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	  "(%c%s->OutA1),(%c%s->OutA2),(%c%s->OutA3),(%c%s->OutA4),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(4, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(%c%s->OutA5),(%c%s->OutA6),(%c%s->OutA7),(%c%s->OutA8),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(%c%s->OutD1),(%c%s->OutD2),(%c%s->OutD3),(%c%s->OutD4),\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"(%c%s->OutD5),(%c%s->OutD6),(%c%s->OutD7),(%c%s->OutD8)));\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(1, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(2, node->ln.oid),
		GCG_PREFIX_REF,
		vldh_IdToStr(3, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1) 
	        gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m41()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for a NMpsTrp object.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints code for initialization of pointers in the object:
*	'objpointer'->'pgmname'P = &'in';
*	Z80000811->InP = &Z800005f5->ActualValue;
*
*	The two first input parameters (In and Out) are initialized
*	'objpointer'->'pgmname'P = 'inobject_pointer';
*	Z80000811->InP = Z80000812;
*
*	If a parameter is not connected or not visible the pointer
*	will point to the own object:
*	
*	'objpointer'->'pgmname'P = &'objpointer'->'pgmname';
*	Z80000811->LimP = &Z80000811->Lim;
*
*	Prints an exec call :
*	'structname'_exec( tp, 'objpointer');
*	ex: compl_exec( tp, Z80000811);
*
*	Prints iniatilization for timer and scantime if the object 
*	contain these functions.
*	
**************************************************************************/

int	gcg_comp_m41( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( tp, %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      if ( i >= 2)
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	      }
	      else
	      {
	        IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = %c%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref));
	      }
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	gcg_timer_print( gcgctx, node->ln.oid);
	gcg_scantime_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m42()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for DATAARITHM.
*	This converts and puts the code in the dataarithm object
*	into the code file. The connections
*	are put into the rtdb object as pointers.
*
*	Syntax control:
*	Checks that there is an expession stored in the 
*	expression parameter.
*	Generated code:
*	Declares an links a rtdb pointer to the arithm object.
*	Converts the code with the lex and yacc generated compiler
*	pwr_exe:ds_foe_dataarithm.exe, by placing the code in an
*	input file the the compiler and then copying the output
*	file of the compiler into the codefile.
*	If an input is not connected it will point to its
*	own object.
*
**************************************************************************/

int	gcg_comp_m42( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	ldh_tSesContext ldhses;
	char			*expression;
	char			*newstr;
	char			error_line[80];
	int			error_num;
	int			error_line_size = sizeof(error_line);
	char			pointer_name[80];
	int			codesize;

#define DATAA_BUFF_SIZE 16000

	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Get c-expression stored in devbody */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"Code",
			(char **)&expression, &size); 
	if ( EVEN(sts)) return sts;

	if ( *expression == '\0')
	  /* There is no expression */
	  gcg_error_msg( gcgctx, GSX__NOEXPR, node);

	sprintf( pointer_name, "%c%s", GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid));

	newstr = (char *)calloc( 1, DATAA_BUFF_SIZE);
	if ( !newstr) 
	  return FOE__NOMEMORY;

	sts = dataarithm_convert( expression, newstr, pointer_name, 
		DATAA_BUFF_SIZE, error_line, &error_line_size, &error_num,
		&codesize);
	if (EVEN(sts))
	{
	  gcg_error_msg( gcgctx, sts, node);
	  printf( "Error in line %d,\n  %s\n", error_num, error_line);
	  free( newstr);
	  free((char *) expression);
	  return GSX__NEXTNODE;
	}
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], "%s", newstr);

	free((char *) expression);
	free( newstr);

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1) 
	        gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m43()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GetData
*	Prints code for declaration and direkt link of a rtdbpointer.
*
*	Checks that the referenced object exists and that the referenced
*	parameter exists in that objekt, and that the type of the parameter
*	is correct.
*	Prints declaration and direct link of pointer to referenced object.
*
* 	Initializes the Output to point att the referenced object
*	ex: Z80000811->Out = Z80000812;
*	Puts the objid of the refereced object in rtbody. Originaly
*	is it put in devbody of graphical reasons.
*
**************************************************************************/

int	gcg_comp_m43( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses, node->ln.oid, 
			"DevBody", bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refattrref = *refattrref_ptr;
	free((char *) bodydef);	
	free((char *) refattrref_ptr);

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}
	/* Check that this is a class, not a type */
	if ( !cdh_tidIsCid( cid)) {
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	/* The Out parameter will contain the pointer to the
	   referenced object */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->Out = %c%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_REF,
			vldh_AttrRefToStr(0, refattrref));

	/* Put referenced object in rt body */
        IF_PR {
	  sts = ldh_SetObjectPar( ldhses, node->ln.oid,
		"RtBody", "DataObjid", (char *)&refattrref.Objid, sizeof( refattrref.Objid)); 
	  if ( EVEN(sts)) return sts;
	}

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m44()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for CurrentData and CurrentIndex
*	Syntax control:
*	Checks that the parent is an cell object.
*
*	Generating code:
*	Declares and links a pointer to the referenced cell object.
*
**************************************************************************/

int	gcg_comp_m44( gcg_ctx gcgctx, vldh_t_node node)
{
	pwr_tObjid		refobjdid;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	int 			sts;

	ldhses = (node->hn.wind)->hw.ldhses; 
	
	/* Check that parent is an order object */
	refobjdid =  node->hn.wind->lw.poid;
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	if ( !((cid == vldh_eclass( ldhses, "NMpsCell")) ||
	       (cid == vldh_eclass( ldhses, "CLoop")) ||
	       (cid == vldh_eclass( ldhses, "NMpsMirrorCell")) ||
	       (cid == vldh_eclass( ldhses, "NMpsStoreCell"))))
	{
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert order ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m45()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method a datacopy.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints code for initialization of pointers in the object:
*	'objpointer'->'pgmname'P = &'in';
*	Z80000811->InP = &Z800005f5->ActualValue;
*
*	If a parameter is not connected or not visible the pointer
*	will point to the own object:
*	
*	'objpointer'->'pgmname'P = &'objpointer'->'pgmname';
*	Z80000811->LimP = &Z80000811->Lim;
*
*	Prints an exec call :
*	'structname'_exec( 'objpointer');
*	ex: compl_exec( Z80000811);
*
**************************************************************************/

int	gcg_comp_m45( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_par[80];
	char			output_prefix;
	char			*name;
	char			*refstructname;
	pwr_tObjid		refobjdid;
	pwr_tObjid		*refobjdid_ptr;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	int			dataclass_found;	

	ldhses = (node->hn.wind)->hw.ldhses;
	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	dataclass_found = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      if ( (i <= 1) && !dataclass_found)
	      {
	        /* Check if this is a GetData object */
	        sts = ldh_GetObjectClass( (node->hn.wind)->hw.ldhses,
			output_node->ln.oid,
			&cid);
	        if( EVEN(sts)) return sts;
	        if ( cid == pwr_cClass_GetData)
	        {

	          /* Get the class of the data in the connected GetData */
	          sts = ldh_GetObjectPar( (node->hn.wind)->hw.ldhses,
			output_node->ln.oid, 
			"DevBody",
			"DataObject",
			(char **)&refobjdid_ptr, &size); 
	          if ( EVEN(sts)) return sts;

	          refobjdid = *refobjdid_ptr;
	          free((char *) refobjdid_ptr);

	          sts = gcg_get_structname( gcgctx, refobjdid, &refstructname);
	          if ( EVEN(sts))
	            return GSX__NEXTNODE;

		  dataclass_found = 1;
	        }
	      }
	      output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);	

/*
	if ( !dataclass_found )
*/

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s, sizeof(pwr_sClass_%s));\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid),
		refstructname);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m46()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for NMpsCell.
*	Syntax control:
*	Check if a subwindow exists.
*
*	Generating code:
*	Identical to method 4 with the following addition:
*	Prints an special exec call including a exec call for the subwindow.
*
**************************************************************************/

int	gcg_comp_m46( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts, found, size;
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;
	ldh_tSesContext ldhses;
	char			*name;
	pwr_tObjid		*resobjid_ptr;
	pwr_tAttrRef		resattrref;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses; 

	sts = gcg_comp_m4( gcgctx, node);
	if ( EVEN(sts)) return sts;

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the init command */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
		"%s_init( %c%s);\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	/* Get the resetobject if there is one */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid,
			"RtBody",
			"ResetObject",
			(char **)&resobjid_ptr, &size);
	if ( EVEN(sts)) return sts;

	resattrref = cdh_ObjidToAref( *resobjid_ptr);
	free((char *) resobjid_ptr);

	if ( cdh_ObjidIsNotNull( resattrref.Objid)) {
	  sts = gcg_replace_ref( gcgctx, &resattrref, node);
	  if ( EVEN(sts)) return sts;

	  /* The reset object has to be a di, do or dv */
	  sts = ldh_GetAttrRefOrigTid( ldhses, &resattrref,
			&cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NORESET, node);
	    return GSX__NEXTNODE;
	  }

	  /* Check that the class of the reset object is correct */
	  if ( !(cid == pwr_cClass_Di ||
		 cid == pwr_cClass_Dv ||
		 cid == pwr_cClass_Po ||
		 cid == pwr_cClass_Do)) {
	    gcg_error_msg( gcgctx, GSX__CLASSRESET, node);
	    return GSX__NEXTNODE;
	  }

	  /* Insert reset object in ioread list */
	  gcg_ioread_insert( gcgctx, resattrref, GCG_PREFIX_REF);

	  /* Place the pointer to the resetobject in the cell */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->ResetObjectP = &%c%s->ActualValue;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_REF,
			vldh_AttrRefToStr(1, resattrref));
	}
	else
	{
	  /* Inget resetobjekt f�r cellen */
	  IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->ResetObjectP = 0;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid));
	}

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	/* Get the first child to the plc */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, &windbuffer, &size);
	  free((char *) windbuffer);
	  if ( ODD(sts)) 
	  {
	    found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( found )
	{

	  /* Get name for this class */
	  sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	  if( EVEN(sts)) return sts;

	  /* Print the execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%sSubWind_exec( %c%s, ",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));


	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s_exec( tp)",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
	  ");\n");

	}

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m47()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for AtLog, DtLog.
*	Generated code:
*	Declares and links a rtdb pointer to the sup object.
*	Prints an exec call.
*	Inits the object by putting the supervised object and parameter 
*	into the object.
*
**************************************************************************/

int	gcg_comp_m47( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	gcg_t_nocondef		nocondef;
	unsigned long		nocontype = GCG_BOOLEAN;

	nocondef.bo = FALSE;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		&nocondef, &nocontype);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
	");\n");

	/* Print the supervised object and parameter in the object */
	sts = gcg_get_inputpoint( node, 0, &point, &par_inverted);
	if ( ODD( sts))
	{
	  /* Look for an output connected to this point */
	  sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef,
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	  if ( EVEN(sts)) return sts;

	  if ( output_count > 0 )
	  {
	    sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	    if ( sts == GSX__NEXTPAR ) return sts;
	    if ( EVEN(sts)) return sts;
	    
#if 0
	    /* Get the name of the connected object */
	    sts = ldh_ObjidToName( 
		gcgctx->ldhses,
		output_objdid, ldh_eName_Hierarchy,
		hier_name, sizeof( hier_name), &size);
	    if( EVEN(sts)) return sts;

	    sts = ldh_NameToAttrRef( gcgctx->ldhses, hier_name, &attrref);
	    if ( EVEN(sts)) return sts;
#endif
	    /* Put the attribut reference in the sup object */
	    IF_PR
	    {
	      sts = ldh_SetObjectPar( gcgctx->ldhses, node->ln.oid,
		"RtBody", "Attribute", (char *)&output_attrref, sizeof( output_attrref)); 
	      if ( EVEN(sts)) return sts;
	    }
	  }
	  else
	  {
	    /* Point not connected, errormessage */
	    gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	  }
	}

	gcg_timer_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m48()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for ExecPlcOn and ExecPlcOff.
*	Syntax control:
*	Check that the objid of a plcpgm is inserted.
*
*	Generating code:
*	Declares and links a rtdb pointer to the ExecPlc object.
*	Declares and links a rtdb pointer to the window of the
*	refereced plcpgm.
*	Prints an exec call including a pointer to the attribute
*	ScanOff in the window of the plc.
*
**************************************************************************/

int	gcg_comp_m48( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts, found, size;
	char			*windbuffer;
	ldh_tSesContext		ldhses;
	char			*name;
	gcg_t_nocondef		nocondef;
	unsigned long		nocontype = GCG_BOOLEAN;
	pwr_tObjid		plcpgm;
	pwr_tObjid		*parvalue;
	pwr_tClassId		cid;
	pwr_tClassId		eclass;
	pwr_tObjid		window;
    
	nocondef.bo = TRUE;

	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Get the plcpgm in attribute plcpgmobject */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"PlcPgmObject",
			(char **)&parvalue, &size); 
	if ( EVEN(sts)) return sts;
	plcpgm = *parvalue;
	free((char *) parvalue);

	/* Check that it is a plcpgm and that is has a plcwindow as a child */
	sts = ldh_GetObjectClass ( ldhses, plcpgm, &cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTNODE;
	}
	sts = ldh_GetChild( ldhses, plcpgm, &window);
	found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			window, "DevBody", "PlcWindow", (pwr_eClass *) 
			&eclass, &windbuffer, &size);
	  free((char *) windbuffer);
	  if ( ODD(sts))
	  {
	    found = 1;
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, window, &window);
	}
	if ( !found)
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);
	sts = gcg_ref_insert( gcgctx, window, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%s_exec( %c%s,",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR,
		&nocondef, &nocontype);
	if ( EVEN(sts)) return sts;

	/* Print the attribute ScanOff of the window */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		", %c%s->ScanOff);\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, window));

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m49()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an object where the
*	the contens of the first parameter of type objid in devbody
*	will be copied to the forst parameter of type objid in rtbody.
* 	This is because only devbody parameters kan be shown in the plc-editor.
*	No exec call will be printed, but direct link will be made.
*
**************************************************************************/

int	gcg_comp_m49( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			i;
	int			size, found;
	pwr_tObjid		*parvalue;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Check the attributes in the parent order node */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);

	if ( EVEN(sts) ) return sts;

	found = 0;
	for ( i = 0; i < rows; i++)
	{
	  if ( bodydef[i].Par->Param.Info.Type == pwr_eType_Objid)
	  {
	    /* Get the parameter value */
	    sts = ldh_GetObjectPar( (node->hn.wind)->hw.ldhses,  
			node->ln.oid, 
			"DevBody",
			bodydef[i].ParName,
			(char **)&parvalue, &size); 
	    if ( EVEN(sts)) return sts;
	    found = 1;
	    break;
	  }
	}
	free((char *) bodydef);	
	if ( !found )
	{
	  free((char *) parvalue);	
	  return GSX__SUCCESS;
	}
	
	/* Check that objid exists */
	sts = ldh_GetObjectClass ( ldhses, *parvalue, &cid);
	if ( EVEN(sts))
	{
	  gcg_error_msg( gcgctx, GSX__REFPAR, node);  
	  return GSX__NEXTNODE;
	}
	
	/* Store value in RtBody */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	found = 0;
	for ( i = 0; i < rows; i++)
	{
	  if ( bodydef[i].Par->Param.Info.Type == pwr_eType_Objid)
	  {
	    /* Set the parameter value */
	    sts = ldh_SetObjectPar( ldhses,
			node->ln.oid, 
			"RtBody",
			bodydef[i].ParName,
			(char *)parvalue, size); 
	    if ( EVEN(sts)) return sts;
	    found = 1;
	    break;
	  }
	}
	free((char *) bodydef);	
	free((char *) parvalue);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m50()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an object with an subwindow.
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints code for initialization of pointers in the object:
*	'objpointer'->'pgmname'P = &'in';
*	Z80000811->InP = &Z800005f5->ActualValue;
*
*	If a parameter is not connected or not visible the pointer
*	will point to the own object:
*	
*	'objpointer'->'pgmname'P = &'objpointer'->'pgmname';
*	Z80000811->LimP = &Z80000811->Lim;
*
*	Prints an exec call :
*	'structname'_exec( 'objpointer', 'subwindow_exec');
*	ex: compl_exec( Z80000811, Z80000812_exec);
*
*	Prints iniatilization for timer and scantime if the object 
*	contain these functions.
*	
**************************************************************************/

int	gcg_comp_m50( gcg_ctx gcgctx, vldh_t_node node)
{
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	int 			wind_found;
	char			*windbuffer;
	pwr_tObjid		windowobjdid;
	pwr_tClassId		windclass;
	ldh_tSesContext ldhses;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the function call to execute the subwindow */
	/* Check first that there is a subwindow */
	/* Get the first child to the plc */
	sts = ldh_GetChild( ldhses, node->ln.oid, &windowobjdid);
	wind_found = 0;
	while ( ODD(sts) )
	{
	  /* Check if window */
	  sts = ldh_GetObjectBuffer( ldhses,
			windowobjdid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, &windbuffer, &size);
	  free((char *) windbuffer);
	  if ( ODD(sts)) 
	  {
	    wind_found = 1;
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, windowobjdid, &windowobjdid);
	}

	if ( !wind_found )
	{
	  /* The window is not created */
	  gcg_error_msg( gcgctx, GSX__NOSUBWINDOW, node);
	}

  	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s,\n",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));


	/* Print the window execute command */
	if ( wind_found)
	{
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s_exec( tp)",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, windowobjdid));
	}
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
	");\n");

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	gcg_timer_print( gcgctx, node->ln.oid);
	gcg_scantime_print( gcgctx, node->ln.oid);

	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_comp_m51()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for NMps Collect objects.
*	Syntax control:
*
*	Generating code:
*	Identical to method 4 with the following addition:
*	Puts pointer an objid of its own object in the output
*	to admitt connection to a dataarithm object.
*
**************************************************************************/

int	gcg_comp_m51( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	ldh_tSesContext 	ldhses;

	ldhses = (node->hn.wind)->hw.ldhses; 

	sts = gcg_comp_m4( gcgctx, node);
	if ( EVEN(sts)) return sts;

	/* Place the pointer to the object in the output */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->OutDataP = %c%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid));

	/* Place the objid of the object in the output */
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->OutData_ObjId.oix = %u;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			node->ln.oid.oix);
	IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->OutData_ObjId.vid = %u;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			node->ln.oid.vid);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m52()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GETDAP
*	Prints code for declaration and direkt link of a rtdbpointer.
*	Prints an exec call :
*	'structname'_exec( 'objpointer');
*	ex: getpi_exec( Z80000811);
*
*	Checks that the referenced object exists and that the referenced
*	parameter exists in that objekt, and that the type of the parameter
*	is correct.
*	Prints declaration and direct link of pointer to referenced object.
*
**************************************************************************/

int	gcg_comp_m52( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	pwr_tObjid		refobjdid;
	pwr_tObjid		*refobjdid_ptr;
	char			*parameter;
	unsigned long		info_type;
	int			found, i;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	int			info_pointer_flag;

	ldhses = (node->hn.wind)->hw.ldhses;  

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get the objdid of the referenced io object stored in the
	  first parameter in defbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refobjdid_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refobjdid = *refobjdid_ptr;
	free((char *) refobjdid_ptr);

	/* Check that this is objdid of an existing object */
	sts = ldh_GetObjectClass(
			(node->hn.wind)->hw.ldhses, 
			refobjdid,
			&cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}

	/* Get the parameter of the referenced object
	  in the second devbody-parameter */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[1].ParName,
			(char **)&parameter, &size); 
	if ( EVEN(sts)) return sts;
	free((char *) bodydef);	

	/* Check that the parameter exists in the referenced object */
	sts = ldh_GetObjectBodyDef( ldhses, cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) )
	{
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}

	for ( i = 0, found = 0; i < rows; i++)
	{
	  /* ML 961009. Use ParName instead of PgmName */
	  if ( strcmp( bodydef[i].ParName, parameter) == 0)
	  {
	    found = 1;
	    break;
	  }
	}
	if ( !found )
	{
	  free((char *) parameter);
	  gcg_error_msg( gcgctx, GSX__REFPAR, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
	
	/* Check type of parameter */
	switch (bodydef[i].ParClass )  {
        case pwr_eClass_Input:
          info_type = bodydef[i].Par->Input.Info.Type ;
	  if ( bodydef[i].Par->Input.Info.Flags & PWR_MASK_RTVIRTUAL	)
	  {
	    /* Parameter is not defined in runtime */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
 	  } 
	  break;
        case pwr_eClass_Output:
          info_type = bodydef[i].Par->Output.Info.Type ;
          info_pointer_flag = 
		PWR_MASK_POINTER & bodydef[i].Par->Output.Info.Flags;
	  break;
        case pwr_eClass_Intern:
          info_type = bodydef[i].Par->Intern.Info.Type ;
          info_pointer_flag = 
		PWR_MASK_POINTER & bodydef[i].Par->Intern.Info.Flags;
	  break;
        case pwr_eClass_Param:
          info_type = bodydef[i].Par->Param.Info.Type ;
          info_pointer_flag = 
		PWR_MASK_POINTER & bodydef[i].Par->Param.Info.Flags;
	  break;
	default:
	  /* Not allowed parameter type */		  
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
        free((char *) bodydef );
        
	if (node->ln.cid == pwr_cClass_GetDap)
	{
	  if ( !info_pointer_flag)
	  {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if (node->ln.cid == pwr_cClass_GetObjidp)
	{
	  if (info_type != pwr_eType_Objid)
	  {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);
	    return GSX__NEXTNODE;
	  }
	}

	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts; 

	/* Print the referenced attribute */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s->%s);\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, refobjdid), 
		parameter);

	free((char *) parameter);

	/* Insert object in ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m53()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for Func and FuncExtend
*
**************************************************************************/

int	gcg_comp_m53( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts, found, size;
	pwr_tClassId		windclass;
	ldh_tSesContext ldhses;
	pwr_tObjid		*function_objid;
	pwr_tObjid		window_objid;
	pwr_tObjid		plcpgm_objid;
	pwr_sAttrRef		attrref[2];
	ldh_tWBContext		ldhwb;
	pwr_sPlcWindow		*windbuffer;
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	pwr_tClassId		cid;

	ldhses = (node->hn.wind)->hw.ldhses; 

	if ( node->ln.cid == pwr_cClass_FuncExtend)
	{
	  /* Check that the Func object exist */
	  sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"FuncObject",
			(char **)&function_objid, &size);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_GetObjectClass ( ldhses, *function_objid, &cid);
	  free((char *) function_objid);
	  if ( EVEN(sts))
	  {
	    /* Function not found */
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	    return GSX__NEXTNODE;
	  }
	  if ( cid != pwr_cClass_Func)
	  {
	    /* Class of referenced object is not correct */
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	}
	else if ( node->ln.cid == pwr_cClass_Func)
	{
	  /* Check first if there is a subwindow */
	  sts = ldh_GetChild( ldhses, node->ln.oid, &window_objid);
	  found = 0;
	  while ( ODD(sts) )
	  {
	    /* Check if window */
	    sts = ldh_GetObjectBuffer( ldhses,
			window_objid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, (char **)&windbuffer, &size);
	    if ( ODD(sts)) 
	    {
	      free((char *) windbuffer);
	      found = 1;	
	       break;
	    }
	    sts = ldh_GetNextSibling( ldhses, window_objid, &window_objid);
	  }

	  if ( found )
	  {
	    /* Delete the window */
	    sts = ldh_DeleteObjectTree( ldhses, window_objid);
	    if ( EVEN(sts)) return sts;
	  }

	  /* Copy a new subwindow from the content of "Function" */
	  sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"Function",
			(char **)&function_objid, &size); 
	  if ( EVEN(sts)) return sts;

	  /* Check existence and class of Function object */
	  sts = ldh_GetObjectClass( ldhses, *function_objid, &cid);
	  if ( EVEN(sts)) {
	    /* Function not found */
	    free((char *) function_objid);	
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	    return GSX__NEXTNODE;
	  }

	  if ( cid != pwr_cClass_plc) {
	    /* Class of referenced object is not correct */
	    free((char *) function_objid);	
	    gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	    return GSX__NEXTNODE;
	  }
	  /* Check that a window is created to this plcpgm */
	  sts = ldh_GetChild( ldhses, *function_objid, &window_objid);
	  if ( EVEN(sts))
	  {
	    /* Window not found */
	    free((char *) function_objid);	
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	    return GSX__NEXTNODE;
	  }

	  attrref[0].Objid = *function_objid;
	  attrref[1].Objid = pwr_cNObjid;
	  free((char *) function_objid);

	  sts = ldh_CopyObjectTrees( ldhses, attrref, node->ln.oid,
		ldh_eDest_IntoFirst,  0, 0);
	  if ( EVEN(sts))
	  {
	    /* Function not found */
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	    return GSX__NEXTNODE;
	  }
	  /* Remove the plcpgm object */
	  sts = ldh_GetChild( ldhses, node->ln.oid, &plcpgm_objid);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_GetChild( ldhses, plcpgm_objid, &window_objid);
	  if ( EVEN(sts)) return sts;

	  sts  = ldh_MoveObject( ldhses, window_objid, node->ln.oid,
			ldh_eDest_IntoFirst);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_DeleteObject( ldhses, plcpgm_objid);
	  if ( EVEN(sts)) return sts;

	  node->ln.subwindow = 1;
 	  node->ln.subwind_oid[0] = window_objid;
	  sts = ldh_SetObjectBuffer(
			ldhses,
			node->ln.oid,
			"DevBody",
			"PlcNode",
			(char *)&node->ln);
	  if( EVEN(sts)) return sts;

	  sts = ldh_GetObjectBuffer( ldhses,
			window_objid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, (char **)&windbuffer, &size);
	  if ( EVEN(sts)) return sts;

	  windbuffer->poid = node->ln.oid;
	  sts = ldh_SetObjectBuffer( 
			ldhses,
			window_objid,
			"DevBody",
			"PlcWindow",
			(char *)windbuffer);
	  if( EVEN(sts)) return sts;
	  free((char *) windbuffer);

	  /* Compile the subwindow... */
	  ldhwb = ldh_SessionToWB( ldhses);
	  sts = gcg_wind_comp_all( ldhwb, ldhses, window_objid,
		gcgctx->print, 0, gcg_debug);
	  node->hn.subwindowobject[0] = 0;
	}

	/* Print the code */
	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	if ( node->ln.cid == pwr_cClass_Func)
	{
	  /* Print the window execute command */
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		", %c%s_exec( tp)",
		GCG_PREFIX_MOD,
		vldh_IdToStr(0, window_objid));
	}
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		");\n");

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1) 
	        gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR )
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted )
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE],
			"%c%s->%sP = &%c%s->%s;\n",
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and the pointer will be zero */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE],
			"%c%s->%sP = 0;\n",
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	return GSX__SUCCESS;
}


/*************************************************************************
*
* Name:		gcg_comp_m54()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for FuncInput and FuncOutput.
*	Checks that the parent object of the window is a Func object.
*	Generated code:
*	Declares and links a rtdb pointer to the parent object.
*
**************************************************************************/

int	gcg_comp_m54( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_tSesContext ldhses;
	pwr_tObjid		refobjdid;
	pwr_tClassId		cid;
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	int			size;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	pwr_tObjid		parent;
	pwr_tObjid		funcextend_objid;
	pwr_tObjid		*func_objid;
	int			found;
	pwr_tUInt32		*index_ptr;
	pwr_tUInt32		*funcextend_index_ptr;

	ldhses = (node->hn.wind)->hw.ldhses;  

	/* Get the parent node to this window, it should be either
	  a trans or a order object */
	refobjdid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull(refobjdid))
	{	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass( ldhses, refobjdid, &cid);
	if ( EVEN(sts)) 
	{
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTNODE;
	}

	if ( !(cid == pwr_cClass_Func))
	{
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);
	  return GSX__NEXTNODE;
	}

	/* Get the Index */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid,
			"DevBody",
			"Index",
			(char **)&index_ptr, &size);
	if ( EVEN(sts)) return sts;

	if ( *index_ptr != 0)
	{
	  /* The FuncInput/FuncOutput will work with a FuncExtend object,
	     find this object */
	  sts = ldh_GetParent( ldhses, refobjdid, &parent);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_GetChild( ldhses, parent, &funcextend_objid);
	  found = 0;
	  while ( ODD(sts) )
	  {
	    /* Check if FuncExtend object with correct FuncObject and Index */
	    sts = ldh_GetObjectClass( ldhses, funcextend_objid, &cid);
	    if ( EVEN(sts)) return sts;

	    if ( cid == pwr_cClass_FuncExtend)
	    {
	      sts = ldh_GetObjectPar( ldhses,
			funcextend_objid,
			"DevBody",
			"FuncObject",
			(char **)&func_objid, &size);
	      if ( EVEN(sts)) return sts;

	      if ( cdh_ObjidIsEqual( *func_objid, refobjdid))
	      {
	        /* Get the Index */
	        sts = ldh_GetObjectPar( ldhses,
			funcextend_objid,
			"DevBody",
			"Index",
			(char **)&funcextend_index_ptr, &size);
	        if ( EVEN(sts)) return sts;

	        if ( *index_ptr == *funcextend_index_ptr)
	        {
	          /* This is the one */
	          found = 1;
	          free((char *) funcextend_index_ptr);
	          free((char *) func_objid);
	          break;
	        }
	        free((char *) funcextend_index_ptr);
	      }
	      free((char *) func_objid);
	    }
	    sts = ldh_GetNextSibling( ldhses, funcextend_objid, 
		&funcextend_objid);
	  }
	  if ( !found)
	  {
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	    free((char *) index_ptr);
	    return GSX__NEXTNODE;
	  }
	  /* Replace the Func object by the FuncExtend object */
	  refobjdid = funcextend_objid;
	}
	free((char *) index_ptr);

	/* Insert parent object in ref list */
	gcg_ref_insert( gcgctx, refobjdid, GCG_PREFIX_REF);

	/* Insert object in ref list */
	gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Print the execute command */
	sts = gcg_print_exec_macro( gcgctx, node, node->ln.oid, GCG_PREFIX_REF);
	if (EVEN(sts)) return sts;

	/* Print the parent object */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		"%c%s);\n",
		GCG_PREFIX_REF,
		vldh_IdToStr(0, refobjdid));

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m55()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for macro node with no inputs.
*	Prints code for declaration and direkt link of rtdbpointer for 
*	the actual object.
*	Prints an exec call :
*
*	'structname'_exec( 'objectpointer');
*	ex: ScanTime_exec( Z800008f1);
*	
**************************************************************************/

int	gcg_comp_m55( gcg_ctx gcgctx, vldh_t_node node)
{
	int			sts;
	char			*name;

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for the exec command for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if ( EVEN(sts)) return sts;	

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s);",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, node->ln.oid));

	gcg_scantime_print( gcgctx, node->ln.oid);
	gcg_timer_print( gcgctx, node->ln.oid);
	
	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_comp_m56()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an generic object that is not exchanged.
*	
**************************************************************************/

int	gcg_comp_m56( gcg_ctx gcgctx, vldh_t_node node)
{

	gcg_error_msg( gcgctx, GSX__GENERIC, node);  
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m57()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for StoDattr, SetDattr, ResDattr, StoAattr,
*	StoIattr.
*	If the object is connected to an order it will be called
*	from the order method.
*	
*	Syntax control:
*	Checks that the referenced attribute is correct.
*
*	Code generation:
*	Declares and links a pointer to the host object.
*	Prints an exec call.
*	StoDattr_exec( W800005f6->SomeAttribute, Z800005f5->ActualValue);
*
*	If the input is not connected the value of the inputparameter
*	will be printed instead of a input object for sto object. For set 
*	and reset object TRUE will printed.
*	StoAattr_exec( W800005f6->SomeAttribute, 3.2500);
*
**************************************************************************/


int	gcg_comp_m57( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	char			refattr[32];
	char			*refattr_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tObjid		host_objid;
	pwr_tClassId		cid;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		output_bodydef;
	char			*nocondef_ptr;
	gcg_t_nocondef		nocondef[2];
	unsigned long		nocontype[2];
	unsigned long		info_type, info_size;
	int 			i, found;
	char			pgmname[32];
	char			*name;

	nocondef[1].bo = 1;
	nocontype[1] = GCG_BOOLEAN;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	if ( !gcgctx->order_comp) {
	  if ( (node->ln.cid == pwr_cClass_StoDattr)) {
	    /* Check first if the object is connected to an order object,
	      if it is, it will be compiled when by the ordermethod, when 
	      gcgctx->order_comp is true */
	    sts = gcg_get_output( node, 0, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;
	    if ( output_count == 1)
	      if ( output_node->ln.cid == pwr_cClass_order)
	        return GSX__SUCCESS;
	  }
	}

	/* Get the referenced attribute io object stored in the
	  first parameter in devbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattr_ptr, &size); 
	if ( EVEN(sts)) return sts;
        strncpy( refattr, refattr_ptr, sizeof(refattr));
	free((char *) refattr_ptr);
	free((char *) bodydef);	

	/* If the object is not connected the value in the
	   parameter should be written in the macro call */
	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"RtBody",
			"In",
			&nocondef_ptr, &size); 
	if ( EVEN(sts)) return sts;

	/* Get the parent node to this window */
	host_objid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull(host_objid)) {	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass( ldhses, host_objid, &cid);
	if ( EVEN(sts))  {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTNODE;
	}

	if ( cid == pwr_cClass_order ||
	     cid == pwr_cClass_csub ||
	     cid == pwr_cClass_substep) {
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);
	  return GSX__NEXTNODE;
	}

	/* Check that the attribute exist in this class */
	sts = ldh_GetObjectClass( ldhses, host_objid, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that the parameter exists in the referenced object */
	sts = ldh_GetObjectBodyDef( ldhses, cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) {
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}

	for ( i = 0, found = 0; i < rows; i++) {
	  /* ML 961009. Use ParName instead of PgmName */
	  if ( strcmp( bodydef[i].ParName, refattr) == 0) {
	    found = 1;
	    break;
	  }
	}
	if ( !found ) {
	  gcg_error_msg( gcgctx, GSX__REFPAR, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
	
	/* Check type of parameter */
	switch (bodydef[i].ParClass )  {
        case pwr_eClass_Input:
          info_type = bodydef[i].Par->Input.Info.Type;
          if ( bodydef[i].Par->Input.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Input.Info.Size /
	      bodydef[i].Par->Input.Info.Elements;
          else
            info_size = bodydef[i].Par->Input.Info.Size;
	  if ( bodydef[i].Par->Input.Info.Flags & PWR_MASK_RTVIRTUAL) {
	    /* Parameter is not defined in runtime */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
 	  } 
	  break;
        case pwr_eClass_Output:
          info_type = bodydef[i].Par->Output.Info.Type;
          if ( bodydef[i].Par->Output.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Output.Info.Size /
	      bodydef[i].Par->Output.Info.Elements;
          else
            info_size = bodydef[i].Par->Output.Info.Size;
	  break;
        case pwr_eClass_Intern:
          info_type = bodydef[i].Par->Intern.Info.Type ;
          if ( bodydef[i].Par->Intern.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Intern.Info.Size /
	      bodydef[i].Par->Intern.Info.Elements;
          else
            info_size = bodydef[i].Par->Intern.Info.Size;
	  break;
        case pwr_eClass_Param:
          info_type = bodydef[i].Par->Param.Info.Type ;
          if ( bodydef[i].Par->Param.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Param.Info.Size /
	      bodydef[i].Par->Param.Info.Elements;
          else
            info_size = bodydef[i].Par->Param.Info.Size;
	  break;
	default:
	  /* Not allowed parameter type */		  
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
        free((char *) bodydef );
	
	/* Check that the type of the referenced attribute is correct */
	if ( node->ln.cid == pwr_cClass_SetDattr ||
	     node->ln.cid == pwr_cClass_ResDattr) {
	  if ( info_type != pwr_eType_Boolean) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = TRUE;
	  nocontype[0] = GCG_BOOLEAN;
	}    
	else if ( node->ln.cid == pwr_cClass_StoDattr) {
	  if ( info_type != pwr_eType_Boolean) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = (int)*nocondef_ptr;
	  nocontype[0] = GCG_BOOLEAN;
	}
	else if ( node->ln.cid == pwr_cClass_StoAattr ||
		  node->ln.cid == pwr_cClass_CStoAattr) {
	  if ( info_type != pwr_eType_Float32) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].fl = *(float *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( node->ln.cid == pwr_cClass_StoIattr ||
		  node->ln.cid == pwr_cClass_CStoIattr) {
	  if ( !(info_type == pwr_eType_Int32 ||
		 info_type == pwr_eType_UInt32 ||
		 info_type == pwr_eType_Int16 ||
		 info_type == pwr_eType_UInt16 ||
		 info_type == pwr_eType_Int8 ||
		 info_type == pwr_eType_UInt8 ||
		 info_type == pwr_eType_Enum ||
		 info_type == pwr_eType_Mask ||
		 info_type == pwr_eType_Status ||
		 info_type == pwr_eType_NetStatus)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  nocondef[0].bo = *(int *) nocondef_ptr;
	  nocontype[0] = GCG_FLOAT;
	}    
	else if ( node->ln.cid == pwr_cClass_StoSattr ||
		  node->ln.cid == pwr_cClass_CStoSattr) {
	  if ( info_type != pwr_eType_String) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  strcpy( nocondef[0].str, (char *) nocondef_ptr);
	  nocontype[0] = GCG_STRING;
	}    
	free(nocondef_ptr);

	/* Insert host object in ref list */
	gcg_ref_insert( gcgctx, host_objid, GCG_PREFIX_REF);

	/* Print the execute command */

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	sts = gcg_parname_to_pgmname(ldhses, cid, refattr, pgmname);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"%s_exec( %c%s->%s, ",
		name,
		GCG_PREFIX_REF,
		vldh_IdToStr(0, host_objid),
		pgmname);

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		nocondef, nocontype);
	if ( EVEN(sts) ) return sts;

	if ( node->ln.cid == pwr_cClass_StoSattr ||
	     node->ln.cid == pwr_cClass_CStoSattr ) {
          // Add size of connected attribute
	  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		",%ld", info_size);
        }

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		");\n");

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m58()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for an object with template plc code in class.
*
**************************************************************************/

int	gcg_comp_m58( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts, found, size;
	pwr_tClassId		windclass;
	ldh_tSesContext ldhses;
	pwr_tObjid		window_objid;
	pwr_tObjid		plcpgm_objid;
	pwr_sAttrRef		attrref[2];
	ldh_tWBContext		ldhwb;
	pwr_sPlcWindow		*windbuffer;
	pwr_sPlcNode		*nodebuffer;
	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
        pwr_tOName	       	oname;
	pwr_tClassId		cid;
	pwr_tTime		*instance_time;
	pwr_tTime		*template_time;
	pwr_tObjid		template_plc;
	pwr_tObjid		template_window;
	pwr_sAttrRef		*connect_aref;
	pwr_sAttrRef		caref, ccaref;
	pwr_tObjName		cname;
	
	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Check if there is a PlcConnected */
	sts = ldh_GetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
				"PlcConnect", (char **)&connect_aref, &size);
	if ( ODD(sts)) {
	  pwr_sAttrRef aref = *connect_aref;
	  free( (char *)connect_aref);
	  sts = gcg_replace_ref( gcgctx, &aref, node);
	  if ( ODD(sts)) {
	    /* Store the converted aref */
	    sts = ldh_SetObjectPar( gcgctx->ldhses, node->ln.oid, "RtBody", 
				"PlcConnect", (char *)&aref, sizeof(aref));
	  }

	  if ( cdh_ObjidIsNull( aref.Objid)) {
	    gcg_error_msg( gcgctx, GSX__NOCONNECT, node);
	    return GSX__NEXTNODE;
	  }

	  // Check that object exist
	  sts = ldh_GetAttrRefOrigTid( gcgctx->ldhses, &aref, &cid);
	  if ( EVEN(sts)) {
	    gcg_error_msg( gcgctx, GSX__NOCONNECT, node);
	    return GSX__NEXTNODE;
	  }

	  gcg_aref_insert( gcgctx, aref, GCG_PREFIX_REF);

	  // Make connection mutual
	  sts = ldh_ObjidToName( ldhses, cdh_ClassIdToObjid( node->ln.cid), 
				 ldh_eName_Object, cname, sizeof( cname), &size);
	  if( EVEN(sts)) return sts;

	  if ( strlen(cname) > 3 && strcmp( &cname[strlen(cname)-3], "Sim") == 0)
	    sts = ldh_ArefANameToAref( ldhses, &aref, "SimConnect", &caref);
	  else
	    sts = ldh_ArefANameToAref( ldhses, &aref, "PlcConnect", &caref);
          if ( ODD(sts)) {
	    sts = ldh_ReadAttribute( ldhses, &caref, &ccaref, sizeof(ccaref));
	    if ( ODD(sts) && cdh_ObjidIsNotEqual( ccaref.Objid, node->ln.oid)) {
	      ccaref = cdh_ObjidToAref( node->ln.oid);
	      sts = ldh_WriteAttribute( ldhses, &caref, &ccaref, sizeof(caref));
	    }
	  }
	}

	/* Check first if there is a subwindow */
	sts = ldh_GetChild( ldhses, node->ln.oid, &window_objid);
	found = 0;
	while ( ODD(sts) ) {
	  /* Check if window */
	  sts = ldh_GetObjectClass( ldhses, window_objid, &cid);
	  if ( EVEN(sts)) return sts;
	  if ( cid == pwr_cClass_windowplc) {
	    found = 1;	
	    break;
	  }
	  sts = ldh_GetNextSibling( ldhses, window_objid, &window_objid);
	}

	if ( found ) {
	  // Get modification time
	  sts = ldh_GetObjectPar( ldhses, window_objid, 
			"DevBody", "Modified", (char **)&instance_time, &size); 
	  if ( EVEN(sts)) return sts;
	}

	// Find the template plc
	sts = ldh_ObjidToName( ldhses, cdh_ClassIdToObjid( node->ln.cid), 
		ldh_eName_VolPath, oname, sizeof( oname), &size);
	if( EVEN(sts)) return sts;

	strcat( oname, "-Code");
	sts = ldh_NameToObjid( ldhses, &template_plc, oname);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__TEMPLATEPLC, node);
	  return GSX__NEXTNODE;
	}
	sts = ldh_GetChild( ldhses, template_plc, &template_window);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__TEMPLATEPLC, node);
	  return GSX__NEXTNODE;
	}

	sts = ldh_GetObjectClass( ldhses, template_window, &cid);
	if ( EVEN(sts)) return sts;
	if ( cid != pwr_cClass_windowplc) {
	  gcg_error_msg( gcgctx, GSX__TEMPLATEPLC, node);
	  return GSX__NEXTNODE;
	}

	// Get modification time
	sts = ldh_GetObjectPar( ldhses, template_window, 
			"DevBody", "Modified",
			(char **)&template_time, &size); 
	if ( EVEN(sts)) return sts;

	if ( !found || template_time->tv_sec != instance_time->tv_sec) {
	  // Replace the code
	  if ( found) {
	    /* Delete the window */
	    sts = ldh_DeleteObjectTree( ldhses, window_objid);
	    if ( EVEN(sts)) return sts;
	  }

	  attrref[0].Objid = template_plc;
	  attrref[1].Objid = pwr_cNObjid;

	  sts = ldh_CopyObjectTrees( ldhses, attrref, node->ln.oid,
		ldh_eDest_IntoFirst, 0, 1);
	  if ( EVEN(sts)) {
	    /* Function not found */
	    gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	    return GSX__NEXTNODE;
	  }

	  /* Remove the plcpgm object */
	  sts = ldh_GetChild( ldhses, node->ln.oid, &plcpgm_objid);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_GetChild( ldhses, plcpgm_objid, &window_objid);
	  if ( EVEN(sts)) return sts;

	  sts  = ldh_MoveObject( ldhses, window_objid, node->ln.oid,
			ldh_eDest_IntoFirst);
	  if ( EVEN(sts)) return sts;

	  sts = ldh_DeleteObject( ldhses, plcpgm_objid);
	  if ( EVEN(sts)) return sts;

	  node->ln.subwindow = 1;
 	  node->ln.subwind_oid[0] = window_objid;

	  sts = ldh_GetObjectBuffer( ldhses, node->ln.oid,
			"DevBody", "PlcNode", (pwr_eClass *) &windclass, 
				     (char **)&nodebuffer, &size);
	  if( EVEN(sts)) return sts;
	  nodebuffer->subwindow = 1;
 	  nodebuffer->subwind_oid[0] = window_objid;
	  sts = ldh_SetObjectBuffer( ldhses, node->ln.oid, 
				     "DevBody", "PlcNode", (char *)nodebuffer);
	  if( EVEN(sts)) return sts;

	  sts = ldh_GetObjectBuffer( ldhses,
			window_objid, "DevBody", "PlcWindow", 
			(pwr_eClass *) &windclass, (char **)&windbuffer, &size);
	  if ( EVEN(sts)) return sts;

	  windbuffer->poid = node->ln.oid;
	  sts = ldh_SetObjectBuffer( 
			ldhses,
			window_objid,
			"DevBody",
			"PlcWindow",
			(char *)windbuffer);
	  if( EVEN(sts)) return sts;
	  free((char *) windbuffer);

	  /* Save the session, otherwise no compilation is done */
	  // sts = ldh_SaveSession( ldhses);
	  // if ( EVEN(sts)) return sts;

	  /* Compile the subwindow... */
	  ldhwb = ldh_SessionToWB( ldhses);
	  sts = gcg_wind_comp_all( ldhwb, ldhses, window_objid,
		gcgctx->print, 0, gcg_debug);
	  node->hn.subwindowobject[0] = 0;
	}
	else {
	  // Check if new structfile
	  char *s;
	  char fname[200];
	  struct stat info;
	  pwr_tTime		*compile_time;

	  /* Get compilation time in parameter Compiled */
	  sts = ldh_GetObjectPar( ldhses, window_objid, 
			"DevBody", "Compiled",
			(char **)&compile_time, &size); 
	  if (EVEN(sts)) return sts;

	  if ( (s = strchr( oname, ':')))
	    *s = 0;
	  sprintf( fname, "$pwrp_inc/pwr_%sclasses.h", oname);
	  cdh_ToLower( fname, fname);
	  dcli_translate_filename( fname, fname);

	  sts = stat( fname, &info);
	  if ( sts != -1 && info.st_ctime > compile_time->tv_sec) {
	    /* Compile the subwindow... */
	    ldhwb = ldh_SessionToWB( ldhses);
	    sts = gcg_wind_comp_all( ldhwb, ldhses, window_objid,
		gcgctx->print, 0, gcg_debug);
	  }
	  free( (char *)compile_time);

	}
	if ( found)
	  free( (char *)instance_time);
	free( (char *)template_time);

	/* Print the code */
	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	/* Get name for this class */
	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	      output_found = 1;
	      if ( output_count > 1) 
	        gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR )
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted )
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE],
			"%c%s->%sP = &%c%s->%s;\n",
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
 			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);

	      switch ( bodydef[i].Par->Param.Info.Type) {
	      case pwr_eType_String:
		IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
			       "strncpy( %c%s->%s, %c%s->%sP, %d);\n",
			       GCG_PREFIX_REF,
			       vldh_IdToStr(0, node->ln.oid),
			       bodydef[i].Par->Param.Info.PgmName,
			       GCG_PREFIX_REF,
			       vldh_IdToStr(0, node->ln.oid),
			       bodydef[i].Par->Param.Info.PgmName,
			       bodydef[i].Par->Param.Info.Size);
		break;
	      default:
		IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
			       "%c%s->%s = *%c%s->%sP;\n",
			       GCG_PREFIX_REF,
			       vldh_IdToStr(0, node->ln.oid),
			       bodydef[i].Par->Param.Info.PgmName,
			       GCG_PREFIX_REF,
			       vldh_IdToStr(0, node->ln.oid),
			       bodydef[i].Par->Param.Info.PgmName);
	      }
	      }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and the pointer will be zero */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE],
			"%c%s->%sP = 0;\n",
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	/* Print the window execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE],
		       "%c%s_exec( tp);\n",
		       GCG_PREFIX_MOD,
		       vldh_IdToStr(0, window_objid));

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m59()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for GetDattr, GetAattr, GetIattr and GetSattr.
*	Checks that the referenced attribute exists, and that the type 
*       of the attribute is correct.
*	Prints declaration and direct link of pointer to referenced object.
*
**************************************************************************/

int	gcg_comp_m59( gcg_ctx gcgctx, vldh_t_node node)
{
	ldh_sParDef 		*bodydef;
	int 			rows, sts;
	int			size;
	char			refattr[32];
	char			*refattr_ptr;
	unsigned long		info_type;
	unsigned long		info_size;
	int			found, i;
	ldh_tSesContext ldhses;
	pwr_tClassId		cid;
	int			element;
	char			*s, *t;
	char			elementstr[20];
	int			nr;
	int			len;
	pwr_tObjid		host_objid;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Get the referenced attribute stored in 'Attribute' */

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			"Attribute",
			(char **)&refattr_ptr, &size); 
	if ( EVEN(sts)) return sts;
        strncpy( refattr, refattr_ptr, sizeof(refattr));
	free((char *) refattr_ptr);

	/* Get the parent node to this window */
	host_objid = (node->hn.wind)->lw.poid;

	if ( cdh_ObjidIsNull(host_objid)) {	
	  /* Parent is a plcprogram */
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);  
	  return GSX__NEXTNODE;
	}

	/* Check class of this objdid */
	sts = ldh_GetObjectClass( ldhses, host_objid, &cid);
	if ( EVEN(sts))  {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTNODE;
	}

	if ( cid == pwr_cClass_order ||
	     cid == pwr_cClass_csub ||
	     cid == pwr_cClass_substep) {
	  gcg_error_msg( gcgctx, GSX__BADWIND, node);
	  return GSX__NEXTNODE;
	}

	/* Check that the attribute exist in this class */
	sts = ldh_GetObjectClass( ldhses, host_objid, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Check that the parameter exists in the referenced object */
	sts = ldh_GetObjectBodyDef( ldhses, cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) {
	  gcg_error_msg( gcgctx, GSX__REFCLASS, node);  
	  return GSX__NEXTNODE;
	}

	for ( i = 0, found = 0; i < rows; i++) {
	  /* ML 961009. Use ParName instead of PgmName */
	  if ( strcmp( bodydef[i].ParName, refattr) == 0) {
	    found = 1;
	    break;
	  }
	}
	if ( !found ) {
	  gcg_error_msg( gcgctx, GSX__REFPAR, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
	
	/* Check type of parameter */
	switch (bodydef[i].ParClass )  {
        case pwr_eClass_Input:
          info_type = bodydef[i].Par->Input.Info.Type;
          if ( bodydef[i].Par->Input.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Input.Info.Size /
	      bodydef[i].Par->Input.Info.Elements;
          else
            info_size = bodydef[i].Par->Input.Info.Size;
	  if ( bodydef[i].Par->Input.Info.Flags & PWR_MASK_RTVIRTUAL) {
	    /* Parameter is not defined in runtime */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
 	  } 
	  break;
        case pwr_eClass_Output:
          info_type = bodydef[i].Par->Output.Info.Type;
          if ( bodydef[i].Par->Output.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Output.Info.Size /
	      bodydef[i].Par->Output.Info.Elements;
          else
            info_size = bodydef[i].Par->Output.Info.Size;
	  break;
        case pwr_eClass_Intern:
          info_type = bodydef[i].Par->Intern.Info.Type ;
          if ( bodydef[i].Par->Intern.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Intern.Info.Size /
	      bodydef[i].Par->Intern.Info.Elements;
          else
            info_size = bodydef[i].Par->Intern.Info.Size;
	  break;
        case pwr_eClass_Param:
          info_type = bodydef[i].Par->Param.Info.Type ;
          if ( bodydef[i].Par->Param.Info.Flags & PWR_MASK_ARRAY)
            info_size = bodydef[i].Par->Param.Info.Size /
	      bodydef[i].Par->Param.Info.Elements;
          else
            info_size = bodydef[i].Par->Param.Info.Size;
	  break;
	default:
	  /* Not allowed parameter type */		  
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  free((char *) bodydef);	
	  return GSX__NEXTNODE;
	}
	
	/* Check that the type of the referenced attribute is correct */
	if ( node->ln.cid == pwr_cClass_GetDattr) {
	  if ( info_type != pwr_eType_Boolean) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetAattr) {
	  if ( info_type != pwr_eType_Float32) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetIattr) {
	  if ( !(info_type == pwr_eType_Int32 ||
		 info_type == pwr_eType_UInt32 ||
		 info_type == pwr_eType_Int16 ||
		 info_type == pwr_eType_UInt16 ||
		 info_type == pwr_eType_Int8 ||
		 info_type == pwr_eType_UInt8 ||
		 info_type == pwr_eType_Enum ||
		 info_type == pwr_eType_Mask ||
		 info_type == pwr_eType_Status ||
		 info_type == pwr_eType_NetStatus)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}    
	else if ( node->ln.cid == pwr_cClass_GetSattr) {
	  if ( info_type != pwr_eType_String) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	}    

	/* Check if parameter is an array */
	s = strchr( refattr, '[');
	if ( s == 0)
	  element = -1;
	else
	{
	  t = strchr( refattr, ']');
	  if ( t == 0)
	  {
	    gcg_error_msg( gcgctx, GSX__REFPAR, node);
	    return GSX__NEXTNODE;
	  }
	  else
	  {
	    len = t - s - 1;
            if ( len > (int)sizeof(elementstr) - 1)
            {
              gcg_error_msg( gcgctx, GSX__REFPAR, node);
              return GSX__NEXTNODE;
            }
	    strncpy( elementstr, s + 1, len);
	    elementstr[len] = 0;
	    nr = sscanf( elementstr, "%d", &element);
  	    if ( nr != 1 )
	    {
	      gcg_error_msg( gcgctx, GSX__REFPAR, node);
	      return GSX__NEXTNODE;
	    }
	    *s = '\0';
	  }
	}

	/* Check elements */
	if ( bodydef[i].Par->Param.Info.Flags & PWR_MASK_ARRAY ) {
	  if ( element == -1) {
	    /* No elementstring found in parameter */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
	  }
	  if ( element > (int)(bodydef[i].Par->Param.Info.Elements - 1)) {
	    /* Element index to large */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
	  }
	}
	else {
	  if ( element != -1) {
	    /* Elementstring found in parameter */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    free((char *) bodydef);	
	    return GSX__NEXTNODE;
	  }
	}

        free((char *) bodydef );        

	/* Insert object in ref list */
	gcg_ref_insert( gcgctx, host_objid, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m60()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for NameToStr.
*	Prints declaration and direct link of pointer to referenced object.
*
**************************************************************************/

int	gcg_comp_m60( gcg_ctx gcgctx, vldh_t_node node)
{
  pwr_sAttrRef 	aref, *arefp;
  pwr_tInt32	*segmentsp;
  int		segments;
  int		size;
  pwr_tAName	name;
  char		*name_p;
  int		sts;
  ldh_tSesContext ldhses = (node->hn.wind)->hw.ldhses;  
	
  /* Get the referenced attribute stored in 'Object' */

  sts = ldh_GetObjectPar( ldhses, node->ln.oid, "DevBody", "Object",
			(char **)&arefp, &size); 
  if ( EVEN(sts)) return sts;
  aref = *arefp;
  free((char *) arefp);

  sts = ldh_GetObjectPar( ldhses, node->ln.oid, "DevBody", "ObjectSegments",
			(char **)&segmentsp, &size); 
  if ( EVEN(sts)) return sts;
  segments = *segmentsp;
  free((char *) segmentsp);

  if ( segments == 0)
    segments = 1;

  if ( cdh_ObjidIsNotNull( arefp->Objid)) {
    sts = gcg_replace_ref( gcgctx, &aref, node);
    if ( EVEN(sts)) return sts;

    sts = ldh_AttrRefToName( ldhses, &aref, ldh_eName_Aref, 
			   &name_p, &size);
    if ( ODD(sts)) {
      utl_cut_segments( name, name_p, segments);
    }
  }

  /* Set the parameter value */
  name[79] = 0;
  sts = ldh_SetObjectPar( ldhses, node->ln.oid, "RtBody",
			"Out", name, sizeof(pwr_tString80)); 
  if ( EVEN(sts)) return sts;

  /* Insert object in ref list */
  sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);
  
  return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m60()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for CArea.
*
**************************************************************************/

int	gcg_comp_m61( gcg_ctx		gcgctx,
		      vldh_t_node	node)
{
	int 			sts;
	gcg_t_nocondef		nocondef;
	unsigned long		nocontype = GCG_BOOLEAN;
	ldh_tSesContext 	ldhses;

	if ( !gcgctx->cmanager_active)
	  return GSX__SUCCESS;

	if ( gcgctx->current_cmanager == node)
	  return GSX__SUCCESS;
	if ( gcgctx->current_cmanager) {
	  /* Other cmanager */
	  gcg_reset_cmanager( gcgctx);
	}
	gcgctx->current_cmanager = node;

	nocondef.bo = FALSE;
	ldhses = (node->hn.wind)->hw.ldhses; 

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		"if ( ");

	sts = gcg_print_inputs( gcgctx, node, ", ", GCG_PRINT_ALLPAR, 
		&nocondef, &nocontype);
	if ( EVEN(sts)) return sts;

	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		" ) {\n");
	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m62()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for Disabled.
*	Checks that the referenced attribute exists and that is has
*       a DisableAttr attribute.
*	Prints declaration and direct link of pointer to DisableAttr of the
*	referenced object.
*
**************************************************************************/

int	gcg_comp_m62( gcg_ctx gcgctx, vldh_t_node node)
{
	int 			sts;
	int			size;
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_sAttrRefInfo	info;
        pwr_tAName	       	aname;
	char			*name_p;
	ldh_tSesContext 	ldhses;
	char			*s;
	pwr_sAttrRef		disaref;

	ldhses = (node->hn.wind)->hw.ldhses;  
	
	/* Get the attrref of the referenced attribute */
	sts = ldh_GetObjectPar( ldhses, node->ln.oid, 
			"DevBody", "Object",
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;

	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	sts = ldh_GetAttrRefInfo( ldhses, &refattrref, &info);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	if ( !(info.flags & PWR_MASK_DISABLEATTR)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Get attrref of DisableAttr attribute */
	disaref = cdh_ArefToDisableAref( &refattrref);

	sts = ldh_GetAttrRefInfo( ldhses, &disaref, &info);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	/* Get rid of last attribute segment of the referenced object */
	sts = ldh_AttrRefToName( ldhses, &disaref, ldh_eName_Aref, 
				 &name_p, &size);
	if ( EVEN(sts)) return sts;

	strcpy( aname, name_p);
	if ( (s = strrchr( aname, '.')) == 0) { 
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTPAR;
	}

	*s = 0;
	sts = ldh_NameToAttrRef( ldhses, aname, &refattrref);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	if ( info.type != pwr_eType_DisableAttr ) {
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);
	  return GSX__NEXTNODE;
	}

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_comp_m63()
*
* Type		void
*
* Type		Parameter	IOGF	Description
* gcg_ctx	gcgctx		I	gcg context.
* vldh_t_node	node		I	vldh node.
*
* Description:
*	Compile method for CStoAttrRefP.
*
*	Syntax control:
*	Check that the referenced object exists and that the referenced
*	parameter exists in this object, and that the object is of
*	the correct type.
*
*	Generating code:
*	Declares and links a rtdb pointer to the referenced object.
*	Prints an exec call.
*
**************************************************************************/

int	gcg_comp_m63( gcg_ctx gcgctx, vldh_t_node node)
{
	pwr_sAttrRef		refattrref;
	pwr_sAttrRef		*refattrref_ptr;
	ldh_tSesContext 	ldhses;
	pwr_tClassId		cid;
	pwr_tAName     		aname;
	char			*name_p;
	ldh_sAttrRefInfo	info;
	char			parameter[80];
	char			*s;
       	unsigned long		point;
	unsigned long		par_inverted;
	vldh_t_node		output_node;
	unsigned long		output_count;
	unsigned long		output_point;
	ldh_sParDef 		*bodydef;
	ldh_sParDef 		output_bodydef;
	int 			rows, sts;
	int			i, output_found, first_par;
	pwr_sAttrRef		output_attrref;
	int			output_type;
	char			output_prefix;
	char			output_par[80];
	char			*name;
	int			size;

	ldhses = (node->hn.wind)->hw.ldhses;

	/* Get the attref of the referenced object stored in the
	  first parameter in devbody */

	/* Get the devbody parameters for this class */
	sts = ldh_GetObjectBodyDef( ldhses,
			node->ln.cid, "DevBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	sts = ldh_GetObjectPar( ldhses,
			node->ln.oid, 
			"DevBody",
			bodydef[0].ParName,
			(char **)&refattrref_ptr, &size); 
	if ( EVEN(sts)) return sts;
	refattrref = *refattrref_ptr;
	free((char *) refattrref_ptr);
	free((char *) bodydef);	

	sts = gcg_replace_ref( gcgctx, &refattrref, node);
	if ( EVEN(sts)) return sts;

	/* Check that this is objdid of an existing object */
	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTNODE;
	}

	sts = ldh_GetAttrRefInfo( ldhses, &refattrref, &info);
	if ( EVEN(sts)) return sts;

	/* Get rid of last attribute segment of the referenced object */
	sts = ldh_AttrRefToName( ldhses, &refattrref, ldh_eName_Aref, 
				 &name_p, &size);
	if ( EVEN(sts)) return sts;

	strcpy( aname, name_p);
	if ( (s = strrchr( aname, '.')) == 0) { 
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
	  return GSX__NEXTPAR;
	}

	*s = 0;
	sts = ldh_NameToAttrRef( ldhses, aname, &refattrref);
	if ( EVEN(sts)) {
	  gcg_error_msg( gcgctx, GSX__REFOBJ, node);
	  return GSX__NEXTPAR;
	}

	sts = ldh_GetAttrRefOrigTid( ldhses, &refattrref, &cid);
	if ( EVEN(sts)) return sts;
	
	sts = gcg_parname_to_pgmname(ldhses, cid, s+1, parameter);
	if ( EVEN(sts)) return sts;

	if ( info.flags & PWR_MASK_RTVIRTUAL) {
	  /* Attribute is not defined in runtime */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	} 

	if ( info.flags & PWR_MASK_ARRAY) {
	  if ( info.nElement == -1) {
	    /* No index in attribute */
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
	  // if ( info.index > info.nElement - 1) {
	  //  /* Element index to large */
	  //  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  //  return GSX__NEXTNODE;
	  //}
	}

	switch ( info.type ) {
        case pwr_eType_AttrRef : 
	  if ( !( node->ln.cid == pwr_cClass_CStoAttrRefP)) {
	    gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	    return GSX__NEXTNODE;
	  }
          break;
	default:
	  /* Not allowed type */
	  gcg_error_msg( gcgctx, GSX__REFPARTYPE, node);  
	  return GSX__NEXTNODE;
	}

	/* Insert object in ref list */
	gcg_aref_insert( gcgctx, refattrref, GCG_PREFIX_REF);

	sts = gcg_ref_insert( gcgctx, node->ln.oid, GCG_PREFIX_REF);

	sts = gcg_get_structname( gcgctx, node->ln.oid, &name);
	if( EVEN(sts)) return sts;

	/* Print the execute command */
	IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		       "%s_exec( tp, %c%s, &%c%s->%s);\n",
		       name,
		       GCG_PREFIX_REF,
		       vldh_IdToStr(0, node->ln.oid), 
		       GCG_PREFIX_REF,
		       vldh_AttrRefToStr(0, refattrref),
		       parameter);

	/* Get the runtime parameters for this class */
	sts = ldh_GetObjectBodyDef((node->hn.wind)->hw.ldhses, 
			node->ln.cid, "RtBody", 1, 
			&bodydef, &rows);
	if ( EVEN(sts) ) return sts;

	i = 0;
	first_par = 1;
	while( 	(i < rows) &&
		(bodydef[i].ParClass == pwr_eClass_Input))
	{
	  /* Get the point for this parameter if there is one */
	  output_found = 0;
	  sts = gcg_get_inputpoint( node, i, &point, &par_inverted);
	  if ( ODD( sts))
	  {
	    /* Look for an output connected to this point */
	    sts = gcg_get_output( node, point, &output_count, &output_node,
			&output_point, &output_bodydef, 
			GOEN_CON_SIGNAL | GOEN_CON_OUTPUTTOINPUT);
	    if ( EVEN(sts)) return sts;

	    if ( output_count > 0 )
	    {
	    output_found = 1;
	      if ( output_count > 1) 
	      gcg_error_msg( gcgctx, GSX__CONOUTPUT, output_node);

	      sts = gcg_get_outputstring( gcgctx, output_node, &output_bodydef, 
			&output_attrref, &output_type, &output_prefix, output_par);
	      if ( sts == GSX__NEXTPAR ) 
	      {
	        i++;
	        continue;
	      }
	      if ( EVEN(sts)) return sts;

	      if ( par_inverted ) 
	        gcg_error_msg( gcgctx, GSX__INV, node);

	      IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			output_prefix,
			output_type == GCG_OTYPE_OID ? 
			     vldh_IdToStr(1, output_attrref.Objid) : 
			     vldh_AttrRefToStr(0, output_attrref),
			output_par);
	    }
	    else
	    {
	      /* Point visible but not connected, errormessage */
	      gcg_error_msg( gcgctx, GSX__NOTCON, node);  
	    }
	    first_par = 0;
	  }
	  if ( !output_found )
	  {
	    /* The point is not connected and will point to its
	       own object */

	    IF_PR fprintf( gcgctx->files[GCGM1_REF_FILE], 
			"%c%s->%sP = &%c%s->%s;\n", 
			GCG_PREFIX_REF,
			vldh_IdToStr(0, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName,
			GCG_PREFIX_REF,
			vldh_IdToStr(1, node->ln.oid),
			bodydef[i].Par->Param.Info.PgmName);
	  }
	  i++;
	}
	free((char *) bodydef);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_wind_check_modification()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*
**************************************************************************/

int gcg_wind_check_modification (
  ldh_tSesContext ldhses,
  pwr_tObjid	objdid
)
{
	pwr_tTime		comp_time;
	pwr_tTime		mod_time;
	pwr_tTime		*mod_time_ptr;
	pwr_tTime		*comp_time_ptr;
	int			sts, size;
	int			modification;

	modification = 0;

	/* Get compilation time in parameter Compiled */
        sts = ldh_GetObjectPar( ldhses, objdid, "DevBody",   
			"Compiled", (char **)&comp_time_ptr, &size); 
	if (EVEN(sts)) return sts;

        memcpy( &comp_time, comp_time_ptr, sizeof(comp_time));
 	free((char *) comp_time_ptr);

	/* Get modification time in parameter Modified */
        sts = ldh_GetObjectPar( ldhses, objdid, "DevBody",   
			"Modified", (char **)&mod_time_ptr, &size); 
        if (EVEN(sts)) return sts;

        memcpy( &mod_time, mod_time_ptr, sizeof(mod_time));
 	free((char *) mod_time_ptr);

	/* Check if modified after compiled */
	if (time_Acomp(&mod_time, &comp_time) > 0)
	  modification = 1;
	else
	  modification = 0;

	if ( modification)
	  return 0;

	return GSX__SUCCESS;
}
/*************************************************************************
*
* Name:		gcg_wind_to_operating_system()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*
**************************************************************************/

int gcg_wind_to_operating_system ( 
    ldh_tSesContext	ldhses, 
    pwr_tObjid		wind, 
    pwr_mOpSys		*os
    )
{
	  int			sts;
	  pwr_tClassId		cid;
	  pwr_tObjid		od;
static    pwr_tClassId		plc_pgm = 0;



        /* Get the PlcPgm object */
	sts = ldh_GetParent(ldhses, wind, &od);
        if (EVEN (sts)) return sts;

	while(1)
	{
	        
	  sts = ldh_GetObjectClass(ldhses, od, &cid);
          if (EVEN (sts)) return sts;

          if (plc_pgm == 0)
	  {
	    sts = ldh_ClassNameToId( ldhses, &plc_pgm, "PlcPgm");
	    if (EVEN (sts)) return sts;
	  }

	  if (cid == plc_pgm)
	    break;

	  sts = ldh_GetParent(ldhses, od, &od);
          if (EVEN (sts)) return sts;
	}

        sts = gcg_plcpgm_to_operating_system(ldhses, od, os);

	return sts;
}
/*************************************************************************
*
* Name:		gcg_plcpgm_to_operating_system()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*
**************************************************************************/

int gcg_plcpgm_to_operating_system
    ( 
    ldh_tSesContext 	ldhses, 
    pwr_tObjid		plcpgm, 
    pwr_mOpSys		*os
    )
{
	int		sts, size;
	pwr_mOpSys    	*operating_system;
	pwr_tObjid	objid;

	objid =  plcpgm;
	objid.oix = 0;

        /* Get the operating system for this volume */
        sts = ldh_GetObjectPar(ldhses, objid, "SysBody", "OperatingSystem",
	  (char **)&operating_system, &size);
        if ( EVEN(sts)) return sts;
   
        *os = *operating_system;
	free((char *)operating_system);

	return GSX__SUCCESS;
}

/*************************************************************************
*
* Name:		gcg_file_check()
*
* Type		int
*
* Type		Parameter	IOGF	Description
*
* Description:
*
**************************************************************************/

static int	gcg_file_check(
    char	*filename
)
{
	FILE 		*checkfile;
	pwr_tFileName	found_file;
	pwr_tStatus 	sts;
        unsigned long 	search_ctx;

        search_ctx = 0;
        sts = dcli_search_file( filename, found_file, DCLI_DIR_SEARCH_INIT);
        dcli_search_file( filename, found_file, DCLI_DIR_SEARCH_END);
 	if ( EVEN(sts))
	{
	  /* Create the file */
	  checkfile = fopen( filename,"w");
	  if ( checkfile == 0)
	    return GSX__OPENFILE;

	  fprintf( checkfile, "/*  Filename: %s    */\n", filename);
	  fclose( checkfile);
	  return GSX__FILECREATED;
	}
	return GSX__SUCCESS;
}

static pwr_tStatus gcg_replace_ref( gcg_ctx gcgctx, pwr_sAttrRef *attrref, 
				    vldh_t_node node)
{
  pwr_tStatus sts;
  pwr_tOid host_oid;
  pwr_tCid host_cid;
  int size;

  if ( node->hn.wind->hw.plc->lp.cid == pwr_cClass_PlcTemplate)
    return GSX__NEXTNODE;

  if ( attrref->Objid.vid == ldh_cPlcMainVolume) {
    pwr_sAttrRef *connect_arp;
    pwr_tCid connect_cid;

    // Replace objid with objid in attribute PlcConnect in host object

    /* Get the parent node to this window */
    host_oid = (node->hn.wind)->lw.poid;

    if ( cdh_ObjidIsNull(host_oid)) {	
      /* Parent is a plcprogram */
      gcg_error_msg( gcgctx, GSX__BADWIND, node);  
      return GSX__NEXTNODE;
    }

    /* Check class of this objid */
    sts = ldh_GetObjectClass( gcgctx->ldhses, host_oid, &host_cid);
    if ( EVEN(sts))  {
      gcg_error_msg( gcgctx, GSX__REFOBJ, node);
      return GSX__NEXTNODE;
    }

    if ( host_cid == pwr_cClass_order ||
	 host_cid == pwr_cClass_csub ||
	 host_cid == pwr_cClass_substep) {
      gcg_error_msg( gcgctx, GSX__BADWIND, node);
      return GSX__NEXTNODE;
    }

    sts = ldh_GetObjectPar( gcgctx->ldhses, host_oid, "RtBody", "PlcConnect",
			    (char **)&connect_arp, &size);
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__BADWIND, node);
      return GSX__NEXTNODE;
    }

    if ( cdh_ObjidIsNull(connect_arp->Objid)) {	
      gcg_error_msg( gcgctx, GSX__REFOBJ, node);  
      return GSX__NEXTNODE;
    }

    // Check that the class or the connectedobject, or any superclass,
    // matches the symbolic reference
    sts = ldh_GetAttrRefOrigTid( gcgctx->ldhses, connect_arp, &connect_cid);
    while ( ODD(sts)) {
      if ( connect_cid == attrref->Objid.oix)
	break;
	
      sts = ldh_GetSuperClass( gcgctx->ldhses, connect_cid, &connect_cid);
    }
    if ( EVEN(sts)) {
      gcg_error_msg( gcgctx, GSX__REFCONNECT, node);  
      return GSX__NEXTNODE;
    }

    attrref->Objid = connect_arp->Objid;
    attrref->Offset += connect_arp->Offset;
    free( (char *)connect_arp);
    return GSX__REPLACED;
  }
  else if ( attrref->Objid.vid == ldh_cPlcFoVolume) {
    // Replace objid with host object

    /* Get the parent node to this window */
    host_oid = (node->hn.wind)->lw.poid;

    if ( cdh_ObjidIsNull(host_oid)) {	
      /* Parent is a plcprogram */
      gcg_error_msg( gcgctx, GSX__BADWIND, node);  
      return GSX__NEXTNODE;
    }

    /* Check class of this objdid */
    sts = ldh_GetObjectClass( gcgctx->ldhses, host_oid, &host_cid);
    if ( EVEN(sts))  {
      gcg_error_msg( gcgctx, GSX__REFOBJ, node);
      return GSX__NEXTNODE;
    }

    if ( host_cid == pwr_cClass_order ||
	 host_cid == pwr_cClass_csub ||
	 host_cid == pwr_cClass_substep) {
      gcg_error_msg( gcgctx, GSX__BADWIND, node);
      return GSX__NEXTNODE;
    }

    attrref->Objid = host_oid;
    return GSX__REPLACED;
  }
  return GSX__SUCCESS;
}

static int gcg_is_window( gcg_ctx gcgctx, pwr_tOid oid)
{
  pwr_sPlcWindow *windbuffer;
  pwr_tClassId windclass;
  int size;
  pwr_tStatus sts;

  sts = ldh_GetObjectBuffer( gcgctx->ldhses, oid, "DevBody", "PlcWindow", 
			     (pwr_eClass *) &windclass, 
			     (char **)&windbuffer, &size);
  if ( ODD(sts)) {
    free((char *) windbuffer);
    return 1;
  }
  return 0;
}


/*************************************************************************
*
*  Set compilation manager for all nodes in a window.
*
**************************************************************************/
static int gcg_set_cmanager( vldh_t_wind wind)
{
  int			sts;
  unsigned long		node_count;
  vldh_t_node		*nodelist;
  vldh_t_node		mgr;
  int			j;

  /* Get all nodes this window */
  vldh_get_nodes( wind, &node_count, &nodelist);

  /* Reset compilation manager */
  for ( j = 0; j < (int)node_count; j++) {
    nodelist[j]->hn.comp_manager = 0;
  }	  

  /* Find managers in this window */
  for ( j = 0; j < (int)node_count; j++) {
    mgr = nodelist[j];
    switch ( mgr->ln.cid) {
    case pwr_cClass_CArea: {
      sts = gcg_cmanager_find_nodes( wind, mgr, nodelist, node_count);
      if ( EVEN(sts)) {
	if ( node_count > 0)
	  free((char *) nodelist);
	return sts;
      }
      break;
    }
    default: ;
    }
  }	  
  if ( node_count > 0)
    free((char *) nodelist);

  return GSX__SUCCESS;
}

static int gcg_cmanager_find_nodes( vldh_t_wind wind, vldh_t_node mgr, 
				vldh_t_node *nodelist, int node_count)
{
  int			sts;
  vldh_t_node		n;
  int			i;

  /* Find managers in this manager */
  for ( i = 0; i < node_count; i++) {
    n = nodelist[i];
    if ( n == mgr)
      continue;
    switch ( n->ln.cid) {
    case pwr_cClass_CArea: {
      if ( mgr->ln.x <= n->ln.x + n->ln.width/2 &&
	   mgr->ln.x + mgr->ln.width >= n->ln.x + n->ln.width/2 &&
	   mgr->ln.y <= n->ln.y + n->ln.height/2 &&
	   mgr->ln.y + mgr->ln.height >= n->ln.y + n->ln.height/2) {
	sts = gcg_cmanager_find_nodes( wind, n, nodelist, node_count);
	if ( EVEN(sts)) return sts;
	if ( n->hn.comp_manager == 0)
	  n->hn.comp_manager = mgr;
      }
      break;
    }
    default: ;
    }
  }

  /* Find nodes for this manager */
  for ( i = 0; i < node_count; i++) {
    n = nodelist[i];
    if ( n == mgr)
      continue;
    if ( n->hn.comp_manager == 0) {
      if ( mgr->ln.x <= n->ln.x + n->ln.width/2 &&
	   mgr->ln.x + mgr->ln.width >= n->ln.x + n->ln.width/2 &&
	   mgr->ln.y <= n->ln.y + n->ln.height/2 &&
	   mgr->ln.y + mgr->ln.height >= n->ln.y + n->ln.height/2) {
	n->hn.comp_manager = mgr;
      }
    }
  }	  
  return GSX__SUCCESS;
}

static int gcg_cmanager_comp( 
    gcg_ctx	gcgctx,
    vldh_t_node	node
)
{
  pwr_tClassId		bodyclass;
  pwr_sGraphPlcNode 	*graphbody;
  int 			sts, size;
  int			compmethod;
  int			mcompmethod;
  vldh_t_wind		wind;
  
  wind = node->hn.wind;

  sts = ldh_GetClassBody(wind->hw.ldhses, node->hn.comp_manager->ln.cid, 
			 "GraphPlcNode", &bodyclass, 
			 (char **)&graphbody, &size);
  if ( EVEN(sts)) return sts;
  
  mcompmethod = graphbody->compmethod;

  /* Get comp method for this node */
  sts = ldh_GetClassBody(wind->hw.ldhses, node->ln.cid, 
			 "GraphPlcNode", &bodyclass, 
			 (char **)&graphbody, &size);
  if ( EVEN(sts)) return sts;
  
  compmethod = graphbody->compmethod;

  if ( compmethod != 2 && compmethod != 8 && compmethod != 10) {
    gcgctx->cmanager_active = 1;
    sts = (gcg_comp_m[ mcompmethod ]) (gcgctx, node->hn.comp_manager); 
    if ( EVEN(sts)) return sts;
    gcgctx->cmanager_active = 0;
  }

  sts = (gcg_comp_m[ compmethod ]) (gcgctx, node); 
  return sts;
}	

static int gcg_reset_cmanager( 
    gcg_ctx	gcgctx
)
{
  gcgctx->current_cmanager = 0;
  IF_PR fprintf( gcgctx->files[GCGM1_CODE_FILE], 
		 "}\n");
  return GSX__SUCCESS;
}	


