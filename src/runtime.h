/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * runtime.h: Core surface and canvas definitions.
 *
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include <glib.h>

G_BEGIN_DECLS

#include <stdint.h>
#include <cairo.h>
#include <gtk/gtkwidget.h>

#include "point.h"
#include "uielement.h"
#include "dependencyobject.h"
#include "dirty.h"
#include "value.h"
#include "type.h"
#include "list.h"
#include "error.h"

#define MAXIMUM_CACHE_SIZE 6000000

#define FRONT_TO_BACK_STATS 0

#define TIMERS 0
#define DEBUG_MARKER_KEY 0
#if TIMERS
#define STARTTIMER(id,str) TimeSpan id##_t_start = get_now(); printf ("timing of '%s' started at %lld\n", str, id##_t_start)
#define ENDTIMER(id,str) TimeSpan id##_t_end = get_now(); printf ("timing of '%s' ended at %lld (%f seconds)\n", str, id##_t_end, (double)(id##_t_end - id##_t_start) / 10000000)
#else
#define STARTTIMER(id,str)
#define ENDTIMER(id,str)
#endif

enum RuntimeInitFlags {
	RUNTIME_INIT_PANGO_TEXT_LAYOUT     = 1 << 0,
	RUNTIME_INIT_MICROSOFT_CODECS      = 1 << 1,
	RUNTIME_INIT_MANUAL_TIMESOURCE     = 1 << 2,
	RUNTIME_INIT_DISABLE_AUDIO         = 1 << 3,
	RUNTIME_INIT_SHOW_EXPOSE           = 1 << 4,
	RUNTIME_INIT_SHOW_CLIPPING         = 1 << 5,
	RUNTIME_INIT_SHOW_BOUNDING_BOXES   = 1 << 6,
	RUNTIME_INIT_SHOW_TEXTBOXES        = 1 << 7,
	RUNTIME_INIT_SHOW_FPS              = 1 << 8,
	RUNTIME_INIT_RENDER_FRONT_TO_BACK  = 1 << 9,
	RUNTIME_INIT_SHOW_CACHE_SIZE	   = 1 << 10,
	RUNTIME_INIT_FFMPEG_YUV_CONVERTER  = 1 << 11,
	RUNTIME_INIT_USE_SHAPE_CACHE	   = 1 << 12,
	RUNTIME_INIT_USE_UPDATE_POSITION   = 1 << 13,
	RUNTIME_INIT_ALLOW_WINDOWLESS      = 1 << 14,
	RUNTIME_INIT_AUDIO_NO_MMAP         = 1 << 15,
};

#define RUNTIME_INIT_DESKTOP (RUNTIME_INIT_PANGO_TEXT_LAYOUT | RUNTIME_INIT_RENDER_FRONT_TO_BACK | RUNTIME_INIT_USE_UPDATE_POSITION | RUNTIME_INIT_USE_SHAPE_CACHE)
#define RUNTIME_INIT_BROWSER (RUNTIME_INIT_MICROSOFT_CODECS | RUNTIME_INIT_RENDER_FRONT_TO_BACK | RUNTIME_INIT_USE_UPDATE_POSITION | RUNTIME_INIT_USE_SHAPE_CACHE | RUNTIME_INIT_ALLOW_WINDOWLESS)

extern guint32 moonlight_flags;

class TimeManager;
class Surface;
class Downloader;

typedef void (* MoonlightFPSReportFunc) (Surface *surface, int nframes, float nsecs, void *user_data);
typedef void (* MoonlightCacheReportFunc) (Surface *surface, long size, void *user_data);
typedef bool (* MoonlightEventEmitFunc) (UIElement *element, GdkEvent *event);

class Surface : public EventObject {
	// are we headed for death?
	bool zombie;
	
	// bad, but these two live in dirty.cpp, not runtime.cpp
	void ProcessDownDirtyElements ();
	void ProcessUpDirtyElements ();
	
	List *down_dirty;
	List *up_dirty;
	
	gpointer downloader_context;
	List *downloaders;
	void DetachDownloaders ();
	static void OnDownloaderDestroyed (EventObject *sender, EventArgs *args, gpointer closure);
	
	int normal_width, normal_height;
	// the actual size of the drawing area, 
	// screen size in fullscreen mode,
	// otherwise normal size.
	int width, height;
	
	bool transparent;
	Color *background_color;
	
	// This is the normal widget
	GtkWidget *widget_normal;
	
	// We set widget to this whenever we are in fullscreen mode.
	GtkWidget *widget_fullscreen;
	
	// This currently can only be a canvas.
	UIElement *toplevel;
	
	// the list of elements (from most deeply nested to the
	// toplevel) we've most recently sent a mouse event to.
	List *input_list;
	
	// is the mouse captured?  if it is, it'll be by the first element in input_list.
	bool captured;
	UIElement *pendingCapture;
	bool pendingReleaseCapture;
	
	// are we currently emitting a mouse event?
	bool emittingMouseEvent;
	
	// the currently shown cursor
	MouseCursor cursor;
	
	// Fullscreen support
	bool full_screen;
	Canvas *full_screen_message;
	char *source_location;
	// Should be set to true only while executing MouseLeftButtonDown, 
	// MouseLeftButtonUp, KeyDown, and KeyUp event handlers
	bool can_full_screen; 
	
	void UpdateFullScreen (bool value);
	
	TimeManager *time_manager;
	
	int frames;
	
	GdkEvent *mouse_event;
	
	// Variables for reporting FPS
	MoonlightFPSReportFunc fps_report;
	gint64 fps_start;
	int fps_nframes;
	void *fps_data;
	
	// Variables for reporting cache size
	MoonlightCacheReportFunc cache_report;
	gint64 cache_size_in_bytes;
	int cache_size_ticker;
	void *cache_data;
	int cache_size_multiplier;
	
	void ConnectEvents (bool realization_signals);
	void Realloc ();
	void InitializeWidget (GtkWidget *widget);
	void ShowFullScreenMessage ();
	void HideFullScreenMessage ();
	
	void CreateSimilarSurface ();
	
	static Key gdk_keyval_to_key (guint keyval);
	
	static void render_cb (EventObject *sender, EventArgs *calldata, gpointer closure);
	static void update_input_cb (EventObject *sender, EventArgs *calldata, gpointer closure);
	static void widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
	static void widget_destroyed (GtkWidget *w, gpointer data);
	
	void FindFirstCommonElement (List *l1, int *index1, List *l2, int *index2);
	bool EmitEventOnList (int event_id, List *element_list, GdkEvent *event, int end_idx);
	void UpdateCursorFromInputList ();
	bool HandleMouseEvent (int event_id, bool emit_leave, bool emit_enter, bool force_emit, GdkEvent *event);
	void PerformCapture (UIElement *capture);
	void PerformReleaseCapture ();
	
 protected:
	// The current widget we are drawing to. If in windowless
	// mode, widget will be NULL - otherwise it will either point
	// to widget_normal or widget_fullscreen depending on whether
	// or not we are in fullscreen mode.
	GtkWidget *widget;
	
	virtual ~Surface();

 public:
	// if we're windowed, @d will be NULL.
	Surface (int width, int height, bool windowless = false);

	// allows you to redirect painting of the surface to an
	// arbitrary cairo context.
	void Paint (cairo_t *ctx, int x, int y, int width, int height);
	void Paint (cairo_t *ctx, Region *);

	void Attach (UIElement *toplevel);

	virtual void SetCursor (GdkCursor *cursor);
	void SetCursor (MouseCursor cursor);

	bool SetMouseCapture (UIElement *capture);

	void Resize (int width, int height);
	int GetWidth () { return width; }
	int GetHeight () { return height; }

	void EmitError (ErrorEventArgs *args);
	void EmitLoad ();

	void SetTrans (bool trans);
	bool GetTrans () { return transparent; }

	void SetBackgroundColor (Color *color);

	int GetFrameCount () { return frames; }
	void ResetFrameCount () { frames = 0; }

	virtual void Invalidate (Rect r);
	virtual void ProcessUpdates ();

	GtkWidget *GetWidget () { return widget; }
	UIElement *GetToplevel() { return toplevel; }
	bool IsTopLevel (UIElement *top);

	bool IsLoaded () { return toplevel != NULL; }

	const static int ResizeEvent;
	const static int FullScreenChangeEvent;
	const static int ErrorEvent;
	const static int LoadEvent;

	bool GetFullScreen () { return full_screen; }
	void SetFullScreen (bool value);
	void SetCanFullScreen (bool value) { can_full_screen = value; }
	void SetSourceLocation (const char *location);
	bool FullScreenKeyHandled (GdkEventKey *key);
	int GetActualWidth () { return width; }
	int GetActualHeight () { return height; }

	TimeManager *GetTimeManager () { return time_manager; }

	virtual Type::Kind GetObjectType () { return Type::SURFACE; };
	
	void SetDownloaderContext (gpointer context) { downloader_context = context; }
	gpointer GetDownloaderContext () { return downloader_context; }
	
	Downloader *CreateDownloader ();
	static Downloader *CreateDownloader (UIElement *element);

	void SetFPSReportFunc (MoonlightFPSReportFunc report, void *user_data);
	void SetCacheReportFunc (MoonlightCacheReportFunc report, void *user_data);

	bool VerifyWithCacheSizeCounter (int w, int h);
	gint64 AddToCacheSizeCounter (int w, int h);
	void RemoveFromCacheSizeCounter (gint64 size);

	// called from the plugin if the surface is headed for death.
	// stops event emission (since the plugin counterparts to xaml
	// objects will be destroyed)
	void Zombify ();
	
#if FRONT_TO_BACK_STATS
	int uielements_rendered_front_to_back;
	int uielements_rendered_back_to_front;
#endif

#ifdef DEBUG
	UIElement *debug_selected_element;
#endif

	gboolean expose_to_drawable (GdkDrawable *drawable, GdkVisual *visual, GdkEventExpose *event, int off_x, int off_y);

	// widget callbacks
	static gboolean expose_event_callback (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
	static gboolean motion_notify_callback (GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
	static gboolean crossing_notify_callback (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
	static gboolean key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
	static gboolean key_release_callback (GtkWidget *widget, GdkEventKey *event, gpointer user_data);
	static gboolean button_release_callback (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
	static gboolean button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
	static gboolean focus_in_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
	static gboolean focus_out_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
	static gboolean realized_callback (GtkWidget *widget, gpointer user_data);
	static gboolean unrealized_callback (GtkWidget *widget, gpointer user_data);
	

	// bad, but these live in dirty.cpp, not runtime.cpp
	void AddDirtyElement (UIElement *element, DirtyType dirt);
	void RemoveDirtyElement (UIElement *element);
	void ProcessDirtyElements ();
	void PropagateDirtyFlagToChildren (UIElement *element, DirtyType dirt);
	bool IsAnythingDirty ();

	static pthread_t main_thread;
	static bool InMainThread () { return pthread_equal (main_thread, pthread_self ()); }
};

/* for hit testing */
class UIElementNode : public List::Node {
public:
	UIElement *uielement;
		
	UIElementNode (UIElement *el);
	virtual ~UIElementNode ();
};

/* for rendering */
typedef void (*RenderFunc) (cairo_t *ctx, UIElement *uielement, Region *region, bool front_to_back);

class RenderNode : public List::Node {
 public:
	UIElement *uielement;
	Region *region;
	bool render_element;
	RenderFunc pre_render;
	RenderFunc post_render;

	RenderNode (UIElement *el, Region *region, bool render_element, RenderFunc pre, RenderFunc post);

	virtual ~RenderNode ();
};


Surface *surface_new       (int width, int height);
void     surface_resize    (Surface *s, int width, int height);
void     surface_attach    (Surface *s, UIElement *element);
void     surface_init      (Surface *s, int width, int height);
void     surface_destroy   (Surface *s);
void     surface_set_trans (Surface *s, bool trans);
bool     surface_get_trans (Surface *s);
void     surface_paint     (Surface *s, cairo_t *ctx, int x, int y, int width, int height);

void	 *surface_get_widget (Surface *s);
TimeManager* surface_get_time_manager (Surface* s);
Downloader* surface_create_downloader (Surface *s);

void runtime_init (guint32 flags);

guint32  runtime_html_timer_timeout_add (gint32 interval, GSourceFunc callback, gpointer data);
void     runtime_html_timer_timeout_stop (guint32 source_id);

void runtime_shutdown (void);


G_END_DECLS

#endif
