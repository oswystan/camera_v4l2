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
#include <libudev.h>

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
#define MK_FMT_DESC(v) {desc: #v, fmt: v}

typedef struct _camera_format_desc {
    const char* desc;
    __u32 fmt;
} camera_format_desc;

static camera_format_desc g_fmtdesc[] = {
    MK_FMT_DESC(V4L2_PIX_FMT_YUYV),
    MK_FMT_DESC(V4L2_PIX_FMT_MJPEG),
    MK_FMT_DESC(V4L2_PIX_FMT_JPEG),
    MK_FMT_DESC(V4L2_PIX_FMT_H264),
    MK_FMT_DESC(V4L2_PIX_FMT_YVYU),
    MK_FMT_DESC(V4L2_PIX_FMT_UYVY),
    MK_FMT_DESC(V4L2_PIX_FMT_YYUV),
    MK_FMT_DESC(V4L2_PIX_FMT_Y41P),
    MK_FMT_DESC(V4L2_PIX_FMT_GREY),
    MK_FMT_DESC(V4L2_PIX_FMT_Y10BPACK),
    MK_FMT_DESC(V4L2_PIX_FMT_Y16),
    MK_FMT_DESC(V4L2_PIX_FMT_YUV420),
    MK_FMT_DESC(V4L2_PIX_FMT_YVU420),
    MK_FMT_DESC(V4L2_PIX_FMT_NV12),
    MK_FMT_DESC(V4L2_PIX_FMT_NV21),
    MK_FMT_DESC(V4L2_PIX_FMT_NV16),
    MK_FMT_DESC(V4L2_PIX_FMT_NV61),
    MK_FMT_DESC(V4L2_PIX_FMT_SPCA501),
    MK_FMT_DESC(V4L2_PIX_FMT_SPCA505),
    MK_FMT_DESC(V4L2_PIX_FMT_SPCA508),
    MK_FMT_DESC(V4L2_PIX_FMT_SGBRG8),
    MK_FMT_DESC(V4L2_PIX_FMT_SGRBG8),
    MK_FMT_DESC(V4L2_PIX_FMT_SBGGR8),
    MK_FMT_DESC(V4L2_PIX_FMT_SRGGB8),
    MK_FMT_DESC(V4L2_PIX_FMT_RGB24),
    MK_FMT_DESC(V4L2_PIX_FMT_BGR24),
};

static const char* camera_get_fmtdesc(__u32 fmt) {
    unsigned int i=0;
    for (i = 0; i < sizeof(g_fmtdesc)/sizeof(g_fmtdesc[0]); i++) {
        if (fmt == g_fmtdesc[i].fmt) {
            return g_fmtdesc[i].desc;
        }
    }
    return "*UNKNOWN*";
}

static void camera_dump_dev(camera_dev_t* d) {
    logv("dev: %s", d->path);
    logv("\tvendor : %x", d->vendor);
    logv("\tproduct: %x", d->product);
    logv("\tbus    : %lu", d->busnum);
    logv("\tdev    : %lu", d->devnum);
}

static int camera_mmap(camera_t* dev) {
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
    int i;
    camera_frame_t* frm = dev->bufs;
    for (i = 0; i < BUF_CNT; i++) {
        if (frm[i].data) {
            int ret = munmap(frm[i].data, frm[i].buf.length);
            if (ret != 0) {
                loge("fail to unmap buffer: %s", strerror(errno));
            }
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
    }
    return camera_mmap(dev);
}

static int camera_mfree(camera_t* dev) {
    struct v4l2_requestbuffers req;
    int ret = 0;

    //unmap buffer first
    camera_munmap(dev);

    memset(&req, 0x00, sizeof(req));
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        loge("fail to free buffer [%s]", strerror(errno));
        return errno;
    }
    return 0;
}

static int camera_qbufs(camera_t* dev) {
    int i;
    for (i = 0; i < BUF_CNT; i++) {
        camera_queue_frame(dev, &dev->bufs[i]);
    }
}

int camera_enum_devices(camera_dev_t devs[], int* cnt) {
    int idx = 0;
    int total = *cnt;

    struct udev* udev = NULL;
    struct udev_enumerate*  enumerate = NULL;
    struct udev_list_entry* devices = NULL;
    struct udev_list_entry* dev_list_entry = NULL;
    struct udev_device *dev = NULL;
    struct v4l2_capability v4l2_cap;

    *cnt = 0;

    udev = udev_new();
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "video4linux");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path, *dev_path;
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        dev_path = udev_device_get_devnode(dev);
        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if (!dev) {
            continue;
        }

        if (idx < total) {
            snprintf(devs[idx].path, sizeof(devs[idx].path), "%s", dev_path);
            devs[idx].vendor = strtoull(udev_device_get_sysattr_value(dev,"idVendor"), NULL, 16);
            devs[idx].product = strtoull(udev_device_get_sysattr_value(dev,"idProduct"), NULL, 16);
            devs[idx].busnum = strtoull(udev_device_get_sysattr_value(dev, "busnum"), NULL, 10);
            devs[idx].devnum = strtoull(udev_device_get_sysattr_value(dev, "devnum"), NULL, 10);
            camera_dump_dev(devs+idx);
        } else {
            loge("more uvc device found: %s", dev_path);
        }
        udev_device_unref(dev);
        ++idx;
    }
    *cnt = idx;

    if (enumerate) {
        udev_enumerate_unref(enumerate);
    }
    if (udev) {
        udev_unref(udev);
    }
    return 0;
}

camera_t* camera_open(const char* dev) {
    logi("open camera %s", dev);
    camera_t* c = (camera_t*)malloc(sizeof(*c));
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
    if (fmt.fmt.pix.pixelformat != pixel_fmt || fmt.fmt.pix.width != w ||
            fmt.fmt.pix.height != h) {
        loge("try configuration[%dx%d@%s] instead of[%dx%d@%s]",
                fmt.fmt.pix.width, fmt.fmt.pix.height, camera_get_fmtdesc(fmt.fmt.pix.pixelformat),
                w, h, camera_get_fmtdesc(pixel_fmt));
        return -1;
    }

    dev->width  = w;
    dev->height = h;
    dev->format = pixel_fmt;
    return 0;
}

int camera_get_format(camera_t* dev, unsigned int* w, unsigned int* h, unsigned int* fmt) {
    check_dev(dev);

    *w = dev->width;
    *h = dev->height;
    *fmt = dev->format;
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
    return camera_mfree(dev);
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

uint64_t camera_get_us(struct timeval ts) {
    return (ts.tv_sec * 1000000UL) + ts.tv_usec;
}
uint64_t camera_get_ms(struct timeval ts) {
    return (ts.tv_sec * 1000UL) + (ts.tv_usec / 1000);
}
/********************************** END **********************************************/

