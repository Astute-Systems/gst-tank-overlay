#include <gst/gst.h>
#include <gst/video/video.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <time.h>
#include <stdio.h>
#include <glib.h>
// std::string
#include <string>
// Required libraries: gstreamer1.0-dev, libgstreamer-plugins-base1.0-dev, libcairo2-dev

#define V4L2_SRC 0

// Callback function to handle messages from the GStreamer bus
static gboolean
on_message(GstBus *bus, GstMessage *message, gpointer user_data)
{
  GMainLoop *loop = (GMainLoop *)user_data;

  switch (GST_MESSAGE_TYPE(message))
  {
  case GST_MESSAGE_ERROR:
  {
    // Handle error messages
    GError *err = NULL;
    gchar *debug;

    gst_message_parse_error(message, &err, &debug);
    g_critical("Got ERROR: %s (%s)", err->message, GST_STR_NULL(debug));
    g_main_loop_quit(loop);
    break;
  }
  case GST_MESSAGE_WARNING:
  {
    // Handle warning messages
    GError *err = NULL;
    gchar *debug;

    gst_message_parse_warning(message, &err, &debug);
    g_warning("Got WARNING: %s (%s)", err->message, GST_STR_NULL(debug));
    g_main_loop_quit(loop);
    break;
  }
  case GST_MESSAGE_EOS:
    // Handle end-of-stream messages
    g_main_loop_quit(loop);
    break;
  default:
    break;
  }

  return TRUE;
}

// Data structure to share state between prepare and render functions
typedef struct
{
  gboolean valid;     // Indicates if the video info is valid
  GstVideoInfo vinfo; // Stores video information
} CairoOverlayState;

// Callback to prepare overlay with video information from caps
static void
prepare_overlay(GstElement *overlay, GstCaps *caps, gpointer user_data)
{
  CairoOverlayState *state = (CairoOverlayState *)user_data;

  // Extract video information from caps
  state->valid = gst_video_info_from_caps(&state->vinfo, caps);
}

static void camera_mode(cairo_t *cr, const std::string &mode)
{
  // Set the text color to gray
  cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);

  // Set the font size for the text
  cairo_set_font_size(cr, 14);

  // Display the mode text
  cairo_show_text(cr, mode.c_str());
}

// Callback to draw the overlay using Cairo
static void
draw_overlay(GstElement *overlay, cairo_t *cr, guint64 timestamp,
             guint64 duration, gpointer user_data)
{
  char *labels[] = {"270", "215", " 0 ", " 45"};
  char *current_label = 0;
  int label_count = 0;
  CairoOverlayState *s = (CairoOverlayState *)user_data;
  double scale = 1;
  int width, height;

  // Get the current time
  time_t T = time(NULL);
  struct tm tm = *localtime(&T);

  if (!s->valid)
    return;

  // Get video dimensions
  width = GST_VIDEO_INFO_WIDTH(&s->vinfo);
  height = GST_VIDEO_INFO_HEIGHT(&s->vinfo);

  // Translate and scale the drawing context
  cairo_translate(cr, width / 2, (height / 2) - 30);
  cairo_scale(cr, scale, scale);
  cairo_set_line_width(cr, 1);
  cairo_set_source_rgb(cr, 1, 1, 1);

  // Draw a square and other shapes
  cairo_rectangle(cr, -10, -10, +20, +20);
  cairo_rectangle(cr, 0, 0, 1, 1);

  // Draw degree markers and labels
  for (int i = 0; i < 40; i++)
  {
    int ii = i - 4;
    int offset = 0;
    if (!(ii % 10))
    {
      offset = 3;
      current_label = labels[label_count++];
      cairo_set_font_size(cr, 14);
      cairo_move_to(cr, -200 + (i * 10) - 10, -160);
      cairo_show_text(cr, current_label);
    }
    cairo_move_to(cr, -200 + (i * 10), -190);
    cairo_line_to(cr, -200 + (i * 10), -185 + offset);
  }

  // Draw crosshair and other elements
  cairo_move_to(cr, -5, -200);
  cairo_line_to(cr, 0, -195);
  cairo_line_to(cr, 5, -200);

  // Additional crosshair lines and telemetry
  // ...

  cairo_stroke(cr);

  // Draw text such as time and telemetry data
  cairo_select_font_face(cr, "Ubuntu Thin", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 24);
  cairo_move_to(cr, -20, 250);
  cairo_show_text(cr, "0025");

  char tstring[200];
  snprintf(tstring, 200, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

  cairo_set_font_size(cr, 14);
  cairo_move_to(cr, -300, -180);
  cairo_show_text(cr, tstring);

  // Telemetry left
  cairo_move_to(cr, -300, -120);
  cairo_show_text(cr, "15°");
  cairo_move_to(cr, -300, -40);
  cairo_show_text(cr, "HORAS - READY");
  cairo_move_to(cr, -300, -20);
  cairo_show_text(cr, "S60");

  // Telemetry right
  cairo_move_to(cr, 270, -120);
  cairo_show_text(cr, "0 KPH");
  cairo_move_to(cr, 270, -40);
  cairo_show_text(cr, "1X");
  cairo_move_to(cr, 270, -20);
  camera_mode(cr, "CAMERA MODE");
}

// Function to set up the GStreamer pipeline
static GstElement *
setup_gst_pipeline(CairoOverlayState *overlay_state)
{
  int width = 640;
  int height = 480;
  int framerate = 30;

  // Create pipeline and elements
  auto *pipeline = gst_pipeline_new("cairo-overlay-example");
#if V4L2_SRC
  auto *source = gst_element_factory_make("v4l2src", "source");
  auto *capabilities = gst_element_factory_make("capsfilter", NULL);
  g_assert(capabilities);
#else // Replace v4l2src with RTP H.264 source
  auto *source = gst_element_factory_make("udpsrc", "source");
  g_object_set(source, "port", 5006, NULL); // Set the UDP port for receiving RTP packets

  // Define caps for RTP H.264
  auto *capabilities = gst_element_factory_make("capsfilter", "capsfilter");
  GstCaps *caps = gst_caps_new_simple(
      "application/x-rtp",
      "media", G_TYPE_STRING, "video",
      "encoding-name", G_TYPE_STRING, "H264",
      "payload", G_TYPE_INT, 96,
      NULL);

  // Add RTP depayloader and H.264 decoder
  auto *rtph264depay = gst_element_factory_make("rtph264depay", "rtph264depay");
  auto *h264decoder = gst_element_factory_make("vaapih264dec", "h264decoder");
#endif
  auto *adaptor1 = gst_element_factory_make("videoconvert", "adaptor1");
  auto *cairo_overlay = gst_element_factory_make("cairooverlay", "overlay");
  auto *adaptor2 = gst_element_factory_make("videoconvert", "adaptor2");
  auto *sink = gst_element_factory_make("ximagesink", "sink");

  // Ensure all elements are created
  g_assert(source);
  g_assert(rtph264depay);
  g_assert(h264decoder);
  g_assert(adaptor1);
  g_assert(cairo_overlay);
  g_assert(adaptor2);
  g_assert(sink);

  // Set sync=false on ximagesink to disable synchronization
  g_object_set(sink, "sync", FALSE, NULL);

  if (sink == NULL)
    sink = gst_element_factory_make("autovideosink", "sink");

  // Ensure cairo overlay is created
  g_assert(cairo_overlay);

  // Set caps for the camera source
  auto *caps_camera = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, width, "height", G_TYPE_INT, height,
                                          "framerate", GST_TYPE_FRACTION, framerate, 1, NULL);

  // Connect signals for cairo overlay
  g_signal_connect(cairo_overlay, "draw", G_CALLBACK(draw_overlay), overlay_state);
  g_signal_connect(cairo_overlay, "caps-changed", G_CALLBACK(prepare_overlay), overlay_state);

  // Add elements to the pipeline and link them
#if V4L2_SRC
  gst_bin_add_many(GST_BIN(pipeline), source, capabilities, adaptor1, cairo_overlay, adaptor2, sink, NULL);
  if (!gst_element_link_many(source, capabilities, adaptor1, cairo_overlay, adaptor2, sink, NULL))
  {
    g_warning("Failed to link elements!");
  }
#else
  gst_bin_add_many(GST_BIN(pipeline), source, capabilities, rtph264depay, h264decoder, adaptor1, cairo_overlay, adaptor2, sink, NULL);
  if (!gst_element_link_many(source, capabilities, rtph264depay, h264decoder, adaptor1, cairo_overlay, adaptor2, sink, NULL))
  {
    g_warning("Failed to link elements!");
  }

#endif

  return pipeline;
}

// Main function
int main(int argc, char **argv)
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstBus *bus;
  CairoOverlayState *overlay_state;

  // Initialize GStreamer
  gst_init(&argc, &argv);
  loop = g_main_loop_new(NULL, FALSE);

  // Allocate overlay state
  overlay_state = g_new0(CairoOverlayState, 1);

  // Set up the pipeline
  pipeline = setup_gst_pipeline(overlay_state);

  // Set up the bus to handle messages
  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_signal_watch(bus);
  g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
  gst_object_unref(GST_OBJECT(bus));

  // Start the pipeline
  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  g_main_loop_run(loop);

  // Clean up
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  g_free(overlay_state);

  return 0;
}