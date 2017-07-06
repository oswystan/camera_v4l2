/*
 **************************************************************************************
 *       Filename:  camera.h
 *    Description:   header file
 *
 *        Version:  1.0
 *        Created:  2017-07-06 11:43:22
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */

#ifndef CAMERA_H_INCLUDED
#define CAMERA_H_INCLUDED

#include <linux/videodev2.h>

#define BUF_CNT 6

typedef struct _camera_frame_t {
    int     idx;
    void*   data;
    struct v4l2_buffer buf;
} camera_frame_t;

typedef struct _camera_t {
    int fd;
    char name[64];
    camera_frame_t bufs[BUF_CNT];
} camera_t;


camera_t* camera_open(const char* dev);
void camera_close(camera_t* dev);

int camera_set_format(camera_t* dev, unsigned int w, unsigned int h, unsigned int fmt);
int camera_set_framerate(camera_t* dev, unsigned int fps);

int camera_streamon(camera_t* dev);
int camera_streamoff(camera_t* dev);

int camera_dqueue_frame(camera_t* dev, camera_frame_t* frame);
int camera_queue_frame(camera_t* dev, camera_frame_t* frame);

#endif /*CAMERA_H_INCLUDED*/

/********************************** END **********************************************/

