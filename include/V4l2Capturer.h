/**
 * @file V4l2Capturer.h
 * @author Alex.D.Scofield (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2021-09-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*onFrameCb)(const void *data, const size_t size);

typedef enum
{
    V4L2_CAP_FMT_H264 = 0,
    V4L2_CAP_FMT_YUYV
} V4l2CapturerFormat;

typedef void *V4l2CapturerHandle;

/**
 * @brief Open selected v4l2 device.
 *
 * @param[in] devName V4l2 device to open. i.e. /dev/video0.
 * @return V4l2CapturerHandle Capturer handle for further operations. It can be NULL if error occurs.
 */
V4l2CapturerHandle v4l2CapturerOpen(const char *devName);

/**
 * @brief Configure capturer formats.
 *
 * @param[in] handle Capturer handle.
 * @param[in] width Width of captured frame.
 * @param[in] height Height of captured frame.
 * @param[in] format Frame format.
 * @param[in] bitrate target bitrate(bps).
 * @return int 0 or error code.
 */
int v4l2CapturerConfig(V4l2CapturerHandle handle, const uint32_t width, const uint32_t height,
                       const V4l2CapturerFormat format, const size_t bitrate);

/**
 * @brief Set onFrame callback.
 *
 * @param[in] handle Capturer handle.
 * @param[in] onFrame onFrame callback.
 * @return int 0 or error code.
 */
int v4l2CapturerSetOnFrameCallback(V4l2CapturerHandle handle, const onFrameCb onFrame);

/**
 * @brief Start streaming.
 *
 * @param[in] capturer Capturer handle.
 * @return int 0 or error code.
 */
int v4l2CapturerStartStreaming(V4l2CapturerHandle handle);

/**
 * @brief Stop streaming.
 *
 * @param[in] handle Capturer handle.
 * @return int 0 or error code.
 */
int v4l2CapturerStopStreaming(V4l2CapturerHandle handle);

/**
 * @brief Run capturer and get frame in async mode. In streaming case, user need to call this in a loop. If onFrameCb
 * was set, this method will fire onFrameCb.
 *
 * @param[in] handle Capturer handle.
 * @param[in] timeoutSec Operation timeout in seconds.
 * @return int 0 or error code.
 */
int v4l2CapturerRun(V4l2CapturerHandle handle, const int timeoutSec);

/**
 * @brief Get frame in sync mode(blocking). This method will not fire onFrameCb.
 *
 * @param[in] handle Capturer handle.
 * @param[in] timeoutSec Operation timeout in seconds.
 * @param[in,out] frameDataBuffer Target frame data buffer.
 * @param[in] frameDataBufferSize Target frame data buffer size in bytes.
 * @param[out] frameSize Frame data size in bytes
 * @return int 0 or error code.
 */
int v4l2CapturerSyncGetFrame(V4l2CapturerHandle handle, const int timeoutSec, void *frameDataBuffer,
                             const size_t frameDataBufferSize, size_t *frameSize);

/**
 * @brief Close capturer and release resources.
 *
 * @param[in] handle Capturer handle.
 * @return int 0 or error code.
 */
int v4l2CapturerClose(V4l2CapturerHandle handle);

#ifdef __cplusplus
}
#endif
