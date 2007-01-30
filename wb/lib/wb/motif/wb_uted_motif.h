/* 
 * Proview   $Id: wb_uted_motif.h,v 1.1 2007-01-04 07:29:02 claes Exp $
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

#ifndef wb_uted_motif_h
#define wb_uted_motif_h

#ifndef wb_uted_h
#include "wb_uted.h"
#endif

struct uted_s_widgets 
{
  Widget	uted_window;
  Widget	label;
  Widget        adb;
  Widget	file_entry;
  Widget	quit;
  Widget	batchoptmenu;
  Widget	commandlabel;
  Widget	batch;
  Widget	currsess;
  Widget	help;
  Widget	timelabel;
  Widget	timevalue;
  Widget	qualifier[UTED_QUALS];
  Widget	value[UTED_QUALS];
  Widget	present[UTED_QUALS];
  Widget	optmenubuttons[UTED_MAX_COMMANDS];
  Widget	command_window;
  Widget	questionbox;
  Widget	commandwind_button;
};

class WUtedMotif : public WUted {
 public:
  Widget		parent_wid;
  struct		uted_s_widgets widgets ;
  Cursor		cursor;

  WUtedMotif( void	       	*wu_parent_ctx,
	      Widget		wu_parent_wid,
	      char	       	*wu_name,
	      char	       	*wu_iconname,
	      ldh_tWBContext	wu_ldhwb,
	      ldh_tSesContext wu_ldhses,
	      int	       	wu_editmode,
	      void 	       	(*wu_quit_cb)(void *),
	      pwr_tStatus  	*status);
  ~WUtedMotif();
  void remove_command_window();
  void reset_qual();
  void message( char *new_label);
  void set_command_window( char *cmd);
  void raise_window();
  void clock_cursor();
  void reset_cursor();
  void configure_quals( char *label);
  void enable_entries( int enable);
  void get_value( int idx, char *str, int len);
  void questionbox( char 	*question_title,
		    char	*question_text,
		    void	(* yes_procedure) (WUted *),
		    void	(* no_procedure) (WUted *),
		    void	(* cancel_procedure) (WUted *), 
		    pwr_tBoolean cancel);

  static void GetCSText( XmString ar_value, char *t_buffer);

  static void activate_command( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_command( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_helputils( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_helppwr_plc( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_batch( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_currsess( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_quit( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_ok( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_cancel( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_show_cmd( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void activate_cmd_wind( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void commandchanged( Widget w, WUted *utedctx, XmCommandCallbackStruct *data);
  static void activate_present1( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present2( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present3( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present4( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present5( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present6( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present7( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present8( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present9( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void activate_present10( Widget w, WUted *utedctx, XmToggleButtonCallbackStruct *data);
  static void create_label( Widget w, WUted *utedctx, XmAnyCallbackStruct *data); 
  static void create_adb( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_file_entry( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_quit( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_commandwind_button( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_batchoptmenu( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_commandlabel( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_batch( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_currsess( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_cmd_wind( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_timelabel( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_timevalue( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier1( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value1( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present1( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier2( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value2( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present2( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier3( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value3( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present3( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier4( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value4( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present4( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier5( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value5( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present5( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier6( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value6( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present6( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier7( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value7( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present7( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier8( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value8( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present8( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier9( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value9( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present9( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_qualifier10( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_value10( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void create_present10( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void qbox_cr( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void qbox_yes_cb( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void qbox_no_cb( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
  static void qbox_cancel_cb( Widget w, WUted *utedctx, XmAnyCallbackStruct *data);
};

#endif