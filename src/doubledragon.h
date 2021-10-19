
#ifndef __GST_DOUBLEDRAGON_H__
#define __GST_DOUBLEDRAGON_H__

#include <gst/gst.h>

#include <gst/base/gstbasetransform.h>

#include <stdbool.h>
#include <stdint.h>

G_BEGIN_DECLS

#define GST_TYPE_DOUBLEDRAGON \
  (gst_doubledragon_get_type())
#define GST_DOUBLEDRAGON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DOUBLEDRAGON,GstDoubledragon))
#define GST_DOUBLEDRAGON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DOUBLEDRAGON,GstDoubledragonClass))
#define GST_IS_DOUBLEDRAGON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DOUBLEDRAGON))
#define GST_IS_DOUBLEDRAGON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DOUBLEDRAGON))

typedef struct _GstDoubledragon GstDoubledragon;
typedef struct _GstDoubledragonClass GstDoubledragonClass;

struct _GstDoubledragon
{
  GstBaseTransform basetransform;

  /* < private > */
  GstBuffer * pending;

  GstFlowReturn (*default_generate_output) (GstBaseTransform*, GstBuffer**);
};

struct _GstDoubledragonClass
{
  GstBaseTransformClass parent_class;
};

GType gst_doubledragon_get_type (void);

G_END_DECLS

#endif /* __GST_DOUBLEDRAGON_H__ */
