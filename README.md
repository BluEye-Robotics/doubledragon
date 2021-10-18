# doubledragon
Gstreamer element which fixes a bug in the sonix c1/c1-pro cameras where frames are occasionally spliced

The Sonic C1/C1-pro cameras seem to have a peculiar bug.
When streaming both h264 and mjpeg data from the camera at the same time, two mjpeg frames will regularly be merged into one frame.
This happens at a rate which corresponds with the keyframe rate of the h264 stream.

When the frame merge happens, one frame of the stream seems to be missing, like if it was dropped.
To find the frame, we need to look at the end of the previous frame.
There is a bit of overlap, so that the beginning of the newest frame hides the end of the older frame.
