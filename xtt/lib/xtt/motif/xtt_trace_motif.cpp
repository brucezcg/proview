/* 
 * Proview   $Id$
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
 */

/* xtt_trace_motif.cpp -- trace in runtime environment */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined OS_LINUX
#include <sys/stat.h>
#endif

#include "pwr.h"
#include "pwr_baseclasses.h"
#include "pwr_privilege.h"

#include "flow_ctx.h"
#include "flow_api.h"

#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Mrm/MrmPublic.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/DialogS.h>

#include "co_cdh.h"
#include "co_api.h"
#include "co_dcli.h"
#include "cow_mrm_util.h"
#include "rt_gdh.h"
#include "flow_x.h"
#include "flow_widget_motif.h"
#include "xtt_trace_motif.h"
#include "cow_wow_motif.h"

#define GOEN_F_GRID 0.05

void RtTraceMotif::create_form( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  ((RtTraceMotif *)tractx)->form = w;
}

void RtTraceMotif::create_menu( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  ((RtTraceMotif *)tractx)->menu = w;
}

void RtTraceMotif::activate_close( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_close();
}

void RtTraceMotif::activate_print( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_print();
}

void RtTraceMotif::activate_printselect( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_printselect();
}

void RtTraceMotif::activate_savetrace( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_savetrace();
}

void RtTraceMotif::activate_restoretrace( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_restoretrace();
}

void RtTraceMotif::activate_cleartrace( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_cleartrace();
}

void RtTraceMotif::activate_display_object( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_display_object();
}

void RtTraceMotif::activate_collect_insert( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_collect_insert();
}

void RtTraceMotif::activate_open_object( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_open_object();
}

void RtTraceMotif::activate_show_cross( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_show_cross();
}

void RtTraceMotif::activate_open_classgraph( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_open_classgraph();
}

void RtTraceMotif::activate_trace( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_trace();
}

void RtTraceMotif::activate_simulate( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_simulate();
}

void RtTraceMotif::activate_view( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_view();
}

void RtTraceMotif::activate_zoomin( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  flow_Zoom( tractx->flow_ctx, 1.0/0.7);
}

void RtTraceMotif::activate_zoomout( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  flow_Zoom( tractx->flow_ctx, 0.7);
}

void RtTraceMotif::activate_zoomreset( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  flow_UnZoom( tractx->flow_ctx);
}

void RtTraceMotif::activate_scantime1( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->scan_time = 0.5;
}

void RtTraceMotif::activate_scantime2( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->scan_time = 0.2;
}

void RtTraceMotif::activate_scantime3( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->scan_time = 0.1;
}

void RtTraceMotif::activate_scantime4( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->scan_time = 0.05;
}

void RtTraceMotif::activate_scantime5( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->scan_time = 0.02;
}

void RtTraceMotif::activate_help( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_help();
}

void RtTraceMotif::activate_helpplc( Widget w, RtTrace *tractx, XmAnyCallbackStruct *data)
{
  tractx->activate_helpplc();
}

RtTraceMotif::~RtTraceMotif()
{
  trace_tNode *node, *fnode;

  trace_stop();

  /* Delete all trace children windows */
  for (node = trace_list; node;) {
    delete node->tractx;
    fnode = node->Next;
    free((char *)node);
    node = fnode;
  }

  delete wow;

  /* Destroy the widgets */
  XtDestroyWidget( toplevel);
}

void RtTraceMotif::pop()
{
  XtPopup( toplevel, XtGrabNone);
/*
  XtUnmapWidget( tractx->toplevel);
  XtMapWidget( tractx->toplevel);
*/
}

void RtTraceMotif::popup_menu_position( int event_x, int event_y, int *x, int *y) 
{
  CoWowMotif::PopupPosition( flow_widget, event_x, event_y, x, y);
}

RtTrace *RtTraceMotif::subwindow_new( void *ctx, pwr_tObjid oid, pwr_tStatus *sts)
{
  return new RtTraceMotif( ctx, flow_widget, oid, sts);
}

RtTraceMotif::RtTraceMotif( void *tr_parent_ctx, Widget tr_parent_wid, pwr_tObjid tr_objid,
			    pwr_tStatus *status) :
  RtTrace( tr_parent_ctx, tr_objid, status), parent_wid(tr_parent_wid)
{
  // FlowWidget	fwidget;
  char		uid_filename[120] = {"xtt_trace.uid"};
  char		*uid_filename_p = uid_filename;
  Arg 		args[20];
  unsigned long sts;
  pwr_tOName   	name;
  int		i;
  pwr_tObjid	window_objid;
  pwr_tClassId	cid;
  char   	title[220];
  pwr_tOName   	hostname;
  pwr_tOName   	plcconnect;
  MrmHierarchy s_DRMh;
  MrmType dclass;
  Widget	trace_widget;
  static MrmRegisterArg	reglist[] = {
        {(char*) "tra_ctx", 0 },
	{(char*) "tra_activate_close",(caddr_t)activate_close },
	{(char*) "tra_activate_print",(caddr_t)activate_print },
	{(char*) "tra_activate_printselect",(caddr_t)activate_printselect },
	{(char*) "tra_activate_savetrace",(caddr_t)activate_savetrace },
	{(char*) "tra_activate_restoretrace",(caddr_t)activate_restoretrace },
	{(char*) "tra_activate_cleartrace",(caddr_t)activate_cleartrace },
	{(char*) "tra_activate_trace",(caddr_t)activate_trace },
	{(char*) "tra_activate_display_object",(caddr_t)activate_display_object },
	{(char*) "tra_activate_open_object",(caddr_t)activate_open_object },
	{(char*) "tra_activate_show_cross",(caddr_t)activate_show_cross },
	{(char*) "tra_activate_open_classgraph",(caddr_t)activate_open_classgraph },
	{(char*) "tra_activate_collect_insert",(caddr_t)activate_collect_insert },
	{(char*) "tra_activate_view",(caddr_t)activate_view },
	{(char*) "tra_activate_simulate",(caddr_t)activate_simulate },
	{(char*) "tra_activate_zoomin",(caddr_t)activate_zoomin },
	{(char*) "tra_activate_zoomout",(caddr_t)activate_zoomout },
	{(char*) "tra_activate_zoomreset",(caddr_t)activate_zoomreset },
	{(char*) "tra_activate_scantime1",(caddr_t)activate_scantime1 },
	{(char*) "tra_activate_scantime2",(caddr_t)activate_scantime2 },
	{(char*) "tra_activate_scantime3",(caddr_t)activate_scantime3 },
	{(char*) "tra_activate_scantime4",(caddr_t)activate_scantime4 },
	{(char*) "tra_activate_scantime5",(caddr_t)activate_scantime5 },
	{(char*) "tra_activate_help",(caddr_t)activate_help },
	{(char*) "tra_activate_helpplc",(caddr_t)activate_helpplc },
	{(char*) "tra_create_form",(caddr_t)create_form },
	{(char*) "tra_create_menu",(caddr_t)create_menu }
	};

  static int	reglist_num = (sizeof reglist / sizeof reglist[0]);

  lng_get_uid( uid_filename, uid_filename);

  sts = gdh_ObjidToName( tr_objid, name, sizeof(name), cdh_mNName); 
  if (EVEN(sts)) {
    *status = sts;
    return;
  }
  strcpy( title, "Trace ");
  strcat( title, name);

  /* Find plcwindow */
  sts = gdh_GetObjectClass( tr_objid, &cid);
  if ( EVEN(sts)) {
    *status = sts;
    return;
  }

  if ( !(cid == pwr_cClass_windowplc ||
         cid == pwr_cClass_windowcond ||
         cid == pwr_cClass_windoworderact ||
         cid == pwr_cClass_windowsubstep ))
  {

    sts = gdh_GetChild( tr_objid, &window_objid);
    if ( EVEN(sts)) {
      *status = sts;
      return;
    }
  }
  else
    window_objid = tr_objid; 

  sts = gdh_GetObjectClass( window_objid, &cid);
  if ( EVEN(sts)) {
    *status = sts;
    return;
  }

  if ( !(cid == pwr_cClass_windowplc ||
         cid == pwr_cClass_windowcond ||
         cid == pwr_cClass_windoworderact ||
         cid == pwr_cClass_windowsubstep )) {
    *status = 0;
    return;
  }

  sts = get_filename( window_objid, filename, &m_has_host, hostname, 
		      plcconnect);
  if ( EVEN(sts)) {
    *status = sts;
    return;
  }

  /* Create object context */
  objid = window_objid;
  if ( m_has_host) {
    strcpy( m_hostname, hostname);
    strcpy( m_plcconnect, plcconnect);
  }
  reglist[0].value = (caddr_t) this;
 
  toplevel = XtCreatePopupShell( name, 
		topLevelShellWidgetClass, parent_wid, args, 0);

  /* Save the context structure in the widget */
  XtSetArg (args[0], XmNuserData, (unsigned int) this);

  sts = MrmOpenHierarchy( 1, &uid_filename_p, NULL, &s_DRMh);
  if (sts != MrmSUCCESS) printf("can't open %s\n", uid_filename);

  MrmRegisterNames(reglist, reglist_num);

  sts = MrmFetchWidgetOverride( s_DRMh, (char*) "trace_window", toplevel,
			title, args, 1, &trace_widget, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch %s\n", name);

  MrmCloseHierarchy(s_DRMh);


  i = 0;
  XtSetArg(args[i],XmNwidth,800);i++;
  XtSetArg(args[i],XmNheight,600);i++;
  XtSetValues( toplevel ,args,i);
    
  XtManageChild( trace_widget);

  i = 0;
/*
  XtSetArg(args[i],XmNwidth,790);i++;
  XtSetArg(args[i],XmNheight,560);i++;
*/
  XtSetArg( args[i], XmNtopAttachment, XmATTACH_WIDGET);i++;
  XtSetArg( args[i], XmNtopWidget, menu);i++;
  XtSetArg( args[i], XmNrightAttachment, XmATTACH_FORM);i++;
  XtSetArg( args[i], XmNleftAttachment, XmATTACH_FORM);i++;
  XtSetArg( args[i], XmNbottomAttachment, XmATTACH_FORM);i++;
  flow_widget = FlowCreate( form, (char*) "Flow window", args, i, 
			    init_flow, (void *)this);

  XtManageChild( (Widget) flow_widget);
/*
  XtRealizeWidget(toplevel);
*/
  XtPopup( toplevel, XtGrabNone);

  // fwidget = (FlowWidget) flow_widget;
  // flow_ctx = (flow_tCtx)fwidget->flow.flow_ctx;
  // flow_SetCtxUserData( flow_ctx, this);

  /* Create navigator popup */

  i = 0;
  XtSetArg(args[i],XmNallowShellResize, TRUE); i++;
  XtSetArg(args[i],XmNallowResize, TRUE); i++;
  XtSetArg(args[i],XmNwidth,200);i++;
  XtSetArg(args[i],XmNheight,200);i++;
  XtSetArg(args[i],XmNx,500);i++;
  XtSetArg(args[i],XmNy,500);i++;

  nav_shell = XmCreateDialogShell( flow_widget, (char*) "Navigator",
        args, i);
  XtManageChild( nav_shell);

  i = 0;
  XtSetArg(args[i],XmNwidth,200);i++;
  XtSetArg(args[i],XmNheight,200);i++;
  nav_widget = FlowCreateNav( nav_shell, (char*) "navigator",
        args, i, flow_widget);
  XtManageChild( nav_widget);
  XtRealizeWidget( nav_shell);

  // Connect the window manager close-button to exit
  flow_AddCloseVMProtocolCb( toplevel, 
	(XtCallbackProc)activate_close, this);

  wow = new CoWowMotif( toplevel);
  trace_timerid = wow->timer_new();

  viewsetup();
  flow_Open( flow_ctx, filename);
  trasetup();
  trace_start();

#if defined OS_LINUX
  {
    struct stat info;

    if ( stat( filename, &info) != -1)
      version = info.st_ctime;    
  }
#endif

}