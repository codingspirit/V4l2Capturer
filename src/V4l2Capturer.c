/**
 * @file V4l2Capturer.c
 * @author Alex.D.Scofield (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2021-09-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "V4l2Capturer.h"

#ifndef LOG
#define LOG printf
#endif

#define V4L2_CAPTURER_NULL_CHECK(x)                                                                                    \
    if (!x)                                                                                                            \
    return -EINVAL

#define V4L2_CAPTURER_GET(x)                                                                                           \
    V4L2_CAPTURER_NULL_CHECK(x);                                                                                       \
    V4l2Capturer *capturer = (V4l2Capturer *)(x)

#define V4L2_CAPTURER_BUF_COUNT 2

typedef struct
{
    void *start;
    size_t length;
} V4l2CapturerBuffer;

typedef struct
{
    const char *devName;
    int fd;
    fd_set fds;
    onFrameCb onFrame;
    V4l2CapturerBuffer buffers[V4L2_CAPTURER_BUF_COUNT];
} V4l2Capturer;

static int xioctl(const int fd, const int request, void *arg)
{
    int res;
    do
    {
        res = ioctl(fd, request, arg);
    } while (-EPERM == res && EINTR == errno);
    return res;
}

static bool isValidCaptureDevice(const int fd)
{
    struct v4l2_capability cap = {0};

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap))
    {
        LOG("Not a valid v4l2 device\n");
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOG("Not a capture device\n");
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        LOG("Not support streaming i/o\n");
        return false;
    }

    return true;
}

static int v4l2CapturerAsyncGetFrame(V4l2CapturerHandle handle)
{
    V4L2_CAPTURER_GET(handle);

    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (xioctl(capturer->fd, VIDIOC_DQBUF, &buf))
    {
        LOG("VIDIOC_DQBUF failed\n");
        return -errno;
    }

    if (capturer->onFrame)
    {
        capturer->onFrame(capturer->buffers[buf.index].start, buf.bytesused);
    }

    if (xioctl(capturer->fd, VIDIOC_QBUF, &buf))
    {
        LOG("VIDIOC_QBUF failed\n");
        return -errno;
    }

    return 0;
}

static int v4l2CapturerSelectFD(V4l2CapturerHandle handle, const int timeoutSec)
{
    V4L2_CAPTURER_GET(handle);

    // TODO: maybe move this part out of loop
    FD_ZERO(&capturer->fds);
    FD_SET(capturer->fd, &capturer->fds);

    struct timeval tv = {
        .tv_sec = timeoutSec,
    };

    int res = select(capturer->fd + 1, &capturer->fds, NULL, NULL, &tv);

    if (res == -EPERM)
    {
        if (EINTR == errno)
        {
            return -EAGAIN;
        }
        LOG("Select failed\n");
        return -errno;
    }
    else if (!res)
    {
        LOG("Capturer run timeout\n");
        return -EAGAIN;
    }

    return 0;
}

V4l2CapturerHandle v4l2CapturerOpen(const char *devName)
{
    if (!devName)
    {
        return NULL;
    }

    V4l2Capturer *capturer = (V4l2Capturer *)malloc(sizeof(V4l2Capturer));
    if (!capturer)
    {
        return NULL;
    }

    capturer->devName = devName;
    capturer->fd = open(capturer->devName, O_RDWR | O_NONBLOCK, 0);
    if (capturer->fd == -EPERM)
    {
        LOG("Cannot open '%s': %d, %s\n", capturer->devName, errno, strerror(errno));
    }
    else if (!isValidCaptureDevice(capturer->fd))
    {
        LOG("Device '%s' is not supported\n", capturer->devName);
    }
    else
    {
        LOG("Device '%s' is opened\n", capturer->devName);
        return (V4l2CapturerHandle)capturer;
    }

    if (capturer)
    {
        free(capturer);
    }
    return NULL;
}

int v4l2CapturerConfig(V4l2CapturerHandle handle, const uint32_t width, const uint32_t height,
                       const V4l2CapturerFormat format, const size_t bitrate)
{
    V4L2_CAPTURER_GET(handle);

    // config format
    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                              .fmt = {
                                  .pix =
                                      {
                                          .width = width,
                                          .height = height,
                                          .field = V4L2_FIELD_INTERLACED,
                                      },
                              }};

    switch (format)
    {
    case V4L2_CAP_FMT_H264:
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
        break;
    case V4L2_CAP_FMT_YUYV:
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        break;
    default:
        return -EINVAL;
    }

    if (xioctl(capturer->fd, VIDIOC_S_FMT, &fmt))
    {
        LOG("Format set failed: %d, %s\n", errno, strerror(errno));
        return -errno;
    }

    // config target bitrate
    struct v4l2_control ctrl = {
        .id = V4L2_CID_MPEG_VIDEO_BITRATE,
        .value = bitrate,
    };

    if (xioctl(capturer->fd, VIDIOC_S_CTRL, &ctrl))
    {
        LOG("Bitrate set failed: %d, %s\n", errno, strerror(errno));
        return -errno;
    }

    // config buffer
    struct v4l2_requestbuffers req = {
        .count = V4L2_CAPTURER_BUF_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (xioctl(capturer->fd, VIDIOC_REQBUFS, &req))
    {
        LOG("%s does not support mmap\n", capturer->devName);
        return -errno;
    }
    else if (req.count < V4L2_CAPTURER_BUF_COUNT)
    {
        LOG("Insufficient buffer on %s\n", capturer->devName);
        return -ENOMEM;
    }

    for (uint32_t i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf = {
            .index = i,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };

        if (xioctl(capturer->fd, VIDIOC_QUERYBUF, &buf))
        {
            return -errno;
        }

        capturer->buffers[i].start =
            mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, capturer->fd, buf.m.offset);
        if (MAP_FAILED == capturer->buffers[i].start)
        {
            LOG("mmap failed on %s\n", capturer->devName);
            return -errno;
        }
        capturer->buffers[i].length = buf.length;
    }

    return 0;
}

int v4l2CapturerSetOnFrameCallback(V4l2CapturerHandle handle, const onFrameCb onFrame)
{
    V4L2_CAPTURER_GET(handle);

    capturer->onFrame = onFrame;

    return 0;
}

int v4l2CapturerStartStreaming(V4l2CapturerHandle handle)
{
    V4L2_CAPTURER_GET(handle);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (uint32_t i = 0; i < V4L2_CAPTURER_BUF_COUNT; ++i)
    {
        struct v4l2_buffer buf = {
            .index = i,
            .type = type,
            .memory = V4L2_MEMORY_MMAP,
        };

        if (xioctl(capturer->fd, VIDIOC_QBUF, &buf))
        {
            LOG("Failed to start stream on %s: %d, %s\n", capturer->devName, errno, strerror(errno));
            return -errno;
        }
    }

    if (xioctl(capturer->fd, VIDIOC_STREAMON, &type))
    {
        LOG("Failed to start stream on %s: %d, %s\n", capturer->devName, errno, strerror(errno));
        return -errno;
    }

    LOG("Started stream on %s\n", capturer->devName);
    return 0;
}

int v4l2CapturerStopStreaming(V4l2CapturerHandle handle)
{
    V4L2_CAPTURER_GET(handle);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(capturer->fd, VIDIOC_STREAMOFF, &type))
    {
        LOG("Failed to stop stream on %s: %d, %s\n", capturer->devName, errno, strerror(errno));
        return -errno;
    }

    LOG("Stopped stream on %s\n", capturer->devName);
    return 0;
}

int v4l2CapturerRun(V4l2CapturerHandle handle, const int timeoutSec)
{
    int res = v4l2CapturerSelectFD(handle, timeoutSec);

    if (res)
    {
        LOG("Failed to select: %d", res);
        return res;
    }

    return v4l2CapturerAsyncGetFrame(handle);
}

int v4l2CapturerSyncGetFrame(V4l2CapturerHandle handle, const int timeoutSec, void *frameDataBuffer,
                             const size_t frameDataBufferSize, size_t *frameSize)
{
    V4L2_CAPTURER_NULL_CHECK(frameDataBuffer);
    V4L2_CAPTURER_NULL_CHECK(frameSize);
    V4L2_CAPTURER_GET(handle);

    int res = 0;

    if (res = v4l2CapturerSelectFD(handle, timeoutSec))
    {
        LOG("Failed to select: %d", res);
        return res;
    }

    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (xioctl(capturer->fd, VIDIOC_DQBUF, &buf))
    {
        LOG("VIDIOC_DQBUF failed\n");
        return -errno;
    }

    if (buf.bytesused > frameDataBufferSize)
    {
        res = -ENOMEM;
    }
    else
    {
        memcpy(frameDataBuffer, capturer->buffers[buf.index].start, buf.bytesused);
        *frameSize = buf.bytesused;
    }

    if (xioctl(capturer->fd, VIDIOC_QBUF, &buf))
    {
        LOG("VIDIOC_QBUF failed\n");
        return -errno;
    }

    return res;
}

int v4l2CapturerClose(V4l2CapturerHandle handle)
{
    V4L2_CAPTURER_GET(handle);

    for (int i = 0; i < V4L2_CAPTURER_BUF_COUNT; ++i)
    {
        munmap(capturer->buffers[i].start, capturer->buffers[i].length);
    }

    if (close(capturer->fd))
    {
        LOG("Cannot close '%s': %d, %s\n", capturer->devName, errno, strerror(errno));
        return -errno;
    }

    LOG("Device '%s' is closed\n", capturer->devName);

    free(capturer);

    return 0;
}
