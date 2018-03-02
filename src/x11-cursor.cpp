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

    try {
        int x11_fd = ConnectionNumber(display);
        struct timeval delay;
        delay.tv_usec = 500000;
        delay.tv_sec = 0;

        fd_set in_fds;
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        while (!ConcreteAgent::quit_requested()) {
            // Wait for an X11 event or for a 0.5s timeout in case of quit_requested

            // Wait for X Event or a Timer
            if (!select(x11_fd+1, &in_fds, 0, 0, &delay)) {
                // Timeout, loop
                continue;
            }

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
    catch (QuitRequested &quit) {
        syslog(LOG_INFO, "X11 cursor thread received request to quit, exiting");
    }
    catch (Error &err) {
        syslog(LOG_ERR, "Got an exception in X11 cursor thread, exiting");
        err.syslog();
    }
}

X11CursorThread::X11CursorThread(Stream &stream)
    : updater(stream),
      thread(record_cursor_changes, this)
{}


X11CursorThread::~X11CursorThread()
{
    syslog(LOG_INFO, "X11 cursor thread must join (quitting)");
    ConcreteAgent::request_quit();
    thread.join();
    syslog(LOG_INFO, "X11 cursor thread joined");
}

}} // namespace spic::streaming_agent
