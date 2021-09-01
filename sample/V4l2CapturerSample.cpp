/**
 * @file sample.cpp
 * @author Alex.D.Scofield (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2021-09-01
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "V4l2Capturer.h"

static void printHelp(void)
{
    std::cerr << "usage: V4l2CapturerSample <device> <frames>" << std::endl;
    std::cerr << "i.e: V4l2CapturerSample /dev/video0 10" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printHelp();
        return -EINVAL;
    }

    V4l2CapturerHandle handle = v4l2CapturerOpen(argv[1]);

    if (!handle)
    {
        return -EFAULT;
    }

    v4l2CapturerConfig(handle, 1920, 1080, V4L2_CAP_FMT_H264);

    v4l2CapturerSetOnFrameCallback(handle, [](const void *data, const size_t size) {
        static size_t frameCount = 1;
        std::stringstream fileName;
        fileName << "frame-" << std::setfill('0') << std::setw(4) << frameCount << ".h264";
        std::ofstream stream;
        stream.open(fileName.str());
        stream.write(static_cast<const char *>(data), size);
        stream.close();
        frameCount++;
    });

    if (v4l2CapturerStartStreaming(handle))
    {
        v4l2CapturerClose(handle);
        return -EFAULT;
    }

    auto capturerThread = std::make_unique<std::thread>([&]() {
        size_t frames = 0;
        try
        {
            frames = std::stoi(argv[2]);
        }
        catch (const std::exception &e)
        {
            printHelp();
            return;
        }
        for (int i = 0; i < frames; ++i)
        {
            v4l2CapturerRun(handle, 2);
        }
    });

    if (capturerThread->joinable())
    {
        capturerThread->join();
    }

    v4l2CapturerStopStreaming(handle);
    return v4l2CapturerClose(handle);
}
