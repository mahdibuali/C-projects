#include <gtk/gtk.h>
#include <string.h>

#define MAX_BUFFER_SIZE (1024)

// This structure contains all the widgets in GUI
struct ui_widgets {
  GtkWidget* text_label;

  // Add your widgets below

} g_widgets;

// This structure contains all the global parameters used among different GUI 
// functions
struct parameters {
  int n;

  // Add you parameters below

} g_parameters;


/*
 *  update the content of text label
 */

void update_text()
{
  // print to buffer

  char buffer[MAX_BUFFER_SIZE];
  snprintf(buffer, MAX_BUFFER_SIZE, "Current number: %d", g_parameters.n);

  // update label's text

  gtk_label_set_text(GTK_LABEL(g_widgets.text_label), buffer);
} /* update_text() */


/* 
 *  callback function for "-1" button
 */

void dec_cb(GtkButton* widget, gpointer data)
{
  --g_parameters.n;
  update_text();
} /* dec_cb() */


/* 
 *  callback function for "+1" button
 */

void inc_cb(GtkButton* widget, gpointer data)
{
  ++g_parameters.n;
  update_text();
} /* inc_cb() */


/* 
 *  called when program starts, creates the whole window
 */

void activate(GtkApplication* app, gpointer user_data)
{
  // init parameters

  g_parameters.n = 0;

  // Create the window, the container of all other widgets

  GtkWidget* window = gtk_application_window_new(app);

  // Set window title

  gtk_window_set_title(GTK_WINDOW(window), "Hello title");

  // box is a container for widgets in a single row or column, here we use it 
  // horizontally

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

  // put this box into the window

  gtk_container_add(GTK_CONTAINER(window), box);

  // creates an empty text label

  g_widgets.text_label = gtk_label_new("");

  // add text to the box

  gtk_box_pack_start(GTK_BOX(box), g_widgets.text_label, TRUE, TRUE, 10);

  // creates a button

  GtkWidget* dec_button = gtk_button_new_with_label("-1");

  // connect the button's clicked event with dec_cb callback function

  g_signal_connect(dec_button, "clicked", G_CALLBACK(dec_cb), NULL);

  // add button to the box as well

  gtk_box_pack_start(GTK_BOX(box), dec_button, TRUE, TRUE, 10);

  // creates another button

  GtkWidget* inc_button = gtk_button_new_with_label("+1");

  // connect its clikced event to inc_cb function

  g_signal_connect(inc_button, "clicked", G_CALLBACK(inc_cb), NULL);

  // add button to the box

  gtk_box_pack_start(GTK_BOX(box), inc_button, TRUE, TRUE, 10);

  update_text();
  
  // show the window

  gtk_widget_show_all(window);
} /* activate() */
