#include <gst/gst.h>
#include <gst/video/video.h>

#include <cairo.h>
#include <cairo-gobject.h>
#include <time.h>
#include <stdio.h>

#include <glib.h>

// apt-get install gstreamer1.0-dev libgstreamer-plugins-base1.0-dev libcairo2-dev

static gboolean
on_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *) user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_critical ("Got ERROR: %s (%s)", err->message, GST_STR_NULL (debug));
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err = NULL;
      gchar *debug;

      gst_message_parse_warning (message, &err, &debug);
      g_warning ("Got WARNING: %s (%s)", err->message, GST_STR_NULL (debug));
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }

  return TRUE;
}

/* Datastructure to share the state we are interested in between
 * prepare and render function. */
typedef struct
{
  gboolean valid;
  GstVideoInfo vinfo;
} CairoOverlayState;

/* Store the information from the caps that we are interested in. */
static void
prepare_overlay (GstElement * overlay, GstCaps * caps, gpointer user_data)
{
  CairoOverlayState *state = (CairoOverlayState *) user_data;

  state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

/* Draw the overlay. 
 * This function draws a cute "beating" heart. */
static void
draw_overlay (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  char* labels[] = { "270", "215", " 0 ", " 45" };
  char* current_label = 0;
  int label_count = 0;
  CairoOverlayState *s = (CairoOverlayState *) user_data;
  double scale = 1;
  int width, height;
  
  time_t T= time(NULL);
  struct  tm tm = *localtime(&T);

  if (!s->valid)
    return;

  width = GST_VIDEO_INFO_WIDTH (&s->vinfo);
  height = GST_VIDEO_INFO_HEIGHT (&s->vinfo);

  // draw red lines out from the center of the window
//  scale = 2 * (((timestamp / (int) 1e7) % 70) + 30) / 100.0;
  cairo_translate (cr, width / 2, (height / 2) - 30);
  cairo_scale (cr, scale, scale);
  cairo_set_line_width(cr, 1);
  cairo_set_source_rgb(cr, 1, 1, 1);

  // square
  cairo_rectangle(cr, -10, -10, +20, +20);
  cairo_rectangle(cr, 0, 0, 1, 1);

  // Degrees
  int l=0;
  for (int i=0; i<40; i++) {
    int ii = i-4;
    int offset=0;
    if (!(ii % 10)) {
      offset =3;
      current_label = labels[label_count++];
      cairo_set_font_size(cr, 14);
      cairo_move_to(cr, -200 + (i*10) - 10, -160);
      cairo_show_text(cr, current_label);  
    }
    cairo_move_to(cr, -200 + (i*10), -190);
    cairo_line_to(cr, -200 + (i*10), -185 + offset);
  }
  // Degrees Centre
  cairo_move_to(cr, -5, -200);
  cairo_line_to(cr, 0, -195);
  cairo_line_to(cr, 5, -200);
  
  // Cross hair
  // Virtical above
  cairo_move_to(cr, 0, -10);
  cairo_line_to(cr, 0, -15);
  cairo_move_to(cr, 0, -25);
  cairo_line_to(cr, 0, -35);
  // Horizontal right
  cairo_move_to(cr, -10, 0);
  cairo_line_to(cr, -30, 0);
  cairo_move_to(cr, -30, -10);
  cairo_line_to(cr, -50, -10);
  cairo_move_to(cr, -30, +10);
  cairo_line_to(cr, -50, +10);
  cairo_move_to(cr, -70, 0);
  cairo_line_to(cr, -90, 0);
  // Virtical below
  cairo_move_to(cr, 0, +10);
  cairo_line_to(cr, 0, +15);
  cairo_move_to(cr, 0, +25);
  cairo_line_to(cr, 0, +35);
  // Horizontal left
  cairo_move_to(cr, +10, 0);
  cairo_line_to(cr, +30, 0);
  cairo_move_to(cr, +30, +10);
  cairo_line_to(cr, +50, +10);
  cairo_move_to(cr, +30, -10);
  cairo_line_to(cr, +50, -10);
  cairo_move_to(cr, +70, 0);
  cairo_line_to(cr, +90, 0);

  cairo_stroke(cr);
  
//  cairo_set_source_rgb(cr, 0.0, 0.8, 0.0);
  cairo_select_font_face(cr, "Ubuntu Thin",
  CAIRO_FONT_SLANT_NORMAL,
  CAIRO_FONT_WEIGHT_BOLD);

  cairo_set_font_size(cr, 24);

  cairo_move_to(cr, -20, 250);
  cairo_show_text(cr, "0025");  
  
  char tstring[200];
  snprintf(tstring, 200, "%02d:%02d:%02d",tm.tm_hour, tm.tm_min, tm.tm_sec);

  cairo_set_font_size(cr, 14);
  cairo_move_to(cr, -300, -180);
  cairo_show_text(cr, tstring);  
  
  // Telemetry left
  cairo_move_to(cr, -300, -120);
  cairo_show_text(cr, "15°");  
  cairo_move_to(cr, -300, -40);
  cairo_show_text(cr, "WPN - READY");  
  cairo_move_to(cr, -300, -20);
  cairo_show_text(cr, "SPIKE");  

  // Telemetry right
  cairo_move_to(cr, 270, -120);
  cairo_show_text(cr, "5 KPH");  
  cairo_move_to(cr, 270, -40);
  cairo_show_text(cr, "1X");
  cairo_move_to(cr, 270, -20);
  cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
  cairo_show_text(cr, "IR");

}

static GstElement *
setup_gst_pipeline (CairoOverlayState * overlay_state)
{
  int width = 640;
  int height = 480;
  int framerate = 30;

  GstElement *pipeline;
  GstElement *cairo_overlay;
  GstElement *source, *capabilities, *adaptor1, *adaptor2, *sink;
  GstCaps *caps, *caps_camera;

  pipeline = gst_pipeline_new ("cairo-overlay-example");

  /* Adaptors needed because cairooverlay only supports ARGB data */
  source = gst_element_factory_make ("v4l2src", "source");
  capabilities = gst_element_factory_make("capsfilter", NULL);
  adaptor1 = gst_element_factory_make ("videoconvert", "adaptor1");
  cairo_overlay = gst_element_factory_make ("cairooverlay", "overlay");
  adaptor2 = gst_element_factory_make ("videoconvert", "adaptor2");
  sink = gst_element_factory_make ("ximagesink", "sink");
  if (sink == NULL)
    sink = gst_element_factory_make ("autovideosink", "sink");

  /* If failing, the element could not be created */
  g_assert (cairo_overlay);

  caps_camera = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height,
                                      "framerate", GST_TYPE_FRACTION, framerate, 1, NULL);

  g_object_set(capabilities, "caps", caps_camera, NULL);


  /* Hook up the necessary signals for cairooverlay */
  g_signal_connect (cairo_overlay, "draw",
      G_CALLBACK (draw_overlay), overlay_state);
  g_signal_connect (cairo_overlay, "caps-changed",
      G_CALLBACK (prepare_overlay), overlay_state);

  gst_bin_add_many (GST_BIN (pipeline), source, capabilities, adaptor1, cairo_overlay, adaptor2, sink, NULL);

  if (!gst_element_link_many (source, capabilities, adaptor1,
          cairo_overlay, adaptor2, sink, NULL)) {
    g_warning ("Failed to link elements!");
  }

  return pipeline;
}

int
main (int argc, char **argv)
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstBus *bus;
  CairoOverlayState *overlay_state;

  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  /* allocate on heap for pedagogical reasons, makes code easier to transfer */
  overlay_state = g_new0 (CairoOverlayState, 1);

  pipeline = setup_gst_pipeline (overlay_state);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message", G_CALLBACK (on_message), loop);
  gst_object_unref (GST_OBJECT (bus));

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  g_free (overlay_state);
  return 0;
}
