/* 
 * Proview   $Id: xtt_ev_motif.cpp,v 1.1 2007-01-04 08:30:03 claes Exp $
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

/* xtt_ev_motif.cpp -- Alarm and event window in xtt */

#include "glow_std.h"

#include <stdio.h>
#include <stdlib.h>

#include "co_cdh.h"
#include "co_time.h"
#include "co_dcli.h"
#include "pwr_baseclasses.h"
#include "rt_gdh.h"
#include "rt_mh.h"
#include "rt_mh_outunit.h"
#include "rt_mh_util.h"

#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Xm/Text.h>
#include <Mrm/MrmPublic.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "flow_x.h"
#include "co_lng.h"
#include "co_mrm_util.h"
#include "xtt_evlist_motif.h"
#include "xtt_ev_motif.h"
#include "rt_xnav_msg.h"

EvMotif::EvMotif( void *ev_parent_ctx,
		  Widget	ev_parent_wid,
		  char *eve_name,
		  char *ala_name,
		  char *blk_name,
		  pwr_tObjid ev_user,
		  int display_ala,
		  int display_eve,
		  int display_blk,
		  int display_return,
		  int display_ack,
		  int ev_beep,
		  pwr_tStatus *status) :
  Ev( ev_parent_ctx, eve_name, ala_name, blk_name, ev_user, display_ala, display_eve,
      display_blk, display_return, display_ack, ev_beep, status),
  parent_wid(ev_parent_wid), parent_wid_eve(NULL), parent_wid_ala(NULL)
{
  char		uid_filename[120] = {"xtt_eve.uid"};
  char		*uid_filename_p = uid_filename;
  Arg 		args[20];
  pwr_tStatus	sts;
  int		i;
  pwr_sClass_User	*userobject_ptr;
  MrmHierarchy s_DRMh;
  MrmType dclass;

  static char eve_translations[] =
    "<FocusIn>: eve_inputfocus()\n";
  static char ala_translations[] =
    "<FocusIn>: ala_inputfocus()\n";
  static char blk_translations[] =
    "<FocusIn>: blk_inputfocus()\n";
  static XtTranslations eve_compiled_translations = NULL;
  static XtTranslations ala_compiled_translations = NULL;
  static XtTranslations blk_compiled_translations = NULL;

  static XtActionsRec eve_actions[] =
  {
    {"eve_inputfocus",      (XtActionProc) eve_action_inputfocus}
  };
  static XtActionsRec ala_actions[] =
  {
    {"ala_inputfocus",      (XtActionProc) ala_action_inputfocus}
  };
  static XtActionsRec blk_actions[] =
  {
    {"blk_inputfocus",      (XtActionProc) blk_action_inputfocus}
  };

  static MrmRegisterArg	reglist[] = {
        { "ev_ctx", 0 },
	{"ev_eve_activate_exit",(caddr_t)eve_activate_exit },
	{"ev_eve_activate_print",(caddr_t)eve_activate_print },
	{"ev_eve_activate_ack_last",(caddr_t)eve_activate_ack_last },
	{"ev_eve_activate_zoom_in",(caddr_t)eve_activate_zoom_in },
	{"ev_eve_activate_zoom_out",(caddr_t)eve_activate_zoom_out },
	{"ev_eve_activate_zoom_reset",(caddr_t)eve_activate_zoom_reset },
	{"ev_eve_activate_open_plc",(caddr_t)eve_activate_open_plc },
	{"ev_eve_activate_display_in_xnav",(caddr_t)eve_activate_display_in_xnav },
	{"ev_eve_activate_disp_hundredth",(caddr_t)eve_activate_disp_hundredth },
	{"ev_eve_activate_hide_object",(caddr_t)eve_activate_hide_object },
	{"ev_eve_activate_hide_text",(caddr_t)eve_activate_hide_text },
	{"ev_eve_activate_help",(caddr_t)eve_activate_help },
	{"ev_eve_activate_helpevent",(caddr_t)eve_activate_helpevent },
	{"ev_eve_create_form",(caddr_t)eve_create_form },
	{"ev_ala_activate_exit",(caddr_t)ala_activate_exit },
	{"ev_ala_activate_print",(caddr_t)ala_activate_print },
	{"ev_ala_activate_ack_last",(caddr_t)ala_activate_ack_last },
	{"ev_ala_activate_zoom_in",(caddr_t)ala_activate_zoom_in },
	{"ev_ala_activate_zoom_out",(caddr_t)ala_activate_zoom_out },
	{"ev_ala_activate_zoom_reset",(caddr_t)ala_activate_zoom_reset },
	{"ev_ala_activate_open_plc",(caddr_t)ala_activate_open_plc },
	{"ev_ala_activate_display_in_xnav",(caddr_t)ala_activate_display_in_xnav },
	{"ev_ala_activate_disp_hundredth",(caddr_t)ala_activate_disp_hundredth },
	{"ev_ala_activate_hide_object",(caddr_t)ala_activate_hide_object },
	{"ev_ala_activate_hide_text",(caddr_t)ala_activate_hide_text },
	{"ev_ala_activate_help",(caddr_t)ala_activate_help },
	{"ev_ala_activate_helpevent",(caddr_t)ala_activate_helpevent },
	{"ev_ala_create_form",(caddr_t)ala_create_form },
	{"ev_blk_activate_exit",(caddr_t)blk_activate_exit },
	{"ev_blk_activate_print",(caddr_t)blk_activate_print },
	{"ev_blk_activate_zoom_in",(caddr_t)blk_activate_zoom_in },
	{"ev_blk_activate_zoom_out",(caddr_t)blk_activate_zoom_out },
	{"ev_blk_activate_zoom_reset",(caddr_t)blk_activate_zoom_reset },
	{"ev_blk_activate_block_remove",(caddr_t)blk_activate_block_remove },
	{"ev_blk_activate_open_plc",(caddr_t)blk_activate_open_plc },
	{"ev_blk_activate_display_in_xnav",(caddr_t)blk_activate_display_in_xnav },
	{"ev_blk_activate_help",(caddr_t)blk_activate_help },
	{"ev_blk_create_form",(caddr_t)blk_create_form }
	};
  static int	reglist_num = (sizeof reglist / sizeof reglist[0]);

  *status = 1;

  Lng::get_uid( uid_filename, uid_filename);

  // Check user object
  if ( cdh_ObjidIsNull( user))
  {
    *status = XNAV__NOUSER;
    return;
  }

  sts = gdh_ObjidToPointer ( user, (pwr_tAddress *) &userobject_ptr);
  if ( EVEN(sts)) 
  {
    *status = XNAV__NOUSER;
    return;
  }
  ala_size = userobject_ptr->MaxNoOfAlarms;
  eve_size = userobject_ptr->MaxNoOfEvents;
  blk_size = 0;
  create_aliaslist( userobject_ptr);

  reglist[0].value = (caddr_t) this;

  // Motif
  MrmInitialize();


  // Save the context structure in the widget
  i = 0;
  XtSetArg(args[i], XmNuserData, (unsigned int) this);i++;
  XtSetArg(args[i], XmNdeleteResponse, XmDO_NOTHING);i++;

  sts = MrmOpenHierarchy( 1, &uid_filename_p, NULL, &s_DRMh);
  if (sts != MrmSUCCESS) printf("can't open %s\n", uid_filename);

  MrmRegisterNames(reglist, reglist_num);

  parent_wid_eve = XtCreatePopupShell( eve_name, 
		topLevelShellWidgetClass, parent_wid, args, i);

  sts = MrmFetchWidgetOverride( s_DRMh, "eve_window", parent_wid_eve,
			eve_name, args, 1, &toplevel_eve, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch %s\n", eve_name);

  parent_wid_ala = XtCreatePopupShell( ala_name, 
		topLevelShellWidgetClass, parent_wid, args, i);

  sts = MrmFetchWidgetOverride( s_DRMh, "ala_window", parent_wid_ala,
			ala_name, args, 1, &toplevel_ala, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch %s\n", ala_name);

  parent_wid_blk = XtCreatePopupShell( blk_name, 
		topLevelShellWidgetClass, parent_wid, args, i);

  sts = MrmFetchWidgetOverride( s_DRMh, "blk_window", parent_wid_blk,
			blk_name, args, 1, &toplevel_blk, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch %s\n", blk_name);

  MrmCloseHierarchy(s_DRMh);

  if ( eve_compiled_translations == NULL) 
  {
    XtAppAddActions( XtWidgetToApplicationContext( toplevel_eve), 
		eve_actions, XtNumber(eve_actions));
    eve_compiled_translations = XtParseTranslationTable( eve_translations);
  }
  XtOverrideTranslations( toplevel_eve, eve_compiled_translations);

  if ( ala_compiled_translations == NULL) 
  {
    XtAppAddActions( XtWidgetToApplicationContext( toplevel_ala), 
		ala_actions, XtNumber(ala_actions));
    ala_compiled_translations = XtParseTranslationTable( ala_translations);
  }
  XtOverrideTranslations( toplevel_ala, ala_compiled_translations);

  if ( blk_compiled_translations == NULL) 
  {
    XtAppAddActions( XtWidgetToApplicationContext( toplevel_blk), 
		blk_actions, XtNumber(blk_actions));
    blk_compiled_translations = XtParseTranslationTable( blk_translations);
  }
  XtOverrideTranslations( toplevel_blk, blk_compiled_translations);

  i = 0;
  XtSetArg(args[i],XmNwidth,700);i++;
  XtSetArg(args[i],XmNheight,600);i++;
  XtSetValues( toplevel_eve ,args,i);
    
  i = 0;
  XtSetArg(args[i],XmNwidth,700);i++;
  XtSetArg(args[i],XmNheight,300);i++;
  XtSetValues( toplevel_ala ,args,i);
    
  i = 0;
  XtSetArg(args[i],XmNwidth,700);i++;
  XtSetArg(args[i],XmNheight,300);i++;
  XtSetValues( toplevel_blk ,args,i);
    
  XtManageChild( toplevel_eve);
  XtManageChild( toplevel_ala);
  XtManageChild( toplevel_blk);

  // Create ala and eve...
  eve = new EvListMotif( this, form_eve, ev_eType_EventList, eve_size, &eve_widget);
  eve->start_trace_cb = &eve_start_trace_cb;
  eve->display_in_xnav_cb = &eve_display_in_xnav_cb;
  eve->name_to_alias_cb = &ev_name_to_alias_cb;
  eve->popup_menu_cb = &ev_popup_menu_cb;

  ala = new EvListMotif( this, form_ala, ev_eType_AlarmList, ala_size, &ala_widget);
  ala->start_trace_cb = &ala_start_trace_cb;
  ala->display_in_xnav_cb = &ala_display_in_xnav_cb;
  ala->name_to_alias_cb = &ev_name_to_alias_cb;
  ala->popup_menu_cb = &ev_popup_menu_cb;
  ala->sound_cb = &ev_sound_cb;

  blk = new EvListMotif( this, form_blk, ev_eType_BlockList, blk_size, &blk_widget);
  blk->start_trace_cb = &blk_start_trace_cb;
  blk->display_in_xnav_cb = &blk_display_in_xnav_cb;
  blk->popup_menu_cb = &ev_popup_menu_cb;
  // blk->hide_text = 1;

//  XtManageChild( form_widget);

  if ( display_eve) {
    XtPopup( parent_wid_eve, XtGrabNone);
    eve_displayed = 1;
  }
  else
    XtRealizeWidget( parent_wid_eve);

  if ( display_ala) {
    XtPopup( parent_wid_ala, XtGrabNone);
    ala_displayed = 1;
  }
  else
    XtRealizeWidget( parent_wid_ala);

  if ( display_blk) {
    XtPopup( parent_wid_blk, XtGrabNone);
    blk_displayed = 1;
  }
  else
    XtRealizeWidget( parent_wid_blk);


  // Connect the window manager close-button to exit
  flow_AddCloseVMProtocolCb( parent_wid_eve, 
	(XtCallbackProc)eve_activate_exit, this);
  flow_AddCloseVMProtocolCb( parent_wid_ala, 
	(XtCallbackProc)ala_activate_exit, this);
  flow_AddCloseVMProtocolCb( parent_wid_blk, 
	(XtCallbackProc)blk_activate_exit, this);

  // Store this for the mh callbacks
  ev = this;

  sts = outunit_connect( user);
  if ( EVEN(sts))
    *status = sts;
}


//
//  Delete ev
//
EvMotif::~EvMotif()
{
  if ( connected)
    mh_OutunitDisconnect();
  if ( parent_wid_eve)
    XtDestroyWidget( parent_wid_eve);
  if ( parent_wid_ala)
    XtDestroyWidget( parent_wid_ala);
  if ( eve)
    delete eve;
  if ( ala)
    delete ala;
  ev = NULL;
}

void EvMotif::map_eve()
{
  if ( !eve_displayed) {
    flow_MapWidget( parent_wid_eve);
    eve_displayed = 1;
  }
  else {
    flow_UnmapWidget( parent_wid_eve);
    flow_MapWidget( parent_wid_eve);
  }
}

void EvMotif::map_ala()
{
  if ( !ala_displayed) {
    flow_MapWidget( parent_wid_ala);
    ala_displayed = 1;
  }
  else {
    flow_UnmapWidget( parent_wid_ala);
    flow_MapWidget( parent_wid_ala);
  }
}

void EvMotif::map_blk()
{
  if ( !blk_displayed) {
    flow_MapWidget( parent_wid_blk);
    blk_displayed = 1;
  }
  else {
    flow_UnmapWidget( parent_wid_blk);
    flow_MapWidget( parent_wid_blk);
  }
}

void EvMotif::unmap_eve()
{
  if ( eve_displayed) {
    flow_UnmapWidget( parent_wid_eve);
    eve_displayed = 0;
  }
}

void EvMotif::unmap_ala()
{
  if ( ala_displayed) {
    flow_UnmapWidget( parent_wid_ala);
    ala_displayed = 0;
  }
}

void EvMotif::unmap_blk()
{
  if ( blk_displayed) {
    flow_UnmapWidget( parent_wid_blk);
    blk_displayed = 0;
  }
}

void EvMotif::eve_action_inputfocus( Widget w, XmAnyCallbackStruct *data)
{
  Arg args[1];
  Ev *ev;

  XtSetArg    (args[0], XmNuserData, &ev);
  XtGetValues (w, args, 1);

  if ( mrm_IsIconicState(w))
    return;

  if ( ev && ev->eve_displayed) {
    if ( ((EvMotif *)ev)->eve_focustimer.disabled())
      return;

    ev->eve->set_input_focus();
    ((EvMotif *)ev)->eve_focustimer.disable( ((EvMotif *)ev)->toplevel_eve, 400);
  }
}

void EvMotif::ala_action_inputfocus( Widget w, XmAnyCallbackStruct *data)
{
  Arg args[1];
  Ev *ev;

  XtSetArg    (args[0], XmNuserData, &ev);
  XtGetValues (w, args, 1);

  if ( mrm_IsIconicState(w))
    return;

  if ( ev && ev->ala_displayed) {
    if ( ((EvMotif *)ev)->ala_focustimer.disabled())
      return;

    ev->ala->set_input_focus();
    ((EvMotif *)ev)->ala_focustimer.disable( ((EvMotif *)ev)->toplevel_ala, 400);
  }
}

void EvMotif::blk_action_inputfocus( Widget w, XmAnyCallbackStruct *data)
{
  Arg args[1];
  Ev *ev;

  XtSetArg    (args[0], XmNuserData, &ev);
  XtGetValues (w, args, 1);

  if ( mrm_IsIconicState(w))
    return;

  if ( ev && ev->blk_displayed) {
    if ( ((EvMotif *)ev)->blk_focustimer.disabled())
      return;

    ev->blk->set_input_focus();
    ((EvMotif *)ev)->blk_focustimer.disable( ((EvMotif *)ev)->toplevel_blk, 400);
  }
}

void EvMotif::eve_activate_exit( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  flow_UnmapWidget( ((EvMotif *)ev)->parent_wid_eve);
  ev->eve_displayed = 0;
}

void EvMotif::ala_activate_exit( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  flow_UnmapWidget( ((EvMotif *)ev)->parent_wid_ala);
  ev->ala_displayed = 0;
}

void EvMotif::blk_activate_exit( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  flow_UnmapWidget( ((EvMotif *)ev)->parent_wid_blk);
  ev->blk_displayed = 0;
}

void EvMotif::eve_activate_print( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve_activate_print();
}

void EvMotif::ala_activate_print( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala_activate_print();
}

void EvMotif::blk_activate_print( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk_activate_print();
}

void EvMotif::eve_activate_ack_last( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve_activate_ack_last();
}

void EvMotif::ala_activate_ack_last( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  eve_activate_ack_last( w, ev, data);
}

void EvMotif::eve_activate_zoom_in( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve->zoom( 1.2);
}

void EvMotif::ala_activate_zoom_in( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala->zoom( 1.2);
}

void EvMotif::blk_activate_zoom_in( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->zoom( 1.2);
}

void EvMotif::eve_activate_zoom_out( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve->zoom( 5.0/6);
}

void EvMotif::ala_activate_zoom_out( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala->zoom( 5.0/6);
}

void EvMotif::blk_activate_zoom_out( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->zoom( 5.0/6);
}

void EvMotif::eve_activate_zoom_reset( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve->unzoom();
}

void EvMotif::ala_activate_zoom_reset( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala->unzoom();
}

void EvMotif::blk_activate_zoom_reset( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->unzoom();
}

void EvMotif::blk_activate_block_remove( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->block_remove();
}

void EvMotif::eve_activate_open_plc( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve->start_trace();
}

void EvMotif::ala_activate_open_plc( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala->start_trace();
}

void EvMotif::blk_activate_open_plc( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->start_trace();
}

void EvMotif::eve_activate_display_in_xnav( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve->display_in_xnav();
}

void EvMotif::ala_activate_display_in_xnav( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala->display_in_xnav();
}

void EvMotif::blk_activate_display_in_xnav( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk->display_in_xnav();
}

void EvMotif::eve_activate_disp_hundredth( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->eve->set_display_hundredth( data->set);
}

void EvMotif::ala_activate_disp_hundredth( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->ala->set_display_hundredth( data->set);
}

void EvMotif::eve_activate_hide_object( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->eve->set_hide_object( data->set);
}

void EvMotif::ala_activate_hide_object( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->ala->set_hide_object( data->set);
}

void EvMotif::eve_activate_hide_text( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->eve->set_hide_text( data->set);
}

void EvMotif::ala_activate_hide_text( Widget w, Ev *ev, XmToggleButtonCallbackStruct *data)
{
  ev->ala->set_hide_text( data->set);
}

void EvMotif::eve_activate_help( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve_activate_help();
}

void EvMotif::eve_activate_helpevent( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->eve_activate_helpevent();
}

void EvMotif::ala_activate_help( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala_activate_help();
}

void EvMotif::ala_activate_helpevent( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->ala_activate_helpevent();
}

void EvMotif::blk_activate_help( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ev->blk_activate_help();
}

void EvMotif::eve_create_form( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ((EvMotif *)ev)->form_eve = w;
}

void EvMotif::ala_create_form( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ((EvMotif *)ev)->form_ala = w;
}

void EvMotif::blk_create_form( Widget w, Ev *ev, XmAnyCallbackStruct *data)
{
  ((EvMotif *)ev)->form_blk = w;
}