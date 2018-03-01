/* X11 cursor transmission
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_X11_CURSOR_HPP
#define SPICE_STREAMING_AGENT_X11_CURSOR_HPP

#include "message.hpp"

#include <spice-streaming-agent/errors.hpp>

#include <thread>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

namespace spice {
namespace streaming_agent {

class X11CursorMessage : public Message<StreamMsgCursorSet, X11CursorMessage,
                                        STREAM_TYPE_CURSOR_SET>
{
public:
    X11CursorMessage(XFixesCursorImage *cursor): Message(cursor) {}
    static size_t size(XFixesCursorImage *cursor)
    {
        return sizeof(MessagePayload) + sizeof(uint32_t) * pixel_count(cursor);
    }

    void write_message_body(Stream &stream, XFixesCursorImage *cursor)
    {
        StreamMsgCursorSet msg = {
            .width = cursor->width,
            .height = cursor->height,
            .hot_spot_x = cursor->xhot,
            .hot_spot_y = cursor->yhot,
            .type = SPICE_CURSOR_TYPE_ALPHA,
            .padding1 = { },
            .data = { }
        };

        size_t pixcount = pixel_count(cursor);
        size_t pixsize = pixcount * sizeof(uint32_t);
        std::unique_ptr<uint32_t[]> pixels(new uint32_t[pixcount]);
        uint32_t *pixbuf = pixels.get();
        fill_pixels(cursor, pixcount, pixbuf);

        stream.write_all("cursor message", &msg, sizeof(msg));
        stream.write_all("cursor pixels", pixbuf, pixsize);
    }

private:
    static size_t pixel_count(XFixesCursorImage *cursor)
    {
        return cursor->width * cursor->height;
    }

    static void fill_pixels(XFixesCursorImage *cursor, unsigned count, uint32_t *pixbuf)
    {
        // This loop is required because historically, X11 uses a data type that
        // may be 64-bit even when it only sends 32-bit pixels values.
        for (unsigned i = 0; i < count; ++i) {
            pixbuf[i] = cursor->pixels[i];
        }
    }
};

class X11CursorUpdater
{
public:
    X11CursorUpdater(Stream &stream);
    ~X11CursorUpdater();
    void send_cursor_changes();

private:
    X11CursorUpdater(const X11CursorUpdater &) = delete;
    X11CursorUpdater(const X11CursorUpdater &&) = delete;
    X11CursorUpdater &operator=(const X11CursorUpdater &) = delete;

private:
    Stream &stream;
    Display *display;
};

class X11CursorThread
{
public:
    X11CursorThread(Stream &stream);
    ~X11CursorThread();
    static void record_cursor_changes(X11CursorThread *self) { self->updater.send_cursor_changes(); }

private:
    X11CursorUpdater updater;
    std::thread thread;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_X11_CURSOR_HPP
