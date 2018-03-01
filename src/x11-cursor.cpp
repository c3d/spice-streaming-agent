/* X11 cursor transmission
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#include "x11-cursor.hpp"
#include "concrete-agent.hpp"

#include <spice-streaming-agent/errors.hpp>

#include <syslog.h>

namespace spice
{
namespace streaming_agent
{

X11CursorUpdater::X11CursorUpdater(Stream &stream)
    : stream(stream),
      display(XOpenDisplay(NULL))
{
    if (display == NULL) {
        throw Error("failed to open display").syslog();
    }
}

X11CursorUpdater::~X11CursorUpdater()
{
    XCloseDisplay(display);
}

void X11CursorUpdater::send_cursor_changes()
{
    unsigned long last_serial = 0;

    int event_base, error_base;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        syslog(LOG_WARNING, "XFixesQueryExtension failed, not sending cursor changes\n");
        return; // also terminates the X11CursorThread if that's how we were launched
    }
    Window rootwindow = DefaultRootWindow(display);
    XFixesSelectCursorInput(display, rootwindow, XFixesDisplayCursorNotifyMask);

    while (!ConcreteAgent::quit_requested) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type != event_base + 1) {
            continue;
        }

        XFixesCursorImage *cursor = XFixesGetCursorImage(display);
        if (!cursor || cursor->cursor_serial == last_serial) {
            continue;
        }

        last_serial = cursor->cursor_serial;
        stream.send<X11CursorMessage>(cursor);
    }
}

X11CursorThread::X11CursorThread(Stream &stream)
    : updater(stream),
      thread(record_cursor_changes, this)
{
    thread.detach();
}

}} // namespace spic::streaming_agent
