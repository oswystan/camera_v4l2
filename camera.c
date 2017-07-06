/*
 **************************************************************************************
 *       Filename:  camera.c
 *    Description:   source file
 *
 *        Version:  1.0
 *        Created:  2017-07-06 11:43:26
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "camera.h"

#ifndef NULL
#define NULL 0
#endif

#define LOG_TAG "camera"
#define logv(fmt, ...) printf("[V/"LOG_TAG"]"fmt"\n", ##__VA_ARGS__)
#define logi(fmt, ...) printf("[I/"LOG_TAG"]"fmt"\n", ##__VA_ARGS__)
#define logw(fmt, ...) printf("[W/"LOG_TAG"]"fmt"\n", ##__VA_ARGS__)
#define loge(fmt, ...) printf("[E/"LOG_TAG"][%s@%d]"fmt"\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define check_dev(d) \
    if(!dev) { \
        loge("null dev"); \
        return -1; \
    }

static int camera_mmap(camera_t* dev) {
    check_dev(dev);

    int i;
    camera_frame_t* frm = dev->bufs;
    for (i = 0; i < BUF_CNT; i++) {
        frm[i].data = mmap(NULL, frm[i].buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, frm[i].buf.m.offset);
        if ( frm[i].data == MAP_FAILED) {
            loge("fail to map buffer [%s]", strerror(errno));
            return errno;
        }
    }

    return 0;
}
static int camera_munmap(camera_t* dev) {
    check_dev(dev);
    int i;
    camera_frame_t* frm = dev->bufs;
    for (i = 0; i < BUF_CNT; i++) {
        if (frm[i].data) {
            munmap(frm[i].data, frm[i].buf.length);
            frm[i].data = NULL;
        }
    }

    return 0;
}

static int camera_malloc(camera_t* dev) {
    struct v4l2_requestbuffers req;
    int ret = 0;

    memset(&req, 0x00, sizeof(req));
    req.count = BUF_CNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        loge("fail to request buffer [%s]", strerror(errno));
        return errno;
    }

    int i;
    camera_frame_t* frm = dev->bufs;
    for (i = 0; i < BUF_CNT; i++) {
        memset(&frm[i].buf, 0x00, sizeof(frm[i].buf));
        frm[i].buf.index = i;
        frm[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        frm[i].buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(dev->fd, VIDIOC_QUERYBUF, &frm[i].buf);
        if (ret < 0) {
            loge("fail to query buffer [%s]", strerror(errno));
            return errno;
        }
        logv("mem[%d]=%d, %d", i, frm[i].buf.length, frm[i].buf.m.offset);
    }
    return camera_mmap(dev);
}

static int camera_qbufs(camera_t* dev) {
    int i;
    for (i = 0; i < BUF_CNT; i++) {
        camera_queue_frame(dev, &dev->bufs[i]);
    }
}

camera_t* camera_open(const char* dev) {
    logi("open camera %s", dev);
    camera_t* c = malloc(sizeof(*c));
    if (!c) {
        loge("fail to alloc memory");
        return NULL;
    }
    memset(c, 0x00, sizeof(*c));
    c->fd = open(dev, O_RDWR);
    if (-1 == c->fd) {
        loge("fail to open dev: %s[%s]", dev, strerror(errno));
        free(c);
        return NULL;
    }
    snprintf(c->name, sizeof(c->name), "%s", dev);
    return c;
}

void camera_close(camera_t* dev) {
    if (dev) {
        logi("close dev %s", dev->name);
        if (dev->fd > 0) {
            close(dev->fd);
        }
        free(dev);
    }
}

int camera_set_format(camera_t* dev, unsigned int w, unsigned int h, unsigned int pixel_fmt) {
    check_dev(dev);
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = w;
    fmt.fmt.pix.height      = h;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    fmt.fmt.pix.pixelformat = pixel_fmt;
    int ret = ioctl(dev->fd, VIDIOC_S_FMT, &fmt);
    if (ret < 0) {
        loge("fail to set format [%s]", strerror(errno));
        return errno;
    }

    /* double check */
    ret = ioctl(dev->fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        loge("fail to set format [%s]", strerror(errno));
        return errno;
    }
    if (fmt.fmt.pix.pixelformat != pixel_fmt) {
        loge("pixel format is not supported");
        return -1;
    }
    return 0;
}

int camera_set_framerate(camera_t* dev, unsigned int fps) {
    check_dev(dev);

    struct v4l2_streamparm para;
    int ret = 0;
    memset(&para, 0x00, sizeof(para));
    para.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(dev->fd, VIDIOC_G_PARM, &para);
    if (ret < 0) {
        loge("fail to get param [%s]", strerror(errno));
        return errno;
    }
    para.parm.capture.timeperframe.numerator = 1;
    para.parm.capture.timeperframe.denominator = fps;

    ret = ioctl(dev->fd, VIDIOC_S_PARM, &para);
    if (ret < 0) {
        loge("fail to set frame rate [%s]", strerror(errno));
        return errno;
    }
    return 0;
}

int camera_streamon(camera_t* dev) {
    check_dev(dev);

    int ret = 0;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if ((ret = camera_malloc(dev)) != 0) {
        return ret;
    }
    if ((ret = camera_mmap(dev)) != 0) {
        camera_munmap(dev);
        return ret;
    }
    camera_qbufs(dev);

    ret = ioctl(dev->fd, VIDIOC_STREAMON, &type);
    if (ret != 0) {
        loge("fail to stream on [%s]", strerror(errno));
        return errno;
    }

    return 0;
}

int camera_streamoff(camera_t* dev) {
    check_dev(dev);
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
    if (ret != 0) {
        loge("fail to stream off [%s]", strerror(errno));
        return errno;
    }
    return 0;
}

int camera_dqueue_frame(camera_t* dev, camera_frame_t* frame) {
    check_dev(dev);
    frame->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frame->buf.memory = V4L2_MEMORY_MMAP;
    int ret = ioctl(dev->fd, VIDIOC_DQBUF, &frame->buf);
    if (ret != 0) {
        loge("fail to dqueue buffer [%s]", strerror(errno));
        return errno;
    }
    /*logv("DQ => %d", frame->buf.index);*/
    frame->data = dev->bufs[frame->buf.index].data;
    return 0;
}

int camera_queue_frame(camera_t* dev, camera_frame_t* frame) {
    check_dev(dev);
    int ret = ioctl(dev->fd, VIDIOC_QBUF, &frame->buf);
    if (ret != 0) {
        loge("fail to queue buffer %d [%s]", frame->buf.index, strerror(errno));
        return errno;
    }
    /*logv("Q  => %d", frame->buf.index);*/
    return 0;
}

/********************************** END **********************************************/

