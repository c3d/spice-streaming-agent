/* An implementation of a SPICE streaming agent
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */

#include "concrete-agent.hpp"
#include "hexdump.h"
#include "mjpeg-fallback.hpp"

#include <spice/stream-device.h>
#include <spice/enums.h>

#include <spice-streaming-agent/frame-capture.hpp>
#include <spice-streaming-agent/plugin.hpp>
#include <spice-streaming-agent/errors.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <poll.h>
#include <syslog.h>
#include <signal.h>
#include <exception>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

using namespace spice::streaming_agent;

static ConcreteAgent agent;

namespace spice
{
namespace streaming_agent
{

/* returns current time in micro-seconds */
static uint64_t get_time(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;

}

class Stream
{
    typedef std::set<SpiceVideoCodecType> codecs_t;

public:
    Stream(const char *name)
    {
        streamfd = open(name, O_RDWR);
        if (streamfd < 0) {
            throw std::runtime_error("failed to open streaming device");
        }
    }
    ~Stream()
    {
        close(streamfd);
    }

    const codecs_t &client_codecs() const { return codecs; }
    bool streaming_requested() const { return is_streaming; }

    template <typename Message, typename ...PayloadArgs>
    void send(PayloadArgs... payload_args)
    {
        Message message(payload_args...);
        std::lock_guard<std::mutex> stream_guard(mutex);
        message.write_header(*this);
        message.write_message_body(*this, payload_args...);
    }

    int read_command(bool blocking);
    void read_all(void *msg, size_t len);
    void write_all(const char *operation, const void *buf, const size_t len);

private:
    int have_something_to_read(int timeout);
    void handle_stream_start_stop(uint32_t len);
    void handle_stream_capabilities(uint32_t len);
    void handle_stream_error(size_t len);
    void read_command_from_device();

private:
    std::mutex mutex;
    codecs_t codecs;
    int streamfd = -1;
    bool is_streaming = false;
};

template <typename Payload, typename Info, unsigned Type>
class Message
{
public:
    template <typename ...PayloadArgs>
    Message(PayloadArgs... payload_args)
        : hdr(StreamDevHeader {
              .protocol_version = STREAM_DEVICE_PROTOCOL,
              .padding = 0,     // Workaround GCC bug "sorry: not implemented"
              .type = Type,
              .size = (uint32_t) Info::size(payload_args...)
          })
    { }
    void write_header(Stream &stream)
    {
        stream.write_all("header", &hdr, sizeof(hdr));
    }

protected:
    StreamDevHeader hdr;
    typedef Payload payload_t;
};

class FormatMessage : public Message<StreamMsgFormat, FormatMessage, STREAM_TYPE_FORMAT>
{
public:
    FormatMessage(unsigned w, unsigned h, uint8_t c) : Message(w, h, c) {}
    static size_t size(unsigned width, unsigned height, uint8_t codec)
    {
        return sizeof(payload_t);
    }
    void write_message_body(Stream &stream, unsigned w, unsigned h, uint8_t c)
    {
        StreamMsgFormat msg = { .width = w, .height = h, .codec = c, .padding1 = {} };
        stream.write_all("format", &msg, sizeof(msg));
    }
};

class FrameMessage : public Message<StreamMsgData, FrameMessage, STREAM_TYPE_DATA>
{
public:
    FrameMessage(const void *frame, size_t length) : Message(frame, length) {}
    static size_t size(const void *frame, size_t length)
    {
        return sizeof(payload_t) + length;
    }
    void write_message_body(Stream &stream, const void *frame, size_t length)
    {
        stream.write_all("frame", frame, length);
    }
};

class CapabilitiesMessage : public Message<StreamMsgData, CapabilitiesMessage, STREAM_TYPE_CAPABILITIES>
{
public:
    CapabilitiesMessage() : Message() {}
    static size_t size()
    {
        return sizeof(payload_t);
    }
    void write_message_body(Stream &stream)
    {
        /* No body for capabilities message */
    }
};

class X11CursorMessage : public Message<StreamMsgCursorSet, X11CursorMessage, STREAM_TYPE_CURSOR_SET>
{
public:
    X11CursorMessage(XFixesCursorImage *cursor): Message(cursor) {}
    static size_t size(XFixesCursorImage *cursor)
    {
        return sizeof(payload_t) + sizeof(uint32_t) * pixel_count(cursor);
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
    Stream &stream;
    Display *display;
};

class FrameLog
{
public:
    FrameLog(const char *filename, bool binary = false);
    ~FrameLog();

    operator bool() { return log != NULL; }
    void dump(const void *buffer, size_t length);

private:
    FILE *log;
    bool binary;
};


FrameLog::FrameLog(const char *filename, bool binary)
    : log(filename ? fopen(filename, "wb") : NULL), binary(binary)
{
    if (filename && !log) {
        throw OpenError("failed to open hexdump log file", filename, errno);
    }
}

FrameLog::~FrameLog()
{
    if (log) {
        fclose(log);
    }
}

void FrameLog::dump(const void *buffer, size_t length)
{
    if (log) {
        if (binary) {
            fwrite(buffer, length, 1, log);
        } else {
            fprintf(log, "%" PRIu64 ": Frame of %zu bytes:\n", get_time(), length);
            hexdump(buffer, length, log);
        }
    }
}

class X11CursorThread
{
public:
    X11CursorThread(Stream &stream);
    static void record_cursor_changes(X11CursorThread *self) { self->updater.send_cursor_changes(); }

private:
    X11CursorUpdater updater;
    std::thread thread;
};

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

    while (true) {
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

}} // namespace spice::streaming_agent

static bool quit_requested = false;

int Stream::have_something_to_read(int timeout)
{
    struct pollfd pollfd = {streamfd, POLLIN, 0};

    if (poll(&pollfd, 1, timeout) < 0) {
        syslog(LOG_ERR, "poll FAILED\n");
        return -1;
    }

    if (pollfd.revents == POLLIN) {
        return 1;
    }

    return 0;
}

void Stream::read_all(void *msg, size_t len)
{
    while (len > 0) {
        ssize_t n = read(streamfd, msg, len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Reading message from device failed: " +
                                     std::string(strerror(errno)));
        }

        len -= n;
        msg = (uint8_t *) msg + n;
    }
}

void Stream::handle_stream_start_stop(uint32_t len)
{
    uint8_t msg[256];

    if (len >= sizeof(msg)) {
        throw std::runtime_error("msg size (" + std::to_string(len) + ") is too long "
                                 "(longer than " + std::to_string(sizeof(msg)) + ")");
    }

    read_all(msg, len);
    is_streaming = (msg[0] != 0); /* num_codecs */
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           is_streaming ? "START" : "STOP");
    codecs.clear();
    for (int i = 1; i <= msg[0]; ++i) {
        codecs.insert((SpiceVideoCodecType) msg[i]);
    }
}

void Stream::handle_stream_capabilities(uint32_t len)
{
    uint8_t caps[STREAM_MSG_CAPABILITIES_MAX_BYTES];

    if (len > sizeof(caps)) {
        throw std::runtime_error("capability message too long");
    }

    read_all(caps, len);
    // we currently do not support extensions so just reply so
    send<CapabilitiesMessage>();
}

void Stream::handle_stream_error(size_t len)
{
    if (len < sizeof(StreamMsgNotifyError)) {
        throw std::runtime_error("Received NotifyError message size " + std::to_string(len) +
                                 " is too small (smaller than " +
                                 std::to_string(sizeof(StreamMsgNotifyError)) + ")");
    }

    struct : StreamMsgNotifyError {
        uint8_t msg[1024];
    } msg;

    size_t len_to_read = std::min(len, sizeof(msg) - 1);

    read_all(&msg, len_to_read);
    msg.msg[len_to_read - sizeof(StreamMsgNotifyError)] = '\0';

    syslog(LOG_ERR, "Received NotifyError message from the server: %d - %s\n",
        msg.error_code, msg.msg);

    if (len_to_read < len) {
        throw std::runtime_error("Received NotifyError message size " + std::to_string(len) +
                                 " is too big (bigger than " + std::to_string(sizeof(msg)) + ")");
    }
}

void Stream::read_command_from_device()
{
    StreamDevHeader hdr;
    int n;

    std::lock_guard<std::mutex> stream_guard(mutex);
    n = read(streamfd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr)) {
        throw std::runtime_error("read command from device FAILED -- read " + std::to_string(n) +
                                 " expected " + std::to_string(sizeof(hdr)));
    }
    if (hdr.protocol_version != STREAM_DEVICE_PROTOCOL) {
        throw std::runtime_error("BAD VERSION " + std::to_string(hdr.protocol_version) +
                                 " (expected is " + std::to_string(STREAM_DEVICE_PROTOCOL) + ")");
    }

    switch (hdr.type) {
    case STREAM_TYPE_CAPABILITIES:
        return handle_stream_capabilities(hdr.size);
    case STREAM_TYPE_NOTIFY_ERROR:
        return handle_stream_error(hdr.size);
    case STREAM_TYPE_START_STOP:
        return handle_stream_start_stop(hdr.size);
    }
    throw std::runtime_error("UNKNOWN msg of type " + std::to_string(hdr.type));
}

int Stream::read_command(bool blocking)
{
    int timeout = blocking?-1:0;
    while (!quit_requested) {
        if (!have_something_to_read(timeout)) {
            if (!blocking) {
                return 0;
            }
            sleep(1);
            continue;
        }
        read_command_from_device();
        break;
    }

    return 1;
}

void Stream::write_all(const char *operation, const void *buf, const size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(streamfd, (const char *) buf + written, len - written);
        if (l < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw WriteError("write failed", operation, errno).syslog();
        }
        written += l;
    }
    syslog(LOG_DEBUG, "write_all -- %zu bytes written\n", written);
}

static void handle_interrupt(int intr)
{
    syslog(LOG_INFO, "Got signal %d, exiting", intr);
    quit_requested = true;
}

static void register_interrupts(void)
{
    struct sigaction sa = { };
    sa.sa_handler = handle_interrupt;
    if ((sigaction(SIGINT, &sa, NULL) != 0) &&
        (sigaction(SIGTERM, &sa, NULL) != 0)) {
        syslog(LOG_WARNING, "failed to register signal handler %m");
    }
}

static void usage(const char *progname)
{
    printf("usage: %s <options>\n", progname);
    printf("options are:\n");
    printf("\t-p portname  -- virtio-serial port to use\n");
    printf("\t-l file -- log frames to file\n");
    printf("\t--log-binary -- log binary frames (following -l)\n");
    printf("\t--plugins-dir=path -- change plugins directory\n");
    printf("\t-d -- enable debug logs\n");
    printf("\t-c variable=value -- change settings\n");
    printf("\t\tframerate = 1-100 (check 10,20,30,40,50,60)\n");
    printf("\n");
    printf("\t-h or --help     -- print this help message\n");

    exit(1);
}

static void
do_capture(Stream &stream, const char *streamport, FrameLog &frame_log)
{
    unsigned int frame_count = 0;
    while (!quit_requested) {
        while (!quit_requested && !stream.streaming_requested()) {
            if (stream.read_command(true) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }

        if (quit_requested) {
            return;
        }

        syslog(LOG_INFO, "streaming starts now\n");
        uint64_t time_last = 0;

        std::unique_ptr<FrameCapture> capture(agent.GetBestFrameCapture(stream.client_codecs()));
        if (!capture) {
            throw std::runtime_error("cannot find a suitable capture system");
        }

        while (!quit_requested && stream.streaming_requested()) {
            if (++frame_count % 100 == 0) {
                syslog(LOG_DEBUG, "SENT %d frames\n", frame_count);
            }
            uint64_t time_before = get_time();

            FrameInfo frame = capture->CaptureFrame();

            uint64_t time_after = get_time();
            syslog(LOG_DEBUG,
                   "got a frame -- size is %zu (%" PRIu64 " ms) "
                   "(%" PRIu64 " ms from last frame)(%" PRIu64 " us)\n",
                   frame.buffer_size, (time_after - time_before)/1000,
                   (time_after - time_last)/1000,
                   (time_before - time_last));
            time_last = time_after;

            if (frame.stream_start) {
                unsigned width, height;
                uint8_t codec;

                width = frame.size.width;
                height = frame.size.height;
                codec = capture->VideoCodecType();

                syslog(LOG_DEBUG, "wXh %uX%u  codec=%u\n", width, height, codec);

                stream.send<FormatMessage>(width, height, codec);
            }
            frame_log.dump(frame.buffer, frame.buffer_size);
            stream.send<FrameMessage>(frame.buffer, frame.buffer_size);
            //usleep(1);
            if (stream.read_command(false) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }
    }
}

#define arg_error(...) syslog(LOG_ERR, ## __VA_ARGS__);

int main(int argc, char* argv[])
{
    const char *streamport = "/dev/virtio-ports/com.redhat.stream.0";
    int opt;
    const char *log_filename = NULL;
    bool log_binary = false;
    int logmask = LOG_UPTO(LOG_WARNING);
    const char *pluginsdir = PLUGINSDIR;
    enum {
        OPT_first = UCHAR_MAX,
        OPT_PLUGINS_DIR,
        OPT_LOG_BINARY,
    };
    static const struct option long_options[] = {
        { "plugins-dir", required_argument, NULL, OPT_PLUGINS_DIR},
        { "log-binary", no_argument, NULL, OPT_LOG_BINARY},
        { "help", no_argument, NULL, 'h'},
        { 0, 0, 0, 0}
    };

    openlog("spice-streaming-agent",
            isatty(fileno(stderr)) ? (LOG_PERROR|LOG_PID) : LOG_PID, LOG_USER);

    setlogmask(logmask);

    while ((opt = getopt_long(argc, argv, "hp:c:l:d", long_options, NULL)) != -1) {
        switch (opt) {
        case 0:
            /* Handle long options if needed */
            break;
        case OPT_PLUGINS_DIR:
            pluginsdir = optarg;
            break;
        case 'p':
            streamport = optarg;
            break;
        case 'c': {
            char *p = strchr(optarg, '=');
            if (p == NULL) {
                arg_error("wrong 'c' argument %s\n", optarg);
                usage(argv[0]);
            }
            *p++ = '\0';
            agent.AddOption(optarg, p);
            break;
        }
        case OPT_LOG_BINARY:
            log_binary = true;
            break;
        case 'l':
            log_filename = optarg;
            break;
        case 'd':
            logmask = LOG_UPTO(LOG_DEBUG);
            setlogmask(logmask);
            break;
        case 'h':
            usage(argv[0]);
            break;
        }
    }

    // register built-in plugins
    MjpegPlugin::Register(&agent);

    agent.LoadPlugins(pluginsdir);

    register_interrupts();

    int ret = EXIT_SUCCESS;
    try {
        Stream stream(streamport);
        FrameLog frame_log(log_filename, log_binary);
        X11CursorThread cursor_thread(stream);
        do_capture(stream, streamport, frame_log);
    }
    catch (Error &err) {
        err.syslog();
        ret = EXIT_FAILURE;
    }
    catch (std::runtime_error &err) {
        syslog(LOG_ERR, "%s\n", err.what());
        ret = EXIT_FAILURE;
    }
    closelog();
    return ret;
}
