# doubledragon
This is a gstreamer element which fixes a bug in the sonix c1/c1-pro cameras where buffers are occasionally spliced.

The Sonic C1/C1-pro cameras seem to have a peculiar bug.
When streaming both h264 and mjpeg data from the camera at the same time, two mjpeg buffers will regularly be merged into one buffer.
This happens at a rate which corresponds with the keyframe rate of the h264 stream.

When the buffer merge happens, one buffer of the stream seems to be missing, like if it was dropped.
To find the buffer, we need to look at the end of the previous buffer.
There is a bit of overlap, so that the beginning of the newest buffer hides the end of the older buffer.

## Fix

The fix is to split the double buffer into two right before the second SOI.
We create a GstBuffer with memory obtained from calling `gst_memory_share` on the double buffer, starting at the beginning of the second jpeg.
We also copy over the GQuark, which contains the allocation information for the dma memory.

The GstBuffer containing the double buffer already points to the first jpeg.
We add one buffer interval to the timestamp of the buffer with the newest jpeg.

In order to make sure we send the buffers with ascending timestamps, we need to make sure that we send the first buffer first.
We do this by first saving the second buffer as a pending buffer and letting the first buffer / double buffer through.
Then we send the pending buffer immediately when we get a new buffer.

## Example pipeline


To test the element on the imx6:

```
GST_DEBUG=doubledragon:9 gst-launch-1.0 -v -e v4l2src device=/dev/video0 ! image/jpeg,width=1920,height=1080,framerate=30/1 ! queue max-size-bytes=0 max-size-time=1000000000 max-size-buffers=0 ! doubledragon ! imxvpudec_jpeg !  video/x-raw,format=I420 ! queue max-size-bytes=0 max-size-time=1000000000 max-size-buffers=0 ! imxvpuenc_h264 bitrate=14000 ! queue max-size-bytes=0 max-size-time=1000000000 max-size-buffers=0 ! h264parse  ! mp4mux ! filesink location=/videos/out.mp4
```

To trigger the bug, along with the fix, you also need to run the following pipeline simultaneously with the first one:

```
gst-launch-1.0 -e v4l2src device=/dev/v4l/by-path/platform-ci_hdrc.1-usb-0:1:1.0-video-index2 ! queue ! fakesink
```
