#include <gtk/gtk.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <math.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "color_detect.h"
#include "gui.h"
#include "preferences.h"
#include "preferences_dialog.h"
#include "screengrab.h"
#include "util.h"

#include "basic.txt.hex"
#include "full.txt.hex"

#define DEBUG 0
#define MAX_CONTEXT_SIZE 100

int basic_color_list_length = 0;
int full_color_list_length  = 0;

int USEC_PER_FRAME;

int cur_context_display_size = 150;
int cur_color_display_size   = 150;
int cur_context_size         = 100;

static Window root;
static cairo_surface_t* rgb_surface;
static cairo_surface_t* context_surface;

int LAST_MOUSE_X = 0;
int LAST_MOUSE_Y = 0;

Display* d;
Screen* screen;
GMainContext* context;

GtkWidget* main_window;
GtkWidget* color_name_label;
GtkWidget* main_color_name_label;
GtkWidget* rgb_label;
GtkWidget* hex_label;
GtkWidget* hsv_label;
GtkWidget* hsl_label;
GtkWidget* color_drawing_area;
GtkWidget* context_drawing_area;
preferences app_preferences;

GThread* update_thread;
GThread* grab_thread;

GMutex context_mutex;
GCond grab_cond;
GCond draw_cond;

int running      = 1;
int need_context = 1;
GMutex running_mutex;

color* front_context_buffer;
color* back_context_buffer;
color* basic_color_list;
color* full_color_list;

struct timeval last_update;

void refresh_preferences() {
    app_preferences.rgb_display ? gtk_widget_show( rgb_label ) : gtk_widget_hide( rgb_label );

    app_preferences.hex_display ? gtk_widget_show( hex_label ) : gtk_widget_hide( hex_label );

    app_preferences.hsv_display ? gtk_widget_show( hsv_label ) : gtk_widget_hide( hsv_label );

    app_preferences.hsl_display ? gtk_widget_show( hsl_label ) : gtk_widget_hide( hsl_label );

    app_preferences.name_display ? gtk_widget_show( color_name_label ) : gtk_widget_hide( color_name_label );

    app_preferences.main_name_display ? gtk_widget_show( main_color_name_label ) : gtk_widget_hide( main_color_name_label );

    gtk_window_set_decorated( (GtkWindow*)main_window, app_preferences.title_bar );

    if( cur_context_size != app_preferences.zoom_level ) {
        cur_context_size = app_preferences.zoom_level;
    }

    USEC_PER_FRAME = pow( app_preferences.frames_per_second, -1 ) * 1000000;

    clear_surface( rgb_surface );
    clear_surface( context_surface );
}

static gboolean on_preferences_closed( GtkWidget* widget, GdkEvent* event, gpointer user_data ) {
    preferences_write( &app_preferences );
    refresh_preferences();

    return FALSE;
}

static void on_preferences( GtkWidget* menu_item, gpointer userdata ) {
    show_preferences_dialog( (GtkWindow*)main_window, &app_preferences, on_preferences_closed );
}

static void on_quit( GtkWidget* menu_item, gpointer userdata ) {
    // gtk_window_close((GtkWindow*)&root);
    exit( 0 );
}

static void clear_surface( cairo_surface_t* surface ) {
    cairo_t* cr;

    cr = cairo_create( surface );

    cairo_set_source_rgb( cr, 1, 1, 1 );
    cairo_paint( cr );

    cairo_destroy( cr );
}

static void view_popup_menu( GtkWidget* widget, GdkEventButton* event, gpointer userdata ) {
    LOG_TID();
    GtkWidget* menu       = gtk_menu_new();
    GtkWidget* menuitem   = gtk_menu_item_new_with_label( "Preferences..." );
    GtkWidget* close_item = gtk_menu_item_new_with_label( "Close" );

    g_signal_connect( menuitem, "activate", (GCallback)on_preferences, NULL );
    g_signal_connect( close_item, "activate", (GCallback)on_quit, NULL );

    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), close_item );
    gtk_widget_show_all( menu );
    gtk_menu_popup_at_pointer( GTK_MENU( menu ), (const GdkEvent*)event );
}

static gboolean on_window_clicked( GtkWidget* widget, GdkEventButton* event, gpointer userdata ) {
    if( event->type == GDK_BUTTON_PRESS && event->button == 3 ) {
        view_popup_menu( widget, NULL, userdata );
    }

    if( event->type == GDK_BUTTON_PRESS && event->button == 1 ) {
        gtk_window_begin_move_drag( GTK_WINDOW( widget ), event->button, event->x_root, event->y_root, event->time );
    }

    return TRUE;
}

static gboolean on_key_pressed( GtkWidget* widget, GdkEventKey* event, gpointer userdata ) {
    if( event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Escape ) {
        g_mutex_lock( &running_mutex );
        running = 0;
        g_mutex_unlock( &running_mutex );
        gtk_widget_destroy( widget );
        return FALSE;
    }
    return TRUE;
}

static gboolean configure_event_cb_rgb( GtkWidget* widget, GdkEventConfigure* event, gpointer data ) {
    if( rgb_surface ) cairo_surface_destroy( rgb_surface );

    rgb_surface = gdk_window_create_similar_surface(
        gtk_widget_get_window( widget ), CAIRO_CONTENT_COLOR, gtk_widget_get_allocated_width( widget ), cur_color_display_size );

    clear_surface( rgb_surface );
    return TRUE;
}

static gboolean configure_event_cb_context( GtkWidget* widget, GdkEventConfigure* event, gpointer data ) {
    if( context_surface ) cairo_surface_destroy( context_surface );

    context_surface = gdk_window_create_similar_surface(
        gtk_widget_get_window( widget ), CAIRO_CONTENT_COLOR, gtk_widget_get_allocated_width( widget ), cur_context_display_size );

    clear_surface( context_surface );
    return TRUE;
}

/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
static gboolean draw_cb_rgb( GtkWidget* widget, cairo_t* cr, gpointer data ) {
    cairo_set_source_surface( cr, rgb_surface, 0, 0 );
    cairo_paint( cr );

    return FALSE;
}

static gboolean draw_cb_context( GtkWidget* widget, cairo_t* cr, gpointer data ) {
    cairo_set_source_surface( cr, context_surface, 0, 0 );
    cairo_paint( cr );

    return FALSE;
}

static void draw_rect( int r, int g, int b ) {
    cairo_t* cr;
    /* Paint to the surface, where we store our state */
    cr = cairo_create( rgb_surface );
    cairo_set_source_rgb( cr, r / 255.0, g / 255.0, b / 255.0 );

    cairo_rectangle( cr, 0, 0, cur_color_display_size, cur_color_display_size );
    cairo_fill( cr );

    cairo_destroy( cr );

    /* Now invalidate the affected region of the drawing area. */
    gtk_widget_queue_draw_area( color_drawing_area, 0, 0, cur_color_display_size, cur_color_display_size );
}

static void draw_crosshair( cairo_t* rect ) {
    const int SPACING     = 5;
    const int DASH_LENGTH = 5;

    const double dashed1[] = { DASH_LENGTH, SPACING };
    static int len1        = sizeof( dashed1 ) / sizeof( dashed1[0] );

    cairo_set_line_width( rect, 1.5 );
    cairo_set_dash( rect, dashed1, len1, 0 );
    cairo_set_source_rgb( rect, 1, 1, 1 );

    const int CENTER_CONTEXT_DISPLAY_PIXEL = cur_context_display_size / 2;

    // horizontal white dashes
    cairo_move_to( rect, 0, CENTER_CONTEXT_DISPLAY_PIXEL );
    cairo_line_to( rect, cur_context_display_size, CENTER_CONTEXT_DISPLAY_PIXEL );
    cairo_stroke( rect );

    // vertical white dashes
    cairo_move_to( rect, CENTER_CONTEXT_DISPLAY_PIXEL, 0 );
    cairo_line_to( rect, CENTER_CONTEXT_DISPLAY_PIXEL, cur_context_display_size );
    cairo_stroke( rect );

    // horizontal black dashes
    cairo_set_source_rgb( rect, 0, 0, 0 );
    cairo_move_to( rect, DASH_LENGTH, CENTER_CONTEXT_DISPLAY_PIXEL );
    cairo_line_to( rect, cur_context_display_size, CENTER_CONTEXT_DISPLAY_PIXEL );
    cairo_stroke( rect );

    // vertical black dashes
    cairo_move_to( rect, CENTER_CONTEXT_DISPLAY_PIXEL, DASH_LENGTH );
    cairo_line_to( rect, CENTER_CONTEXT_DISPLAY_PIXEL, cur_context_display_size );
    cairo_stroke( rect );
}

static void get_center_context_pixel( int* r, int* g, int* b ) {
    int center         = cur_context_size / 2;
    color center_pixel = front_context_buffer[center * cur_context_size + center];
    *r                 = center_pixel.r;
    *g                 = center_pixel.g;
    *b                 = center_pixel.b;
}

static void draw_context_pixels() {
    cairo_t* cr;
    cr = cairo_create( context_surface );

    int PIXEL_SCALE = cur_context_display_size / cur_context_size;

    for( int x = 0; x < cur_context_size; x++ ) {
        for( int y = 0; y < cur_context_size; y++ ) {
            cairo_set_source_rgb( cr,    // row-major layout
                                  front_context_buffer[cur_context_size * x + y].r / 255.0,
                                  front_context_buffer[cur_context_size * x + y].g / 255.0,
                                  front_context_buffer[cur_context_size * x + y].b / 255.0 );
            cairo_rectangle( cr, x * PIXEL_SCALE, y * PIXEL_SCALE, PIXEL_SCALE, PIXEL_SCALE );
            cairo_fill( cr );
        }
    }

    if( app_preferences.draw_crosshair ) {
        draw_crosshair( cr );
    }

    cairo_destroy( cr );
    gtk_widget_queue_draw_area( context_drawing_area, 0, 0, cur_context_display_size, cur_context_display_size );
}

static gboolean delete_event( GtkWidget* w, GdkEvent* e, gpointer d ) {
    g_mutex_lock( &running_mutex );
    running = 0;
    g_mutex_unlock( &running_mutex );
    return FALSE;
}

static void close_window( void ) {
    if( rgb_surface ) cairo_surface_destroy( rgb_surface );

    if( context_surface ) cairo_surface_destroy( context_surface );
}

static void query_pointer( int* x, int* y ) {
    static int once;
    int i;
    unsigned m;
    Window w;

    if( once == 0 ) {
        once = 1;
        root = DefaultRootWindow( d );
    }

    if( !XQueryPointer( d, root, &root, &w, x, y, &i, &i, &m ) ) {
        for( i = -1; ++i < ScreenCount( d ); ) {
            if( root == RootWindow( d, i ) ) {
                break;
            }
        }
    }
}

void clip_coords_to_display_size( int* x, int* y ) {
    if( *x < 0 ) {
        *x = 0;
    }
    if( *x >= screen->width ) {
        *x = screen->width - 1;
    }

    if( *y < 0 ) {
        *y = 0;
    }
    if( *y >= screen->height ) {
        *y = screen->height - 1;
    }
}

void get_context_pixels( Display* d, int x, int y ) {
    x -= cur_context_size / 2;
    y -= cur_context_size / 2;
    clip_coords_to_display_size( &x, &y );
    int w = cur_context_size;
    int h = cur_context_size;
    if( x + w >= screen->width ) {
        x = screen->width - cur_context_size;
    }
    if( y + h >= screen->height ) {
        y = screen->height - cur_context_size;
    }
    XImage* image = screengrab_xlib( d, x, y, w, h );
    for( int i = 0; i < cur_context_size; i++ ) {
        for( int j = 0; j < cur_context_size; j++ ) {
            unsigned long c                                 = XGetPixel( image, i, j );
            back_context_buffer[i * cur_context_size + j].r = ( c & image->red_mask ) >> 16;
            back_context_buffer[i * cur_context_size + j].g = ( c & image->green_mask ) >> 8;
            back_context_buffer[i * cur_context_size + j].b = ( c & image->blue_mask );
        }
    }
    XDestroyImage( image );

    struct timeval cur_time = get_time();
    long elapsed            = get_usec_elapsed( &cur_time, &last_update );
    long remainder          = USEC_PER_FRAME - elapsed;
    if( remainder > 0 ) {
        // sleep for remainder of the time slice
        usleep( remainder );
    }
    memcpy( front_context_buffer, back_context_buffer, sizeof( color ) * cur_context_size * cur_context_size );
    last_update = get_time();
}

static gboolean update_color( gpointer user_data ) {
    LOG_TID();
    if( !running ) return G_SOURCE_REMOVE;

    // get color of pixel under cursor
    int r, g, b;
    char s2[50] = "";
    get_center_context_pixel( &r, &g, &b );

    // get pixels around cursor
    LOG_TIMING( draw_context_pixels(), "DRAWCONTEXT" );

    // set RGB readout
    sprintf( s2, "RGB: %03d %03d %03d", r, g, b );

    char* rgb_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                     "%s"
                                     "</span>",
                                     s2 );

    gtk_label_set_markup( GTK_LABEL( rgb_label ), rgb_str );

    // set HSV readout
    float h, s, v;
    hsv_from_rgb( &h, &s, &v, r, g, b );
    char* hsv_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                     "HSV: %03.0f %03.0f %03.0f"
                                     "</span>",
                                     h,
                                     s * 100.0,
                                     v * 100.0 );
    gtk_label_set_markup( GTK_LABEL( hsv_label ), hsv_str );

    // set HSL readout
    float hsl_h, hsl_s, hsl_l;
    hsl_from_hsv( &hsl_h, &hsl_s, &hsl_l, h, s, v );
    char* hsl_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                     "HSL: %03.0f %03.0f %03.0f"
                                     "</span>",
                                     hsl_h,
                                     hsl_s * 100,
                                     hsl_l * 100 );
    gtk_label_set_markup( GTK_LABEL( hsl_label ), hsl_str );

    // set name readout
    color c = nearest_color( r, g, b, full_color_list, full_color_list_length );

    char nameLbl[60];
    sprintf( nameLbl, c.name );
    char* name_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                      "%s"
                                      "</span>",
                                      nameLbl );
    gtk_label_set_markup( GTK_LABEL( color_name_label ), name_str );

    color sc = nearest_color( r, g, b, basic_color_list, basic_color_list_length );

    char main_nameLbl[60];
    sprintf( main_nameLbl, sc.name );
    char* main_name_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                           "%s"
                                           "</span>",
                                           main_nameLbl );
    gtk_label_set_markup( GTK_LABEL( main_color_name_label ), main_name_str );

    char* hex_str = g_strdup_printf( "<span font=\"12\" color=\"black\">"
                                     "Hex: %02X %02X %02X"
                                     "</span>",
                                     r,
                                     g,
                                     b );
    gtk_label_set_markup( GTK_LABEL( hex_label ), hex_str );

    draw_rect( r, g, b );

    g_free( rgb_str );
    g_free( hsv_str );
    g_free( hsl_str );
    g_free( name_str );
    g_free( main_name_str );
    g_free( hex_str );

    return G_SOURCE_REMOVE;
}

static gpointer grab_thread_func( gpointer user_data ) {
    for( ;; ) {
        g_mutex_lock( &running_mutex );
        if( !running ) {
            g_mutex_unlock( &running_mutex );
            g_thread_exit( NULL );
        }
        g_mutex_unlock( &running_mutex );
        query_pointer( &LAST_MOUSE_X, &LAST_MOUSE_Y );
        g_mutex_lock( &context_mutex );
        while( !need_context ) {
            g_cond_wait( &grab_cond, &context_mutex );
        }
        LOG_TIMING( get_context_pixels( d, LAST_MOUSE_X, LAST_MOUSE_Y ), "GETCONTEXT" );
        need_context = 0;
        g_cond_signal( &draw_cond );
        g_mutex_unlock( &context_mutex );
    }

    return NULL;
}

static gpointer update_thread_func( gpointer user_data ) {
    GSource* source;

    for( ;; ) {
        g_mutex_lock( &running_mutex );
        if( !running ) {
            g_mutex_unlock( &running_mutex );
            g_thread_exit( NULL );
        }
        // do the read/write from this thread, then gui thread just swaps front/back buffers
        g_mutex_unlock( &running_mutex );
        source = g_idle_source_new();

        g_mutex_lock( &context_mutex );
        while( need_context ) {
            g_cond_wait( &draw_cond, &context_mutex );
        }
        g_source_set_callback( source, update_color, NULL, NULL );
        g_source_attach( source, context );
        g_source_unref( source );
        need_context = 1;
        g_cond_signal( &grab_cond );
        g_mutex_unlock( &context_mutex );
    }

    return NULL;
}

static int catchFalseAlarm( void ) { return 0; }

void setup_x11() {
    XSetWindowAttributes attribs;
    Window w;

    if( !( d = XOpenDisplay( 0 ) ) ) {
        fprintf( stderr, "Couldn't connect to %s\n", XDisplayName( 0 ) );
        exit( EXIT_FAILURE );
    }
    screen = ScreenOfDisplay( d, 0 );

    attribs.override_redirect = True;
    w = XCreateWindow( d, DefaultRootWindow( d ), 800, 800, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, CWOverrideRedirect, &attribs );
    XMapWindow( d, w );
    XSetErrorHandler( (XErrorHandler)catchFalseAlarm );
    XSync( d, 0 );
}

static void activate( GtkApplication* app, gpointer user_data ) {
    GtkWidget* frame;
    GtkWidget* context_frame;
    GtkWidget* hbox;
    GtkWidget* vbox;

    setup_x11();

    main_window = gtk_application_window_new( app );
    gtk_window_set_title( GTK_WINDOW( main_window ), "Color Helper" );
    gtk_window_set_resizable( GTK_WINDOW( main_window ), FALSE );
    gtk_window_set_default_size( GTK_WINDOW( main_window ), 400, 100 );

    g_signal_connect( main_window, "destroy", G_CALLBACK( close_window ), NULL );
    g_signal_connect( main_window, "delete-event", G_CALLBACK( delete_event ), NULL );

    gtk_container_set_border_width( GTK_CONTAINER( main_window ), 8 );

    frame = gtk_frame_new( NULL );
    gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );

    context_frame = gtk_frame_new( NULL );
    gtk_frame_set_shadow_type( GTK_FRAME( context_frame ), GTK_SHADOW_NONE );

    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 5 );
    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 5 );
    gtk_container_add( GTK_CONTAINER( main_window ), hbox );

    gtk_box_pack_start( GTK_BOX( hbox ), frame, 0, 0, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ), context_frame, 0, 0, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ), vbox, 0, 0, 0 );

    color_name_label = gtk_label_new( "Cornflower blue" );
    gtk_widget_set_valign( color_name_label, GTK_ALIGN_START );
    gtk_widget_set_halign( color_name_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), color_name_label, 0, 0, 0 );

    main_color_name_label = gtk_label_new( "Blue" );
    gtk_widget_set_valign( main_color_name_label, GTK_ALIGN_START );
    gtk_widget_set_halign( main_color_name_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), main_color_name_label, 0, 0, 0 );

    rgb_label = gtk_label_new( "127, 127, 127" );
    gtk_widget_set_valign( rgb_label, GTK_ALIGN_START );
    gtk_widget_set_halign( rgb_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), rgb_label, 0, 0, 0 );

    hex_label = gtk_label_new( "FF FF FF" );
    gtk_widget_set_valign( hex_label, GTK_ALIGN_START );
    gtk_widget_set_halign( hex_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), hex_label, 0, 0, 0 );

    hsv_label = gtk_label_new( "0 0 0" );
    gtk_widget_set_valign( hsv_label, GTK_ALIGN_START );
    gtk_widget_set_halign( hsv_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), hsv_label, 0, 0, 0 );

    hsl_label = gtk_label_new( "0 0 0" );
    gtk_widget_set_valign( hsl_label, GTK_ALIGN_START );
    gtk_widget_set_halign( hsl_label, GTK_ALIGN_START );
    gtk_box_pack_start( GTK_BOX( vbox ), hsl_label, 0, 0, 0 );

    color_drawing_area   = gtk_drawing_area_new();
    context_drawing_area = gtk_drawing_area_new();

    // set a minimum size
    gtk_widget_set_size_request( color_drawing_area, cur_color_display_size, cur_color_display_size );
    gtk_widget_set_size_request( context_drawing_area, cur_context_display_size, cur_context_display_size );

    gtk_container_add( GTK_CONTAINER( frame ), color_drawing_area );
    gtk_container_add( GTK_CONTAINER( context_frame ), context_drawing_area );

    // signals used to handle the backing surface
    g_signal_connect( color_drawing_area, "draw", G_CALLBACK( draw_cb_rgb ), NULL );
    g_signal_connect( color_drawing_area, "configure-event", G_CALLBACK( configure_event_cb_rgb ), NULL );

    g_signal_connect( context_drawing_area, "draw", G_CALLBACK( draw_cb_context ), NULL );
    g_signal_connect( context_drawing_area, "configure-event", G_CALLBACK( configure_event_cb_context ), NULL );

    g_signal_connect( main_window, "button-press-event", G_CALLBACK( on_window_clicked ), NULL );

    g_signal_connect( main_window, "key-press-event", G_CALLBACK( on_key_pressed ), NULL );

    gtk_widget_show_all( main_window );
    refresh_preferences();
    update_thread = g_thread_new( NULL, update_thread_func, (gpointer)d );
    grab_thread   = g_thread_new( NULL, grab_thread_func, (gpointer)d );
}

void load_preferences() {
    preferences* prefs = preferences_read();
    app_preferences    = *prefs;
    free( prefs );
}

void load_full_color_list() {
    if( full_color_list ) {
        free( full_color_list );
    }

    if( !read_colors( &full_color_list, full_txt, full_txt_len, &full_color_list_length ) ) {
        printf( "Failed to load full colors. Color naming is disabled.\n" );
    }
}

void load_basic_color_list() {
    if( basic_color_list ) {
        free( basic_color_list );
    }

    if( !read_colors( &basic_color_list, basic_txt, basic_txt_len, &basic_color_list_length ) ) {
        printf( "Failed to load basic colors. Color naming is disabled.\n" );
    }
}

int main( int argc, char** argv ) {
    load_basic_color_list();
    load_full_color_list();
    load_preferences();
    front_context_buffer = calloc( MAX_CONTEXT_SIZE * MAX_CONTEXT_SIZE, sizeof( color ) );
    back_context_buffer  = calloc( MAX_CONTEXT_SIZE * MAX_CONTEXT_SIZE, sizeof( color ) );
    last_update          = get_time();

    XInitThreads();
    g_mutex_init( &running_mutex );

    GtkApplication* app = gtk_application_new( "org.gtk.example", G_APPLICATION_FLAGS_NONE );
    g_signal_connect( app, "activate", G_CALLBACK( activate ), NULL );

    int status = g_application_run( G_APPLICATION( app ), argc, argv );

    // wait on both threads before deleting buffers
    g_thread_join( update_thread );
    g_thread_join( grab_thread );

    // free all resources
    g_object_unref( app );
    free( full_color_list );
    free( basic_color_list );
    free( front_context_buffer );
    free( back_context_buffer );
    free( app_preferences.color_map_file );
    XCloseDisplay( d );

    return status;
}
