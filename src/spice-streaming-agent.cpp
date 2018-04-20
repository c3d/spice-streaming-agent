/* An implementation of a SPICE streaming agent
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */

#include "concrete-agent.hpp"
#include "hexdump.h"
#include "mjpeg-fallback.hpp"
#include "stream-port.hpp"
#include "error.hpp"

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
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

using namespace spice::streaming_agent;

static ConcreteAgent agent;

struct SpiceStreamFormatMessage
{
    StreamDevHeader hdr;
    StreamMsgFormat msg;
};

struct SpiceStreamDataMessage
{
    StreamDevHeader hdr;
    StreamMsgData msg;
};

static bool streaming_requested = false;
static bool quit_requested = false;
static bool log_binary = false;
static bool log_frames = false;
static std::set<SpiceVideoCodecType> client_codecs;

static bool have_something_to_read(StreamPort &stream_port, bool blocking)
{
    struct pollfd pollfd = {stream_port.fd, POLLIN, 0};

    if (poll(&pollfd, 1, blocking ? -1 : 0) < 0) {
        if (errno == EINTR) {
            // report nothing to read, next iteration of the enclosing loop will retry
            return false;
        }

        throw IOError("poll failed on the device", errno);
    }

    if (pollfd.revents & POLLIN) {
        return true;
    }

    return false;
}

static void handle_stream_start_stop(StreamPort &stream_port, uint32_t len)
{
    uint8_t msg[256];

    if (len >= sizeof(msg)) {
        throw std::runtime_error("msg size (" + std::to_string(len) + ") is too long "
                                 "(longer than " + std::to_string(sizeof(msg)) + ")");
    }

    stream_port.read(msg, len);
    streaming_requested = (msg[0] != 0); /* num_codecs */
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           streaming_requested ? "START" : "STOP");
    client_codecs.clear();
    for (int i = 1; i <= msg[0]; ++i) {
        client_codecs.insert((SpiceVideoCodecType) msg[i]);
    }
}

static void handle_stream_capabilities(StreamPort &stream_port, uint32_t len)
{
    uint8_t caps[STREAM_MSG_CAPABILITIES_MAX_BYTES];

    if (len > sizeof(caps)) {
        throw std::runtime_error("capability message too long");
    }

    stream_port.read(caps, len);
    // we currently do not support extensions so just reply so
    StreamDevHeader hdr = {
        STREAM_DEVICE_PROTOCOL,
        0,
        STREAM_TYPE_CAPABILITIES,
        0
    };

    stream_port.write(&hdr, sizeof(hdr));
}

static void handle_stream_error(StreamPort &stream_port, size_t len)
{
    if (len < sizeof(StreamMsgNotifyError)) {
        throw std::runtime_error("Received NotifyError message size " + std::to_string(len) +
                                 " is too small (smaller than " +
                                 std::to_string(sizeof(StreamMsgNotifyError)) + ")");
    }

    struct StreamMsgNotifyError1K : StreamMsgNotifyError {
        uint8_t msg[1024];
    } msg;

    size_t len_to_read = std::min(len, sizeof(msg) - 1);

    stream_port.read(&msg, len_to_read);
    msg.msg[len_to_read - sizeof(StreamMsgNotifyError)] = '\0';

    syslog(LOG_ERR, "Received NotifyError message from the server: %d - %s\n",
        msg.error_code, msg.msg);

    if (len_to_read < len) {
        throw std::runtime_error("Received NotifyError message size " + std::to_string(len) +
                                 " is too big (bigger than " + std::to_string(sizeof(msg)) + ")");
    }
}

static void read_command_from_device(StreamPort &stream_port)
{
    StreamDevHeader hdr;

    std::lock_guard<std::mutex> guard(stream_port.mutex);
    stream_port.read(&hdr, sizeof(hdr));

    if (hdr.protocol_version != STREAM_DEVICE_PROTOCOL) {
        throw std::runtime_error("BAD VERSION " + std::to_string(hdr.protocol_version) +
                                 " (expected is " + std::to_string(STREAM_DEVICE_PROTOCOL) + ")");
    }

    switch (hdr.type) {
    case STREAM_TYPE_CAPABILITIES:
        return handle_stream_capabilities(stream_port, hdr.size);
    case STREAM_TYPE_NOTIFY_ERROR:
        return handle_stream_error(stream_port, hdr.size);
    case STREAM_TYPE_START_STOP:
        return handle_stream_start_stop(stream_port, hdr.size);
    }
    throw std::runtime_error("UNKNOWN msg of type " + std::to_string(hdr.type));
}

static void read_command(StreamPort &stream_port, bool blocking)
{
    while (!quit_requested) {
        if (have_something_to_read(stream_port, blocking)) {
            read_command_from_device(stream_port);
            break;
        }

        if (!blocking) {
            break;
        }

        sleep(1);
    }
}

static void spice_stream_send_format(StreamPort &stream_port, unsigned w, unsigned h, unsigned c)
{

    SpiceStreamFormatMessage msg;
    const size_t msgsize = sizeof(msg);
    const size_t hdrsize  = sizeof(msg.hdr);
    memset(&msg, 0, msgsize);
    msg.hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    msg.hdr.type = STREAM_TYPE_FORMAT;
    msg.hdr.size = msgsize - hdrsize; /* includes only the body? */
    msg.msg.width = w;
    msg.msg.height = h;
    msg.msg.codec = c;

    syslog(LOG_DEBUG, "writing format\n");
    std::lock_guard<std::mutex> guard(stream_port.mutex);
    stream_port.write(&msg, msgsize);
}

static void spice_stream_send_frame(StreamPort &stream_port, const void *buf, const unsigned size)
{
    SpiceStreamDataMessage msg;
    const size_t msgsize = sizeof(msg);

    memset(&msg, 0, msgsize);
    msg.hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    msg.hdr.type = STREAM_TYPE_DATA;
    msg.hdr.size = size; /* includes only the body? */

    std::lock_guard<std::mutex> guard(stream_port.mutex);
    stream_port.write(&msg, msgsize);
    stream_port.write(buf, size);

    syslog(LOG_DEBUG, "Sent a frame of size %u\n", size);
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
    printf("\t--log-categories -- log categories, separated by ':' (currently: frames)\n");
    printf("\t--plugins-dir=path -- change plugins directory\n");
    printf("\t-d -- enable debug logs\n");
    printf("\t-c variable=value -- change settings\n");
    printf("\t\tframerate = 1-100 (check 10,20,30,40,50,60)\n");
    printf("\n");
    printf("\t-h or --help     -- print this help message\n");

    exit(1);
}

static void
send_cursor(StreamPort &stream_port, unsigned width, unsigned height, int hotspot_x, int hotspot_y,
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

static void cursor_changes(StreamPort *stream_port, Display *display, int event_base)
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
        send_cursor(*stream_port, cursor->width, cursor->height, cursor->xhot, cursor->yhot, fill_cursor);
    }
}

#define STAT_LOG(format, ...) do { \
    if (f_log && !log_binary) { \
        fprintf(f_log, "%" PRIu64 ": " format "\n", get_time(), ## __VA_ARGS__); \
    } \
} while(0)

static void
do_capture(StreamPort &stream_port, FILE *f_log)
{
    unsigned int frame_count = 0;
    while (!quit_requested) {
        while (!quit_requested && !streaming_requested) {
            read_command(stream_port, true);
        }

        if (quit_requested) {
            return;
        }

        syslog(LOG_INFO, "streaming starts now\n");
        uint64_t time_last = 0;

        std::unique_ptr<FrameCapture> capture(agent.GetBestFrameCapture(client_codecs));
        if (!capture) {
            throw std::runtime_error("cannot find a suitable capture system");
        }

        while (!quit_requested && streaming_requested) {
            if (++frame_count % 100 == 0) {
                syslog(LOG_DEBUG, "SENT %d frames\n", frame_count);
            }
            uint64_t time_before = get_time();

            STAT_LOG("Capturing...");
            FrameInfo frame = capture->CaptureFrame();
            STAT_LOG("Captured");

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
                unsigned char codec;

                width = frame.size.width;
                height = frame.size.height;
                codec = capture->VideoCodecType();

                syslog(LOG_DEBUG, "wXh %uX%u  codec=%u\n", width, height, codec);
                STAT_LOG("Started new stream wXh %uX%u  codec=%u", width, height, codec);

                spice_stream_send_format(stream_port, width, height, codec);
            }
            STAT_LOG("Frame of %zu bytes:", frame.buffer_size);
            if (f_log) {
                if (log_binary) {
                    fwrite(frame.buffer, frame.buffer_size, 1, f_log);
                } else if (log_frames) {
                    hexdump(frame.buffer, frame.buffer_size, f_log);
                }
            }

            try {
                spice_stream_send_frame(stream_port, frame.buffer, frame.buffer_size);
            } catch (const WriteError& e) {
                syslog(e);
                break;
            }
            STAT_LOG("Sent");

            read_command(stream_port, false);
        }
    }
}

#define arg_error(...) syslog(LOG_ERR, ## __VA_ARGS__);

int main(int argc, char* argv[])
{
    const char *stream_port_name = "/dev/virtio-ports/org.spice-space.stream.0";
    int opt;
    const char *log_filename = NULL;
    int logmask = LOG_UPTO(LOG_WARNING);
    const char *pluginsdir = PLUGINSDIR;
    enum {
        OPT_first = UCHAR_MAX,
        OPT_PLUGINS_DIR,
        OPT_LOG_BINARY,
        OPT_LOG_CATEGORIES,
    };
    static const struct option long_options[] = {
        { "plugins-dir", required_argument, NULL, OPT_PLUGINS_DIR},
        { "log-binary", no_argument, NULL, OPT_LOG_BINARY},
        { "log-categories", required_argument, NULL, OPT_LOG_CATEGORIES},
        { "help", no_argument, NULL, 'h'},
        { 0, 0, 0, 0}
    };
    std::vector<std::string> old_args(argv, argv+argc);

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
            stream_port_name = optarg;
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
        case OPT_LOG_CATEGORIES:
            for (const char *tok = strtok(optarg, ":"); tok; tok = strtok(nullptr, ":")) {
                std::string cat = tok;
                if (cat == "frames") {
                    log_frames = true;
                }
                // ignore not existing, compatibility for future
            }
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
        if (!log_binary) {
            setlinebuf(f_log);
        }
        for (const std::string& arg: old_args) {
            STAT_LOG("Args: %s", arg.c_str());
        }
    }
    old_args.clear();

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

    int ret = EXIT_SUCCESS;

    try {
        StreamPort stream_port(stream_port_name);

        std::thread cursor_th(cursor_changes, &stream_port, display, event_base);
        cursor_th.detach();

        do_capture(stream_port, f_log);
    }
    catch (std::exception &err) {
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
