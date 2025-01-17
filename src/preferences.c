#include "preferences.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

gchar* get_preferences_file_path();

int get_integer_value( GKeyFile* file, const gchar* group_name, const gchar* key, int default_val ) {
    GError* error = NULL;
    int val       = g_key_file_get_integer( file, group_name, key, &error );
    if( error ) {
        val = default_val;
    }
    return val;
}

char* get_string_value( GKeyFile* file, const gchar* group_name, const gchar* key, char* default_val ) {
    GError* error = NULL;
    char* val     = g_key_file_get_string( file, group_name, key, &error );
    if( error ) {
        val = strdup( default_val );
    }

    return val;
}

preferences* preferences_read() {
    GKeyFile* file     = g_key_file_new();
    preferences* prefs = malloc( sizeof( preferences ) );

    gchar* preferences_filepath = get_preferences_file_path();
    g_key_file_load_from_file( file, preferences_filepath, G_KEY_FILE_NONE, NULL );
    g_free( preferences_filepath );

    prefs->rgb_display       = get_integer_value( file, "View", "rgb_display", 1 );
    prefs->hex_display       = get_integer_value( file, "View", "hex_display", 0 );
    prefs->hsv_display       = get_integer_value( file, "View", "hsv_display", 0 );
    prefs->hsl_display       = get_integer_value( file, "View", "hsl_display", 0 );
    prefs->name_display      = get_integer_value( file, "View", "name_display", 1 );
    prefs->main_name_display = get_integer_value( file, "View", "main_name_display", 1 );
    prefs->title_bar         = get_integer_value( file, "View", "title_bar", 1 );
    prefs->zoom_level        = get_integer_value( file, "View", "zoom_level", 25 );
    prefs->draw_crosshair    = get_integer_value( file, "View", "draw_crosshair", 1 );
    prefs->frames_per_second = get_integer_value( file, "System", "frames_per_second", 20 );

    return prefs;
}

void preferences_print( preferences* prefs ) {
    printf( "preferences: %d %d %d %d\n", prefs->rgb_display, prefs->hex_display, prefs->hsv_display, prefs->title_bar );
}

gchar* get_preferences_file_path() {
    const gchar* dir   = g_get_user_config_dir();
    gchar* config_file = g_build_path( "/", dir, "colorhelperrc", NULL );

    return config_file;
}

void preferences_write( preferences* prefs ) {
    GKeyFile* file = g_key_file_new();

    g_key_file_set_integer( file, "View", "rgb_display", prefs->rgb_display );

    g_key_file_set_integer( file, "View", "hex_display", prefs->hex_display );

    g_key_file_set_integer( file, "View", "hsv_display", prefs->hsv_display );

    g_key_file_set_integer( file, "View", "hsl_display", prefs->hsl_display );

    g_key_file_set_integer( file, "View", "name_display", prefs->name_display );

    g_key_file_set_integer( file, "View", "main_name_display", prefs->main_name_display );

    g_key_file_set_integer( file, "View", "title_bar", prefs->title_bar );

    g_key_file_set_integer( file, "View", "zoom_level", prefs->zoom_level );

    g_key_file_set_integer( file, "View", "draw_crosshair", prefs->draw_crosshair );

    g_key_file_set_integer( file, "System", "frames_per_second", prefs->frames_per_second );

    gchar* preferences_filepath = get_preferences_file_path();
    g_key_file_save_to_file( file, preferences_filepath, NULL );
    g_free( preferences_filepath );
    g_key_file_free( file );
}
