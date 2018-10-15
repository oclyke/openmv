# INRIA Person Detection CNN Example
#
# This example shows off how to use the OpenMV Cam to detect a
# person in a scene using a CNN trained on the INRIA dataset.

import sensor, image, time, os, nn

sensor.reset()                         # Reset and initialize the sensor.
sensor.set_pixformat(sensor.RGB565)    # Set pixel format to RGB565
sensor.set_framesize(sensor.QVGA)      # Set frame size to QVGA (320x240)
sensor.set_windowing((64, 128))        # Set 64x128 window.
sensor.skip_frames(time=500)
sensor.set_auto_gain(False)
sensor.set_auto_exposure(False)

# Load cnrpark network
net = nn.load('/inria.network')
labels = ['Y', 'N']

clock = time.clock()                # Create a clock object to track the FPS.
while(True):
    clock.tick()                    # Update the FPS clock.
    img = sensor.snapshot()         # Take a picture and return the image.
    out = net.forward(img, softmax=True)
    max_idx = out.index(max(out))
    score = int(out[max_idx]*100)
    if (score < 70):
        score_str = "??:??%"
    else:
        score_str = "%s:%d%% "%(labels[max_idx], score)
    img.draw_string(0, 0, score_str, color=(255, 0, 0))

    print(clock.fps())             # Note: OpenMV Cam runs about half as fast when connected
                                   # to the IDE. The FPS should increase once disconnected.
