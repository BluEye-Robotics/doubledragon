/**
 * SECTION:element-gst_doubledragon
 *
 * Doubledragon is a texttransform that uses memory mapping for vpu buffers
 *
 *  Jpeg buffer get delayed when they collide with h264 Iframes. They combine with the next buffer and they
 *  make a jpeg with twice the size of what's usual.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
  gst-launch-1.0 -v -e imxv4l2videosrc device=/dev/video1 input=0 fps=30/1 ! doubledragon ! image/jpeg,width=1920,height=1080,framerate=30/1 ! queue max-size-bytes=0 max-size-time=0 max-size-buffers=0 ! imxvpudec !  video/x-raw,format=I420 ! queue max-size-bytes=0 max-size-time=0 max-size-buffers=0 ! imxvpuenc_h264 bitrate=14000 ! h264parse  ! mp4mux ! filesink location=/videos/out.mp4
 * </refsect2>
 */

#define VERSION "1.0"
#define PACKAGE "gst_doubledragon"
#define GST_PACKAGE_NAME PACKAGE
#define GST_PACKAGE_ORIGIN "Norway"

#include "doubledragon.h"

#include <stdio.h>
#include <sys/time.h>

inline double ms()
{
  struct timeval tp;
  gettimeofday(&tp, NULL);

  double ms = (double)(tp.tv_sec)*1000 + (double)(tp.tv_usec)/1000;
  return ms;
}

GST_DEBUG_CATEGORY_STATIC(imx_v4l2_buffer_pool_debug);
#define GST_CAT_DEFAULT imx_v4l2_buffer_pool_debug

#define gst_doubledragon_parent_class parent_class
G_DEFINE_TYPE (GstDoubledragon, gst_doubledragon, GST_TYPE_BASE_TRANSFORM);

#define CAPS_STR "image/jpeg"

static GstStaticPadTemplate gst_doubledragon_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_doubledragon_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static int
gst_doubledragon_expected_size (GstDoubledragon * dragon, int size)
{
  int expected_size;

  // Pseudo median_filter on the last N_SIZES buffer sizes
  #define N_SIZES 3
  #define DEFAULT_EXPECTED_SIZE 250000
  #define SWAP(a,b) do {  \
    typeof(a) tmp = a;    \
    a = b;                \
    b = tmp;              \
  }while(0)
  static int sizes[N_SIZES+1] = {0};
  static int size_index = 0;

  sizes[size_index] = size; // insert new size at size_index

  for (int i=size_index+1; i < N_SIZES; ++i)
    if ((sizes[i] < sizes[i-1]) || (sizes[i] == 0))
      SWAP(sizes[i], sizes[i-1]);

  for (int i=size_index-1; i >= 0; --i)
    if ((sizes[i] > sizes[i+1]) || (sizes[i] == 0))
      SWAP(sizes[i], sizes[i+1]);

  size_index = (size_index + 1) % N_SIZES; // size_index goes through sorted list incrementally instead of proper FIFO
  const int median_size = sizes[(N_SIZES/2) + (N_SIZES % 2) - 1];

  expected_size = median_size ?: DEFAULT_EXPECTED_SIZE;
  GST_DEBUG_OBJECT(dragon, "%d\t%d\t%d\t%d\n", expected_size, sizes[0], sizes[1], sizes[2]);
  #undef N_SIZES
  #undef DEFAULT_EXPECTED_SIZE
  #undef SWAP

  return expected_size;
}

static int
gst_doubledragon_find_soi (GstDoubledragon * dragon, const guint8 * mapped, const int size)
{
  // time profiling:
  //static double start;
  //start = ms();

  int soi = 0;

  GST_DEBUG_OBJECT(dragon, "SOI: %x\t%x\n", mapped[0], mapped[1]);

  if ((mapped[0] == 0xff) && (mapped[1] == 0xd8)) // sanity check for invalid jpg buffers
  {
    for (int i = (5*size)/11; i < (6 * size)/11; ++i) // we search in a limited interval around expected_size
    {
      if ((mapped[i] == 0xff) && (mapped[i+1] == 0xd8))
      {
        soi = i;
        break;
      }
    }
  }

	//GST_WARNING_OBJECT(dragon, "%d\t%f", size, ms() - start);

  if (!soi)
	  GST_WARNING_OBJECT(dragon, "NO SOI");

  return soi;
}

static GstFlowReturn
gst_doubledragon_transform_ip (GstBaseTransform * basetransform, GstBuffer * buf)
{
  GstDoubledragon *dragon = GST_DOUBLEDRAGON (basetransform);
  guint8 *nsrc;
  GstClockTime timestamp, stream_time;

  timestamp = GST_BUFFER_TIMESTAMP (buf);
  stream_time =
      gst_segment_to_stream_time (&GST_BASE_TRANSFORM (basetransform)->segment,
      GST_FORMAT_TIME, timestamp);

  GST_DEBUG_OBJECT (dragon, "sync to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (dragon), stream_time);

  //GstMemory* mem = gst_buffer_get_memory(buf, 0);
  //printf("\t\t\tsize: %zu\n", mem->size);

  //GstV4l2Memory* mem = gst_mini_object_get_qdata(GST_MINI_OBJECT(buf), g_quark_from_static_string ("GstV4l2Memory"));

  //printf("\t\t\tsize: %zu\n", gst_buffer_get_size(buf));
  GST_DEBUG_OBJECT(dragon, "\t\t\tdts: %llu, pts: %llu", buf->dts, buf->pts);

  //static double start = 0;
  //GST_WARNING_OBJECT(dragon, "%f", ms() - start);
  //start = ms();

  GST_OBJECT_LOCK (dragon);

  if (dragon->pending)
  {
    gst_pad_push(basetransform->srcpad, dragon->pending);
    dragon->pending = NULL;
  }

  int size = gst_buffer_get_size(buf);
  const int expected_size = gst_doubledragon_expected_size (dragon, size);
  GST_DEBUG_OBJECT(dragon, "\t\t\tsize: %d\texpected_size: %d", size, expected_size);

  if (size > (3*expected_size)/2)
  {
    GST_WARNING_OBJECT(dragon, "duplicate buffer with size: %d", size);

    GstMapInfo info;
    if (!gst_buffer_map(buf, &info, GST_MAP_READ))
      return GST_FLOW_ERROR;

    const guint8* mapped = info.data;

    int soi = gst_doubledragon_find_soi(dragon, mapped, size);

    if (soi)
    {
	    GST_WARNING_OBJECT(dragon, "Found SOI at %d", soi);

      //FILE *jpg = fopen("/data/out.jpg", "wb");
      //fwrite(mapped, size, 1, jpg);
      //fclose(jpg);

      GstMemory* mem = gst_buffer_get_memory(buf, 0);

      GstMemory * dup_mem = gst_memory_share (mem, soi, size - soi);

      GstBuffer* dup = gst_buffer_new();

	    gst_buffer_remove_all_memory(dup);
      gst_buffer_append_memory(dup, dup_mem);

      dup->pts = buf->pts;
      dup->dts = buf->dts;
      dup->duration = buf->duration;

      dragon->pending = dup;

      // We need to either subtract the duration from the newest buffer, or add it to the oldest, depending on the gstreamer implementation.
      // This behaviour is controlled through DOUBLE_DRAGON_SUBTRACT_TS.
      // To find out which one to choose, compare the timestamps of the double buffer with the one before or after.
      // If the one before is two durations before the double buffer, then we need to substract one duration from the oldest buffer in the double buffer, and vice versa.
      #ifdef DOUBLE_DRAGON_SUBTRACT_TS
      buf->pts = buf->pts > buf->duration ? buf->pts - buf->duration : 0;
      #else
      dup->pts += buf->duration;
      #endif
    }
    else
    {
	    GST_WARNING_OBJECT(dragon, "Found no SOI");
    }

    gst_buffer_unmap(buf, &info);
  }

  GST_OBJECT_UNLOCK (dragon);

  return GST_FLOW_OK;
}

static void
gst_doubledragon_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstDoubledragon *dragon = GST_DOUBLEDRAGON (object);

  GST_OBJECT_LOCK (dragon);
  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (dragon);
}

static void
gst_doubledragon_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDoubledragon *dragon = GST_DOUBLEDRAGON (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_doubledragon_finalize (GObject * object)
{
  GstDoubledragon *dragon = GST_DOUBLEDRAGON (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_doubledragon_class_init (GstDoubledragonClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *) klass;
  GstBaseTransformClass *base_class = (GstBaseTransformClass *) klass;

  gobject_class->finalize     = gst_doubledragon_finalize;
  gobject_class->set_property = gst_doubledragon_set_property;
  gobject_class->get_property = gst_doubledragon_get_property;

  GST_DEBUG_CATEGORY_INIT(imx_v4l2_buffer_pool_debug, "doubledragon", 0, "Fix for double buffer bug in sonix c1/c1-pro cameras");


  gst_element_class_set_static_metadata (gstelement_class, "Doubledragon",
      "Transform",
      "Fix for double buffer bug in sonix c1/c1-pro cameras",
      "Erlend Eriksen <erlend.eriksen@blueye.no>");

  gst_element_class_add_static_pad_template (gstelement_class, &gst_doubledragon_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &gst_doubledragon_src_template);

  basetransform_class->transform_ip = GST_DEBUG_FUNCPTR (gst_doubledragon_transform_ip);

  base_class->passthrough_on_same_caps = TRUE;
}

static void
gst_doubledragon_init (GstDoubledragon * dragon)
{
  dragon->pending = NULL;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "doubledragon", GST_RANK_NONE, gst_doubledragon_get_type()))
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    doubledragon,
          "Fix for double buffer bug in sonix c1/c1-pro cameras",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
