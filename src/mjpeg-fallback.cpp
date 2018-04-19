/* Plugin implementation for Mjpeg
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */

#include <config.h>
#include "mjpeg-fallback.hpp"

#include <spice-streaming-agent/errors.hpp>

#include <cstring>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <syslog.h>
#include <X11/Xlib.h>

#include "jpeg.hpp"

using namespace spice::streaming_agent;

static inline uint64_t get_time()
{
    timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return (uint64_t)now.tv_sec * 10000000000u + (uint64_t)now.tv_nsec;
}

namespace {

class MjpegFrameCapture final: public FrameCapture
{
public:
    MjpegFrameCapture(const MjpegSettings &settings);
    ~MjpegFrameCapture();
    FrameInfo CaptureFrame() override;
    void Reset() override;
    SpiceVideoCodecType VideoCodecType() const override {
        return SPICE_VIDEO_CODEC_TYPE_MJPEG;
    }
private:
    MjpegSettings settings;
    Display *dpy;

    std::vector<uint8_t> frame;

    // last frame sizes
    int last_width = -1, last_height = -1;
    // last time before capture
    uint64_t last_time = 0;
};

}

MjpegFrameCapture::MjpegFrameCapture(const MjpegSettings& settings):
    settings(settings)
{
    dpy = XOpenDisplay(NULL);
    if (!dpy)
        throw Error("unable to initialize X11");
}

MjpegFrameCapture::~MjpegFrameCapture()
{
    XCloseDisplay(dpy);
}

void MjpegFrameCapture::Reset()
{
    frame.clear();
    last_width = last_height = ~0u;
}

FrameInfo MjpegFrameCapture::CaptureFrame()
{
    FrameInfo info;

    // reduce speed considering FPS
    auto now = get_time();
    if (last_time == 0) {
        last_time = now;
    } else {
        const uint64_t delta = 1000000000u / settings.fps;
        if (now >= last_time + delta) {
            last_time = now;
        } else {
            uint64_t wait_time = last_time + delta - now;
            // mathematically wait_time must be less than a second as
            // delta would be 1 seconds only for FPS == 1 but in this
            // side of the if now < last_time + delta
            // but is also true that now > last_time so
            // last_time + delta > now > last_time so
            // 1s >= delta > now - last_time > 0 so
            // wait_time = delta - (now - last_time) < delta <= 1s
            timespec delay = { 0, (long) wait_time };
            nanosleep(&delay, NULL);
            last_time += delta;
        }
    }

    int screen = XDefaultScreen(dpy);

    Window win = RootWindow(dpy, screen);

    XWindowAttributes win_info;
    XGetWindowAttributes(dpy, win, &win_info);

    bool is_first = false;
    if (win_info.width != last_width || win_info.height != last_height) {
        last_width = win_info.width;
        last_height = win_info.height;
        is_first = true;
    }

    info.size.width = win_info.width;
    info.size.height = win_info.height;

    int format = ZPixmap;
    // TODO handle errors
    XImage *image = XGetImage(dpy, win, win_info.x, win_info.y,
                              win_info.width, win_info.height, AllPlanes, format);

    // TODO handle errors
    // TODO multiple formats (only 32 bit)
    write_JPEG_file(frame, settings.quality, (uint8_t*) image->data,
                    image->width, image->height);

    image->f.destroy_image(image);

    info.buffer = &frame[0];
    info.buffer_size = frame.size();

    info.stream_start = is_first;

    return info;
}

FrameCapture *MjpegPlugin::CreateCapture()
{
    return new MjpegFrameCapture(settings);
}

unsigned MjpegPlugin::Rank()
{
    return FallBackMin;
}

void MjpegPlugin::ParseOptions(const ConfigureOption *options)
{
    for (; options->name; ++options) {
        const std::string name = options->name;
        const std::string value = options->value;

        if (name == "framerate") {
            try {
                settings.fps = stoi(value);
            } catch (const std::exception &e) {
                throw OptionValueError("invalid MJPEG framerate",
                                       options->value, "mjpeg.framerate");
            }
        } else if (name == "mjpeg.quality") {
            try {
                settings.quality = stoi(value);
            } catch (const std::exception &e) {
                throw OptionValueError("invalid MJPEG quality", options->value, "mjpeg.quality");
            }
        }
    }
}

MjpegSettings MjpegPlugin::Options() const
{
    return settings;
}

SpiceVideoCodecType MjpegPlugin::VideoCodecType() const {
    return SPICE_VIDEO_CODEC_TYPE_MJPEG;
}

bool MjpegPlugin::Register(Agent* agent)
{
    std::unique_ptr<MjpegPlugin> plugin(new MjpegPlugin());

    try {
        plugin->ParseOptions(agent->Options());
    } catch (const std::exception &e) {
        syslog(LOG_ERR, "Error parsing plugin option: %s\n", e.what());
    }

    agent->Register(*plugin.release());

    return true;
}
