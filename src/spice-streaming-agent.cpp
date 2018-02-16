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

struct FormatMessage
{
    StreamDevHeader hdr;
    StreamMsgFormat msg;
};

struct DataMessage
{
    StreamDevHeader hdr;
    StreamMsgData msg;
};

struct CursorMessage
{
    StreamDevHeader hdr;
    StreamMsgCursorSet msg;
};

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

    int read_command(bool blocking);
    void read_all(void *msg, size_t len);
    size_t write_all(const void *buf, const size_t len);
    int send_format(unsigned w, unsigned h, uint8_t c);
    int send_frame(const void *buf, const unsigned size);
    void send_cursor(uint16_t width, uint16_t height,
                     uint16_t hotspot_x, uint16_t hotspot_y,
                     std::function<void(uint32_t *)> fill_cursor);

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
};

}} // namespace spice::streaming_agent

static bool streaming_requested = false;
static bool quit_requested = false;
static bool log_binary = false;

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
    streaming_requested = (msg[0] != 0); /* num_codecs */
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           streaming_requested ? "START" : "STOP");
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
    StreamDevHeader hdr = {
        STREAM_DEVICE_PROTOCOL,
        0,
        STREAM_TYPE_CAPABILITIES,
        0
    };
    if (write_all(&hdr, sizeof(hdr)) != sizeof(hdr)) {
        throw std::runtime_error("error writing capabilities");
    }
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

size_t Stream::write_all(const void *buf, const size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(streamfd, (const char *) buf + written, len - written);
        if (l < 0) {
            if (errno == EINTR) {
                continue;
            }
            syslog(LOG_ERR, "write failed - %m");
            return l;
        }
        written += l;
    }
    syslog(LOG_DEBUG, "write_all -- %u bytes written\n", (unsigned)written);
    return written;
}

int Stream::send_format(unsigned w, unsigned h, uint8_t c)
{
    const size_t msgsize = sizeof(FormatMessage);
    const size_t hdrsize = sizeof(StreamDevHeader);
    FormatMessage msg = {
        .hdr = {
            .protocol_version = STREAM_DEVICE_PROTOCOL,
            .padding = 0,       // Workaround GCC "not implemented" bug
            .type = STREAM_TYPE_FORMAT,
            .size = msgsize - hdrsize
        },
        .msg = {
            .width = w,
            .height = h,
            .codec = c,
            .padding1 = { }
        }
    };
    syslog(LOG_DEBUG, "writing format\n");
    std::lock_guard<std::mutex> stream_guard(mutex);
    if (write_all(&msg, msgsize) != msgsize) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int Stream::send_frame(const void *buf, const unsigned size)
{
    ssize_t n;
    const size_t msgsize = sizeof(FormatMessage);
    DataMessage msg = {
        .hdr = {
            .protocol_version = STREAM_DEVICE_PROTOCOL,
            .padding = 0,       // Workaround GCC "not implemented" bug
            .type = STREAM_TYPE_DATA,
            .size = size  /* includes only the body? */
        },
        .msg = {}
    };

    std::lock_guard<std::mutex> stream_guard(mutex);
    n = write_all(&msg, msgsize);
    syslog(LOG_DEBUG,
           "wrote %ld bytes of header of data msg with frame of size %u bytes\n",
           n, msg.hdr.size);
    if (n != msgsize) {
        syslog(LOG_WARNING, "write_all header: wrote %ld expected %lu\n",
               n, msgsize);
        return EXIT_FAILURE;
    }
    n = write_all(buf, size);
    syslog(LOG_DEBUG, "wrote data msg body of size %ld\n", n);
    if (n != size) {
        syslog(LOG_WARNING, "write_all header: wrote %ld expected %u\n",
               n, size);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* returns current time in micro-seconds */
static uint64_t get_time(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;

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

void
Stream::send_cursor(uint16_t width, uint16_t height,
                    uint16_t hotspot_x, uint16_t hotspot_y,
                    std::function<void(uint32_t *)> fill_cursor)
{
    if (width >= STREAM_MSG_CURSOR_SET_MAX_WIDTH || height >= STREAM_MSG_CURSOR_SET_MAX_HEIGHT) {
        return;
    }

    const uint32_t msgsize = sizeof(CursorMessage) + width * height * sizeof(uint32_t);
    const uint32_t hdrsize = sizeof(StreamDevHeader);

    std::unique_ptr<uint8_t[]> storage(new uint8_t[msgsize]);

    CursorMessage *cursor_msg =
        new(storage.get()) CursorMessage {
        .hdr = {
            .protocol_version = STREAM_DEVICE_PROTOCOL,
            .padding = 0,       // Workaround GCC internal / not implemented compiler error
            .type = STREAM_TYPE_CURSOR_SET,
            .size = msgsize - hdrsize
        },
        .msg = {
            .width = width,
            .height = height,
            .hot_spot_x = hotspot_x,
            .hot_spot_y = hotspot_y,
            .type = SPICE_CURSOR_TYPE_ALPHA,
            .padding1 = { },
            .data = { }
        }
    };

    uint32_t *pixels = reinterpret_cast<uint32_t *>(cursor_msg->msg.data);
    fill_cursor(pixels);

    std::lock_guard<std::mutex> stream_guard(mutex);
    write_all(storage.get(), msgsize);
}

static void cursor_changes(Stream *stream, Display *display, int event_base)
{
    unsigned long last_serial = 0;

    while (1) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type != event_base + 1) {
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
        stream->send_cursor(cursor->width, cursor->height,
                            cursor->xhot, cursor->yhot, fill_cursor);
    }
}

static void
do_capture(Stream &stream, const char *streamport, FILE *f_log)
{
    unsigned int frame_count = 0;
    while (!quit_requested) {
        while (!quit_requested && !streaming_requested) {
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

        while (!quit_requested && streaming_requested) {
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

                if (stream.send_format(width, height, codec) == EXIT_FAILURE) {
                    throw std::runtime_error("FAILED to send format message");
                }
            }
            if (f_log) {
                if (log_binary) {
                    fwrite(frame.buffer, frame.buffer_size, 1, f_log);
                } else {
                    fprintf(f_log, "%" PRIu64 ": Frame of %zu bytes:\n",
                            get_time(), frame.buffer_size);
                    hexdump(frame.buffer, frame.buffer_size, f_log);
                }
            }
            if (stream.send_frame(frame.buffer, frame.buffer_size) == EXIT_FAILURE) {
                syslog(LOG_ERR, "FAILED to send a frame\n");
                break;
            }
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

    FILE *f_log = NULL;
    if (log_filename) {
        f_log = fopen(log_filename, "wb");
        if (!f_log) {
            syslog(LOG_ERR, "Failed to open log file '%s': %s\n",
                   log_filename, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        syslog(LOG_ERR, "failed to open display\n");
        return EXIT_FAILURE;
    }
    int event_base, error_base;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        syslog(LOG_ERR, "XFixesQueryExtension failed\n");
        return EXIT_FAILURE;
    }
    Window rootwindow = DefaultRootWindow(display);
    XFixesSelectCursorInput(display, rootwindow, XFixesDisplayCursorNotifyMask);

    Stream stream(streamport);
    std::thread cursor_th(cursor_changes, &stream, display, event_base);
    cursor_th.detach();

    int ret = EXIT_SUCCESS;
    try {
        do_capture(stream, streamport, f_log);
    }
    catch (std::runtime_error &err) {
        syslog(LOG_ERR, "%s\n", err.what());
        ret = EXIT_FAILURE;
    }

    if (f_log) {
        fclose(f_log);
        f_log = NULL;
    }
    closelog();
    return ret;
}
