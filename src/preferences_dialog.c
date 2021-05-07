#include <gtk/gtk.h>

#include "preferences_dialog.h"

GtkWidget* rgb_check;
GtkWidget* hex_check;
GtkWidget* hsv_check;
GtkWidget* hsl_check;
GtkWidget* name_check;
GtkWidget* main_name_check;
GtkWidget* title_bar_check;
GtkWidget* draw_crosshair_check;
GtkWidget* zoom_level_combo;
GtkWidget* fps_spin_button;

static const int ZOOM_LEVELS[] = { 10, 25, 50, 100 };

void on_toggle_option_changed(GtkToggleButton* widget, gpointer userdata) {
    int* option = userdata;
    *option = gtk_toggle_button_get_active(widget) ? 1 : 0;
}

void on_fps_changed(GtkToggleButton* widget, gpointer userdata) {
    preferences* prefs = (preferences*)userdata;
    prefs->frames_per_second = gtk_spin_button_get_value_as_int((GtkSpinButton*)fps_spin_button);
}

void on_zoom_combo_changed(GtkComboBoxText* widget, gpointer userdata) {
    gchar* selected = gtk_combo_box_text_get_active_text(widget);
    preferences* p = (preferences*) userdata;
    p->zoom_level = atoi(selected);
    g_free(selected);
}

void show_preferences_dialog(GtkWindow* parent, preferences* prefs, gboolean(* on_preferences_closed_func)(GtkWidget*, GdkEvent*, gpointer)) {
    GtkWidget* dialog = gtk_dialog_new();
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget* notebook = gtk_notebook_new();
    add_view_tab(notebook, prefs);
    add_system_tab(notebook, prefs);
    gtk_container_add(GTK_CONTAINER(content_area), notebook);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    g_signal_connect (dialog, "delete-event", G_CALLBACK (on_preferences_closed_func), (gpointer)prefs);

    display_preferences(prefs);

    gtk_widget_show_all(dialog);
}

void add_system_tab(GtkWidget* notebook, preferences* prefs) {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget* fps_control_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* fps_label = gtk_label_new("Frames per second:");
    gtk_box_pack_start(GTK_BOX(fps_control_hbox), fps_label, 0, 0, 0);

    GtkAdjustment* adjustment = gtk_adjustment_new(prefs->frames_per_second, 1, 200, 1, 0, 0);
    fps_spin_button = gtk_spin_button_new(adjustment, 1.0, 0);

    gtk_box_pack_start(GTK_BOX(fps_control_hbox), fps_spin_button, 0, 0, 0);

    g_signal_connect(G_OBJECT(fps_spin_button), "value-changed", G_CALLBACK(on_fps_changed), prefs);

    gtk_box_pack_start(GTK_BOX(vbox), fps_control_hbox, 0, 0, 0);

    GtkWidget* system_tab_label = gtk_label_new("System");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, system_tab_label);
}

void add_view_tab(GtkWidget* notebook, preferences* prefs) {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    GtkWidget* view_tab_label = gtk_label_new("View");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, view_tab_label);

    rgb_check = gtk_check_button_new_with_label("Show RGB value");
    gtk_box_pack_start(GTK_BOX(vbox), rgb_check, 0, 0, 0);

    hex_check = gtk_check_button_new_with_label("Show HEX value");
    gtk_box_pack_start(GTK_BOX(vbox), hex_check, 0, 0, 0);

    hsv_check = gtk_check_button_new_with_label("Show HSV value");
    gtk_box_pack_start(GTK_BOX(vbox), hsv_check, 0, 0, 0);

    hsl_check = gtk_check_button_new_with_label("Show HSL value");
    gtk_box_pack_start(GTK_BOX(vbox), hsl_check, 0, 0, 0);

    title_bar_check = gtk_check_button_new_with_label("Enable title bar");
    gtk_box_pack_start(GTK_BOX(vbox), title_bar_check, 0, 0, 0);

    draw_crosshair_check = gtk_check_button_new_with_label("Draw crosshair");
    gtk_box_pack_start(GTK_BOX(vbox), draw_crosshair_check, 0, 0, 0);

    GtkWidget* zoom_controls_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget* zoom_level_label = gtk_label_new("Zoom level:");
    gtk_box_pack_start(GTK_BOX(zoom_controls_hbox), zoom_level_label, 0, 0, 0);

    zoom_level_combo = gtk_combo_box_text_new();
    int NUM_ZOOM_LEVELS = sizeof(ZOOM_LEVELS)/sizeof(ZOOM_LEVELS[0]);
    for(int i = 0; i < NUM_ZOOM_LEVELS; i++) {
        char str[5];
        sprintf(str, "%d", ZOOM_LEVELS[i]);
        gtk_combo_box_text_append((GtkComboBoxText*)zoom_level_combo, NULL, str);
    }
    gtk_box_pack_start(GTK_BOX(zoom_controls_hbox), zoom_level_combo, 0, 0, 0);

    gtk_box_pack_start(GTK_BOX(vbox), zoom_controls_hbox, 0, 0, 0);

    g_signal_connect(G_OBJECT(rgb_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->rgb_display));
    g_signal_connect(G_OBJECT(hex_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->hex_display));
    g_signal_connect(G_OBJECT(hsv_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->hsv_display));
    g_signal_connect(G_OBJECT(hsl_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->hsl_display));
    g_signal_connect(G_OBJECT(name_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->name_display));
    g_signal_connect(G_OBJECT(main_name_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->main_name_display));
    g_signal_connect(G_OBJECT(title_bar_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->title_bar));
    g_signal_connect(G_OBJECT(draw_crosshair_check), "toggled", G_CALLBACK(on_toggle_option_changed), &(prefs->draw_crosshair));

    g_signal_connect(G_OBJECT(zoom_level_combo), "changed", G_CALLBACK(on_zoom_combo_changed), prefs);
}

void display_preferences(preferences* prefs) {
    gtk_toggle_button_set_active((GtkToggleButton*)rgb_check, prefs->rgb_display);
    gtk_toggle_button_set_active((GtkToggleButton*)hex_check, prefs->hex_display);
    gtk_toggle_button_set_active((GtkToggleButton*)hsv_check, prefs->hsv_display);
    gtk_toggle_button_set_active((GtkToggleButton*)hsl_check, prefs->hsl_display);
    gtk_toggle_button_set_active((GtkToggleButton*)name_check, prefs->name_display);
    gtk_toggle_button_set_active((GtkToggleButton*)main_name_check, prefs->main_name_display);
    gtk_toggle_button_set_active((GtkToggleButton*)title_bar_check, prefs->title_bar);
    gtk_toggle_button_set_active((GtkToggleButton*)draw_crosshair_check, prefs->draw_crosshair);

    const int NUM_ZOOM_LEVELS = sizeof(ZOOM_LEVELS)/sizeof(ZOOM_LEVELS[0]);
    for(int i = 0; i < NUM_ZOOM_LEVELS; i++) {
        if(prefs->zoom_level == ZOOM_LEVELS[i]) {
            gtk_combo_box_set_active((GtkComboBox*)zoom_level_combo, i);
        }
    }
}

