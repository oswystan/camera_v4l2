/*
 **************************************************************************************
 *       Filename:  main.c
 *    Description:   source file
 *
 *        Version:  1.0
 *        Created:  2017-07-06 11:35:43
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include "camera.h"

int main(int argc, const char *argv[]) {
    int i = 0;
    int ret = 0;

    camera_t* c = camera_open("/dev/video0");
    if (!c) {
        return -1;
    }

    camera_set_format(c, 640, 480, V4L2_PIX_FMT_MJPEG);
    camera_set_framerate(c, 30);
    camera_streamon(c);

    camera_frame_t frame;
    for (i = 0; i < 10; i++) {
        memset(&frame, 0x00, sizeof(frame));
        ret = camera_dqueue_frame(c, &frame);
        ret = camera_queue_frame(c, &frame);
    }

    camera_streamoff(c);
    camera_close(c);
    return 0;
}

/********************************** END **********************************************/

