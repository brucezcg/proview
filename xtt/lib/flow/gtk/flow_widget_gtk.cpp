/* 
 * Proview   $Id: flow_widget_gtk.cpp,v 1.2 2007-01-11 11:40:30 claes Exp $
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

#include <stdlib.h>
#include "glow_std.h"

#include "flow.h"
#include "flow_ctx.h"
#include "flow_ctx.h"
#include "flow_draw.h"
#include "flow_draw_gtk.h"
#include "flow_widget_gtk.h"

typedef struct _FlowWidgetGtk		FlowWidgetGtk;
typedef struct _FlowWidgetGtkClass	FlowWidgetGtkClass;

typedef struct {
	GtkWidget  	*flow;
	GtkWidget	*form;
	GtkWidget	*scroll_h;
	GtkWidget	*scroll_v;
	int		scroll_h_managed;
	int		scroll_v_managed;
	} flowwidget_sScroll;

struct _FlowWidgetGtk {
  GtkDrawingArea parent;

  /* Private */
  void		*flow_ctx;
  void 		*draw_ctx;
  int		(*init_proc)(FlowCtx *ctx, void *clien_data);
  int		is_navigator;
  void		*client_data;
  GtkWidget	*main_flow_widget;
  GtkWidget    	*scroll_h;
  GtkWidget    	*scroll_v;
  GtkWidget    	*form;
  int		scroll_h_ignore;
  int		scroll_v_ignore;
};

struct _FlowWidgetGtkClass {
  GtkDrawingAreaClass parent_class;
};

G_DEFINE_TYPE( FlowWidgetGtk, flowwidgetgtk, GTK_TYPE_DRAWING_AREA);

static void scroll_callback( flow_sScroll *data)
{
  flowwidget_sScroll *scroll_data;

  scroll_data = (flowwidget_sScroll *) data->scroll_data;

  if ( data->total_width <= data->window_width) {
    if ( data->offset_x == 0)
      data->total_width = data->window_width;
    if ( scroll_data->scroll_h_managed) {
      // Remove horizontal scrollbar
    }
  }
  else {
    if ( !scroll_data->scroll_h_managed) {
      // Insert horizontal scrollbar
    }
  }

  if ( data->total_height <= data->window_height) {
    if ( data->offset_y == 0)
      data->total_height = data->window_height;
    if ( scroll_data->scroll_v_managed) {
      // Remove vertical scrollbar
    }
  }
  else {
    if ( !scroll_data->scroll_v_managed) {
      // Insert vertical scrollbar
    }
  }
  if ( data->offset_x < 0) {
    data->total_width += -data->offset_x;
    data->offset_x = 0;
  }
  if ( data->offset_y < 0) {
    data->total_height += -data->offset_y;
    data->offset_y = 0;
  }
  if ( data->total_height < data->window_height + data->offset_y)
    data->total_height = data->window_height + data->offset_y;
  if ( data->total_width < data->window_width + data->offset_x)
    data->total_width = data->window_width + data->offset_x;
  if ( data->window_width < 1)
    data->window_width = 1;
  if ( data->window_height < 1)
    data->window_height = 1;

  if ( scroll_data->scroll_h_managed) {
    ((FlowWidgetGtk *)scroll_data->flow)->scroll_h_ignore = 1;
    g_object_set( ((GtkScrollbar *)scroll_data->scroll_h)->range.adjustment,
		 "upper", (gdouble)data->total_width,
		 "page-size", (gdouble)data->window_width,
		 "value", (gdouble)data->offset_x,
		 NULL);
    gtk_adjustment_changed( 
        ((GtkScrollbar *)scroll_data->scroll_h)->range.adjustment);
  }

  if ( scroll_data->scroll_v_managed) {
    ((FlowWidgetGtk *)scroll_data->flow)->scroll_v_ignore = 1;
    g_object_set( ((GtkScrollbar *)scroll_data->scroll_v)->range.adjustment,
		 "upper", (gdouble)data->total_height,
		 "page-size", (gdouble)data->window_height,
		 "value", (gdouble)data->offset_y,
		 NULL);
    gtk_adjustment_changed( 
        ((GtkScrollbar *)scroll_data->scroll_v)->range.adjustment);
  }
}

static void scroll_h_action( 	GtkWidget      	*w,
				gpointer 	data)
{
  FlowWidgetGtk *floww = (FlowWidgetGtk *)data;
  if ( floww->scroll_h_ignore) {
    floww->scroll_h_ignore = 0;
    return;
  }

  FlowCtx *ctx = (FlowCtx *) floww->flow_ctx;
  gdouble value;
  g_object_get( w,
		"value", &value,
		NULL);
  flow_scroll_horizontal( ctx, int(value), 0);

}

static void scroll_v_action( 	GtkWidget 	*w,
				gpointer 	data)
{
  FlowWidgetGtk *floww = (FlowWidgetGtk *)data;

  if ( floww->scroll_v_ignore) {
    floww->scroll_v_ignore = 0;
    return;
  }
    
  FlowCtx *ctx = (FlowCtx *) floww->flow_ctx;
  gdouble value;
  g_object_get( w,
		"value", &value,
		NULL);
  flow_scroll_vertical( ctx, int(value), 0);
}

static int flow_init_proc( GtkWidget *w, FlowCtx *fctx, void *client_data)
{
  flowwidget_sScroll *scroll_data;
  FlowCtx	*ctx;

  ctx = (FlowCtx *) ((FlowWidgetGtk *) w)->flow_ctx;

  if ( ((FlowWidgetGtk *) w)->scroll_h) {
    scroll_data = (flowwidget_sScroll *) malloc( sizeof( flowwidget_sScroll));
    scroll_data->flow = w;
    scroll_data->scroll_h = ((FlowWidgetGtk *) w)->scroll_h;
    scroll_data->scroll_v = ((FlowWidgetGtk *) w)->scroll_v;
    scroll_data->form = ((FlowWidgetGtk *) w)->form;
    scroll_data->scroll_h_managed = 1;
    scroll_data->scroll_v_managed = 1;

    ctx->register_scroll_callback( (void *) scroll_data, scroll_callback);
  }
  return (((FlowWidgetGtk *) w)->init_proc)( ctx, client_data);
}

static gboolean flowwidgetgtk_expose( GtkWidget *flow, GdkEventExpose *event)
{
  ((FlowDrawGtk *)((FlowCtx *)((FlowWidgetGtk *)flow)->flow_ctx)->fdraw)->event_handler( 										(FlowCtx *)((FlowWidgetGtk *)flow)->flow_ctx, *(GdkEvent *)event);
  return TRUE;
}

static gboolean flowwidgetgtk_event( GtkWidget *flow, GdkEvent *event)
{
  if ( event->type == GDK_MOTION_NOTIFY) {
    gdk_display_flush( ((FlowDrawGtk *)((FlowCtx *)((FlowWidgetGtk *)flow)->flow_ctx)->fdraw)->display);
    GdkEvent *next = gdk_event_peek();
    if ( next && next->type == GDK_MOTION_NOTIFY) {
      gdk_event_free( next);
      return TRUE;
    }
    else if ( next)
      gdk_event_free( next);
  }

  ((FlowDrawGtk *)((FlowCtx *)((FlowWidgetGtk *)flow)->flow_ctx)->fdraw)->event_handler( (FlowCtx *)((FlowWidgetGtk *)flow)->flow_ctx, *event);
  return TRUE;
}

static void flowwidgetgtk_realize( GtkWidget *widget)
{
  GdkWindowAttr attr;
  gint attr_mask;
  FlowWidgetGtk *flow;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (IS_FLOWWIDGETGTK( widget));

  GTK_WIDGET_SET_FLAGS( widget, GTK_REALIZED);
  flow = FLOWWIDGETGTK( widget);

  attr.x = widget->allocation.x;
  attr.y = widget->allocation.y;
  attr.width = widget->allocation.width;
  attr.height = widget->allocation.height;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.window_type = GDK_WINDOW_CHILD;
  attr.event_mask = gtk_widget_get_events( widget) |
    GDK_EXPOSURE_MASK | 
    GDK_BUTTON_PRESS_MASK | 
    GDK_BUTTON_RELEASE_MASK | 
    GDK_KEY_PRESS_MASK |
    GDK_POINTER_MOTION_MASK |
    GDK_BUTTON_MOTION_MASK |
    GDK_ENTER_NOTIFY_MASK |
    GDK_LEAVE_NOTIFY_MASK;
  attr.visual = gtk_widget_get_visual( widget);
  attr.colormap = gtk_widget_get_colormap( widget);

  attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new( widget->parent->window, &attr, attr_mask);
  widget->style = gtk_style_attach( widget->style, widget->window);
  gdk_window_set_user_data( widget->window, widget);
  gtk_style_set_background( widget->style, widget->window, GTK_STATE_ACTIVE);

  GTK_WIDGET_SET_FLAGS( widget, GTK_CAN_FOCUS);

  if ( flow->is_navigator) {
    if ( !flow->flow_ctx) {
      FlowWidgetGtk *main_flow = (FlowWidgetGtk *) flow->main_flow_widget;

      flow->flow_ctx = main_flow->flow_ctx;
      flow->draw_ctx = main_flow->draw_ctx;
      ((FlowDrawGtk *)flow->draw_ctx)->init_nav( widget, flow->flow_ctx);
    }
  }
  else {
    if ( !flow->flow_ctx) {
      flow->draw_ctx = new FlowDrawGtk( widget, 
					&flow->flow_ctx, 
					flow_init_proc, 
					flow->client_data,
					flow_eCtxType_Flow);
    }
  }

}

static void flowwidgetgtk_class_init( FlowWidgetGtkClass *klass)
{
  GtkWidgetClass *widget_class;
  widget_class = GTK_WIDGET_CLASS( klass);
  widget_class->realize = flowwidgetgtk_realize;
  widget_class->expose_event = flowwidgetgtk_expose;
  widget_class->event = flowwidgetgtk_event;
}

static void flowwidgetgtk_init( FlowWidgetGtk *flow)
{
}

GtkWidget *flowwidgetgtk_new(
        int (*init_proc)(FlowCtx *ctx, void *client_data),
	void *client_data)
{
  FlowWidgetGtk *w;
  w =  (FlowWidgetGtk *) g_object_new( FLOWWIDGETGTK_TYPE, NULL);
  w->init_proc = init_proc;
  w->flow_ctx = 0;
  w->is_navigator = 0;
  w->client_data = client_data;
  w->scroll_h = 0;
  w->scroll_v = 0;
  return (GtkWidget *) w;  
}

GtkWidget *scrolledflowwidgetgtk_new(
        int (*init_proc)(FlowCtx *ctx, void *client_data),
	void *client_data, GtkWidget **flowwidget)
{
  FlowWidgetGtk *w;

  GtkWidget *form = gtk_scrolled_window_new( NULL, NULL);

  w =  (FlowWidgetGtk *) g_object_new( FLOWWIDGETGTK_TYPE, NULL);
  w->init_proc = init_proc;
  w->flow_ctx = 0;
  w->is_navigator = 0;
  w->client_data = client_data;
  w->scroll_h = gtk_scrolled_window_get_hscrollbar( GTK_SCROLLED_WINDOW(form));
  w->scroll_v = gtk_scrolled_window_get_vscrollbar( GTK_SCROLLED_WINDOW(form));
  w->scroll_h_ignore = 0;
  w->scroll_v_ignore = 0;
  w->form = form;
  *flowwidget = GTK_WIDGET( w);

  g_signal_connect( ((GtkScrollbar *)w->scroll_h)->range.adjustment, 
		    "value-changed", G_CALLBACK(scroll_h_action), w);
  g_signal_connect( ((GtkScrollbar *)w->scroll_v)->range.adjustment, 
		    "value-changed", G_CALLBACK(scroll_v_action), w);

  GtkWidget *viewport = gtk_viewport_new( NULL, NULL);
  gtk_container_add( GTK_CONTAINER(viewport), GTK_WIDGET(w));
  gtk_container_add( GTK_CONTAINER(form), GTK_WIDGET(viewport));

  return (GtkWidget *) form;  
}

GtkWidget *flownavwidgetgtk_new( GtkWidget *main_flow)
{
  FlowWidgetGtk *w;
  w =  (FlowWidgetGtk *) g_object_new( FLOWWIDGETGTK_TYPE, NULL);
  w->init_proc = 0;
  w->flow_ctx = 0;
  w->is_navigator = 1;
  w->main_flow_widget = main_flow;
  w->client_data = 0;
  w->scroll_h = 0;
  w->scroll_v = 0;
  w->scroll_h_ignore = 0;
  w->scroll_v_ignore = 0;
  return (GtkWidget *) w;  
}

#if 0
GType flowwidgetgtk_get_type(void)
{
  static GType flowwidgetgtk_type = 0;

  if ( !flowwidgetgtk_type) {
    static const GTypeInfo flowwidgetgtk_info = {
      sizeof(FlowWidgetGtkClass), NULL, NULL, (GClassInitFunc)flowwidgetgtk_class_init,
      NULL, NULL, sizeof(FlowWidgetGtk), 1, NULL, NULL};
    
    flowwidgetgtk_type = g_type_register_static( G_TYPE_OBJECT, "FlowWidgetGtk", &flowwidgetgtk_info, 
					   (GTypeFlags)0);  
  }
  return flowwidgetgtk_type;
}

void flowwidgetgtk_get_ctx( GtkWidget *w, void **ctx)
{
  *ctx = ((FlowWidgetGtk *)w)->flow_ctx;
}
#endif

void flowwidgetgtk_modify_ctx( GtkWidget *w, void *ctx)
{
  ((FlowWidgetGtk *)w)->flow_ctx = ctx;
}
