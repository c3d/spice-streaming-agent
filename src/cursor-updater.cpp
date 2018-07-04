/* A class that monitors X11 cursor changes and sends the cursor image over the
 * streaming virtio port.
 *
 * \copyright
 * Copyright 2016-2018 Red Hat Inc. All rights reserved.
 */

#include "cursor-updater.hpp"

#include "error.hpp"

#include <spice/stream-device.h>
#include <spice/enums.h>

#include <cstring>
#include <functional>
#include <memory>
#include <X11/extensions/Xfixes.h>


namespace spice {
namespace streaming_agent {

namespace {

void send_cursor(StreamPort &stream_port, unsigned width, unsigned height, int hotspot_x, int hotspot_y,
                 std::function<void(uint32_t *)> fill_cursor)
{
    if (width >= STREAM_MSG_CURSOR_SET_MAX_WIDTH || height >= STREAM_MSG_CURSOR_SET_MAX_HEIGHT) {
        return;
    }

    size_t cursor_size =
        sizeof(StreamDevHeader) + sizeof(StreamMsgCursorSet) +
        width * height * sizeof(uint32_t);
    std::unique_ptr<uint8_t[]> msg(new uint8_t[cursor_size]);

    StreamDevHeader &dev_hdr(*reinterpret_cast<StreamDevHeader*>(msg.get()));
    memset(&dev_hdr, 0, sizeof(dev_hdr));
    dev_hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    dev_hdr.type = STREAM_TYPE_CURSOR_SET;
    dev_hdr.size = cursor_size - sizeof(StreamDevHeader);

    StreamMsgCursorSet &cursor_msg(*reinterpret_cast<StreamMsgCursorSet *>(msg.get() + sizeof(StreamDevHeader)));
    memset(&cursor_msg, 0, sizeof(cursor_msg));

    cursor_msg.type = SPICE_CURSOR_TYPE_ALPHA;
    cursor_msg.width = width;
    cursor_msg.height = height;
    cursor_msg.hot_spot_x = hotspot_x;
    cursor_msg.hot_spot_y = hotspot_y;

    uint32_t *pixels = reinterpret_cast<uint32_t *>(cursor_msg.data);
    fill_cursor(pixels);

    std::lock_guard<std::mutex> guard(stream_port.mutex);
    stream_port.write(msg.get(), cursor_size);
}

} // namespace

CursorUpdater::CursorUpdater(StreamPort *stream_port) : stream_port(stream_port)
{
    display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        throw Error("Failed to open X display");
    }

    int error_base;
    if (!XFixesQueryExtension(display, &xfixes_event_base, &error_base)) {
        throw Error("XFixesQueryExtension failed");
    }

    XFixesSelectCursorInput(display, DefaultRootWindow(display), XFixesDisplayCursorNotifyMask);
}

[[noreturn]] void CursorUpdater::operator()()
{
    unsigned long last_serial = 0;

    while (1) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type != xfixes_event_base + 1) {
            continue;
        }

        XFixesCursorImage *cursor = XFixesGetCursorImage(display);
        if (!cursor) {
            continue;
        }

        if (cursor->cursor_serial == last_serial) {
            continue;
        }

        last_serial = cursor->cursor_serial;
        auto fill_cursor = [cursor](uint32_t *pixels) {
            for (unsigned i = 0; i < cursor->width * cursor->height; ++i)
                pixels[i] = cursor->pixels[i];
        };
        send_cursor(*stream_port, cursor->width, cursor->height, cursor->xhot, cursor->yhot, fill_cursor);
    }
}

}} // namespace spice::streaming_agent
