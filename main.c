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


int save(void* data, int size) {
    static int i = 0;
    char fn[128];
    snprintf(fn, sizeof(fn), "./save-%03d.jpg", i++);
    FILE* fp = fopen(fn, "w");
    if (fp) {
        fwrite(data, 1, size, fp);
        fclose(fp);
    }
}

unsigned long timeval2ul(struct timeval ts) {
    return (ts.tv_sec * 1000) + (ts.tv_usec / 1000);
}

int main(int argc, const char *argv[]) {
    int i = 0;
    int ret = 0;

    unsigned long last_ts = 0;
    unsigned long cur_ts = 0;

    camera_t* c = camera_open("/dev/video0");
    if (!c) {
        return -1;
    }

    camera_set_format(c, 640, 480, V4L2_PIX_FMT_MJPEG);
    camera_set_framerate(c, 30);
    camera_streamon(c);

    camera_frame_t frame;
    for (i = 0; i < 100; i++) {
        memset(&frame, 0x00, sizeof(frame));
        ret = camera_dqueue_frame(c, &frame);
        cur_ts = timeval2ul(frame.buf.timestamp);

        printf("ts=%lu\n", cur_ts - last_ts);
        last_ts = cur_ts;
        /*save(frame.data, frame.buf.length);*/
        ret = camera_queue_frame(c, &frame);
    }

    camera_streamoff(c);
    camera_close(c);
    return 0;
}

/********************************** END **********************************************/

