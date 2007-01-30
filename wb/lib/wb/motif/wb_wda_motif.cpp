/* 
 * Proview   $Id: wb_wda_motif.cpp,v 1.1 2007-01-04 07:29:02 claes Exp $
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

/* wb_wda_motif.cpp -- spreadsheet editor */

#if defined OS_VMS && defined __ALPHA
# pragma message disable (NOSIMPINT,EXTROUENCUNNOBJ)
#endif

#if defined OS_VMS && !defined __ALPHA
# pragma message disable (LONGEXTERN)
#endif

#include "glow_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/XmP.h>
#include <Xm/Text.h>
#include <Mrm/MrmPublic.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>

#include "co_cdh.h"
#include "co_dcli.h"
#include "co_time.h"
#include "flow_x.h"
#include "wb_wda_msg.h"
#include "co_mrm_util.h"
#include "co_wow_motif.h"

#include "flow.h"
#include "flow_browctx.h"
#include "flow_browapi.h"
#include "wb_wda_motif.h"
#include "wb_wdanav_motif.h"
#include "wb_wtt.h"
#include "wb_wnav.h"
#include "co_xhelp.h"


// Static member elements
char WdaMotif::value_recall[30][160];

void WdaMotif::message( char severity, char *message)
{
  Arg 		args[2];
  XmString	cstr;

  cstr=XmStringCreateLtoR( message, "ISO8859-1");
  XtSetArg(args[0],XmNlabelString, cstr);
  XtSetArg(args[1],XmNheight, 20);
  XtSetValues( msg_label, args, 2);
  XmStringFree( cstr);
}

void WdaMotif::set_prompt( char *prompt)
{
  Arg 		args[3];
  XmString	cstr;

  cstr=XmStringCreateLtoR( prompt, "ISO8859-1");
  XtSetArg(args[0],XmNlabelString, cstr);
  XtSetArg(args[1],XmNwidth, 50);
  XtSetArg(args[2],XmNheight, 30);
  XtSetValues( cmd_prompt, args, 3);
  XmStringFree( cstr);
}

void WdaMotif::change_value( int set_focus)
{
  int		sts;
  Widget	text_w;
  int		multiline;
  char		*value;
  Arg 		args[5];
  int		input_size;

  if ( input_open) {
    XtUnmanageChild( cmd_input);
    set_prompt( "");
    input_open = 0;
    return;
  }

  sts = ((WdaNav *)wdanav)->check_attr( &multiline, &input_node, input_name,
		&value, &input_size);
  if ( EVEN(sts)) {
    if ( sts == WDA__NOATTRSEL)
      message( 'E', "No attribute is selected");
    else
      message( 'E', wnav_get_message( sts));
    return;
  }

  if ( multiline) {
    text_w = cmd_scrolledinput;
    XtManageChild( text_w);
    XtManageChild( cmd_scrolled_ok);
    XtManageChild( cmd_scrolled_ca);

    // XtSetArg(args[0], XmNpaneMaximum, 300);
    // XtSetValues( wdanav_form, args, 1);

    XtSetArg(args[0], XmNmaxLength, input_size-1);
    XtSetValues( text_w, args, 1);
    if ( value) {
      XmTextSetString( text_w, value);
//    XmTextSetInsertionPosition( text_w, strlen(value));
    }
    else
      XmTextSetString( text_w, "");

    input_multiline = 1;
  }
  else {
    text_w = cmd_input;
    XtManageChild( text_w);
    XtSetArg(args[0],XmNmaxLength, input_size-1);
    XtSetValues( text_w, args, 1);
    if ( value) {
      XmTextSetString( text_w, value);
      XmTextSetInsertionPosition( text_w, strlen(value));
    }
    else
      XmTextSetString( text_w, "");

    input_multiline = 0;
  }

  if ( value)
    free( value);

  message( ' ', "");
  if ( set_focus)
    flow_SetInputFocus( text_w);
  set_prompt( "value >");
  input_open = 1;
}

//
//  Callbackfunctions from menu entries
//
void WdaMotif::activate_change_value( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->change_value(1);
}

void WdaMotif::activate_close_changeval( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->change_value_close();
}

void WdaMotif::activate_exit( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  if ( wda->close_cb)
    (wda->close_cb)( wda);
  else
    delete wda;
}
void WdaMotif::activate_print( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->print();
}
void WdaMotif::activate_setclass( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  ((Wda *)wda)->open_class_dialog();
}

void WdaMotif::activate_setattr( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->open_attr_dialog();
}

void WdaMotif::activate_nextattr( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  int sts;

  sts = wda->next_attr();
  if ( EVEN(sts))
    wda->wow->DisplayError( "Spreadsheet error", wnav_get_message( sts));
}

void WdaMotif::activate_prevattr( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  int sts;

  sts = wda->prev_attr();
  if ( EVEN(sts))
    wda->wow->DisplayError( "Spreadsheet error", wnav_get_message( sts));
}

void WdaMotif::activate_help( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  CoXHelp::dhelp( "spreadsheeteditor_refman", 0, navh_eHelpFile_Other, 
		  "$pwr_lang/man_dg.dat", true);
}

void WdaMotif::create_msg_label( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->msg_label = w;
}
void WdaMotif::create_cmd_prompt( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->cmd_prompt = w;
}
void WdaMotif::create_cmd_input( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  Arg args[2];

  XtSetArg    (args[0], XmNuserData, wda);
  XtSetValues (w, args, 1);

  mrm_TextInit( w, (XtActionProc) valchanged_cmd_input, mrm_eUtility_Wda);
  wda->cmd_input = w;
}
void WdaMotif::create_cmd_scrolledinput( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->cmd_scrolledinput = w;
}
void WdaMotif::create_cmd_scrolled_ok( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->cmd_scrolled_ok = w;
}
void WdaMotif::create_cmd_scrolled_ca( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->cmd_scrolled_ca = w;
}

void WdaMotif::create_wdanav_form( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->wdanav_form = w;
}
void WdaMotif::class_create_hiervalue( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->wdaclass_hiervalue = w;
}

void WdaMotif::class_create_classvalue( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->wdaclass_classvalue = w;
}

void WdaMotif::class_create_attrobjects( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  wda->wdaclass_attrobjects = w;
}

void WdaMotif::class_activate_ok( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  char *hiername;
  char *classname;
  int sts;
  pwr_tClassId new_classid;

  hiername = XmTextGetString( wda->wdaclass_hiervalue);
  classname = XmTextGetString( wda->wdaclass_classvalue);
  wda->attrobjects = XmToggleButtonGetState(wda->wdaclass_attrobjects);

  if ( strcmp( hiername, "") == 0)
    wda->objid = pwr_cNObjid;
  else {
    sts = ldh_NameToObjid( wda->ldhses, &wda->objid, hiername);
    if ( EVEN(sts)) {
      CoWowMotif ww(wda->wdaclass_dia);
      ww.DisplayError( "Hierarchy object error", wnav_get_message( sts));
      return;
    }
  }

  sts = ldh_ClassNameToId( wda->ldhses, &new_classid, classname);
  if ( EVEN(sts)) {
    CoWowMotif ww(wda->wdaclass_dia);
    ww.DisplayError( "Class error", wnav_get_message( sts));
    return;
  }

  XtUnmanageChild( wda->wdaclass_dia);

  if ( wda->classid != new_classid) {
    // Enter attribute
    wda->classid = new_classid;
    wda->open_attr_dialog();
  }
  else {
    // Find new attributes
    sts = ((WdaNav *)wda->wdanav)->update( wda->objid, wda->classid,
		wda->attribute, wda->attrobjects);
    if ( EVEN(sts))
      wda->wow->DisplayError( "Spreadsheet error", wnav_get_message( sts));
  }

}

void WdaMotif::class_activate_cancel( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  XtUnmanageChild( wda->wdaclass_dia);
}

void WdaMotif::enable_set_focus( WdaMotif *wda)
{
  wda->set_focus_disabled--;
}

void WdaMotif::disable_set_focus( WdaMotif *wda, int time)
{
  wda->set_focus_disabled++;
  wda->focus_timerid = XtAppAddTimeOut(
	XtWidgetToApplicationContext( wda->toplevel), time,
	(XtTimerCallbackProc)enable_set_focus, wda);
}

void WdaMotif::action_inputfocus( Widget w, XmAnyCallbackStruct *data)
{
  Arg args[1];
  WdaMotif *wda;

  XtSetArg    (args[0], XmNuserData, &wda);
  XtGetValues (w, args, 1);

  if ( !wda)
    return;

  if ( mrm_IsIconicState(w))
    return;

  if ( wda->set_focus_disabled)
    return;

  if ( flow_IsManaged( wda->cmd_scrolledinput))
    flow_SetInputFocus( wda->cmd_scrolledinput);
  else if ( flow_IsManaged( wda->cmd_input))
    flow_SetInputFocus( wda->cmd_input);
  else if ( wda->wdanav)
    ((WdaNav *)wda->wdanav)->set_inputfocus();

  disable_set_focus( wda, 400);

}

void WdaMotif::valchanged_cmd_input( Widget w, XEvent *event)
{
  Wda 	*wda;
  int 	sts;
  char 	*text;
  Arg 	args[2];

  XtSetArg(args[0], XmNuserData, &wda);
  XtGetValues(w, args, 1);

  sts = mrm_TextInput( w, event, (char *)value_recall, sizeof(value_recall[0]),
	sizeof( value_recall)/sizeof(value_recall[0]),
	&((WdaMotif *)wda)->value_current_recall);
  if ( sts)
  {
    text = XmTextGetString( w);
    if ( wda->input_open)
    {
      sts = ((WdaNav *)wda->wdanav)->set_attr_value( wda->input_node, 
		wda->input_name, text);
      XtUnmanageChild( w);
      wda->set_prompt( "");
      wda->input_open = 0;
      if ( wda->redraw_cb)
        (wda->redraw_cb)( wda);

      ((WdaNav *)wda->wdanav)->set_inputfocus();
    }
  }
}

void WdaMotif::change_value_close()
{
  char *text;
  int sts;

  text = XmTextGetString( cmd_scrolledinput);
  if ( input_open) {
    if ( input_multiline) {
      sts = ((WdaNav *)wdanav)->set_attr_value( input_node,
		input_name, text);
      XtUnmanageChild( cmd_scrolledinput);
      XtUnmanageChild( cmd_scrolled_ok);
      XtUnmanageChild( cmd_scrolled_ca);
      set_prompt( "");
      input_open = 0;

      ((WdaNav *)wdanav)->redraw();
      ((WdaNav *)wdanav)->set_inputfocus();
    }
    else {
      text = XmTextGetString( cmd_input);

      sts = ((WdaNav *)wdanav)->set_attr_value( input_node, 
		input_name, text);
      XtUnmanageChild( cmd_input);
      set_prompt( "");
      input_open = 0;
      if ( redraw_cb)
        (redraw_cb)( this);

      ((WdaNav *)wdanav)->set_inputfocus();
    }
  }
}

void WdaMotif::activate_cmd_input( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  char *text;
  int sts;

  text = XmTextGetString( w);
  if ( wda->input_open)
  {
    sts = ((WdaNav *)wda->wdanav)->set_attr_value( wda->input_node, 
		wda->input_name, text);
    XtUnmanageChild( w);
    wda->set_prompt( "");
    wda->input_open = 0;
    if ( wda->redraw_cb)
      (wda->redraw_cb)( wda);

    ((WdaNav *)wda->wdanav)->set_inputfocus();
  }
}

void WdaMotif::activate_cmd_scrolled_ok( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{
  char *text;
  int sts;

  text = XmTextGetString( wda->cmd_scrolledinput);
  if ( wda->input_open)
  {
    sts = ((WdaNav *)wda->wdanav)->set_attr_value( wda->input_node,
		wda->input_name, text);
    XtUnmanageChild( wda->cmd_scrolledinput);
    XtUnmanageChild( wda->cmd_scrolled_ok);
    XtUnmanageChild( wda->cmd_scrolled_ca);
    wda->set_prompt( "");
    wda->input_open = 0;

    ((WdaNav *)wda->wdanav)->redraw();
    ((WdaNav *)wda->wdanav)->set_inputfocus();
  }
}

void WdaMotif::activate_cmd_scrolled_ca( Widget w, WdaMotif *wda, XmAnyCallbackStruct *data)
{

  if ( wda->input_open)
  {
    XtUnmanageChild( wda->cmd_scrolledinput);
    XtUnmanageChild( wda->cmd_scrolled_ok);
    XtUnmanageChild( wda->cmd_scrolled_ca);
    wda->set_prompt( "");
    wda->input_open = 0;
    ((WdaNav *)wda->wdanav)->set_inputfocus();
  }
}

void WdaMotif::pop()
{
  flow_UnmapWidget( parent_wid);
  flow_MapWidget( parent_wid);
}

void WdaMotif::open_class_dialog( char *hierstr, char *classstr)
{
  XmTextSetString( wdaclass_hiervalue, hierstr);
  XmTextSetString( wdaclass_classvalue, classstr);
  XmToggleButtonSetState( wdaclass_attrobjects, 
			(Boolean) attrobjects, False);

  XtManageChild( wdaclass_dia);
}

WdaMotif::~WdaMotif()
{
  if ( set_focus_disabled)
    XtRemoveTimeOut( focus_timerid);
  if ( wow)
    delete wow;

  delete (WdaNav *)wdanav;
  XtDestroyWidget( parent_wid);
}

WdaMotif::WdaMotif( 
	Widget 		wa_parent_wid,
	void 		*wa_parent_ctx, 
	ldh_tSesContext wa_ldhses, 
	pwr_tObjid 	wa_objid,
	pwr_tClassId 	wa_classid,
	char            *wa_attribute,
	int 		wa_editmode,
	int 		wa_advanced_user,
	int		wa_display_objectname) :
  Wda(wa_parent_ctx,wa_ldhses,wa_objid,wa_classid,wa_attribute,wa_editmode,
      wa_advanced_user,wa_display_objectname), 
  parent_wid(wa_parent_wid), set_focus_disabled(0), value_current_recall(0)
{
  char		uid_filename[120] = {"pwr_exe:wb_wda.uid"};
  char		*uid_filename_p = uid_filename;
  Arg 		args[20];
  pwr_tStatus	sts;
  char 		title[80];
  int		i;
  MrmHierarchy s_DRMh;
  MrmType dclass;
  char		name[] = "Proview/R Navigator";

  static char translations[] =
    "<FocusIn>: wda_inputfocus()\n";
  static XtTranslations compiled_translations = NULL;

  static XtActionsRec actions[] =
  {
    {"wda_inputfocus",      (XtActionProc) action_inputfocus}
  };

  static MrmRegisterArg	reglist[] = {
        { "wda_ctx", 0 },
	{"wda_activate_setclass",(caddr_t)activate_setclass },
	{"wda_activate_setattr",(caddr_t)activate_setattr },
	{"wda_activate_nextattr",(caddr_t)activate_nextattr },
	{"wda_activate_prevattr",(caddr_t)activate_prevattr },
	{"wda_activate_exit",(caddr_t)activate_exit },
	{"wda_activate_print",(caddr_t)activate_print },
	{"wda_activate_change_value",(caddr_t)activate_change_value },
	{"wda_activate_close_changeval",(caddr_t)activate_close_changeval },
	{"wda_activate_help",(caddr_t)activate_help },
	{"wda_create_msg_label",(caddr_t)create_msg_label },
	{"wda_create_cmd_prompt",(caddr_t)create_cmd_prompt },
	{"wda_create_cmd_input",(caddr_t)create_cmd_input },
	{"wda_create_cmd_scrolledinput",(caddr_t)create_cmd_scrolledinput },
	{"wda_create_cmd_scrolled_ok",(caddr_t)create_cmd_scrolled_ok },
	{"wda_create_cmd_scrolled_ca",(caddr_t)create_cmd_scrolled_ca },
	{"wda_create_wdanav_form",(caddr_t)create_wdanav_form },
	{"wda_activate_cmd_scrolledinput",(caddr_t)activate_cmd_input },
	{"wda_activate_cmd_scrolled_ok",(caddr_t)activate_cmd_scrolled_ok },
	{"wda_activate_cmd_scrolled_ca",(caddr_t)activate_cmd_scrolled_ca },
	{"wdaclass_activate_ok",(caddr_t)class_activate_ok },
	{"wdaclass_activate_cancel",(caddr_t)class_activate_cancel },
	{"wdaclass_create_hiervalue",(caddr_t)class_create_hiervalue },
	{"wdaclass_create_classvalue",(caddr_t)class_create_classvalue },
	{"wdaclass_create_attrobjects",(caddr_t)class_create_attrobjects }
	};

  static int	reglist_num = (sizeof reglist / sizeof reglist[0]);

  // for ( i = 0; i < int(sizeof(value_recall)/sizeof(value_recall[0])); i++)
  //  value_recall[i][0] = 0;

  dcli_translate_filename( uid_filename, uid_filename);

  // Create object context
//  attrctx->close_cb = close_cb;
//  attrctx->redraw_cb = redraw_cb;

  // Motif
  MrmInitialize();

  strcpy( title, "PwR Object attributes");

  reglist[0].value = (caddr_t) this;

  // Save the context structure in the widget
  XtSetArg (args[0], XmNuserData, (unsigned int) this);

  sts = MrmOpenHierarchy( 1, &uid_filename_p, NULL, &s_DRMh);
  if (sts != MrmSUCCESS) printf("can't open %s\n", uid_filename);

  MrmRegisterNames(reglist, reglist_num);

  parent_wid = XtCreatePopupShell("spreadSheetEditor", 
		topLevelShellWidgetClass, parent_wid, args, 0);

  sts = MrmFetchWidgetOverride( s_DRMh, "wda_window", parent_wid,
			name, args, 1, &toplevel, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch %s\n", name);

  sts = MrmFetchWidget(s_DRMh, "wdaclass_dia", parent_wid,
		&wdaclass_dia, &dclass);
  if (sts != MrmSUCCESS)  printf("can't fetch class input dialog\n");

  MrmCloseHierarchy(s_DRMh);


  if (compiled_translations == NULL) 
    XtAppAddActions( XtWidgetToApplicationContext(toplevel), 
						actions, XtNumber(actions));
 
  if (compiled_translations == NULL) 
    compiled_translations = XtParseTranslationTable(translations);
  XtOverrideTranslations( toplevel, compiled_translations);

  i = 0;
  XtSetArg(args[i],XmNwidth,800);i++;
  XtSetArg(args[i],XmNheight,600);i++;
  XtSetValues( toplevel ,args,i);
    
  XtManageChild( toplevel);
  XtUnmanageChild( cmd_input);
  XtUnmanageChild( cmd_scrolledinput);
  XtUnmanageChild( cmd_scrolled_ok);
  XtUnmanageChild( cmd_scrolled_ca);

  utility = ((WUtility *)parent_ctx)->utype;
  wdanav = new WdaNavMotif( (void *)this, wdanav_form, "Plant",
		ldhses, objid, classid, attribute, wa_editmode,
		wa_advanced_user,
		wa_display_objectname, utility, &brow_widget, &sts);
  ((WdaNav *)wdanav)->message_cb = &Wda::message_cb;
  ((WdaNav *)wdanav)->change_value_cb = &Wda::change_value_cb;

  XtPopup( parent_wid, XtGrabNone);

  wow = new CoWowMotif( parent_wid);

  // Connect the window manager close-button to exit
  flow_AddCloseVMProtocolCb( parent_wid, 
	(XtCallbackProc)activate_exit, this);

  if ( utility == wb_eUtility_Wtt)
  {
    ((Wtt *)parent_ctx)->register_utility( (void *) this,
	wb_eUtility_SpreadsheetEditor);
  }
}
