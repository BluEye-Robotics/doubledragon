# doubledragon
Gstreamer element which fixes a bug in the sonix c1/c1-pro cameras where buffers are occasionally spliced

The Sonic C1/C1-pro cameras seem to have a peculiar bug.
When streaming both h264 and mjpeg data from the camera at the same time, two mjpeg buffers will regularly be merged into one buffer.
This happens at a rate which corresponds with the keyframe rate of the h264 stream.

When the buffer merge happens, one buffer of the stream seems to be missing, like if it was dropped.
To find the buffer, we need to look at the end of the previous buffer.
There is a bit of overlap, so that the beginning of the newest buffer hides the end of the older buffer.

## Fix

The fix is to split the double buffer into two right before the second SOI.
