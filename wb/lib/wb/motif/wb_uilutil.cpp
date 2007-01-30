/* 
 * Proview   $Id: wb_uilutil.cpp,v 1.1 2007-01-04 07:29:02 claes Exp $
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

/* wb_uilutil.c 
   This module contains utility routines for UIL-file handling.  */

#include <stdio.h>
#include <stdlib.h>
#include "wb_uilutil.h"

#include <Mrm/MrmPublic.h>

/*_Local procedures___________________________________________*/

static void s_errors( char *problem_string, char *ps2)
{
  if (ps2)
    printf(problem_string, ps2 );
  else
    printf("%s\n", problem_string);
  exit(0);
}

/*_Global procedures__________________________________________*/

/************************************************************************
*
* Name: uilutil_fetch
*
* Type: void
*
* Type		Parameter	IOGF	Description
* char * 	db_file_v[]	I	List of UIL-files
* int		db_file_n	I	Number of UIL-files in list
* MrmRegisterArg reglist[]	I	List of names to register
* int		reglist_num	I	Number of names to register
* Widget	parent		I	parent widget
* char *	name		I	name of UIL-file template instance
* char		*newname	I	Name of created widget
* Args		args		I	Resources to modify template with
* int		argc		I	Number of resources
* Widget	*child		O	Address to return widget in
* MrmHierarchy * ret_DRMh	O	Address to return hierarcy address in.
*					If given as NULL, UIL-file is opened and
*					closed.
*
* Description:
*	Work procedure to add the pupup menu
*
*************************************************************************/

void uilutil_fetch(
		char *db_file_v[],
		int db_file_n,
		MrmRegisterArg reglist[],
		int reglist_num,
		Widget parent,
		char *name,
		char *newname,
		ArgList args,
		int argc,
		Widget *child,
		MrmHierarchy *ret_DRMh
		)
{

/* DRM database hierarchy related variables */
  MrmHierarchy s_DRMh;
  MrmType dclass;
  int sts;

/*
 * Open the UID files (the output of the UIL compiler) in the hierarchy
 *  if the hierarchy parameter is given check if already open.
 * Register the items DRM needs to bind for us.
 * Create a new svn-widget
 */ 

  if (ret_DRMh == NULL || (ret_DRMh != NULL && *ret_DRMh == NULL))
  {
    sts = MrmOpenHierarchy(db_file_n, db_file_v, NULL, &s_DRMh);
    if (sts != MrmSUCCESS) s_errors("can't open %s\n", db_file_v[0]);
    if (ret_DRMh != NULL) *ret_DRMh = s_DRMh;
  }
  else
    s_DRMh = *ret_DRMh;

  MrmRegisterNames(reglist, reglist_num);

  sts = MrmFetchWidgetOverride( s_DRMh, name, parent,
			newname, args, argc, child, &dclass);
  if (sts != MrmSUCCESS) s_errors("can't fetch %s\n", name);
  if (ret_DRMh == NULL)
  {
    MrmCloseHierarchy(s_DRMh);
    s_DRMh = NULL;
  }
}

/*------------------------------------------------------------------------*/
/*
 *++
 * Routine:
 *	int vms_perror(userstring, msgid )
 *
 * Functional description:
 *	Read the system error with the $GETMSG system call and
 *	output the userstring and the errormessage on stdout.
 *
 * Input parameters:
 *	*userstring	Pointer to the text to be output before the
 *			system message.
 *	msgid		System message id code.
 *
 * Implicit inputs:
 *	None
 *
 * Output parameters:
 *	None
 *
 * Implicit outputs:
 *	None
 *
 * Routine value:
 *	None
 *
 * Side effects:
 *	None
 *
 *--
 */
/*------------------------------------------------------------------------*/

void vms_perror(
	char	*userstring,
	int	msgid)
{
  static char	msgbuf[257];
#if defined OS_VMS
  short	msglen;
  int	outadr;
  static $DESCRIPTOR( buf, msgbuf );
#endif

  if (userstring == NULL) userstring = "";
#if defined OS_VMS
  sys$getmsg( msgid, &msglen, &buf, 15, &outadr );
  msgbuf[msglen] = '\0';
#else
  sprintf( msgbuf, "uilutil: %x", msgid);
#endif
  printf("%s%s%s\n", userstring, strlen( userstring ) > 0 ? ": " : "", msgbuf);
}